/* $Id$ */
/** @file
 * DBGF - Debugger Facility, Register Methods.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
#include <VBox/dbgf.h>
#include "DBGFInternal.h"
#include <VBox/vm.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/log.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @name Register and value sizes used by dbgfR3RegQueryWorker and
 *        dbgfR3RegSetWorker.
 * @{ */
#define R_SZ_8          RT_BIT(0)
#define R_SZ_16         RT_BIT(1)
#define R_SZ_32         RT_BIT(2)
#define R_SZ_64         RT_BIT(3)
#define R_SZ_64_16      RT_BIT(4)
#define R_SZ_8_TO_64    (R_SZ_8 | R_SZ_16 | R_SZ_32 | R_SZ_64)
#define R_SZ_16_TO_64   (R_SZ_16 | R_SZ_32 | R_SZ_64)
#define R_SZ_32_OR_64   (R_SZ_32 | R_SZ_64)
/** @}  */


/**
 * Wrapper around CPUMQueryGuestMsr.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_DBGF_INVALID_REGISTER
 *
 * @param   pVCpu               The current CPU.
 * @param   pu64                Where to store the register value.
 * @param   pfRegSizes          Where to store the register sizes.
 * @param   idMsr               The MSR to get.
 */
static uint64_t dbgfR3RegGetMsr(PVMCPU pVCpu, uint64_t *pu64, uint32_t *pfRegSizes, uint32_t idMsr)
{
    *pfRegSizes = R_SZ_64;
    int rc = CPUMQueryGuestMsr(pVCpu, idMsr, pu64);
    if (RT_FAILURE(rc))
    {
        AssertMsg(rc == VERR_CPUM_RAISE_GP_0, ("%Rrc\n", rc));
        *pu64 = 0;
    }
    return VINF_SUCCESS;
}

/**
 * Worker for DBGFR3RegQueryU8, DBGFR3RegQueryU16, DBGFR3RegQueryU32 and
 * DBGFR3RegQueryU64.
 *
 * @param   pVM                 The VM handle.
 * @param   idCpu               The target CPU ID.
 * @param   enmReg              The register that's being queried.
 * @param   pu64                Where to store the register value.
 * @param   pfRegSizes          Where to store the register sizes.
 */
