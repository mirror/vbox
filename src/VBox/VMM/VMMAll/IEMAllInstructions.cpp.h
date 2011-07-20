/* $Id$ */
/** @file
 * IEM - Instruction Decoding and Emulation.
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
*   Global Variables                                                           *
*******************************************************************************/
extern const PFNIEMOP g_apfnOneByteMap[256]; /* not static since we need to forward declare it. */


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
        IEMOP_HLP_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(3, 0);
        IEM_MC_ARG(uint8_t *,  pu8Dst,  0);
        IEM_MC_ARG(uint8_t,    u8Src,   1);
        IEM_MC_ARG(uint32_t *, pEFlags, 2);

        IEM_MC_FETCH_GREG_U8(u8Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
        IEM_MC_REF_GREG_U8(pu8Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_MEM_MAP(pu8Dst, fAccess, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
        IEM_MC_FETCH_GREG_U8(u8Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
        IEM_MC_FETCH_EFLAGS(EFlags);
        if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
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
        IEMOP_HLP_NO_LOCK_PREFIX();

        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *, pu16Dst, 0);
                IEM_MC_ARG(uint16_t,   u16Src,  1);
                IEM_MC_ARG(uint32_t *, pEFlags, 2);

                IEM_MC_FETCH_GREG_U16(u16Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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

                IEM_MC_FETCH_GREG_U32(u32Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, u32Src, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint64_t *, pu64Dst, 0);
                IEM_MC_ARG(uint64_t,   u64Src,  1);
                IEM_MC_ARG(uint32_t *, pEFlags, 2);

                IEM_MC_FETCH_GREG_U64(u64Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint16_t *, pu16Dst,          0);
                IEM_MC_ARG(uint16_t,   u16Src,           1);
                IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_MEM_MAP(pu16Dst, fAccess, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_GREG_U16(u16Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_MEM_MAP(pu32Dst, fAccess, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_GREG_U32(u32Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_MEM_MAP(pu64Dst, fAccess, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_GREG_U64(u64Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
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
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEM_MC_BEGIN(3, 0);
        IEM_MC_ARG(uint8_t *,  pu8Dst,  0);
        IEM_MC_ARG(uint8_t,    u8Src,   1);
        IEM_MC_ARG(uint32_t *, pEFlags, 2);

        IEM_MC_FETCH_GREG_U8(u8Src, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
        IEM_MC_REF_GREG_U8(pu8Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
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

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_FETCH_MEM_U8(u8Src, pIemCpu->iEffSeg, GCPtrEffDst);
        IEM_MC_REF_GREG_U8(pu8Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
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
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *, pu16Dst, 0);
                IEM_MC_ARG(uint16_t,   u16Src,  1);
                IEM_MC_ARG(uint32_t *, pEFlags, 2);

                IEM_MC_FETCH_GREG_U16(u16Src, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_GREG_U16(pu16Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
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

                IEM_MC_FETCH_GREG_U32(u32Src, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_GREG_U32(pu32Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, u32Src, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint64_t *, pu64Dst, 0);
                IEM_MC_ARG(uint64_t,   u64Src,  1);
                IEM_MC_ARG(uint32_t *, pEFlags, 2);

                IEM_MC_FETCH_GREG_U64(u64Src, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_GREG_U64(pu64Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
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
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint16_t *, pu16Dst, 0);
                IEM_MC_ARG(uint16_t,   u16Src,  1);
                IEM_MC_ARG(uint32_t *, pEFlags, 2);
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_MEM_U16(u16Src, pIemCpu->iEffSeg, GCPtrEffDst);
                IEM_MC_REF_GREG_U16(pu16Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_MEM_U32(u32Src, pIemCpu->iEffSeg, GCPtrEffDst);
                IEM_MC_REF_GREG_U32(pu32Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, u32Src, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint64_t *, pu64Dst, 0);
                IEM_MC_ARG(uint64_t,   u64Src,  1);
                IEM_MC_ARG(uint32_t *, pEFlags, 2);
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_MEM_U64(u64Src, pIemCpu->iEffSeg, GCPtrEffDst);
                IEM_MC_REF_GREG_U64(pu64Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
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
    IEMOP_HLP_NO_LOCK_PREFIX();

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
    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
        {
            uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
            IEMOP_HLP_NO_LOCK_PREFIX();

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
            IEMOP_HLP_NO_LOCK_PREFIX();

            IEM_MC_BEGIN(3, 0);
            IEM_MC_ARG(uint32_t *,      pu32Dst,                0);
            IEM_MC_ARG_CONST(uint32_t,  u32Src,/*=*/ u32Imm,    1);
            IEM_MC_ARG(uint32_t *,      pEFlags,                2);

            IEM_MC_REF_GREG_U32(pu32Dst, X86_GREG_xAX);
            IEM_MC_REF_EFLAGS(pEFlags);
            IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, u32Src, pEFlags);

            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;
        }

        case IEMMODE_64BIT:
        {
            uint64_t u64Imm; IEM_OPCODE_GET_NEXT_S32_SX_U64(&u64Imm);
            IEMOP_HLP_NO_LOCK_PREFIX();

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
FNIEMOP_STUB_1(iemOp_Grp6_sldt, uint8_t, bRm);


/** Opcode 0x0f 0x00 /1. */
FNIEMOP_STUB_1(iemOp_Grp6_str, uint8_t, bRm);


/** Opcode 0x0f 0x00 /2. */
FNIEMOP_DEF_1(iemOp_Grp6_lldt, uint8_t, bRm)
{
    IEMOP_HLP_NO_LOCK_PREFIX();
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEM_MC_BEGIN(1, 0);
        IEM_MC_ARG(uint16_t, u16Sel, 0);
        IEM_MC_FETCH_GREG_U16(u16Sel, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
        IEM_MC_CALL_CIMPL_1(iemCImpl_lldt, u16Sel);
        IEM_MC_END();
    }
    else
    {
        IEM_MC_BEGIN(1, 1);
        IEM_MC_ARG(uint16_t, u16Sel, 0);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
        IEM_MC_RAISE_GP0_IF_CPL_NOT_ZERO();
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm);
        IEM_MC_FETCH_MEM_U16(u16Sel, pIemCpu->iEffSeg, GCPtrEffSrc);
        IEM_MC_CALL_CIMPL_1(iemCImpl_lldt, u16Sel);
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x00 /3. */
FNIEMOP_DEF_1(iemOp_Grp6_ltr, uint8_t, bRm)
{
    IEMOP_HLP_NO_LOCK_PREFIX();
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEM_MC_BEGIN(1, 0);
        IEM_MC_ARG(uint16_t, u16Sel, 0);
        IEM_MC_FETCH_GREG_U16(u16Sel, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
        IEM_MC_CALL_CIMPL_1(iemCImpl_ltr, u16Sel);
        IEM_MC_END();
    }
    else
    {
        IEM_MC_BEGIN(1, 1);
        IEM_MC_ARG(uint16_t, u16Sel, 0);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
        IEM_MC_RAISE_GP0_IF_CPL_NOT_ZERO();
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm);
        IEM_MC_FETCH_MEM_U16(u16Sel, pIemCpu->iEffSeg, GCPtrEffSrc);
        IEM_MC_CALL_CIMPL_1(iemCImpl_ltr, u16Sel);
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x00 /4. */
FNIEMOP_STUB_1(iemOp_Grp6_verr, uint8_t, bRm);


/** Opcode 0x0f 0x00 /5. */
FNIEMOP_STUB_1(iemOp_Grp6_verw, uint8_t, bRm);


/** Opcode 0x0f 0x00. */
FNIEMOP_DEF(iemOp_Grp6)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
    {
        case 0: return FNIEMOP_CALL_1(iemOp_Grp6_sldt, bRm);
        case 1: return FNIEMOP_CALL_1(iemOp_Grp6_str,  bRm);
        case 2: return FNIEMOP_CALL_1(iemOp_Grp6_lldt, bRm);
        case 3: return FNIEMOP_CALL_1(iemOp_Grp6_ltr,  bRm);
        case 4: return FNIEMOP_CALL_1(iemOp_Grp6_verr, bRm);
        case 5: return FNIEMOP_CALL_1(iemOp_Grp6_verw, bRm);
        case 6: return IEMOP_RAISE_INVALID_OPCODE();
        case 7: return IEMOP_RAISE_INVALID_OPCODE();
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }

}


/** Opcode 0x0f 0x01 /0. */
FNIEMOP_DEF_1(iemOp_Grp7_sgdt, uint8_t, bRm)
{
    AssertFailedReturn(VERR_NOT_IMPLEMENTED);
}


/** Opcode 0x0f 0x01 /0. */
FNIEMOP_DEF(iemOp_Grp7_vmcall)
{
    AssertFailed();
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Opcode 0x0f 0x01 /0. */
FNIEMOP_DEF(iemOp_Grp7_vmlaunch)
{
    AssertFailed();
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Opcode 0x0f 0x01 /0. */
FNIEMOP_DEF(iemOp_Grp7_vmresume)
{
    AssertFailed();
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Opcode 0x0f 0x01 /0. */
FNIEMOP_DEF(iemOp_Grp7_vmxoff)
{
    AssertFailed();
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Opcode 0x0f 0x01 /1. */
FNIEMOP_DEF_1(iemOp_Grp7_sidt, uint8_t, bRm)
{
    AssertFailedReturn(VERR_NOT_IMPLEMENTED);
}


/** Opcode 0x0f 0x01 /1. */
FNIEMOP_DEF(iemOp_Grp7_monitor)
{
    AssertFailedReturn(VERR_NOT_IMPLEMENTED);
}


/** Opcode 0x0f 0x01 /1. */
FNIEMOP_DEF(iemOp_Grp7_mwait)
{
    AssertFailedReturn(VERR_NOT_IMPLEMENTED);
}


/** Opcode 0x0f 0x01 /2. */
FNIEMOP_DEF_1(iemOp_Grp7_lgdt, uint8_t, bRm)
{
    IEMOP_HLP_NO_LOCK_PREFIX();

    IEMMODE enmEffOpSize = pIemCpu->enmCpuMode == IEMMODE_64BIT
                         ? IEMMODE_64BIT
                         : pIemCpu->enmEffOpSize;
    IEM_MC_BEGIN(3, 1);
    IEM_MC_ARG_CONST(uint8_t,   iEffSeg, /*=*/pIemCpu->iEffSeg,     0);
    IEM_MC_ARG(RTGCPTR,         GCPtrEffSrc,                        1);
    IEM_MC_ARG_CONST(IEMMODE,   enmEffOpSizeArg,/*=*/enmEffOpSize,  2);
    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm);
    IEM_MC_CALL_CIMPL_3(iemCImpl_lgdt, iEffSeg, GCPtrEffSrc, enmEffOpSizeArg);
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x01 /2. */
FNIEMOP_DEF(iemOp_Grp7_xgetbv)
{
    AssertFailed();
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Opcode 0x0f 0x01 /2. */
FNIEMOP_DEF(iemOp_Grp7_xsetbv)
{
    AssertFailed();
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Opcode 0x0f 0x01 /3. */
FNIEMOP_DEF_1(iemOp_Grp7_lidt, uint8_t, bRm)
{
    IEMOP_HLP_NO_LOCK_PREFIX();

    IEMMODE enmEffOpSize = pIemCpu->enmCpuMode == IEMMODE_64BIT
                         ? IEMMODE_64BIT
                         : pIemCpu->enmEffOpSize;
    IEM_MC_BEGIN(3, 1);
    IEM_MC_ARG_CONST(uint8_t,   iEffSeg, /*=*/pIemCpu->iEffSeg,     0);
    IEM_MC_ARG(RTGCPTR,         GCPtrEffSrc,                        1);
    IEM_MC_ARG_CONST(IEMMODE,   enmEffOpSizeArg,/*=*/enmEffOpSize,  2);
    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm);
    IEM_MC_CALL_CIMPL_3(iemCImpl_lidt, iEffSeg, GCPtrEffSrc, enmEffOpSizeArg);
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x01 /4. */
FNIEMOP_DEF_1(iemOp_Grp7_smsw, uint8_t, bRm)
{
    IEMOP_HLP_NO_LOCK_PREFIX();
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint16_t, u16Tmp);
                IEM_MC_FETCH_CR0_U16(u16Tmp);
                IEM_MC_STORE_GREG_U16((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, u16Tmp);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint32_t, u32Tmp);
                IEM_MC_FETCH_CR0_U32(u32Tmp);
                IEM_MC_STORE_GREG_U32((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, u32Tmp);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint64_t, u64Tmp);
                IEM_MC_FETCH_CR0_U64(u64Tmp);
                IEM_MC_STORE_GREG_U64((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, u64Tmp);
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
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_FETCH_CR0_U16(u16Tmp);
        IEM_MC_STORE_MEM_U16(pIemCpu->iEffSeg, GCPtrEffDst, u16Tmp);
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
    IEMOP_HLP_NO_LOCK_PREFIX();
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEM_MC_BEGIN(1, 0);
        IEM_MC_ARG(uint16_t, u16Tmp, 0);
        IEM_MC_FETCH_GREG_U16(u16Tmp, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
        IEM_MC_CALL_CIMPL_1(iemCImpl_lmsw, u16Tmp);
        IEM_MC_END();
    }
    else
    {
        IEM_MC_BEGIN(1, 1);
        IEM_MC_ARG(uint16_t, u16Tmp, 0);
        IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_FETCH_MEM_U16(u16Tmp, pIemCpu->iEffSeg, GCPtrEffDst);
        IEM_MC_CALL_CIMPL_1(iemCImpl_lmsw, u16Tmp);
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x01 /7. */
FNIEMOP_DEF_1(iemOp_Grp7_invlpg, uint8_t, bRm)
{
    AssertFailedReturn(VERR_NOT_IMPLEMENTED);
}


/** Opcode 0x0f 0x01 /7. */
FNIEMOP_DEF(iemOp_Grp7_swapgs)
{
    AssertFailedReturn(VERR_NOT_IMPLEMENTED);
}


/** Opcode 0x0f 0x01 /7. */
FNIEMOP_DEF(iemOp_Grp7_rdtscp)
{
    AssertFailedReturn(VERR_NOT_IMPLEMENTED);
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
            return IEMOP_RAISE_INVALID_OPCODE();

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


/** Opcode 0x0f 0x02. */
FNIEMOP_STUB(iemOp_lar_Gv_Ew);
/** Opcode 0x0f 0x03. */
FNIEMOP_STUB(iemOp_lsl_Gv_Ew);
/** Opcode 0x0f 0x04. */
FNIEMOP_STUB(iemOp_syscall);


/** Opcode 0x0f 0x05. */
FNIEMOP_DEF(iemOp_clts)
{
    IEMOP_MNEMONIC("clts");
    IEMOP_HLP_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_clts);
}


/** Opcode 0x0f 0x06. */
FNIEMOP_STUB(iemOp_sysret);
/** Opcode 0x0f 0x08. */
FNIEMOP_STUB(iemOp_invd);
/** Opcode 0x0f 0x09. */
FNIEMOP_STUB(iemOp_wbinvd);
/** Opcode 0x0f 0x0b. */
FNIEMOP_STUB(iemOp_ud2);
/** Opcode 0x0f 0x0d. */
FNIEMOP_STUB(iemOp_nop_Ev_prefetch);
/** Opcode 0x0f 0x0e. */
FNIEMOP_STUB(iemOp_femms);
/** Opcode 0x0f 0x0f. */
FNIEMOP_STUB(iemOp_3Dnow);
/** Opcode 0x0f 0x10. */
FNIEMOP_STUB(iemOp_movups_Vps_Wps__movupd_Vpd_Wpd__movss_Vss_Wss__movsd_Vsd_Wsd);
/** Opcode 0x0f 0x11. */
FNIEMOP_STUB(iemOp_movups_Wps_Vps__movupd_Wpd_Vpd__movss_Wss_Vss__movsd_Vsd_Wsd);
/** Opcode 0x0f 0x12. */
FNIEMOP_STUB(iemOp_movlps_Vq_Mq__movhlps_Vq_Uq__movlpd_Vq_Mq__movsldup_Vq_Wq__movddup_Vq_Wq);
/** Opcode 0x0f 0x13. */
FNIEMOP_STUB(iemOp_movlps_Mq_Vq__movlpd_Mq_Vq);
/** Opcode 0x0f 0x14. */
FNIEMOP_STUB(iemOp_unpckhlps_Vps_Wq__unpcklpd_Vpd_Wq);
/** Opcode 0x0f 0x15. */
FNIEMOP_STUB(iemOp_unpckhps_Vps_Wq__unpckhpd_Vpd_Wq);
/** Opcode 0x0f 0x16. */
FNIEMOP_STUB(iemOp_movhps_Vq_Mq__movlhps_Vq_Uq__movhpd_Vq_Mq__movshdup_Vq_Wq);
/** Opcode 0x0f 0x17. */
FNIEMOP_STUB(iemOp_movhps_Mq_Vq__movhpd_Mq_Vq);
/** Opcode 0x0f 0x18. */
FNIEMOP_STUB(iemOp_prefetch_Grp16);


/** Opcode 0x0f 0x20. */
FNIEMOP_DEF(iemOp_mov_Rd_Cd)
{
    /* mod is ignored, as is operand size overrides. */
    IEMOP_MNEMONIC("mov Rd,Cd");
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
        pIemCpu->enmEffOpSize = pIemCpu->enmDefOpSize = IEMMODE_64BIT;
    else
        pIemCpu->enmEffOpSize = pIemCpu->enmDefOpSize = IEMMODE_32BIT;

    /** @todo Verify that the the invalid lock sequence exception (\#UD) is raised
     *        before the privilege level violation (\#GP). */
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    uint8_t iCrReg = ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg;
    if (pIemCpu->fPrefixes & IEM_OP_PRF_LOCK)
    {
        /* The lock prefix can be used to encode CR8 accesses on some CPUs. */
        if (!IEM_IS_AMD_CPUID_FEATURE_PRESENT_ECX(X86_CPUID_AMD_FEATURE_ECX_CR8L))
            return IEMOP_RAISE_INVALID_LOCK_PREFIX();
        iCrReg |= 8;
    }
    switch (iCrReg)
    {
        case 0: case 2: case 3: case 4: case 8:
            break;
        default:
            return IEMOP_RAISE_INVALID_OPCODE();
    }

    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_mov_Rd_Cd, (X86_MODRM_RM_MASK & bRm) | pIemCpu->uRexB, iCrReg);
}


/** Opcode 0x0f 0x21. */
FNIEMOP_DEF(iemOp_mov_Rd_Dd)
{
    IEMOP_MNEMONIC("mov Rd,Dd");
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX();
    if (pIemCpu->fPrefixes & IEM_OP_PRF_REX_R)
        return IEMOP_RAISE_INVALID_OPCODE();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_mov_Rd_Dd,
                                   (X86_MODRM_RM_MASK & bRm) | pIemCpu->uRexB,
                                   ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK));
}


/** Opcode 0x0f 0x22. */
FNIEMOP_DEF(iemOp_mov_Cd_Rd)
{
    /* mod is ignored, as is operand size overrides. */
    IEMOP_MNEMONIC("mov Cd,Rd");
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
        pIemCpu->enmEffOpSize = pIemCpu->enmDefOpSize = IEMMODE_64BIT;
    else
        pIemCpu->enmEffOpSize = pIemCpu->enmDefOpSize = IEMMODE_32BIT;

    /** @todo Verify that the the invalid lock sequence exception (\#UD) is raised
     *        before the privilege level violation (\#GP). */
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    uint8_t iCrReg = ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg;
    if (pIemCpu->fPrefixes & IEM_OP_PRF_LOCK)
    {
        /* The lock prefix can be used to encode CR8 accesses on some CPUs. */
        if (!IEM_IS_AMD_CPUID_FEATURE_PRESENT_ECX(X86_CPUID_AMD_FEATURE_ECX_CR8L))
            return IEMOP_RAISE_INVALID_LOCK_PREFIX();
        iCrReg |= 8;
    }
    switch (iCrReg)
    {
        case 0: case 2: case 3: case 4: case 8:
            break;
        default:
            return IEMOP_RAISE_INVALID_OPCODE();
    }

    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_mov_Cd_Rd, iCrReg, (X86_MODRM_RM_MASK & bRm) | pIemCpu->uRexB);
}


/** Opcode 0x0f 0x23. */
FNIEMOP_DEF(iemOp_mov_Dd_Rd)
{
    IEMOP_MNEMONIC("mov Dd,Rd");
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX();
    if (pIemCpu->fPrefixes & IEM_OP_PRF_REX_R)
        return IEMOP_RAISE_INVALID_OPCODE();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_mov_Dd_Rd,
                                   ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK),
                                   (X86_MODRM_RM_MASK & bRm) | pIemCpu->uRexB);
}


/** Opcode 0x0f 0x24. */
FNIEMOP_DEF(iemOp_mov_Rd_Td)
{
    IEMOP_MNEMONIC("mov Rd,Td");
/** @todo Is the invalid opcode raise before parsing any R/M byte? */
    return IEMOP_RAISE_INVALID_OPCODE();
}



/** Opcode 0x0f 0x26. */
FNIEMOP_DEF(iemOp_mov_Td_Rd)
{
    IEMOP_MNEMONIC("mov Td,Rd");
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Opcode 0x0f 0x28. */
FNIEMOP_STUB(iemOp_movaps_Vps_Wps__movapd_Vpd_Wpd);
/** Opcode 0x0f 0x29. */
FNIEMOP_STUB(iemOp_movaps_Wps_Vps__movapd_Wpd_Vpd);
/** Opcode 0x0f 0x2a. */
FNIEMOP_STUB(iemOp_cvtpi2ps_Vps_Qpi__cvtpi2pd_Vpd_Qpi__cvtsi2ss_Vss_Ey__cvtsi2sd_Vsd_Ey);
/** Opcode 0x0f 0x2b. */
FNIEMOP_STUB(iemOp_movntps_Mps_Vps__movntpd_Mpd_Vpd);
/** Opcode 0x0f 0x2c. */
FNIEMOP_STUB(iemOp_cvttps2pi_Ppi_Wps__cvttpd2pi_Ppi_Wpd__cvttss2si_Gy_Wss__cvttsd2si_Yu_Wsd);
/** Opcode 0x0f 0x2d. */
FNIEMOP_STUB(iemOp_cvtps2pi_Ppi_Wps__cvtpd2pi_QpiWpd__cvtss2si_Gy_Wss__cvtsd2si_Gy_Wsd);
/** Opcode 0x0f 0x2e. */
FNIEMOP_STUB(iemOp_ucomiss_Vss_Wss__ucomisd_Vsd_Wsd);
/** Opcode 0x0f 0x2f. */
FNIEMOP_STUB(iemOp_comiss_Vss_Wss__comisd_Vsd_Wsd);
/** Opcode 0x0f 0x30. */
FNIEMOP_STUB(iemOp_wrmsr);


/** Opcode 0x0f 0x31. */
FNIEMOP_DEF(iemOp_rdtsc)
{
    IEMOP_MNEMONIC("rdtsc");
    IEMOP_HLP_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_rdtsc);
}


/** Opcode 0x0f 0x33. */
FNIEMOP_STUB(iemOp_rdmsr);
/** Opcode 0x0f 0x34. */
FNIEMOP_STUB(iemOp_rdpmc);
/** Opcode 0x0f 0x34. */
FNIEMOP_STUB(iemOp_sysenter);
/** Opcode 0x0f 0x35. */
FNIEMOP_STUB(iemOp_sysexit);
/** Opcode 0x0f 0x37. */
FNIEMOP_STUB(iemOp_getsec);
/** Opcode 0x0f 0x38. */
FNIEMOP_STUB(iemOp_3byte_Esc_A4);
/** Opcode 0x0f 0x39. */
FNIEMOP_STUB(iemOp_3byte_Esc_A5);
/** Opcode 0x0f 0x3c (?). */
FNIEMOP_STUB(iemOp_movnti_Gv_Ev);

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
        switch (pIemCpu->enmEffOpSize) \
        { \
            case IEMMODE_16BIT: \
                IEM_MC_BEGIN(0, 1); \
                IEM_MC_LOCAL(uint16_t, u16Tmp); \
                a_Cnd { \
                    IEM_MC_FETCH_GREG_U16(u16Tmp, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB); \
                    IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u16Tmp); \
                } IEM_MC_ENDIF(); \
                IEM_MC_ADVANCE_RIP(); \
                IEM_MC_END(); \
                return VINF_SUCCESS; \
    \
            case IEMMODE_32BIT: \
                IEM_MC_BEGIN(0, 1); \
                IEM_MC_LOCAL(uint32_t, u32Tmp); \
                a_Cnd { \
                    IEM_MC_FETCH_GREG_U32(u32Tmp, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB); \
                    IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u32Tmp); \
                } IEM_MC_ELSE() { \
                    IEM_MC_CLEAR_HIGH_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg); \
                } IEM_MC_ENDIF(); \
                IEM_MC_ADVANCE_RIP(); \
                IEM_MC_END(); \
                return VINF_SUCCESS; \
    \
            case IEMMODE_64BIT: \
                IEM_MC_BEGIN(0, 1); \
                IEM_MC_LOCAL(uint64_t, u64Tmp); \
                a_Cnd { \
                    IEM_MC_FETCH_GREG_U64(u64Tmp, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB); \
                    IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u64Tmp); \
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
        switch (pIemCpu->enmEffOpSize) \
        { \
            case IEMMODE_16BIT: \
                IEM_MC_BEGIN(0, 2); \
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffSrc); \
                IEM_MC_LOCAL(uint16_t, u16Tmp); \
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm); \
                IEM_MC_FETCH_MEM_U16(u16Tmp, pIemCpu->iEffSeg, GCPtrEffSrc); \
                a_Cnd { \
                    IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u16Tmp); \
                } IEM_MC_ENDIF(); \
                IEM_MC_ADVANCE_RIP(); \
                IEM_MC_END(); \
                return VINF_SUCCESS; \
    \
            case IEMMODE_32BIT: \
                IEM_MC_BEGIN(0, 2); \
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffSrc); \
                IEM_MC_LOCAL(uint32_t, u32Tmp); \
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm); \
                IEM_MC_FETCH_MEM_U32(u32Tmp, pIemCpu->iEffSeg, GCPtrEffSrc); \
                a_Cnd { \
                    IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u32Tmp); \
                } IEM_MC_ELSE() { \
                    IEM_MC_CLEAR_HIGH_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg); \
                } IEM_MC_ENDIF(); \
                IEM_MC_ADVANCE_RIP(); \
                IEM_MC_END(); \
                return VINF_SUCCESS; \
    \
            case IEMMODE_64BIT: \
                IEM_MC_BEGIN(0, 2); \
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffSrc); \
                IEM_MC_LOCAL(uint64_t, u64Tmp); \
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm); \
                IEM_MC_FETCH_MEM_U64(u64Tmp, pIemCpu->iEffSeg, GCPtrEffSrc); \
                a_Cnd { \
                    IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u64Tmp); \
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
FNIEMOP_STUB(iemOp_addps_Vps_Wps__addpd_Vpd_Wpd__addss_Vss_Wss__addsd_Vsd_Wsd);
/** Opcode 0x0f 0x59. */
FNIEMOP_STUB(iemOp_mulps_Vps_Wps__mulpd_Vpd_Wpd__mulss_Vss__Wss__mulsd_Vsd_Wsd);
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
/** Opcode 0x0f 0x60. */
FNIEMOP_STUB(iemOp_punpcklbw_Pq_Qd__punpcklbw_Vdq_Wdq);
/** Opcode 0x0f 0x61. */
FNIEMOP_STUB(iemOp_punpcklwd_Pq_Qd__punpcklwd_Vdq_Wdq);
/** Opcode 0x0f 0x62. */
FNIEMOP_STUB(iemOp_punpckldq_Pq_Qd__punpckldq_Vdq_Wdq);
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
/** Opcode 0x0f 0x68. */
FNIEMOP_STUB(iemOp_punpckhbw_Pq_Qq__punpckhbw_Vdq_Wdq);
/** Opcode 0x0f 0x69. */
FNIEMOP_STUB(iemOp_punpckhwd_Pq_Qd__punpckhwd_Vdq_Wdq);
/** Opcode 0x0f 0x6a. */
FNIEMOP_STUB(iemOp_punpckhdq_Pq_Qd__punpckhdq_Vdq_Wdq);
/** Opcode 0x0f 0x6b. */
FNIEMOP_STUB(iemOp_packssdw_Pq_Qd__packssdq_Vdq_Wdq);
/** Opcode 0x0f 0x6c. */
FNIEMOP_STUB(iemOp_punpcklqdq_Vdq_Wdq);
/** Opcode 0x0f 0x6d. */
FNIEMOP_STUB(iemOp_punpckhqdq_Vdq_Wdq);
/** Opcode 0x0f 0x6e. */
FNIEMOP_STUB(iemOp_movd_q_Pd_Ey__movd_q_Vy_Ey);
/** Opcode 0x0f 0x6f. */
FNIEMOP_STUB(iemOp_movq_Pq_Qq__movdqa_Vdq_Wdq__movdqu_Vdq_Wdq);
/** Opcode 0x0f 0x70. */
FNIEMOP_STUB(iemOp_pshufw_Pq_Qq_Ib__pshufd_Vdq_Wdq_Ib__pshufhw_Vdq_Wdq_Ib__pshuflq_Vdq_Wdq_Ib);
/** Opcode 0x0f 0x71. */
FNIEMOP_STUB(iemOp_Grp12);
/** Opcode 0x0f 0x72. */
FNIEMOP_STUB(iemOp_Grp13);
/** Opcode 0x0f 0x73. */
FNIEMOP_STUB(iemOp_Grp14);
/** Opcode 0x0f 0x74. */
FNIEMOP_STUB(iemOp_pcmpeqb_Pq_Qq__pcmpeqb_Vdq_Wdq);
/** Opcode 0x0f 0x75. */
FNIEMOP_STUB(iemOp_pcmpeqw_Pq_Qq__pcmpeqw_Vdq_Wdq);
/** Opcode 0x0f 0x76. */
FNIEMOP_STUB(iemOp_pcmped_Pq_Qq__pcmpeqd_Vdq_Wdq);
/** Opcode 0x0f 0x77. */
FNIEMOP_STUB(iemOp_emms);
/** Opcode 0x0f 0x78. */
FNIEMOP_STUB(iemOp_vmread);
/** Opcode 0x0f 0x79. */
FNIEMOP_STUB(iemOp_vmwrite);
/** Opcode 0x0f 0x7c. */
FNIEMOP_STUB(iemOp_haddpd_Vdp_Wpd__haddps_Vps_Wps);
/** Opcode 0x0f 0x7d. */
FNIEMOP_STUB(iemOp_hsubpd_Vpd_Wpd__hsubps_Vps_Wps);
/** Opcode 0x0f 0x7e. */
FNIEMOP_STUB(iemOp_movd_q_Ey_Pd__movd_q_Ey_Vy__movq_Vq_Wq);
/** Opcode 0x0f 0x7f. */
FNIEMOP_STUB(iemOp_movq_Qq_Pq__movq_movdqa_Wdq_Vdq__movdqu_Wdq_Vdq);


/** Opcode 0x0f 0x80. */
FNIEMOP_DEF(iemOp_jo_Jv)
{
    IEMOP_MNEMONIC("jo  Jv");
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pIemCpu->enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_NO_LOCK_PREFIX();

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
        IEMOP_HLP_NO_LOCK_PREFIX();

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
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pIemCpu->enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_NO_LOCK_PREFIX();

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
        IEMOP_HLP_NO_LOCK_PREFIX();

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
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pIemCpu->enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_NO_LOCK_PREFIX();

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
        IEMOP_HLP_NO_LOCK_PREFIX();

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
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pIemCpu->enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_NO_LOCK_PREFIX();

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
        IEMOP_HLP_NO_LOCK_PREFIX();

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
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pIemCpu->enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_NO_LOCK_PREFIX();

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
        IEMOP_HLP_NO_LOCK_PREFIX();

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
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pIemCpu->enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_NO_LOCK_PREFIX();

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
        IEMOP_HLP_NO_LOCK_PREFIX();

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
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pIemCpu->enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_NO_LOCK_PREFIX();

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
        IEMOP_HLP_NO_LOCK_PREFIX();

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
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pIemCpu->enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_NO_LOCK_PREFIX();

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
        IEMOP_HLP_NO_LOCK_PREFIX();

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
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pIemCpu->enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_NO_LOCK_PREFIX();

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
        IEMOP_HLP_NO_LOCK_PREFIX();

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
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pIemCpu->enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_NO_LOCK_PREFIX();

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
        IEMOP_HLP_NO_LOCK_PREFIX();

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
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pIemCpu->enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_NO_LOCK_PREFIX();

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
        IEMOP_HLP_NO_LOCK_PREFIX();

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
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pIemCpu->enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_NO_LOCK_PREFIX();

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
        IEMOP_HLP_NO_LOCK_PREFIX();

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
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pIemCpu->enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_NO_LOCK_PREFIX();

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
        IEMOP_HLP_NO_LOCK_PREFIX();

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
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pIemCpu->enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_NO_LOCK_PREFIX();

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
        IEMOP_HLP_NO_LOCK_PREFIX();

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
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pIemCpu->enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_NO_LOCK_PREFIX();

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
        IEMOP_HLP_NO_LOCK_PREFIX();

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
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pIemCpu->enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_NO_LOCK_PREFIX();

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
        IEMOP_HLP_NO_LOCK_PREFIX();

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
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo too early? */

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_OF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_OF) {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 0);
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
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo too early? */

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_OF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_OF) {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 1);
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
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo too early? */

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_CF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_CF) {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 0);
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
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo too early? */

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_CF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_CF) {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 1);
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
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo too early? */

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_ZF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_ZF) {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 0);
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
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo too early? */

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_ZF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_ZF) {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 1);
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
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo too early? */

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_ANY_BITS_SET(X86_EFL_CF | X86_EFL_ZF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_IF_EFL_ANY_BITS_SET(X86_EFL_CF | X86_EFL_ZF) {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 0);
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
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo too early? */

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_ANY_BITS_SET(X86_EFL_CF | X86_EFL_ZF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_IF_EFL_ANY_BITS_SET(X86_EFL_CF | X86_EFL_ZF) {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 1);
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
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo too early? */

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_SF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_SF) {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 0);
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
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo too early? */

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_SF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_SF) {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 1);
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
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo too early? */

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_PF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_PF) {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 0);
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
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo too early? */

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_PF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_PF) {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 1);
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
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo too early? */

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BITS_NE(X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_IF_EFL_BITS_NE(X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 0);
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
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo too early? */

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BITS_NE(X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_IF_EFL_BITS_NE(X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 1);
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
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo too early? */

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET_OR_BITS_NE(X86_EFL_ZF, X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_IF_EFL_BIT_SET_OR_BITS_NE(X86_EFL_ZF, X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 0);
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
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo too early? */

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET_OR_BITS_NE(X86_EFL_ZF, X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_IF_EFL_BIT_SET_OR_BITS_NE(X86_EFL_ZF, X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pIemCpu->iEffSeg, GCPtrEffDst, 1);
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
    IEMOP_HLP_NO_LOCK_PREFIX();
    if (iReg < X86_SREG_FS)
        IEMOP_HLP_NO_64BIT();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    switch (pIemCpu->enmEffOpSize)
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
            IEM_MC_PUSH_U32(u32Value);
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
    IEMOP_HLP_NO_LOCK_PREFIX();
    return FNIEMOP_CALL_1(iemOpCommonPushSReg, X86_SREG_FS);
}


/** Opcode 0x0f 0xa1. */
FNIEMOP_DEF(iemOp_pop_fs)
{
    IEMOP_MNEMONIC("pop fs");
    IEMOP_HLP_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_pop_Sreg, X86_SREG_FS, pIemCpu->enmEffOpSize);
}


/** Opcode 0x0f 0xa2. */
FNIEMOP_DEF(iemOp_cpuid)
{
    IEMOP_MNEMONIC("cpuid");
    IEMOP_HLP_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_cpuid);
}


/**
 * Common worker for iemOp_bt_Ev_Gv, iemOp_btc_Ev_Gv, iemOp_btr_Ev_Gv and
 * iemOp_bts_Ev_Gv.
 */
FNIEMOP_DEF_1(iemOpCommonBit_Ev_Gv, PCIEMOPBINSIZES, pImpl)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX();
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register destination. */
        IEMOP_HLP_NO_LOCK_PREFIX();
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                0);
                IEM_MC_ARG(uint16_t,        u16Src,                 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                2);

                IEM_MC_FETCH_GREG_U16(u16Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_AND_LOCAL_U16(u16Src, 0xf);
                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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

                IEM_MC_FETCH_GREG_U32(u32Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_AND_LOCAL_U32(u32Src, 0x1f);
                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, u32Src, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint64_t *,      pu64Dst,                0);
                IEM_MC_ARG(uint64_t,        u64Src,                 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                2);

                IEM_MC_FETCH_GREG_U64(u64Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_AND_LOCAL_U64(u64Src, 0x3f);
                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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
        {
            IEMOP_HLP_NO_LOCK_PREFIX();
            fAccess = IEM_ACCESS_DATA_R;
        }

        /** @todo test negative bit offsets! */
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint16_t *,              pu16Dst,                0);
                IEM_MC_ARG(uint16_t,                u16Src,                 1);
                IEM_MC_ARG_LOCAL_EFLAGS(            pEFlags, EFlags,        2);
                IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);
                IEM_MC_LOCAL(int16_t,               i16AddrAdj);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_GREG_U16(u16Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_ASSIGN(i16AddrAdj, u16Src);
                IEM_MC_AND_ARG_U16(u16Src, 0x0f);
                IEM_MC_SAR_LOCAL_S16(i16AddrAdj, 4);
                IEM_MC_SAR_LOCAL_S16(i16AddrAdj, 1);
                IEM_MC_ADD_LOCAL_S16_TO_EFF_ADDR(GCPtrEffDst, i16AddrAdj);
                IEM_MC_FETCH_EFLAGS(EFlags);

                IEM_MC_MEM_MAP(pu16Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0);
                if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_GREG_U32(u32Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_ASSIGN(i32AddrAdj, u32Src);
                IEM_MC_AND_ARG_U32(u32Src, 0x1f);
                IEM_MC_SAR_LOCAL_S32(i32AddrAdj, 5);
                IEM_MC_SHL_LOCAL_S32(i32AddrAdj, 2);
                IEM_MC_ADD_LOCAL_S32_TO_EFF_ADDR(GCPtrEffDst, i32AddrAdj);
                IEM_MC_FETCH_EFLAGS(EFlags);

                IEM_MC_MEM_MAP(pu32Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0);
                if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_GREG_U64(u64Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_ASSIGN(i64AddrAdj, u64Src);
                IEM_MC_AND_ARG_U64(u64Src, 0x3f);
                IEM_MC_SAR_LOCAL_S64(i64AddrAdj, 6);
                IEM_MC_SHL_LOCAL_S64(i64AddrAdj, 3);
                IEM_MC_ADD_LOCAL_S64_TO_EFF_ADDR(GCPtrEffDst, i64AddrAdj);
                IEM_MC_FETCH_EFLAGS(EFlags);

                IEM_MC_MEM_MAP(pu64Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0);
                if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
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
    IEMOP_MNEMONIC("bt  Gv,Mp");
    return FNIEMOP_CALL_1(iemOpCommonBit_Ev_Gv, &g_iemAImpl_bt);
}


/**
 * Common worker for iemOp_shrd_Ev_Gv_Ib and iemOp_shld_Ev_Gv_Ib.
 */
FNIEMOP_DEF_1(iemOpCommonShldShrd_Ib, PCIEMOPSHIFTDBLSIZES, pImpl)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX();
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF | X86_EFL_OF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        uint8_t cShift; IEM_OPCODE_GET_NEXT_U8(&cShift);
        IEMOP_HLP_NO_LOCK_PREFIX();

        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(4, 0);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                0);
                IEM_MC_ARG(uint16_t,        u16Src,                 1);
                IEM_MC_ARG_CONST(uint8_t,   cShiftArg, /*=*/cShift, 2);
                IEM_MC_ARG(uint32_t *,      pEFlags,                3);

                IEM_MC_FETCH_GREG_U16(u16Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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

                IEM_MC_FETCH_GREG_U32(u32Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_4(pImpl->pfnNormalU32, pu32Dst, u32Src, cShiftArg, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(4, 0);
                IEM_MC_ARG(uint64_t *,      pu64Dst,                0);
                IEM_MC_ARG(uint64_t,        u64Src,                 1);
                IEM_MC_ARG_CONST(uint8_t,   cShiftArg, /*=*/cShift, 2);
                IEM_MC_ARG(uint32_t *,      pEFlags,                3);

                IEM_MC_FETCH_GREG_U64(u64Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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
        IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo too early? */

        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(4, 2);
                IEM_MC_ARG(uint16_t *,              pu16Dst,                0);
                IEM_MC_ARG(uint16_t,                u16Src,                 1);
                IEM_MC_ARG(uint8_t,                 cShiftArg,              2);
                IEM_MC_ARG_LOCAL_EFLAGS(            pEFlags, EFlags,        3);
                IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                uint8_t cShift; IEM_OPCODE_GET_NEXT_U8(&cShift);
                IEM_MC_ASSIGN(cShiftArg, cShift);
                IEM_MC_FETCH_GREG_U16(u16Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_MEM_MAP(pu16Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0);
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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                uint8_t cShift; IEM_OPCODE_GET_NEXT_U8(&cShift);
                IEM_MC_ASSIGN(cShiftArg, cShift);
                IEM_MC_FETCH_GREG_U32(u32Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_MEM_MAP(pu32Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0);
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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                uint8_t cShift; IEM_OPCODE_GET_NEXT_U8(&cShift);
                IEM_MC_ASSIGN(cShiftArg, cShift);
                IEM_MC_FETCH_GREG_U64(u64Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_MEM_MAP(pu64Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0);
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
    IEMOP_HLP_NO_LOCK_PREFIX();
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF | X86_EFL_OF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_NO_LOCK_PREFIX();

        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(4, 0);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                0);
                IEM_MC_ARG(uint16_t,        u16Src,                 1);
                IEM_MC_ARG(uint8_t,         cShiftArg,              2);
                IEM_MC_ARG(uint32_t *,      pEFlags,                3);

                IEM_MC_FETCH_GREG_U16(u16Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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

                IEM_MC_FETCH_GREG_U32(u32Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_4(pImpl->pfnNormalU32, pu32Dst, u32Src, cShiftArg, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(4, 0);
                IEM_MC_ARG(uint64_t *,      pu64Dst,                0);
                IEM_MC_ARG(uint64_t,        u64Src,                 1);
                IEM_MC_ARG(uint8_t,         cShiftArg,              2);
                IEM_MC_ARG(uint32_t *,      pEFlags,                3);

                IEM_MC_FETCH_GREG_U64(u64Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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
        IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo too early? */

        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(4, 2);
                IEM_MC_ARG(uint16_t *,              pu16Dst,                0);
                IEM_MC_ARG(uint16_t,                u16Src,                 1);
                IEM_MC_ARG(uint8_t,                 cShiftArg,              2);
                IEM_MC_ARG_LOCAL_EFLAGS(            pEFlags, EFlags,        3);
                IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_GREG_U16(u16Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_MEM_MAP(pu16Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0);
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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_GREG_U32(u32Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_MEM_MAP(pu32Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0);
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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_GREG_U64(u64Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_MEM_MAP(pu64Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0);
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
    return FNIEMOP_CALL_1(iemOpCommonShldShrd_Ib, &g_iemAImpl_shld);
}


/** Opcode 0x0f 0xa7. */
FNIEMOP_DEF(iemOp_shld_Ev_Gv_CL)
{
    IEMOP_MNEMONIC("shld Ev,Gv,CL");
    return FNIEMOP_CALL_1(iemOpCommonShldShrd_CL, &g_iemAImpl_shld);
}


/** Opcode 0x0f 0xa8. */
FNIEMOP_DEF(iemOp_push_gs)
{
    IEMOP_MNEMONIC("push gs");
    IEMOP_HLP_NO_LOCK_PREFIX();
    return FNIEMOP_CALL_1(iemOpCommonPushSReg, X86_SREG_GS);
}


/** Opcode 0x0f 0xa9. */
FNIEMOP_DEF(iemOp_pop_gs)
{
    IEMOP_MNEMONIC("pop gs");
    IEMOP_HLP_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_pop_Sreg, X86_SREG_GS, pIemCpu->enmEffOpSize);
}


/** Opcode 0x0f 0xaa. */
FNIEMOP_STUB(iemOp_rsm);


/** Opcode 0x0f 0xab. */
FNIEMOP_DEF(iemOp_bts_Ev_Gv)
{
    IEMOP_MNEMONIC("bts Gv,Mp");
    return FNIEMOP_CALL_1(iemOpCommonBit_Ev_Gv, &g_iemAImpl_bts);
}


/** Opcode 0x0f 0xac. */
FNIEMOP_DEF(iemOp_shrd_Ev_Gv_Ib)
{
    IEMOP_MNEMONIC("shrd Ev,Gv,Ib");
    return FNIEMOP_CALL_1(iemOpCommonShldShrd_Ib, &g_iemAImpl_shrd);
}


/** Opcode 0x0f 0xad. */
FNIEMOP_DEF(iemOp_shrd_Ev_Gv_CL)
{
    IEMOP_MNEMONIC("shrd Ev,Gv,CL");
    return FNIEMOP_CALL_1(iemOpCommonShldShrd_CL, &g_iemAImpl_shrd);
}


/** Opcode 0x0f 0xae. */
FNIEMOP_STUB(iemOp_Grp15);


/** Opcode 0x0f 0xaf. */
FNIEMOP_DEF(iemOp_imul_Gv_Ev)
{
    IEMOP_MNEMONIC("imul Gv,Ev");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rv_rm, &g_iemAImpl_imul_two);
}


/** Opcode 0x0f 0xb0. */
FNIEMOP_STUB(iemOp_cmpxchg_Eb_Gb);
/** Opcode 0x0f 0xb1. */
FNIEMOP_STUB(iemOp_cmpxchg_Ev_Gv);


FNIEMOP_DEF_1(iemOpCommonLoadSRegAndGreg, uint8_t, iSegReg)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */

    /* The source cannot be a register. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        return IEMOP_RAISE_INVALID_OPCODE();
    uint8_t const iGReg = ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg;

    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(5, 1);
            IEM_MC_ARG(uint16_t,        uSel,                                    0);
            IEM_MC_ARG(uint16_t,        offSeg,                                  1);
            IEM_MC_ARG_CONST(uint8_t,   iSegRegArg,/*=*/iSegReg,                 2);
            IEM_MC_ARG_CONST(uint8_t,   iGRegArg,  /*=*/iGReg,                   3);
            IEM_MC_ARG_CONST(IEMMODE,   enmEffOpSize,/*=*/pIemCpu->enmEffOpSize, 4);
            IEM_MC_LOCAL(RTGCPTR,       GCPtrEff);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEff, bRm);
            IEM_MC_FETCH_MEM_U16(offSeg, pIemCpu->iEffSeg, GCPtrEff);
            IEM_MC_FETCH_MEM_U16_DISP(uSel, pIemCpu->iEffSeg, GCPtrEff, 2);
            IEM_MC_CALL_CIMPL_5(iemCImpl_load_SReg_Greg, uSel, offSeg, iSegRegArg, iGRegArg, enmEffOpSize);
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(5, 1);
            IEM_MC_ARG(uint16_t,        uSel,                                    0);
            IEM_MC_ARG(uint32_t,        offSeg,                                  1);
            IEM_MC_ARG_CONST(uint8_t,   iSegRegArg,/*=*/iSegReg,                 2);
            IEM_MC_ARG_CONST(uint8_t,   iGRegArg,  /*=*/iGReg,                   3);
            IEM_MC_ARG_CONST(IEMMODE,   enmEffOpSize,/*=*/pIemCpu->enmEffOpSize, 4);
            IEM_MC_LOCAL(RTGCPTR,       GCPtrEff);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEff, bRm);
            IEM_MC_FETCH_MEM_U32(offSeg, pIemCpu->iEffSeg, GCPtrEff);
            IEM_MC_FETCH_MEM_U16_DISP(uSel, pIemCpu->iEffSeg, GCPtrEff, 4);
            IEM_MC_CALL_CIMPL_5(iemCImpl_load_SReg_Greg, uSel, offSeg, iSegRegArg, iGRegArg, enmEffOpSize);
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(5, 1);
            IEM_MC_ARG(uint16_t,        uSel,                                    0);
            IEM_MC_ARG(uint64_t,        offSeg,                                  1);
            IEM_MC_ARG_CONST(uint8_t,   iSegRegArg,/*=*/iSegReg,                 2);
            IEM_MC_ARG_CONST(uint8_t,   iGRegArg,  /*=*/iGReg,                   3);
            IEM_MC_ARG_CONST(IEMMODE,   enmEffOpSize,/*=*/pIemCpu->enmEffOpSize, 4);
            IEM_MC_LOCAL(RTGCPTR,       GCPtrEff);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEff, bRm);
            IEM_MC_FETCH_MEM_U64(offSeg, pIemCpu->iEffSeg, GCPtrEff);
            IEM_MC_FETCH_MEM_U16_DISP(uSel, pIemCpu->iEffSeg, GCPtrEff, 8);
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
    return FNIEMOP_CALL_1(iemOpCommonLoadSRegAndGreg, X86_SREG_SS);
}


/** Opcode 0x0f 0xb3. */
FNIEMOP_DEF(iemOp_btr_Ev_Gv)
{
    IEMOP_MNEMONIC("btr Gv,Mp");
    return FNIEMOP_CALL_1(iemOpCommonBit_Ev_Gv, &g_iemAImpl_btr);
}


/** Opcode 0x0f 0xb4. */
FNIEMOP_DEF(iemOp_lfs_Gv_Mp)
{
    IEMOP_MNEMONIC("lfs Gv,Mp");
    return FNIEMOP_CALL_1(iemOpCommonLoadSRegAndGreg, X86_SREG_FS);
}


/** Opcode 0x0f 0xb5. */
FNIEMOP_DEF(iemOp_lgs_Gv_Mp)
{
    IEMOP_MNEMONIC("lgs Gv,Mp");
    return FNIEMOP_CALL_1(iemOpCommonLoadSRegAndGreg, X86_SREG_GS);
}


/** Opcode 0x0f 0xb6. */
FNIEMOP_DEF(iemOp_movzx_Gv_Eb)
{
    IEMOP_MNEMONIC("movzx Gv,Eb");

    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint16_t, u16Value);
                IEM_MC_FETCH_GREG_U8_ZX_U16(u16Value, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u16Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint32_t, u32Value);
                IEM_MC_FETCH_GREG_U8_ZX_U32(u32Value, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u32Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint64_t, u64Value);
                IEM_MC_FETCH_GREG_U8_ZX_U64(u64Value, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u64Value);
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
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint16_t, u16Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_MEM_U8_ZX_U16(u16Value, pIemCpu->iEffSeg, GCPtrEffDst);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u16Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint32_t, u32Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_MEM_U8_ZX_U32(u32Value, pIemCpu->iEffSeg, GCPtrEffDst);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u32Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint64_t, u64Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_MEM_U8_ZX_U64(u64Value, pIemCpu->iEffSeg, GCPtrEffDst);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u64Value);
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

    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */

    /** @todo Not entirely sure how the operand size prefix is handled here,
     *        assuming that it will be ignored. Would be nice to have a few
     *        test for this. */
    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        if (pIemCpu->enmEffOpSize != IEMMODE_64BIT)
        {
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint32_t, u32Value);
            IEM_MC_FETCH_GREG_U16_ZX_U32(u32Value, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
            IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u32Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
        else
        {
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint64_t, u64Value);
            IEM_MC_FETCH_GREG_U16_ZX_U64(u64Value, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
            IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u64Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
    }
    else
    {
        /*
         * We're loading a register from memory.
         */
        if (pIemCpu->enmEffOpSize != IEMMODE_64BIT)
        {
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(uint32_t, u32Value);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
            IEM_MC_FETCH_MEM_U16_ZX_U32(u32Value, pIemCpu->iEffSeg, GCPtrEffDst);
            IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u32Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
        else
        {
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(uint64_t, u64Value);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
            IEM_MC_FETCH_MEM_U16_ZX_U64(u64Value, pIemCpu->iEffSeg, GCPtrEffDst);
            IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u64Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0xb8. */
FNIEMOP_STUB(iemOp_popcnt_Gv_Ev_jmpe);
/** Opcode 0x0f 0xb9. */
FNIEMOP_STUB(iemOp_Grp10);


/** Opcode 0x0f 0xba. */
FNIEMOP_DEF(iemOp_Grp8)
{
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
    IEMOP_HLP_NO_LOCK_PREFIX();
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register destination. */
        uint8_t u8Bit; IEM_OPCODE_GET_NEXT_U8(&u8Bit);
        IEMOP_HLP_NO_LOCK_PREFIX();

        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                    0);
                IEM_MC_ARG_CONST(uint16_t,  u16Src, /*=*/ u8Bit & 0x0f, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                    2);

                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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

                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, u32Src, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint64_t *,      pu64Dst,                    0);
                IEM_MC_ARG_CONST(uint64_t,  u64Src, /*=*/ u8Bit & 0x3f, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                    2);

                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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
        {
            IEMOP_HLP_NO_LOCK_PREFIX();
            fAccess = IEM_ACCESS_DATA_R;
        }

        /** @todo test negative bit offsets! */
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint16_t *,              pu16Dst,                0);
                IEM_MC_ARG(uint16_t,                u16Src,                 1);
                IEM_MC_ARG_LOCAL_EFLAGS(            pEFlags, EFlags,        2);
                IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                uint8_t u8Bit; IEM_OPCODE_GET_NEXT_U8(&u8Bit);
                IEM_MC_ASSIGN(u16Src, u8Bit & 0x0f);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_MEM_MAP(pu16Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0);
                if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU16, pu16Dst, u16Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnLockedU16, pu16Dst, u16Src, pEFlags);
                IEM_MC_MEM_COMMIT_AND_UNMAP(pu16Dst, IEM_ACCESS_DATA_RW);

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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                uint8_t u8Bit; IEM_OPCODE_GET_NEXT_U8(&u8Bit);
                IEM_MC_ASSIGN(u32Src, u8Bit & 0x1f);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_MEM_MAP(pu32Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0);
                if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, u32Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnLockedU32, pu32Dst, u32Src, pEFlags);
                IEM_MC_MEM_COMMIT_AND_UNMAP(pu32Dst, IEM_ACCESS_DATA_RW);

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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                uint8_t u8Bit; IEM_OPCODE_GET_NEXT_U8(&u8Bit);
                IEM_MC_ASSIGN(u64Src, u8Bit & 0x3f);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_MEM_MAP(pu64Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0);
                if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
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


/** Opcode 0x0f 0xbb. */
FNIEMOP_DEF(iemOp_btc_Ev_Gv)
{
    IEMOP_MNEMONIC("btc Gv,Mp");
    return FNIEMOP_CALL_1(iemOpCommonBit_Ev_Gv, &g_iemAImpl_btc);
}


/** Opcode 0x0f 0xbc. */
FNIEMOP_DEF(iemOp_bsf_Gv_Ev)
{
    IEMOP_MNEMONIC("bsf Gv,Ev");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF | X86_EFL_SF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rv_rm, &g_iemAImpl_bsf);
}


/** Opcode 0x0f 0xbd. */
FNIEMOP_DEF(iemOp_bsr_Gv_Ev)
{
    IEMOP_MNEMONIC("bsr Gv,Ev");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF | X86_EFL_SF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rv_rm, &g_iemAImpl_bsr);
}


/** Opcode 0x0f 0xbe. */
FNIEMOP_DEF(iemOp_movsx_Gv_Eb)
{
    IEMOP_MNEMONIC("movsx Gv,Eb");

    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint16_t, u16Value);
                IEM_MC_FETCH_GREG_U8_SX_U16(u16Value, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u16Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint32_t, u32Value);
                IEM_MC_FETCH_GREG_U8_SX_U32(u32Value, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u32Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint64_t, u64Value);
                IEM_MC_FETCH_GREG_U8_SX_U64(u64Value, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u64Value);
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
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint16_t, u16Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_MEM_U8_SX_U16(u16Value, pIemCpu->iEffSeg, GCPtrEffDst);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u16Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint32_t, u32Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_MEM_U8_SX_U32(u32Value, pIemCpu->iEffSeg, GCPtrEffDst);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u32Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint64_t, u64Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_MEM_U8_SX_U64(u64Value, pIemCpu->iEffSeg, GCPtrEffDst);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u64Value);
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

    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */

    /** @todo Not entirely sure how the operand size prefix is handled here,
     *        assuming that it will be ignored. Would be nice to have a few
     *        test for this. */
    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        if (pIemCpu->enmEffOpSize != IEMMODE_64BIT)
        {
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint32_t, u32Value);
            IEM_MC_FETCH_GREG_U16_SX_U32(u32Value, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
            IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u32Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
        else
        {
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint64_t, u64Value);
            IEM_MC_FETCH_GREG_U16_SX_U64(u64Value, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
            IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u64Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
    }
    else
    {
        /*
         * We're loading a register from memory.
         */
        if (pIemCpu->enmEffOpSize != IEMMODE_64BIT)
        {
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(uint32_t, u32Value);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
            IEM_MC_FETCH_MEM_U16_SX_U32(u32Value, pIemCpu->iEffSeg, GCPtrEffDst);
            IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u32Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
        else
        {
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(uint64_t, u64Value);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
            IEM_MC_FETCH_MEM_U16_SX_U64(u64Value, pIemCpu->iEffSeg, GCPtrEffDst);
            IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u64Value);
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
    IEMOP_MNEMONIC("xadd Eb,Gb");

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(3, 0);
        IEM_MC_ARG(uint8_t *,  pu8Dst,  0);
        IEM_MC_ARG(uint8_t *,  pu8Reg,  1);
        IEM_MC_ARG(uint32_t *, pEFlags, 2);

        IEM_MC_REF_GREG_U8(pu8Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
        IEM_MC_REF_GREG_U8(pu8Reg, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_MEM_MAP(pu8Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
        IEM_MC_FETCH_GREG_U8(u8RegCopy, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
        IEM_MC_REF_LOCAL(pu8Reg, u8RegCopy);
        IEM_MC_FETCH_EFLAGS(EFlags);
        if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
            IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_xadd_u8, pu8Dst, pu8Reg, pEFlags);
        else
            IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_xadd_u8_locked, pu8Dst, pu8Reg, pEFlags);

        IEM_MC_MEM_COMMIT_AND_UNMAP(pu8Dst, IEM_ACCESS_DATA_RW);
        IEM_MC_COMMIT_EFLAGS(EFlags);
        IEM_MC_STORE_GREG_U8((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, u8RegCopy);
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
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_NO_LOCK_PREFIX();

        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *, pu16Dst,  0);
                IEM_MC_ARG(uint16_t *, pu16Reg,  1);
                IEM_MC_ARG(uint32_t *, pEFlags, 2);

                IEM_MC_REF_GREG_U16(pu16Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_REF_GREG_U16(pu16Reg, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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

                IEM_MC_REF_GREG_U32(pu32Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_REF_GREG_U32(pu32Reg, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_xadd_u32, pu32Dst, pu32Reg, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint64_t *, pu64Dst,  0);
                IEM_MC_ARG(uint64_t *, pu64Reg,  1);
                IEM_MC_ARG(uint32_t *, pEFlags, 2);

                IEM_MC_REF_GREG_U64(pu64Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_REF_GREG_U64(pu64Reg, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 3);
                IEM_MC_ARG(uint16_t *,  pu16Dst,         0);
                IEM_MC_ARG(uint16_t *,  pu16Reg,         1);
                IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
                IEM_MC_LOCAL(uint16_t,  u16RegCopy);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_MEM_MAP(pu16Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_GREG_U16(u16RegCopy, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_LOCAL(pu16Reg, u16RegCopy);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_xadd_u16, pu16Dst, pu16Reg, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_xadd_u16_locked, pu16Dst, pu16Reg, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu16Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_STORE_GREG_U16((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, u16RegCopy);
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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_MEM_MAP(pu32Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_GREG_U32(u32RegCopy, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_LOCAL(pu32Reg, u32RegCopy);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_xadd_u32, pu32Dst, pu32Reg, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_xadd_u32_locked, pu32Dst, pu32Reg, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu32Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_STORE_GREG_U32((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, u32RegCopy);
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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_MEM_MAP(pu64Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_GREG_U64(u64RegCopy, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_LOCAL(pu64Reg, u64RegCopy);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_xadd_u64, pu64Dst, pu64Reg, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_xadd_u64_locked, pu64Dst, pu64Reg, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu64Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_STORE_GREG_U64((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, u64RegCopy);
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
FNIEMOP_STUB(iemOp_movnti_My_Gy);
/** Opcode 0x0f 0xc4. */
FNIEMOP_STUB(iemOp_pinsrw_Pq_Ry_Mw_Ib__pinsrw_Vdq_Ry_Mw_Ib);
/** Opcode 0x0f 0xc5. */
FNIEMOP_STUB(iemOp_pextrw_Gd_Nq_Ib__pextrw_Gd_Udq_Ib);
/** Opcode 0x0f 0xc6. */
FNIEMOP_STUB(iemOp_shufps_Vps_Wps_Ib__shufdp_Vpd_Wpd_Ib);
/** Opcode 0x0f 0xc7. */
FNIEMOP_STUB(iemOp_Grp9);
/** Opcode 0x0f 0xc8. */
FNIEMOP_STUB(iemOp_bswap_rAX_r8);
/** Opcode 0x0f 0xc9. */
FNIEMOP_STUB(iemOp_bswap_rCX_r9);
/** Opcode 0x0f 0xca. */
FNIEMOP_STUB(iemOp_bswap_rDX_r10);
/** Opcode 0x0f 0xcb. */
FNIEMOP_STUB(iemOp_bswap_rBX_r11);
/** Opcode 0x0f 0xcc. */
FNIEMOP_STUB(iemOp_bswap_rSP_r12);
/** Opcode 0x0f 0xcd. */
FNIEMOP_STUB(iemOp_bswap_rBP_r13);
/** Opcode 0x0f 0xce. */
FNIEMOP_STUB(iemOp_bswap_rSI_r14);
/** Opcode 0x0f 0xcf. */
FNIEMOP_STUB(iemOp_bswap_rDI_r15);
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
FNIEMOP_STUB(iemOp_pmovmskb_Gd_Nq__pmovmskb_Gd_Udq);
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
FNIEMOP_STUB(iemOp_movntq_Mq_Pq__movntdq_Mdq_Vdq);
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
FNIEMOP_STUB(iemOp_pxor_Pq_Qq__pxor_Vdq_Wdq);
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
FNIEMOP_STUB(iemOp_psubb_Pq_Qq_psubb_Vdq_Wdq);
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


const PFNIEMOP g_apfnTwoByteMap[256] =
{
    /* 0x00 */  iemOp_Grp6,             iemOp_Grp7,             iemOp_lar_Gv_Ew,        iemOp_lsl_Gv_Ew,
    /* 0x04 */  iemOp_Invalid,          iemOp_syscall,          iemOp_clts,             iemOp_sysret,
    /* 0x08 */  iemOp_invd,             iemOp_wbinvd,           iemOp_Invalid,          iemOp_ud2,
    /* 0x0c */  iemOp_Invalid,          iemOp_nop_Ev_prefetch,  iemOp_femms,            iemOp_3Dnow,
    /* 0x10 */  iemOp_movups_Vps_Wps__movupd_Vpd_Wpd__movss_Vss_Wss__movsd_Vsd_Wsd,
    /* 0x11 */  iemOp_movups_Wps_Vps__movupd_Wpd_Vpd__movss_Wss_Vss__movsd_Vsd_Wsd,
    /* 0x12 */  iemOp_movlps_Vq_Mq__movhlps_Vq_Uq__movlpd_Vq_Mq__movsldup_Vq_Wq__movddup_Vq_Wq,
    /* 0x13 */  iemOp_movlps_Mq_Vq__movlpd_Mq_Vq,
    /* 0x14 */  iemOp_unpckhlps_Vps_Wq__unpcklpd_Vpd_Wq,
    /* 0x15 */  iemOp_unpckhps_Vps_Wq__unpckhpd_Vpd_Wq,
    /* 0x16 */  iemOp_movhps_Vq_Mq__movlhps_Vq_Uq__movhpd_Vq_Mq__movshdup_Vq_Wq,
    /* 0x17 */  iemOp_movhps_Mq_Vq__movhpd_Mq_Vq,
    /* 0x18 */  iemOp_prefetch_Grp16,   iemOp_Invalid,          iemOp_Invalid,          iemOp_Invalid,
    /* 0x1c */  iemOp_Invalid,          iemOp_Invalid,          iemOp_Invalid,          iemOp_Invalid,
    /* 0x20 */  iemOp_mov_Rd_Cd,        iemOp_mov_Rd_Dd,        iemOp_mov_Cd_Rd,        iemOp_mov_Dd_Rd,
    /* 0x24 */  iemOp_mov_Rd_Td,        iemOp_Invalid,          iemOp_mov_Td_Rd,        iemOp_Invalid,
    /* 0x28 */  iemOp_movaps_Vps_Wps__movapd_Vpd_Wpd,
    /* 0x29 */  iemOp_movaps_Wps_Vps__movapd_Wpd_Vpd,
    /* 0x2a */  iemOp_cvtpi2ps_Vps_Qpi__cvtpi2pd_Vpd_Qpi__cvtsi2ss_Vss_Ey__cvtsi2sd_Vsd_Ey,
    /* 0x2b */  iemOp_movntps_Mps_Vps__movntpd_Mpd_Vpd,
    /* 0x2c */  iemOp_cvttps2pi_Ppi_Wps__cvttpd2pi_Ppi_Wpd__cvttss2si_Gy_Wss__cvttsd2si_Yu_Wsd,
    /* 0x2d */  iemOp_cvtps2pi_Ppi_Wps__cvtpd2pi_QpiWpd__cvtss2si_Gy_Wss__cvtsd2si_Gy_Wsd,
    /* 0x2e */  iemOp_ucomiss_Vss_Wss__ucomisd_Vsd_Wsd,
    /* 0x2f */  iemOp_comiss_Vss_Wss__comisd_Vsd_Wsd,
    /* 0x30 */  iemOp_wrmsr,            iemOp_rdtsc,            iemOp_rdmsr,            iemOp_rdpmc,
    /* 0x34 */  iemOp_sysenter,         iemOp_sysexit,          iemOp_Invalid,          iemOp_getsec,
    /* 0x38 */  iemOp_3byte_Esc_A4,     iemOp_Invalid,          iemOp_3byte_Esc_A5,     iemOp_Invalid,
    /* 0x3c */  iemOp_movnti_Gv_Ev/*?*/,iemOp_Invalid,          iemOp_Invalid,          iemOp_Invalid,
    /* 0x40 */  iemOp_cmovo_Gv_Ev,      iemOp_cmovno_Gv_Ev,     iemOp_cmovc_Gv_Ev,      iemOp_cmovnc_Gv_Ev,
    /* 0x44 */  iemOp_cmove_Gv_Ev,      iemOp_cmovne_Gv_Ev,     iemOp_cmovbe_Gv_Ev,     iemOp_cmovnbe_Gv_Ev,
    /* 0x48 */  iemOp_cmovs_Gv_Ev,      iemOp_cmovns_Gv_Ev,     iemOp_cmovp_Gv_Ev,      iemOp_cmovnp_Gv_Ev,
    /* 0x4c */  iemOp_cmovl_Gv_Ev,      iemOp_cmovnl_Gv_Ev,     iemOp_cmovle_Gv_Ev,     iemOp_cmovnle_Gv_Ev,
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
    /* 0x78 */  iemOp_vmread,           iemOp_vmwrite,          iemOp_Invalid,          iemOp_Invalid,
    /* 0x7c */  iemOp_haddpd_Vdp_Wpd__haddps_Vps_Wps,
    /* 0x7d */  iemOp_hsubpd_Vpd_Wpd__hsubps_Vps_Wps,
    /* 0x7e */  iemOp_movd_q_Ey_Pd__movd_q_Ey_Vy__movq_Vq_Wq,
    /* 0x7f */  iemOp_movq_Qq_Pq__movq_movdqa_Wdq_Vdq__movdqu_Wdq_Vdq,
    /* 0x80 */  iemOp_jo_Jv,            iemOp_jno_Jv,           iemOp_jc_Jv,            iemOp_jnc_Jv,
    /* 0x84 */  iemOp_je_Jv,            iemOp_jne_Jv,           iemOp_jbe_Jv,           iemOp_jnbe_Jv,
    /* 0x88 */  iemOp_js_Jv,            iemOp_jns_Jv,           iemOp_jp_Jv,            iemOp_jnp_Jv,
    /* 0x8c */  iemOp_jl_Jv,            iemOp_jnl_Jv,           iemOp_jle_Jv,           iemOp_jnle_Jv,
    /* 0x90 */  iemOp_seto_Eb,          iemOp_setno_Eb,         iemOp_setc_Eb,          iemOp_setnc_Eb,
    /* 0x94 */  iemOp_sete_Eb,          iemOp_setne_Eb,         iemOp_setbe_Eb,         iemOp_setnbe_Eb,
    /* 0x98 */  iemOp_sets_Eb,          iemOp_setns_Eb,         iemOp_setp_Eb,          iemOp_setnp_Eb,
    /* 0x9c */  iemOp_setl_Eb,          iemOp_setnl_Eb,         iemOp_setle_Eb,         iemOp_setnle_Eb,
    /* 0xa0 */  iemOp_push_fs,          iemOp_pop_fs,           iemOp_cpuid,            iemOp_bt_Ev_Gv,
    /* 0xa4 */  iemOp_shld_Ev_Gv_Ib,    iemOp_shld_Ev_Gv_CL,    iemOp_Invalid,          iemOp_Invalid,
    /* 0xa8 */  iemOp_push_gs,          iemOp_pop_gs,           iemOp_rsm,              iemOp_bts_Ev_Gv,
    /* 0xac */  iemOp_shrd_Ev_Gv_Ib,    iemOp_shrd_Ev_Gv_CL,    iemOp_Grp15,            iemOp_imul_Gv_Ev,
    /* 0xb0 */  iemOp_cmpxchg_Eb_Gb,    iemOp_cmpxchg_Ev_Gv,    iemOp_lss_Gv_Mp,        iemOp_btr_Ev_Gv,
    /* 0xb4 */  iemOp_lfs_Gv_Mp,        iemOp_lgs_Gv_Mp,        iemOp_movzx_Gv_Eb,      iemOp_movzx_Gv_Ew,
    /* 0xb8 */  iemOp_popcnt_Gv_Ev_jmpe,iemOp_Grp10,            iemOp_Grp8,             iemOp_btc_Ev_Gv,
    /* 0xbc */  iemOp_bsf_Gv_Ev,        iemOp_bsr_Gv_Ev,        iemOp_movsx_Gv_Eb,      iemOp_movsx_Gv_Ew,
    /* 0xc0 */  iemOp_xadd_Eb_Gb,
    /* 0xc1 */  iemOp_xadd_Ev_Gv,
    /* 0xc2 */  iemOp_cmpps_Vps_Wps_Ib__cmppd_Vpd_Wpd_Ib__cmpss_Vss_Wss_Ib__cmpsd_Vsd_Wsd_Ib,
    /* 0xc3 */  iemOp_movnti_My_Gy,
    /* 0xc4 */  iemOp_pinsrw_Pq_Ry_Mw_Ib__pinsrw_Vdq_Ry_Mw_Ib,
    /* 0xc5 */  iemOp_pextrw_Gd_Nq_Ib__pextrw_Gd_Udq_Ib,
    /* 0xc6 */  iemOp_shufps_Vps_Wps_Ib__shufdp_Vpd_Wpd_Ib,
    /* 0xc7 */  iemOp_Grp9,
    /* 0xc8 */  iemOp_bswap_rAX_r8,     iemOp_bswap_rCX_r9,     iemOp_bswap_rDX_r10,    iemOp_bswap_rBX_r11,
    /* 0xcc */  iemOp_bswap_rSP_r12,    iemOp_bswap_rBP_r13,    iemOp_bswap_rSI_r14,    iemOp_bswap_rDI_r15,
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
    IEMOP_HLP_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_pop_Sreg, X86_SREG_ES, pIemCpu->enmEffOpSize);
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
    IEMOP_HLP_NO_LOCK_PREFIX();
    IEMOP_HLP_NO_64BIT();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_pop_Sreg, X86_SREG_SS, pIemCpu->enmEffOpSize);
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
    IEMOP_HLP_NO_LOCK_PREFIX();
    IEMOP_HLP_NO_64BIT();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_pop_Sreg, X86_SREG_DS, pIemCpu->enmEffOpSize);
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
    pIemCpu->fPrefixes |= IEM_OP_PRF_SEG_ES;
    pIemCpu->iEffSeg    = X86_SREG_ES;

    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    return FNIEMOP_CALL(g_apfnOneByteMap[b]);
}


/** Opcode 0x27. */
FNIEMOP_STUB(iemOp_daa);


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
    pIemCpu->fPrefixes |= IEM_OP_PRF_SEG_CS;
    pIemCpu->iEffSeg    = X86_SREG_CS;

    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    return FNIEMOP_CALL(g_apfnOneByteMap[b]);
}


/** Opcode 0x2f. */
FNIEMOP_STUB(iemOp_das);


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
    pIemCpu->fPrefixes |= IEM_OP_PRF_SEG_SS;
    pIemCpu->iEffSeg    = X86_SREG_SS;

    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    return FNIEMOP_CALL(g_apfnOneByteMap[b]);
}


/** Opcode 0x37. */
FNIEMOP_STUB(iemOp_aaa);


/** Opcode 0x38. */
FNIEMOP_DEF(iemOp_cmp_Eb_Gb)
{
    IEMOP_MNEMONIC("cmp Eb,Gb");
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo do we have to decode the whole instruction first?  */
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rm_r8, &g_iemAImpl_cmp);
}


/** Opcode 0x39. */
FNIEMOP_DEF(iemOp_cmp_Ev_Gv)
{
    IEMOP_MNEMONIC("cmp Ev,Gv");
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo do we have to decode the whole instruction first?  */
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
    pIemCpu->fPrefixes |= IEM_OP_PRF_SEG_DS;
    pIemCpu->iEffSeg    = X86_SREG_DS;

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
    IEMOP_HLP_NO_LOCK_PREFIX();
    switch (pIemCpu->enmEffOpSize)
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
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        pIemCpu->fPrefixes |= IEM_OP_PRF_REX;

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
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        pIemCpu->fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_B;
        pIemCpu->uRexB     = 1 << 3;

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
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        pIemCpu->fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_X;
        pIemCpu->uRexIndex = 1 << 3;

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
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        pIemCpu->fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_B | IEM_OP_PRF_REX_X;
        pIemCpu->uRexB     = 1 << 3;
        pIemCpu->uRexIndex = 1 << 3;

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
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        pIemCpu->fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_R;
        pIemCpu->uRexReg   = 1 << 3;

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
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        pIemCpu->fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_R | IEM_OP_PRF_REX_B;
        pIemCpu->uRexReg   = 1 << 3;
        pIemCpu->uRexB     = 1 << 3;

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
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        pIemCpu->fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_R | IEM_OP_PRF_REX_X;
        pIemCpu->uRexReg   = 1 << 3;
        pIemCpu->uRexIndex = 1 << 3;

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
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        pIemCpu->fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_R | IEM_OP_PRF_REX_B | IEM_OP_PRF_REX_X;
        pIemCpu->uRexReg   = 1 << 3;
        pIemCpu->uRexB     = 1 << 3;
        pIemCpu->uRexIndex = 1 << 3;

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
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        pIemCpu->fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_SIZE_REX_W;
        iemRecalEffOpSize(pIemCpu);

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
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        pIemCpu->fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_B | IEM_OP_PRF_SIZE_REX_W;
        pIemCpu->uRexB     = 1 << 3;
        iemRecalEffOpSize(pIemCpu);

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
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        pIemCpu->fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_X | IEM_OP_PRF_SIZE_REX_W;
        pIemCpu->uRexIndex = 1 << 3;
        iemRecalEffOpSize(pIemCpu);

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
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        pIemCpu->fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_B | IEM_OP_PRF_REX_X | IEM_OP_PRF_SIZE_REX_W;
        pIemCpu->uRexB     = 1 << 3;
        pIemCpu->uRexIndex = 1 << 3;
        iemRecalEffOpSize(pIemCpu);

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
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        pIemCpu->fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_R | IEM_OP_PRF_SIZE_REX_W;
        pIemCpu->uRexReg   = 1 << 3;
        iemRecalEffOpSize(pIemCpu);

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
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        pIemCpu->fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_R | IEM_OP_PRF_REX_B | IEM_OP_PRF_SIZE_REX_W;
        pIemCpu->uRexReg   = 1 << 3;
        pIemCpu->uRexB     = 1 << 3;
        iemRecalEffOpSize(pIemCpu);

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
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        pIemCpu->fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_R | IEM_OP_PRF_REX_X | IEM_OP_PRF_SIZE_REX_W;
        pIemCpu->uRexReg   = 1 << 3;
        pIemCpu->uRexIndex = 1 << 3;
        iemRecalEffOpSize(pIemCpu);

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
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        pIemCpu->fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_R | IEM_OP_PRF_REX_B | IEM_OP_PRF_REX_X | IEM_OP_PRF_SIZE_REX_W;
        pIemCpu->uRexReg   = 1 << 3;
        pIemCpu->uRexB     = 1 << 3;
        pIemCpu->uRexIndex = 1 << 3;
        iemRecalEffOpSize(pIemCpu);

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
    IEMOP_HLP_NO_LOCK_PREFIX();
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        iReg |= pIemCpu->uRexB;
        pIemCpu->enmDefOpSize = IEMMODE_64BIT;
        pIemCpu->enmEffOpSize = !(pIemCpu->fPrefixes & IEM_OP_PRF_SIZE_OP) ? IEMMODE_64BIT : IEMMODE_16BIT;
    }

    switch (pIemCpu->enmEffOpSize)
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
    IEMOP_HLP_NO_LOCK_PREFIX();
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        iReg |= pIemCpu->uRexB;
        pIemCpu->enmDefOpSize = IEMMODE_64BIT;
        pIemCpu->enmEffOpSize = !(pIemCpu->fPrefixes & IEM_OP_PRF_SIZE_OP) ? IEMMODE_64BIT : IEMMODE_16BIT;
    }

/** @todo How does this code handle iReg==X86_GREG_xSP. How does a real CPU
 *        handle it, for that matter (Intel pseudo code hints that the popped
 *        value is incremented by the stack item size.)  Test it, both encodings
 *        and all three register sizes. */
    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint16_t, *pu16Dst);
            IEM_MC_REF_GREG_U16(pu16Dst, iReg);
            IEM_MC_POP_U16(pu16Dst);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            break;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint32_t, *pu32Dst);
            IEM_MC_REF_GREG_U32(pu32Dst, iReg);
            IEM_MC_POP_U32(pu32Dst);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            break;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint64_t, *pu64Dst);
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
    return FNIEMOP_CALL_1(iemOpCommonPopGReg, X86_GREG_xSP);
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
    IEMOP_HLP_NO_64BIT();
    if (pIemCpu->enmEffOpSize == IEMMODE_16BIT)
        return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_pusha_16);
    Assert(pIemCpu->enmEffOpSize == IEMMODE_32BIT);
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_pusha_32);
}


/** Opcode 0x61. */
FNIEMOP_DEF(iemOp_popa)
{
    IEMOP_MNEMONIC("popa");
    IEMOP_HLP_NO_64BIT();
    if (pIemCpu->enmEffOpSize == IEMMODE_16BIT)
        return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_popa_16);
    Assert(pIemCpu->enmEffOpSize == IEMMODE_32BIT);
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_popa_32);
}


/** Opcode 0x62. */
FNIEMOP_STUB(iemOp_bound_Gv_Ma);
/** Opcode 0x63. */
FNIEMOP_STUB(iemOp_arpl_Ew_Gw);


/** Opcode 0x64. */
FNIEMOP_DEF(iemOp_seg_FS)
{
    pIemCpu->fPrefixes |= IEM_OP_PRF_SEG_FS;
    pIemCpu->iEffSeg    = X86_SREG_FS;

    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    return FNIEMOP_CALL(g_apfnOneByteMap[b]);
}


/** Opcode 0x65. */
FNIEMOP_DEF(iemOp_seg_GS)
{
    pIemCpu->fPrefixes |= IEM_OP_PRF_SEG_GS;
    pIemCpu->iEffSeg    = X86_SREG_GS;

    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    return FNIEMOP_CALL(g_apfnOneByteMap[b]);
}


/** Opcode 0x66. */
FNIEMOP_DEF(iemOp_op_size)
{
    pIemCpu->fPrefixes |= IEM_OP_PRF_SIZE_OP;
    iemRecalEffOpSize(pIemCpu);

    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    return FNIEMOP_CALL(g_apfnOneByteMap[b]);
}


/** Opcode 0x67. */
FNIEMOP_DEF(iemOp_addr_size)
{
    pIemCpu->fPrefixes |= IEM_OP_PRF_SIZE_ADDR;
    switch (pIemCpu->enmDefAddrMode)
    {
        case IEMMODE_16BIT: pIemCpu->enmEffAddrMode = IEMMODE_32BIT; break;
        case IEMMODE_32BIT: pIemCpu->enmEffAddrMode = IEMMODE_16BIT; break;
        case IEMMODE_64BIT: pIemCpu->enmEffAddrMode = IEMMODE_32BIT; break;
        default: AssertFailed();
    }

    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    return FNIEMOP_CALL(g_apfnOneByteMap[b]);
}


/** Opcode 0x68. */
FNIEMOP_DEF(iemOp_push_Iz)
{
    IEMOP_MNEMONIC("push Iz");
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
        {
            uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
            IEMOP_HLP_NO_LOCK_PREFIX();
            IEM_MC_BEGIN(0,0);
            IEM_MC_PUSH_U16(u16Imm);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;
        }

        case IEMMODE_32BIT:
        {
            uint32_t u32Imm; IEM_OPCODE_GET_NEXT_U32(&u32Imm);
            IEMOP_HLP_NO_LOCK_PREFIX();
            IEM_MC_BEGIN(0,0);
            IEM_MC_PUSH_U32(u32Imm);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;
        }

        case IEMMODE_64BIT:
        {
            uint64_t u64Imm; IEM_OPCODE_GET_NEXT_S32_SX_U64(&u64Imm);
            IEMOP_HLP_NO_LOCK_PREFIX();
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
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF);

    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
        {
            uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
            IEMOP_HLP_NO_LOCK_PREFIX();
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /* register operand */
                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint16_t *,      pu16Dst,            0);
                IEM_MC_ARG_CONST(uint16_t,  u16Src,/*=*/ u16Imm,1);
                IEM_MC_ARG(uint32_t *,      pEFlags,            2);
                IEM_MC_LOCAL(uint16_t,      u16Tmp);

                IEM_MC_FETCH_GREG_U16(u16Tmp, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_LOCAL(pu16Dst, u16Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u16, pu16Dst, u16Src, pEFlags);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u16Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /* memory operand */
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint16_t *,      pu16Dst,            0);
                IEM_MC_ARG_CONST(uint16_t,  u16Src,/*=*/ u16Imm,1);
                IEM_MC_ARG(uint32_t *,      pEFlags,            2);
                IEM_MC_LOCAL(uint16_t,      u16Tmp);
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_MEM_U16(u16Tmp, pIemCpu->iEffSeg, GCPtrEffDst);
                IEM_MC_REF_LOCAL(pu16Dst, u16Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u16, pu16Dst, u16Src, pEFlags);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u16Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;
        }

        case IEMMODE_32BIT:
        {
            uint32_t u32Imm; IEM_OPCODE_GET_NEXT_U32(&u32Imm);
            IEMOP_HLP_NO_LOCK_PREFIX();
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /* register operand */
                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint32_t *,      pu32Dst,            0);
                IEM_MC_ARG_CONST(uint32_t,  u32Src,/*=*/ u32Imm,1);
                IEM_MC_ARG(uint32_t *,      pEFlags,            2);
                IEM_MC_LOCAL(uint32_t,      u32Tmp);

                IEM_MC_FETCH_GREG_U32(u32Tmp, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_LOCAL(pu32Dst, u32Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u32, pu32Dst, u32Src, pEFlags);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u32Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /* memory operand */
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint32_t *,      pu32Dst,            0);
                IEM_MC_ARG_CONST(uint32_t,  u32Src,/*=*/ u32Imm,1);
                IEM_MC_ARG(uint32_t *,      pEFlags,            2);
                IEM_MC_LOCAL(uint32_t,      u32Tmp);
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_MEM_U32(u32Tmp, pIemCpu->iEffSeg, GCPtrEffDst);
                IEM_MC_REF_LOCAL(pu32Dst, u32Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u32, pu32Dst, u32Src, pEFlags);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u32Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;
        }

        case IEMMODE_64BIT:
        {
            uint64_t u64Imm; IEM_OPCODE_GET_NEXT_S32_SX_U64(&u64Imm);
            IEMOP_HLP_NO_LOCK_PREFIX();
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /* register operand */
                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint64_t *,      pu64Dst,            0);
                IEM_MC_ARG_CONST(uint64_t,  u64Src,/*=*/ u64Imm,1);
                IEM_MC_ARG(uint32_t *,      pEFlags,            2);
                IEM_MC_LOCAL(uint64_t,      u64Tmp);

                IEM_MC_FETCH_GREG_U64(u64Tmp, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_LOCAL(pu64Dst, u64Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u64, pu64Dst, u64Src, pEFlags);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u64Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /* memory operand */
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint64_t *,      pu64Dst,            0);
                IEM_MC_ARG_CONST(uint64_t,  u64Src,/*=*/ u64Imm,1);
                IEM_MC_ARG(uint32_t *,      pEFlags,            2);
                IEM_MC_LOCAL(uint64_t,      u64Tmp);
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_MEM_U64(u64Tmp, pIemCpu->iEffSeg, GCPtrEffDst);
                IEM_MC_REF_LOCAL(pu64Dst, u64Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u64, pu64Dst, u64Src, pEFlags);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u64Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;
        }
    }
    AssertFailedReturn(VERR_INTERNAL_ERROR_3);
}


/** Opcode 0x6a. */
FNIEMOP_DEF(iemOp_push_Ib)
{
    IEMOP_MNEMONIC("push Ib");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    IEM_MC_BEGIN(0,0);
    switch (pIemCpu->enmEffOpSize)
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
    uint8_t bRm;   IEM_OPCODE_GET_NEXT_U8(&bRm);
    uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
    IEMOP_HLP_NO_LOCK_PREFIX();
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF);

    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /* register operand */
                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                    0);
                IEM_MC_ARG_CONST(uint16_t,  u16Src,/*=*/ (int8_t)u8Imm, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                    2);
                IEM_MC_LOCAL(uint16_t,      u16Tmp);

                IEM_MC_FETCH_GREG_U16(u16Tmp, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_LOCAL(pu16Dst, u16Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u16, pu16Dst, u16Src, pEFlags);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u16Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /* memory operand */
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                    0);
                IEM_MC_ARG_CONST(uint16_t,  u16Src,/*=*/ (int8_t)u8Imm, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                    2);
                IEM_MC_LOCAL(uint16_t,      u16Tmp);
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_MEM_U16(u16Tmp, pIemCpu->iEffSeg, GCPtrEffDst);
                IEM_MC_REF_LOCAL(pu16Dst, u16Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u16, pu16Dst, u16Src, pEFlags);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u16Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /* register operand */
                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint32_t *,      pu32Dst,                    0);
                IEM_MC_ARG_CONST(uint32_t,  u32Src,/*=*/ (int8_t)u8Imm, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                    2);
                IEM_MC_LOCAL(uint32_t,      u32Tmp);

                IEM_MC_FETCH_GREG_U32(u32Tmp, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_LOCAL(pu32Dst, u32Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u32, pu32Dst, u32Src, pEFlags);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u32Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /* memory operand */
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint32_t *,      pu32Dst,                    0);
                IEM_MC_ARG_CONST(uint32_t,  u32Src,/*=*/ (int8_t)u8Imm, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                    2);
                IEM_MC_LOCAL(uint32_t,      u32Tmp);
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_MEM_U32(u32Tmp, pIemCpu->iEffSeg, GCPtrEffDst);
                IEM_MC_REF_LOCAL(pu32Dst, u32Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u32, pu32Dst, u32Src, pEFlags);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u32Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /* register operand */
                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint64_t *,      pu64Dst,                    0);
                IEM_MC_ARG_CONST(uint64_t,  u64Src,/*=*/ (int8_t)u8Imm, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                    2);
                IEM_MC_LOCAL(uint64_t,      u64Tmp);

                IEM_MC_FETCH_GREG_U64(u64Tmp, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_LOCAL(pu64Dst, u64Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u64, pu64Dst, u64Src, pEFlags);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u64Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /* memory operand */
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint64_t *,      pu64Dst,                    0);
                IEM_MC_ARG_CONST(uint64_t,  u64Src,/*=*/ (int8_t)u8Imm, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                    2);
                IEM_MC_LOCAL(uint64_t,      u64Tmp);
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_MEM_U64(u64Tmp, pIemCpu->iEffSeg, GCPtrEffDst);
                IEM_MC_REF_LOCAL(pu64Dst, u64Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u64, pu64Dst, u64Src, pEFlags);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u64Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;
    }
    AssertFailedReturn(VERR_INTERNAL_ERROR_3);
}


