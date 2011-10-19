/* $Id$ */
/** @file
 * CPUM - CPU Monitor / Manager, Debugger & Debugging APIs.
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/pdmapi.h>
#include "CPUMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/thread.h>
#include <iprt/uint128.h>


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegGet_Generic(void *pvUser, PCDBGFREGDESC pDesc, PDBGFREGVAL pValue)
{
    PVMCPU      pVCpu   = (PVMCPU)pvUser;
    void const *pv      = (uint8_t const *)&pVCpu->cpum + pDesc->offRegister;

    VMCPU_ASSERT_EMT(pVCpu);

    switch (pDesc->enmType)
    {
        case DBGFREGVALTYPE_U8:        pValue->u8   = *(uint8_t  const *)pv; return VINF_SUCCESS;
        case DBGFREGVALTYPE_U16:       pValue->u16  = *(uint16_t const *)pv; return VINF_SUCCESS;
        case DBGFREGVALTYPE_U32:       pValue->u32  = *(uint32_t const *)pv; return VINF_SUCCESS;
        case DBGFREGVALTYPE_U64:       pValue->u64  = *(uint64_t const *)pv; return VINF_SUCCESS;
        case DBGFREGVALTYPE_U128:      pValue->u128 = *(PCRTUINT128U    )pv; return VINF_SUCCESS;
        default:
            AssertMsgFailedReturn(("%d %s\n", pDesc->enmType, pDesc->pszName), VERR_INTERNAL_ERROR_3);
    }
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegSet_Generic(void *pvUser, PCDBGFREGDESC pDesc, PCDBGFREGVAL pValue, PCDBGFREGVAL pfMask)
{
    PVMCPU      pVCpu = (PVMCPU)pvUser;
    void       *pv    = (uint8_t *)&pVCpu->cpum + pDesc->offRegister;

    VMCPU_ASSERT_EMT(pVCpu);

    switch (pDesc->enmType)
    {
        case DBGFREGVALTYPE_U8:
            *(uint8_t *)pv &= ~pfMask->u8;
            *(uint8_t *)pv |= pValue->u8 & pfMask->u8;
            return VINF_SUCCESS;

        case DBGFREGVALTYPE_U16:
            *(uint16_t *)pv &= ~pfMask->u16;
            *(uint16_t *)pv |= pValue->u16 & pfMask->u16;
            return VINF_SUCCESS;

        case DBGFREGVALTYPE_U32:
            *(uint32_t *)pv &= ~pfMask->u32;
            *(uint32_t *)pv |= pValue->u32 & pfMask->u32;
            return VINF_SUCCESS;

        case DBGFREGVALTYPE_U64:
            *(uint64_t *)pv &= ~pfMask->u64;
            *(uint64_t *)pv |= pValue->u64 & pfMask->u64;
            return VINF_SUCCESS;

        case DBGFREGVALTYPE_U128:
        {
            RTUINT128U Val;
            RTUInt128AssignAnd((PRTUINT128U)pv, RTUInt128AssignBitwiseNot(RTUInt128Assign(&Val, &pfMask->u128)));
            RTUInt128AssignOr((PRTUINT128U)pv, RTUInt128AssignAnd(RTUInt128Assign(&Val, &pValue->u128), &pfMask->u128));
            return VINF_SUCCESS;
        }

        default:
            AssertMsgFailedReturn(("%d %s\n", pDesc->enmType, pDesc->pszName), VERR_INTERNAL_ERROR_3);
    }
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegSet_seg(void *pvUser, PCDBGFREGDESC pDesc, PCDBGFREGVAL pValue, PCDBGFREGVAL pfMask)
{
    /** @todo perform a selector load, updating hidden selectors and stuff. */
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegGet_gdtr(void *pvUser, PCDBGFREGDESC pDesc, PDBGFREGVAL pValue)
{
    PVMCPU          pVCpu = (PVMCPU)pvUser;
    VBOXGDTR const *pGdtr = (VBOXGDTR const *)((uint8_t const *)&pVCpu->cpum + pDesc->offRegister);

    VMCPU_ASSERT_EMT(pVCpu);
    Assert(pDesc->enmType == DBGFREGVALTYPE_DTR);

    pValue->dtr.u32Limit  = pGdtr->cbGdt;
    pValue->dtr.u64Base   = pGdtr->pGdt;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegSet_gdtr(void *pvUser, PCDBGFREGDESC pDesc, PCDBGFREGVAL pValue, PCDBGFREGVAL pfMask)
{
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegGet_idtr(void *pvUser, PCDBGFREGDESC pDesc, PDBGFREGVAL pValue)
{
    PVMCPU          pVCpu = (PVMCPU)pvUser;
    VBOXIDTR const *pIdtr = (VBOXIDTR const *)((uint8_t const *)&pVCpu->cpum + pDesc->offRegister);

    VMCPU_ASSERT_EMT(pVCpu);
    Assert(pDesc->enmType == DBGFREGVALTYPE_DTR);

    pValue->dtr.u32Limit  = pIdtr->cbIdt;
    pValue->dtr.u64Base   = pIdtr->pIdt;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegSet_idtr(void *pvUser, PCDBGFREGDESC pDesc, PCDBGFREGVAL pValue, PCDBGFREGVAL pfMask)
{
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Is the FPU state in FXSAVE format or not.
 *
 * @returns true if it is, false if it's in FNSAVE.
 * @param   pVCpu               The virtual CPU handle.
 */
DECLINLINE(bool) cpumR3RegIsFxSaveFormat(PVMCPU pVCpu)
{
#ifdef RT_ARCH_AMD64
    return true;
#else
    return pVCpu->pVMR3->cpum.s.CPUFeatures.edx.u1FXSR;
#endif
}


/**
 * Determins the tag register value for a CPU register when the FPU state
 * format is FXSAVE.
 *
 * @returns The tag register value.
 * @param   pVCpu               The virtual CPU handle.
 * @param   iReg                The register number (0..7).
 */
DECLINLINE(uint16_t) cpumR3RegCalcFpuTagFromFxSave(PCX86FXSTATE pFpu, unsigned iReg)
{
    /*
     * See table 11-1 in the AMD docs.
     */
    if (!(pFpu->FTW & RT_BIT_32(iReg)))
        return 3; /* b11 - empty */

    uint16_t const uExp  = pFpu->aRegs[iReg].au16[4];
    if (uExp == 0)
    {
        if (pFpu->aRegs[iReg].au64[0] == 0) /* J & M == 0 */
            return 1; /* b01 - zero */
        return 2; /* b10 - special */
    }

    if (uExp == UINT16_C(0xffff))
        return 2; /* b10 - special */

    if (!(pFpu->aRegs[iReg].au64[0] >> 63)) /* J == 0 */
        return 2; /* b10 - special */

    return 0; /* b00 - valid (normal) */
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegGet_ftw(void *pvUser, PCDBGFREGDESC pDesc, PDBGFREGVAL pValue)
{
    PVMCPU          pVCpu   = (PVMCPU)pvUser;
    PCX86FXSTATE    pFpu    = (PCX86FXSTATE)((uint8_t const *)&pVCpu->cpum + pDesc->offRegister);

    VMCPU_ASSERT_EMT(pVCpu);
    Assert(pDesc->enmType == DBGFREGVALTYPE_U16);

    if (cpumR3RegIsFxSaveFormat(pVCpu))
        pValue->u16 =  cpumR3RegCalcFpuTagFromFxSave(pFpu, 0)
                    | (cpumR3RegCalcFpuTagFromFxSave(pFpu, 1) <<  2)
                    | (cpumR3RegCalcFpuTagFromFxSave(pFpu, 2) <<  4)
                    | (cpumR3RegCalcFpuTagFromFxSave(pFpu, 3) <<  6)
                    | (cpumR3RegCalcFpuTagFromFxSave(pFpu, 4) <<  8)
                    | (cpumR3RegCalcFpuTagFromFxSave(pFpu, 5) << 10)
                    | (cpumR3RegCalcFpuTagFromFxSave(pFpu, 6) << 12)
                    | (cpumR3RegCalcFpuTagFromFxSave(pFpu, 7) << 14);
    else
    {
        PCX86FPUSTATE pOldFpu = (PCX86FPUSTATE)pFpu;
        pValue->u16 = pOldFpu->FTW;
    }
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegSet_ftw(void *pvUser, PCDBGFREGDESC pDesc, PCDBGFREGVAL pValue, PCDBGFREGVAL pfMask)
{
    return VERR_DBGF_READ_ONLY_REGISTER;
}



/*
 *
 * Guest register access functions.
 *
 */

/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegGstGet_crX(void *pvUser, PCDBGFREGDESC pDesc, PDBGFREGVAL pValue)
{
    PVMCPU pVCpu = (PVMCPU)pvUser;
    VMCPU_ASSERT_EMT(pVCpu);

    uint64_t u64Value;
    int rc = CPUMGetGuestCRx(pVCpu, pDesc->offRegister, &u64Value);
    AssertRCReturn(rc, rc);
    switch (pDesc->enmType)
    {
        case DBGFREGVALTYPE_U64:    pValue->u64 = u64Value; break;
        case DBGFREGVALTYPE_U32:    pValue->u32 = (uint32_t)u64Value; break;
        default:
            AssertFailedReturn(VERR_INTERNAL_ERROR_4);
    }
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegGstSet_crX(void *pvUser, PCDBGFREGDESC pDesc, PCDBGFREGVAL pValue, PCDBGFREGVAL pfMask)
{
    int         rc;
    PVMCPU      pVCpu = (PVMCPU)pvUser;

    VMCPU_ASSERT_EMT(pVCpu);

    /*
     * Calculate the new value.
     */
    uint64_t u64Value;
    uint64_t fMask;
    uint64_t fMaskMax;
    switch (pDesc->enmType)
    {
        case DBGFREGVALTYPE_U64:
            u64Value = pValue->u64;
            fMask    = pfMask->u64;
            fMaskMax = UINT64_MAX;
            break;
        case DBGFREGVALTYPE_U32:
            u64Value = pValue->u32;
            fMask    = pfMask->u32;
            fMaskMax = UINT32_MAX;
            break;
        default:                    AssertFailedReturn(VERR_INTERNAL_ERROR_4);
    }
    if (fMask != fMaskMax)
    {
        uint64_t u64FullValue;
        rc = CPUMGetGuestCRx(pVCpu, pDesc->offRegister, &u64FullValue);
        if (RT_FAILURE(rc))
            return rc;
        u64Value = (u64FullValue & ~fMask)
                 | (u64Value     &  fMask);
    }

    /*
     * Perform the assignment.
     */
    switch (pDesc->offRegister)
    {
        case 0: rc = CPUMSetGuestCR0(pVCpu, u64Value); break;
        case 2: rc = CPUMSetGuestCR2(pVCpu, u64Value); break;
        case 3: rc = CPUMSetGuestCR3(pVCpu, u64Value); break;
        case 4: rc = CPUMSetGuestCR4(pVCpu, u64Value); break;
        case 8: rc = PDMApicSetTPR(pVCpu, (uint8_t)(u64Value << 4)); break;
        default:
            AssertFailedReturn(VERR_INTERNAL_ERROR_2);
    }
    return rc;
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegGstGet_drX(void *pvUser, PCDBGFREGDESC pDesc, PDBGFREGVAL pValue)
{
    PVMCPU pVCpu = (PVMCPU)pvUser;
    VMCPU_ASSERT_EMT(pVCpu);

    uint64_t u64Value;
    int rc = CPUMGetGuestDRx(pVCpu, pDesc->offRegister, &u64Value);
    AssertRCReturn(rc, rc);
    switch (pDesc->enmType)
    {
        case DBGFREGVALTYPE_U64:    pValue->u64 = u64Value; break;
        case DBGFREGVALTYPE_U32:    pValue->u32 = (uint32_t)u64Value; break;
        default:
            AssertFailedReturn(VERR_INTERNAL_ERROR_4);
    }
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegGstSet_drX(void *pvUser, PCDBGFREGDESC pDesc, PCDBGFREGVAL pValue, PCDBGFREGVAL pfMask)
{
    int         rc;
    PVMCPU      pVCpu = (PVMCPU)pvUser;

    VMCPU_ASSERT_EMT(pVCpu);

    /*
     * Calculate the new value.
     */
    uint64_t u64Value;
    uint64_t fMask;
    uint64_t fMaskMax;
    switch (pDesc->enmType)
    {
        case DBGFREGVALTYPE_U64:
            u64Value = pValue->u64;
            fMask    = pfMask->u64;
            fMaskMax = UINT64_MAX;
            break;
        case DBGFREGVALTYPE_U32:
            u64Value = pValue->u32;
            fMask    = pfMask->u32;
            fMaskMax = UINT32_MAX;
            break;
        default:                    AssertFailedReturn(VERR_INTERNAL_ERROR_4);
    }
    if (fMask != fMaskMax)
    {
        uint64_t u64FullValue;
        rc = CPUMGetGuestDRx(pVCpu, pDesc->offRegister, &u64FullValue);
        if (RT_FAILURE(rc))
            return rc;
        u64Value = (u64FullValue & ~fMask)
                 | (u64Value     &  fMask);
    }

    /*
     * Perform the assignment.
     */
    return CPUMSetGuestDRx(pVCpu, pDesc->offRegister, u64Value);
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegGstGet_msr(void *pvUser, PCDBGFREGDESC pDesc, PDBGFREGVAL pValue)
{
    PVMCPU pVCpu = (PVMCPU)pvUser;
    VMCPU_ASSERT_EMT(pVCpu);

    uint64_t u64Value;
    int rc = CPUMQueryGuestMsr(pVCpu, pDesc->offRegister, &u64Value);
    if (RT_SUCCESS(rc))
    {
        switch (pDesc->enmType)
        {
            case DBGFREGVALTYPE_U64:    pValue->u64 = u64Value; break;
            case DBGFREGVALTYPE_U32:    pValue->u32 = (uint32_t)u64Value; break;
            case DBGFREGVALTYPE_U16:    pValue->u16 = (uint16_t)u64Value; break;
            default:
                AssertFailedReturn(VERR_INTERNAL_ERROR_4);
        }
    }
    /** @todo what to do about errors? */
    return rc;
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegGstSet_msr(void *pvUser, PCDBGFREGDESC pDesc, PCDBGFREGVAL pValue, PCDBGFREGVAL pfMask)
{
    int         rc;
    PVMCPU      pVCpu   = (PVMCPU)pvUser;

    VMCPU_ASSERT_EMT(pVCpu);

    /*
     * Calculate the new value.
     */
    uint64_t u64Value;
    uint64_t fMask;
    uint64_t fMaskMax;
    switch (pDesc->enmType)
    {
        case DBGFREGVALTYPE_U64:
            u64Value = pValue->u64;
            fMask    = pfMask->u64;
            fMaskMax = UINT64_MAX;
            break;
        case DBGFREGVALTYPE_U32:
            u64Value = pValue->u32;
            fMask    = pfMask->u32;
            fMaskMax = UINT32_MAX;
            break;
        case DBGFREGVALTYPE_U16:
            u64Value = pValue->u16;
            fMask    = pfMask->u16;
            fMaskMax = UINT16_MAX;
            break;
        default:                    AssertFailedReturn(VERR_INTERNAL_ERROR_4);
    }
    if (fMask != fMaskMax)
    {
        uint64_t u64FullValue;
        rc = CPUMQueryGuestMsr(pVCpu, pDesc->offRegister, &u64FullValue);
        if (RT_FAILURE(rc))
            return rc;
        u64Value = (u64FullValue & ~fMask)
                 | (u64Value     &  fMask);
    }

    /*
     * Perform the assignment.
     */
    return CPUMSetGuestMsr(pVCpu, pDesc->offRegister, u64Value);
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegGstGet_stN(void *pvUser, PCDBGFREGDESC pDesc, PDBGFREGVAL pValue)
{
    PVMCPU pVCpu = (PVMCPU)pvUser;
    VMCPU_ASSERT_EMT(pVCpu);
    Assert(pDesc->enmType == DBGFREGVALTYPE_R80);

    if (cpumR3RegIsFxSaveFormat(pVCpu))
    {
        unsigned iReg = (pVCpu->cpum.s.Guest.fpu.FSW >> 11) & 7;
        iReg += pDesc->offRegister;
        iReg &= 7;
        pValue->r80 = pVCpu->cpum.s.Guest.fpu.aRegs[iReg].r80;
    }
    else
    {
        PCX86FPUSTATE pOldFpu = (PCX86FPUSTATE)&pVCpu->cpum.s.Guest.fpu;

        unsigned iReg = (pOldFpu->FSW >> 11) & 7;
        iReg += pDesc->offRegister;
        iReg &= 7;

        pValue->r80 = pOldFpu->regs[iReg].r80;
    }

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegGstSet_stN(void *pvUser, PCDBGFREGDESC pDesc, PCDBGFREGVAL pValue, PCDBGFREGVAL pfMask)
{
    return VERR_NOT_IMPLEMENTED;
}



/*
 *
 * Hypervisor register access functions.
 *
 */

/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegHyperGet_crX(void *pvUser, PCDBGFREGDESC pDesc, PDBGFREGVAL pValue)
{
    PVMCPU pVCpu = (PVMCPU)pvUser;
    VMCPU_ASSERT_EMT(pVCpu);

    uint64_t u64Value;
    switch (pDesc->offRegister)
    {
        case 0: u64Value = UINT64_MAX; break;
        case 2: u64Value = UINT64_MAX; break;
        case 3: u64Value = CPUMGetHyperCR3(pVCpu); break;
        case 4: u64Value = UINT64_MAX; break;
        case 8: u64Value = UINT64_MAX; break;
        default:
            AssertFailedReturn(VERR_INTERNAL_ERROR_3);
    }
    switch (pDesc->enmType)
    {
        case DBGFREGVALTYPE_U64:    pValue->u64 = u64Value; break;
        case DBGFREGVALTYPE_U32:    pValue->u32 = (uint32_t)u64Value; break;
        default:
            AssertFailedReturn(VERR_INTERNAL_ERROR_4);
    }
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegHyperSet_crX(void *pvUser, PCDBGFREGDESC pDesc, PCDBGFREGVAL pValue, PCDBGFREGVAL pfMask)
{
    /* Not settable, prevents killing your host. */
    return VERR_ACCESS_DENIED;
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegHyperGet_drX(void *pvUser, PCDBGFREGDESC pDesc, PDBGFREGVAL pValue)
{
    PVMCPU pVCpu = (PVMCPU)pvUser;
    VMCPU_ASSERT_EMT(pVCpu);

    uint64_t    u64Value;
    switch (pDesc->offRegister)
    {
        case 0: u64Value = CPUMGetHyperDR0(pVCpu); break;
        case 1: u64Value = CPUMGetHyperDR1(pVCpu); break;
        case 2: u64Value = CPUMGetHyperDR2(pVCpu); break;
        case 3: u64Value = CPUMGetHyperDR3(pVCpu); break;
        case 6: u64Value = CPUMGetHyperDR6(pVCpu); break;
        case 7: u64Value = CPUMGetHyperDR7(pVCpu); break;
        default:
            AssertFailedReturn(VERR_INTERNAL_ERROR_3);
    }
    switch (pDesc->enmType)
    {
        case DBGFREGVALTYPE_U64:    pValue->u64 = u64Value; break;
        case DBGFREGVALTYPE_U32:    pValue->u32 = (uint32_t)u64Value; break;
        default:
            AssertFailedReturn(VERR_INTERNAL_ERROR_4);
    }
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegHyperSet_drX(void *pvUser, PCDBGFREGDESC pDesc, PCDBGFREGVAL pValue, PCDBGFREGVAL pfMask)
{
    /* Not settable, prevents killing your host. */
    return VERR_ACCESS_DENIED;
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegHyperGet_msr(void *pvUser, PCDBGFREGDESC pDesc, PDBGFREGVAL pValue)
{
    /* Not availble at present, return all FFs to keep things quiet */
    uint64_t u64Value = UINT64_MAX;
    switch (pDesc->enmType)
    {
        case DBGFREGVALTYPE_U64:    pValue->u64 = u64Value; break;
        case DBGFREGVALTYPE_U32:    pValue->u32 = (uint32_t)u64Value; break;
        case DBGFREGVALTYPE_U16:    pValue->u16 = (uint16_t)u64Value; break;
        default:
            AssertFailedReturn(VERR_INTERNAL_ERROR_4);
    }
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegHyperSet_msr(void *pvUser, PCDBGFREGDESC pDesc, PCDBGFREGVAL pValue, PCDBGFREGVAL pfMask)
{
    /* Not settable, return failure. */
    NOREF(pvUser); NOREF(pDesc); NOREF(pValue); NOREF(pfMask);
    return VERR_ACCESS_DENIED;
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegHyperGet_stN(void *pvUser, PCDBGFREGDESC pDesc, PDBGFREGVAL pValue)
{
    PVMCPU      pVCpu   = (PVMCPU)pvUser;

    VMCPU_ASSERT_EMT(pVCpu);
    Assert(pDesc->enmType == DBGFREGVALTYPE_R80);

    if (cpumR3RegIsFxSaveFormat(pVCpu))
    {
        unsigned iReg = (pVCpu->cpum.s.Guest.fpu.FSW >> 11) & 7;
        iReg += pDesc->offRegister;
        iReg &= 7;
        pValue->r80 = pVCpu->cpum.s.Guest.fpu.aRegs[iReg].r80;
    }
    else
    {
        PCX86FPUSTATE pOldFpu = (PCX86FPUSTATE)&pVCpu->cpum.s.Guest.fpu;

        unsigned iReg = (pOldFpu->FSW >> 11) & 7;
        iReg += pDesc->offRegister;
        iReg &= 7;

        pValue->r80 = pOldFpu->regs[iReg].r80;
    }

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{DBGFREGDESC, pfnGet}
 */
static DECLCALLBACK(int) cpumR3RegHyperSet_stN(void *pvUser, PCDBGFREGDESC pDesc, PCDBGFREGVAL pValue, PCDBGFREGVAL pfMask)
{
    /* There isn't a FPU context for the hypervisor yet, so no point in trying to set stuff. */
    return VERR_ACCESS_DENIED;
}



/*
 * Set up aliases.
 */
#define CPUMREGALIAS_STD(Name, psz32, psz16, psz8)  \
    static DBGFREGALIAS const g_aCpumRegAliases_##Name[] = \
    { \
        { psz32, DBGFREGVALTYPE_U32     }, \
        { psz16, DBGFREGVALTYPE_U16     }, \
        { psz8,  DBGFREGVALTYPE_U8      }, \
        { NULL,  DBGFREGVALTYPE_INVALID } \
    }
CPUMREGALIAS_STD(rax,  "eax",   "ax",   "al");
CPUMREGALIAS_STD(rcx,  "ecx",   "cx",   "cl");
CPUMREGALIAS_STD(rdx,  "edx",   "dx",   "dl");
CPUMREGALIAS_STD(rbx,  "ebx",   "bx",   "bl");
CPUMREGALIAS_STD(rsp,  "esp",   "sp",   NULL);
CPUMREGALIAS_STD(rbp,  "ebp",   "bp",   NULL);
CPUMREGALIAS_STD(rsi,  "esi",   "si",  "sil");
CPUMREGALIAS_STD(rdi,  "edi",   "di",  "dil");
CPUMREGALIAS_STD(r8,   "r8d",  "r8w",  "r8b");
CPUMREGALIAS_STD(r9,   "r9d",  "r9w",  "r9b");
CPUMREGALIAS_STD(r10, "r10d", "r10w", "r10b");
CPUMREGALIAS_STD(r11, "r11d", "r11w", "r11b");
CPUMREGALIAS_STD(r12, "r12d", "r12w", "r12b");
CPUMREGALIAS_STD(r13, "r13d", "r13w", "r13b");
CPUMREGALIAS_STD(r14, "r14d", "r14w", "r14b");
CPUMREGALIAS_STD(r15, "r15d", "r15w", "r15b");
CPUMREGALIAS_STD(rip, "eip",   "ip",    NULL);
CPUMREGALIAS_STD(rflags, "eflags", "flags", NULL);
#undef CPUMREGALIAS_STD

static DBGFREGALIAS const g_aCpumRegAliases_fpuip[] =
{
    { "fpuip16", DBGFREGVALTYPE_U16  },
    { NULL, DBGFREGVALTYPE_INVALID }
};

static DBGFREGALIAS const g_aCpumRegAliases_fpudp[] =
{
    { "fpudp16", DBGFREGVALTYPE_U16  },
    { NULL, DBGFREGVALTYPE_INVALID }
};

static DBGFREGALIAS const g_aCpumRegAliases_cr0[] =
{
    { "msw", DBGFREGVALTYPE_U16  },
    { NULL, DBGFREGVALTYPE_INVALID }
};

/*
 * Sub fields.
 */
/** Sub-fields for the (hidden) segment attribute register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_seg[] =
{
    DBGFREGSUBFIELD_RW("type",   0,   4,  0),
    DBGFREGSUBFIELD_RW("s",      4,   1,  0),
    DBGFREGSUBFIELD_RW("dpl",    5,   2,  0),
    DBGFREGSUBFIELD_RW("p",      7,   1,  0),
    DBGFREGSUBFIELD_RW("avl",   12,   1,  0),
    DBGFREGSUBFIELD_RW("l",     13,   1,  0),
    DBGFREGSUBFIELD_RW("d",     14,   1,  0),
    DBGFREGSUBFIELD_RW("g",     15,   1,  0),
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the flags register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_rflags[] =
{
    DBGFREGSUBFIELD_RW("cf",     0,   1,  0),
    DBGFREGSUBFIELD_RW("pf",     2,   1,  0),
    DBGFREGSUBFIELD_RW("af",     4,   1,  0),
    DBGFREGSUBFIELD_RW("zf",     6,   1,  0),
    DBGFREGSUBFIELD_RW("sf",     7,   1,  0),
    DBGFREGSUBFIELD_RW("tf",     8,   1,  0),
    DBGFREGSUBFIELD_RW("if",     9,   1,  0),
    DBGFREGSUBFIELD_RW("df",    10,   1,  0),
    DBGFREGSUBFIELD_RW("of",    11,   1,  0),
    DBGFREGSUBFIELD_RW("iopl",  12,   2,  0),
    DBGFREGSUBFIELD_RW("nt",    14,   1,  0),
    DBGFREGSUBFIELD_RW("rf",    16,   1,  0),
    DBGFREGSUBFIELD_RW("vm",    17,   1,  0),
    DBGFREGSUBFIELD_RW("ac",    18,   1,  0),
    DBGFREGSUBFIELD_RW("vif",   19,   1,  0),
    DBGFREGSUBFIELD_RW("vip",   20,   1,  0),
    DBGFREGSUBFIELD_RW("id",    21,   1,  0),
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the FPU control word register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_fcw[] =
{
    DBGFREGSUBFIELD_RW("im",     1,   1,  0),
    DBGFREGSUBFIELD_RW("dm",     2,   1,  0),
    DBGFREGSUBFIELD_RW("zm",     3,   1,  0),
    DBGFREGSUBFIELD_RW("om",     4,   1,  0),
    DBGFREGSUBFIELD_RW("um",     5,   1,  0),
    DBGFREGSUBFIELD_RW("pm",     6,   1,  0),
    DBGFREGSUBFIELD_RW("pc",     8,   2,  0),
    DBGFREGSUBFIELD_RW("rc",    10,   2,  0),
    DBGFREGSUBFIELD_RW("x",     12,   1,  0),
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the FPU status word register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_fsw[] =
{
    DBGFREGSUBFIELD_RW("ie",     0,   1,  0),
    DBGFREGSUBFIELD_RW("de",     1,   1,  0),
    DBGFREGSUBFIELD_RW("ze",     2,   1,  0),
    DBGFREGSUBFIELD_RW("oe",     3,   1,  0),
    DBGFREGSUBFIELD_RW("ue",     4,   1,  0),
    DBGFREGSUBFIELD_RW("pe",     5,   1,  0),
    DBGFREGSUBFIELD_RW("se",     6,   1,  0),
    DBGFREGSUBFIELD_RW("es",     7,   1,  0),
    DBGFREGSUBFIELD_RW("c0",     8,   1,  0),
    DBGFREGSUBFIELD_RW("c1",     9,   1,  0),
    DBGFREGSUBFIELD_RW("c2",    10,   1,  0),
    DBGFREGSUBFIELD_RW("top",   11,   3,  0),
    DBGFREGSUBFIELD_RW("c3",    14,   1,  0),
    DBGFREGSUBFIELD_RW("b",     15,   1,  0),
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the FPU tag word register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_ftw[] =
{
    DBGFREGSUBFIELD_RW("tag0",   0,   2,  0),
    DBGFREGSUBFIELD_RW("tag1",   2,   2,  0),
    DBGFREGSUBFIELD_RW("tag2",   4,   2,  0),
    DBGFREGSUBFIELD_RW("tag3",   6,   2,  0),
    DBGFREGSUBFIELD_RW("tag4",   8,   2,  0),
    DBGFREGSUBFIELD_RW("tag5",  10,   2,  0),
    DBGFREGSUBFIELD_RW("tag6",  12,   2,  0),
    DBGFREGSUBFIELD_RW("tag7",  14,   2,  0),
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the Multimedia Extensions Control and Status Register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_mxcsr[] =
{
    DBGFREGSUBFIELD_RW("ie",     0,   1,  0),
    DBGFREGSUBFIELD_RW("de",     1,   1,  0),
    DBGFREGSUBFIELD_RW("ze",     2,   1,  0),
    DBGFREGSUBFIELD_RW("oe",     3,   1,  0),
    DBGFREGSUBFIELD_RW("ue",     4,   1,  0),
    DBGFREGSUBFIELD_RW("pe",     5,   1,  0),
    DBGFREGSUBFIELD_RW("daz",    6,   1,  0),
    DBGFREGSUBFIELD_RW("im",     7,   1,  0),
    DBGFREGSUBFIELD_RW("dm",     8,   1,  0),
    DBGFREGSUBFIELD_RW("zm",     9,   1,  0),
    DBGFREGSUBFIELD_RW("om",    10,   1,  0),
    DBGFREGSUBFIELD_RW("um",    11,   1,  0),
    DBGFREGSUBFIELD_RW("pm",    12,   1,  0),
    DBGFREGSUBFIELD_RW("rc",    13,   2,  0),
    DBGFREGSUBFIELD_RW("fz",    14,   1,  0),
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the FPU tag word register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_stN[] =
{
    DBGFREGSUBFIELD_RW("man",    0,  64,  0),
    DBGFREGSUBFIELD_RW("exp",   64,  15,  0),
    DBGFREGSUBFIELD_RW("sig",   79,   1,  0),
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the MMX registers. */
static DBGFREGSUBFIELD const g_aCpumRegFields_mmN[] =
{
    DBGFREGSUBFIELD_RW("dw0",    0,  32,  0),
    DBGFREGSUBFIELD_RW("dw1",   32,  32,  0),
    DBGFREGSUBFIELD_RW("w0",     0,  16,  0),
    DBGFREGSUBFIELD_RW("w1",    16,  16,  0),
    DBGFREGSUBFIELD_RW("w2",    32,  16,  0),
    DBGFREGSUBFIELD_RW("w3",    48,  16,  0),
    DBGFREGSUBFIELD_RW("b0",     0,   8,  0),
    DBGFREGSUBFIELD_RW("b1",     8,   8,  0),
    DBGFREGSUBFIELD_RW("b2",    16,   8,  0),
    DBGFREGSUBFIELD_RW("b3",    24,   8,  0),
    DBGFREGSUBFIELD_RW("b4",    32,   8,  0),
    DBGFREGSUBFIELD_RW("b5",    40,   8,  0),
    DBGFREGSUBFIELD_RW("b6",    48,   8,  0),
    DBGFREGSUBFIELD_RW("b7",    56,   8,  0),
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the XMM registers. */
static DBGFREGSUBFIELD const g_aCpumRegFields_xmmN[] =
{
    DBGFREGSUBFIELD_RW("r0",      0,     32,  0),
    DBGFREGSUBFIELD_RW("r0.man",  0+ 0,  23,  0),
    DBGFREGSUBFIELD_RW("r0.exp",  0+23,   8,  0),
    DBGFREGSUBFIELD_RW("r0.sig",  0+31,   1,  0),
    DBGFREGSUBFIELD_RW("r1",     32,     32,  0),
    DBGFREGSUBFIELD_RW("r1.man", 32+ 0,  23,  0),
    DBGFREGSUBFIELD_RW("r1.exp", 32+23,   8,  0),
    DBGFREGSUBFIELD_RW("r1.sig", 32+31,   1,  0),
    DBGFREGSUBFIELD_RW("r2",     64,     32,  0),
    DBGFREGSUBFIELD_RW("r2.man", 64+ 0,  23,  0),
    DBGFREGSUBFIELD_RW("r2.exp", 64+23,   8,  0),
    DBGFREGSUBFIELD_RW("r2.sig", 64+31,   1,  0),
    DBGFREGSUBFIELD_RW("r3",     96,     32,  0),
    DBGFREGSUBFIELD_RW("r3.man", 96+ 0,  23,  0),
    DBGFREGSUBFIELD_RW("r3.exp", 96+23,   8,  0),
    DBGFREGSUBFIELD_RW("r3.sig", 96+31,   1,  0),
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the CR0 register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_cr0[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the CR3 register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_cr3[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the CR4 register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_cr4[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the DR6 register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_dr6[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the DR7 register. */
static DBGFREGSUBFIELD const g_aCpumRegFields_dr7[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the CR_PAT MSR. */
static DBGFREGSUBFIELD const g_aCpumRegFields_apic_base[] =
{
    DBGFREGSUBFIELD_RW("bsp",     8,      1,  0),
    DBGFREGSUBFIELD_RW("ge",      9,      1,  0),
    DBGFREGSUBFIELD_RW("base",    12,    20, 12),
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the CR_PAT MSR. */
static DBGFREGSUBFIELD const g_aCpumRegFields_cr_pat[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the PERF_STATUS MSR. */
static DBGFREGSUBFIELD const g_aCpumRegFields_perf_status[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the EFER MSR. */
static DBGFREGSUBFIELD const g_aCpumRegFields_efer[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the STAR MSR. */
static DBGFREGSUBFIELD const g_aCpumRegFields_star[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the CSTAR MSR. */
static DBGFREGSUBFIELD const g_aCpumRegFields_cstar[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the LSTAR MSR. */
static DBGFREGSUBFIELD const g_aCpumRegFields_lstar[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};

/** Sub-fields for the SF_MASK MSR. */
static DBGFREGSUBFIELD const g_aCpumRegFields_sf_mask[] =
{
    /** @todo  */
    DBGFREGSUBFIELD_TERMINATOR()
};


/** @name Macros for producing register descriptor table entries.
 * @{ */
#define CPU_REG_EX_AS(a_szName, a_RegSuff, a_TypeSuff, a_offRegister, a_pfnGet, a_pfnSet, a_paAliases, a_paSubFields) \
    { a_szName, DBGFREG_##a_RegSuff, DBGFREGVALTYPE_##a_TypeSuff, 0 /*fFlags*/, a_offRegister, a_pfnGet, a_pfnSet, a_paAliases, a_paSubFields }

#define CPU_REG_REG(UName, LName) \
    CPU_REG_RW_AS(#LName,           UName,          U64, LName,                 cpumR3RegGet_Generic, cpumR3RegSet_Generic, g_aCpumRegAliases_##LName,  NULL)

#define CPU_REG_SEG(UName, LName) \
    CPU_REG_RW_AS(#LName,           UName,          U16, LName,                 cpumR3RegGet_Generic, cpumR3RegSet_seg,     NULL,                       NULL                ), \
    CPU_REG_RW_AS(#LName "_attr",   UName##_ATTR,   U32, LName##Hid.Attr.u,     cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL,                       g_aCpumRegFields_seg), \
    CPU_REG_RW_AS(#LName "_base",   UName##_BASE,   U64, LName##Hid.u64Base,    cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL,                       NULL                ), \
    CPU_REG_RW_AS(#LName "_lim",    UName##_LIMIT,  U32, LName##Hid.u32Limit,   cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL,                       NULL                )

#define CPU_REG_MM(n) \
    CPU_REG_RW_AS("mm" #n,          MM##n,          U64, fpu.aRegs[n].mmx,      cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL,                       g_aCpumRegFields_mmN)

#define CPU_REG_XMM(n) \
    CPU_REG_RW_AS("xmm" #n,         XMM##n,         U128, fpu.aXMM[n].xmm,      cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL,                       g_aCpumRegFields_xmmN)
/** @} */


/**
 * The guest register descriptors.
 */
static DBGFREGDESC const g_aCpumRegGstDescs[] =
{
#define CPU_REG_RW_AS(a_szName, a_RegSuff, a_TypeSuff, a_CpumCtxMemb, a_pfnGet, a_pfnSet, a_paAliases, a_paSubFields) \
    { a_szName, DBGFREG_##a_RegSuff, DBGFREGVALTYPE_##a_TypeSuff, 0 /*fFlags*/,            RT_OFFSETOF(CPUMCPU, Guest.a_CpumCtxMemb), a_pfnGet, a_pfnSet, a_paAliases, a_paSubFields }
#define CPU_REG_RO_AS(a_szName, a_RegSuff, a_TypeSuff, a_CpumCtxMemb, a_pfnGet, a_pfnSet, a_paAliases, a_paSubFields) \
    { a_szName, DBGFREG_##a_RegSuff, DBGFREGVALTYPE_##a_TypeSuff, DBGFREG_FLAGS_READ_ONLY, RT_OFFSETOF(CPUMCPU, Guest.a_CpumCtxMemb), a_pfnGet, a_pfnSet, a_paAliases, a_paSubFields }
#define CPU_REG_MSR(a_szName, UName, a_TypeSuff, a_paSubFields) \
    CPU_REG_EX_AS(a_szName,         MSR_##UName,    a_TypeSuff, MSR_##UName,    cpumR3RegGstGet_msr,  cpumR3RegGstSet_msr,  NULL,                       a_paSubFields)
#define CPU_REG_ST(n) \
    CPU_REG_EX_AS("st" #n,          ST##n,          R80, n,                     cpumR3RegGstGet_stN,  cpumR3RegGstSet_stN,  NULL,                       g_aCpumRegFields_stN)

    CPU_REG_REG(RAX, rax),
    CPU_REG_REG(RCX, rcx),
    CPU_REG_REG(RDX, rdx),
    CPU_REG_REG(RBX, rbx),
    CPU_REG_REG(RSP, rsp),
    CPU_REG_REG(RBP, rbp),
    CPU_REG_REG(RSI, rsi),
    CPU_REG_REG(RDI, rdi),
    CPU_REG_REG(R8,   r8),
    CPU_REG_REG(R9,   r9),
    CPU_REG_REG(R10, r10),
    CPU_REG_REG(R11, r11),
    CPU_REG_REG(R12, r12),
    CPU_REG_REG(R13, r13),
    CPU_REG_REG(R14, r14),
    CPU_REG_REG(R15, r15),
    CPU_REG_SEG(CS, cs),
    CPU_REG_SEG(DS, ds),
    CPU_REG_SEG(ES, es),
    CPU_REG_SEG(FS, fs),
    CPU_REG_SEG(GS, gs),
    CPU_REG_SEG(SS, ss),
    CPU_REG_REG(RIP, rip),
    CPU_REG_RW_AS("rflags",         RFLAGS,         U64, rflags,                cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   g_aCpumRegAliases_rflags,   g_aCpumRegFields_rflags ),
    CPU_REG_RW_AS("fcw",            FCW,            U16, fpu.FCW,               cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   NULL,                       g_aCpumRegFields_fcw    ),
    CPU_REG_RW_AS("fsw",            FSW,            U16, fpu.FSW,               cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   NULL,                       g_aCpumRegFields_fsw    ),
    CPU_REG_RO_AS("ftw",            FTW,            U16, fpu,                   cpumR3RegGet_ftw,       cpumR3RegSet_ftw,       NULL,                       g_aCpumRegFields_ftw    ),
    CPU_REG_RW_AS("fop",            FOP,            U16, fpu.FOP,               cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   NULL,                       NULL                    ),
    CPU_REG_RW_AS("fpuip",          FPUIP,          U32, fpu.FPUIP,             cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   g_aCpumRegAliases_fpuip,    NULL                    ),
    CPU_REG_RW_AS("fpucs",          FPUCS,          U16, fpu.CS,                cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   NULL,                       NULL                    ),
    CPU_REG_RW_AS("fpudp",          FPUDP,          U32, fpu.FPUDP,             cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   g_aCpumRegAliases_fpudp,    NULL                    ),
    CPU_REG_RW_AS("fpuds",          FPUDS,          U16, fpu.DS,                cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   NULL,                       NULL                    ),
    CPU_REG_RW_AS("mxcsr",          MXCSR,          U32, fpu.MXCSR,             cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   NULL,                       g_aCpumRegFields_mxcsr  ),
    CPU_REG_RW_AS("mxcsr_mask",     MXCSR_MASK,     U32, fpu.MXCSR_MASK,        cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   NULL,                       g_aCpumRegFields_mxcsr  ),
    CPU_REG_ST(0),
    CPU_REG_ST(1),
    CPU_REG_ST(2),
    CPU_REG_ST(3),
    CPU_REG_ST(4),
    CPU_REG_ST(5),
    CPU_REG_ST(6),
    CPU_REG_ST(7),
    CPU_REG_MM(0),
    CPU_REG_MM(1),
    CPU_REG_MM(2),
    CPU_REG_MM(3),
    CPU_REG_MM(4),
    CPU_REG_MM(5),
    CPU_REG_MM(6),
    CPU_REG_MM(7),
    CPU_REG_XMM(0),
    CPU_REG_XMM(1),
    CPU_REG_XMM(2),
    CPU_REG_XMM(3),
    CPU_REG_XMM(4),
    CPU_REG_XMM(5),
    CPU_REG_XMM(6),
    CPU_REG_XMM(7),
    CPU_REG_XMM(8),
    CPU_REG_XMM(9),
    CPU_REG_XMM(10),
    CPU_REG_XMM(11),
    CPU_REG_XMM(12),
    CPU_REG_XMM(13),
    CPU_REG_XMM(14),
    CPU_REG_XMM(15),
    CPU_REG_RW_AS("gdtr_base",      GDTR_BASE,      U64, gdtr.pGdt,             cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   NULL,                       NULL                    ),
    CPU_REG_RW_AS("gdtr_lim",       GDTR_LIMIT,     U16, gdtr.cbGdt,            cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   NULL,                       NULL                    ),
    CPU_REG_RW_AS("idtr_base",      IDTR_BASE,      U64, idtr.pIdt,             cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   NULL,                       NULL                    ),
    CPU_REG_RW_AS("idtr_lim",       IDTR_LIMIT,     U16, idtr.cbIdt,            cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   NULL,                       NULL                    ),
    CPU_REG_SEG(LDTR, ldtr),
    CPU_REG_SEG(TR, tr),
    CPU_REG_EX_AS("cr0",            CR0,            U32, 0,                     cpumR3RegGstGet_crX,    cpumR3RegGstSet_crX,    g_aCpumRegAliases_cr0,      g_aCpumRegFields_cr0    ),
    CPU_REG_EX_AS("cr2",            CR2,            U64, 2,                     cpumR3RegGstGet_crX,    cpumR3RegGstSet_crX,    NULL,                       NULL                    ),
    CPU_REG_EX_AS("cr3",            CR3,            U64, 3,                     cpumR3RegGstGet_crX,    cpumR3RegGstSet_crX,    NULL,                       g_aCpumRegFields_cr3    ),
    CPU_REG_EX_AS("cr4",            CR4,            U32, 4,                     cpumR3RegGstGet_crX,    cpumR3RegGstSet_crX,    NULL,                       g_aCpumRegFields_cr4    ),
    CPU_REG_EX_AS("cr8",            CR8,            U32, 8,                     cpumR3RegGstGet_crX,    cpumR3RegGstSet_crX,    NULL,                       NULL                    ),
    CPU_REG_EX_AS("dr0",            DR0,            U64, 0,                     cpumR3RegGstGet_drX,    cpumR3RegGstSet_drX,    NULL,                       NULL                    ),
    CPU_REG_EX_AS("dr1",            DR1,            U64, 1,                     cpumR3RegGstGet_drX,    cpumR3RegGstSet_drX,    NULL,                       NULL                    ),
    CPU_REG_EX_AS("dr2",            DR2,            U64, 2,                     cpumR3RegGstGet_drX,    cpumR3RegGstSet_drX,    NULL,                       NULL                    ),
    CPU_REG_EX_AS("dr3",            DR3,            U64, 3,                     cpumR3RegGstGet_drX,    cpumR3RegGstSet_drX,    NULL,                       NULL                    ),
    CPU_REG_EX_AS("dr6",            DR6,            U32, 6,                     cpumR3RegGstGet_drX,    cpumR3RegGstSet_drX,    NULL,                       g_aCpumRegFields_dr6    ),
    CPU_REG_EX_AS("dr7",            DR7,            U32, 7,                     cpumR3RegGstGet_drX,    cpumR3RegGstSet_drX,    NULL,                       g_aCpumRegFields_dr7    ),
    CPU_REG_MSR("apic_base",     IA32_APICBASE,     U32, g_aCpumRegFields_apic_base  ),
    CPU_REG_MSR("pat",           IA32_CR_PAT,       U64, g_aCpumRegFields_cr_pat     ),
    CPU_REG_MSR("perf_status",   IA32_PERF_STATUS,  U64, g_aCpumRegFields_perf_status),
    CPU_REG_MSR("sysenter_cs",   IA32_SYSENTER_CS,  U16, NULL                        ),
    CPU_REG_MSR("sysenter_eip",  IA32_SYSENTER_EIP, U32, NULL                        ),
    CPU_REG_MSR("sysenter_esp",  IA32_SYSENTER_ESP, U32, NULL                        ),
    CPU_REG_MSR("tsc",           IA32_TSC,          U32, NULL                        ),
    CPU_REG_MSR("efer",          K6_EFER,           U32, g_aCpumRegFields_efer       ),
    CPU_REG_MSR("star",          K6_STAR,           U64, g_aCpumRegFields_star       ),
    CPU_REG_MSR("cstar",         K8_CSTAR,          U64, g_aCpumRegFields_cstar      ),
    CPU_REG_MSR("msr_fs_base",   K8_FS_BASE,        U64, NULL                        ),
    CPU_REG_MSR("msr_gs_base",   K8_GS_BASE,        U64, NULL                        ),
    CPU_REG_MSR("krnl_gs_base",  K8_KERNEL_GS_BASE, U64, NULL                        ),
    CPU_REG_MSR("lstar",         K8_LSTAR,          U64, g_aCpumRegFields_lstar      ),
    CPU_REG_MSR("sf_mask",       K8_SF_MASK,        U64, NULL                        ),
    CPU_REG_MSR("tsc_aux",       K8_TSC_AUX,        U64, NULL                        ),
    CPU_REG_EX_AS("ah",             AH,             U8,  RT_OFFSETOF(CPUMCPU, Guest.rax) + 1, cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL,             NULL                    ),
    CPU_REG_EX_AS("ch",             CH,             U8,  RT_OFFSETOF(CPUMCPU, Guest.rcx) + 1, cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL,             NULL                    ),
    CPU_REG_EX_AS("dh",             DH,             U8,  RT_OFFSETOF(CPUMCPU, Guest.rdx) + 1, cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL,             NULL                    ),
    CPU_REG_EX_AS("bh",             BH,             U8,  RT_OFFSETOF(CPUMCPU, Guest.rbx) + 1, cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL,             NULL                    ),
    CPU_REG_RW_AS("gdtr",           GDTR,           DTR, gdtr,                  cpumR3RegGet_gdtr,        cpumR3RegSet_gdtr,    NULL,                       NULL                    ),
    CPU_REG_RW_AS("idtr",           IDTR,           DTR, idtr,                  cpumR3RegGet_idtr,        cpumR3RegSet_idtr,    NULL,                       NULL                    ),
    DBGFREGDESC_TERMINATOR()

#undef CPU_REG_RW_AS
#undef CPU_REG_RO_AS
#undef CPU_REG_MSR
#undef CPU_REG_ST
};


/**
 * The hypervisor (raw-mode) register descriptors.
 */
static DBGFREGDESC const g_aCpumRegHyperDescs[] =
{
#define CPU_REG_RW_AS(a_szName, a_RegSuff, a_TypeSuff, a_CpumCtxMemb, a_pfnGet, a_pfnSet, a_paAliases, a_paSubFields) \
    { a_szName, DBGFREG_##a_RegSuff, DBGFREGVALTYPE_##a_TypeSuff, 0 /*fFlags*/,            RT_OFFSETOF(CPUMCPU, Hyper.a_CpumCtxMemb), a_pfnGet, a_pfnSet, a_paAliases, a_paSubFields }
#define CPU_REG_RO_AS(a_szName, a_RegSuff, a_TypeSuff, a_CpumCtxMemb, a_pfnGet, a_pfnSet, a_paAliases, a_paSubFields) \
    { a_szName, DBGFREG_##a_RegSuff, DBGFREGVALTYPE_##a_TypeSuff, DBGFREG_FLAGS_READ_ONLY, RT_OFFSETOF(CPUMCPU, Hyper.a_CpumCtxMemb), a_pfnGet, a_pfnSet, a_paAliases, a_paSubFields }
#define CPU_REG_MSR(a_szName, UName, a_TypeSuff, a_paSubFields) \
    CPU_REG_EX_AS(a_szName,         MSR_##UName,    a_TypeSuff, MSR_##UName,    cpumR3RegHyperGet_msr,  cpumR3RegHyperSet_msr,  NULL,                       a_paSubFields)
#define CPU_REG_ST(n) \
    CPU_REG_EX_AS("st" #n,          ST##n,          R80, n,                     cpumR3RegHyperGet_stN,  cpumR3RegHyperSet_stN,  NULL,                       g_aCpumRegFields_stN)

    CPU_REG_REG(RAX, rax),
    CPU_REG_REG(RCX, rcx),
    CPU_REG_REG(RDX, rdx),
    CPU_REG_REG(RBX, rbx),
    CPU_REG_REG(RSP, rsp),
    CPU_REG_REG(RBP, rbp),
    CPU_REG_REG(RSI, rsi),
    CPU_REG_REG(RDI, rdi),
    CPU_REG_REG(R8,   r8),
    CPU_REG_REG(R9,   r9),
    CPU_REG_REG(R10, r10),
    CPU_REG_REG(R11, r11),
    CPU_REG_REG(R12, r12),
    CPU_REG_REG(R13, r13),
    CPU_REG_REG(R14, r14),
    CPU_REG_REG(R15, r15),
    CPU_REG_SEG(CS, cs),
    CPU_REG_SEG(DS, ds),
    CPU_REG_SEG(ES, es),
    CPU_REG_SEG(FS, fs),
    CPU_REG_SEG(GS, gs),
    CPU_REG_SEG(SS, ss),
    CPU_REG_REG(RIP, rip),
    CPU_REG_RW_AS("rflags",         RFLAGS,         U64, rflags,                cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   g_aCpumRegAliases_rflags,   g_aCpumRegFields_rflags ),
    CPU_REG_RW_AS("fcw",            FCW,            U16, fpu.FCW,               cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   NULL,                       g_aCpumRegFields_fcw    ),
    CPU_REG_RW_AS("fsw",            FSW,            U16, fpu.FSW,               cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   NULL,                       g_aCpumRegFields_fsw    ),
    CPU_REG_RO_AS("ftw",            FTW,            U16, fpu,                   cpumR3RegGet_ftw,       cpumR3RegSet_ftw,       NULL,                       g_aCpumRegFields_ftw    ),
    CPU_REG_RW_AS("fop",            FOP,            U16, fpu.FOP,               cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   NULL,                       NULL                    ),
    CPU_REG_RW_AS("fpuip",          FPUIP,          U32, fpu.FPUIP,             cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   g_aCpumRegAliases_fpuip,    NULL                    ),
    CPU_REG_RW_AS("fpucs",          FPUCS,          U16, fpu.CS,                cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   NULL,                       NULL                    ),
    CPU_REG_RW_AS("fpudp",          FPUDP,          U32, fpu.FPUDP,             cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   g_aCpumRegAliases_fpudp,    NULL                    ),
    CPU_REG_RW_AS("fpuds",          FPUDS,          U16, fpu.DS,                cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   NULL,                       NULL                    ),
    CPU_REG_RW_AS("mxcsr",          MXCSR,          U32, fpu.MXCSR,             cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   NULL,                       g_aCpumRegFields_mxcsr  ),
    CPU_REG_RW_AS("mxcsr_mask",     MXCSR_MASK,     U32, fpu.MXCSR_MASK,        cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   NULL,                       g_aCpumRegFields_mxcsr  ),
    CPU_REG_ST(0),
    CPU_REG_ST(1),
    CPU_REG_ST(2),
    CPU_REG_ST(3),
    CPU_REG_ST(4),
    CPU_REG_ST(5),
    CPU_REG_ST(6),
    CPU_REG_ST(7),
    CPU_REG_MM(0),
    CPU_REG_MM(1),
    CPU_REG_MM(2),
    CPU_REG_MM(3),
    CPU_REG_MM(4),
    CPU_REG_MM(5),
    CPU_REG_MM(6),
    CPU_REG_MM(7),
    CPU_REG_XMM(0),
    CPU_REG_XMM(1),
    CPU_REG_XMM(2),
    CPU_REG_XMM(3),
    CPU_REG_XMM(4),
    CPU_REG_XMM(5),
    CPU_REG_XMM(6),
    CPU_REG_XMM(7),
    CPU_REG_XMM(8),
    CPU_REG_XMM(9),
    CPU_REG_XMM(10),
    CPU_REG_XMM(11),
    CPU_REG_XMM(12),
    CPU_REG_XMM(13),
    CPU_REG_XMM(14),
    CPU_REG_XMM(15),
    CPU_REG_RW_AS("gdtr_base",      GDTR_BASE,      U64, gdtr.pGdt,             cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   NULL,                       NULL                    ),
    CPU_REG_RW_AS("gdtr_lim",       GDTR_LIMIT,     U16, gdtr.cbGdt,            cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   NULL,                       NULL                    ),
    CPU_REG_RW_AS("idtr_base",      IDTR_BASE,      U64, idtr.pIdt,             cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   NULL,                       NULL                    ),
    CPU_REG_RW_AS("idtr_lim",       IDTR_LIMIT,     U16, idtr.cbIdt,            cpumR3RegGet_Generic,   cpumR3RegSet_Generic,   NULL,                       NULL                    ),
    CPU_REG_SEG(LDTR, ldtr),
    CPU_REG_SEG(TR, tr),
    CPU_REG_EX_AS("cr0",            CR0,            U32, 0,                     cpumR3RegHyperGet_crX,  cpumR3RegHyperSet_crX,  g_aCpumRegAliases_cr0,      g_aCpumRegFields_cr0    ),
    CPU_REG_EX_AS("cr2",            CR2,            U64, 2,                     cpumR3RegHyperGet_crX,  cpumR3RegHyperSet_crX,  NULL,                       NULL                    ),
    CPU_REG_EX_AS("cr3",            CR3,            U64, 3,                     cpumR3RegHyperGet_crX,  cpumR3RegHyperSet_crX,  NULL,                       g_aCpumRegFields_cr3    ),
    CPU_REG_EX_AS("cr4",            CR4,            U32, 4,                     cpumR3RegHyperGet_crX,  cpumR3RegHyperSet_crX,  NULL,                       g_aCpumRegFields_cr4    ),
    CPU_REG_EX_AS("cr8",            CR8,            U32, 8,                     cpumR3RegHyperGet_crX,  cpumR3RegHyperSet_crX,  NULL,                       NULL                    ),
    CPU_REG_EX_AS("dr0",            DR0,            U64, 0,                     cpumR3RegHyperGet_drX,  cpumR3RegHyperSet_drX,  NULL,                       NULL                    ),
    CPU_REG_EX_AS("dr1",            DR1,            U64, 1,                     cpumR3RegHyperGet_drX,  cpumR3RegHyperSet_drX,  NULL,                       NULL                    ),
    CPU_REG_EX_AS("dr2",            DR2,            U64, 2,                     cpumR3RegHyperGet_drX,  cpumR3RegHyperSet_drX,  NULL,                       NULL                    ),
    CPU_REG_EX_AS("dr3",            DR3,            U64, 3,                     cpumR3RegHyperGet_drX,  cpumR3RegHyperSet_drX,  NULL,                       NULL                    ),
    CPU_REG_EX_AS("dr6",            DR6,            U32, 6,                     cpumR3RegHyperGet_drX,  cpumR3RegHyperSet_drX,  NULL,                       g_aCpumRegFields_dr6    ),
    CPU_REG_EX_AS("dr7",            DR7,            U32, 7,                     cpumR3RegHyperGet_drX,  cpumR3RegHyperSet_drX,  NULL,                       g_aCpumRegFields_dr7    ),
    CPU_REG_MSR("apic_base",     IA32_APICBASE,     U32, g_aCpumRegFields_apic_base  ),
    CPU_REG_MSR("pat",           IA32_CR_PAT,       U64, g_aCpumRegFields_cr_pat     ),
    CPU_REG_MSR("perf_status",   IA32_PERF_STATUS,  U64, g_aCpumRegFields_perf_status),
    CPU_REG_MSR("sysenter_cs",   IA32_SYSENTER_CS,  U16, NULL                        ),
    CPU_REG_MSR("sysenter_eip",  IA32_SYSENTER_EIP, U32, NULL                        ),
    CPU_REG_MSR("sysenter_esp",  IA32_SYSENTER_ESP, U32, NULL                        ),
    CPU_REG_MSR("tsc",           IA32_TSC,          U32, NULL                        ),
    CPU_REG_MSR("efer",          K6_EFER,           U32, g_aCpumRegFields_efer       ),
    CPU_REG_MSR("star",          K6_STAR,           U64, g_aCpumRegFields_star       ),
    CPU_REG_MSR("cstar",         K8_CSTAR,          U64, g_aCpumRegFields_cstar      ),
    CPU_REG_MSR("msr_fs_base",   K8_FS_BASE,        U64, NULL                        ),
    CPU_REG_MSR("msr_gs_base",   K8_GS_BASE,        U64, NULL                        ),
    CPU_REG_MSR("krnl_gs_base",  K8_KERNEL_GS_BASE, U64, NULL                        ),
    CPU_REG_MSR("lstar",         K8_LSTAR,          U64, g_aCpumRegFields_lstar      ),
    CPU_REG_MSR("sf_mask",       K8_SF_MASK,        U64, NULL                        ),
    CPU_REG_MSR("tsc_aux",       K8_TSC_AUX,        U64, NULL                        ),
    CPU_REG_EX_AS("ah",             AH,             U8,  RT_OFFSETOF(CPUMCPU, Hyper.rax) + 1, cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL,             NULL                    ),
    CPU_REG_EX_AS("ch",             CH,             U8,  RT_OFFSETOF(CPUMCPU, Hyper.rcx) + 1, cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL,             NULL                    ),
    CPU_REG_EX_AS("dh",             DH,             U8,  RT_OFFSETOF(CPUMCPU, Hyper.rdx) + 1, cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL,             NULL                    ),
    CPU_REG_EX_AS("bh",             BH,             U8,  RT_OFFSETOF(CPUMCPU, Hyper.rbx) + 1, cpumR3RegGet_Generic, cpumR3RegSet_Generic, NULL,             NULL                    ),
    CPU_REG_RW_AS("gdtr",           GDTR,           DTR, gdtr,                  cpumR3RegGet_gdtr,        cpumR3RegSet_gdtr,    NULL,                       NULL                    ),
    CPU_REG_RW_AS("idtr",           IDTR,           DTR, idtr,                  cpumR3RegGet_idtr,        cpumR3RegSet_idtr,    NULL,                       NULL                    ),
    DBGFREGDESC_TERMINATOR()
#undef CPU_REG_RW_AS
#undef CPU_REG_RO_AS
#undef CPU_REG_MSR
#undef CPU_REG_ST
};


/**
 * Initializes the debugger related sides of the CPUM component.
 *
 * Called by CPUMR3Init.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 */
int cpumR3DbgInit(PVM pVM)
{
    for (VMCPUID iCpu = 0; iCpu < pVM->cCpus; iCpu++)
    {
        int rc = DBGFR3RegRegisterCpu(pVM, &pVM->aCpus[iCpu], g_aCpumRegGstDescs, true /*fGuestRegs*/);
        AssertLogRelRCReturn(rc, rc);
        rc = DBGFR3RegRegisterCpu(pVM, &pVM->aCpus[iCpu], g_aCpumRegHyperDescs, false /*fGuestRegs*/);
        AssertLogRelRCReturn(rc, rc);
    }

    return VINF_SUCCESS;
}