static DECLCALLBACK(int) dbgfR3RegQueryWorker(PVM pVM, VMCPUID idCpu, DBGFREG enmReg, uint64_t *pu64, uint32_t *pfRegSizes)
{
    PVMCPU    pVCpu = &pVM->aCpus[idCpu];
    PCCPUMCTX pCtx  = CPUMQueryGuestCtxPtr(pVCpu);
    switch (enmReg)
    {
        case DBGFREG_RAX:       *pu64 = pCtx->rax;  *pfRegSizes = R_SZ_8_TO_64;         return VINF_SUCCESS;
        case DBGFREG_RCX:       *pu64 = pCtx->rcx;  *pfRegSizes = R_SZ_8_TO_64;         return VINF_SUCCESS;
        case DBGFREG_RDX:       *pu64 = pCtx->rdx;  *pfRegSizes = R_SZ_8_TO_64;         return VINF_SUCCESS;
        case DBGFREG_RBX:       *pu64 = pCtx->rbx;  *pfRegSizes = R_SZ_8_TO_64;         return VINF_SUCCESS;
        case DBGFREG_RSP:       *pu64 = pCtx->rsp;  *pfRegSizes = R_SZ_8_TO_64;         return VINF_SUCCESS;
        case DBGFREG_RBP:       *pu64 = pCtx->rbp;  *pfRegSizes = R_SZ_8_TO_64;         return VINF_SUCCESS;
        case DBGFREG_RSI:       *pu64 = pCtx->rsi;  *pfRegSizes = R_SZ_8_TO_64;         return VINF_SUCCESS;
        case DBGFREG_RDI:       *pu64 = pCtx->rdi;  *pfRegSizes = R_SZ_8_TO_64;         return VINF_SUCCESS;
        case DBGFREG_R8:        *pu64 = pCtx->r8;   *pfRegSizes = R_SZ_8_TO_64;         return VINF_SUCCESS;
        case DBGFREG_R9:        *pu64 = pCtx->r9;   *pfRegSizes = R_SZ_8_TO_64;         return VINF_SUCCESS;
        case DBGFREG_R10:       *pu64 = pCtx->r10;  *pfRegSizes = R_SZ_8_TO_64;         return VINF_SUCCESS;
        case DBGFREG_R11:       *pu64 = pCtx->r11;  *pfRegSizes = R_SZ_8_TO_64;         return VINF_SUCCESS;
        case DBGFREG_R12:       *pu64 = pCtx->r12;  *pfRegSizes = R_SZ_8_TO_64;         return VINF_SUCCESS;
        case DBGFREG_R13:       *pu64 = pCtx->r13;  *pfRegSizes = R_SZ_8_TO_64;         return VINF_SUCCESS;
        case DBGFREG_R14:       *pu64 = pCtx->r14;  *pfRegSizes = R_SZ_8_TO_64;         return VINF_SUCCESS;
        case DBGFREG_R15:       *pu64 = pCtx->r15;  *pfRegSizes = R_SZ_8_TO_64;         return VINF_SUCCESS;

        case DBGFREG_AH:        *pu64 = RT_BYTE2(pCtx->ax); *pfRegSizes = R_SZ_8;       return VINF_SUCCESS;
        case DBGFREG_CH:        *pu64 = RT_BYTE2(pCtx->cx); *pfRegSizes = R_SZ_8;       return VINF_SUCCESS;
        case DBGFREG_DH:        *pu64 = RT_BYTE2(pCtx->dx); *pfRegSizes = R_SZ_8;       return VINF_SUCCESS;
        case DBGFREG_BH:        *pu64 = RT_BYTE2(pCtx->bx); *pfRegSizes = R_SZ_8;       return VINF_SUCCESS;

        case DBGFREG_CS:        *pu64 = pCtx->cs;   *pfRegSizes = R_SZ_16;              return VINF_SUCCESS;
        case DBGFREG_DS:        *pu64 = pCtx->ds;   *pfRegSizes = R_SZ_16;              return VINF_SUCCESS;
        case DBGFREG_ES:        *pu64 = pCtx->es;   *pfRegSizes = R_SZ_16;              return VINF_SUCCESS;
        case DBGFREG_FS:        *pu64 = pCtx->fs;   *pfRegSizes = R_SZ_16;              return VINF_SUCCESS;
        case DBGFREG_GS:        *pu64 = pCtx->gs;   *pfRegSizes = R_SZ_16;              return VINF_SUCCESS;
        case DBGFREG_SS:        *pu64 = pCtx->ss;   *pfRegSizes = R_SZ_16;              return VINF_SUCCESS;

        case DBGFREG_CS_ATTR:   *pu64 = pCtx->csHid.Attr.u;  *pfRegSizes = R_SZ_32;     return VINF_SUCCESS;
        case DBGFREG_DS_ATTR:   *pu64 = pCtx->dsHid.Attr.u;  *pfRegSizes = R_SZ_32;     return VINF_SUCCESS;
        case DBGFREG_ES_ATTR:   *pu64 = pCtx->esHid.Attr.u;  *pfRegSizes = R_SZ_32;     return VINF_SUCCESS;
        case DBGFREG_FS_ATTR:   *pu64 = pCtx->fsHid.Attr.u;  *pfRegSizes = R_SZ_32;     return VINF_SUCCESS;
        case DBGFREG_GS_ATTR:   *pu64 = pCtx->gsHid.Attr.u;  *pfRegSizes = R_SZ_32;     return VINF_SUCCESS;
        case DBGFREG_SS_ATTR:   *pu64 = pCtx->ssHid.Attr.u;  *pfRegSizes = R_SZ_32;     return VINF_SUCCESS;

        case DBGFREG_CS_BASE:   *pu64 = pCtx->csHid.u64Base; *pfRegSizes = R_SZ_64;     return VINF_SUCCESS;
        case DBGFREG_DS_BASE:   *pu64 = pCtx->dsHid.u64Base; *pfRegSizes = R_SZ_64;     return VINF_SUCCESS;
        case DBGFREG_ES_BASE:   *pu64 = pCtx->esHid.u64Base; *pfRegSizes = R_SZ_64;     return VINF_SUCCESS;
        case DBGFREG_FS_BASE:   *pu64 = pCtx->fsHid.u64Base; *pfRegSizes = R_SZ_64;     return VINF_SUCCESS;
        case DBGFREG_GS_BASE:   *pu64 = pCtx->gsHid.u64Base; *pfRegSizes = R_SZ_64;     return VINF_SUCCESS;
        case DBGFREG_SS_BASE:   *pu64 = pCtx->ssHid.u64Base; *pfRegSizes = R_SZ_64;     return VINF_SUCCESS;

        case DBGFREG_CS_LIMIT:  *pu64 = pCtx->csHid.u32Limit; *pfRegSizes = R_SZ_32;    return VINF_SUCCESS;
        case DBGFREG_DS_LIMIT:  *pu64 = pCtx->dsHid.u32Limit; *pfRegSizes = R_SZ_32;    return VINF_SUCCESS;
        case DBGFREG_ES_LIMIT:  *pu64 = pCtx->esHid.u32Limit; *pfRegSizes = R_SZ_32;    return VINF_SUCCESS;
        case DBGFREG_FS_LIMIT:  *pu64 = pCtx->fsHid.u32Limit; *pfRegSizes = R_SZ_32;    return VINF_SUCCESS;
        case DBGFREG_GS_LIMIT:  *pu64 = pCtx->gsHid.u32Limit; *pfRegSizes = R_SZ_32;    return VINF_SUCCESS;
        case DBGFREG_SS_LIMIT:  *pu64 = pCtx->ssHid.u32Limit; *pfRegSizes = R_SZ_32;    return VINF_SUCCESS;

        case DBGFREG_RIP:       *pu64 = pCtx->rip;      *pfRegSizes = R_SZ_16_TO_64;    return VINF_SUCCESS;
        case DBGFREG_FLAGS:     *pu64 = pCtx->rflags.u; *pfRegSizes = R_SZ_16_TO_64;    return VINF_SUCCESS;

        case DBGFREG_ST0:                                                   return VERR_NOT_IMPLEMENTED;
        case DBGFREG_ST1:                                                   return VERR_NOT_IMPLEMENTED;
        case DBGFREG_ST2:                                                   return VERR_NOT_IMPLEMENTED;
        case DBGFREG_ST3:                                                   return VERR_NOT_IMPLEMENTED;
        case DBGFREG_ST4:                                                   return VERR_NOT_IMPLEMENTED;
        case DBGFREG_ST5:                                                   return VERR_NOT_IMPLEMENTED;
        case DBGFREG_ST6:                                                   return VERR_NOT_IMPLEMENTED;
        case DBGFREG_ST7:                                                   return VERR_NOT_IMPLEMENTED;

        case DBGFREG_MM0:                                                   return VERR_NOT_IMPLEMENTED;
        case DBGFREG_MM1:                                                   return VERR_NOT_IMPLEMENTED;
        case DBGFREG_MM2:                                                   return VERR_NOT_IMPLEMENTED;
        case DBGFREG_MM3:                                                   return VERR_NOT_IMPLEMENTED;
        case DBGFREG_MM4:                                                   return VERR_NOT_IMPLEMENTED;
        case DBGFREG_MM5:                                                   return VERR_NOT_IMPLEMENTED;
        case DBGFREG_MM6:                                                   return VERR_NOT_IMPLEMENTED;
        case DBGFREG_MM7:                                                   return VERR_NOT_IMPLEMENTED;

        case DBGFREG_FCW:                                                   return VERR_NOT_IMPLEMENTED;
        case DBGFREG_FSW:                                                   return VERR_NOT_IMPLEMENTED;
        case DBGFREG_FTW:                                                   return VERR_NOT_IMPLEMENTED;
        case DBGFREG_FOP:                                                   return VERR_NOT_IMPLEMENTED;
        case DBGFREG_FPUIP:                                                 return VERR_NOT_IMPLEMENTED;
        case DBGFREG_FPUCS:                                                 return VERR_NOT_IMPLEMENTED;
        case DBGFREG_FPUDP:                                                 return VERR_NOT_IMPLEMENTED;
        case DBGFREG_FPUDS:                                                 return VERR_NOT_IMPLEMENTED;
        case DBGFREG_MXCSR:                                                 return VERR_NOT_IMPLEMENTED;
        case DBGFREG_MXCSR_MASK:                                            return VERR_NOT_IMPLEMENTED;

        case DBGFREG_XMM0:                                                  return VERR_NOT_IMPLEMENTED;
        case DBGFREG_XMM1:                                                  return VERR_NOT_IMPLEMENTED;
        case DBGFREG_XMM2:                                                  return VERR_NOT_IMPLEMENTED;
        case DBGFREG_XMM3:                                                  return VERR_NOT_IMPLEMENTED;
        case DBGFREG_XMM4:                                                  return VERR_NOT_IMPLEMENTED;
        case DBGFREG_XMM5:                                                  return VERR_NOT_IMPLEMENTED;
        case DBGFREG_XMM6:                                                  return VERR_NOT_IMPLEMENTED;
        case DBGFREG_XMM7:                                                  return VERR_NOT_IMPLEMENTED;
        case DBGFREG_XMM8:                                                  return VERR_NOT_IMPLEMENTED;
        case DBGFREG_XMM9:                                                  return VERR_NOT_IMPLEMENTED;
        case DBGFREG_XMM10:                                                 return VERR_NOT_IMPLEMENTED;
        case DBGFREG_XMM11:                                                 return VERR_NOT_IMPLEMENTED;
        case DBGFREG_XMM12:                                                 return VERR_NOT_IMPLEMENTED;
        case DBGFREG_XMM13:                                                 return VERR_NOT_IMPLEMENTED;
        case DBGFREG_XMM14:                                                 return VERR_NOT_IMPLEMENTED;
        case DBGFREG_XMM15:                                                 return VERR_NOT_IMPLEMENTED;

        case DBGFREG_GDTR:          *pu64 = pCtx->gdtr.pGdt;        *pfRegSizes = R_SZ_64_16;   return VINF_SUCCESS;
        case DBGFREG_GDTR_BASE:     *pu64 = pCtx->gdtr.pGdt;        *pfRegSizes = R_SZ_64;      return VINF_SUCCESS;
        case DBGFREG_GDTR_LIMIT:    *pu64 = pCtx->gdtr.cbGdt;       *pfRegSizes = R_SZ_32;      return VINF_SUCCESS;
        case DBGFREG_IDTR:          *pu64 = pCtx->idtr.pIdt;        *pfRegSizes = R_SZ_64_16;   return VINF_SUCCESS;
        case DBGFREG_IDTR_BASE:     *pu64 = pCtx->idtr.pIdt;        *pfRegSizes = R_SZ_64;      return VINF_SUCCESS;
        case DBGFREG_IDTR_LIMIT:    *pu64 = pCtx->idtr.cbIdt;       *pfRegSizes = R_SZ_32;      return VINF_SUCCESS;
        case DBGFREG_LDTR:          *pu64 = pCtx->ldtr;             *pfRegSizes = R_SZ_64;      return VINF_SUCCESS;
        case DBGFREG_LDTR_ATTR:     *pu64 = pCtx->ldtrHid.Attr.u;   *pfRegSizes = R_SZ_32;      return VINF_SUCCESS;
        case DBGFREG_LDTR_BASE:     *pu64 = pCtx->ldtrHid.u64Base;  *pfRegSizes = R_SZ_64;      return VINF_SUCCESS;
        case DBGFREG_LDTR_LIMIT:    *pu64 = pCtx->ldtrHid.u32Limit; *pfRegSizes = R_SZ_32;      return VINF_SUCCESS;
        case DBGFREG_TR:            *pu64 = pCtx->tr;               *pfRegSizes = R_SZ_16;      return VINF_SUCCESS;
        case DBGFREG_TR_ATTR:       *pu64 = pCtx->trHid.Attr.u;     *pfRegSizes = R_SZ_32;      return VINF_SUCCESS;
        case DBGFREG_TR_BASE:       *pu64 = pCtx->trHid.u64Base;    *pfRegSizes = R_SZ_64;      return VINF_SUCCESS;
        case DBGFREG_TR_LIMIT:      *pu64 = pCtx->trHid.u32Limit;   *pfRegSizes = R_SZ_32;      return VINF_SUCCESS;

        case DBGFREG_CR0:           *pu64 = CPUMGetGuestCR0(pVCpu); *pfRegSizes = R_SZ_32_OR_64; return VINF_SUCCESS;
        case DBGFREG_CR2:           *pu64 = CPUMGetGuestCR2(pVCpu); *pfRegSizes = R_SZ_32_OR_64; return VINF_SUCCESS;
        case DBGFREG_CR3:           *pu64 = CPUMGetGuestCR3(pVCpu); *pfRegSizes = R_SZ_32_OR_64; return VINF_SUCCESS;
        case DBGFREG_CR4:           *pu64 = CPUMGetGuestCR4(pVCpu); *pfRegSizes = R_SZ_32_OR_64; return VINF_SUCCESS;
        case DBGFREG_CR8:           *pu64 = CPUMGetGuestCR8(pVCpu); *pfRegSizes = R_SZ_32_OR_64; return VINF_SUCCESS;

        case DBGFREG_DR0:           *pu64 = CPUMGetGuestDR0(pVCpu); *pfRegSizes = R_SZ_32_OR_64; return VINF_SUCCESS;
        case DBGFREG_DR1:           *pu64 = CPUMGetGuestDR1(pVCpu); *pfRegSizes = R_SZ_32_OR_64; return VINF_SUCCESS;
        case DBGFREG_DR2:           *pu64 = CPUMGetGuestDR2(pVCpu); *pfRegSizes = R_SZ_32_OR_64; return VINF_SUCCESS;
        case DBGFREG_DR3:           *pu64 = CPUMGetGuestDR3(pVCpu); *pfRegSizes = R_SZ_32_OR_64; return VINF_SUCCESS;
        case DBGFREG_DR6:           *pu64 = CPUMGetGuestDR6(pVCpu); *pfRegSizes = R_SZ_32_OR_64; return VINF_SUCCESS;
        case DBGFREG_DR7:           *pu64 = CPUMGetGuestDR7(pVCpu); *pfRegSizes = R_SZ_32_OR_64; return VINF_SUCCESS;

        case DBGFREG_MSR_IA32_APICBASE:     return dbgfR3RegGetMsr(pVCpu, pu64, pfRegSizes, MSR_IA32_APICBASE);
        case DBGFREG_MSR_IA32_CR_PAT:       return dbgfR3RegGetMsr(pVCpu, pu64, pfRegSizes, MSR_IA32_CR_PAT);
        case DBGFREG_MSR_IA32_PERF_STATUS:  return dbgfR3RegGetMsr(pVCpu, pu64, pfRegSizes, MSR_IA32_PERF_STATUS);
        case DBGFREG_MSR_IA32_SYSENTER_CS:  return dbgfR3RegGetMsr(pVCpu, pu64, pfRegSizes, MSR_IA32_SYSENTER_CS);
        case DBGFREG_MSR_IA32_SYSENTER_EIP: return dbgfR3RegGetMsr(pVCpu, pu64, pfRegSizes, MSR_IA32_SYSENTER_EIP);
        case DBGFREG_MSR_IA32_SYSENTER_ESP: return dbgfR3RegGetMsr(pVCpu, pu64, pfRegSizes, MSR_IA32_SYSENTER_ESP);
        case DBGFREG_MSR_IA32_TSC:          return dbgfR3RegGetMsr(pVCpu, pu64, pfRegSizes, MSR_IA32_TSC);
        case DBGFREG_MSR_K6_EFER:           return dbgfR3RegGetMsr(pVCpu, pu64, pfRegSizes, MSR_K6_EFER);
        case DBGFREG_MSR_K6_STAR:           return dbgfR3RegGetMsr(pVCpu, pu64, pfRegSizes, MSR_K6_STAR);
        case DBGFREG_MSR_K8_CSTAR:          return dbgfR3RegGetMsr(pVCpu, pu64, pfRegSizes, MSR_K8_CSTAR);
        case DBGFREG_MSR_K8_FS_BASE:        return dbgfR3RegGetMsr(pVCpu, pu64, pfRegSizes, MSR_K8_FS_BASE);
        case DBGFREG_MSR_K8_GS_BASE:        return dbgfR3RegGetMsr(pVCpu, pu64, pfRegSizes, MSR_K8_GS_BASE);
        case DBGFREG_MSR_K8_KERNEL_GS_BASE: return dbgfR3RegGetMsr(pVCpu, pu64, pfRegSizes, MSR_K8_KERNEL_GS_BASE);
        case DBGFREG_MSR_K8_LSTAR:          return dbgfR3RegGetMsr(pVCpu, pu64, pfRegSizes, MSR_K8_LSTAR);
        case DBGFREG_MSR_K8_SF_MASK:        return dbgfR3RegGetMsr(pVCpu, pu64, pfRegSizes, MSR_K8_SF_MASK);
        case DBGFREG_MSR_K8_TSC_AUX:        return dbgfR3RegGetMsr(pVCpu, pu64, pfRegSizes, MSR_K8_TSC_AUX);

        case DBGFREG_END:
        case DBGFREG_32BIT_HACK:
            /* no default! We want GCC warnings. */
            break;
    }

    AssertMsgFailed(("%d (%#x)\n", enmReg, enmReg));
    return VERR_DBGF_INVALID_REGISTER;
}