/** Opcode 0x6c. */
FNIEMOP_DEF(iemOp_insb_Yb_DX)
{
    IEMOP_HLP_NO_LOCK_PREFIX();
    if (pIemCpu->fPrefixes & (IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        IEMOP_MNEMONIC("rep ins Yb,DX");
        switch (pIemCpu->enmEffAddrMode)
        {
            case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_rep_ins_op8_addr16);
            case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_rep_ins_op8_addr32);
            case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_rep_ins_op8_addr64);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        IEMOP_MNEMONIC("ins Yb,DX");
        switch (pIemCpu->enmEffAddrMode)
        {
            case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_ins_op8_addr16);
            case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_ins_op8_addr32);
            case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_ins_op8_addr64);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/** Opcode 0x6d. */
FNIEMOP_DEF(iemOp_inswd_Yv_DX)
{
    IEMOP_HLP_NO_LOCK_PREFIX();
    if (pIemCpu->fPrefixes & (IEM_OP_PRF_REPZ | IEM_OP_PRF_REPNZ))
    {
        IEMOP_MNEMONIC("rep ins Yv,DX");
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_rep_ins_op16_addr16);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_rep_ins_op16_addr32);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_rep_ins_op16_addr64);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case IEMMODE_64BIT:
            case IEMMODE_32BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_rep_ins_op32_addr16);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_rep_ins_op32_addr32);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_rep_ins_op32_addr64);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        IEMOP_MNEMONIC("ins Yv,DX");
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_ins_op16_addr16);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_ins_op16_addr32);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_ins_op16_addr64);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case IEMMODE_64BIT:
            case IEMMODE_32BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_ins_op32_addr16);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_ins_op32_addr32);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_ins_op32_addr64);
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
    IEMOP_HLP_NO_LOCK_PREFIX();
    if (pIemCpu->fPrefixes & (IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        IEMOP_MNEMONIC("rep out DX,Yb");
        switch (pIemCpu->enmEffAddrMode)
        {
            case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_outs_op8_addr16, pIemCpu->iEffSeg);
            case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_outs_op8_addr32, pIemCpu->iEffSeg);
            case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_outs_op8_addr64, pIemCpu->iEffSeg);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        IEMOP_MNEMONIC("out DX,Yb");
        switch (pIemCpu->enmEffAddrMode)
        {
            case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_outs_op8_addr16, pIemCpu->iEffSeg);
            case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_outs_op8_addr32, pIemCpu->iEffSeg);
            case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_outs_op8_addr64, pIemCpu->iEffSeg);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/** Opcode 0x6f. */
