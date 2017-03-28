/* $Id$ */
/** @file
 * IEM - Instruction Decoding and Emulation.
 */

/*
 * Copyright (C) 2011-2016 Oracle Corporation
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
    IEMOP_MNEMONIC(sldt, "sldt Rv/Mw");
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
    IEMOP_MNEMONIC(str, "str Rv/Mw");
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
    IEMOP_MNEMONIC(lldt, "lldt Ew");
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
    IEMOP_MNEMONIC(ltr, "ltr Ew");
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
    IEMOP_MNEMONIC(verr, "verr Ew");
    IEMOP_HLP_MIN_286();
    return FNIEMOP_CALL_2(iemOpCommonGrp6VerX, bRm, false);
}


/** Opcode 0x0f 0x00 /5. */
FNIEMOPRM_DEF(iemOp_Grp6_verw)
{
    IEMOP_MNEMONIC(verw, "verw Ew");
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
    IEMOP_MNEMONIC(sgdt, "sgdt Ms");
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
    IEMOP_MNEMONIC(sidt, "sidt Ms");
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
    IEMOP_MNEMONIC(monitor, "monitor");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX(); /** @todo Verify that monitor is allergic to lock prefixes. */
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_monitor, pVCpu->iem.s.iEffSeg);
}


/** Opcode 0x0f 0x01 /1. */
FNIEMOP_DEF(iemOp_Grp7_mwait)
{
    IEMOP_MNEMONIC(mwait, "mwait"); /** @todo Verify that mwait is allergic to lock prefixes. */
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_mwait);
}


/** Opcode 0x0f 0x01 /2. */
FNIEMOP_DEF_1(iemOp_Grp7_lgdt, uint8_t, bRm)
{
    IEMOP_MNEMONIC(lgdt, "lgdt");
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
    IEMOP_MNEMONIC(xgetbv, "xgetbv");
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
    IEMOP_MNEMONIC(xsetbv, "xsetbv");
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
    IEMOP_MNEMONIC(lidt, "lidt");
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


#ifdef VBOX_WITH_NESTED_HWVIRT
/** Opcode 0x0f 0x01 0xd8. */
FNIEMOP_DEF(iemOp_Grp7_Amd_vmrun)
{
    IEMOP_MNEMONIC(vmrun, "vmrun");
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_vmrun);
}

/** Opcode 0x0f 0x01 0xd9. */
FNIEMOP_DEF(iemOp_Grp7_Amd_vmmcall)
{
    IEMOP_MNEMONIC(vmmcall, "vmmcall");
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_vmmcall);
}


/** Opcode 0x0f 0x01 0xda. */
FNIEMOP_DEF(iemOp_Grp7_Amd_vmload)
{
    IEMOP_MNEMONIC(vmload, "vmload");
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_vmload);
}


/** Opcode 0x0f 0x01 0xdb. */
FNIEMOP_DEF(iemOp_Grp7_Amd_vmsave)
{
    IEMOP_MNEMONIC(vmsave, "vmsave");
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_vmsave);
}


/** Opcode 0x0f 0x01 0xdc. */
FNIEMOP_DEF(iemOp_Grp7_Amd_stgi)
{
    IEMOP_MNEMONIC(stgi, "stgi");
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_stgi);
}


/** Opcode 0x0f 0x01 0xdd. */
FNIEMOP_DEF(iemOp_Grp7_Amd_clgi)
{
    IEMOP_MNEMONIC(clgi, "clgi");
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_clgi);
}


/** Opcode 0x0f 0x01 0xdf. */
FNIEMOP_DEF(iemOp_Grp7_Amd_invlpga)
{
    IEMOP_MNEMONIC(invlpga, "invlpga");
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_invlpga);
}
#else
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

/** Opcode 0x0f 0x01 0xdf. */
FNIEMOP_UD_STUB(iemOp_Grp7_Amd_invlpga);
#endif /* VBOX_WITH_NESTED_HWVIRT */

/** Opcode 0x0f 0x01 0xde. */
FNIEMOP_UD_STUB(iemOp_Grp7_Amd_skinit);