/**
 * Queries a 8-bit register value.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_VM_HANDLE
 * @retval  VERR_INVALID_CPU_ID
 * @retval  VERR_DBGF_INVALID_REGISTER
 * @retval  VINF_DBGF_TRUNCATED_REGISTER
 *
 * @param   pVM                 The VM handle.
 * @param   idCpu               The target CPU ID.
 * @param   enmReg              The register that's being queried.
 * @param   pu8                 Where to store the register value.
 */
VMMR3DECL(int) DBGFR3RegQueryU8(PVM pVM, VMCPUID idCpu, DBGFREG enmReg, uint8_t *pu8)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(idCpu < pVM->cCpus, VERR_INVALID_CPU_ID);

    uint64_t u64Value;
    uint32_t fRegSizes;
    int rc = VMR3ReqCallWait(pVM, idCpu, (PFNRT)dbgfR3RegQueryWorker, 5, pVM, idCpu, enmReg, &u64Value, &fRegSizes);
    if (RT_SUCCESS(rc))
    {
        *pu8 = (uint8_t)u64Value;
        if (R_SZ_8 & fRegSizes)
            rc = VINF_SUCCESS;
        else
            rc = VINF_DBGF_TRUNCATED_REGISTER;
    }
    else
        *pu8 = 0;
    return rc;
}