FNIEMOP_DEF(iemOp_outswd_Yv_DX)
{
    IEMOP_HLP_NO_LOCK_PREFIX();
    if (pIemCpu->fPrefixes & (IEM_OP_PRF_REPZ | IEM_OP_PRF_REPNZ))
    {
        IEMOP_MNEMONIC("rep outs DX,Yv");
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_outs_op16_addr16, pIemCpu->iEffSeg);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_outs_op16_addr32, pIemCpu->iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_outs_op16_addr64, pIemCpu->iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case IEMMODE_64BIT:
            case IEMMODE_32BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_outs_op32_addr16, pIemCpu->iEffSeg);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_outs_op32_addr32, pIemCpu->iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_outs_op32_addr64, pIemCpu->iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        IEMOP_MNEMONIC("outs DX,Yv");
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_outs_op16_addr16, pIemCpu->iEffSeg);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_outs_op16_addr32, pIemCpu->iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_outs_op16_addr64, pIemCpu->iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case IEMMODE_64BIT:
            case IEMMODE_32BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_outs_op32_addr16, pIemCpu->iEffSeg);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_outs_op32_addr32, pIemCpu->iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_outs_op32_addr64, pIemCpu->iEffSeg);
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
    IEMOP_HLP_NO_LOCK_PREFIX();
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
    IEMOP_HLP_NO_LOCK_PREFIX();
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
    IEMOP_HLP_NO_LOCK_PREFIX();
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
    IEMOP_HLP_NO_LOCK_PREFIX();
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
    IEMOP_HLP_NO_LOCK_PREFIX();
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
    IEMOP_HLP_NO_LOCK_PREFIX();
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
    IEMOP_HLP_NO_LOCK_PREFIX();
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
    IEMOP_HLP_NO_LOCK_PREFIX();
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
    IEMOP_HLP_NO_LOCK_PREFIX();
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
    IEMOP_HLP_NO_LOCK_PREFIX();
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
    IEMOP_HLP_NO_LOCK_PREFIX();
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
    IEMOP_HLP_NO_LOCK_PREFIX();
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
    IEMOP_HLP_NO_LOCK_PREFIX();
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
    IEMOP_HLP_NO_LOCK_PREFIX();
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
    IEMOP_HLP_NO_LOCK_PREFIX();
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
    IEMOP_HLP_NO_LOCK_PREFIX();
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
        IEMOP_HLP_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(3, 0);
        IEM_MC_ARG(uint8_t *,       pu8Dst,                 0);
        IEM_MC_ARG_CONST(uint8_t,   u8Src, /*=*/ u8Imm,     1);
        IEM_MC_ARG(uint32_t *,      pEFlags,                2);

        IEM_MC_REF_GREG_U8(pu8Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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
        else
        {   /* CMP */
            IEMOP_HLP_NO_LOCK_PREFIX();
            fAccess = IEM_ACCESS_DATA_R;
        }
        IEM_MC_BEGIN(3, 2);
        IEM_MC_ARG(uint8_t *,       pu8Dst,                 0);
        IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags,        2);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
        IEM_MC_ARG_CONST(uint8_t,   u8Src, /*=*/ u8Imm,     1);

        IEM_MC_MEM_MAP(pu8Dst, fAccess, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
        IEM_MC_FETCH_EFLAGS(EFlags);
        if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
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

    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
        {
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /* register target */
                uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
                IEMOP_HLP_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                0);
                IEM_MC_ARG_CONST(uint16_t,  u16Src, /*=*/ u16Imm,   1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                2);

                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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
                else
                {   /* CMP, TEST */
                    IEMOP_HLP_NO_LOCK_PREFIX();
                    fAccess = IEM_ACCESS_DATA_R;
                }
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                0);
                IEM_MC_ARG(uint16_t,        u16Src,                 1);
                IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags,        2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
                IEM_MC_ASSIGN(u16Src, u16Imm);
                IEM_MC_MEM_MAP(pu16Dst, fAccess, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
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
                IEMOP_HLP_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint32_t *,      pu32Dst,                0);
                IEM_MC_ARG_CONST(uint32_t,  u32Src, /*=*/ u32Imm,   1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                2);

                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, u32Src, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /* memory target */
                uint32_t fAccess;
                if (pImpl->pfnLockedU32)
                    fAccess = IEM_ACCESS_DATA_RW;
                else
                {   /* CMP, TEST */
                    IEMOP_HLP_NO_LOCK_PREFIX();
                    fAccess = IEM_ACCESS_DATA_R;
                }
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint32_t *,      pu32Dst,                0);
                IEM_MC_ARG(uint32_t,        u32Src,                 1);
                IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags,        2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                uint32_t u32Imm; IEM_OPCODE_GET_NEXT_U32(&u32Imm);
                IEM_MC_ASSIGN(u32Src, u32Imm);
                IEM_MC_MEM_MAP(pu32Dst, fAccess, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
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
                IEMOP_HLP_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint64_t *,      pu64Dst,                0);
                IEM_MC_ARG_CONST(uint64_t,  u64Src, /*=*/ u64Imm,   1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                2);

                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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
                else
                {   /* CMP */
                    IEMOP_HLP_NO_LOCK_PREFIX();
                    fAccess = IEM_ACCESS_DATA_R;
                }
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint64_t *,      pu64Dst,                0);
                IEM_MC_ARG(uint64_t,        u64Src,                 1);
                IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags,        2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                uint64_t u64Imm; IEM_OPCODE_GET_NEXT_S32_SX_U64(&u64Imm);
                IEM_MC_ASSIGN(u64Src, u64Imm);
                IEM_MC_MEM_MAP(pu64Dst, fAccess, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
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
    PCIEMOPBINSIZES pImpl = g_apIemImplGrp1[(bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK];

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /*
         * Register target
         */
        IEMOP_HLP_NO_LOCK_PREFIX();
        uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
            {
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                    0);
                IEM_MC_ARG_CONST(uint16_t,  u16Src, /*=*/ (int8_t)u8Imm,1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                    2);

                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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

                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, u32Src, pEFlags);

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

                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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
        else
        {   /* CMP */
            IEMOP_HLP_NO_LOCK_PREFIX();
            fAccess = IEM_ACCESS_DATA_R;
        }

        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
            {
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                    0);
                IEM_MC_ARG(uint16_t,        u16Src,                     1);
                IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags,            2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
                IEM_MC_ASSIGN(u16Src, (int8_t)u8Imm);
                IEM_MC_MEM_MAP(pu16Dst, fAccess, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
                IEM_MC_ASSIGN(u32Src, (int8_t)u8Imm);
                IEM_MC_MEM_MAP(pu32Dst, fAccess, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
                IEM_MC_ASSIGN(u64Src, (int8_t)u8Imm);
                IEM_MC_MEM_MAP(pu64Dst, fAccess, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
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
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo do we have to decode the whole instruction first?  */
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rm_r8, &g_iemAImpl_test);
}


/** Opcode 0x85. */
FNIEMOP_DEF(iemOp_test_Ev_Gv)
{
    IEMOP_MNEMONIC("test Ev,Gv");
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo do we have to decode the whole instruction first?  */
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
        IEMOP_HLP_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint8_t, uTmp1);
        IEM_MC_LOCAL(uint8_t, uTmp2);

        IEM_MC_FETCH_GREG_U8(uTmp1, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
        IEM_MC_FETCH_GREG_U8(uTmp2, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
        IEM_MC_STORE_GREG_U8((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB,                              uTmp1);
        IEM_MC_STORE_GREG_U8(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, uTmp2);

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

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_MEM_MAP(pu8Mem, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
        IEM_MC_REF_GREG_U8(pu8Reg, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
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
        IEMOP_HLP_NO_LOCK_PREFIX();

        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint16_t, uTmp1);
                IEM_MC_LOCAL(uint16_t, uTmp2);

                IEM_MC_FETCH_GREG_U16(uTmp1, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_FETCH_GREG_U16(uTmp2, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_STORE_GREG_U16((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB,                              uTmp1);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, uTmp2);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint32_t, uTmp1);
                IEM_MC_LOCAL(uint32_t, uTmp2);

                IEM_MC_FETCH_GREG_U32(uTmp1, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_FETCH_GREG_U32(uTmp2, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_STORE_GREG_U32((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB,                              uTmp1);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, uTmp2);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint64_t, uTmp1);
                IEM_MC_LOCAL(uint64_t, uTmp2);

                IEM_MC_FETCH_GREG_U64(uTmp1, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_FETCH_GREG_U64(uTmp2, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_STORE_GREG_U64((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB,                              uTmp1);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, uTmp2);

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
        switch (pIemCpu->enmEffOpSize)
        {
/** @todo the register must be committed separately! */
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(2, 2);
                IEM_MC_ARG(uint16_t *,  pu16Mem, 0);
                IEM_MC_ARG(uint16_t *,  pu16Reg, 1);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_MEM_MAP(pu16Mem, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_REF_GREG_U16(pu16Reg, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_MEM_MAP(pu32Mem, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_REF_GREG_U32(pu32Reg, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_CALL_VOID_AIMPL_2(iemAImpl_xchg_u32, pu32Mem, pu32Reg);
                IEM_MC_MEM_COMMIT_AND_UNMAP(pu32Mem, IEM_ACCESS_DATA_RW);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(2, 2);
                IEM_MC_ARG(uint64_t *,  pu64Mem, 0);
                IEM_MC_ARG(uint64_t *,  pu64Reg, 1);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_MEM_MAP(pu64Mem, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_REF_GREG_U64(pu64Reg, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
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
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(uint8_t, u8Value);
        IEM_MC_FETCH_GREG_U8(u8Value, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
        IEM_MC_STORE_GREG_U8((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, u8Value);
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
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_FETCH_GREG_U8(u8Value, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
        IEM_MC_STORE_MEM_U8(pIemCpu->iEffSeg, GCPtrEffDst, u8Value);
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
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint16_t, u16Value);
                IEM_MC_FETCH_GREG_U16(u16Value, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_STORE_GREG_U16((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, u16Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint32_t, u32Value);
                IEM_MC_FETCH_GREG_U32(u32Value, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_STORE_GREG_U32((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, u32Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint64_t, u64Value);
                IEM_MC_FETCH_GREG_U64(u64Value, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_STORE_GREG_U64((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, u64Value);
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
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint16_t, u16Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_GREG_U16(u16Value, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_STORE_MEM_U16(pIemCpu->iEffSeg, GCPtrEffDst, u16Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint32_t, u32Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_GREG_U32(u32Value, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_STORE_MEM_U32(pIemCpu->iEffSeg, GCPtrEffDst, u32Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint64_t, u64Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_GREG_U64(u64Value, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg);
                IEM_MC_STORE_MEM_U64(pIemCpu->iEffSeg, GCPtrEffDst, u64Value);
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
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(uint8_t, u8Value);
        IEM_MC_FETCH_GREG_U8(u8Value, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
        IEM_MC_STORE_GREG_U8(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u8Value);
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
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_FETCH_MEM_U8(u8Value, pIemCpu->iEffSeg, GCPtrEffDst);
        IEM_MC_STORE_GREG_U8(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u8Value);
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
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint16_t, u16Value);
                IEM_MC_FETCH_GREG_U16(u16Value, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u16Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint32_t, u32Value);
                IEM_MC_FETCH_GREG_U32(u32Value, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u32Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint64_t, u64Value);
                IEM_MC_FETCH_GREG_U64(u64Value, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u64Value);
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
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint16_t, u16Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_MEM_U16(u16Value, pIemCpu->iEffSeg, GCPtrEffDst);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u16Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint32_t, u32Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_MEM_U32(u32Value, pIemCpu->iEffSeg, GCPtrEffDst);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u32Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint64_t, u64Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_MEM_U64(u64Value, pIemCpu->iEffSeg, GCPtrEffDst);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u64Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;
        }
    }
    return VINF_SUCCESS;
}


/** Opcode 0x8c. */
FNIEMOP_DEF(iemOp_mov_Ev_Sw)
{
    IEMOP_MNEMONIC("mov Ev,Sw");

    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */

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
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint16_t, u16Value);
                IEM_MC_FETCH_SREG_U16(u16Value, iSegReg);
                IEM_MC_STORE_GREG_U16((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, u16Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint32_t, u32Value);
                IEM_MC_FETCH_SREG_ZX_U32(u32Value, iSegReg);
                IEM_MC_STORE_GREG_U32((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, u32Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint64_t, u64Value);
                IEM_MC_FETCH_SREG_ZX_U64(u64Value, iSegReg);
                IEM_MC_STORE_GREG_U64((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, u64Value);
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
        pIemCpu->enmEffOpSize = pIemCpu->enmDefOpSize = IEMMODE_16BIT;
#endif
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint16_t,  u16Value);
        IEM_MC_LOCAL(RTGCPTR,   GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_FETCH_SREG_U16(u16Value, iSegReg);
        IEM_MC_STORE_MEM_U16(pIemCpu->iEffSeg, GCPtrEffDst, u16Value);
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
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        return IEMOP_RAISE_INVALID_LOCK_PREFIX(); /* no register form */

    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(RTGCPTR,  GCPtrEffSrc);
            IEM_MC_LOCAL(uint16_t, u16Cast);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm);
            IEM_MC_ASSIGN_TO_SMALLER(u16Cast, GCPtrEffSrc);
            IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u16Cast);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
            IEM_MC_LOCAL(uint32_t, u32Cast);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm);
            IEM_MC_ASSIGN_TO_SMALLER(u32Cast, GCPtrEffSrc);
            IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, u32Cast);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm);
            IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pIemCpu->uRexReg, GCPtrEffSrc);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;
    }
    AssertFailedReturn(VERR_INTERNAL_ERROR_5);
}


/** Opcode 0x8e. */
FNIEMOP_DEF(iemOp_mov_Sw_Ev)
{
    IEMOP_MNEMONIC("mov Sw,Ev");

    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */

    /*
     * The practical operand size is 16-bit.
     */
#if 0 /* not necessary */
    pIemCpu->enmEffOpSize = pIemCpu->enmDefOpSize = IEMMODE_16BIT;
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
        IEM_MC_BEGIN(2, 0);
        IEM_MC_ARG_CONST(uint8_t, iSRegArg, iSegReg, 0);
        IEM_MC_ARG(uint16_t,      u16Value,          1);
        IEM_MC_FETCH_GREG_U16(u16Value, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_FETCH_MEM_U16(u16Value, pIemCpu->iEffSeg, GCPtrEffDst);
        IEM_MC_CALL_CIMPL_2(iemCImpl_load_SReg, iSRegArg, u16Value);
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x8f. */
FNIEMOP_DEF(iemOp_pop_Ev)
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
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */

    /* Register access is relatively easy and can share code. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        return FNIEMOP_CALL_1(iemOpCommonPopGReg, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);

    /*
     * Memory target.
     *
     * Intel says that RSP is incremented before it's used in any effective
     * address calcuations.  This means some serious extra annoyance here since
     * we decode and caclulate the effective address in one step and like to
     * delay committing registers till everything is done.
     *
     * So, we'll decode and calculate the effective address twice.  This will
     * require some recoding if turned into a recompiler.
     */
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE(); /* The common code does this differently. */

#ifndef TST_IEM_CHECK_MC
    /* Calc effective address with modified ESP. */
    uint8_t const   offOpcodeSaved = pIemCpu->offOpcode;
    RTGCPTR         GCPtrEff;
    VBOXSTRICTRC    rcStrict;
    rcStrict = iemOpHlpCalcRmEffAddr(pIemCpu, bRm, &GCPtrEff);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;
    pIemCpu->offOpcode = offOpcodeSaved;

    PCPUMCTX        pCtx     = pIemCpu->CTX_SUFF(pCtx);
    uint64_t const  RspSaved = pCtx->rsp;
    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT: iemRegAddToRsp(pCtx, 2); break;
        case IEMMODE_32BIT: iemRegAddToRsp(pCtx, 4); break;
        case IEMMODE_64BIT: iemRegAddToRsp(pCtx, 8); break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
    rcStrict = iemOpHlpCalcRmEffAddr(pIemCpu, bRm, &GCPtrEff);
    Assert(rcStrict == VINF_SUCCESS);
    pCtx->rsp = RspSaved;

    /* Perform the operation - this should be CImpl. */
    RTUINT64U TmpRsp;
    TmpRsp.u = pCtx->rsp;
    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
        {
            uint16_t u16Value;
            rcStrict = iemMemStackPopU16Ex(pIemCpu, &u16Value, &TmpRsp);
            if (rcStrict == VINF_SUCCESS)
                rcStrict = iemMemStoreDataU16(pIemCpu, pIemCpu->iEffSeg, GCPtrEff, u16Value);
            break;
        }

        case IEMMODE_32BIT:
        {
            uint32_t u32Value;
            rcStrict = iemMemStackPopU32Ex(pIemCpu, &u32Value, &TmpRsp);
            if (rcStrict == VINF_SUCCESS)
                rcStrict = iemMemStoreDataU32(pIemCpu, pIemCpu->iEffSeg, GCPtrEff, u32Value);
            break;
        }

        case IEMMODE_64BIT:
        {
            uint64_t u64Value;
            rcStrict = iemMemStackPopU64Ex(pIemCpu, &u64Value, &TmpRsp);
            if (rcStrict == VINF_SUCCESS)
                rcStrict = iemMemStoreDataU16(pIemCpu, pIemCpu->iEffSeg, GCPtrEff, u64Value);
            break;
        }

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
    if (rcStrict == VINF_SUCCESS)
    {
        pCtx->rsp = TmpRsp.u;
        iemRegUpdateRip(pIemCpu);
    }
    return rcStrict;

#else
    return VERR_NOT_IMPLEMENTED;
#endif
}


/**
 * Common 'xchg reg,rAX' helper.
 */
FNIEMOP_DEF_1(iemOpCommonXchgGRegRax, uint8_t, iReg)
{
    IEMOP_HLP_NO_LOCK_PREFIX();

    iReg |= pIemCpu->uRexB;
    switch (pIemCpu->enmEffOpSize)
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
    if (pIemCpu->fPrefixes & IEM_OP_PRF_REX_B)
    {
        IEMOP_MNEMONIC("xchg r8,rAX");
        return FNIEMOP_CALL_1(iemOpCommonXchgGRegRax, X86_GREG_xAX);
    }

    if (pIemCpu->fPrefixes & IEM_OP_PRF_LOCK)
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
    IEMOP_HLP_NO_LOCK_PREFIX();
    switch (pIemCpu->enmEffOpSize)
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
    IEMOP_HLP_NO_LOCK_PREFIX();
    switch (pIemCpu->enmEffOpSize)
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
    if (pIemCpu->enmEffOpSize != IEMMODE_16BIT)
        IEM_OPCODE_GET_NEXT_U32(&offSeg);
    else
    {
        uint16_t offSeg16; IEM_OPCODE_GET_NEXT_U16(&offSeg16);
        offSeg = offSeg16;
    }
    uint16_t uSel;  IEM_OPCODE_GET_NEXT_U16(&uSel);
    IEMOP_HLP_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_3(iemCImpl_callf, uSel, offSeg, pIemCpu->enmEffOpSize);
}


/** Opcode 0x9b. (aka fwait) */
FNIEMOP_DEF(iemOp_wait)
{
    IEMOP_MNEMONIC("wait");
    IEMOP_HLP_NO_LOCK_PREFIX();

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
    IEMOP_HLP_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_pushf, pIemCpu->enmEffOpSize);
}


/** Opcode 0x9d. */
FNIEMOP_DEF(iemOp_popf_Fv)
{
    IEMOP_HLP_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_popf, pIemCpu->enmEffOpSize);
}


/** Opcode 0x9e. */
FNIEMOP_DEF(iemOp_sahf)
{
    IEMOP_MNEMONIC("sahf");
    IEMOP_HLP_NO_LOCK_PREFIX();
    IEMOP_HLP_NO_64BIT();
    IEM_MC_BEGIN(0, 2);
    IEM_MC_LOCAL(uint32_t, u32Flags);
    IEM_MC_LOCAL(uint32_t, EFlags);
    IEM_MC_FETCH_EFLAGS(EFlags);
    IEM_MC_FETCH_GREG_U8_ZX_U32(u32Flags, X86_GREG_xSP/*=AH*/);
    IEM_MC_AND_LOCAL_U32(u32Flags, UINT32_C(0xd7));
    IEM_MC_AND_LOCAL_U32(EFlags, UINT32_C(0xffffff00));
    IEM_MC_OR_LOCAL_U32(u32Flags, UINT32_C(0x00000002));
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
    IEMOP_HLP_NO_LOCK_PREFIX();
    IEMOP_HLP_NO_64BIT();
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
        switch (pIemCpu->enmEffAddrMode) \
        { \
            case IEMMODE_16BIT: \
            { \
                uint16_t u16Off; IEM_OPCODE_GET_NEXT_U16(&u16Off); \
                (a_GCPtrMemOff) = u16Off; \
                break; \
            } \
            case IEMMODE_32BIT: \
            { \
                uint32_t u32Off; IEM_OPCODE_GET_NEXT_U32(&u32Off); \
                (a_GCPtrMemOff) = u32Off; \
                break; \
            } \
            case IEMMODE_64BIT: \
                IEM_OPCODE_GET_NEXT_U64(&(a_GCPtrMemOff)); \
                break; \
            IEM_NOT_REACHED_DEFAULT_CASE_RET(); \
        } \
        IEMOP_HLP_NO_LOCK_PREFIX(); \
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
    IEM_MC_FETCH_MEM_U8(u8Tmp, pIemCpu->iEffSeg, GCPtrMemOff);
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
    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(0,1);
            IEM_MC_LOCAL(uint16_t, u16Tmp);
            IEM_MC_FETCH_MEM_U16(u16Tmp, pIemCpu->iEffSeg, GCPtrMemOff);
            IEM_MC_STORE_GREG_U16(X86_GREG_xAX, u16Tmp);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(0,1);
            IEM_MC_LOCAL(uint32_t, u32Tmp);
            IEM_MC_FETCH_MEM_U32(u32Tmp, pIemCpu->iEffSeg, GCPtrMemOff);
            IEM_MC_STORE_GREG_U32(X86_GREG_xAX, u32Tmp);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(0,1);
            IEM_MC_LOCAL(uint64_t, u64Tmp);
            IEM_MC_FETCH_MEM_U64(u64Tmp, pIemCpu->iEffSeg, GCPtrMemOff);
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
    IEM_MC_STORE_MEM_U8(pIemCpu->iEffSeg, GCPtrMemOff, u8Tmp);
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
    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(0,1);
            IEM_MC_LOCAL(uint16_t, u16Tmp);
            IEM_MC_FETCH_GREG_U16(u16Tmp, X86_GREG_xAX);
            IEM_MC_STORE_MEM_U16(pIemCpu->iEffSeg, GCPtrMemOff, u16Tmp);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(0,1);
            IEM_MC_LOCAL(uint32_t, u32Tmp);
            IEM_MC_FETCH_GREG_U32(u32Tmp, X86_GREG_xAX);
            IEM_MC_STORE_MEM_U32(pIemCpu->iEffSeg, GCPtrMemOff, u32Tmp);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(0,1);
            IEM_MC_LOCAL(uint64_t, u64Tmp);
            IEM_MC_FETCH_GREG_U64(u64Tmp, X86_GREG_xAX);
            IEM_MC_STORE_MEM_U64(pIemCpu->iEffSeg, GCPtrMemOff, u64Tmp);
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
        IEM_MC_FETCH_MEM_U##ValBits(uValue, pIemCpu->iEffSeg, uAddr); \
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
    IEMOP_HLP_NO_LOCK_PREFIX();

    /*
     * Use the C implementation if a repeat prefix is encountered.
     */
    if (pIemCpu->fPrefixes & (IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        IEMOP_MNEMONIC("rep movsb Xb,Yb");
        switch (pIemCpu->enmEffAddrMode)
        {
            case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_movs_op8_addr16, pIemCpu->iEffSeg);
            case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_movs_op8_addr32, pIemCpu->iEffSeg);
            case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_movs_op8_addr64, pIemCpu->iEffSeg);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    IEMOP_MNEMONIC("movsb Xb,Yb");

    /*
     * Sharing case implementation with movs[wdq] below.
     */
    switch (pIemCpu->enmEffAddrMode)
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
    IEMOP_HLP_NO_LOCK_PREFIX();

    /*
     * Use the C implementation if a repeat prefix is encountered.
     */
    if (pIemCpu->fPrefixes & (IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        IEMOP_MNEMONIC("rep movs Xv,Yv");
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_movs_op16_addr16, pIemCpu->iEffSeg);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_movs_op16_addr32, pIemCpu->iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_movs_op16_addr64, pIemCpu->iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case IEMMODE_32BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_movs_op32_addr16, pIemCpu->iEffSeg);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_movs_op32_addr32, pIemCpu->iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_movs_op32_addr64, pIemCpu->iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            case IEMMODE_64BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: AssertFailedReturn(VERR_INTERNAL_ERROR_3);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_movs_op64_addr32, pIemCpu->iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_movs_op64_addr64, pIemCpu->iEffSeg);
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
    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
            switch (pIemCpu->enmEffAddrMode)
            {
                case IEMMODE_16BIT: IEM_MOVS_CASE(16, 16); break;
                case IEMMODE_32BIT: IEM_MOVS_CASE(16, 32); break;
                case IEMMODE_64BIT: IEM_MOVS_CASE(16, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;

        case IEMMODE_32BIT:
            switch (pIemCpu->enmEffAddrMode)
            {
                case IEMMODE_16BIT: IEM_MOVS_CASE(32, 16); break;
                case IEMMODE_32BIT: IEM_MOVS_CASE(32, 32); break;
                case IEMMODE_64BIT: IEM_MOVS_CASE(32, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;

        case IEMMODE_64BIT:
            switch (pIemCpu->enmEffAddrMode)
            {
                case IEMMODE_16BIT: AssertFailedReturn(VERR_INTERNAL_ERROR_4); /* cannot be encoded */ break;
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
        IEM_MC_FETCH_MEM_U##ValBits(uValue1, pIemCpu->iEffSeg, uAddr); \
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
    IEMOP_HLP_NO_LOCK_PREFIX();

    /*
     * Use the C implementation if a repeat prefix is encountered.
     */
    if (pIemCpu->fPrefixes & IEM_OP_PRF_REPZ)
    {
        IEMOP_MNEMONIC("repe cmps Xb,Yb");
        switch (pIemCpu->enmEffAddrMode)
        {
            case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repe_cmps_op8_addr16, pIemCpu->iEffSeg);
            case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repe_cmps_op8_addr32, pIemCpu->iEffSeg);
            case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repe_cmps_op8_addr64, pIemCpu->iEffSeg);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    if (pIemCpu->fPrefixes & IEM_OP_PRF_REPNZ)
    {
        IEMOP_MNEMONIC("repe cmps Xb,Yb");
        switch (pIemCpu->enmEffAddrMode)
        {
            case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repne_cmps_op8_addr16, pIemCpu->iEffSeg);
            case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repne_cmps_op8_addr32, pIemCpu->iEffSeg);
            case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repne_cmps_op8_addr64, pIemCpu->iEffSeg);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    IEMOP_MNEMONIC("cmps Xb,Yb");

    /*
     * Sharing case implementation with cmps[wdq] below.
     */
    switch (pIemCpu->enmEffAddrMode)
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
    IEMOP_HLP_NO_LOCK_PREFIX();

    /*
     * Use the C implementation if a repeat prefix is encountered.
     */
    if (pIemCpu->fPrefixes & IEM_OP_PRF_REPZ)
    {
        IEMOP_MNEMONIC("repe cmps Xv,Yv");
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repe_cmps_op16_addr16, pIemCpu->iEffSeg);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repe_cmps_op16_addr32, pIemCpu->iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repe_cmps_op16_addr64, pIemCpu->iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case IEMMODE_32BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repe_cmps_op32_addr16, pIemCpu->iEffSeg);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repe_cmps_op32_addr32, pIemCpu->iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repe_cmps_op32_addr64, pIemCpu->iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            case IEMMODE_64BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: AssertFailedReturn(VERR_INTERNAL_ERROR_3);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repe_cmps_op64_addr32, pIemCpu->iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repe_cmps_op64_addr64, pIemCpu->iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }

    if (pIemCpu->fPrefixes & IEM_OP_PRF_REPNZ)
    {
        IEMOP_MNEMONIC("repne cmps Xv,Yv");
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repne_cmps_op16_addr16, pIemCpu->iEffSeg);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repne_cmps_op16_addr32, pIemCpu->iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repne_cmps_op16_addr64, pIemCpu->iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case IEMMODE_32BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repne_cmps_op32_addr16, pIemCpu->iEffSeg);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repne_cmps_op32_addr32, pIemCpu->iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repne_cmps_op32_addr64, pIemCpu->iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            case IEMMODE_64BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: AssertFailedReturn(VERR_INTERNAL_ERROR_3);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repne_cmps_op64_addr32, pIemCpu->iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repne_cmps_op64_addr64, pIemCpu->iEffSeg);
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
    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
            switch (pIemCpu->enmEffAddrMode)
            {
                case IEMMODE_16BIT: IEM_CMPS_CASE(16, 16); break;
                case IEMMODE_32BIT: IEM_CMPS_CASE(16, 32); break;
                case IEMMODE_64BIT: IEM_CMPS_CASE(16, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;

        case IEMMODE_32BIT:
            switch (pIemCpu->enmEffAddrMode)
            {
                case IEMMODE_16BIT: IEM_CMPS_CASE(32, 16); break;
                case IEMMODE_32BIT: IEM_CMPS_CASE(32, 32); break;
                case IEMMODE_64BIT: IEM_CMPS_CASE(32, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;

        case IEMMODE_64BIT:
            switch (pIemCpu->enmEffAddrMode)
            {
                case IEMMODE_16BIT: AssertFailedReturn(VERR_INTERNAL_ERROR_4); /* cannot be encoded */ break;
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
    IEMOP_HLP_NO_LOCK_PREFIX();

    /*
     * Use the C implementation if a repeat prefix is encountered.
     */
    if (pIemCpu->fPrefixes & (IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        IEMOP_MNEMONIC("rep stos Yb,al");
        switch (pIemCpu->enmEffAddrMode)
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
    switch (pIemCpu->enmEffAddrMode)
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
    IEMOP_HLP_NO_LOCK_PREFIX();

    /*
     * Use the C implementation if a repeat prefix is encountered.
     */
    if (pIemCpu->fPrefixes & (IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        IEMOP_MNEMONIC("rep stos Yv,rAX");
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_stos_ax_m16);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_stos_ax_m32);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_stos_ax_m64);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case IEMMODE_32BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_stos_eax_m16);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_stos_eax_m32);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_stos_eax_m64);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            case IEMMODE_64BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: AssertFailedReturn(VERR_INTERNAL_ERROR_3);
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
    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
            switch (pIemCpu->enmEffAddrMode)
            {
                case IEMMODE_16BIT: IEM_STOS_CASE(16, 16); break;
                case IEMMODE_32BIT: IEM_STOS_CASE(16, 32); break;
                case IEMMODE_64BIT: IEM_STOS_CASE(16, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;

        case IEMMODE_32BIT:
            switch (pIemCpu->enmEffAddrMode)
            {
                case IEMMODE_16BIT: IEM_STOS_CASE(32, 16); break;
                case IEMMODE_32BIT: IEM_STOS_CASE(32, 32); break;
                case IEMMODE_64BIT: IEM_STOS_CASE(32, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;

        case IEMMODE_64BIT:
            switch (pIemCpu->enmEffAddrMode)
            {
                case IEMMODE_16BIT: AssertFailedReturn(VERR_INTERNAL_ERROR_4); /* cannot be encoded */ break;
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
        IEM_MC_FETCH_MEM_U##ValBits(uValue, pIemCpu->iEffSeg, uAddr); \
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
    IEMOP_HLP_NO_LOCK_PREFIX();

    /*
     * Use the C implementation if a repeat prefix is encountered.
     */
    if (pIemCpu->fPrefixes & (IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        IEMOP_MNEMONIC("rep lodsb al,Xb");
        switch (pIemCpu->enmEffAddrMode)
        {
            case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_lods_al_m16, pIemCpu->iEffSeg);
            case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_lods_al_m32, pIemCpu->iEffSeg);
            case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_lods_al_m64, pIemCpu->iEffSeg);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    IEMOP_MNEMONIC("lodsb al,Xb");

    /*
     * Sharing case implementation with stos[wdq] below.
     */
    switch (pIemCpu->enmEffAddrMode)
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
    IEMOP_HLP_NO_LOCK_PREFIX();

    /*
     * Use the C implementation if a repeat prefix is encountered.
     */
    if (pIemCpu->fPrefixes & (IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        IEMOP_MNEMONIC("rep lods rAX,Xv");
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_lods_ax_m16, pIemCpu->iEffSeg);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_lods_ax_m32, pIemCpu->iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_lods_ax_m64, pIemCpu->iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case IEMMODE_32BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_lods_eax_m16, pIemCpu->iEffSeg);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_lods_eax_m32, pIemCpu->iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_lods_eax_m64, pIemCpu->iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            case IEMMODE_64BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: AssertFailedReturn(VERR_INTERNAL_ERROR_3);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_lods_rax_m32, pIemCpu->iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_lods_rax_m64, pIemCpu->iEffSeg);
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
    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
            switch (pIemCpu->enmEffAddrMode)
            {
                case IEMMODE_16BIT: IEM_LODS_CASE(16, 16); break;
                case IEMMODE_32BIT: IEM_LODS_CASE(16, 32); break;
                case IEMMODE_64BIT: IEM_LODS_CASE(16, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;

        case IEMMODE_32BIT:
            switch (pIemCpu->enmEffAddrMode)
            {
                case IEMMODE_16BIT: IEM_LODS_CASE(32, 16); break;
                case IEMMODE_32BIT: IEM_LODS_CASE(32, 32); break;
                case IEMMODE_64BIT: IEM_LODS_CASE(32, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;

        case IEMMODE_64BIT:
            switch (pIemCpu->enmEffAddrMode)
            {
                case IEMMODE_16BIT: AssertFailedReturn(VERR_INTERNAL_ERROR_4); /* cannot be encoded */ break;
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
    IEMOP_HLP_NO_LOCK_PREFIX();

    /*
     * Use the C implementation if a repeat prefix is encountered.
     */
    if (pIemCpu->fPrefixes & IEM_OP_PRF_REPZ)
    {
        IEMOP_MNEMONIC("repe scasb al,Xb");
        switch (pIemCpu->enmEffAddrMode)
        {
            case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_al_m16);
            case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_al_m32);
            case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_al_m64);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    if (pIemCpu->fPrefixes & IEM_OP_PRF_REPNZ)
    {
        IEMOP_MNEMONIC("repne scasb al,Xb");
        switch (pIemCpu->enmEffAddrMode)
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
    switch (pIemCpu->enmEffAddrMode)
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
    IEMOP_HLP_NO_LOCK_PREFIX();

    /*
     * Use the C implementation if a repeat prefix is encountered.
     */
    if (pIemCpu->fPrefixes & IEM_OP_PRF_REPZ)
    {
        IEMOP_MNEMONIC("repe scas rAX,Xv");
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_ax_m16);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_ax_m32);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_ax_m64);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case IEMMODE_32BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_eax_m16);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_eax_m32);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_eax_m64);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            case IEMMODE_64BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: AssertFailedReturn(VERR_INTERNAL_ERROR_3); /** @todo It's this wrong, we can do 16-bit addressing in 64-bit mode, but not 32-bit. right? */
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_rax_m32);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_rax_m64);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    if (pIemCpu->fPrefixes & IEM_OP_PRF_REPNZ)
    {
        IEMOP_MNEMONIC("repne scas rAX,Xv");
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_ax_m16);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_ax_m32);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_ax_m64);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case IEMMODE_32BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_eax_m16);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_eax_m32);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_eax_m64);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            case IEMMODE_64BIT:
                switch (pIemCpu->enmEffAddrMode)
                {
                    case IEMMODE_16BIT: AssertFailedReturn(VERR_INTERNAL_ERROR_3);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_rax_m32);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_rax_m64);
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
    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
            switch (pIemCpu->enmEffAddrMode)
            {
                case IEMMODE_16BIT: IEM_SCAS_CASE(16, 16); break;
                case IEMMODE_32BIT: IEM_SCAS_CASE(16, 32); break;
                case IEMMODE_64BIT: IEM_SCAS_CASE(16, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;

        case IEMMODE_32BIT:
            switch (pIemCpu->enmEffAddrMode)
            {
                case IEMMODE_16BIT: IEM_SCAS_CASE(32, 16); break;
                case IEMMODE_32BIT: IEM_SCAS_CASE(32, 32); break;
                case IEMMODE_64BIT: IEM_SCAS_CASE(32, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;

        case IEMMODE_64BIT:
            switch (pIemCpu->enmEffAddrMode)
            {
                case IEMMODE_16BIT: AssertFailedReturn(VERR_INTERNAL_ERROR_4); /* cannot be encoded */ break;
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
    IEMOP_HLP_NO_LOCK_PREFIX();

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
    return FNIEMOP_CALL_1(iemOpCommonMov_r8_Ib, X86_GREG_xAX);
}


/** Opcode 0xb1. */
FNIEMOP_DEF(iemOp_CL_Ib)
{
    IEMOP_MNEMONIC("mov CL,Ib");
    return FNIEMOP_CALL_1(iemOpCommonMov_r8_Ib, X86_GREG_xCX);
}


/** Opcode 0xb2. */
FNIEMOP_DEF(iemOp_DL_Ib)
{
    IEMOP_MNEMONIC("mov DL,Ib");
    return FNIEMOP_CALL_1(iemOpCommonMov_r8_Ib, X86_GREG_xDX);
}


/** Opcode 0xb3. */
FNIEMOP_DEF(iemOp_BL_Ib)
{
    IEMOP_MNEMONIC("mov BL,Ib");
    return FNIEMOP_CALL_1(iemOpCommonMov_r8_Ib, X86_GREG_xBX);
}


/** Opcode 0xb4. */
FNIEMOP_DEF(iemOp_mov_AH_Ib)
{
    IEMOP_MNEMONIC("mov AH,Ib");
    return FNIEMOP_CALL_1(iemOpCommonMov_r8_Ib, X86_GREG_xSP);
}


/** Opcode 0xb5. */
FNIEMOP_DEF(iemOp_CH_Ib)
{
    IEMOP_MNEMONIC("mov CH,Ib");
    return FNIEMOP_CALL_1(iemOpCommonMov_r8_Ib, X86_GREG_xBP);
}


/** Opcode 0xb6. */
FNIEMOP_DEF(iemOp_DH_Ib)
{
    IEMOP_MNEMONIC("mov DH,Ib");
    return FNIEMOP_CALL_1(iemOpCommonMov_r8_Ib, X86_GREG_xSI);
}


/** Opcode 0xb7. */
FNIEMOP_DEF(iemOp_BH_Ib)
{
    IEMOP_MNEMONIC("mov BH,Ib");
    return FNIEMOP_CALL_1(iemOpCommonMov_r8_Ib, X86_GREG_xDI);
}


/**
 * Common 'mov regX,immX' helper.
 */
FNIEMOP_DEF_1(iemOpCommonMov_Rv_Iv, uint8_t, iReg)
{
    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
        {
            uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
            IEMOP_HLP_NO_LOCK_PREFIX();

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
            IEMOP_HLP_NO_LOCK_PREFIX();

            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL_CONST(uint32_t, u32Value,/*=*/ u32Imm);
            IEM_MC_STORE_GREG_U32(iReg, u32Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            break;
        }
        case IEMMODE_64BIT:
        {
            uint64_t u64Imm; IEM_OPCODE_GET_NEXT_U64(&u64Imm);
            IEMOP_HLP_NO_LOCK_PREFIX();

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
    return FNIEMOP_CALL_1(iemOpCommonMov_Rv_Iv, X86_GREG_xAX);
}


/** Opcode 0xb9. */
FNIEMOP_DEF(iemOp_eCX_Iv)
{
    IEMOP_MNEMONIC("mov rCX,IV");
    return FNIEMOP_CALL_1(iemOpCommonMov_Rv_Iv, X86_GREG_xCX);
}


/** Opcode 0xba. */
FNIEMOP_DEF(iemOp_eDX_Iv)
{
    IEMOP_MNEMONIC("mov rDX,IV");
    return FNIEMOP_CALL_1(iemOpCommonMov_Rv_Iv, X86_GREG_xDX);
}


/** Opcode 0xbb. */
FNIEMOP_DEF(iemOp_eBX_Iv)
{
    IEMOP_MNEMONIC("mov rBX,IV");
    return FNIEMOP_CALL_1(iemOpCommonMov_Rv_Iv, X86_GREG_xBX);
}


/** Opcode 0xbc. */
FNIEMOP_DEF(iemOp_eSP_Iv)
{
    IEMOP_MNEMONIC("mov rSP,IV");
    return FNIEMOP_CALL_1(iemOpCommonMov_Rv_Iv, X86_GREG_xSP);
}


/** Opcode 0xbd. */
FNIEMOP_DEF(iemOp_eBP_Iv)
{
    IEMOP_MNEMONIC("mov rBP,IV");
    return FNIEMOP_CALL_1(iemOpCommonMov_Rv_Iv, X86_GREG_xBP);
}


/** Opcode 0xbe. */
FNIEMOP_DEF(iemOp_eSI_Iv)
{
    IEMOP_MNEMONIC("mov rSI,IV");
    return FNIEMOP_CALL_1(iemOpCommonMov_Rv_Iv, X86_GREG_xSI);
}


/** Opcode 0xbf. */
FNIEMOP_DEF(iemOp_eDI_Iv)
{
    IEMOP_MNEMONIC("mov rDI,IV");
    return FNIEMOP_CALL_1(iemOpCommonMov_Rv_Iv, X86_GREG_xDI);
}


/** Opcode 0xc0. */
FNIEMOP_DEF(iemOp_Grp2_Eb_Ib)
{
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
        case 6: return IEMOP_RAISE_INVALID_LOCK_PREFIX();
        IEM_NOT_REACHED_DEFAULT_CASE_RET(); /* gcc maybe stupid */
    }
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF | X86_EFL_AF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register */
        uint8_t cShift; IEM_OPCODE_GET_NEXT_U8(&cShift);
        IEMOP_HLP_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(3, 0);
        IEM_MC_ARG(uint8_t *,       pu8Dst,            0);
        IEM_MC_ARG_CONST(uint8_t,   cShiftArg, cShift, 1);
        IEM_MC_ARG(uint32_t *,      pEFlags,           2);
        IEM_MC_REF_GREG_U8(pu8Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
        IEM_MC_REF_EFLAGS(pEFlags);
        IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU8, pu8Dst, cShiftArg, pEFlags);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory */
        IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */
        IEM_MC_BEGIN(3, 2);
        IEM_MC_ARG(uint8_t *,   pu8Dst,    0);
        IEM_MC_ARG(uint8_t,     cShiftArg,  1);
        IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        uint8_t cShift; IEM_OPCODE_GET_NEXT_U8(&cShift);
        IEM_MC_ASSIGN(cShiftArg, cShift);
        IEM_MC_MEM_MAP(pu8Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
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
        case 6: return IEMOP_RAISE_INVALID_LOCK_PREFIX();
        IEM_NOT_REACHED_DEFAULT_CASE_RET(); /* gcc maybe stupid */
    }
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF | X86_EFL_AF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register */
        uint8_t cShift; IEM_OPCODE_GET_NEXT_U8(&cShift);
        IEMOP_HLP_NO_LOCK_PREFIX();
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *,      pu16Dst,           0);
                IEM_MC_ARG_CONST(uint8_t,   cShiftArg, cShift, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,           2);
                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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
                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, cShiftArg, pEFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint64_t *,      pu64Dst,           0);
                IEM_MC_ARG_CONST(uint8_t,   cShiftArg, cShift, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,           2);
                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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
        IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint16_t *,  pu16Dst,    0);
                IEM_MC_ARG(uint8_t,     cShiftArg,  1);
                IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                uint8_t cShift; IEM_OPCODE_GET_NEXT_U8(&cShift);
                IEM_MC_ASSIGN(cShiftArg, cShift);
                IEM_MC_MEM_MAP(pu16Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                uint8_t cShift; IEM_OPCODE_GET_NEXT_U8(&cShift);
                IEM_MC_ASSIGN(cShiftArg, cShift);
                IEM_MC_MEM_MAP(pu32Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                uint8_t cShift; IEM_OPCODE_GET_NEXT_U8(&cShift);
                IEM_MC_ASSIGN(cShiftArg, cShift);
                IEM_MC_MEM_MAP(pu64Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
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
    IEMOP_HLP_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_retn, pIemCpu->enmEffOpSize, u16Imm);
}


/** Opcode 0xc3. */
FNIEMOP_DEF(iemOp_retn)
{
    IEMOP_MNEMONIC("retn");
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    IEMOP_HLP_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_retn, pIemCpu->enmEffOpSize, 0);
}


/** Opcode 0xc4. */
FNIEMOP_DEF(iemOp_les_Gv_Mp)
{
    IEMOP_MNEMONIC("les Gv,Mp");
    return FNIEMOP_CALL_1(iemOpCommonLoadSRegAndGreg, X86_SREG_ES);
}


/** Opcode 0xc5. */
FNIEMOP_DEF(iemOp_lds_Gv_Mp)
{
    IEMOP_MNEMONIC("lds Gv,Mp");
    return FNIEMOP_CALL_1(iemOpCommonLoadSRegAndGreg, X86_SREG_DS);
}


/** Opcode 0xc6. */
FNIEMOP_DEF(iemOp_Grp11_Eb_Ib)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */
    if ((bRm & X86_MODRM_REG_MASK) != (0 << X86_MODRM_REG_SHIFT)) /* only mov Eb,Ib in this group. */
        return IEMOP_RAISE_INVALID_LOCK_PREFIX();
    IEMOP_MNEMONIC("mov Eb,Ib");

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register access */
        uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
        IEM_MC_BEGIN(0, 0);
        IEM_MC_STORE_GREG_U8((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, u8Imm);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory access. */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
        IEM_MC_STORE_MEM_U8(pIemCpu->iEffSeg, GCPtrEffDst, u8Imm);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0xc7. */
FNIEMOP_DEF(iemOp_Grp11_Ev_Iz)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */
    if ((bRm & X86_MODRM_REG_MASK) != (0 << X86_MODRM_REG_SHIFT)) /* only mov Eb,Ib in this group. */
        return IEMOP_RAISE_INVALID_LOCK_PREFIX();
    IEMOP_MNEMONIC("mov Ev,Iz");

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register access */
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 0);
                uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
                IEM_MC_STORE_GREG_U16((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, u16Imm);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 0);
                uint32_t u32Imm; IEM_OPCODE_GET_NEXT_U32(&u32Imm);
                IEM_MC_STORE_GREG_U32((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, u32Imm);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 0);
                uint64_t u64Imm; IEM_OPCODE_GET_NEXT_U64(&u64Imm);
                IEM_MC_STORE_GREG_U64((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB, u64Imm);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        /* memory access. */
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
                IEM_MC_STORE_MEM_U16(pIemCpu->iEffSeg, GCPtrEffDst, u16Imm);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                uint32_t u32Imm; IEM_OPCODE_GET_NEXT_U32(&u32Imm);
                IEM_MC_STORE_MEM_U32(pIemCpu->iEffSeg, GCPtrEffDst, u32Imm);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                uint64_t u64Imm; IEM_OPCODE_GET_NEXT_U64(&u64Imm);
                IEM_MC_STORE_MEM_U64(pIemCpu->iEffSeg, GCPtrEffDst, u64Imm);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}




/** Opcode 0xc8. */
FNIEMOP_STUB(iemOp_enter_Iw_Ib);


/** Opcode 0xc9. */
FNIEMOP_DEF(iemOp_leave)
{
    IEMOP_MNEMONIC("retn");
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    IEMOP_HLP_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_leave, pIemCpu->enmEffOpSize);
}


/** Opcode 0xca. */
FNIEMOP_DEF(iemOp_retf_Iw)
{
    IEMOP_MNEMONIC("retf Iw");
    uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
    IEMOP_HLP_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_retf, pIemCpu->enmEffOpSize, u16Imm);
}


/** Opcode 0xcb. */
FNIEMOP_DEF(iemOp_retf)
{
    IEMOP_MNEMONIC("retf");
    IEMOP_HLP_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_retf, pIemCpu->enmEffOpSize, 0);
}


/** Opcode 0xcc. */
FNIEMOP_DEF(iemOp_int_3)
{
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_int, X86_XCPT_BP, true /*fIsBpInstr*/);
}


/** Opcode 0xcd. */
FNIEMOP_DEF(iemOp_int_Ib)
{
    uint8_t u8Int; IEM_OPCODE_GET_NEXT_U8(&u8Int);
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_int, u8Int, false /*fIsBpInstr*/);
}


/** Opcode 0xce. */
FNIEMOP_DEF(iemOp_into)
{
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
    IEMOP_HLP_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_iret, pIemCpu->enmEffOpSize);
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
        case 6: return IEMOP_RAISE_INVALID_LOCK_PREFIX();
        IEM_NOT_REACHED_DEFAULT_CASE_RET(); /* gcc maybe, well... */
    }
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF | X86_EFL_AF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register */
        IEMOP_HLP_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(3, 0);
        IEM_MC_ARG(uint8_t *,       pu8Dst,             0);
        IEM_MC_ARG_CONST(uint8_t,   cShiftArg,/*=*/1,   1);
        IEM_MC_ARG(uint32_t *,      pEFlags,            2);
        IEM_MC_REF_GREG_U8(pu8Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
        IEM_MC_REF_EFLAGS(pEFlags);
        IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU8, pu8Dst, cShiftArg, pEFlags);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory */
        IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */
        IEM_MC_BEGIN(3, 2);
        IEM_MC_ARG(uint8_t *,       pu8Dst,             0);
        IEM_MC_ARG_CONST(uint8_t,   cShiftArg,/*=*/1,   1);
        IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags,        2);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_MEM_MAP(pu8Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
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
        case 6: return IEMOP_RAISE_INVALID_LOCK_PREFIX();
        IEM_NOT_REACHED_DEFAULT_CASE_RET(); /* gcc maybe, well... */
    }
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF | X86_EFL_AF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register */
        IEMOP_HLP_NO_LOCK_PREFIX();
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *,      pu16Dst,           0);
                IEM_MC_ARG_CONST(uint8_t,   cShiftArg,/*=1*/1, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,           2);
                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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
                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, cShiftArg, pEFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint64_t *,      pu64Dst,           0);
                IEM_MC_ARG_CONST(uint8_t,   cShiftArg,/*=1*/1, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,           2);
                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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
        IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint16_t *,      pu16Dst,            0);
                IEM_MC_ARG_CONST(uint8_t,   cShiftArg,/*=1*/1,  1);
                IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags,        2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_MEM_MAP(pu16Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_MEM_MAP(pu32Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_MEM_MAP(pu64Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
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
        case 6: return IEMOP_RAISE_INVALID_LOCK_PREFIX();
        IEM_NOT_REACHED_DEFAULT_CASE_RET(); /* gcc, grr. */
    }
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF | X86_EFL_AF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register */
        IEMOP_HLP_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(3, 0);
        IEM_MC_ARG(uint8_t *,   pu8Dst,     0);
        IEM_MC_ARG(uint8_t,     cShiftArg,  1);
        IEM_MC_ARG(uint32_t *,  pEFlags,    2);
        IEM_MC_REF_GREG_U8(pu8Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
        IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
        IEM_MC_REF_EFLAGS(pEFlags);
        IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU8, pu8Dst, cShiftArg, pEFlags);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory */
        IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */
        IEM_MC_BEGIN(3, 2);
        IEM_MC_ARG(uint8_t *,   pu8Dst,          0);
        IEM_MC_ARG(uint8_t,     cShiftArg,       1);
        IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
        IEM_MC_MEM_MAP(pu8Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
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
        IEMOP_HLP_NO_LOCK_PREFIX();
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *,      pu16Dst,    0);
                IEM_MC_ARG(uint8_t,         cShiftArg,  1);
                IEM_MC_ARG(uint32_t *,      pEFlags,    2);
                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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
                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, cShiftArg, pEFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint64_t *,      pu64Dst,    0);
                IEM_MC_ARG(uint8_t,         cShiftArg,  1);
                IEM_MC_ARG(uint32_t *,      pEFlags,    2);
                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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
        IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint16_t *,  pu16Dst,    0);
                IEM_MC_ARG(uint8_t,     cShiftArg,  1);
                IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
                IEM_MC_MEM_MAP(pu16Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
                IEM_MC_MEM_MAP(pu32Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
                IEM_MC_MEM_MAP(pu64Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
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
    IEMOP_HLP_NO_LOCK_PREFIX();
    IEMOP_HLP_NO_64BIT();
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_aam, bImm);
}


/** Opcode 0xd5. */
FNIEMOP_DEF(iemOp_aad_Ib)
{
    IEMOP_MNEMONIC("aad Ib");
    uint8_t bImm; IEM_OPCODE_GET_NEXT_U8(&bImm);
    IEMOP_HLP_NO_LOCK_PREFIX();
    IEMOP_HLP_NO_64BIT();
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_aad, bImm);
}


/** Opcode 0xd7. */
FNIEMOP_DEF(iemOp_xlat)
{
    IEMOP_MNEMONIC("xlat");
    IEMOP_HLP_NO_LOCK_PREFIX();
    switch (pIemCpu->enmEffAddrMode)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(2, 0);
            IEM_MC_LOCAL(uint8_t,  u8Tmp);
            IEM_MC_LOCAL(uint16_t, u16Addr);
            IEM_MC_FETCH_GREG_U8_ZX_U16(u16Addr, X86_GREG_xAX);
            IEM_MC_ADD_GREG_U16_TO_LOCAL(u16Addr, X86_GREG_xBX);
            IEM_MC_FETCH_MEM16_U8(u8Tmp, pIemCpu->iEffSeg, u16Addr);
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
            IEM_MC_FETCH_MEM32_U8(u8Tmp, pIemCpu->iEffSeg, u32Addr);
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
            IEM_MC_FETCH_MEM_U8(u8Tmp, pIemCpu->iEffSeg, u64Addr);
            IEM_MC_STORE_GREG_U8(X86_GREG_xAX, u8Tmp);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

         IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/** Opcode 0xd8. */
FNIEMOP_STUB(iemOp_EscF0);
/** Opcode 0xd9. */
FNIEMOP_STUB(iemOp_EscF1);
/** Opcode 0xda. */
FNIEMOP_STUB(iemOp_EscF2);


/** Opcode 0xdb /0. */
FNIEMOP_STUB_1(iemOp_fild_dw, uint8_t, bRm);
/** Opcode 0xdb /1. */
FNIEMOP_STUB_1(iemOp_fisttp_dw, uint8_t, bRm);
/** Opcode 0xdb /2. */
FNIEMOP_STUB_1(iemOp_fist_dw, uint8_t, bRm);
/** Opcode 0xdb /3. */
FNIEMOP_STUB_1(iemOp_fistp_dw, uint8_t, bRm);
/** Opcode 0xdb /5. */
FNIEMOP_STUB_1(iemOp_fld_xr, uint8_t, bRm);
/** Opcode 0xdb /7. */
FNIEMOP_STUB_1(iemOp_fstp_xr, uint8_t, bRm);


/** Opcode 0xdb 0xe0. */
FNIEMOP_DEF(iemOp_fneni)
{
    IEMOP_MNEMONIC("fneni (8087/ign)");
    IEM_MC_BEGIN(0,0);
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdb 0xe1. */
FNIEMOP_DEF(iemOp_fndisi)
{
    IEMOP_MNEMONIC("fndisi (8087/ign)");
    IEM_MC_BEGIN(0,0);
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdb 0xe2. */
FNIEMOP_STUB(iemOp_fnclex);


/** Opcode 0xdb 0xe3. */
FNIEMOP_DEF(iemOp_fninit)
{
    IEMOP_MNEMONIC("fninit");
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_finit, false /*fCheckXcpts*/);
}


/** Opcode 0xdb 0xe4. */
FNIEMOP_DEF(iemOp_fnsetpm)
{
    IEMOP_MNEMONIC("fnsetpm (80287/ign)");   /* set protected mode on fpu. */
    IEM_MC_BEGIN(0,0);
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdb 0xe5. */
FNIEMOP_DEF(iemOp_frstpm)
{
    IEMOP_MNEMONIC("frstpm (80287XL/ign)"); /* reset pm, back to real mode. */
    IEM_MC_BEGIN(0,0);
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdb. */
FNIEMOP_DEF(iemOp_EscF3)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        switch (bRm & 0xf8)
        {
            case 0xc0: AssertFailedReturn(VERR_NOT_IMPLEMENTED); // fcmovnb
            case 0xc8: AssertFailedReturn(VERR_NOT_IMPLEMENTED); // fcmovne
            case 0xd0: AssertFailedReturn(VERR_NOT_IMPLEMENTED); // fcmovnbe
            case 0xd8: AssertFailedReturn(VERR_NOT_IMPLEMENTED); // fcmovnu
            case 0xe0:
                IEMOP_HLP_NO_LOCK_PREFIX();
                switch (bRm)
                {
                    case 0xe0:  return FNIEMOP_CALL(iemOp_fneni);
                    case 0xe1:  return FNIEMOP_CALL(iemOp_fndisi);
                    case 0xe2:  return FNIEMOP_CALL(iemOp_fnclex);
                    case 0xe3:  return FNIEMOP_CALL(iemOp_fninit);
                    case 0xe4:  return FNIEMOP_CALL(iemOp_fnsetpm);
                    case 0xe5:  return FNIEMOP_CALL(iemOp_frstpm);
                    default:    return IEMOP_RAISE_INVALID_OPCODE();
                }
                break;
            case 0xe8: AssertFailedReturn(VERR_NOT_IMPLEMENTED); // fucomi
            case 0xf0: AssertFailedReturn(VERR_NOT_IMPLEMENTED); // fcomi
            case 0xf8: return IEMOP_RAISE_INVALID_OPCODE();
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
        {
            case 0: return FNIEMOP_CALL_1(iemOp_fild_dw,  bRm);
            case 1: return FNIEMOP_CALL_1(iemOp_fisttp_dw,bRm);
            case 2: return FNIEMOP_CALL_1(iemOp_fist_dw,  bRm);
            case 3: return FNIEMOP_CALL_1(iemOp_fistp_dw, bRm);
            case 4: return IEMOP_RAISE_INVALID_OPCODE();
            case 5: return FNIEMOP_CALL_1(iemOp_fld_xr,   bRm);
            case 6: return IEMOP_RAISE_INVALID_OPCODE();
            case 7: return FNIEMOP_CALL_1(iemOp_fstp_xr,  bRm);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}

/** Opcode 0xdc. */
FNIEMOP_STUB(iemOp_EscF4);
/** Opcode 0xdd. */
FNIEMOP_STUB(iemOp_EscF5);

/** Opcode 0xde 0xd9. */
FNIEMOP_STUB(iemOp_fcompp);

/** Opcode 0xde. */
FNIEMOP_DEF(iemOp_EscF6)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        switch (bRm & 0xf8)
        {
            case 0xc0: AssertFailedReturn(VERR_NOT_IMPLEMENTED); // fiaddp
            case 0xc8: AssertFailedReturn(VERR_NOT_IMPLEMENTED); // fimulp
            case 0xd0: return IEMOP_RAISE_INVALID_OPCODE();
            case 0xd8:
                switch (bRm)
                {
                    case 0xd9:  return FNIEMOP_CALL(iemOp_fcompp);
                    default:    return IEMOP_RAISE_INVALID_OPCODE();
                }
            case 0xe0: AssertFailedReturn(VERR_NOT_IMPLEMENTED); // fsubrp
            case 0xe8: AssertFailedReturn(VERR_NOT_IMPLEMENTED); // fsubp
            case 0xf0: AssertFailedReturn(VERR_NOT_IMPLEMENTED); // fdivrp
            case 0xf8: AssertFailedReturn(VERR_NOT_IMPLEMENTED); // fdivp
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
#if 0
        switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
        {
            case 0: return FNIEMOP_CALL_1(iemOp_fiadd_w,  bRm);
            case 1: return FNIEMOP_CALL_1(iemOp_fimul_w,  bRm);
            case 2: return FNIEMOP_CALL_1(iemOp_ficom_w,  bRm);
            case 3: return FNIEMOP_CALL_1(iemOp_ficomp_w, bRm);
            case 4: return FNIEMOP_CALL_1(iemOp_fisub_w,  bRm);
            case 5: return FNIEMOP_CALL_1(iemOp_fisubr_w, bRm);
            case 6: return FNIEMOP_CALL_1(iemOp_fidiv_w,  bRm);
            case 7: return FNIEMOP_CALL_1(iemOp_fidivr_w, bRm);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
#endif
        AssertFailedReturn(VERR_NOT_IMPLEMENTED);
    }
}


/** Opcode 0xdf 0xe0. */
FNIEMOP_DEF(iemOp_fnstsw_ax)
{
    IEMOP_MNEMONIC("fnstsw ax");
    IEMOP_HLP_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(0, 1);
    IEM_MC_LOCAL(uint16_t, u16Tmp);
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_FETCH_FSW(u16Tmp);
    IEM_MC_STORE_GREG_U16(X86_GREG_xAX, u16Tmp);
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
        switch (bRm & 0xf8)
        {
            case 0xc0: return IEMOP_RAISE_INVALID_OPCODE();
            case 0xc8: return IEMOP_RAISE_INVALID_OPCODE();
            case 0xd0: return IEMOP_RAISE_INVALID_OPCODE();
            case 0xd8: return IEMOP_RAISE_INVALID_OPCODE();
            case 0xe0:
                switch (bRm)
                {
                    case 0xe0: return FNIEMOP_CALL(iemOp_fnstsw_ax);
                    default:   return IEMOP_RAISE_INVALID_OPCODE();
                }
            case 0xe8: AssertFailedReturn(VERR_NOT_IMPLEMENTED); // fucomip
            case 0xf0: AssertFailedReturn(VERR_NOT_IMPLEMENTED); // fcomip
            case 0xf8: return IEMOP_RAISE_INVALID_OPCODE();
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        AssertFailedReturn(VERR_NOT_IMPLEMENTED);
    }
}


/** Opcode 0xe0. */
FNIEMOP_DEF(iemOp_loopne_Jb)
{
    IEMOP_MNEMONIC("loopne Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    switch (pIemCpu->enmEffAddrMode)
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
    IEMOP_HLP_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    switch (pIemCpu->enmEffAddrMode)
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
    IEMOP_HLP_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    /** @todo Check out the #GP case if EIP < CS.Base or EIP > CS.Limit when
     * using the 32-bit operand size override.  How can that be restarted?  See
     * weird pseudo code in intel manual. */
    switch (pIemCpu->enmEffAddrMode)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(0,0);
            IEM_MC_SUB_GREG_U16(X86_GREG_xCX, 1);
            IEM_MC_IF_CX_IS_NZ() {
                IEM_MC_REL_JMP_S8(i8Imm);
            } IEM_MC_ELSE() {
                IEM_MC_ADVANCE_RIP();
            } IEM_MC_ENDIF();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(0,0);
            IEM_MC_SUB_GREG_U32(X86_GREG_xCX, 1);
            IEM_MC_IF_ECX_IS_NZ() {
                IEM_MC_REL_JMP_S8(i8Imm);
            } IEM_MC_ELSE() {
                IEM_MC_ADVANCE_RIP();
            } IEM_MC_ENDIF();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(0,0);
            IEM_MC_SUB_GREG_U64(X86_GREG_xCX, 1);
            IEM_MC_IF_RCX_IS_NZ() {
                IEM_MC_REL_JMP_S8(i8Imm);
            } IEM_MC_ELSE() {
                IEM_MC_ADVANCE_RIP();
            } IEM_MC_ENDIF();
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
    IEMOP_HLP_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    switch (pIemCpu->enmEffAddrMode)
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
    IEMOP_HLP_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_in, u8Imm, 1);
}


/** Opcode 0xe5 */
FNIEMOP_DEF(iemOp_in_eAX_Ib)
{
    IEMOP_MNEMONIC("in eAX,Ib");
    uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
    IEMOP_HLP_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_in, u8Imm, pIemCpu->enmEffOpSize == IEMMODE_16BIT ? 2 : 4);
}


/** Opcode 0xe6 */
FNIEMOP_DEF(iemOp_out_Ib_AL)
{
    IEMOP_MNEMONIC("out Ib,AL");
    uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
    IEMOP_HLP_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_out, u8Imm, 1);
}


/** Opcode 0xe7 */
FNIEMOP_DEF(iemOp_out_Ib_eAX)
{
    IEMOP_MNEMONIC("out Ib,eAX");
    uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
    IEMOP_HLP_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_out, u8Imm, pIemCpu->enmEffOpSize == IEMMODE_16BIT ? 2 : 4);
}


/** Opcode 0xe8. */
FNIEMOP_DEF(iemOp_call_Jv)
{
    IEMOP_MNEMONIC("call Jv");
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
        {
            uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
            return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_call_rel_16, (int32_t)u16Imm);
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
    switch (pIemCpu->enmEffOpSize)
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
    if (pIemCpu->enmEffOpSize != IEMMODE_16BIT)
        IEM_OPCODE_GET_NEXT_U32(&offSeg);
    else
    {
        uint16_t offSeg16; IEM_OPCODE_GET_NEXT_U16(&offSeg16);
        offSeg = offSeg16;
    }
    uint16_t uSel;  IEM_OPCODE_GET_NEXT_U16(&uSel);
    IEMOP_HLP_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_3(iemCImpl_FarJmp, uSel, offSeg, pIemCpu->enmEffOpSize);
}


/** Opcode 0xeb. */
FNIEMOP_DEF(iemOp_jmp_Jb)
{
    IEMOP_MNEMONIC("jmp Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_NO_LOCK_PREFIX();
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
    IEMOP_HLP_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_in_eAX_DX, 1);
}


/** Opcode 0xed */
FNIEMOP_DEF(iemOp_eAX_DX)
{
    IEMOP_MNEMONIC("in  eAX,DX");
    IEMOP_HLP_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_in_eAX_DX, pIemCpu->enmEffOpSize == IEMMODE_16BIT ? 2 : 4);
}


/** Opcode 0xee */
FNIEMOP_DEF(iemOp_out_DX_AL)
{
    IEMOP_MNEMONIC("out DX,AL");
    IEMOP_HLP_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_out_DX_eAX, 1);
}


/** Opcode 0xef */
FNIEMOP_DEF(iemOp_out_DX_eAX)
{
    IEMOP_MNEMONIC("out DX,eAX");
    IEMOP_HLP_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_out_DX_eAX, pIemCpu->enmEffOpSize == IEMMODE_16BIT ? 2 : 4);
}


/** Opcode 0xf0. */
FNIEMOP_DEF(iemOp_lock)
{
    pIemCpu->fPrefixes |= IEM_OP_PRF_LOCK;

    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    return FNIEMOP_CALL(g_apfnOneByteMap[b]);
}


/** Opcode 0xf2. */
FNIEMOP_DEF(iemOp_repne)
{
    /* This overrides any previous REPE prefix. */
    pIemCpu->fPrefixes &= ~IEM_OP_PRF_REPZ;
    pIemCpu->fPrefixes |= IEM_OP_PRF_REPNZ;

    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    return FNIEMOP_CALL(g_apfnOneByteMap[b]);
}


/** Opcode 0xf3. */
FNIEMOP_DEF(iemOp_repe)
{
    /* This overrides any previous REPNE prefix. */
    pIemCpu->fPrefixes &= ~IEM_OP_PRF_REPNZ;
    pIemCpu->fPrefixes |= IEM_OP_PRF_REPZ;

    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    return FNIEMOP_CALL(g_apfnOneByteMap[b]);
}


/** Opcode 0xf4. */
FNIEMOP_DEF(iemOp_hlt)
{
    IEMOP_HLP_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_hlt);
}


/** Opcode 0xf5. */
FNIEMOP_DEF(iemOp_cmc)
{
    IEMOP_MNEMONIC("cmc");
    IEMOP_HLP_NO_LOCK_PREFIX();
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
        IEM_MC_REF_GREG_U8(pu8Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_MEM_MAP(pu8Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
        IEM_MC_FETCH_EFLAGS(EFlags);
        if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
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
        return FNIEMOP_CALL_2(iemOpCommonUnaryGReg, pImpl, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);

    /* Memory we do here. */
    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(2, 2);
            IEM_MC_ARG(uint16_t *,      pu16Dst,         0);
            IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags, 1);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
            IEM_MC_MEM_MAP(pu16Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
            IEM_MC_FETCH_EFLAGS(EFlags);
            if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
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

            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
            IEM_MC_MEM_MAP(pu32Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
            IEM_MC_FETCH_EFLAGS(EFlags);
            if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
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

            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
            IEM_MC_MEM_MAP(pu64Dst, IEM_ACCESS_DATA_RW, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
            IEM_MC_FETCH_EFLAGS(EFlags);
            if (!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))
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
        IEMOP_HLP_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(3, 0);
        IEM_MC_ARG(uint8_t *,       pu8Dst,             0);
        IEM_MC_ARG_CONST(uint8_t,   u8Src,/*=*/u8Imm,   1);
        IEM_MC_ARG(uint32_t *,      pEFlags,            2);
        IEM_MC_REF_GREG_U8(pu8Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
        IEM_MC_REF_EFLAGS(pEFlags);
        IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_test_u8, pu8Dst, u8Src, pEFlags);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory access. */
        IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */

        IEM_MC_BEGIN(3, 2);
        IEM_MC_ARG(uint8_t *,       pu8Dst,             0);
        IEM_MC_ARG(uint8_t,         u8Src,              1);
        IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags,    2);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
        IEM_MC_ASSIGN(u8Src, u8Imm);
        IEM_MC_MEM_MAP(pu8Dst, IEM_ACCESS_DATA_R, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
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
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register access */
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
            {
                uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                0);
                IEM_MC_ARG_CONST(uint16_t,  u16Src,/*=*/u16Imm,     1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                2);
                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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
                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_test_u32, pu32Dst, u32Src, pEFlags);
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
                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
            {
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint16_t *,      pu16Dst,            0);
                IEM_MC_ARG(uint16_t,        u16Src,             1);
                IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags,    2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
                IEM_MC_ASSIGN(u16Src, u16Imm);
                IEM_MC_MEM_MAP(pu16Dst, IEM_ACCESS_DATA_R, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                uint32_t u32Imm; IEM_OPCODE_GET_NEXT_U32(&u32Imm);
                IEM_MC_ASSIGN(u32Src, u32Imm);
                IEM_MC_MEM_MAP(pu32Dst, IEM_ACCESS_DATA_R, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
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

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                uint64_t u64Imm; IEM_OPCODE_GET_NEXT_S32_SX_U64(&u64Imm);
                IEM_MC_ASSIGN(u64Src, u64Imm);
                IEM_MC_MEM_MAP(pu64Dst, IEM_ACCESS_DATA_R, pIemCpu->iEffSeg, GCPtrEffDst, 0 /*arg*/);
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
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register access */
        IEMOP_HLP_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(3, 0);
        IEM_MC_ARG(uint16_t *,      pu16AX,     0);
        IEM_MC_ARG(uint8_t,         u8Value,    1);
        IEM_MC_ARG(uint32_t *,      pEFlags,    2);
        IEM_MC_FETCH_GREG_U8(u8Value, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
        IEM_MC_REF_GREG_U16(pu16AX, X86_GREG_xAX);
        IEM_MC_REF_EFLAGS(pEFlags);
        IEM_MC_CALL_VOID_AIMPL_3(pfnU8, pu16AX, u8Value, pEFlags);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory access. */
        IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */

        IEM_MC_BEGIN(3, 1);
        IEM_MC_ARG(uint16_t *,      pu16AX,     0);
        IEM_MC_ARG(uint8_t,         u8Value,    1);
        IEM_MC_ARG(uint32_t *,      pEFlags,    2);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
        IEM_MC_FETCH_MEM_U8(u8Value, pIemCpu->iEffSeg, GCPtrEffDst);
        IEM_MC_REF_GREG_U16(pu16AX, X86_GREG_xAX);
        IEM_MC_REF_EFLAGS(pEFlags);
        IEM_MC_CALL_VOID_AIMPL_3(pfnU8, pu16AX, u8Value, pEFlags);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0xf7 /4, /5, /6 and /7. */
FNIEMOP_DEF_2(iemOpCommonGrp3MulDivEv, uint8_t, bRm, PCIEMOPMULDIVSIZES, pImpl)
{
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register access */
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
            {
                IEMOP_HLP_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(4, 1);
                IEM_MC_ARG(uint16_t *,      pu16AX,     0);
                IEM_MC_ARG(uint16_t *,      pu16DX,     1);
                IEM_MC_ARG(uint16_t,        u16Value,   2);
                IEM_MC_ARG(uint32_t *,      pEFlags,    3);
                IEM_MC_LOCAL(int32_t,       rc);

                IEM_MC_FETCH_GREG_U16(u16Value, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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
                IEMOP_HLP_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(4, 1);
                IEM_MC_ARG(uint32_t *,      pu32AX,     0);
                IEM_MC_ARG(uint32_t *,      pu32DX,     1);
                IEM_MC_ARG(uint32_t,        u32Value,   2);
                IEM_MC_ARG(uint32_t *,      pEFlags,    3);
                IEM_MC_LOCAL(int32_t,       rc);

                IEM_MC_FETCH_GREG_U32(u32Value, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_REF_GREG_U32(pu32AX, X86_GREG_xAX);
                IEM_MC_REF_GREG_U32(pu32DX, X86_GREG_xDX);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_AIMPL_4(rc, pImpl->pfnU32, pu32AX, pu32DX, u32Value, pEFlags);
                IEM_MC_IF_LOCAL_IS_Z(rc) {
                    IEM_MC_ADVANCE_RIP();
                } IEM_MC_ELSE() {
                    IEM_MC_RAISE_DIVIDE_ERROR();
                } IEM_MC_ENDIF();

                IEM_MC_END();
                return VINF_SUCCESS;
            }

            case IEMMODE_64BIT:
            {
                IEMOP_HLP_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(4, 1);
                IEM_MC_ARG(uint64_t *,      pu64AX,     0);
                IEM_MC_ARG(uint64_t *,      pu64DX,     1);
                IEM_MC_ARG(uint64_t,        u64Value,   2);
                IEM_MC_ARG(uint32_t *,      pEFlags,    3);
                IEM_MC_LOCAL(int32_t,       rc);

                IEM_MC_FETCH_GREG_U64(u64Value, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
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
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
            {
                IEMOP_HLP_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(4, 2);
                IEM_MC_ARG(uint16_t *,      pu16AX,     0);
                IEM_MC_ARG(uint16_t *,      pu16DX,     1);
                IEM_MC_ARG(uint16_t,        u16Value,   2);
                IEM_MC_ARG(uint32_t *,      pEFlags,    3);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_LOCAL(int32_t,       rc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_MEM_U16(u16Value, pIemCpu->iEffSeg, GCPtrEffDst);
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
                IEMOP_HLP_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(4, 2);
                IEM_MC_ARG(uint32_t *,      pu32AX,     0);
                IEM_MC_ARG(uint32_t *,      pu32DX,     1);
                IEM_MC_ARG(uint32_t,        u32Value,   2);
                IEM_MC_ARG(uint32_t *,      pEFlags,    3);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_LOCAL(int32_t,       rc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_MEM_U32(u32Value, pIemCpu->iEffSeg, GCPtrEffDst);
                IEM_MC_REF_GREG_U32(pu32AX, X86_GREG_xAX);
                IEM_MC_REF_GREG_U32(pu32DX, X86_GREG_xDX);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_AIMPL_4(rc, pImpl->pfnU32, pu32AX, pu32DX, u32Value, pEFlags);
                IEM_MC_IF_LOCAL_IS_Z(rc) {
                    IEM_MC_ADVANCE_RIP();
                } IEM_MC_ELSE() {
                    IEM_MC_RAISE_DIVIDE_ERROR();
                } IEM_MC_ENDIF();

                IEM_MC_END();
                return VINF_SUCCESS;
            }

            case IEMMODE_64BIT:
            {
                IEMOP_HLP_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(4, 2);
                IEM_MC_ARG(uint64_t *,      pu64AX,     0);
                IEM_MC_ARG(uint64_t *,      pu64DX,     1);
                IEM_MC_ARG(uint64_t,        u64Value,   2);
                IEM_MC_ARG(uint32_t *,      pEFlags,    3);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_LOCAL(int32_t,       rc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm);
                IEM_MC_FETCH_MEM_U64(u64Value, pIemCpu->iEffSeg, GCPtrEffDst);
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
            return IEMOP_RAISE_INVALID_LOCK_PREFIX();
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
            return IEMOP_RAISE_INVALID_LOCK_PREFIX();
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
    IEMOP_HLP_NO_LOCK_PREFIX();
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
    IEMOP_HLP_NO_LOCK_PREFIX();
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
    IEMOP_HLP_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_cli);
}


FNIEMOP_DEF(iemOp_sti)
{
    IEMOP_MNEMONIC("sti");
    IEMOP_HLP_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_sti);
}


/** Opcode 0xfc. */
FNIEMOP_DEF(iemOp_cld)
{
    IEMOP_MNEMONIC("cld");
    IEMOP_HLP_NO_LOCK_PREFIX();
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
    IEMOP_HLP_NO_LOCK_PREFIX();
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
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo Too early? */
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* The new RIP is taken from a register. */
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(1, 0);
                IEM_MC_ARG(uint16_t, u16Target, 0);
                IEM_MC_FETCH_GREG_U16(u16Target, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_CALL_CIMPL_1(iemCImpl_call_16, u16Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(1, 0);
                IEM_MC_ARG(uint32_t, u32Target, 0);
                IEM_MC_FETCH_GREG_U32(u32Target, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_CALL_CIMPL_1(iemCImpl_call_32, u32Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(1, 0);
                IEM_MC_ARG(uint64_t, u64Target, 0);
                IEM_MC_FETCH_GREG_U64(u64Target, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_CALL_CIMPL_1(iemCImpl_call_64, u64Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        /* The new RIP is taken from a register. */
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(1, 1);
                IEM_MC_ARG(uint16_t,  u16Target, 0);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm);
                IEM_MC_FETCH_MEM_U16(u16Target, pIemCpu->iEffSeg, GCPtrEffSrc);
                IEM_MC_CALL_CIMPL_1(iemCImpl_call_16, u16Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(1, 1);
                IEM_MC_ARG(uint32_t,  u32Target, 0);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm);
                IEM_MC_FETCH_MEM_U32(u32Target, pIemCpu->iEffSeg, GCPtrEffSrc);
                IEM_MC_CALL_CIMPL_1(iemCImpl_call_32, u32Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(1, 1);
                IEM_MC_ARG(uint64_t,  u64Target, 0);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm);
                IEM_MC_FETCH_MEM_U64(u64Target, pIemCpu->iEffSeg, GCPtrEffSrc);
                IEM_MC_CALL_CIMPL_1(iemCImpl_call_64, u64Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/**
 * Opcode 0xff /3.
 * @param   bRm             The RM byte.
 */
FNIEMOP_DEF_1(iemOp_Grp5_callf_Ep, uint8_t, bRm)
{
    IEMOP_MNEMONIC("callf Ep");
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo Too early? */

    /* Registers? How?? */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /** @todo How the heck does a 'callf eax' work? Probably just have to
         *        search the docs... */
        AssertFailedReturn(VERR_NOT_IMPLEMENTED);
    }

    /* Far pointer loaded from memory. */
    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(3, 1);
            IEM_MC_ARG(uint16_t,        u16Sel,                         0);
            IEM_MC_ARG(uint16_t,        offSeg,                         1);
            IEM_MC_ARG_CONST(IEMMODE,   enmEffOpSize, IEMMODE_16BIT,    2);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm);
            IEM_MC_FETCH_MEM_U16(offSeg, pIemCpu->iEffSeg, GCPtrEffSrc);
            IEM_MC_FETCH_MEM_U16_DISP(u16Sel, pIemCpu->iEffSeg, GCPtrEffSrc, 2);
            IEM_MC_CALL_CIMPL_3(iemCImpl_callf, u16Sel, offSeg, enmEffOpSize);
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(3, 1);
            IEM_MC_ARG(uint16_t,        u16Sel,                         0);
            IEM_MC_ARG(uint32_t,        offSeg,                         1);
            IEM_MC_ARG_CONST(IEMMODE,   enmEffOpSize, IEMMODE_32BIT,    2);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm);
            IEM_MC_FETCH_MEM_U32(offSeg, pIemCpu->iEffSeg, GCPtrEffSrc);
            IEM_MC_FETCH_MEM_U16_DISP(u16Sel, pIemCpu->iEffSeg, GCPtrEffSrc, 4);
            IEM_MC_CALL_CIMPL_3(iemCImpl_callf, u16Sel, offSeg, enmEffOpSize);
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(3, 1);
            IEM_MC_ARG(uint16_t,        u16Sel,                         0);
            IEM_MC_ARG(uint64_t,        offSeg,                         1);
            IEM_MC_ARG_CONST(IEMMODE,   enmEffOpSize, IEMMODE_16BIT,    2);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm);
            IEM_MC_FETCH_MEM_U64(offSeg, pIemCpu->iEffSeg, GCPtrEffSrc);
            IEM_MC_FETCH_MEM_U16_DISP(u16Sel, pIemCpu->iEffSeg, GCPtrEffSrc, 8);
            IEM_MC_CALL_CIMPL_3(iemCImpl_callf, u16Sel, offSeg, enmEffOpSize);
            IEM_MC_END();
            return VINF_SUCCESS;

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/**
 * Opcode 0xff /4.
 * @param   bRm             The RM byte.
 */
FNIEMOP_DEF_1(iemOp_Grp5_jmpn_Ev, uint8_t, bRm)
{
    IEMOP_MNEMONIC("jmpn Ev");
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo Too early? */
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* The new RIP is taken from a register. */
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint16_t, u16Target);
                IEM_MC_FETCH_GREG_U16(u16Target, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_SET_RIP_U16(u16Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint32_t, u32Target);
                IEM_MC_FETCH_GREG_U32(u32Target, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_SET_RIP_U32(u32Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint64_t, u64Target);
                IEM_MC_FETCH_GREG_U64(u64Target, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);
                IEM_MC_SET_RIP_U64(u64Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        /* The new RIP is taken from a register. */
        switch (pIemCpu->enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint16_t, u16Target);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm);
                IEM_MC_FETCH_MEM_U16(u16Target, pIemCpu->iEffSeg, GCPtrEffSrc);
                IEM_MC_SET_RIP_U16(u16Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint32_t, u32Target);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm);
                IEM_MC_FETCH_MEM_U32(u32Target, pIemCpu->iEffSeg, GCPtrEffSrc);
                IEM_MC_SET_RIP_U32(u32Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint32_t, u32Target);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm);
                IEM_MC_FETCH_MEM_U32(u32Target, pIemCpu->iEffSeg, GCPtrEffSrc);
                IEM_MC_SET_RIP_U32(u32Target);
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
    IEMOP_MNEMONIC("jmp Ep");
    IEMOP_HLP_NO_64BIT();
    /** @todo could share all the decoding with iemOp_Grp5_callf_Ep. */

    /* Decode the far pointer address and pass it on to the far call C
       implementation. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /** @todo How the heck does a 'callf eax' work? Probably just have to
         *        search the docs... */
        AssertFailedReturn(VERR_NOT_IMPLEMENTED);
    }

    /* Far pointer loaded from memory. */
    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(3, 1);
            IEM_MC_ARG(uint16_t,        u16Sel,                         0);
            IEM_MC_ARG(uint16_t,        offSeg,                         1);
            IEM_MC_ARG_CONST(IEMMODE,   enmEffOpSize, IEMMODE_16BIT,    2);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm);
            IEM_MC_FETCH_MEM_U16(offSeg, pIemCpu->iEffSeg, GCPtrEffSrc);
            IEM_MC_FETCH_MEM_U16_DISP(u16Sel, pIemCpu->iEffSeg, GCPtrEffSrc, 2);
            IEM_MC_CALL_CIMPL_3(iemCImpl_FarJmp, u16Sel, offSeg, enmEffOpSize);
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(3, 1);
            IEM_MC_ARG(uint16_t,        u16Sel,                         0);
            IEM_MC_ARG(uint32_t,        offSeg,                         1);
            IEM_MC_ARG_CONST(IEMMODE,   enmEffOpSize, IEMMODE_32BIT,    2);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm);
            IEM_MC_FETCH_MEM_U32(offSeg, pIemCpu->iEffSeg, GCPtrEffSrc);
            IEM_MC_FETCH_MEM_U16_DISP(u16Sel, pIemCpu->iEffSeg, GCPtrEffSrc, 4);
            IEM_MC_CALL_CIMPL_3(iemCImpl_FarJmp, u16Sel, offSeg, enmEffOpSize);
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(3, 1);
            IEM_MC_ARG(uint16_t,        u16Sel,                         0);
            IEM_MC_ARG(uint64_t,        offSeg,                         1);
            IEM_MC_ARG_CONST(IEMMODE,   enmEffOpSize, IEMMODE_16BIT,    2);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm);
            IEM_MC_FETCH_MEM_U64(offSeg, pIemCpu->iEffSeg, GCPtrEffSrc);
            IEM_MC_FETCH_MEM_U16_DISP(u16Sel, pIemCpu->iEffSeg, GCPtrEffSrc, 8);
            IEM_MC_CALL_CIMPL_3(iemCImpl_FarJmp, u16Sel, offSeg, enmEffOpSize);
            IEM_MC_END();
            return VINF_SUCCESS;

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/**
 * Opcode 0xff /6.
 * @param   bRm             The RM byte.
 */
FNIEMOP_DEF_1(iemOp_Grp5_push_Ev, uint8_t, bRm)
{
    IEMOP_MNEMONIC("push Ev");
    IEMOP_HLP_NO_LOCK_PREFIX(); /** @todo Too early? */

    /* Registers are handled by a common worker. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        return FNIEMOP_CALL_1(iemOpCommonPushGReg, (bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB);

    /* Memory we do here. */
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(uint16_t,  u16Src);
            IEM_MC_LOCAL(RTGCPTR,   GCPtrEffSrc);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm);
            IEM_MC_FETCH_MEM_U16(u16Src, pIemCpu->iEffSeg, GCPtrEffSrc);
            IEM_MC_PUSH_U16(u16Src);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(uint32_t,  u32Src);
            IEM_MC_LOCAL(RTGCPTR,   GCPtrEffSrc);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm);
            IEM_MC_FETCH_MEM_U32(u32Src, pIemCpu->iEffSeg, GCPtrEffSrc);
            IEM_MC_PUSH_U32(u32Src);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(uint64_t,  u64Src);
            IEM_MC_LOCAL(RTGCPTR,   GCPtrEffSrc);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm);
            IEM_MC_FETCH_MEM_U64(u64Src, pIemCpu->iEffSeg, GCPtrEffSrc);
            IEM_MC_PUSH_U64(u64Src);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;
    }
    AssertFailedReturn(VERR_NOT_IMPLEMENTED);
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
    AssertFailedReturn(VERR_INTERNAL_ERROR_2);
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
    /* 0x60 */  iemOp_pusha,            iemOp_popa,             iemOp_bound_Gv_Ma,      iemOp_arpl_Ew_Gw,
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
    /* 0x8c */  iemOp_mov_Ev_Sw,        iemOp_lea_Gv_M,         iemOp_mov_Sw_Ev,        iemOp_pop_Ev,
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
    /* 0xc4 */  iemOp_les_Gv_Mp,        iemOp_lds_Gv_Mp,        iemOp_Grp11_Eb_Ib,      iemOp_Grp11_Ev_Iz,
    /* 0xc8 */  iemOp_enter_Iw_Ib,      iemOp_leave,            iemOp_retf_Iw,          iemOp_retf,
    /* 0xcc */  iemOp_int_3,            iemOp_int_Ib,           iemOp_into,             iemOp_iret,
    /* 0xd0 */  iemOp_Grp2_Eb_1,        iemOp_Grp2_Ev_1,        iemOp_Grp2_Eb_CL,       iemOp_Grp2_Ev_CL,
    /* 0xd4 */  iemOp_aam_Ib,           iemOp_aad_Ib,           iemOp_Invalid,          iemOp_xlat,
    /* 0xd8 */  iemOp_EscF0,            iemOp_EscF1,            iemOp_EscF2,            iemOp_EscF3,
    /* 0xdc */  iemOp_EscF4,            iemOp_EscF5,            iemOp_EscF6,            iemOp_EscF7,
    /* 0xe0 */  iemOp_loopne_Jb,        iemOp_loope_Jb,         iemOp_loop_Jb,          iemOp_jecxz_Jb,
    /* 0xe4 */  iemOp_in_AL_Ib,         iemOp_in_eAX_Ib,        iemOp_out_Ib_AL,        iemOp_out_Ib_eAX,
    /* 0xe8 */  iemOp_call_Jv,          iemOp_jmp_Jv,           iemOp_jmp_Ap,           iemOp_jmp_Jb,
    /* 0xec */  iemOp_in_AL_DX,         iemOp_eAX_DX,           iemOp_out_DX_AL,        iemOp_out_DX_eAX,
    /* 0xf0 */  iemOp_lock,             iemOp_Invalid,          iemOp_repne,            iemOp_repe, /** @todo 0xf1 is INT1 / ICEBP. */
    /* 0xf4 */  iemOp_hlt,              iemOp_cmc,              iemOp_Grp3_Eb,          iemOp_Grp3_Ev,
    /* 0xf8 */  iemOp_clc,              iemOp_stc,              iemOp_cli,              iemOp_sti,
    /* 0xfc */  iemOp_cld,              iemOp_std,              iemOp_Grp4,             iemOp_Grp5,
};


/** @} */