/** Opcode 0x0f 0x01 /4. */
FNIEMOP_DEF_1(iemOp_Grp7_smsw, uint8_t, bRm)
{
    IEMOP_MNEMONIC(smsw, "smsw");
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
    IEMOP_MNEMONIC(lmsw, "lmsw");
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
    IEMOP_MNEMONIC(invlpg, "invlpg");
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
    IEMOP_MNEMONIC(swapgs, "swapgs");
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


/**
 * Group 7 jump table, memory variant.
 */
IEM_STATIC const PFNIEMOPRM g_apfnGroup7Mem[8] =
{
    iemOp_Grp7_sgdt,
    iemOp_Grp7_sidt,
    iemOp_Grp7_lgdt,
    iemOp_Grp7_lidt,
    iemOp_Grp7_smsw,
    iemOp_InvalidWithRM,
    iemOp_Grp7_lmsw,
    iemOp_Grp7_invlpg
};


/** Opcode 0x0f 0x01. */
FNIEMOP_DEF(iemOp_Grp7)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
        return FNIEMOP_CALL_1(g_apfnGroup7Mem[(bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK], bRm);

    switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
    {
        case 0:
            switch (bRm & X86_MODRM_RM_MASK)
            {
                case 1: return FNIEMOP_CALL(iemOp_Grp7_vmcall);
                case 2: return FNIEMOP_CALL(iemOp_Grp7_vmlaunch);
                case 3: return FNIEMOP_CALL(iemOp_Grp7_vmresume);
                case 4: return FNIEMOP_CALL(iemOp_Grp7_vmxoff);
            }
            return IEMOP_RAISE_INVALID_OPCODE();

        case 1:
            switch (bRm & X86_MODRM_RM_MASK)
            {
                case 0: return FNIEMOP_CALL(iemOp_Grp7_monitor);
                case 1: return FNIEMOP_CALL(iemOp_Grp7_mwait);
            }
            return IEMOP_RAISE_INVALID_OPCODE();

        case 2:
            switch (bRm & X86_MODRM_RM_MASK)
            {
                case 0: return FNIEMOP_CALL(iemOp_Grp7_xgetbv);
                case 1: return FNIEMOP_CALL(iemOp_Grp7_xsetbv);
            }
            return IEMOP_RAISE_INVALID_OPCODE();

        case 3:
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
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *,  pu16Dst,           0);
                IEM_MC_ARG(uint16_t,    u16Sel,            1);
                IEM_MC_ARG_CONST(bool,  fIsLarArg, fIsLar, 2);

                IEM_MC_REF_GREG_U16(pu16Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_FETCH_GREG_U16(u16Sel, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_CALL_CIMPL_3(iemCImpl_LarLsl_u16, pu16Dst, u16Sel, fIsLarArg);

                IEM_MC_END();
                return VINF_SUCCESS;
            }

            case IEMMODE_32BIT:
            case IEMMODE_64BIT:
            {
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint64_t *,  pu64Dst,           0);
                IEM_MC_ARG(uint16_t,    u16Sel,            1);
                IEM_MC_ARG_CONST(bool,  fIsLarArg, fIsLar, 2);

                IEM_MC_REF_GREG_U64(pu64Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_FETCH_GREG_U16(u16Sel, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_CALL_CIMPL_3(iemCImpl_LarLsl_u64, pu64Dst, u16Sel, fIsLarArg);

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
                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint16_t *,  pu16Dst,           0);
                IEM_MC_ARG(uint16_t,    u16Sel,            1);
                IEM_MC_ARG_CONST(bool,  fIsLarArg, fIsLar, 2);
                IEM_MC_LOCAL(RTGCPTR,   GCPtrEffSrc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DECODED_NL_2(fIsLar ? OP_LAR : OP_LSL, IEMOPFORM_RM_MEM, OP_PARM_Gv, OP_PARM_Ew, DISOPTYPE_DANGEROUS | DISOPTYPE_PRIVILEGED_NOTRAP);

                IEM_MC_FETCH_MEM_U16(u16Sel, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
                IEM_MC_REF_GREG_U16(pu16Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_CALL_CIMPL_3(iemCImpl_LarLsl_u16, pu16Dst, u16Sel, fIsLarArg);

                IEM_MC_END();
                return VINF_SUCCESS;
            }

            case IEMMODE_32BIT:
            case IEMMODE_64BIT:
            {
                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint64_t *,  pu64Dst,           0);
                IEM_MC_ARG(uint16_t,    u16Sel,            1);
                IEM_MC_ARG_CONST(bool,  fIsLarArg, fIsLar, 2);
                IEM_MC_LOCAL(RTGCPTR,   GCPtrEffSrc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DECODED_NL_2(fIsLar ? OP_LAR : OP_LSL, IEMOPFORM_RM_MEM, OP_PARM_Gv, OP_PARM_Ew, DISOPTYPE_DANGEROUS | DISOPTYPE_PRIVILEGED_NOTRAP);
/** @todo testcase: make sure it's a 16-bit read. */

                IEM_MC_FETCH_MEM_U16(u16Sel, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
                IEM_MC_REF_GREG_U64(pu64Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_CALL_CIMPL_3(iemCImpl_LarLsl_u64, pu64Dst, u16Sel, fIsLarArg);

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
    IEMOP_MNEMONIC(lar, "lar Gv,Ew");
    return FNIEMOP_CALL_1(iemOpCommonLarLsl_Gv_Ew, true);
}


/** Opcode 0x0f 0x03. */
FNIEMOP_DEF(iemOp_lsl_Gv_Ew)
{
    IEMOP_MNEMONIC(lsl, "lsl Gv,Ew");
    return FNIEMOP_CALL_1(iemOpCommonLarLsl_Gv_Ew, false);
}


/** Opcode 0x0f 0x05. */
FNIEMOP_DEF(iemOp_syscall)
{
    IEMOP_MNEMONIC(syscall, "syscall"); /** @todo 286 LOADALL   */
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_syscall);
}


/** Opcode 0x0f 0x06. */
FNIEMOP_DEF(iemOp_clts)
{
    IEMOP_MNEMONIC(clts, "clts");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_clts);
}


/** Opcode 0x0f 0x07. */
FNIEMOP_DEF(iemOp_sysret)
{
    IEMOP_MNEMONIC(sysret, "sysret");  /** @todo 386 LOADALL   */
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_sysret);
}


/** Opcode 0x0f 0x08. */
FNIEMOP_STUB(iemOp_invd);
// IEMOP_HLP_MIN_486();


/** Opcode 0x0f 0x09. */
FNIEMOP_DEF(iemOp_wbinvd)
{
    IEMOP_MNEMONIC(wbinvd, "wbinvd");
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
    IEMOP_MNEMONIC(ud2, "ud2");
    return IEMOP_RAISE_INVALID_OPCODE();
}

/** Opcode 0x0f 0x0d. */
FNIEMOP_DEF(iemOp_nop_Ev_GrpP)
{
    /* AMD prefetch group, Intel implements this as NOP Ev (and so do we). */
    if (!IEM_GET_GUEST_CPU_FEATURES(pVCpu)->f3DNowPrefetch)
    {
        IEMOP_MNEMONIC(GrpPNotSupported, "GrpP");
        return IEMOP_RAISE_INVALID_OPCODE();
    }

    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_MNEMONIC(GrpPInvalid, "GrpP");
        return IEMOP_RAISE_INVALID_OPCODE();
    }

    switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
    {
        case 2: /* Aliased to /0 for the time being. */
        case 4: /* Aliased to /0 for the time being. */
        case 5: /* Aliased to /0 for the time being. */
        case 6: /* Aliased to /0 for the time being. */
        case 7: /* Aliased to /0 for the time being. */
        case 0: IEMOP_MNEMONIC(prefetch, "prefetch"); break;
        case 1: IEMOP_MNEMONIC(prefetchw_1, "prefetchw"); break;
        case 3: IEMOP_MNEMONIC(prefetchw_3, "prefetchw"); break;
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
        IEMOP_MNEMONIC(Inv3Dnow, "3Dnow");
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


/** Opcode      0x0f 0x10 - vmovups Vps, Wps */
FNIEMOP_STUB(iemOp_vmovups_Vps_Wps);
/** Opcode 0x66 0x0f 0x10 - vmovupd Vpd, Wpd */
FNIEMOP_STUB(iemOp_vmovupd_Vpd_Wpd);
/** Opcode 0xf3 0x0f 0x10 - vmovss Vx, Hx, Wss */
FNIEMOP_STUB(iemOp_vmovss_Vx_Hx_Wss);
/** Opcode 0xf2 0x0f 0x10 - vmovsd Vx, Hx, Wsd */
FNIEMOP_STUB(iemOp_vmovsd_Vx_Hx_Wsd);


/**
 * @opcode      0x11
 * @oppfx       none
 * @opcpuid     sse
 * @opgroup     og_sse_simdfp_datamove
 * @opxcpttype  4UA
 * @optest      op1=1 op2=2 -> op1=2
 * @optest      op1=0 op2=-42 -> op1=-42
 */
FNIEMOP_DEF(iemOp_vmovups_Wps_Vps)
{
    IEMOP_MNEMONIC2(MR, MOVUPS, movups, Wps, Vps, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE);
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /*
         * Register, register.
         */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
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
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT();
        IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();

        IEM_MC_FETCH_XREG_U128(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_STORE_MEM_U128(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/**
 * @opcode      0x11
 * @oppfx       0x66
 * @opcpuid     sse2
 * @opgroup     og_sse2_pcksclr_datamove
 * @opxcpttype  4UA
 * @optest      op1=1 op2=2 -> op1=2
 * @optest      op1=0 op2=-42 -> op1=-42
 */
FNIEMOP_DEF(iemOp_vmovupd_Wpd_Vpd)
{
    IEMOP_MNEMONIC2(MR, MOVUPD, movupd, Wpd, Vpd, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE);
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
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
         * Memory, register.
         */
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint128_t,                 uSrc); /** @todo optimize this one day... */
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
        IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();

        IEM_MC_FETCH_XREG_U128(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_STORE_MEM_U128(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/**
 * @opcode      0x11
 * @oppfx       0xf3
 * @opcpuid     sse
 * @opgroup     og_sse_simdfp_datamove
 * @opxcpttype  5
 * @optest      op1=1 op2=2 -> op1=2
 * @optest      op1=0 op2=-22 -> op1=-22
 */
FNIEMOP_DEF(iemOp_vmovss_Wss_Hx_Vss)
{
    IEMOP_MNEMONIC2(MR, MOVSS, movss, Wss, Vss, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE);
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /*
         * Register, register.
         */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(uint32_t,                  uSrc);

        IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT();
        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
        IEM_MC_FETCH_XREG_U32(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_STORE_XREG_U32((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /*
         * Memory, register.
         */
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint32_t,                  uSrc);
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT();
        IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();

        IEM_MC_FETCH_XREG_U32(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_STORE_MEM_U32(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/**
 * @opcode      0x11
 * @oppfx       0xf2
 * @opcpuid     sse2
 * @opgroup     og_sse2_pcksclr_datamove
 * @opxcpttype  5
 * @optest      op1=1 op2=2 -> op1=2
 * @optest      op1=0 op2=-42 -> op1=-42
 */
FNIEMOP_DEF(iemOp_vmovsd_Wsd_Hx_Vsd)
{
    IEMOP_MNEMONIC2(MR, MOVSD, movsd, Wsd, Vsd, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE);
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
    return VINF_SUCCESS;
}


FNIEMOP_DEF(iemOp_vmovlps_Vq_Hq_Mq__vmovhlps)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /**
         * @opcode      0x12
         * @opcodesub   11 mr/reg
         * @oppfx       none
         * @opcpuid     sse
         * @opgroup     og_sse_simdfp_datamove
         * @opxcpttype  5
         * @optest      op1=1 op2=2 -> op1=2
         * @optest      op1=0 op2=-42 -> op1=-42
         */
        IEMOP_MNEMONIC2(RM_REG, MOVHLPS, movhlps, Vq, UqHi, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE);

        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(uint64_t,                  uSrc);

        IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT();
        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
        IEM_MC_FETCH_XREG_HI_U64(uSrc, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_STORE_XREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /**
         * @opdone
         * @opcode      0x12
         * @opcodesub   !11 mr/reg
         * @oppfx       none
         * @opcpuid     sse
         * @opgroup     og_sse_simdfp_datamove
         * @opxcpttype  5
         * @optest      op1=1 op2=2 -> op1=2
         * @optest      op1=0 op2=-42 -> op1=-42
         * @opfunction  iemOp_vmovlps_Vq_Hq_Mq__vmovhlps
         */
        IEMOP_MNEMONIC2(RM_MEM, MOVLPS, movlps, Vq, Mq, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE);

        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint64_t,                  uSrc);
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT();
        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();

        IEM_MC_FETCH_MEM_U64(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
        IEM_MC_STORE_XREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/**
 * @opcode      0x12
 * @opcodesub   !11 mr/reg
 * @oppfx       0x66
 * @opcpuid     sse2
 * @opgroup     og_sse2_pcksclr_datamove
 * @opxcpttype  5
 * @optest      op1=1 op2=2 -> op1=2
 * @optest      op1=0 op2=-42 -> op1=-42
 */
FNIEMOP_DEF(iemOp_vmovlpd_Vq_Hq_Mq)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_MNEMONIC2(RM_MEM, MOVLPD, movlpd, Vq, Mq, DISOPTYPE_HARMLESS, IEMOPHINT_IGNORES_OP_SIZE);

        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint64_t,                  uSrc);
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();

        IEM_MC_FETCH_MEM_U64(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
        IEM_MC_STORE_XREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
        return VINF_SUCCESS;
    }

    /**
     * @opdone
     * @opmnemonic  ud660f12m3
     * @opcode      0x12
     * @opcodesub   11 mr/reg
     * @oppfx       0x66
     * @opunused    immediate
     * @opcpuid     sse
     * @optest      ->
     */
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Opcode 0xf3 0x0f 0x12. */
FNIEMOP_STUB(iemOp_vmovsldup_Vx_Wx); //NEXT

/** Opcode 0xf2 0x0f 0x12. */
FNIEMOP_STUB(iemOp_vmovddup_Vx_Wx);  //NEXT

/** Opcode      0x0f 0x13 - vmovlps Mq, Vq */
FNIEMOP_STUB(iemOp_vmovlps_Mq_Vq);

/** Opcode 0x66 0x0f 0x13 - vmovlpd Mq, Vq */
FNIEMOP_DEF(iemOp_vmovlpd_Mq_Vq)
{
    IEMOP_MNEMONIC(movlpd_Mq_Vq, "movlpd Mq,Vq");
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
#if 0
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
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
        IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();

        IEM_MC_FETCH_XREG_U64(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_STORE_MEM_U64(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}

/*  Opcode 0xf3 0x0f 0x13 - invalid */
/*  Opcode 0xf2 0x0f 0x13 - invalid */

/** Opcode      0x0f 0x14 - vunpcklps Vx, Hx, Wx*/
FNIEMOP_STUB(iemOp_vunpcklps_Vx_Hx_Wx);
/** Opcode 0x66 0x0f 0x14 - vunpcklpd Vx,Hx,Wx   */
FNIEMOP_STUB(iemOp_vunpcklpd_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0x14 - invalid */
/*  Opcode 0xf2 0x0f 0x14 - invalid */
/** Opcode      0x0f 0x15 - vunpckhps Vx, Hx, Wx   */
FNIEMOP_STUB(iemOp_vunpckhps_Vx_Hx_Wx);
/** Opcode 0x66 0x0f 0x15 - vunpckhpd Vx,Hx,Wx   */
FNIEMOP_STUB(iemOp_vunpckhpd_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0x15 - invalid */
/*  Opcode 0xf2 0x0f 0x15 - invalid */
/** Opcode      0x0f 0x16 - vmovhpsv1 Vdq, Hq, Mq vmovlhps Vdq, Hq, Uq   */
FNIEMOP_STUB(iemOp_vmovhpsv1_Vdq_Hq_Mq__vmovlhps_Vdq_Hq_Uq);  //NEXT
/** Opcode 0x66 0x0f 0x16 - vmovhpdv1 Vdq, Hq, Mq   */
FNIEMOP_STUB(iemOp_vmovhpdv1_Vdq_Hq_Mq);  //NEXT
/** Opcode 0xf3 0x0f 0x16 - vmovshdup Vx, Wx   */
FNIEMOP_STUB(iemOp_vmovshdup_Vx_Wx); //NEXT
/*  Opcode 0xf2 0x0f 0x16 - invalid */
/** Opcode      0x0f 0x17 - vmovhpsv1 Mq, Vq   */
FNIEMOP_STUB(iemOp_vmovhpsv1_Mq_Vq);  //NEXT
/** Opcode 0x66 0x0f 0x17 - vmovhpdv1 Mq, Vq   */
FNIEMOP_STUB(iemOp_vmovhpdv1_Mq_Vq);  //NEXT
/*  Opcode 0xf3 0x0f 0x17 - invalid */
/*  Opcode 0xf2 0x0f 0x17 - invalid */


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
            case 0: IEMOP_MNEMONIC(prefetchNTA, "prefetchNTA m8"); break;
            case 1: IEMOP_MNEMONIC(prefetchT0, "prefetchT0  m8"); break;
            case 2: IEMOP_MNEMONIC(prefetchT1, "prefetchT1  m8"); break;
            case 3: IEMOP_MNEMONIC(prefetchT2, "prefetchT2  m8"); break;
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
    IEMOP_MNEMONIC(nop_Ev, "nop Ev");
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
    IEMOP_MNEMONIC(mov_Rd_Cd, "mov Rd,Cd");
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
    IEMOP_MNEMONIC(mov_Rd_Dd, "mov Rd,Dd");
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
    IEMOP_MNEMONIC(mov_Cd_Rd, "mov Cd,Rd");
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
    IEMOP_MNEMONIC(mov_Dd_Rd, "mov Dd,Rd");
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
    IEMOP_MNEMONIC(mov_Rd_Td, "mov Rd,Td");
    /** @todo works on 386 and 486. */
    /* The RM byte is not considered, see testcase. */
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Opcode 0x0f 0x26. */
FNIEMOP_DEF(iemOp_mov_Td_Rd)
{
    IEMOP_MNEMONIC(mov_Td_Rd, "mov Td,Rd");
    /** @todo works on 386 and 486. */
    /* The RM byte is not considered, see testcase. */
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Opcode      0x0f 0x28 - vmovaps Vps, Wps */
FNIEMOP_DEF(iemOp_vmovaps_Vps_Wps)
{
    IEMOP_MNEMONIC(movaps_r_mr, "movaps r,mr");
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /*
         * Register, register.
         */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT();
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
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT();
        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();

        IEM_MC_FETCH_MEM_U128_ALIGN_SSE(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
        IEM_MC_STORE_XREG_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}

/** Opcode 0x66 0x0f 0x28 - vmovapd Vpd, Wpd */
FNIEMOP_DEF(iemOp_vmovapd_Vpd_Wpd)
{
    IEMOP_MNEMONIC(movapd_r_mr, "movapd r,mr");
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
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
        IEM_MC_LOCAL(uint128_t,                 uSrc); /** @todo optimize this one day... */
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();

        IEM_MC_FETCH_MEM_U128_ALIGN_SSE(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
        IEM_MC_STORE_XREG_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}

/*  Opcode 0xf3 0x0f 0x28 - invalid */
/*  Opcode 0xf2 0x0f 0x28 - invalid */

/** Opcode      0x0f 0x29 - vmovaps Wps, Vps */
FNIEMOP_DEF(iemOp_vmovaps_Wps_Vps)
{
    IEMOP_MNEMONIC(movaps_mr_r, "movaps Wps,Vps");
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /*
         * Register, register.
         */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
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
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT();
        IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();

        IEM_MC_FETCH_XREG_U128(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_STORE_MEM_U128_ALIGN_SSE(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}

/** Opcode 0x66 0x0f 0x29 - vmovapd Wpd,Vpd */
FNIEMOP_DEF(iemOp_vmovapd_Wpd_Vpd)
{
    IEMOP_MNEMONIC(movapd_mr_r, "movapd Wpd,Vpd");
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
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
         * Memory, register.
         */
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint128_t,                 uSrc); /** @todo optimize this one day... */
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
        IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();

        IEM_MC_FETCH_XREG_U128(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_STORE_MEM_U128_ALIGN_SSE(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}

/*  Opcode 0xf3 0x0f 0x29 - invalid */
/*  Opcode 0xf2 0x0f 0x29 - invalid */


/** Opcode      0x0f 0x2a - cvtpi2ps Vps, Qpi */
FNIEMOP_STUB(iemOp_cvtpi2ps_Vps_Qpi); //NEXT
/** Opcode 0x66 0x0f 0x2a - cvtpi2pd Vpd, Qpi */
FNIEMOP_STUB(iemOp_cvtpi2pd_Vpd_Qpi); //NEXT
/** Opcode 0xf3 0x0f 0x2a - vcvtsi2ss Vss, Hss, Ey */
FNIEMOP_STUB(iemOp_vcvtsi2ss_Vss_Hss_Ey); //NEXT
/** Opcode 0xf2 0x0f 0x2a - vcvtsi2sd Vsd, Hsd, Ey */
FNIEMOP_STUB(iemOp_vcvtsi2sd_Vsd_Hsd_Ey); //NEXT


/** Opcode      0x0f 0x2b - vmovntps Mps, Vps */
FNIEMOP_DEF(iemOp_vmovntps_Mps_Vps)
{
    IEMOP_MNEMONIC(movntps_mr_r, "movntps Mps,Vps");
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
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT();
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

/** Opcode 0x66 0x0f 0x2b - vmovntpd Mpd, Vpd */
FNIEMOP_DEF(iemOp_vmovntpd_Mpd_Vpd)
{
    IEMOP_MNEMONIC(movntpd_mr_r, "movntpd Mdq,Vpd");
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
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
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
/*  Opcode 0xf3 0x0f 0x2b - invalid */
/*  Opcode 0xf2 0x0f 0x2b - invalid */


/** Opcode      0x0f 0x2c - cvttps2pi Ppi, Wps */
FNIEMOP_STUB(iemOp_cvttps2pi_Ppi_Wps);
/** Opcode 0x66 0x0f 0x2c - cvttpd2pi Ppi, Wpd */
FNIEMOP_STUB(iemOp_cvttpd2pi_Ppi_Wpd);
/** Opcode 0xf3 0x0f 0x2c - vcvttss2si Gy, Wss */
FNIEMOP_STUB(iemOp_vcvttss2si_Gy_Wss);
/** Opcode 0xf2 0x0f 0x2c - vcvttsd2si Gy, Wsd */
FNIEMOP_STUB(iemOp_vcvttsd2si_Gy_Wsd);

/** Opcode      0x0f 0x2d - cvtps2pi Ppi, Wps */
FNIEMOP_STUB(iemOp_cvtps2pi_Ppi_Wps);
/** Opcode 0x66 0x0f 0x2d - cvtpd2pi Qpi, Wpd */
FNIEMOP_STUB(iemOp_cvtpd2pi_Qpi_Wpd);
/** Opcode 0xf3 0x0f 0x2d - vcvtss2si Gy, Wss */
FNIEMOP_STUB(iemOp_vcvtss2si_Gy_Wss);
/** Opcode 0xf2 0x0f 0x2d - vcvtsd2si Gy, Wsd */
FNIEMOP_STUB(iemOp_vcvtsd2si_Gy_Wsd);

/** Opcode      0x0f 0x2e - vucomiss Vss, Wss */
FNIEMOP_STUB(iemOp_vucomiss_Vss_Wss); // NEXT
/** Opcode 0x66 0x0f 0x2e - vucomisd Vsd, Wsd */
FNIEMOP_STUB(iemOp_vucomisd_Vsd_Wsd); // NEXT
/*  Opcode 0xf3 0x0f 0x2e - invalid */
/*  Opcode 0xf2 0x0f 0x2e - invalid */

/** Opcode      0x0f 0x2f - vcomiss Vss, Wss */
FNIEMOP_STUB(iemOp_vcomiss_Vss_Wss);
/** Opcode 0x66 0x0f 0x2f - vcomisd Vsd, Wsd */
FNIEMOP_STUB(iemOp_vcomisd_Vsd_Wsd);
/*  Opcode 0xf3 0x0f 0x2f - invalid */
/*  Opcode 0xf2 0x0f 0x2f - invalid */

/** Opcode 0x0f 0x30. */
FNIEMOP_DEF(iemOp_wrmsr)
{
    IEMOP_MNEMONIC(wrmsr, "wrmsr");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_wrmsr);
}


/** Opcode 0x0f 0x31. */
FNIEMOP_DEF(iemOp_rdtsc)
{
    IEMOP_MNEMONIC(rdtsc, "rdtsc");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_rdtsc);
}


/** Opcode 0x0f 0x33. */
FNIEMOP_DEF(iemOp_rdmsr)
{
    IEMOP_MNEMONIC(rdmsr, "rdmsr");
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
    IEMOP_MNEMONIC(cmovo_Gv_Ev, "cmovo Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_SET(X86_EFL_OF));
}


/** Opcode 0x0f 0x41. */
FNIEMOP_DEF(iemOp_cmovno_Gv_Ev)
{
    IEMOP_MNEMONIC(cmovno_Gv_Ev, "cmovno Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_NOT_SET(X86_EFL_OF));
}


/** Opcode 0x0f 0x42. */
FNIEMOP_DEF(iemOp_cmovc_Gv_Ev)
{
    IEMOP_MNEMONIC(cmovc_Gv_Ev, "cmovc Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_SET(X86_EFL_CF));
}


/** Opcode 0x0f 0x43. */
FNIEMOP_DEF(iemOp_cmovnc_Gv_Ev)
{
    IEMOP_MNEMONIC(cmovnc_Gv_Ev, "cmovnc Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_NOT_SET(X86_EFL_CF));
}


/** Opcode 0x0f 0x44. */
FNIEMOP_DEF(iemOp_cmove_Gv_Ev)
{
    IEMOP_MNEMONIC(cmove_Gv_Ev, "cmove Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_SET(X86_EFL_ZF));
}


/** Opcode 0x0f 0x45. */
FNIEMOP_DEF(iemOp_cmovne_Gv_Ev)
{
    IEMOP_MNEMONIC(cmovne_Gv_Ev, "cmovne Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_NOT_SET(X86_EFL_ZF));
}


/** Opcode 0x0f 0x46. */
FNIEMOP_DEF(iemOp_cmovbe_Gv_Ev)
{
    IEMOP_MNEMONIC(cmovbe_Gv_Ev, "cmovbe Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_ANY_BITS_SET(X86_EFL_CF | X86_EFL_ZF));
}


/** Opcode 0x0f 0x47. */
FNIEMOP_DEF(iemOp_cmovnbe_Gv_Ev)
{
    IEMOP_MNEMONIC(cmovnbe_Gv_Ev, "cmovnbe Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_NO_BITS_SET(X86_EFL_CF | X86_EFL_ZF));
}


/** Opcode 0x0f 0x48. */
FNIEMOP_DEF(iemOp_cmovs_Gv_Ev)
{
    IEMOP_MNEMONIC(cmovs_Gv_Ev, "cmovs Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_SET(X86_EFL_SF));
}


/** Opcode 0x0f 0x49. */
FNIEMOP_DEF(iemOp_cmovns_Gv_Ev)
{
    IEMOP_MNEMONIC(cmovns_Gv_Ev, "cmovns Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_NOT_SET(X86_EFL_SF));
}


/** Opcode 0x0f 0x4a. */
FNIEMOP_DEF(iemOp_cmovp_Gv_Ev)
{
    IEMOP_MNEMONIC(cmovp_Gv_Ev, "cmovp Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_SET(X86_EFL_PF));
}


/** Opcode 0x0f 0x4b. */
FNIEMOP_DEF(iemOp_cmovnp_Gv_Ev)
{
    IEMOP_MNEMONIC(cmovnp_Gv_Ev, "cmovnp Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_NOT_SET(X86_EFL_PF));
}


/** Opcode 0x0f 0x4c. */
FNIEMOP_DEF(iemOp_cmovl_Gv_Ev)
{
    IEMOP_MNEMONIC(cmovl_Gv_Ev, "cmovl Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BITS_NE(X86_EFL_SF, X86_EFL_OF));
}


/** Opcode 0x0f 0x4d. */
FNIEMOP_DEF(iemOp_cmovnl_Gv_Ev)
{
    IEMOP_MNEMONIC(cmovnl_Gv_Ev, "cmovnl Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BITS_EQ(X86_EFL_SF, X86_EFL_OF));
}


/** Opcode 0x0f 0x4e. */
FNIEMOP_DEF(iemOp_cmovle_Gv_Ev)
{
    IEMOP_MNEMONIC(cmovle_Gv_Ev, "cmovle Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_SET_OR_BITS_NE(X86_EFL_ZF, X86_EFL_SF, X86_EFL_OF));
}


/** Opcode 0x0f 0x4f. */
FNIEMOP_DEF(iemOp_cmovnle_Gv_Ev)
{
    IEMOP_MNEMONIC(cmovnle_Gv_Ev, "cmovnle Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_NOT_SET_AND_BITS_EQ(X86_EFL_ZF, X86_EFL_SF, X86_EFL_OF));
}

#undef CMOV_X

/** Opcode      0x0f 0x50 - vmovmskps Gy, Ups */
FNIEMOP_STUB(iemOp_vmovmskps_Gy_Ups);
/** Opcode 0x66 0x0f 0x50 - vmovmskpd Gy,Upd */
FNIEMOP_STUB(iemOp_vmovmskpd_Gy_Upd);
/*  Opcode 0xf3 0x0f 0x50 - invalid */
/*  Opcode 0xf2 0x0f 0x50 - invalid */

/** Opcode      0x0f 0x51 - vsqrtps Vps, Wps */
FNIEMOP_STUB(iemOp_vsqrtps_Vps_Wps);
/** Opcode 0x66 0x0f 0x51 - vsqrtpd Vpd, Wpd */
FNIEMOP_STUB(iemOp_vsqrtpd_Vpd_Wpd);
/** Opcode 0xf3 0x0f 0x51 - vsqrtss Vss, Hss, Wss */
FNIEMOP_STUB(iemOp_vsqrtss_Vss_Hss_Wss);
/** Opcode 0xf2 0x0f 0x51 - vsqrtsd Vsd, Hsd, Wsd */
FNIEMOP_STUB(iemOp_vsqrtsd_Vsd_Hsd_Wsd);

/** Opcode      0x0f 0x52 - vrsqrtps Vps, Wps */
FNIEMOP_STUB(iemOp_vrsqrtps_Vps_Wps);
/*  Opcode 0x66 0x0f 0x52 - invalid */
/** Opcode 0xf3 0x0f 0x52 - vrsqrtss Vss, Hss, Wss */
FNIEMOP_STUB(iemOp_vrsqrtss_Vss_Hss_Wss);
/*  Opcode 0xf2 0x0f 0x52 - invalid */

/** Opcode      0x0f 0x53 - vrcpps Vps, Wps */
FNIEMOP_STUB(iemOp_vrcpps_Vps_Wps);
/*  Opcode 0x66 0x0f 0x53 - invalid */
/** Opcode 0xf3 0x0f 0x53 - vrcpss Vss, Hss, Wss */
FNIEMOP_STUB(iemOp_vrcpss_Vss_Hss_Wss);
/*  Opcode 0xf2 0x0f 0x53 - invalid */

/** Opcode      0x0f 0x54 - vandps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vandps_Vps_Hps_Wps);
/** Opcode 0x66 0x0f 0x54 - vandpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vandpd_Vpd_Hpd_Wpd);
/*  Opcode 0xf3 0x0f 0x54 - invalid */
/*  Opcode 0xf2 0x0f 0x54 - invalid */

/** Opcode      0x0f 0x55 - vandnps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vandnps_Vps_Hps_Wps);
/** Opcode 0x66 0x0f 0x55 - vandnpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vandnpd_Vpd_Hpd_Wpd);
/*  Opcode 0xf3 0x0f 0x55 - invalid */
/*  Opcode 0xf2 0x0f 0x55 - invalid */

/** Opcode      0x0f 0x56 - vorps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vorps_Vps_Hps_Wps);
/** Opcode 0x66 0x0f 0x56 - vorpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vorpd_Vpd_Hpd_Wpd);
/*  Opcode 0xf3 0x0f 0x56 - invalid */
/*  Opcode 0xf2 0x0f 0x56 - invalid */

/** Opcode      0x0f 0x57 - vxorps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vxorps_Vps_Hps_Wps);
/** Opcode 0x66 0x0f 0x57 - vxorpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vxorpd_Vpd_Hpd_Wpd);
/*  Opcode 0xf3 0x0f 0x57 - invalid */
/*  Opcode 0xf2 0x0f 0x57 - invalid */

/** Opcode      0x0f 0x58 - vaddps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vaddps_Vps_Hps_Wps);
/** Opcode 0x66 0x0f 0x58 - vaddpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vaddpd_Vpd_Hpd_Wpd);
/** Opcode 0xf3 0x0f 0x58 - vaddss Vss, Hss, Wss */
FNIEMOP_STUB(iemOp_vaddss_Vss_Hss_Wss);
/** Opcode 0xf2 0x0f 0x58 - vaddsd Vsd, Hsd, Wsd */
FNIEMOP_STUB(iemOp_vaddsd_Vsd_Hsd_Wsd);

/** Opcode      0x0f 0x59 - vmulps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vmulps_Vps_Hps_Wps);
/** Opcode 0x66 0x0f 0x59 - vmulpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vmulpd_Vpd_Hpd_Wpd);
/** Opcode 0xf3 0x0f 0x59 - vmulss Vss, Hss, Wss */
FNIEMOP_STUB(iemOp_vmulss_Vss_Hss_Wss);
/** Opcode 0xf2 0x0f 0x59 - vmulsd Vsd, Hsd, Wsd */
FNIEMOP_STUB(iemOp_vmulsd_Vsd_Hsd_Wsd);

/** Opcode      0x0f 0x5a - vcvtps2pd Vpd, Wps */
FNIEMOP_STUB(iemOp_vcvtps2pd_Vpd_Wps);
/** Opcode 0x66 0x0f 0x5a - vcvtpd2ps Vps, Wpd */
FNIEMOP_STUB(iemOp_vcvtpd2ps_Vps_Wpd);
/** Opcode 0xf3 0x0f 0x5a - vcvtss2sd Vsd, Hx, Wss */
FNIEMOP_STUB(iemOp_vcvtss2sd_Vsd_Hx_Wss);
/** Opcode 0xf2 0x0f 0x5a - vcvtsd2ss Vss, Hx, Wsd */
FNIEMOP_STUB(iemOp_vcvtsd2ss_Vss_Hx_Wsd);

/** Opcode      0x0f 0x5b - vcvtdq2ps Vps, Wdq */
FNIEMOP_STUB(iemOp_vcvtdq2ps_Vps_Wdq);
/** Opcode 0x66 0x0f 0x5b - vcvtps2dq Vdq, Wps */
FNIEMOP_STUB(iemOp_vcvtps2dq_Vdq_Wps);
/** Opcode 0xf3 0x0f 0x5b - vcvttps2dq Vdq, Wps */
FNIEMOP_STUB(iemOp_vcvttps2dq_Vdq_Wps);
/*  Opcode 0xf2 0x0f 0x5b - invalid */

/** Opcode      0x0f 0x5c - vsubps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vsubps_Vps_Hps_Wps);
/** Opcode 0x66 0x0f 0x5c - vsubpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vsubpd_Vpd_Hpd_Wpd);
/** Opcode 0xf3 0x0f 0x5c - vsubss Vss, Hss, Wss */
FNIEMOP_STUB(iemOp_vsubss_Vss_Hss_Wss);
/** Opcode 0xf2 0x0f 0x5c - vsubsd Vsd, Hsd, Wsd */
FNIEMOP_STUB(iemOp_vsubsd_Vsd_Hsd_Wsd);

/** Opcode      0x0f 0x5d - vminps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vminps_Vps_Hps_Wps);
/** Opcode 0x66 0x0f 0x5d - vminpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vminpd_Vpd_Hpd_Wpd);
/** Opcode 0xf3 0x0f 0x5d - vminss Vss, Hss, Wss */
FNIEMOP_STUB(iemOp_vminss_Vss_Hss_Wss);
/** Opcode 0xf2 0x0f 0x5d - vminsd Vsd, Hsd, Wsd */
FNIEMOP_STUB(iemOp_vminsd_Vsd_Hsd_Wsd);

/** Opcode      0x0f 0x5e - vdivps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vdivps_Vps_Hps_Wps);
/** Opcode 0x66 0x0f 0x5e - vdivpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vdivpd_Vpd_Hpd_Wpd);
/** Opcode 0xf3 0x0f 0x5e - vdivss Vss, Hss, Wss */
FNIEMOP_STUB(iemOp_vdivss_Vss_Hss_Wss);
/** Opcode 0xf2 0x0f 0x5e - vdivsd Vsd, Hsd, Wsd */
FNIEMOP_STUB(iemOp_vdivsd_Vsd_Hsd_Wsd);

/** Opcode      0x0f 0x5f - vmaxps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vmaxps_Vps_Hps_Wps);
/** Opcode 0x66 0x0f 0x5f - vmaxpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vmaxpd_Vpd_Hpd_Wpd);
/** Opcode 0xf3 0x0f 0x5f - vmaxss Vss, Hss, Wss */
FNIEMOP_STUB(iemOp_vmaxss_Vss_Hss_Wss);
/** Opcode 0xf2 0x0f 0x5f - vmaxsd Vsd, Hsd, Wsd */
FNIEMOP_STUB(iemOp_vmaxsd_Vsd_Hsd_Wsd);

/**
 * Common worker for MMX instructions on the forms:
 *      pxxxx mm1, mm2/mem32
 *
 * The 2nd operand is the first half of a register, which in the memory case
 * means a 32-bit memory access for MMX and 128-bit aligned 64-bit or 128-bit
 * memory accessed for MMX.
 *
 * Exceptions type 4.
 */
FNIEMOP_DEF_1(iemOpCommonMmx_LowLow_To_Full, PCIEMOPMEDIAF1L1, pImpl)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
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
}


/**
 * Common worker for SSE2 instructions on the forms:
 *      pxxxx xmm1, xmm2/mem128
 *
 * The 2nd operand is the first half of a register, which in the memory case
 * means a 32-bit memory access for MMX and 128-bit aligned 64-bit or 128-bit
 * memory accessed for MMX.
 *
 * Exceptions type 4.
 */
FNIEMOP_DEF_1(iemOpCommonSse_LowLow_To_Full, PCIEMOPMEDIAF1L1, pImpl)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
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
}


/** Opcode      0x0f 0x60 - punpcklbw Pq, Qd */
FNIEMOP_DEF(iemOp_punpcklbw_Pq_Qd)
{
    IEMOP_MNEMONIC(punpcklbw, "punpcklbw Pq, Qd");
    return FNIEMOP_CALL_1(iemOpCommonMmx_LowLow_To_Full, &g_iemAImpl_punpcklbw);
}

/** Opcode 0x66 0x0f 0x60 - vpunpcklbw Vx, Hx, W */
FNIEMOP_DEF(iemOp_vpunpcklbw_Vx_Hx_Wx)
{
    IEMOP_MNEMONIC(vpunpcklbw, "vpunpcklbw Vx, Hx, Wx");
    return FNIEMOP_CALL_1(iemOpCommonMmx_LowLow_To_Full, &g_iemAImpl_punpcklbw);
}

/*  Opcode 0xf3 0x0f 0x60 - invalid */


/** Opcode      0x0f 0x61 - punpcklwd Pq, Qd */
FNIEMOP_DEF(iemOp_punpcklwd_Pq_Qd)
{
    IEMOP_MNEMONIC(punpcklwd, "punpcklwd Pq, Qd"); /** @todo AMD mark the MMX version as 3DNow!. Intel says MMX CPUID req. */
    return FNIEMOP_CALL_1(iemOpCommonMmx_LowLow_To_Full, &g_iemAImpl_punpcklwd);
}

/** Opcode 0x66 0x0f 0x61 - vpunpcklwd Vx, Hx, Wx */
FNIEMOP_DEF(iemOp_vpunpcklwd_Vx_Hx_Wx)
{
    IEMOP_MNEMONIC(vpunpcklwd, "vpunpcklwd Vx, Hx, Wx");
    return FNIEMOP_CALL_1(iemOpCommonSse_LowLow_To_Full, &g_iemAImpl_punpcklwd);
}

/*  Opcode 0xf3 0x0f 0x61 - invalid */


/** Opcode      0x0f 0x62 - punpckldq Pq, Qd */
FNIEMOP_DEF(iemOp_punpckldq_Pq_Qd)
{
    IEMOP_MNEMONIC(punpckldq, "punpckldq Pq, Qd");
    return FNIEMOP_CALL_1(iemOpCommonMmx_LowLow_To_Full, &g_iemAImpl_punpckldq);
}

/** Opcode 0x66 0x0f 0x62 - vpunpckldq Vx, Hx, Wx */
FNIEMOP_DEF(iemOp_vpunpckldq_Vx_Hx_Wx)
{
    IEMOP_MNEMONIC(vpunpckldq, "vpunpckldq Vx, Hx, Wx");
    return FNIEMOP_CALL_1(iemOpCommonSse_LowLow_To_Full, &g_iemAImpl_punpckldq);
}

/*  Opcode 0xf3 0x0f 0x62 - invalid */



/** Opcode      0x0f 0x63 - packsswb Pq, Qq */
FNIEMOP_STUB(iemOp_packsswb_Pq_Qq);
/** Opcode 0x66 0x0f 0x63 - vpacksswb Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpacksswb_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0x63 - invalid */

/** Opcode      0x0f 0x64 - pcmpgtb Pq, Qq */
FNIEMOP_STUB(iemOp_pcmpgtb_Pq_Qq);
/** Opcode 0x66 0x0f 0x64 - vpcmpgtb Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpcmpgtb_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0x64 - invalid */

/** Opcode      0x0f 0x65 - pcmpgtw Pq, Qq */
FNIEMOP_STUB(iemOp_pcmpgtw_Pq_Qq);
/** Opcode 0x66 0x0f 0x65 - vpcmpgtw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpcmpgtw_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0x65 - invalid */

/** Opcode      0x0f 0x66 - pcmpgtd Pq, Qq */
FNIEMOP_STUB(iemOp_pcmpgtd_Pq_Qq);
/** Opcode 0x66 0x0f 0x66 - vpcmpgtd Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpcmpgtd_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0x66 - invalid */

/** Opcode      0x0f 0x67 - packuswb Pq, Qq */
FNIEMOP_STUB(iemOp_packuswb_Pq_Qq);
/** Opcode 0x66 0x0f 0x67 - vpackuswb Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpackuswb_Vx_Hx_W);
/*  Opcode 0xf3 0x0f 0x67 - invalid */


/**
 * Common worker for MMX instructions on the form:
 *      pxxxx mm1, mm2/mem64
 *
 * The 2nd operand is the second half of a register, which in the memory case
 * means a 64-bit memory access for MMX, and for SSE a 128-bit aligned access
 * where it may read the full 128 bits or only the upper 64 bits.
 *
 * Exceptions type 4.
 */
FNIEMOP_DEF_1(iemOpCommonMmx_HighHigh_To_Full, PCIEMOPMEDIAF1H1, pImpl)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    AssertReturn(pImpl->pfnU64, IEMOP_RAISE_INVALID_OPCODE());
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
}


/**
 * Common worker for SSE2 instructions on the form:
 *      pxxxx xmm1, xmm2/mem128
 *
 * The 2nd operand is the second half of a register, which in the memory case
 * means a 64-bit memory access for MMX, and for SSE a 128-bit aligned access
 * where it may read the full 128 bits or only the upper 64 bits.
 *
 * Exceptions type 4.
 */
FNIEMOP_DEF_1(iemOpCommonSse_HighHigh_To_Full, PCIEMOPMEDIAF1H1, pImpl)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
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
}


/** Opcode      0x0f 0x68 - punpckhbw Pq, Qd */
FNIEMOP_DEF(iemOp_punpckhbw_Pq_Qd)
{
    IEMOP_MNEMONIC(punpckhbw, "punpckhbw Pq, Qd");
    return FNIEMOP_CALL_1(iemOpCommonMmx_HighHigh_To_Full, &g_iemAImpl_punpckhbw);
}

/** Opcode 0x66 0x0f 0x68 - vpunpckhbw Vx, Hx, Wx */
FNIEMOP_DEF(iemOp_vpunpckhbw_Vx_Hx_Wx)
{
    IEMOP_MNEMONIC(vpunpckhbw, "vpunpckhbw Vx, Hx, Wx");
    return FNIEMOP_CALL_1(iemOpCommonSse_HighHigh_To_Full, &g_iemAImpl_punpckhbw);
}
/*  Opcode 0xf3 0x0f 0x68 - invalid */


/** Opcode      0x0f 0x69 - punpckhwd Pq, Qd */
FNIEMOP_DEF(iemOp_punpckhwd_Pq_Qd)
{
    IEMOP_MNEMONIC(punpckhwd, "punpckhwd Pq, Qd");
    return FNIEMOP_CALL_1(iemOpCommonMmx_HighHigh_To_Full, &g_iemAImpl_punpckhwd);
}

/** Opcode 0x66 0x0f 0x69 - vpunpckhwd Vx, Hx, Wx */
FNIEMOP_DEF(iemOp_vpunpckhwd_Vx_Hx_Wx)
{
    IEMOP_MNEMONIC(vpunpckhwd, "vpunpckhwd Vx, Hx, Wx");
    return FNIEMOP_CALL_1(iemOpCommonSse_HighHigh_To_Full, &g_iemAImpl_punpckhwd);

}
/*  Opcode 0xf3 0x0f 0x69 - invalid */


/** Opcode      0x0f 0x6a - punpckhdq Pq, Qd */
FNIEMOP_DEF(iemOp_punpckhdq_Pq_Qd)
{
    IEMOP_MNEMONIC(punpckhdq, "punpckhdq Pq, Qd");
    return FNIEMOP_CALL_1(iemOpCommonMmx_HighHigh_To_Full, &g_iemAImpl_punpckhdq);
}

/** Opcode 0x66 0x0f 0x6a - vpunpckhdq Vx, Hx, W */
FNIEMOP_DEF(iemOp_vpunpckhdq_Vx_Hx_W)
{
    IEMOP_MNEMONIC(vpunpckhdq, "vpunpckhdq Vx, Hx, W");
    return FNIEMOP_CALL_1(iemOpCommonSse_HighHigh_To_Full, &g_iemAImpl_punpckhdq);
}
/*  Opcode 0xf3 0x0f 0x6a - invalid */


/** Opcode      0x0f 0x6b - packssdw Pq, Qd */
FNIEMOP_STUB(iemOp_packssdw_Pq_Qd);
/** Opcode 0x66 0x0f 0x6b - vpackssdw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpackssdw_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0x6b - invalid */


/*  Opcode      0x0f 0x6c - invalid */

/** Opcode 0x66 0x0f 0x6c - vpunpcklqdq Vx, Hx, Wx */
FNIEMOP_DEF(iemOp_vpunpcklqdq_Vx_Hx_Wx)
{
    IEMOP_MNEMONIC(vpunpcklqdq, "vpunpcklqdq Vx, Hx, Wx");
    return FNIEMOP_CALL_1(iemOpCommonSse_LowLow_To_Full, &g_iemAImpl_punpcklqdq);
}

/*  Opcode 0xf3 0x0f 0x6c - invalid */
/*  Opcode 0xf2 0x0f 0x6c - invalid */


/*  Opcode      0x0f 0x6d - invalid */

/** Opcode 0x66 0x0f 0x6d - vpunpckhqdq Vx, Hx, W */
FNIEMOP_DEF(iemOp_vpunpckhqdq_Vx_Hx_W)
{
    IEMOP_MNEMONIC(punpckhqdq, "punpckhqdq");
    return FNIEMOP_CALL_1(iemOpCommonSse_HighHigh_To_Full, &g_iemAImpl_punpckhqdq);
}

/*  Opcode 0xf3 0x0f 0x6d - invalid */


/** Opcode      0x0f 0x6e - movd/q Pd, Ey */
FNIEMOP_DEF(iemOp_movd_q_Pd_Ey)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_REX_W)
        IEMOP_MNEMONIC(movq_Pq_Eq, "movq Pq,Eq");
    else
        IEMOP_MNEMONIC(movd_Pd_Ed, "movd Pd,Ed");
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
}

/** Opcode 0x66 0x0f 0x6e - vmovd/q Vy, Ey */
FNIEMOP_DEF(iemOp_vmovd_q_Vy_Ey)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_REX_W)
        IEMOP_MNEMONIC(vmovdq_Wq_Eq, "vmovq Wq,Eq");
    else
        IEMOP_MNEMONIC(vmovdq_Wd_Ed, "vmovd Wd,Ed");
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
}

/*  Opcode 0xf3 0x0f 0x6e - invalid */


/** Opcode      0x0f 0x6f - movq Pq, Qq */
FNIEMOP_DEF(iemOp_movq_Pq_Qq)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_MNEMONIC(movq_Pq_Qq, "movq Pq,Qq");
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
}

/** Opcode 0x66 0x0f 0x6f - vmovdqa Vx, Wx */
FNIEMOP_DEF(iemOp_vmovdqa_Vx_Wx)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_MNEMONIC(movdqa_Vdq_Wdq, "movdqa Vdq,Wdq");
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
        IEM_MC_FETCH_MEM_U128_ALIGN_SSE(u128Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
        IEM_MC_STORE_XREG_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u128Tmp);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}

/** Opcode 0xf3 0x0f 0x6f - vmovdqu Vx, Wx */
FNIEMOP_DEF(iemOp_vmovdqu_Vx_Wx)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_MNEMONIC(movdqu_Vdq_Wdq, "movdqu Vdq,Wdq");
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
        IEM_MC_FETCH_MEM_U128(u128Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
        IEM_MC_STORE_XREG_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u128Tmp);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode      0x0f 0x70 - pshufw Pq, Qq, Ib */
FNIEMOP_DEF(iemOp_pshufw_Pq_Qq_Ib)
{
    IEMOP_MNEMONIC(pshufw_Pq_Qq, "pshufw Pq,Qq,Ib");
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
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
}

/** Opcode 0x66 0x0f 0x70 - vpshufd Vx, Wx, Ib */
FNIEMOP_DEF(iemOp_vpshufd_Vx_Wx_Ib)
{
    IEMOP_MNEMONIC(vpshufd_Vx_Wx_Ib, "vpshufd Vx,Wx,Ib");
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
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
        IEM_MC_CALL_SSE_AIMPL_3(iemAImpl_pshufd, pDst, pSrc, bEvilArg);
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
        IEM_MC_CALL_SSE_AIMPL_3(iemAImpl_pshufd, pDst, pSrc, bEvilArg);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}

/** Opcode 0xf3 0x0f 0x70 - vpshufhw Vx, Wx, Ib */
FNIEMOP_DEF(iemOp_vpshufhw_Vx_Wx_Ib)
{
    IEMOP_MNEMONIC(vpshufhw_Vx_Wx_Ib, "vpshufhw Vx,Wx,Ib");
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
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
        IEM_MC_CALL_SSE_AIMPL_3(iemAImpl_pshufhw, pDst, pSrc, bEvilArg);
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
        IEM_MC_CALL_SSE_AIMPL_3(iemAImpl_pshufhw, pDst, pSrc, bEvilArg);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}

/** Opcode 0xf2 0x0f 0x70 - vpshuflw Vx, Wx, Ib */
FNIEMOP_DEF(iemOp_vpshuflw_Vx_Wx_Ib)
{
    IEMOP_MNEMONIC(vpshuflw_Vx_Wx_Ib, "vpshuflw Vx,Wx,Ib");
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
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
        IEM_MC_CALL_SSE_AIMPL_3(iemAImpl_pshuflw, pDst, pSrc, bEvilArg);
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
        IEM_MC_CALL_SSE_AIMPL_3(iemAImpl_pshuflw, pDst, pSrc, bEvilArg);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x71 11/2. */
FNIEMOP_STUB_1(iemOp_Grp12_psrlw_Nq_Ib,  uint8_t, bRm);

/** Opcode 0x66 0x0f 0x71 11/2. */
FNIEMOP_STUB_1(iemOp_Grp12_vpsrlw_Hx_Ux_Ib, uint8_t, bRm);

/** Opcode 0x0f 0x71 11/4. */
FNIEMOP_STUB_1(iemOp_Grp12_psraw_Nq_Ib,  uint8_t, bRm);

/** Opcode 0x66 0x0f 0x71 11/4. */
FNIEMOP_STUB_1(iemOp_Grp12_vpsraw_Hx_Ux_Ib, uint8_t, bRm);

/** Opcode 0x0f 0x71 11/6. */
FNIEMOP_STUB_1(iemOp_Grp12_psllw_Nq_Ib,  uint8_t, bRm);

/** Opcode 0x66 0x0f 0x71 11/6. */
FNIEMOP_STUB_1(iemOp_Grp12_vpsllw_Hx_Ux_Ib, uint8_t, bRm);


/**
 * Group 12 jump table for register variant.
 */
IEM_STATIC const PFNIEMOPRM g_apfnGroup12RegReg[] =
{
    /* /0 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /1 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /2 */ iemOp_Grp12_psrlw_Nq_Ib,   iemOp_Grp12_vpsrlw_Hx_Ux_Ib, iemOp_InvalidWithRMNeedImm8, iemOp_InvalidWithRMNeedImm8,
    /* /3 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /4 */ iemOp_Grp12_psraw_Nq_Ib,   iemOp_Grp12_vpsraw_Hx_Ux_Ib, iemOp_InvalidWithRMNeedImm8, iemOp_InvalidWithRMNeedImm8,
    /* /5 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /6 */ iemOp_Grp12_psllw_Nq_Ib,   iemOp_Grp12_vpsllw_Hx_Ux_Ib, iemOp_InvalidWithRMNeedImm8, iemOp_InvalidWithRMNeedImm8,
    /* /7 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8)
};
AssertCompile(RT_ELEMENTS(g_apfnGroup12RegReg) == 8*4);


/** Opcode 0x0f 0x71. */
FNIEMOP_DEF(iemOp_Grp12)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        /* register, register */
        return FNIEMOP_CALL_1(g_apfnGroup12RegReg[  ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) * 4
                                                  + pVCpu->iem.s.idxPrefix], bRm);
    return FNIEMOP_CALL_1(iemOp_InvalidWithRMNeedImm8, bRm);
}


/** Opcode 0x0f 0x72 11/2. */
FNIEMOP_STUB_1(iemOp_Grp13_psrld_Nq_Ib,  uint8_t, bRm);

/** Opcode 0x66 0x0f 0x72 11/2. */
FNIEMOP_STUB_1(iemOp_Grp13_vpsrld_Hx_Ux_Ib, uint8_t, bRm);

/** Opcode 0x0f 0x72 11/4. */
FNIEMOP_STUB_1(iemOp_Grp13_psrad_Nq_Ib,  uint8_t, bRm);

/** Opcode 0x66 0x0f 0x72 11/4. */
FNIEMOP_STUB_1(iemOp_Grp13_vpsrad_Hx_Ux_Ib, uint8_t, bRm);

/** Opcode 0x0f 0x72 11/6. */
FNIEMOP_STUB_1(iemOp_Grp13_pslld_Nq_Ib,  uint8_t, bRm);

/** Opcode 0x66 0x0f 0x72 11/6. */
FNIEMOP_STUB_1(iemOp_Grp13_vpslld_Hx_Ux_Ib, uint8_t, bRm);


/**
 * Group 13 jump table for register variant.
 */
IEM_STATIC const PFNIEMOPRM g_apfnGroup13RegReg[] =
{
    /* /0 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /1 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /2 */ iemOp_Grp13_psrld_Nq_Ib,   iemOp_Grp13_vpsrld_Hx_Ux_Ib, iemOp_InvalidWithRMNeedImm8, iemOp_InvalidWithRMNeedImm8,
    /* /3 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /4 */ iemOp_Grp13_psrad_Nq_Ib,   iemOp_Grp13_vpsrad_Hx_Ux_Ib, iemOp_InvalidWithRMNeedImm8, iemOp_InvalidWithRMNeedImm8,
    /* /5 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /6 */ iemOp_Grp13_pslld_Nq_Ib,   iemOp_Grp13_vpslld_Hx_Ux_Ib, iemOp_InvalidWithRMNeedImm8, iemOp_InvalidWithRMNeedImm8,
    /* /7 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8)
};
AssertCompile(RT_ELEMENTS(g_apfnGroup13RegReg) == 8*4);

/** Opcode 0x0f 0x72. */
FNIEMOP_DEF(iemOp_Grp13)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        /* register, register */
        return FNIEMOP_CALL_1(g_apfnGroup13RegReg[ ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) * 4
                                                  + pVCpu->iem.s.idxPrefix], bRm);
    return FNIEMOP_CALL_1(iemOp_InvalidWithRMNeedImm8, bRm);
}


/** Opcode 0x0f 0x73 11/2. */
FNIEMOP_STUB_1(iemOp_Grp14_psrlq_Nq_Ib, uint8_t, bRm);

/** Opcode 0x66 0x0f 0x73 11/2. */
FNIEMOP_STUB_1(iemOp_Grp14_vpsrlq_Hx_Ux_Ib, uint8_t, bRm);

/** Opcode 0x66 0x0f 0x73 11/3. */
FNIEMOP_STUB_1(iemOp_Grp14_vpsrldq_Hx_Ux_Ib, uint8_t, bRm); //NEXT

/** Opcode 0x0f 0x73 11/6. */
FNIEMOP_STUB_1(iemOp_Grp14_psllq_Nq_Ib, uint8_t, bRm);

/** Opcode 0x66 0x0f 0x73 11/6. */
FNIEMOP_STUB_1(iemOp_Grp14_vpsllq_Hx_Ux_Ib, uint8_t, bRm);

/** Opcode 0x66 0x0f 0x73 11/7. */
FNIEMOP_STUB_1(iemOp_Grp14_vpslldq_Hx_Ux_Ib, uint8_t, bRm); //NEXT

/**
 * Group 14 jump table for register variant.
 */
IEM_STATIC const PFNIEMOPRM g_apfnGroup14RegReg[] =
{
    /* /0 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /1 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /2 */ iemOp_Grp14_psrlq_Nq_Ib,     iemOp_Grp14_vpsrlq_Hx_Ux_Ib,  iemOp_InvalidWithRMNeedImm8, iemOp_InvalidWithRMNeedImm8,
    /* /3 */ iemOp_InvalidWithRMNeedImm8, iemOp_Grp14_vpsrldq_Hx_Ux_Ib, iemOp_InvalidWithRMNeedImm8, iemOp_InvalidWithRMNeedImm8,
    /* /4 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /5 */ IEMOP_X4(iemOp_InvalidWithRMNeedImm8),
    /* /6 */ iemOp_Grp14_psllq_Nq_Ib,     iemOp_Grp14_vpsllq_Hx_Ux_Ib,  iemOp_InvalidWithRMNeedImm8, iemOp_InvalidWithRMNeedImm8,
    /* /7 */ iemOp_InvalidWithRMNeedImm8, iemOp_Grp14_vpslldq_Hx_Ux_Ib, iemOp_InvalidWithRMNeedImm8, iemOp_InvalidWithRMNeedImm8,
};
AssertCompile(RT_ELEMENTS(g_apfnGroup14RegReg) == 8*4);


/** Opcode 0x0f 0x73. */
FNIEMOP_DEF(iemOp_Grp14)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        /* register, register */
        return FNIEMOP_CALL_1(g_apfnGroup14RegReg[ ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) * 4
                                                  + pVCpu->iem.s.idxPrefix], bRm);
    return FNIEMOP_CALL_1(iemOp_InvalidWithRMNeedImm8, bRm);
}


/**
 * Common worker for MMX instructions on the form:
 *      pxxx    mm1, mm2/mem64
 */
FNIEMOP_DEF_1(iemOpCommonMmx_FullFull_To_Full, PCIEMOPMEDIAF2, pImpl)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
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
}


/**
 * Common worker for SSE2 instructions on the forms:
 *      pxxx    xmm1, xmm2/mem128
 *
 * Proper alignment of the 128-bit operand is enforced.
 * Exceptions type 4. SSE2 cpuid checks.
 */
FNIEMOP_DEF_1(iemOpCommonSse2_FullFull_To_Full, PCIEMOPMEDIAF2, pImpl)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
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
}


/** Opcode      0x0f 0x74 - pcmpeqb Pq, Qq */
FNIEMOP_DEF(iemOp_pcmpeqb_Pq_Qq)
{
    IEMOP_MNEMONIC(pcmpeqb, "pcmpeqb");
    return FNIEMOP_CALL_1(iemOpCommonMmx_FullFull_To_Full, &g_iemAImpl_pcmpeqb);
}

/** Opcode 0x66 0x0f 0x74 - vpcmpeqb Vx, Hx, Wx */
FNIEMOP_DEF(iemOp_vpcmpeqb_Vx_Hx_Wx)
{
    IEMOP_MNEMONIC(vpcmpeqb, "vpcmpeqb");
    return FNIEMOP_CALL_1(iemOpCommonSse2_FullFull_To_Full, &g_iemAImpl_pcmpeqb);
}

/*  Opcode 0xf3 0x0f 0x74 - invalid */
/*  Opcode 0xf2 0x0f 0x74 - invalid */


/** Opcode      0x0f 0x75 - pcmpeqw Pq, Qq */
FNIEMOP_DEF(iemOp_pcmpeqw_Pq_Qq)
{
    IEMOP_MNEMONIC(pcmpeqw, "pcmpeqw");
    return FNIEMOP_CALL_1(iemOpCommonMmx_FullFull_To_Full, &g_iemAImpl_pcmpeqw);
}

/** Opcode 0x66 0x0f 0x75 - vpcmpeqw Vx, Hx, Wx */
FNIEMOP_DEF(iemOp_vpcmpeqw_Vx_Hx_Wx)
{
    IEMOP_MNEMONIC(vpcmpeqw, "vpcmpeqw");
    return FNIEMOP_CALL_1(iemOpCommonSse2_FullFull_To_Full, &g_iemAImpl_pcmpeqw);
}

/*  Opcode 0xf3 0x0f 0x75 - invalid */
/*  Opcode 0xf2 0x0f 0x75 - invalid */


/** Opcode      0x0f 0x76 - pcmpeqd Pq, Qq */
FNIEMOP_DEF(iemOp_pcmpeqd_Pq_Qq)
{
    IEMOP_MNEMONIC(pcmpeqd, "pcmpeqd");
    return FNIEMOP_CALL_1(iemOpCommonMmx_FullFull_To_Full, &g_iemAImpl_pcmpeqd);
}

/** Opcode 0x66 0x0f 0x76 - vpcmpeqd Vx, Hx, Wx */
FNIEMOP_DEF(iemOp_vpcmpeqd_Vx_Hx_Wx)
{
    IEMOP_MNEMONIC(vpcmpeqd, "vpcmpeqd");
    return FNIEMOP_CALL_1(iemOpCommonSse2_FullFull_To_Full, &g_iemAImpl_pcmpeqd);
}

/*  Opcode 0xf3 0x0f 0x76 - invalid */
/*  Opcode 0xf2 0x0f 0x76 - invalid */


/** Opcode      0x0f 0x77 - emms vzeroupperv vzeroallv */
FNIEMOP_STUB(iemOp_emms__vzeroupperv__vzeroallv);
/*  Opcode 0x66 0x0f 0x77 - invalid */
/*  Opcode 0xf3 0x0f 0x77 - invalid */
/*  Opcode 0xf2 0x0f 0x77 - invalid */

/** Opcode      0x0f 0x78 - VMREAD Ey, Gy */
FNIEMOP_STUB(iemOp_vmread_Ey_Gy);
/*  Opcode 0x66 0x0f 0x78 - AMD Group 17 */
FNIEMOP_STUB(iemOp_AmdGrp17);
/*  Opcode 0xf3 0x0f 0x78 - invalid */
/*  Opcode 0xf2 0x0f 0x78 - invalid */

/** Opcode      0x0f 0x79 - VMWRITE Gy, Ey */
FNIEMOP_STUB(iemOp_vmwrite_Gy_Ey);
/*  Opcode 0x66 0x0f 0x79 - invalid */
/*  Opcode 0xf3 0x0f 0x79 - invalid */
/*  Opcode 0xf2 0x0f 0x79 - invalid */

/*  Opcode      0x0f 0x7a - invalid */
/*  Opcode 0x66 0x0f 0x7a - invalid */
/*  Opcode 0xf3 0x0f 0x7a - invalid */
/*  Opcode 0xf2 0x0f 0x7a - invalid */

/*  Opcode      0x0f 0x7b - invalid */
/*  Opcode 0x66 0x0f 0x7b - invalid */
/*  Opcode 0xf3 0x0f 0x7b - invalid */
/*  Opcode 0xf2 0x0f 0x7b - invalid */

/*  Opcode      0x0f 0x7c - invalid */
/** Opcode 0x66 0x0f 0x7c - vhaddpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vhaddpd_Vpd_Hpd_Wpd);
/*  Opcode 0xf3 0x0f 0x7c - invalid */
/** Opcode 0xf2 0x0f 0x7c - vhaddps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vhaddps_Vps_Hps_Wps);

/*  Opcode      0x0f 0x7d - invalid */
/** Opcode 0x66 0x0f 0x7d - vhsubpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vhsubpd_Vpd_Hpd_Wpd);
/*  Opcode 0xf3 0x0f 0x7d - invalid */
/** Opcode 0xf2 0x0f 0x7d - vhsubps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vhsubps_Vps_Hps_Wps);


/** Opcode      0x0f 0x7e - movd_q Ey, Pd */
FNIEMOP_DEF(iemOp_movd_q_Ey_Pd)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_REX_W)
        IEMOP_MNEMONIC(movq_Eq_Pq, "movq Eq,Pq");
    else
        IEMOP_MNEMONIC(movd_Ed_Pd, "movd Ed,Pd");
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
}

/** Opcode 0x66 0x0f 0x7e - vmovd_q Ey, Vy */
FNIEMOP_DEF(iemOp_vmovd_q_Ey_Vy)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_REX_W)
        IEMOP_MNEMONIC(vmovq_Eq_Wq, "vmovq Eq,Wq");
    else
        IEMOP_MNEMONIC(vmovd_Ed_Wd, "vmovd Ed,Wd");
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
}

/** Opcode 0xf3 0x0f 0x7e - vmovq Vq, Wq */
FNIEMOP_STUB(iemOp_vmovq_Vq_Wq);
/*  Opcode 0xf2 0x0f 0x7e - invalid */


/** Opcode      0x0f 0x7f - movq Qq, Pq */
FNIEMOP_DEF(iemOp_movq_Qq_Pq)
{
    IEMOP_MNEMONIC(movq_Qq_Pq, "movq Qq,Pq");
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
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
}

/** Opcode 0x66 0x0f 0x7f - vmovdqa Wx,Vx */
FNIEMOP_DEF(iemOp_vmovdqa_Wx_Vx)
{
    IEMOP_MNEMONIC(vmovdqa_Wdq_Vdq, "vmovdqa Wx,Vx");
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
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
        IEM_MC_STORE_MEM_U128_ALIGN_SSE(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, u128Tmp);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}

/** Opcode 0xf3 0x0f 0x7f - vmovdqu Wx,Vx */
FNIEMOP_DEF(iemOp_vmovdqu_Wx_Vx)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_MNEMONIC(vmovdqu_Wdq_Vdq, "vmovdqu Wx,Vx");
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
        IEM_MC_STORE_MEM_U128(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, u128Tmp);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}

/*  Opcode 0xf2 0x0f 0x7f - invalid */



/** Opcode 0x0f 0x80. */
FNIEMOP_DEF(iemOp_jo_Jv)
{
    IEMOP_MNEMONIC(jo_Jv, "jo  Jv");
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
    IEMOP_MNEMONIC(jno_Jv, "jno Jv");
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
    IEMOP_MNEMONIC(jc_Jv, "jc/jb/jnae Jv");
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
    IEMOP_MNEMONIC(jnc_Jv, "jnc/jnb/jae Jv");
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
    IEMOP_MNEMONIC(je_Jv, "je/jz Jv");
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
    IEMOP_MNEMONIC(jne_Jv, "jne/jnz Jv");
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
    IEMOP_MNEMONIC(jbe_Jv, "jbe/jna Jv");
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
    IEMOP_MNEMONIC(ja_Jv, "jnbe/ja Jv");
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
    IEMOP_MNEMONIC(js_Jv, "js  Jv");
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
    IEMOP_MNEMONIC(jns_Jv, "jns Jv");
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
    IEMOP_MNEMONIC(jp_Jv, "jp  Jv");
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
    IEMOP_MNEMONIC(jnp_Jv, "jnp Jv");
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
    IEMOP_MNEMONIC(jl_Jv, "jl/jnge Jv");
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
    IEMOP_MNEMONIC(jge_Jv, "jnl/jge Jv");
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
    IEMOP_MNEMONIC(jle_Jv, "jle/jng Jv");
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
    IEMOP_MNEMONIC(jg_Jv, "jnle/jg Jv");
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
    IEMOP_MNEMONIC(seto_Eb, "seto Eb");
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
    IEMOP_MNEMONIC(setno_Eb, "setno Eb");
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
    IEMOP_MNEMONIC(setc_Eb, "setc Eb");
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
    IEMOP_MNEMONIC(setnc_Eb, "setnc Eb");
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
    IEMOP_MNEMONIC(sete_Eb, "sete Eb");
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
    IEMOP_MNEMONIC(setne_Eb, "setne Eb");
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
    IEMOP_MNEMONIC(setbe_Eb, "setbe Eb");
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
    IEMOP_MNEMONIC(setnbe_Eb, "setnbe Eb");
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
    IEMOP_MNEMONIC(sets_Eb, "sets Eb");
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
    IEMOP_MNEMONIC(setns_Eb, "setns Eb");
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
    IEMOP_MNEMONIC(setp_Eb, "setp Eb");
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
    IEMOP_MNEMONIC(setnp_Eb, "setnp Eb");
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
    IEMOP_MNEMONIC(setl_Eb, "setl Eb");
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
    IEMOP_MNEMONIC(setnl_Eb, "setnl Eb");
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
    IEMOP_MNEMONIC(setle_Eb, "setle Eb");
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
    IEMOP_MNEMONIC(setnle_Eb, "setnle Eb");
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
    Assert(iReg < X86_SREG_FS || pVCpu->iem.s.enmCpuMode != IEMMODE_64BIT);
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
    IEMOP_MNEMONIC(push_fs, "push fs");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return FNIEMOP_CALL_1(iemOpCommonPushSReg, X86_SREG_FS);
}


/** Opcode 0x0f 0xa1. */
FNIEMOP_DEF(iemOp_pop_fs)
{
    IEMOP_MNEMONIC(pop_fs, "pop fs");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_pop_Sreg, X86_SREG_FS, pVCpu->iem.s.enmEffOpSize);
}


/** Opcode 0x0f 0xa2. */
FNIEMOP_DEF(iemOp_cpuid)
{
    IEMOP_MNEMONIC(cpuid, "cpuid");
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
                IEM_MC_SHL_LOCAL_S16(i16AddrAdj, 1);
                IEM_MC_ADD_LOCAL_S16_TO_EFF_ADDR(GCPtrEffDst, i16AddrAdj);
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


/** Opcode 0x0f 0xa3. */
FNIEMOP_DEF(iemOp_bt_Ev_Gv)
{
    IEMOP_MNEMONIC(bt_Ev_Gv, "bt  Ev,Gv");
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
    IEMOP_MNEMONIC(shld_Ev_Gv_Ib, "shld Ev,Gv,Ib");
    IEMOP_HLP_MIN_386();
    return FNIEMOP_CALL_1(iemOpCommonShldShrd_Ib, &g_iemAImpl_shld);
}


/** Opcode 0x0f 0xa5. */
FNIEMOP_DEF(iemOp_shld_Ev_Gv_CL)
{
    IEMOP_MNEMONIC(shld_Ev_Gv_CL, "shld Ev,Gv,CL");
    IEMOP_HLP_MIN_386();
    return FNIEMOP_CALL_1(iemOpCommonShldShrd_CL, &g_iemAImpl_shld);
}


/** Opcode 0x0f 0xa8. */
FNIEMOP_DEF(iemOp_push_gs)
{
    IEMOP_MNEMONIC(push_gs, "push gs");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return FNIEMOP_CALL_1(iemOpCommonPushSReg, X86_SREG_GS);
}


/** Opcode 0x0f 0xa9. */
FNIEMOP_DEF(iemOp_pop_gs)
{
    IEMOP_MNEMONIC(pop_gs, "pop gs");
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
    IEMOP_MNEMONIC(bts_Ev_Gv, "bts Ev,Gv");
    IEMOP_HLP_MIN_386();
    return FNIEMOP_CALL_1(iemOpCommonBit_Ev_Gv, &g_iemAImpl_bts);
}


/** Opcode 0x0f 0xac. */
FNIEMOP_DEF(iemOp_shrd_Ev_Gv_Ib)
{
    IEMOP_MNEMONIC(shrd_Ev_Gv_Ib, "shrd Ev,Gv,Ib");
    IEMOP_HLP_MIN_386();
    return FNIEMOP_CALL_1(iemOpCommonShldShrd_Ib, &g_iemAImpl_shrd);
}


/** Opcode 0x0f 0xad. */
FNIEMOP_DEF(iemOp_shrd_Ev_Gv_CL)
{
    IEMOP_MNEMONIC(shrd_Ev_Gv_CL, "shrd Ev,Gv,CL");
    IEMOP_HLP_MIN_386();
    return FNIEMOP_CALL_1(iemOpCommonShldShrd_CL, &g_iemAImpl_shrd);
}


/** Opcode 0x0f 0xae mem/0. */
FNIEMOP_DEF_1(iemOp_Grp15_fxsave,   uint8_t, bRm)
{
    IEMOP_MNEMONIC(fxsave, "fxsave m512");
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
    IEMOP_MNEMONIC(fxrstor, "fxrstor m512");
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
    RT_NOREF_PV(bRm);
    IEMOP_MNEMONIC(lfence, "lfence");
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
    RT_NOREF_PV(bRm);
    IEMOP_MNEMONIC(mfence, "mfence");
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
    RT_NOREF_PV(bRm);
    IEMOP_MNEMONIC(sfence, "sfence");
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
/** @todo continue here tomorrow! (see bs3-cpu-decoding-1.c32 r113507).  */
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
    IEMOP_MNEMONIC(imul_Gv_Ev, "imul Gv,Ev");
    IEMOP_HLP_MIN_386();
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rv_rm, &g_iemAImpl_imul_two);
}


/** Opcode 0x0f 0xb0. */
FNIEMOP_DEF(iemOp_cmpxchg_Eb_Gb)
{
    IEMOP_MNEMONIC(cmpxchg_Eb_Gb, "cmpxchg Eb,Gb");
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
    IEMOP_MNEMONIC(cmpxchg_Ev_Gv, "cmpxchg Ev,Gv");
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
    IEMOP_MNEMONIC(lss_Gv_Mp, "lss Gv,Mp");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        return IEMOP_RAISE_INVALID_OPCODE();
    return FNIEMOP_CALL_2(iemOpCommonLoadSRegAndGreg, X86_SREG_SS, bRm);
}


/** Opcode 0x0f 0xb3. */
FNIEMOP_DEF(iemOp_btr_Ev_Gv)
{
    IEMOP_MNEMONIC(btr_Ev_Gv, "btr Ev,Gv");
    IEMOP_HLP_MIN_386();
    return FNIEMOP_CALL_1(iemOpCommonBit_Ev_Gv, &g_iemAImpl_btr);
}


/** Opcode 0x0f 0xb4. */
FNIEMOP_DEF(iemOp_lfs_Gv_Mp)
{
    IEMOP_MNEMONIC(lfs_Gv_Mp, "lfs Gv,Mp");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        return IEMOP_RAISE_INVALID_OPCODE();
    return FNIEMOP_CALL_2(iemOpCommonLoadSRegAndGreg, X86_SREG_FS, bRm);
}


/** Opcode 0x0f 0xb5. */
FNIEMOP_DEF(iemOp_lgs_Gv_Mp)
{
    IEMOP_MNEMONIC(lgs_Gv_Mp, "lgs Gv,Mp");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        return IEMOP_RAISE_INVALID_OPCODE();
    return FNIEMOP_CALL_2(iemOpCommonLoadSRegAndGreg, X86_SREG_GS, bRm);
}


/** Opcode 0x0f 0xb6. */
FNIEMOP_DEF(iemOp_movzx_Gv_Eb)
{
    IEMOP_MNEMONIC(movzx_Gv_Eb, "movzx Gv,Eb");
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
    IEMOP_MNEMONIC(movzx_Gv_Ew, "movzx Gv,Ew");
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


/** Opcode      0x0f 0xb8 - JMPE (reserved for emulator on IPF) */
FNIEMOP_UD_STUB(iemOp_jmpe);
/** Opcode 0xf3 0x0f 0xb8 - POPCNT Gv, Ev */
FNIEMOP_STUB(iemOp_popcnt_Gv_Ev);


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
        case 4: pImpl = &g_iemAImpl_bt;  IEMOP_MNEMONIC(bt_Ev_Ib,  "bt  Ev,Ib"); break;
        case 5: pImpl = &g_iemAImpl_bts; IEMOP_MNEMONIC(bts_Ev_Ib, "bts Ev,Ib"); break;
        case 6: pImpl = &g_iemAImpl_btr; IEMOP_MNEMONIC(btr_Ev_Ib, "btr Ev,Ib"); break;
        case 7: pImpl = &g_iemAImpl_btc; IEMOP_MNEMONIC(btc_Ev_Ib, "btc Ev,Ib"); break;
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
    IEMOP_MNEMONIC(btc_Ev_Gv, "btc Ev,Gv");
    IEMOP_HLP_MIN_386();
    return FNIEMOP_CALL_1(iemOpCommonBit_Ev_Gv, &g_iemAImpl_btc);
}


/** Opcode 0x0f 0xbc. */
FNIEMOP_DEF(iemOp_bsf_Gv_Ev)
{
    IEMOP_MNEMONIC(bsf_Gv_Ev, "bsf Gv,Ev");
    IEMOP_HLP_MIN_386();
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF | X86_EFL_SF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rv_rm, &g_iemAImpl_bsf);
}


/** Opcode 0xf3 0x0f 0xbc - TZCNT Gv, Ev */
FNIEMOP_STUB(iemOp_tzcnt_Gv_Ev);


/** Opcode 0x0f 0xbd. */
FNIEMOP_DEF(iemOp_bsr_Gv_Ev)
{
    IEMOP_MNEMONIC(bsr_Gv_Ev, "bsr Gv,Ev");
    IEMOP_HLP_MIN_386();
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF | X86_EFL_SF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rv_rm, &g_iemAImpl_bsr);
}


/** Opcode 0xf3 0x0f 0xbd - LZCNT Gv, Ev */
FNIEMOP_STUB(iemOp_lzcnt_Gv_Ev);


/** Opcode 0x0f 0xbe. */
FNIEMOP_DEF(iemOp_movsx_Gv_Eb)
{
    IEMOP_MNEMONIC(movsx_Gv_Eb, "movsx Gv,Eb");
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
    IEMOP_MNEMONIC(movsx_Gv_Ew, "movsx Gv,Ew");
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
    IEMOP_MNEMONIC(xadd_Eb_Gb, "xadd Eb,Gb");

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
    IEMOP_MNEMONIC(xadd_Ev_Gv, "xadd Ev,Gv");
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


/** Opcode      0x0f 0xc2 - vcmpps Vps,Hps,Wps,Ib */
FNIEMOP_STUB(iemOp_vcmpps_Vps_Hps_Wps_Ib);
/** Opcode 0x66 0x0f 0xc2 - vcmppd Vpd,Hpd,Wpd,Ib */
FNIEMOP_STUB(iemOp_vcmppd_Vpd_Hpd_Wpd_Ib);
/** Opcode 0xf3 0x0f 0xc2 - vcmpss Vss,Hss,Wss,Ib */
FNIEMOP_STUB(iemOp_vcmpss_Vss_Hss_Wss_Ib);
/** Opcode 0xf2 0x0f 0xc2 - vcmpsd Vsd,Hsd,Wsd,Ib */
FNIEMOP_STUB(iemOp_vcmpsd_Vsd_Hsd_Wsd_Ib);


/** Opcode 0x0f 0xc3. */
FNIEMOP_DEF(iemOp_movnti_My_Gy)
{
    IEMOP_MNEMONIC(movnti_My_Gy, "movnti My,Gy");

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
/*  Opcode 0x66 0x0f 0xc3 - invalid */
/*  Opcode 0xf3 0x0f 0xc3 - invalid */
/*  Opcode 0xf2 0x0f 0xc3 - invalid */

/** Opcode      0x0f 0xc4 - pinsrw Pq,Ry/Mw,Ib */
FNIEMOP_STUB(iemOp_pinsrw_Pq_RyMw_Ib);
/** Opcode 0x66 0x0f 0xc4 - vpinsrw Vdq,Hdq,Ry/Mw,Ib */
FNIEMOP_STUB(iemOp_vpinsrw_Vdq_Hdq_RyMw_Ib);
/*  Opcode 0xf3 0x0f 0xc4 - invalid */
/*  Opcode 0xf2 0x0f 0xc4 - invalid */

/** Opcode      0x0f 0xc5 - pextrw Gd, Nq, Ib */
FNIEMOP_STUB(iemOp_pextrw_Gd_Nq_Ib);
/** Opcode 0x66 0x0f 0xc5 - vpextrw Gd, Udq, Ib */
FNIEMOP_STUB(iemOp_vpextrw_Gd_Udq_Ib);
/*  Opcode 0xf3 0x0f 0xc5 - invalid */
/*  Opcode 0xf2 0x0f 0xc5 - invalid */

/** Opcode      0x0f 0xc6 - vshufps Vps,Hps,Wps,Ib */
FNIEMOP_STUB(iemOp_vshufps_Vps_Hps_Wps_Ib);
/** Opcode 0x66 0x0f 0xc6 - vshufpd Vpd,Hpd,Wpd,Ib */
FNIEMOP_STUB(iemOp_vshufpd_Vpd_Hpd_Wpd_Ib);
/*  Opcode 0xf3 0x0f 0xc6 - invalid */
/*  Opcode 0xf2 0x0f 0xc6 - invalid */


/** Opcode 0x0f 0xc7 !11/1. */
FNIEMOP_DEF_1(iemOp_Grp9_cmpxchg8b_Mq, uint8_t, bRm)
{
    IEMOP_MNEMONIC(cmpxchg8b, "cmpxchg8b Mq");

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
FNIEMOP_DEF_1(iemOp_Grp9_cmpxchg16b_Mdq, uint8_t, bRm)
{
    IEMOP_MNEMONIC(cmpxchg16b, "cmpxchg16b Mdq");
    if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fMovCmpXchg16b)
    {
#if 0
        RT_NOREF(bRm);
        IEMOP_BITCH_ABOUT_STUB();
        return VERR_IEM_INSTR_NOT_IMPLEMENTED;
#else
        IEM_MC_BEGIN(4, 3);
        IEM_MC_ARG(PRTUINT128U, pu128MemDst,     0);
        IEM_MC_ARG(PRTUINT128U, pu128RaxRdx,     1);
        IEM_MC_ARG(PRTUINT128U, pu128RbxRcx,     2);
        IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 3);
        IEM_MC_LOCAL(RTUINT128U, u128RaxRdx);
        IEM_MC_LOCAL(RTUINT128U, u128RbxRcx);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING();
        IEM_MC_RAISE_GP0_IF_EFF_ADDR_UNALIGNED(GCPtrEffDst, 16);
        IEM_MC_MEM_MAP(pu128MemDst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);

        IEM_MC_FETCH_GREG_U64(u128RaxRdx.s.Lo, X86_GREG_xAX);
        IEM_MC_FETCH_GREG_U64(u128RaxRdx.s.Hi, X86_GREG_xDX);
        IEM_MC_REF_LOCAL(pu128RaxRdx, u128RaxRdx);

        IEM_MC_FETCH_GREG_U64(u128RbxRcx.s.Lo, X86_GREG_xBX);
        IEM_MC_FETCH_GREG_U64(u128RbxRcx.s.Hi, X86_GREG_xCX);
        IEM_MC_REF_LOCAL(pu128RbxRcx, u128RbxRcx);

        IEM_MC_FETCH_EFLAGS(EFlags);
# ifdef RT_ARCH_AMD64
        if (IEM_GET_HOST_CPU_FEATURES(pVCpu)->fMovCmpXchg16b)
        {
            if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg16b, pu128MemDst, pu128RaxRdx, pu128RbxRcx, pEFlags);
            else
                IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg16b_locked, pu128MemDst, pu128RaxRdx, pu128RbxRcx, pEFlags);
        }
        else
# endif
        {
            /* Note! The fallback for 32-bit systems and systems without CX16 is multiple
                     accesses and not all all atomic, which works fine on in UNI CPU guest
                     configuration (ignoring DMA).  If guest SMP is active we have no choice
                     but to use a rendezvous callback here.  Sigh. */
            if (pVCpu->CTX_SUFF(pVM)->cCpus == 1)
                IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg16b_fallback, pu128MemDst, pu128RaxRdx, pu128RbxRcx, pEFlags);
            else
            {
                IEM_MC_CALL_CIMPL_4(iemCImpl_cmpxchg16b_fallback_rendezvous, pu128MemDst, pu128RaxRdx, pu128RbxRcx, pEFlags);
                /* Does not get here, tail code is duplicated in iemCImpl_cmpxchg16b_fallback_rendezvous. */
            }
        }

        IEM_MC_MEM_COMMIT_AND_UNMAP(pu128MemDst, IEM_ACCESS_DATA_RW);
        IEM_MC_COMMIT_EFLAGS(EFlags);
        IEM_MC_IF_EFL_BIT_NOT_SET(X86_EFL_ZF)
            IEM_MC_STORE_GREG_U64(X86_GREG_xAX, u128RaxRdx.s.Lo);
            IEM_MC_STORE_GREG_U64(X86_GREG_xDX, u128RaxRdx.s.Hi);
        IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();

        IEM_MC_END();
        return VINF_SUCCESS;
#endif
    }
    Log(("cmpxchg16b -> #UD\n"));
    return IEMOP_RAISE_INVALID_OPCODE();
}


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
            if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_REX_W)
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
    IEMOP_MNEMONIC(bswap_rAX_r8, "bswap rAX/r8");
    /* Note! Intel manuals states that R8-R15 can be accessed by using a REX.X
             prefix.  REX.B is the correct prefix it appears.  For a parallel
             case, see iemOp_mov_AL_Ib and iemOp_mov_eAX_Iv. */
    IEMOP_HLP_MIN_486();
    return FNIEMOP_CALL_1(iemOpCommonBswapGReg, X86_GREG_xAX | pVCpu->iem.s.uRexB);
}


/** Opcode 0x0f 0xc9. */
FNIEMOP_DEF(iemOp_bswap_rCX_r9)
{
    IEMOP_MNEMONIC(bswap_rCX_r9, "bswap rCX/r9");
    IEMOP_HLP_MIN_486();
    return FNIEMOP_CALL_1(iemOpCommonBswapGReg, X86_GREG_xCX | pVCpu->iem.s.uRexB);
}


/** Opcode 0x0f 0xca. */
FNIEMOP_DEF(iemOp_bswap_rDX_r10)
{
    IEMOP_MNEMONIC(bswap_rDX_r9, "bswap rDX/r9");
    IEMOP_HLP_MIN_486();
    return FNIEMOP_CALL_1(iemOpCommonBswapGReg, X86_GREG_xDX | pVCpu->iem.s.uRexB);
}


/** Opcode 0x0f 0xcb. */
FNIEMOP_DEF(iemOp_bswap_rBX_r11)
{
    IEMOP_MNEMONIC(bswap_rBX_r9, "bswap rBX/r9");
    IEMOP_HLP_MIN_486();
    return FNIEMOP_CALL_1(iemOpCommonBswapGReg, X86_GREG_xBX | pVCpu->iem.s.uRexB);
}


/** Opcode 0x0f 0xcc. */
FNIEMOP_DEF(iemOp_bswap_rSP_r12)
{
    IEMOP_MNEMONIC(bswap_rSP_r12, "bswap rSP/r12");
    IEMOP_HLP_MIN_486();
    return FNIEMOP_CALL_1(iemOpCommonBswapGReg, X86_GREG_xSP | pVCpu->iem.s.uRexB);
}


/** Opcode 0x0f 0xcd. */
FNIEMOP_DEF(iemOp_bswap_rBP_r13)
{
    IEMOP_MNEMONIC(bswap_rBP_r13, "bswap rBP/r13");
    IEMOP_HLP_MIN_486();
    return FNIEMOP_CALL_1(iemOpCommonBswapGReg, X86_GREG_xBP | pVCpu->iem.s.uRexB);
}


/** Opcode 0x0f 0xce. */
FNIEMOP_DEF(iemOp_bswap_rSI_r14)
{
    IEMOP_MNEMONIC(bswap_rSI_r14, "bswap rSI/r14");
    IEMOP_HLP_MIN_486();
    return FNIEMOP_CALL_1(iemOpCommonBswapGReg, X86_GREG_xSI | pVCpu->iem.s.uRexB);
}


/** Opcode 0x0f 0xcf. */
FNIEMOP_DEF(iemOp_bswap_rDI_r15)
{
    IEMOP_MNEMONIC(bswap_rDI_r15, "bswap rDI/r15");
    IEMOP_HLP_MIN_486();
    return FNIEMOP_CALL_1(iemOpCommonBswapGReg, X86_GREG_xDI | pVCpu->iem.s.uRexB);
}


/*  Opcode      0x0f 0xd0 - invalid */
/** Opcode 0x66 0x0f 0xd0 - vaddsubpd Vpd, Hpd, Wpd */
FNIEMOP_STUB(iemOp_vaddsubpd_Vpd_Hpd_Wpd);
/*  Opcode 0xf3 0x0f 0xd0 - invalid */
/** Opcode 0xf2 0x0f 0xd0 - vaddsubps Vps, Hps, Wps */
FNIEMOP_STUB(iemOp_vaddsubps_Vps_Hps_Wps);

/** Opcode      0x0f 0xd1 - psrlw Pq, Qq */
FNIEMOP_STUB(iemOp_psrlw_Pq_Qq);
/** Opcode 0x66 0x0f 0xd1 - vpsrlw Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpsrlw_Vx_Hx_W);
/*  Opcode 0xf3 0x0f 0xd1 - invalid */
/*  Opcode 0xf2 0x0f 0xd1 - invalid */

/** Opcode      0x0f 0xd2 - psrld Pq, Qq */
FNIEMOP_STUB(iemOp_psrld_Pq_Qq);
/** Opcode 0x66 0x0f 0xd2 - vpsrld Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpsrld_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0xd2 - invalid */
/*  Opcode 0xf2 0x0f 0xd2 - invalid */

/** Opcode      0x0f 0xd3 - psrlq Pq, Qq */
FNIEMOP_STUB(iemOp_psrlq_Pq_Qq);
/** Opcode 0x66 0x0f 0xd3 - vpsrlq Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpsrlq_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0xd3 - invalid */
/*  Opcode 0xf2 0x0f 0xd3 - invalid */

/** Opcode      0x0f 0xd4 - paddq Pq, Qq */
FNIEMOP_STUB(iemOp_paddq_Pq_Qq);
/** Opcode 0x66 0x0f 0xd4 - vpaddq Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpaddq_Vx_Hx_W);
/*  Opcode 0xf3 0x0f 0xd4 - invalid */
/*  Opcode 0xf2 0x0f 0xd4 - invalid */

/** Opcode      0x0f 0xd5 - pmullw Pq, Qq */
FNIEMOP_STUB(iemOp_pmullw_Pq_Qq);
/** Opcode 0x66 0x0f 0xd5 - vpmullw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpmullw_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0xd5 - invalid */
/*  Opcode 0xf2 0x0f 0xd5 - invalid */

/*  Opcode      0x0f 0xd6 - invalid */
/** Opcode 0x66 0x0f 0xd6 - vmovq Wq, Vq */
FNIEMOP_STUB(iemOp_vmovq_Wq_Vq);
/** Opcode 0xf3 0x0f 0xd6 - movq2dq Vdq, Nq */
FNIEMOP_STUB(iemOp_movq2dq_Vdq_Nq);
/** Opcode 0xf2 0x0f 0xd6 - movdq2q Pq, Uq */
FNIEMOP_STUB(iemOp_movdq2q_Pq_Uq);
#if 0
FNIEMOP_DEF(iemOp_movq_Wq_Vq__movq2dq_Vdq_Nq__movdq2q_Pq_Uq)
{
    /* Docs says register only. */
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        case IEM_OP_PRF_SIZE_OP: /* SSE */
            IEMOP_MNEMONIC(movq_Wq_Vq, "movq Wq,Vq");
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
            I E M O P _ M N E M O N I C(pmovmskb_Gd_Udq, "pmovmskb Gd,Udq");
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
#endif


/** Opcode      0x0f 0xd7 - pmovmskb Gd, Nq */
FNIEMOP_DEF(iemOp_pmovmskb_Gd_Nq)
{
    /* Note! Taking the lazy approch here wrt the high 32-bits of the GREG. */
    /** @todo testcase: Check that the instruction implicitly clears the high
     *        bits in 64-bit mode.  The REX.W is first necessary when VLMAX > 256
     *        and opcode modifications are made to work with the whole width (not
     *        just 128). */
    IEMOP_MNEMONIC(pmovmskb_Gd_Udq, "pmovmskb Gd,Nq");
    /* Docs says register only. */
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT)) /** @todo test that this is registers only. */
    {
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
    }
    return IEMOP_RAISE_INVALID_OPCODE();
}

/** Opcode 0x66 0x0f 0xd7 -  */
FNIEMOP_DEF(iemOp_vpmovmskb_Gd_Ux)
{
    /* Note! Taking the lazy approch here wrt the high 32-bits of the GREG. */
    /** @todo testcase: Check that the instruction implicitly clears the high
     *        bits in 64-bit mode.  The REX.W is first necessary when VLMAX > 256
     *        and opcode modifications are made to work with the whole width (not
     *        just 128). */
    IEMOP_MNEMONIC(pmovmskb_Gd_Nq, "vpmovmskb Gd, Ux");
    /* Docs says register only. */
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT)) /** @todo test that this is registers only. */
    {
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
    }
    return IEMOP_RAISE_INVALID_OPCODE();
}

/*  Opcode 0xf3 0x0f 0xd7 - invalid */
/*  Opcode 0xf2 0x0f 0xd7 - invalid */


/** Opcode      0x0f 0xd8 - psubusb Pq, Qq */
FNIEMOP_STUB(iemOp_psubusb_Pq_Qq);
/** Opcode 0x66 0x0f 0xd8 - vpsubusb Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpsubusb_Vx_Hx_W);
/*  Opcode 0xf3 0x0f 0xd8 - invalid */
/*  Opcode 0xf2 0x0f 0xd8 - invalid */

/** Opcode      0x0f 0xd9 - psubusw Pq, Qq */
FNIEMOP_STUB(iemOp_psubusw_Pq_Qq);
/** Opcode 0x66 0x0f 0xd9 - vpsubusw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpsubusw_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0xd9 - invalid */
/*  Opcode 0xf2 0x0f 0xd9 - invalid */

/** Opcode      0x0f 0xda - pminub Pq, Qq */
FNIEMOP_STUB(iemOp_pminub_Pq_Qq);
/** Opcode 0x66 0x0f 0xda - vpminub Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpminub_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0xda - invalid */
/*  Opcode 0xf2 0x0f 0xda - invalid */

/** Opcode      0x0f 0xdb - pand Pq, Qq */
FNIEMOP_STUB(iemOp_pand_Pq_Qq);
/** Opcode 0x66 0x0f 0xdb - vpand Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpand_Vx_Hx_W);
/*  Opcode 0xf3 0x0f 0xdb - invalid */
/*  Opcode 0xf2 0x0f 0xdb - invalid */

/** Opcode      0x0f 0xdc - paddusb Pq, Qq */
FNIEMOP_STUB(iemOp_paddusb_Pq_Qq);
/** Opcode 0x66 0x0f 0xdc - vpaddusb Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpaddusb_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0xdc - invalid */
/*  Opcode 0xf2 0x0f 0xdc - invalid */

/** Opcode      0x0f 0xdd - paddusw Pq, Qq */
FNIEMOP_STUB(iemOp_paddusw_Pq_Qq);
/** Opcode 0x66 0x0f 0xdd - vpaddusw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpaddusw_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0xdd - invalid */
/*  Opcode 0xf2 0x0f 0xdd - invalid */

/** Opcode      0x0f 0xde - pmaxub Pq, Qq */
FNIEMOP_STUB(iemOp_pmaxub_Pq_Qq);
/** Opcode 0x66 0x0f 0xde - vpmaxub Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpmaxub_Vx_Hx_W);
/*  Opcode 0xf3 0x0f 0xde - invalid */
/*  Opcode 0xf2 0x0f 0xde - invalid */

/** Opcode      0x0f 0xdf - pandn Pq, Qq */
FNIEMOP_STUB(iemOp_pandn_Pq_Qq);
/** Opcode 0x66 0x0f 0xdf - vpandn Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpandn_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0xdf - invalid */
/*  Opcode 0xf2 0x0f 0xdf - invalid */

/** Opcode      0x0f 0xe0 - pavgb Pq, Qq */
FNIEMOP_STUB(iemOp_pavgb_Pq_Qq);
/** Opcode 0x66 0x0f 0xe0 - vpavgb Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpavgb_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0xe0 - invalid */
/*  Opcode 0xf2 0x0f 0xe0 - invalid */

/** Opcode      0x0f 0xe1 - psraw Pq, Qq */
FNIEMOP_STUB(iemOp_psraw_Pq_Qq);
/** Opcode 0x66 0x0f 0xe1 - vpsraw Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpsraw_Vx_Hx_W);
/*  Opcode 0xf3 0x0f 0xe1 - invalid */
/*  Opcode 0xf2 0x0f 0xe1 - invalid */

/** Opcode      0x0f 0xe2 - psrad Pq, Qq */
FNIEMOP_STUB(iemOp_psrad_Pq_Qq);
/** Opcode 0x66 0x0f 0xe2 - vpsrad Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpsrad_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0xe2 - invalid */
/*  Opcode 0xf2 0x0f 0xe2 - invalid */

/** Opcode      0x0f 0xe3 - pavgw Pq, Qq */
FNIEMOP_STUB(iemOp_pavgw_Pq_Qq);
/** Opcode 0x66 0x0f 0xe3 - vpavgw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpavgw_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0xe3 - invalid */
/*  Opcode 0xf2 0x0f 0xe3 - invalid */

/** Opcode      0x0f 0xe4 - pmulhuw Pq, Qq */
FNIEMOP_STUB(iemOp_pmulhuw_Pq_Qq);
/** Opcode 0x66 0x0f 0xe4 - vpmulhuw Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpmulhuw_Vx_Hx_W);
/*  Opcode 0xf3 0x0f 0xe4 - invalid */
/*  Opcode 0xf2 0x0f 0xe4 - invalid */

/** Opcode      0x0f 0xe5 - pmulhw Pq, Qq */
FNIEMOP_STUB(iemOp_pmulhw_Pq_Qq);
/** Opcode 0x66 0x0f 0xe5 - vpmulhw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpmulhw_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0xe5 - invalid */
/*  Opcode 0xf2 0x0f 0xe5 - invalid */

/*  Opcode      0x0f 0xe6 - invalid */
/** Opcode 0x66 0x0f 0xe6 - vcvttpd2dq Vx, Wpd */
FNIEMOP_STUB(iemOp_vcvttpd2dq_Vx_Wpd);
/** Opcode 0xf3 0x0f 0xe6 - vcvtdq2pd Vx, Wpd */
FNIEMOP_STUB(iemOp_vcvtdq2pd_Vx_Wpd);
/** Opcode 0xf2 0x0f 0xe6 - vcvtpd2dq Vx, Wpd */
FNIEMOP_STUB(iemOp_vcvtpd2dq_Vx_Wpd);


/** Opcode      0x0f 0xe7 - movntq Mq, Pq */
FNIEMOP_DEF(iemOp_movntq_Mq_Pq)
{
    IEMOP_MNEMONIC(movntq_Mq_Pq, "movntq Mq,Pq");
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
    {
        /* Register, memory. */
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
        return VINF_SUCCESS;
    }
    /* The register, register encoding is invalid. */
    return IEMOP_RAISE_INVALID_OPCODE();
}

/** Opcode 0x66 0x0f 0xe7 - vmovntdq Mx, Vx */
FNIEMOP_DEF(iemOp_vmovntdq_Mx_Vx)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
    {
        /* Register, memory. */
        IEMOP_MNEMONIC(vmovntdq_Mx_Vx, "vmovntdq Mx,Vx");
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
        return VINF_SUCCESS;
    }

    /* The register, register encoding is invalid. */
    return IEMOP_RAISE_INVALID_OPCODE();
}

/*  Opcode 0xf3 0x0f 0xe7 - invalid */
/*  Opcode 0xf2 0x0f 0xe7 - invalid */


/** Opcode      0x0f 0xe8 - psubsb Pq, Qq */
FNIEMOP_STUB(iemOp_psubsb_Pq_Qq);
/** Opcode 0x66 0x0f 0xe8 - vpsubsb Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpsubsb_Vx_Hx_W);
/*  Opcode 0xf3 0x0f 0xe8 - invalid */
/*  Opcode 0xf2 0x0f 0xe8 - invalid */

/** Opcode      0x0f 0xe9 - psubsw Pq, Qq */
FNIEMOP_STUB(iemOp_psubsw_Pq_Qq);
/** Opcode 0x66 0x0f 0xe9 - vpsubsw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpsubsw_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0xe9 - invalid */
/*  Opcode 0xf2 0x0f 0xe9 - invalid */

/** Opcode      0x0f 0xea - pminsw Pq, Qq */
FNIEMOP_STUB(iemOp_pminsw_Pq_Qq);
/** Opcode 0x66 0x0f 0xea - vpminsw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpminsw_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0xea - invalid */
/*  Opcode 0xf2 0x0f 0xea - invalid */

/** Opcode      0x0f 0xeb - por Pq, Qq */
FNIEMOP_STUB(iemOp_por_Pq_Qq);
/** Opcode 0x66 0x0f 0xeb - vpor Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpor_Vx_Hx_W);
/*  Opcode 0xf3 0x0f 0xeb - invalid */
/*  Opcode 0xf2 0x0f 0xeb - invalid */

/** Opcode      0x0f 0xec - paddsb Pq, Qq */
FNIEMOP_STUB(iemOp_paddsb_Pq_Qq);
/** Opcode 0x66 0x0f 0xec - vpaddsb Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpaddsb_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0xec - invalid */
/*  Opcode 0xf2 0x0f 0xec - invalid */

/** Opcode      0x0f 0xed - paddsw Pq, Qq */
FNIEMOP_STUB(iemOp_paddsw_Pq_Qq);
/** Opcode 0x66 0x0f 0xed - vpaddsw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpaddsw_Vx_Hx_Wx);
/*  Opcode 0xf3 0x0f 0xed - invalid */
/*  Opcode 0xf2 0x0f 0xed - invalid */

/** Opcode      0x0f 0xee - pmaxsw Pq, Qq */
FNIEMOP_STUB(iemOp_pmaxsw_Pq_Qq);
/** Opcode 0x66 0x0f 0xee - vpmaxsw Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpmaxsw_Vx_Hx_W);
/*  Opcode 0xf3 0x0f 0xee - invalid */
/*  Opcode 0xf2 0x0f 0xee - invalid */


/** Opcode      0x0f 0xef - pxor Pq, Qq */
FNIEMOP_DEF(iemOp_pxor_Pq_Qq)
{
    IEMOP_MNEMONIC(pxor, "pxor");
    return FNIEMOP_CALL_1(iemOpCommonMmx_FullFull_To_Full, &g_iemAImpl_pxor);
}

/** Opcode 0x66 0x0f 0xef - vpxor Vx, Hx, Wx */
FNIEMOP_DEF(iemOp_vpxor_Vx_Hx_Wx)
{
    IEMOP_MNEMONIC(vpxor, "vpxor");
    return FNIEMOP_CALL_1(iemOpCommonSse2_FullFull_To_Full, &g_iemAImpl_pxor);
}

/*  Opcode 0xf3 0x0f 0xef - invalid */
/*  Opcode 0xf2 0x0f 0xef - invalid */

/*  Opcode      0x0f 0xf0 - invalid */
/*  Opcode 0x66 0x0f 0xf0 - invalid */
/** Opcode 0xf2 0x0f 0xf0 - vlddqu Vx, Mx */
FNIEMOP_STUB(iemOp_vlddqu_Vx_Mx);

/** Opcode      0x0f 0xf1 - psllw Pq, Qq */
FNIEMOP_STUB(iemOp_psllw_Pq_Qq);
/** Opcode 0x66 0x0f 0xf1 - vpsllw Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpsllw_Vx_Hx_W);
/*  Opcode 0xf2 0x0f 0xf1 - invalid */

/** Opcode      0x0f 0xf2 - pslld Pq, Qq */
FNIEMOP_STUB(iemOp_pslld_Pq_Qq);
/** Opcode 0x66 0x0f 0xf2 - vpslld Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpslld_Vx_Hx_Wx);
/*  Opcode 0xf2 0x0f 0xf2 - invalid */

/** Opcode      0x0f 0xf3 - psllq Pq, Qq */
FNIEMOP_STUB(iemOp_psllq_Pq_Qq);
/** Opcode 0x66 0x0f 0xf3 - vpsllq Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpsllq_Vx_Hx_Wx);
/*  Opcode 0xf2 0x0f 0xf3 - invalid */

/** Opcode      0x0f 0xf4 - pmuludq Pq, Qq */
FNIEMOP_STUB(iemOp_pmuludq_Pq_Qq);
/** Opcode 0x66 0x0f 0xf4 - vpmuludq Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpmuludq_Vx_Hx_W);
/*  Opcode 0xf2 0x0f 0xf4 - invalid */

/** Opcode      0x0f 0xf5 - pmaddwd Pq, Qq */
FNIEMOP_STUB(iemOp_pmaddwd_Pq_Qq);
/** Opcode 0x66 0x0f 0xf5 - vpmaddwd Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpmaddwd_Vx_Hx_Wx);
/*  Opcode 0xf2 0x0f 0xf5 - invalid */

/** Opcode      0x0f 0xf6 - psadbw Pq, Qq */
FNIEMOP_STUB(iemOp_psadbw_Pq_Qq);
/** Opcode 0x66 0x0f 0xf6 - vpsadbw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpsadbw_Vx_Hx_Wx);
/*  Opcode 0xf2 0x0f 0xf6 - invalid */

/** Opcode      0x0f 0xf7 - maskmovq Pq, Nq */
FNIEMOP_STUB(iemOp_maskmovq_Pq_Nq);
/** Opcode 0x66 0x0f 0xf7 - vmaskmovdqu Vdq, Udq */
FNIEMOP_STUB(iemOp_vmaskmovdqu_Vdq_Udq);
/*  Opcode 0xf2 0x0f 0xf7 - invalid */

/** Opcode      0x0f 0xf8 - psubb Pq, Qq */
FNIEMOP_STUB(iemOp_psubb_Pq_Qq);
/** Opcode 0x66 0x0f 0xf8 - vpsubb Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpsubb_Vx_Hx_W);
/*  Opcode 0xf2 0x0f 0xf8 - invalid */

/** Opcode      0x0f 0xf9 - psubw Pq, Qq */
FNIEMOP_STUB(iemOp_psubw_Pq_Qq);
/** Opcode 0x66 0x0f 0xf9 - vpsubw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpsubw_Vx_Hx_Wx);
/*  Opcode 0xf2 0x0f 0xf9 - invalid */

/** Opcode      0x0f 0xfa - psubd Pq, Qq */
FNIEMOP_STUB(iemOp_psubd_Pq_Qq);
/** Opcode 0x66 0x0f 0xfa - vpsubd Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpsubd_Vx_Hx_Wx);
/*  Opcode 0xf2 0x0f 0xfa - invalid */

/** Opcode      0x0f 0xfb - psubq Pq, Qq */
FNIEMOP_STUB(iemOp_psubq_Pq_Qq);
/** Opcode 0x66 0x0f 0xfb - vpsubq Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpsubq_Vx_Hx_W);
/*  Opcode 0xf2 0x0f 0xfb - invalid */

/** Opcode      0x0f 0xfc - paddb Pq, Qq */
FNIEMOP_STUB(iemOp_paddb_Pq_Qq);
/** Opcode 0x66 0x0f 0xfc - vpaddb Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpaddb_Vx_Hx_Wx);
/*  Opcode 0xf2 0x0f 0xfc - invalid */

/** Opcode      0x0f 0xfd - paddw Pq, Qq */
FNIEMOP_STUB(iemOp_paddw_Pq_Qq);
/** Opcode 0x66 0x0f 0xfd - vpaddw Vx, Hx, Wx */
FNIEMOP_STUB(iemOp_vpaddw_Vx_Hx_Wx);
/*  Opcode 0xf2 0x0f 0xfd - invalid */

/** Opcode      0x0f 0xfe - paddd Pq, Qq */
FNIEMOP_STUB(iemOp_paddd_Pq_Qq);
/** Opcode 0x66 0x0f 0xfe - vpaddd Vx, Hx, W */
FNIEMOP_STUB(iemOp_vpaddd_Vx_Hx_W);
/*  Opcode 0xf2 0x0f 0xfe - invalid */


/** Opcode **** 0x0f 0xff - UD0 */
FNIEMOP_DEF(iemOp_ud0)
{
    IEMOP_MNEMONIC(ud0, "ud0");
    if (pVCpu->iem.s.enmCpuVendor == CPUMCPUVENDOR_INTEL)
    {
        uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm); RT_NOREF(bRm);
#ifndef TST_IEM_CHECK_MC
        RTGCPTR      GCPtrEff;
        VBOXSTRICTRC rcStrict = iemOpHlpCalcRmEffAddr(pVCpu, bRm, 0, &GCPtrEff);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
#endif
        IEMOP_HLP_DONE_DECODING();
    }
    return IEMOP_RAISE_INVALID_OPCODE();
}



/**
 * Two byte opcode map, first byte 0x0f.
 *
 * @remarks The g_apfnVexMap1 table is currently a subset of this one, so please
 *          check if it needs updating as well when making changes.
 */
IEM_STATIC const PFNIEMOP g_apfnTwoByteMap[] =
{
    /*          no prefix,                  066h prefix                 f3h prefix,                 f2h prefix */
    /* 0x00 */  IEMOP_X4(iemOp_Grp6),
    /* 0x01 */  IEMOP_X4(iemOp_Grp7),
    /* 0x02 */  IEMOP_X4(iemOp_lar_Gv_Ew),
    /* 0x03 */  IEMOP_X4(iemOp_lsl_Gv_Ew),
    /* 0x04 */  IEMOP_X4(iemOp_Invalid),
    /* 0x05 */  IEMOP_X4(iemOp_syscall),
    /* 0x06 */  IEMOP_X4(iemOp_clts),
    /* 0x07 */  IEMOP_X4(iemOp_sysret),
    /* 0x08 */  IEMOP_X4(iemOp_invd),
    /* 0x09 */  IEMOP_X4(iemOp_wbinvd),
    /* 0x0a */  IEMOP_X4(iemOp_Invalid),
    /* 0x0b */  IEMOP_X4(iemOp_ud2),
    /* 0x0c */  IEMOP_X4(iemOp_Invalid),
    /* 0x0d */  IEMOP_X4(iemOp_nop_Ev_GrpP),
    /* 0x0e */  IEMOP_X4(iemOp_femms),
    /* 0x0f */  IEMOP_X4(iemOp_3Dnow),

    /* 0x10 */  iemOp_vmovups_Vps_Wps,      iemOp_vmovupd_Vpd_Wpd,      iemOp_vmovss_Vx_Hx_Wss,     iemOp_vmovsd_Vx_Hx_Wsd,
    /* 0x11 */  iemOp_vmovups_Wps_Vps,      iemOp_vmovupd_Wpd_Vpd,      iemOp_vmovss_Wss_Hx_Vss,    iemOp_vmovsd_Wsd_Hx_Vsd,
    /* 0x12 */  iemOp_vmovlps_Vq_Hq_Mq__vmovhlps, iemOp_vmovlpd_Vq_Hq_Mq, iemOp_vmovsldup_Vx_Wx,    iemOp_vmovddup_Vx_Wx,
    /* 0x13 */  iemOp_vmovlps_Mq_Vq,        iemOp_vmovlpd_Mq_Vq,        iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x14 */  iemOp_vunpcklps_Vx_Hx_Wx,   iemOp_vunpcklpd_Vx_Hx_Wx,   iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x15 */  iemOp_vunpckhps_Vx_Hx_Wx,   iemOp_vunpckhpd_Vx_Hx_Wx,   iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x16 */  iemOp_vmovhpsv1_Vdq_Hq_Mq__vmovlhps_Vdq_Hq_Uq, iemOp_vmovhpdv1_Vdq_Hq_Mq, iemOp_vmovshdup_Vx_Wx, iemOp_InvalidNeedRM,
    /* 0x17 */  iemOp_vmovhpsv1_Mq_Vq,      iemOp_vmovhpdv1_Mq_Vq,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x18 */  IEMOP_X4(iemOp_prefetch_Grp16),
    /* 0x19 */  IEMOP_X4(iemOp_nop_Ev),
    /* 0x1a */  IEMOP_X4(iemOp_nop_Ev),
    /* 0x1b */  IEMOP_X4(iemOp_nop_Ev),
    /* 0x1c */  IEMOP_X4(iemOp_nop_Ev),
    /* 0x1d */  IEMOP_X4(iemOp_nop_Ev),
    /* 0x1e */  IEMOP_X4(iemOp_nop_Ev),
    /* 0x1f */  IEMOP_X4(iemOp_nop_Ev),

    /* 0x20 */  iemOp_mov_Rd_Cd,            iemOp_mov_Rd_Cd,            iemOp_mov_Rd_Cd,            iemOp_mov_Rd_Cd,
    /* 0x21 */  iemOp_mov_Rd_Dd,            iemOp_mov_Rd_Dd,            iemOp_mov_Rd_Dd,            iemOp_mov_Rd_Dd,
    /* 0x22 */  iemOp_mov_Cd_Rd,            iemOp_mov_Cd_Rd,            iemOp_mov_Cd_Rd,            iemOp_mov_Cd_Rd,
    /* 0x23 */  iemOp_mov_Dd_Rd,            iemOp_mov_Dd_Rd,            iemOp_mov_Dd_Rd,            iemOp_mov_Dd_Rd,
    /* 0x24 */  iemOp_mov_Rd_Td,            iemOp_mov_Rd_Td,            iemOp_mov_Rd_Td,            iemOp_mov_Rd_Td,
    /* 0x25 */  iemOp_Invalid,              iemOp_Invalid,              iemOp_Invalid,              iemOp_Invalid,
    /* 0x26 */  iemOp_mov_Td_Rd,            iemOp_mov_Td_Rd,            iemOp_mov_Td_Rd,            iemOp_mov_Td_Rd,
    /* 0x27 */  iemOp_Invalid,              iemOp_Invalid,              iemOp_Invalid,              iemOp_Invalid,
    /* 0x28 */  iemOp_vmovaps_Vps_Wps,      iemOp_vmovapd_Vpd_Wpd,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x29 */  iemOp_vmovaps_Wps_Vps,      iemOp_vmovapd_Wpd_Vpd,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x2a */  iemOp_cvtpi2ps_Vps_Qpi,     iemOp_cvtpi2pd_Vpd_Qpi,     iemOp_vcvtsi2ss_Vss_Hss_Ey, iemOp_vcvtsi2sd_Vsd_Hsd_Ey,
    /* 0x2b */  iemOp_vmovntps_Mps_Vps,     iemOp_vmovntpd_Mpd_Vpd,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x2c */  iemOp_cvttps2pi_Ppi_Wps,    iemOp_cvttpd2pi_Ppi_Wpd,    iemOp_vcvttss2si_Gy_Wss,    iemOp_vcvttsd2si_Gy_Wsd,
    /* 0x2d */  iemOp_cvtps2pi_Ppi_Wps,     iemOp_cvtpd2pi_Qpi_Wpd,     iemOp_vcvtss2si_Gy_Wss,     iemOp_vcvtsd2si_Gy_Wsd,
    /* 0x2e */  iemOp_vucomiss_Vss_Wss,     iemOp_vucomisd_Vsd_Wsd,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x2f */  iemOp_vcomiss_Vss_Wss,      iemOp_vcomisd_Vsd_Wsd,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,

    /* 0x30 */  IEMOP_X4(iemOp_wrmsr),
    /* 0x31 */  IEMOP_X4(iemOp_rdtsc),
    /* 0x32 */  IEMOP_X4(iemOp_rdmsr),
    /* 0x33 */  IEMOP_X4(iemOp_rdpmc),
    /* 0x34 */  IEMOP_X4(iemOp_sysenter),
    /* 0x35 */  IEMOP_X4(iemOp_sysexit),
    /* 0x36 */  IEMOP_X4(iemOp_Invalid),
    /* 0x37 */  IEMOP_X4(iemOp_getsec),
    /* 0x38 */  IEMOP_X4(iemOp_3byte_Esc_A4),
    /* 0x39 */  IEMOP_X4(iemOp_InvalidNeed3ByteEscRM),
    /* 0x3a */  IEMOP_X4(iemOp_3byte_Esc_A5),
    /* 0x3b */  IEMOP_X4(iemOp_InvalidNeed3ByteEscRMImm8),
    /* 0x3c */  IEMOP_X4(iemOp_InvalidNeed3ByteEscRM),
    /* 0x3d */  IEMOP_X4(iemOp_InvalidNeed3ByteEscRM),
    /* 0x3e */  IEMOP_X4(iemOp_InvalidNeed3ByteEscRMImm8),
    /* 0x3f */  IEMOP_X4(iemOp_InvalidNeed3ByteEscRMImm8),

    /* 0x40 */  IEMOP_X4(iemOp_cmovo_Gv_Ev),
    /* 0x41 */  IEMOP_X4(iemOp_cmovno_Gv_Ev),
    /* 0x42 */  IEMOP_X4(iemOp_cmovc_Gv_Ev),
    /* 0x43 */  IEMOP_X4(iemOp_cmovnc_Gv_Ev),
    /* 0x44 */  IEMOP_X4(iemOp_cmove_Gv_Ev),
    /* 0x45 */  IEMOP_X4(iemOp_cmovne_Gv_Ev),
    /* 0x46 */  IEMOP_X4(iemOp_cmovbe_Gv_Ev),
    /* 0x47 */  IEMOP_X4(iemOp_cmovnbe_Gv_Ev),
    /* 0x48 */  IEMOP_X4(iemOp_cmovs_Gv_Ev),
    /* 0x49 */  IEMOP_X4(iemOp_cmovns_Gv_Ev),
    /* 0x4a */  IEMOP_X4(iemOp_cmovp_Gv_Ev),
    /* 0x4b */  IEMOP_X4(iemOp_cmovnp_Gv_Ev),
    /* 0x4c */  IEMOP_X4(iemOp_cmovl_Gv_Ev),
    /* 0x4d */  IEMOP_X4(iemOp_cmovnl_Gv_Ev),
    /* 0x4e */  IEMOP_X4(iemOp_cmovle_Gv_Ev),
    /* 0x4f */  IEMOP_X4(iemOp_cmovnle_Gv_Ev),

    /* 0x50 */  iemOp_vmovmskps_Gy_Ups,     iemOp_vmovmskpd_Gy_Upd,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x51 */  iemOp_vsqrtps_Vps_Wps,      iemOp_vsqrtpd_Vpd_Wpd,      iemOp_vsqrtss_Vss_Hss_Wss,  iemOp_vsqrtsd_Vsd_Hsd_Wsd,
    /* 0x52 */  iemOp_vrsqrtps_Vps_Wps,     iemOp_InvalidNeedRM,        iemOp_vrsqrtss_Vss_Hss_Wss, iemOp_InvalidNeedRM,
    /* 0x53 */  iemOp_vrcpps_Vps_Wps,       iemOp_InvalidNeedRM,        iemOp_vrcpss_Vss_Hss_Wss,   iemOp_InvalidNeedRM,
    /* 0x54 */  iemOp_vandps_Vps_Hps_Wps,   iemOp_vandpd_Vpd_Hpd_Wpd,   iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x55 */  iemOp_vandnps_Vps_Hps_Wps,  iemOp_vandnpd_Vpd_Hpd_Wpd,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x56 */  iemOp_vorps_Vps_Hps_Wps,    iemOp_vorpd_Vpd_Hpd_Wpd,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x57 */  iemOp_vxorps_Vps_Hps_Wps,   iemOp_vxorpd_Vpd_Hpd_Wpd,   iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x58 */  iemOp_vaddps_Vps_Hps_Wps,   iemOp_vaddpd_Vpd_Hpd_Wpd,   iemOp_vaddss_Vss_Hss_Wss,   iemOp_vaddsd_Vsd_Hsd_Wsd,
    /* 0x59 */  iemOp_vmulps_Vps_Hps_Wps,   iemOp_vmulpd_Vpd_Hpd_Wpd,   iemOp_vmulss_Vss_Hss_Wss,   iemOp_vmulsd_Vsd_Hsd_Wsd,
    /* 0x5a */  iemOp_vcvtps2pd_Vpd_Wps,    iemOp_vcvtpd2ps_Vps_Wpd,    iemOp_vcvtss2sd_Vsd_Hx_Wss, iemOp_vcvtsd2ss_Vss_Hx_Wsd,
    /* 0x5b */  iemOp_vcvtdq2ps_Vps_Wdq,    iemOp_vcvtps2dq_Vdq_Wps,    iemOp_vcvttps2dq_Vdq_Wps,   iemOp_InvalidNeedRM,
    /* 0x5c */  iemOp_vsubps_Vps_Hps_Wps,   iemOp_vsubpd_Vpd_Hpd_Wpd,   iemOp_vsubss_Vss_Hss_Wss,   iemOp_vsubsd_Vsd_Hsd_Wsd,
    /* 0x5d */  iemOp_vminps_Vps_Hps_Wps,   iemOp_vminpd_Vpd_Hpd_Wpd,   iemOp_vminss_Vss_Hss_Wss,   iemOp_vminsd_Vsd_Hsd_Wsd,
    /* 0x5e */  iemOp_vdivps_Vps_Hps_Wps,   iemOp_vdivpd_Vpd_Hpd_Wpd,   iemOp_vdivss_Vss_Hss_Wss,   iemOp_vdivsd_Vsd_Hsd_Wsd,
    /* 0x5f */  iemOp_vmaxps_Vps_Hps_Wps,   iemOp_vmaxpd_Vpd_Hpd_Wpd,   iemOp_vmaxss_Vss_Hss_Wss,   iemOp_vmaxsd_Vsd_Hsd_Wsd,

    /* 0x60 */  iemOp_punpcklbw_Pq_Qd,      iemOp_vpunpcklbw_Vx_Hx_Wx,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x61 */  iemOp_punpcklwd_Pq_Qd,      iemOp_vpunpcklwd_Vx_Hx_Wx,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x62 */  iemOp_punpckldq_Pq_Qd,      iemOp_vpunpckldq_Vx_Hx_Wx,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x63 */  iemOp_packsswb_Pq_Qq,       iemOp_vpacksswb_Vx_Hx_Wx,   iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x64 */  iemOp_pcmpgtb_Pq_Qq,        iemOp_vpcmpgtb_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x65 */  iemOp_pcmpgtw_Pq_Qq,        iemOp_vpcmpgtw_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x66 */  iemOp_pcmpgtd_Pq_Qq,        iemOp_vpcmpgtd_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x67 */  iemOp_packuswb_Pq_Qq,       iemOp_vpackuswb_Vx_Hx_W,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x68 */  iemOp_punpckhbw_Pq_Qd,      iemOp_vpunpckhbw_Vx_Hx_Wx,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x69 */  iemOp_punpckhwd_Pq_Qd,      iemOp_vpunpckhwd_Vx_Hx_Wx,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x6a */  iemOp_punpckhdq_Pq_Qd,      iemOp_vpunpckhdq_Vx_Hx_W,   iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x6b */  iemOp_packssdw_Pq_Qd,       iemOp_vpackssdw_Vx_Hx_Wx,   iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x6c */  iemOp_InvalidNeedRM,        iemOp_vpunpcklqdq_Vx_Hx_Wx, iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x6d */  iemOp_InvalidNeedRM,        iemOp_vpunpckhqdq_Vx_Hx_W,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x6e */  iemOp_movd_q_Pd_Ey,         iemOp_vmovd_q_Vy_Ey,        iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x6f */  iemOp_movq_Pq_Qq,           iemOp_vmovdqa_Vx_Wx,        iemOp_vmovdqu_Vx_Wx,        iemOp_InvalidNeedRM,

    /* 0x70 */  iemOp_pshufw_Pq_Qq_Ib,      iemOp_vpshufd_Vx_Wx_Ib,     iemOp_vpshufhw_Vx_Wx_Ib,    iemOp_vpshuflw_Vx_Wx_Ib,
    /* 0x71 */  IEMOP_X4(iemOp_Grp12),
    /* 0x72 */  IEMOP_X4(iemOp_Grp13),
    /* 0x73 */  IEMOP_X4(iemOp_Grp14),
    /* 0x74 */  iemOp_pcmpeqb_Pq_Qq,        iemOp_vpcmpeqb_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x75 */  iemOp_pcmpeqw_Pq_Qq,        iemOp_vpcmpeqw_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x76 */  iemOp_pcmpeqd_Pq_Qq,        iemOp_vpcmpeqd_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x77 */  iemOp_emms__vzeroupperv__vzeroallv, iemOp_InvalidNeedRM, iemOp_InvalidNeedRM,       iemOp_InvalidNeedRM,

    /* 0x78 */  iemOp_vmread_Ey_Gy,         iemOp_AmdGrp17,             iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x79 */  iemOp_vmwrite_Gy_Ey,        iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x7a */  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x7b */  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x7c */  iemOp_InvalidNeedRM,        iemOp_vhaddpd_Vpd_Hpd_Wpd,  iemOp_InvalidNeedRM,        iemOp_vhaddps_Vps_Hps_Wps,
    /* 0x7d */  iemOp_InvalidNeedRM,        iemOp_vhsubpd_Vpd_Hpd_Wpd,  iemOp_InvalidNeedRM,        iemOp_vhsubps_Vps_Hps_Wps,
    /* 0x7e */  iemOp_movd_q_Ey_Pd,         iemOp_vmovd_q_Ey_Vy,        iemOp_vmovq_Vq_Wq,          iemOp_InvalidNeedRM,
    /* 0x7f */  iemOp_movq_Qq_Pq,           iemOp_vmovdqa_Wx_Vx,        iemOp_vmovdqu_Wx_Vx,        iemOp_InvalidNeedRM,

    /* 0x80 */  IEMOP_X4(iemOp_jo_Jv),
    /* 0x81 */  IEMOP_X4(iemOp_jno_Jv),
    /* 0x82 */  IEMOP_X4(iemOp_jc_Jv),
    /* 0x83 */  IEMOP_X4(iemOp_jnc_Jv),
    /* 0x84 */  IEMOP_X4(iemOp_je_Jv),
    /* 0x85 */  IEMOP_X4(iemOp_jne_Jv),
    /* 0x86 */  IEMOP_X4(iemOp_jbe_Jv),
    /* 0x87 */  IEMOP_X4(iemOp_jnbe_Jv),
    /* 0x88 */  IEMOP_X4(iemOp_js_Jv),
    /* 0x89 */  IEMOP_X4(iemOp_jns_Jv),
    /* 0x8a */  IEMOP_X4(iemOp_jp_Jv),
    /* 0x8b */  IEMOP_X4(iemOp_jnp_Jv),
    /* 0x8c */  IEMOP_X4(iemOp_jl_Jv),
    /* 0x8d */  IEMOP_X4(iemOp_jnl_Jv),
    /* 0x8e */  IEMOP_X4(iemOp_jle_Jv),
    /* 0x8f */  IEMOP_X4(iemOp_jnle_Jv),

    /* 0x90 */  IEMOP_X4(iemOp_seto_Eb),
    /* 0x91 */  IEMOP_X4(iemOp_setno_Eb),
    /* 0x92 */  IEMOP_X4(iemOp_setc_Eb),
    /* 0x93 */  IEMOP_X4(iemOp_setnc_Eb),
    /* 0x94 */  IEMOP_X4(iemOp_sete_Eb),
    /* 0x95 */  IEMOP_X4(iemOp_setne_Eb),
    /* 0x96 */  IEMOP_X4(iemOp_setbe_Eb),
    /* 0x97 */  IEMOP_X4(iemOp_setnbe_Eb),
    /* 0x98 */  IEMOP_X4(iemOp_sets_Eb),
    /* 0x99 */  IEMOP_X4(iemOp_setns_Eb),
    /* 0x9a */  IEMOP_X4(iemOp_setp_Eb),
    /* 0x9b */  IEMOP_X4(iemOp_setnp_Eb),
    /* 0x9c */  IEMOP_X4(iemOp_setl_Eb),
    /* 0x9d */  IEMOP_X4(iemOp_setnl_Eb),
    /* 0x9e */  IEMOP_X4(iemOp_setle_Eb),
    /* 0x9f */  IEMOP_X4(iemOp_setnle_Eb),

    /* 0xa0 */  IEMOP_X4(iemOp_push_fs),
    /* 0xa1 */  IEMOP_X4(iemOp_pop_fs),
    /* 0xa2 */  IEMOP_X4(iemOp_cpuid),
    /* 0xa3 */  IEMOP_X4(iemOp_bt_Ev_Gv),
    /* 0xa4 */  IEMOP_X4(iemOp_shld_Ev_Gv_Ib),
    /* 0xa5 */  IEMOP_X4(iemOp_shld_Ev_Gv_CL),
    /* 0xa6 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xa7 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xa8 */  IEMOP_X4(iemOp_push_gs),
    /* 0xa9 */  IEMOP_X4(iemOp_pop_gs),
    /* 0xaa */  IEMOP_X4(iemOp_rsm),
    /* 0xab */  IEMOP_X4(iemOp_bts_Ev_Gv),
    /* 0xac */  IEMOP_X4(iemOp_shrd_Ev_Gv_Ib),
    /* 0xad */  IEMOP_X4(iemOp_shrd_Ev_Gv_CL),
    /* 0xae */  IEMOP_X4(iemOp_Grp15),
    /* 0xaf */  IEMOP_X4(iemOp_imul_Gv_Ev),

    /* 0xb0 */  IEMOP_X4(iemOp_cmpxchg_Eb_Gb),
    /* 0xb1 */  IEMOP_X4(iemOp_cmpxchg_Ev_Gv),
    /* 0xb2 */  IEMOP_X4(iemOp_lss_Gv_Mp),
    /* 0xb3 */  IEMOP_X4(iemOp_btr_Ev_Gv),
    /* 0xb4 */  IEMOP_X4(iemOp_lfs_Gv_Mp),
    /* 0xb5 */  IEMOP_X4(iemOp_lgs_Gv_Mp),
    /* 0xb6 */  IEMOP_X4(iemOp_movzx_Gv_Eb),
    /* 0xb7 */  IEMOP_X4(iemOp_movzx_Gv_Ew),
    /* 0xb8 */  iemOp_jmpe,                 iemOp_InvalidNeedRM,        iemOp_popcnt_Gv_Ev,         iemOp_InvalidNeedRM,
    /* 0xb9 */  IEMOP_X4(iemOp_Grp10),
    /* 0xba */  IEMOP_X4(iemOp_Grp8),
    /* 0xbb */  IEMOP_X4(iemOp_btc_Ev_Gv), // 0xf3?
    /* 0xbc */  iemOp_bsf_Gv_Ev,            iemOp_bsf_Gv_Ev,            iemOp_tzcnt_Gv_Ev,          iemOp_bsf_Gv_Ev,
    /* 0xbd */  iemOp_bsr_Gv_Ev,            iemOp_bsr_Gv_Ev,            iemOp_lzcnt_Gv_Ev,          iemOp_bsr_Gv_Ev,
    /* 0xbe */  IEMOP_X4(iemOp_movsx_Gv_Eb),
    /* 0xbf */  IEMOP_X4(iemOp_movsx_Gv_Ew),

    /* 0xc0 */  IEMOP_X4(iemOp_xadd_Eb_Gb),
    /* 0xc1 */  IEMOP_X4(iemOp_xadd_Ev_Gv),
    /* 0xc2 */  iemOp_vcmpps_Vps_Hps_Wps_Ib, iemOp_vcmppd_Vpd_Hpd_Wpd_Ib, iemOp_vcmpss_Vss_Hss_Wss_Ib, iemOp_vcmpsd_Vsd_Hsd_Wsd_Ib,
    /* 0xc3 */  iemOp_movnti_My_Gy,         iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xc4 */  iemOp_pinsrw_Pq_RyMw_Ib,    iemOp_vpinsrw_Vdq_Hdq_RyMw_Ib, iemOp_InvalidNeedRMImm8, iemOp_InvalidNeedRMImm8,
    /* 0xc5 */  iemOp_pextrw_Gd_Nq_Ib,      iemOp_vpextrw_Gd_Udq_Ib,    iemOp_InvalidNeedRMImm8,    iemOp_InvalidNeedRMImm8,
    /* 0xc6 */  iemOp_vshufps_Vps_Hps_Wps_Ib, iemOp_vshufpd_Vpd_Hpd_Wpd_Ib, iemOp_InvalidNeedRMImm8,iemOp_InvalidNeedRMImm8,
    /* 0xc7 */  IEMOP_X4(iemOp_Grp9),
    /* 0xc8 */  IEMOP_X4(iemOp_bswap_rAX_r8),
    /* 0xc9 */  IEMOP_X4(iemOp_bswap_rCX_r9),
    /* 0xca */  IEMOP_X4(iemOp_bswap_rDX_r10),
    /* 0xcb */  IEMOP_X4(iemOp_bswap_rBX_r11),
    /* 0xcc */  IEMOP_X4(iemOp_bswap_rSP_r12),
    /* 0xcd */  IEMOP_X4(iemOp_bswap_rBP_r13),
    /* 0xce */  IEMOP_X4(iemOp_bswap_rSI_r14),
    /* 0xcf */  IEMOP_X4(iemOp_bswap_rDI_r15),

    /* 0xd0 */  iemOp_InvalidNeedRM,        iemOp_vaddsubpd_Vpd_Hpd_Wpd, iemOp_InvalidNeedRM,       iemOp_vaddsubps_Vps_Hps_Wps,
    /* 0xd1 */  iemOp_psrlw_Pq_Qq,          iemOp_vpsrlw_Vx_Hx_W,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd2 */  iemOp_psrld_Pq_Qq,          iemOp_vpsrld_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd3 */  iemOp_psrlq_Pq_Qq,          iemOp_vpsrlq_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd4 */  iemOp_paddq_Pq_Qq,          iemOp_vpaddq_Vx_Hx_W,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd5 */  iemOp_pmullw_Pq_Qq,         iemOp_vpmullw_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd6 */  iemOp_InvalidNeedRM,        iemOp_vmovq_Wq_Vq,          iemOp_movq2dq_Vdq_Nq,       iemOp_movdq2q_Pq_Uq,
    /* 0xd7 */  iemOp_pmovmskb_Gd_Nq,       iemOp_vpmovmskb_Gd_Ux,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd8 */  iemOp_psubusb_Pq_Qq,        iemOp_vpsubusb_Vx_Hx_W,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd9 */  iemOp_psubusw_Pq_Qq,        iemOp_vpsubusw_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xda */  iemOp_pminub_Pq_Qq,         iemOp_vpminub_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xdb */  iemOp_pand_Pq_Qq,           iemOp_vpand_Vx_Hx_W,        iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xdc */  iemOp_paddusb_Pq_Qq,        iemOp_vpaddusb_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xdd */  iemOp_paddusw_Pq_Qq,        iemOp_vpaddusw_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xde */  iemOp_pmaxub_Pq_Qq,         iemOp_vpmaxub_Vx_Hx_W,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xdf */  iemOp_pandn_Pq_Qq,          iemOp_vpandn_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,

    /* 0xe0 */  iemOp_pavgb_Pq_Qq,          iemOp_vpavgb_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe1 */  iemOp_psraw_Pq_Qq,          iemOp_vpsraw_Vx_Hx_W,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe2 */  iemOp_psrad_Pq_Qq,          iemOp_vpsrad_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe3 */  iemOp_pavgw_Pq_Qq,          iemOp_vpavgw_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe4 */  iemOp_pmulhuw_Pq_Qq,        iemOp_vpmulhuw_Vx_Hx_W,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe5 */  iemOp_pmulhw_Pq_Qq,         iemOp_vpmulhw_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe6 */  iemOp_InvalidNeedRM,        iemOp_vcvttpd2dq_Vx_Wpd,    iemOp_vcvtdq2pd_Vx_Wpd,     iemOp_vcvtpd2dq_Vx_Wpd,
    /* 0xe7 */  iemOp_movntq_Mq_Pq,         iemOp_vmovntdq_Mx_Vx,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe8 */  iemOp_psubsb_Pq_Qq,         iemOp_vpsubsb_Vx_Hx_W,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe9 */  iemOp_psubsw_Pq_Qq,         iemOp_vpsubsw_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xea */  iemOp_pminsw_Pq_Qq,         iemOp_vpminsw_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xeb */  iemOp_por_Pq_Qq,            iemOp_vpor_Vx_Hx_W,         iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xec */  iemOp_paddsb_Pq_Qq,         iemOp_vpaddsb_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xed */  iemOp_paddsw_Pq_Qq,         iemOp_vpaddsw_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xee */  iemOp_pmaxsw_Pq_Qq,         iemOp_vpmaxsw_Vx_Hx_W,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xef */  iemOp_pxor_Pq_Qq,           iemOp_vpxor_Vx_Hx_Wx,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,

    /* 0xf0 */  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,        iemOp_vlddqu_Vx_Mx,
    /* 0xf1 */  iemOp_psllw_Pq_Qq,          iemOp_vpsllw_Vx_Hx_W,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf2 */  iemOp_pslld_Pq_Qq,          iemOp_vpslld_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf3 */  iemOp_psllq_Pq_Qq,          iemOp_vpsllq_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf4 */  iemOp_pmuludq_Pq_Qq,        iemOp_vpmuludq_Vx_Hx_W,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf5 */  iemOp_pmaddwd_Pq_Qq,        iemOp_vpmaddwd_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf6 */  iemOp_psadbw_Pq_Qq,         iemOp_vpsadbw_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf7 */  iemOp_maskmovq_Pq_Nq,       iemOp_vmaskmovdqu_Vdq_Udq,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf8 */  iemOp_psubb_Pq_Qq,          iemOp_vpsubb_Vx_Hx_W,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf9 */  iemOp_psubw_Pq_Qq,          iemOp_vpsubw_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xfa */  iemOp_psubd_Pq_Qq,          iemOp_vpsubd_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xfb */  iemOp_psubq_Pq_Qq,          iemOp_vpsubq_Vx_Hx_W,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xfc */  iemOp_paddb_Pq_Qq,          iemOp_vpaddb_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xfd */  iemOp_paddw_Pq_Qq,          iemOp_vpaddw_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xfe */  iemOp_paddd_Pq_Qq,          iemOp_vpaddd_Vx_Hx_W,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xff */  IEMOP_X4(iemOp_ud0),
};
AssertCompile(RT_ELEMENTS(g_apfnTwoByteMap) == 1024);


/**
 * VEX opcode map \#1.
 *
 * @remarks This is (currently) a subset of g_apfnTwoByteMap, so please check if
 *          it it needs updating too when making changes.
 */
IEM_STATIC const PFNIEMOP g_apfnVexMap1[] =
{
    /*          no prefix,                  066h prefix                 f3h prefix,                 f2h prefix */
    /* 0x00 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x01 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x02 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x03 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x04 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x05 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x06 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x07 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x08 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x09 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x0a */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x0b */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x0c */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x0d */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x0e */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x0f */  IEMOP_X4(iemOp_InvalidNeedRM),

    /* 0x10 */  iemOp_vmovups_Vps_Wps,      iemOp_vmovupd_Vpd_Wpd,      iemOp_vmovss_Vx_Hx_Wss,     iemOp_vmovsd_Vx_Hx_Wsd,
    /* 0x11 */  iemOp_vmovups_Wps_Vps,      iemOp_vmovupd_Wpd_Vpd,      iemOp_vmovss_Wss_Hx_Vss,    iemOp_vmovsd_Wsd_Hx_Vsd,
    /* 0x12 */  iemOp_vmovlps_Vq_Hq_Mq__vmovhlps, iemOp_vmovlpd_Vq_Hq_Mq, iemOp_vmovsldup_Vx_Wx,    iemOp_vmovddup_Vx_Wx,
    /* 0x13 */  iemOp_vmovlps_Mq_Vq,        iemOp_vmovlpd_Mq_Vq,        iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x14 */  iemOp_vunpcklps_Vx_Hx_Wx,   iemOp_vunpcklpd_Vx_Hx_Wx,   iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x15 */  iemOp_vunpckhps_Vx_Hx_Wx,   iemOp_vunpckhpd_Vx_Hx_Wx,   iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x16 */  iemOp_vmovhpsv1_Vdq_Hq_Mq__vmovlhps_Vdq_Hq_Uq, iemOp_vmovhpdv1_Vdq_Hq_Mq, iemOp_vmovshdup_Vx_Wx, iemOp_InvalidNeedRM,
    /* 0x17 */  iemOp_vmovhpsv1_Mq_Vq,      iemOp_vmovhpdv1_Mq_Vq,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x18 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x19 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x1a */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x1b */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x1c */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x1d */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x1e */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x1f */  IEMOP_X4(iemOp_InvalidNeedRM),

    /* 0x20 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x21 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x22 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x23 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x24 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x25 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x26 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x27 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x28 */  iemOp_vmovaps_Vps_Wps,      iemOp_vmovapd_Vpd_Wpd,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x29 */  iemOp_vmovaps_Wps_Vps,      iemOp_vmovapd_Wpd_Vpd,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x2a */  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,        iemOp_vcvtsi2ss_Vss_Hss_Ey, iemOp_vcvtsi2sd_Vsd_Hsd_Ey,
    /* 0x2b */  iemOp_vmovntps_Mps_Vps,     iemOp_vmovntpd_Mpd_Vpd,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x2c */  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,        iemOp_vcvttss2si_Gy_Wss,    iemOp_vcvttsd2si_Gy_Wsd,
    /* 0x2d */  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,        iemOp_vcvtss2si_Gy_Wss,     iemOp_vcvtsd2si_Gy_Wsd,
    /* 0x2e */  iemOp_vucomiss_Vss_Wss,     iemOp_vucomisd_Vsd_Wsd,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x2f */  iemOp_vcomiss_Vss_Wss,      iemOp_vcomisd_Vsd_Wsd,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,

    /* 0x30 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x31 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x32 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x33 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x34 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x35 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x36 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x37 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x38 */  IEMOP_X4(iemOp_InvalidNeedRM),  /** @todo check that there is no escape table stuff here */
    /* 0x39 */  IEMOP_X4(iemOp_InvalidNeedRM),  /** @todo check that there is no escape table stuff here */
    /* 0x3a */  IEMOP_X4(iemOp_InvalidNeedRM),  /** @todo check that there is no escape table stuff here */
    /* 0x3b */  IEMOP_X4(iemOp_InvalidNeedRM),  /** @todo check that there is no escape table stuff here */
    /* 0x3c */  IEMOP_X4(iemOp_InvalidNeedRM),  /** @todo check that there is no escape table stuff here */
    /* 0x3d */  IEMOP_X4(iemOp_InvalidNeedRM),  /** @todo check that there is no escape table stuff here */
    /* 0x3e */  IEMOP_X4(iemOp_InvalidNeedRM),  /** @todo check that there is no escape table stuff here */
    /* 0x3f */  IEMOP_X4(iemOp_InvalidNeedRM),  /** @todo check that there is no escape table stuff here */

    /* 0x40 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x41 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x42 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x43 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x44 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x45 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x46 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x47 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x48 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x49 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x4a */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x4b */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x4c */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x4d */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x4e */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x4f */  IEMOP_X4(iemOp_InvalidNeedRM),

    /* 0x50 */  iemOp_vmovmskps_Gy_Ups,     iemOp_vmovmskpd_Gy_Upd,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x51 */  iemOp_vsqrtps_Vps_Wps,      iemOp_vsqrtpd_Vpd_Wpd,      iemOp_vsqrtss_Vss_Hss_Wss,  iemOp_vsqrtsd_Vsd_Hsd_Wsd,
    /* 0x52 */  iemOp_vrsqrtps_Vps_Wps,     iemOp_InvalidNeedRM,        iemOp_vrsqrtss_Vss_Hss_Wss, iemOp_InvalidNeedRM,
    /* 0x53 */  iemOp_vrcpps_Vps_Wps,       iemOp_InvalidNeedRM,        iemOp_vrcpss_Vss_Hss_Wss,   iemOp_InvalidNeedRM,
    /* 0x54 */  iemOp_vandps_Vps_Hps_Wps,   iemOp_vandpd_Vpd_Hpd_Wpd,   iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x55 */  iemOp_vandnps_Vps_Hps_Wps,  iemOp_vandnpd_Vpd_Hpd_Wpd,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x56 */  iemOp_vorps_Vps_Hps_Wps,    iemOp_vorpd_Vpd_Hpd_Wpd,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x57 */  iemOp_vxorps_Vps_Hps_Wps,   iemOp_vxorpd_Vpd_Hpd_Wpd,   iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x58 */  iemOp_vaddps_Vps_Hps_Wps,   iemOp_vaddpd_Vpd_Hpd_Wpd,   iemOp_vaddss_Vss_Hss_Wss,   iemOp_vaddsd_Vsd_Hsd_Wsd,
    /* 0x59 */  iemOp_vmulps_Vps_Hps_Wps,   iemOp_vmulpd_Vpd_Hpd_Wpd,   iemOp_vmulss_Vss_Hss_Wss,   iemOp_vmulsd_Vsd_Hsd_Wsd,
    /* 0x5a */  iemOp_vcvtps2pd_Vpd_Wps,    iemOp_vcvtpd2ps_Vps_Wpd,    iemOp_vcvtss2sd_Vsd_Hx_Wss, iemOp_vcvtsd2ss_Vss_Hx_Wsd,
    /* 0x5b */  iemOp_vcvtdq2ps_Vps_Wdq,    iemOp_vcvtps2dq_Vdq_Wps,    iemOp_vcvttps2dq_Vdq_Wps,   iemOp_InvalidNeedRM,
    /* 0x5c */  iemOp_vsubps_Vps_Hps_Wps,   iemOp_vsubpd_Vpd_Hpd_Wpd,   iemOp_vsubss_Vss_Hss_Wss,   iemOp_vsubsd_Vsd_Hsd_Wsd,
    /* 0x5d */  iemOp_vminps_Vps_Hps_Wps,   iemOp_vminpd_Vpd_Hpd_Wpd,   iemOp_vminss_Vss_Hss_Wss,   iemOp_vminsd_Vsd_Hsd_Wsd,
    /* 0x5e */  iemOp_vdivps_Vps_Hps_Wps,   iemOp_vdivpd_Vpd_Hpd_Wpd,   iemOp_vdivss_Vss_Hss_Wss,   iemOp_vdivsd_Vsd_Hsd_Wsd,
    /* 0x5f */  iemOp_vmaxps_Vps_Hps_Wps,   iemOp_vmaxpd_Vpd_Hpd_Wpd,   iemOp_vmaxss_Vss_Hss_Wss,   iemOp_vmaxsd_Vsd_Hsd_Wsd,

    /* 0x60 */  iemOp_InvalidNeedRM,        iemOp_vpunpcklbw_Vx_Hx_Wx,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x61 */  iemOp_InvalidNeedRM,        iemOp_vpunpcklwd_Vx_Hx_Wx,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x62 */  iemOp_InvalidNeedRM,        iemOp_vpunpckldq_Vx_Hx_Wx,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x63 */  iemOp_InvalidNeedRM,        iemOp_vpacksswb_Vx_Hx_Wx,   iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x64 */  iemOp_InvalidNeedRM,        iemOp_vpcmpgtb_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x65 */  iemOp_InvalidNeedRM,        iemOp_vpcmpgtw_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x66 */  iemOp_InvalidNeedRM,        iemOp_vpcmpgtd_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x67 */  iemOp_InvalidNeedRM,        iemOp_vpackuswb_Vx_Hx_W,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x68 */  iemOp_InvalidNeedRM,        iemOp_vpunpckhbw_Vx_Hx_Wx,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x69 */  iemOp_InvalidNeedRM,        iemOp_vpunpckhwd_Vx_Hx_Wx,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x6a */  iemOp_InvalidNeedRM,        iemOp_vpunpckhdq_Vx_Hx_W,   iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x6b */  iemOp_InvalidNeedRM,        iemOp_vpackssdw_Vx_Hx_Wx,   iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x6c */  iemOp_InvalidNeedRM,        iemOp_vpunpcklqdq_Vx_Hx_Wx, iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x6d */  iemOp_InvalidNeedRM,        iemOp_vpunpckhqdq_Vx_Hx_W,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x6e */  iemOp_InvalidNeedRM,        iemOp_vmovd_q_Vy_Ey,        iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x6f */  iemOp_InvalidNeedRM,        iemOp_vmovdqa_Vx_Wx,        iemOp_vmovdqu_Vx_Wx,        iemOp_InvalidNeedRM,

    /* 0x70 */  iemOp_InvalidNeedRM,        iemOp_vpshufd_Vx_Wx_Ib,     iemOp_vpshufhw_Vx_Wx_Ib,    iemOp_vpshuflw_Vx_Wx_Ib,
    /* 0x71 */  iemOp_InvalidNeedRM,        iemOp_Grp12,                iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x72 */  iemOp_InvalidNeedRM,        iemOp_Grp13,                iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x73 */  iemOp_InvalidNeedRM,        iemOp_Grp14,                iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x74 */  iemOp_pcmpeqb_Pq_Qq,        iemOp_vpcmpeqb_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x75 */  iemOp_pcmpeqw_Pq_Qq,        iemOp_vpcmpeqw_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x76 */  iemOp_pcmpeqd_Pq_Qq,        iemOp_vpcmpeqd_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0x77 */  iemOp_emms__vzeroupperv__vzeroallv, iemOp_InvalidNeedRM, iemOp_InvalidNeedRM,       iemOp_InvalidNeedRM,
    /* 0x78 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x79 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x7a */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x7b */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x7c */  iemOp_InvalidNeedRM,        iemOp_vhaddpd_Vpd_Hpd_Wpd,  iemOp_InvalidNeedRM,        iemOp_vhaddps_Vps_Hps_Wps,
    /* 0x7d */  iemOp_InvalidNeedRM,        iemOp_vhsubpd_Vpd_Hpd_Wpd,  iemOp_InvalidNeedRM,        iemOp_vhsubps_Vps_Hps_Wps,
    /* 0x7e */  iemOp_InvalidNeedRM,        iemOp_vmovd_q_Ey_Vy,        iemOp_vmovq_Vq_Wq,          iemOp_InvalidNeedRM,
    /* 0x7f */  iemOp_InvalidNeedRM,        iemOp_vmovdqa_Wx_Vx,        iemOp_vmovdqu_Wx_Vx,        iemOp_InvalidNeedRM,

    /* 0x80 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x81 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x82 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x83 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x84 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x85 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x86 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x87 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x88 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x89 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x8a */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x8b */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x8c */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x8d */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x8e */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x8f */  IEMOP_X4(iemOp_InvalidNeedRM),
                IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x90 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x91 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x92 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x93 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x94 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x95 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x96 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x97 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x98 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x99 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x9a */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x9b */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x9c */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x9d */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x9e */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0x9f */  IEMOP_X4(iemOp_InvalidNeedRM),

    /* 0xa0 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xa1 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xa2 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xa3 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xa4 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xa5 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xa6 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xa7 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xa8 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xa9 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xaa */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xab */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xac */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xad */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xae */  IEMOP_X4(iemOp_Grp15), /** @todo groups and vex */
    /* 0xaf */  IEMOP_X4(iemOp_InvalidNeedRM),

    /* 0xb0 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xb1 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xb2 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xb3 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xb4 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xb5 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xb6 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xb7 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xb8 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xb9 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xba */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xbb */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xbc */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xbd */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xbe */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xbf */  IEMOP_X4(iemOp_InvalidNeedRM),

    /* 0xc0 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xc1 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xc2 */  iemOp_vcmpps_Vps_Hps_Wps_Ib, iemOp_vcmppd_Vpd_Hpd_Wpd_Ib, iemOp_vcmpss_Vss_Hss_Wss_Ib, iemOp_vcmpsd_Vsd_Hsd_Wsd_Ib,
    /* 0xc3 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xc4 */  iemOp_InvalidNeedRM,        iemOp_vpinsrw_Vdq_Hdq_RyMw_Ib, iemOp_InvalidNeedRMImm8, iemOp_InvalidNeedRMImm8,
    /* 0xc5 */  iemOp_InvalidNeedRM,        iemOp_vpextrw_Gd_Udq_Ib,       iemOp_InvalidNeedRMImm8,    iemOp_InvalidNeedRMImm8,
    /* 0xc6 */  iemOp_vshufps_Vps_Hps_Wps_Ib, iemOp_vshufpd_Vpd_Hpd_Wpd_Ib, iemOp_InvalidNeedRMImm8,iemOp_InvalidNeedRMImm8,
    /* 0xc7 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xc8 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xc9 */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xca */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xcb */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xcc */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xcd */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xce */  IEMOP_X4(iemOp_InvalidNeedRM),
    /* 0xcf */  IEMOP_X4(iemOp_InvalidNeedRM),

    /* 0xd0 */  iemOp_InvalidNeedRM,        iemOp_vaddsubpd_Vpd_Hpd_Wpd, iemOp_InvalidNeedRM,       iemOp_vaddsubps_Vps_Hps_Wps,
    /* 0xd1 */  iemOp_InvalidNeedRM,        iemOp_vpsrlw_Vx_Hx_W,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd2 */  iemOp_InvalidNeedRM,        iemOp_vpsrld_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd3 */  iemOp_InvalidNeedRM,        iemOp_vpsrlq_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd4 */  iemOp_InvalidNeedRM,        iemOp_vpaddq_Vx_Hx_W,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd5 */  iemOp_InvalidNeedRM,        iemOp_vpmullw_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd6 */  iemOp_InvalidNeedRM,        iemOp_vmovq_Wq_Vq,          iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd7 */  iemOp_InvalidNeedRM,        iemOp_vpmovmskb_Gd_Ux,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd8 */  iemOp_InvalidNeedRM,        iemOp_vpsubusb_Vx_Hx_W,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xd9 */  iemOp_InvalidNeedRM,        iemOp_vpsubusw_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xda */  iemOp_InvalidNeedRM,        iemOp_vpminub_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xdb */  iemOp_InvalidNeedRM,        iemOp_vpand_Vx_Hx_W,        iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xdc */  iemOp_InvalidNeedRM,        iemOp_vpaddusb_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xdd */  iemOp_InvalidNeedRM,        iemOp_vpaddusw_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xde */  iemOp_InvalidNeedRM,        iemOp_vpmaxub_Vx_Hx_W,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xdf */  iemOp_InvalidNeedRM,        iemOp_vpandn_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,

    /* 0xe0 */  iemOp_InvalidNeedRM,        iemOp_vpavgb_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe1 */  iemOp_InvalidNeedRM,        iemOp_vpsraw_Vx_Hx_W,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe2 */  iemOp_InvalidNeedRM,        iemOp_vpsrad_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe3 */  iemOp_InvalidNeedRM,        iemOp_vpavgw_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe4 */  iemOp_InvalidNeedRM,        iemOp_vpmulhuw_Vx_Hx_W,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe5 */  iemOp_InvalidNeedRM,        iemOp_vpmulhw_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe6 */  iemOp_InvalidNeedRM,        iemOp_vcvttpd2dq_Vx_Wpd,    iemOp_vcvtdq2pd_Vx_Wpd,     iemOp_vcvtpd2dq_Vx_Wpd,
    /* 0xe7 */  iemOp_InvalidNeedRM,        iemOp_vmovntdq_Mx_Vx,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe8 */  iemOp_InvalidNeedRM,        iemOp_vpsubsb_Vx_Hx_W,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xe9 */  iemOp_InvalidNeedRM,        iemOp_vpsubsw_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xea */  iemOp_InvalidNeedRM,        iemOp_vpminsw_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xeb */  iemOp_InvalidNeedRM,        iemOp_vpor_Vx_Hx_W,         iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xec */  iemOp_InvalidNeedRM,        iemOp_vpaddsb_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xed */  iemOp_InvalidNeedRM,        iemOp_vpaddsw_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xee */  iemOp_InvalidNeedRM,        iemOp_vpmaxsw_Vx_Hx_W,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xef */  iemOp_InvalidNeedRM,        iemOp_vpxor_Vx_Hx_Wx,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,

    /* 0xf0 */  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,        iemOp_vlddqu_Vx_Mx,
    /* 0xf1 */  iemOp_InvalidNeedRM,        iemOp_vpsllw_Vx_Hx_W,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf2 */  iemOp_InvalidNeedRM,        iemOp_vpslld_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf3 */  iemOp_InvalidNeedRM,        iemOp_vpsllq_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf4 */  iemOp_InvalidNeedRM,        iemOp_vpmuludq_Vx_Hx_W,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf5 */  iemOp_InvalidNeedRM,        iemOp_vpmaddwd_Vx_Hx_Wx,    iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf6 */  iemOp_InvalidNeedRM,        iemOp_vpsadbw_Vx_Hx_Wx,     iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf7 */  iemOp_InvalidNeedRM,        iemOp_vmaskmovdqu_Vdq_Udq,  iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf8 */  iemOp_InvalidNeedRM,        iemOp_vpsubb_Vx_Hx_W,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xf9 */  iemOp_InvalidNeedRM,        iemOp_vpsubw_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xfa */  iemOp_InvalidNeedRM,        iemOp_vpsubd_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xfb */  iemOp_InvalidNeedRM,        iemOp_vpsubq_Vx_Hx_W,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xfc */  iemOp_InvalidNeedRM,        iemOp_vpaddb_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xfd */  iemOp_InvalidNeedRM,        iemOp_vpaddw_Vx_Hx_Wx,      iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xfe */  iemOp_InvalidNeedRM,        iemOp_vpaddd_Vx_Hx_W,       iemOp_InvalidNeedRM,        iemOp_InvalidNeedRM,
    /* 0xff */  IEMOP_X4(iemOp_ud0),
};
AssertCompile(RT_ELEMENTS(g_apfnTwoByteMap) == 1024);
/** @}  */