/**
 * Queries a 16-bit register value.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_VM_HANDLE
 * @retval  VERR_INVALID_CPU_ID
 * @retval  VERR_DBGF_INVALID_REGISTER
 * @retval  VINF_DBGF_TRUNCATED_REGISTER
 * @retval  VINF_DBGF_ZERO_EXTENDED_REGISTER
 *
 * @param   pVM                 The VM handle.
 * @param   idCpu               The target CPU ID.
 * @param   enmReg              The register that's being queried.
 * @param   pu16                Where to store the register value.
 */
VMMR3DECL(int) DBGFR3RegQueryU16(PVM pVM, VMCPUID idCpu, DBGFREG enmReg, uint16_t *pu16)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(idCpu < pVM->cCpus, VERR_INVALID_CPU_ID);

    uint64_t u64Value;
    uint32_t fRegSizes;
    int rc = VMR3ReqCallWait(pVM, idCpu, (PFNRT)dbgfR3RegQueryWorker, 5, pVM, idCpu, enmReg, &u64Value, &fRegSizes);
    if (RT_SUCCESS(rc))
    {
        *pu16 = (uint16_t)u64Value;
        if (R_SZ_16 & fRegSizes)
            rc = VINF_SUCCESS;
        else if (~(R_SZ_8 | R_SZ_16) & fRegSizes)
            rc = VINF_DBGF_TRUNCATED_REGISTER;
        else
            rc = VINF_DBGF_ZERO_EXTENDED_REGISTER;
    }
    else
        *pu16 = 0;
    return rc;
}


/**
 * Queries a 32-bit register value.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_VM_HANDLE
 * @retval  VERR_INVALID_CPU_ID
 * @retval  VERR_DBGF_INVALID_REGISTER
 * @retval  VINF_DBGF_TRUNCATED_REGISTER
 * @retval  VINF_DBGF_ZERO_EXTENDED_REGISTER
 *
 * @param   pVM                 The VM handle.
 * @param   idCpu               The target CPU ID.
 * @param   enmReg              The register that's being queried.
 * @param   pu32                Where to store the register value.
 */
VMMR3DECL(int) DBGFR3RegQueryU32(PVM pVM, VMCPUID idCpu, DBGFREG enmReg, uint32_t *pu32)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(idCpu < pVM->cCpus, VERR_INVALID_CPU_ID);

    uint64_t u64Value;
    uint32_t fRegSizes;
    int rc = VMR3ReqCallWait(pVM, idCpu, (PFNRT)dbgfR3RegQueryWorker, 5, pVM, idCpu, enmReg, &u64Value, &fRegSizes);
    if (RT_SUCCESS(rc))
    {
        *pu32 = (uint32_t)u64Value;
        if (R_SZ_32 & fRegSizes)
            rc = VINF_SUCCESS;
        else if (~(R_SZ_8 | R_SZ_16 | R_SZ_32) & fRegSizes)
            rc = VINF_DBGF_TRUNCATED_REGISTER;
        else
            rc = VINF_DBGF_ZERO_EXTENDED_REGISTER;
    }
    else
        *pu32 = 0;
    return rc;
}


/**
 * Queries a 64-bit register value.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_INVALID_VM_HANDLE
 * @retval  VERR_INVALID_CPU_ID
 * @retval  VERR_DBGF_INVALID_REGISTER
 * @retval  VINF_DBGF_TRUNCATED_REGISTER
 * @retval  VINF_DBGF_ZERO_EXTENDED_REGISTER
 *
 * @param   pVM                 The VM handle.
 * @param   idCpu               The target CPU ID.
 * @param   enmReg              The register that's being queried.
 * @param   pu64                Where to store the register value.
 */
VMMR3DECL(int) DBGFR3RegQueryU64(PVM pVM, VMCPUID idCpu, DBGFREG enmReg, uint64_t *pu64)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(idCpu < pVM->cCpus, VERR_INVALID_CPU_ID);

    uint64_t u64Value;
    uint32_t fRegSizes;
    int rc = VMR3ReqCallWait(pVM, idCpu, (PFNRT)dbgfR3RegQueryWorker, 5, pVM, idCpu, enmReg, &u64Value, &fRegSizes);
    if (RT_SUCCESS(rc))
    {
        *pu64 = u64Value;
        if (R_SZ_64 & fRegSizes)
            rc = VINF_SUCCESS;
        else if (~(R_SZ_8 | R_SZ_16 | R_SZ_32 | R_SZ_64) & fRegSizes)
            rc = VINF_DBGF_TRUNCATED_REGISTER;
        else
            rc = VINF_DBGF_ZERO_EXTENDED_REGISTER;
    }
    else
        *pu64 = 0;
    return rc;
}

