/* $Id$ */
/** @file
 * NEM - Native execution manager, native ring-3 macOS backend using Hypervisor.framework, ARMv8 variant.
 *
 * Log group 2: Exit logging.
 * Log group 3: Log context on exit.
 * Log group 5: Ring-3 memory management
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_NEM
#define VMCPU_INCL_CPUM_GST_CTX
#define CPUM_WITH_NONCONST_HOST_FEATURES /* required for initializing parts of the g_CpumHostFeatures structure here. */
#include <VBox/vmm/nem.h>
#include <VBox/vmm/iem.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/gic.h>
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/dbgftrace.h>
#include <VBox/vmm/gcm.h>
#include "NEMInternal.h"
#include <VBox/vmm/vmcc.h>
#include "dtrace/VBoxVMM.h"

#include <iprt/armv8.h>
#include <iprt/asm.h>
#include <iprt/ldr.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/system.h>
#include <iprt/utf16.h>

#include <mach/mach_time.h>
#include <mach/kern_return.h>

#include <Hypervisor/Hypervisor.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


/** @todo The vTimer PPI for the virt platform, make it configurable. */
#define NEM_DARWIN_VTIMER_GIC_PPI_IRQ           11


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** The general registers. */
static const struct
{
    hv_reg_t    enmHvReg;
    uint32_t    fCpumExtrn;
    uint32_t    offCpumCtx;
} s_aCpumRegs[] =
{
#define CPUM_GREG_EMIT_X0_X3(a_Idx)  { HV_REG_X ## a_Idx, CPUMCTX_EXTRN_X ## a_Idx, RT_UOFFSETOF(CPUMCTX, aGRegs[a_Idx].x) }
#define CPUM_GREG_EMIT_X4_X28(a_Idx) { HV_REG_X ## a_Idx, CPUMCTX_EXTRN_X4_X28, RT_UOFFSETOF(CPUMCTX, aGRegs[a_Idx].x) }
    CPUM_GREG_EMIT_X0_X3(0),
    CPUM_GREG_EMIT_X0_X3(1),
    CPUM_GREG_EMIT_X0_X3(2),
    CPUM_GREG_EMIT_X0_X3(3),
    CPUM_GREG_EMIT_X4_X28(4),
    CPUM_GREG_EMIT_X4_X28(5),
    CPUM_GREG_EMIT_X4_X28(6),
    CPUM_GREG_EMIT_X4_X28(7),
    CPUM_GREG_EMIT_X4_X28(8),
    CPUM_GREG_EMIT_X4_X28(9),
    CPUM_GREG_EMIT_X4_X28(10),
    CPUM_GREG_EMIT_X4_X28(11),
    CPUM_GREG_EMIT_X4_X28(12),
    CPUM_GREG_EMIT_X4_X28(13),
    CPUM_GREG_EMIT_X4_X28(14),
    CPUM_GREG_EMIT_X4_X28(15),
    CPUM_GREG_EMIT_X4_X28(16),
    CPUM_GREG_EMIT_X4_X28(17),
    CPUM_GREG_EMIT_X4_X28(18),
    CPUM_GREG_EMIT_X4_X28(19),
    CPUM_GREG_EMIT_X4_X28(20),
    CPUM_GREG_EMIT_X4_X28(21),
    CPUM_GREG_EMIT_X4_X28(22),
    CPUM_GREG_EMIT_X4_X28(23),
    CPUM_GREG_EMIT_X4_X28(24),
    CPUM_GREG_EMIT_X4_X28(25),
    CPUM_GREG_EMIT_X4_X28(26),
    CPUM_GREG_EMIT_X4_X28(27),
    CPUM_GREG_EMIT_X4_X28(28),
    { HV_REG_FP,   CPUMCTX_EXTRN_FP,   RT_UOFFSETOF(CPUMCTX, aGRegs[29].x) },
    { HV_REG_LR,   CPUMCTX_EXTRN_LR,   RT_UOFFSETOF(CPUMCTX, aGRegs[30].x) },
    { HV_REG_PC,   CPUMCTX_EXTRN_PC,   RT_UOFFSETOF(CPUMCTX, Pc.u64)       },
    { HV_REG_FPCR, CPUMCTX_EXTRN_FPCR, RT_UOFFSETOF(CPUMCTX, fpcr)         },
    { HV_REG_FPSR, CPUMCTX_EXTRN_FPSR, RT_UOFFSETOF(CPUMCTX, fpsr)         }
#undef CPUM_GREG_EMIT_X0_X3
#undef CPUM_GREG_EMIT_X4_X28
};
/** SIMD/FP registers. */
static const struct
{
    hv_simd_fp_reg_t    enmHvReg;
    uint32_t            offCpumCtx;
} s_aCpumFpRegs[] =
{
#define CPUM_VREG_EMIT(a_Idx)  { HV_SIMD_FP_REG_Q ## a_Idx, RT_UOFFSETOF(CPUMCTX, aVRegs[a_Idx].v) }
    CPUM_VREG_EMIT(0),
    CPUM_VREG_EMIT(1),
    CPUM_VREG_EMIT(2),
    CPUM_VREG_EMIT(3),
    CPUM_VREG_EMIT(4),
    CPUM_VREG_EMIT(5),
    CPUM_VREG_EMIT(6),
    CPUM_VREG_EMIT(7),
    CPUM_VREG_EMIT(8),
    CPUM_VREG_EMIT(9),
    CPUM_VREG_EMIT(10),
    CPUM_VREG_EMIT(11),
    CPUM_VREG_EMIT(12),
    CPUM_VREG_EMIT(13),
    CPUM_VREG_EMIT(14),
    CPUM_VREG_EMIT(15),
    CPUM_VREG_EMIT(16),
    CPUM_VREG_EMIT(17),
    CPUM_VREG_EMIT(18),
    CPUM_VREG_EMIT(19),
    CPUM_VREG_EMIT(20),
    CPUM_VREG_EMIT(21),
    CPUM_VREG_EMIT(22),
    CPUM_VREG_EMIT(23),
    CPUM_VREG_EMIT(24),
    CPUM_VREG_EMIT(25),
    CPUM_VREG_EMIT(26),
    CPUM_VREG_EMIT(27),
    CPUM_VREG_EMIT(28),
    CPUM_VREG_EMIT(29),
    CPUM_VREG_EMIT(30),
    CPUM_VREG_EMIT(31)
#undef CPUM_VREG_EMIT
};
/** System registers. */
static const struct
{
    hv_sys_reg_t    enmHvReg;
    uint32_t        fCpumExtrn;
    uint32_t        offCpumCtx;
} s_aCpumSysRegs[] =
{
    { HV_SYS_REG_SP_EL0,    CPUMCTX_EXTRN_SP,               RT_UOFFSETOF(CPUMCTX, aSpReg[0].u64)    },
    { HV_SYS_REG_SP_EL1,    CPUMCTX_EXTRN_SP,               RT_UOFFSETOF(CPUMCTX, aSpReg[1].u64)    },
    { HV_SYS_REG_SPSR_EL1,  CPUMCTX_EXTRN_SPSR,             RT_UOFFSETOF(CPUMCTX, Spsr.u64)         },
    { HV_SYS_REG_ELR_EL1,   CPUMCTX_EXTRN_ELR,              RT_UOFFSETOF(CPUMCTX, Elr.u64)          },
    { HV_SYS_REG_SCTLR_EL1, CPUMCTX_EXTRN_SCTLR_TCR_TTBR,   RT_UOFFSETOF(CPUMCTX, Sctlr.u64)        },
    { HV_SYS_REG_TCR_EL1,   CPUMCTX_EXTRN_SCTLR_TCR_TTBR,   RT_UOFFSETOF(CPUMCTX, Tcr.u64)          },
    { HV_SYS_REG_TTBR0_EL1, CPUMCTX_EXTRN_SCTLR_TCR_TTBR,   RT_UOFFSETOF(CPUMCTX, Ttbr0.u64)        },
    { HV_SYS_REG_TTBR1_EL1, CPUMCTX_EXTRN_SCTLR_TCR_TTBR,   RT_UOFFSETOF(CPUMCTX, Ttbr1.u64)        },
};


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/


/**
 * Converts a HV return code to a VBox status code.
 *
 * @returns VBox status code.
 * @param   hrc                 The HV return code to convert.
 */
DECLINLINE(int) nemR3DarwinHvSts2Rc(hv_return_t hrc)
{
    if (hrc == HV_SUCCESS)
        return VINF_SUCCESS;

    switch (hrc)
    {
        case HV_ERROR:        return VERR_INVALID_STATE;
        case HV_BUSY:         return VERR_RESOURCE_BUSY;
        case HV_BAD_ARGUMENT: return VERR_INVALID_PARAMETER;
        case HV_NO_RESOURCES: return VERR_OUT_OF_RESOURCES;
        case HV_NO_DEVICE:    return VERR_NOT_FOUND;
        case HV_UNSUPPORTED:  return VERR_NOT_SUPPORTED;
    }

    return VERR_IPE_UNEXPECTED_STATUS;
}


/**
 * Returns a human readable string of the given exception class.
 *
 * @returns Pointer to the string matching the given EC.
 * @param   u32Ec           The exception class to return the string for.
 */
static const char *nemR3DarwinEsrEl2EcStringify(uint32_t u32Ec)
{
    switch (u32Ec)
    {
#define ARMV8_EC_CASE(a_Ec) case a_Ec: return #a_Ec
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_UNKNOWN);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_TRAPPED_WFX);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_AARCH32_TRAPPED_MCR_MRC_COPROC_15);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_AARCH32_TRAPPED_MCRR_MRRC_COPROC15);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_AARCH32_TRAPPED_MCR_MRC_COPROC_14);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_AARCH32_TRAPPED_LDC_STC);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_AARCH32_TRAPPED_SME_SVE_NEON);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_AARCH32_TRAPPED_VMRS);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_AARCH32_TRAPPED_PA_INSN);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_LS64_EXCEPTION);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_AARCH32_TRAPPED_MRRC_COPROC14);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_BTI_BRANCH_TARGET_EXCEPTION);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_ILLEGAL_EXECUTION_STATE);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_AARCH32_SVC_INSN);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_AARCH32_HVC_INSN);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_AARCH32_SMC_INSN);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_AARCH64_SVC_INSN);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_AARCH64_HVC_INSN);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_AARCH64_SMC_INSN);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_AARCH64_TRAPPED_SYS_INSN);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_SVE_TRAPPED);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_PAUTH_NV_TRAPPED_ERET_ERETAA_ERETAB);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_TME_TSTART_INSN_EXCEPTION);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_FPAC_PA_INSN_FAILURE_EXCEPTION);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_SME_TRAPPED_SME_ACCESS);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_RME_GRANULE_PROT_CHECK_EXCEPTION);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_INSN_ABORT_FROM_LOWER_EL);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_INSN_ABORT_FROM_EL2);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_PC_ALIGNMENT_EXCEPTION);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_DATA_ABORT_FROM_LOWER_EL);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_DATA_ABORT_FROM_EL2);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_SP_ALIGNMENT_EXCEPTION);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_MOPS_EXCEPTION);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_AARCH32_TRAPPED_FP_EXCEPTION);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_AARCH64_TRAPPED_FP_EXCEPTION);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_SERROR_INTERRUPT);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_BKPT_EXCEPTION_FROM_LOWER_EL);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_BKPT_EXCEPTION_FROM_EL2);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_SS_EXCEPTION_FROM_LOWER_EL);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_SS_EXCEPTION_FROM_EL2);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_WATCHPOINT_EXCEPTION_FROM_LOWER_EL);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_WATCHPOINT_EXCEPTION_FROM_EL2);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_AARCH32_BKPT_INSN);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_AARCH32_VEC_CATCH_EXCEPTION);
        ARMV8_EC_CASE(ARMV8_ESR_EL2_EC_AARCH64_BRK_INSN);
#undef ARMV8_EC_CASE
        default:
            break;
    }

    return "<INVALID>";
}


/**
 * Resolves a NEM page state from the given protection flags.
 *
 * @returns NEM page state.
 * @param   fPageProt           The page protection flags.
 */
DECLINLINE(uint8_t) nemR3DarwinPageStateFromProt(uint32_t fPageProt)
{
    switch (fPageProt)
    {
        case NEM_PAGE_PROT_NONE:
            return NEM_DARWIN_PAGE_STATE_UNMAPPED;
        case NEM_PAGE_PROT_READ | NEM_PAGE_PROT_EXECUTE:
            return NEM_DARWIN_PAGE_STATE_RX;
        case NEM_PAGE_PROT_READ | NEM_PAGE_PROT_WRITE:
            return NEM_DARWIN_PAGE_STATE_RW;
        case NEM_PAGE_PROT_READ | NEM_PAGE_PROT_WRITE | NEM_PAGE_PROT_EXECUTE:
            return NEM_DARWIN_PAGE_STATE_RWX;
        default:
            break;
    }

    AssertLogRelMsgFailed(("Invalid combination of page protection flags %#x, can't map to page state!\n", fPageProt));
    return NEM_DARWIN_PAGE_STATE_UNMAPPED;
}


/**
 * Unmaps the given guest physical address range (page aligned).
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   GCPhys              The guest physical address to start unmapping at.
 * @param   cb                  The size of the range to unmap in bytes.
 * @param   pu2State            Where to store the new state of the unmappd page, optional.
 */
DECLINLINE(int) nemR3DarwinUnmap(PVM pVM, RTGCPHYS GCPhys, size_t cb, uint8_t *pu2State)
{
    if (*pu2State <= NEM_DARWIN_PAGE_STATE_UNMAPPED)
    {
        Log5(("nemR3DarwinUnmap: %RGp == unmapped\n", GCPhys));
        *pu2State = NEM_DARWIN_PAGE_STATE_UNMAPPED;
        return VINF_SUCCESS;
    }

    LogFlowFunc(("Unmapping %RGp LB %zu\n", GCPhys, cb));
    hv_return_t hrc = hv_vm_unmap(GCPhys, cb);
    if (RT_LIKELY(hrc == HV_SUCCESS))
    {
        STAM_REL_COUNTER_INC(&pVM->nem.s.StatUnmapPage);
        if (pu2State)
            *pu2State = NEM_DARWIN_PAGE_STATE_UNMAPPED;
        Log5(("nemR3DarwinUnmap: %RGp => unmapped\n", GCPhys));
        return VINF_SUCCESS;
    }

    STAM_REL_COUNTER_INC(&pVM->nem.s.StatUnmapPageFailed);
    LogRel(("nemR3DarwinUnmap(%RGp): failed! hrc=%#x\n",
            GCPhys, hrc));
    return VERR_NEM_IPE_6;
}


/**
 * Maps a given guest physical address range backed by the given memory with the given
 * protection flags.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   GCPhys              The guest physical address to start mapping.
 * @param   pvRam               The R3 pointer of the memory to back the range with.
 * @param   cb                  The size of the range, page aligned.
 * @param   fPageProt           The page protection flags to use for this range, combination of NEM_PAGE_PROT_XXX
 * @param   pu2State            Where to store the state for the new page, optional.
 */
DECLINLINE(int) nemR3DarwinMap(PVM pVM, RTGCPHYS GCPhys, const void *pvRam, size_t cb, uint32_t fPageProt, uint8_t *pu2State)
{
    LogFlowFunc(("Mapping %RGp LB %zu fProt=%#x\n", GCPhys, cb, fPageProt));

    Assert(fPageProt != NEM_PAGE_PROT_NONE);
    RT_NOREF(pVM);

    hv_memory_flags_t fHvMemProt = 0;
    if (fPageProt & NEM_PAGE_PROT_READ)
        fHvMemProt |= HV_MEMORY_READ;
    if (fPageProt & NEM_PAGE_PROT_WRITE)
        fHvMemProt |= HV_MEMORY_WRITE;
    if (fPageProt & NEM_PAGE_PROT_EXECUTE)
        fHvMemProt |= HV_MEMORY_EXEC;

    hv_return_t hrc = hv_vm_map((void *)pvRam, GCPhys, cb, fHvMemProt);
    if (hrc == HV_SUCCESS)
    {
        if (pu2State)
            *pu2State = nemR3DarwinPageStateFromProt(fPageProt);
        return VINF_SUCCESS;
    }

    return nemR3DarwinHvSts2Rc(hrc);
}

#if 0 /* unused */
DECLINLINE(int) nemR3DarwinProtectPage(PVM pVM, RTGCPHYS GCPhys, size_t cb, uint32_t fPageProt)
{
    hv_memory_flags_t fHvMemProt = 0;
    if (fPageProt & NEM_PAGE_PROT_READ)
        fHvMemProt |= HV_MEMORY_READ;
    if (fPageProt & NEM_PAGE_PROT_WRITE)
        fHvMemProt |= HV_MEMORY_WRITE;
    if (fPageProt & NEM_PAGE_PROT_EXECUTE)
        fHvMemProt |= HV_MEMORY_EXEC;

    hv_return_t hrc;
    if (pVM->nem.s.fCreatedAsid)
        hrc = hv_vm_protect_space(pVM->nem.s.uVmAsid, GCPhys, cb, fHvMemProt);
    else
        hrc = hv_vm_protect(GCPhys, cb, fHvMemProt);

    return nemR3DarwinHvSts2Rc(hrc);
}
#endif

#ifdef LOG_ENABLED
/**
 * Logs the current CPU state.
 */
static void nemR3DarwinLogState(PVMCC pVM, PVMCPUCC pVCpu)
{
    if (LogIs3Enabled())
    {
        char szRegs[4096];
        DBGFR3RegPrintf(pVM->pUVM, pVCpu->idCpu, &szRegs[0], sizeof(szRegs),
                        "x0=%016VR{x0} x1=%016VR{x1} x2=%016VR{x2} x3=%016VR{x3}\n"
                        "x4=%016VR{x4} x5=%016VR{x5} x6=%016VR{x6} x7=%016VR{x7}\n"
                        "x8=%016VR{x8} x9=%016VR{x9} x10=%016VR{x10} x11=%016VR{x11}\n"
                        "x12=%016VR{x12} x13=%016VR{x13} x14=%016VR{x14} x15=%016VR{x15}\n"
                        "x16=%016VR{x16} x17=%016VR{x17} x18=%016VR{x18} x19=%016VR{x19}\n"
                        "x20=%016VR{x20} x21=%016VR{x21} x22=%016VR{x22} x23=%016VR{x23}\n"
                        "x24=%016VR{x24} x25=%016VR{x25} x26=%016VR{x26} x27=%016VR{x27}\n"
                        "x28=%016VR{x28} x29=%016VR{x29} x30=%016VR{x30}\n"
                        "pc=%016VR{pc} pstate=%016VR{pstate}\n"
                        "sp_el0=%016VR{sp_el0} sp_el1=%016VR{sp_el1} elr_el1=%016VR{elr_el1}\n"
                        "sctlr_el1=%016VR{sctlr_el1} tcr_el1=%016VR{tcr_el1}\n"
                        "ttbr0_el1=%016VR{ttbr0_el1} ttbr1_el1=%016VR{ttbr1_el1}\n"
                        );
        char szInstr[256]; RT_ZERO(szInstr);
#if 0
        DBGFR3DisasInstrEx(pVM->pUVM, pVCpu->idCpu, 0, 0,
                           DBGF_DISAS_FLAGS_CURRENT_GUEST | DBGF_DISAS_FLAGS_DEFAULT_MODE,
                           szInstr, sizeof(szInstr), NULL);
#endif
        Log3(("%s%s\n", szRegs, szInstr));
    }
}
#endif /* LOG_ENABLED */


static int nemR3DarwinCopyStateFromHv(PVMCC pVM, PVMCPUCC pVCpu, uint64_t fWhat)
{
    RT_NOREF(pVM);
    hv_return_t hrc = HV_SUCCESS;

    if (fWhat & (CPUMCTX_EXTRN_GPRS_MASK | CPUMCTX_EXTRN_PC | CPUMCTX_EXTRN_FPCR | CPUMCTX_EXTRN_FPSR))
    {
        for (uint32_t i = 0; i < RT_ELEMENTS(s_aCpumRegs); i++)
        {
            if (s_aCpumRegs[i].fCpumExtrn & fWhat)
            {
                uint64_t *pu64 = (uint64_t *)((uint8_t *)&pVCpu->cpum.GstCtx + s_aCpumRegs[i].offCpumCtx);
                hrc |= hv_vcpu_get_reg(pVCpu->nem.s.hVCpu, s_aCpumRegs[i].enmHvReg, pu64);
            }
        }
    }

    if (   hrc == HV_SUCCESS
        && (fWhat & CPUMCTX_EXTRN_V0_V31))
    {
        /* SIMD/FP registers. */
        for (uint32_t i = 0; i < RT_ELEMENTS(s_aCpumFpRegs); i++)
        {
            hv_simd_fp_uchar16_t *pu128 = (hv_simd_fp_uchar16_t *)((uint8_t *)&pVCpu->cpum.GstCtx + s_aCpumFpRegs[i].offCpumCtx);
            hrc |= hv_vcpu_get_simd_fp_reg(pVCpu->nem.s.hVCpu, s_aCpumFpRegs[i].enmHvReg, pu128);
        }
    }

    if (   hrc == HV_SUCCESS
        && (fWhat & (CPUMCTX_EXTRN_SPSR | CPUMCTX_EXTRN_ELR | CPUMCTX_EXTRN_SP | CPUMCTX_EXTRN_SCTLR_TCR_TTBR)))
    {
        /* System registers. */
        for (uint32_t i = 0; i < RT_ELEMENTS(s_aCpumSysRegs); i++)
        {
            if (s_aCpumSysRegs[i].fCpumExtrn & fWhat)
            {
                uint64_t *pu64 = (uint64_t *)((uint8_t *)&pVCpu->cpum.GstCtx + s_aCpumSysRegs[i].offCpumCtx);
                hrc |= hv_vcpu_get_sys_reg(pVCpu->nem.s.hVCpu, s_aCpumSysRegs[i].enmHvReg, pu64);
            }
        }
    }

    if (   hrc == HV_SUCCESS
        && (fWhat & CPUMCTX_EXTRN_PSTATE))
    {
        uint64_t u64Tmp;
        hrc |= hv_vcpu_get_reg(pVCpu->nem.s.hVCpu, HV_REG_CPSR, &u64Tmp);
        if (hrc == HV_SUCCESS)
            pVCpu->cpum.GstCtx.fPState = (uint32_t)u64Tmp;
    }

    /* Almost done, just update extern flags. */
    pVCpu->cpum.GstCtx.fExtrn &= ~fWhat;
    if (!(pVCpu->cpum.GstCtx.fExtrn & CPUMCTX_EXTRN_ALL))
        pVCpu->cpum.GstCtx.fExtrn = 0;

    return nemR3DarwinHvSts2Rc(hrc);
}


/**
 * Exports the guest state to HV for execution.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure of the
 *                          calling EMT.
 */
static int nemR3DarwinExportGuestState(PVMCC pVM, PVMCPUCC pVCpu)
{
    RT_NOREF(pVM);
    hv_return_t hrc = HV_SUCCESS;

    if (   (pVCpu->cpum.GstCtx.fExtrn & (CPUMCTX_EXTRN_GPRS_MASK | CPUMCTX_EXTRN_PC | CPUMCTX_EXTRN_FPCR | CPUMCTX_EXTRN_FPSR))
        !=                              (CPUMCTX_EXTRN_GPRS_MASK | CPUMCTX_EXTRN_PC | CPUMCTX_EXTRN_FPCR | CPUMCTX_EXTRN_FPSR))
    {
        for (uint32_t i = 0; i < RT_ELEMENTS(s_aCpumRegs); i++)
        {
            if (!(s_aCpumRegs[i].fCpumExtrn & pVCpu->cpum.GstCtx.fExtrn))
            {
                uint64_t *pu64 = (uint64_t *)((uint8_t *)&pVCpu->cpum.GstCtx + s_aCpumRegs[i].offCpumCtx);
                hrc |= hv_vcpu_set_reg(pVCpu->nem.s.hVCpu, s_aCpumRegs[i].enmHvReg, *pu64);
            }
        }
    }

    if (   hrc == HV_SUCCESS
        && !(pVCpu->cpum.GstCtx.fExtrn & CPUMCTX_EXTRN_V0_V31))
    {
        /* SIMD/FP registers. */
        for (uint32_t i = 0; i < RT_ELEMENTS(s_aCpumFpRegs); i++)
        {
            hv_simd_fp_uchar16_t *pu128 = (hv_simd_fp_uchar16_t *)((uint8_t *)&pVCpu->cpum.GstCtx + s_aCpumFpRegs[i].offCpumCtx);
            hrc |= hv_vcpu_set_simd_fp_reg(pVCpu->nem.s.hVCpu, s_aCpumFpRegs[i].enmHvReg, *pu128);
        }
    }

    if (   hrc == HV_SUCCESS
        &&     (pVCpu->cpum.GstCtx.fExtrn & (CPUMCTX_EXTRN_SPSR | CPUMCTX_EXTRN_ELR | CPUMCTX_EXTRN_SP | CPUMCTX_EXTRN_SCTLR_TCR_TTBR))
            !=                              (CPUMCTX_EXTRN_SPSR | CPUMCTX_EXTRN_ELR | CPUMCTX_EXTRN_SP | CPUMCTX_EXTRN_SCTLR_TCR_TTBR))
    {
        /* System registers. */
        for (uint32_t i = 0; i < RT_ELEMENTS(s_aCpumSysRegs); i++)
        {
            if (!(s_aCpumSysRegs[i].fCpumExtrn & pVCpu->cpum.GstCtx.fExtrn))
            {
                uint64_t *pu64 = (uint64_t *)((uint8_t *)&pVCpu->cpum.GstCtx + s_aCpumSysRegs[i].offCpumCtx);
                hrc |= hv_vcpu_set_sys_reg(pVCpu->nem.s.hVCpu, s_aCpumSysRegs[i].enmHvReg, *pu64);
            }
        }
    }

    if (   hrc == HV_SUCCESS
        && !(pVCpu->cpum.GstCtx.fExtrn & CPUMCTX_EXTRN_PSTATE))
        hrc = hv_vcpu_set_reg(pVCpu->nem.s.hVCpu, HV_REG_CPSR, pVCpu->cpum.GstCtx.fPState);

    pVCpu->cpum.GstCtx.fExtrn |= CPUMCTX_EXTRN_ALL | CPUMCTX_EXTRN_KEEPER_NEM;
    return nemR3DarwinHvSts2Rc(hrc);
}


/**
 * Try initialize the native API.
 *
 * This may only do part of the job, more can be done in
 * nemR3NativeInitAfterCPUM() and nemR3NativeInitCompleted().
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   fFallback       Whether we're in fallback mode or use-NEM mode. In
 *                          the latter we'll fail if we cannot initialize.
 * @param   fForced         Whether the HMForced flag is set and we should
 *                          fail if we cannot initialize.
 */
int nemR3NativeInit(PVM pVM, bool fFallback, bool fForced)
{
    AssertReturn(!pVM->nem.s.fCreatedVm, VERR_WRONG_ORDER);

    /*
     * Some state init.
     */
    PCFGMNODE pCfgNem = CFGMR3GetChild(CFGMR3GetRoot(pVM), "NEM/");
    RT_NOREF(pCfgNem);

    /*
     * Error state.
     * The error message will be non-empty on failure and 'rc' will be set too.
     */
    RTERRINFOSTATIC ErrInfo;
    PRTERRINFO pErrInfo = RTErrInfoInitStatic(&ErrInfo);

    int rc = VINF_SUCCESS;
    hv_return_t hrc = hv_vm_create(NULL);
    if (hrc == HV_SUCCESS)
    {
        pVM->nem.s.fCreatedVm = true;
        VM_SET_MAIN_EXECUTION_ENGINE(pVM, VM_EXEC_ENGINE_NATIVE_API);
        Log(("NEM: Marked active!\n"));
        PGMR3EnableNemMode(pVM);
    }
    else
        rc = RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED,
                           "hv_vm_create() failed: %#x", hrc);

    /*
     * We only fail if in forced mode, otherwise just log the complaint and return.
     */
    Assert(pVM->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API || RTErrInfoIsSet(pErrInfo));
    if (   (fForced || !fFallback)
        && pVM->bMainExecutionEngine != VM_EXEC_ENGINE_NATIVE_API)
        return VMSetError(pVM, RT_SUCCESS_NP(rc) ? VERR_NEM_NOT_AVAILABLE : rc, RT_SRC_POS, "%s", pErrInfo->pszMsg);

if (RTErrInfoIsSet(pErrInfo))
        LogRel(("NEM: Not available: %s\n", pErrInfo->pszMsg));
    return VINF_SUCCESS;
}


/**
 * Worker to create the vCPU handle on the EMT running it later on (as required by HV).
 *
 * @returns VBox status code
 * @param   pVM                 The VM handle.
 * @param   pVCpu               The vCPU handle.
 * @param   idCpu               ID of the CPU to create.
 */
static DECLCALLBACK(int) nemR3DarwinNativeInitVCpuOnEmt(PVM pVM, PVMCPU pVCpu, VMCPUID idCpu)
{
    hv_return_t hrc = hv_vcpu_create(&pVCpu->nem.s.hVCpu, &pVCpu->nem.s.pHvExit, NULL);
    if (hrc != HV_SUCCESS)
        return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS,
                          "Call to hv_vcpu_create failed on vCPU %u: %#x (%Rrc)", idCpu, hrc, nemR3DarwinHvSts2Rc(hrc));

    if (idCpu == 0)
    {
        /** @todo */
    }

    return VINF_SUCCESS;
}


/**
 * Worker to destroy the vCPU handle on the EMT running it later on (as required by HV).
 *
 * @returns VBox status code
 * @param   pVCpu               The vCPU handle.
 */
static DECLCALLBACK(int) nemR3DarwinNativeTermVCpuOnEmt(PVMCPU pVCpu)
{
    hv_return_t hrc = hv_vcpu_destroy(pVCpu->nem.s.hVCpu);
    Assert(hrc == HV_SUCCESS); RT_NOREF(hrc);
    return VINF_SUCCESS;
}


/**
 * This is called after CPUMR3Init is done.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle..
 */
int nemR3NativeInitAfterCPUM(PVM pVM)
{
    /*
     * Validate sanity.
     */
    AssertReturn(!pVM->nem.s.fCreatedEmts, VERR_WRONG_ORDER);
    AssertReturn(pVM->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API, VERR_WRONG_ORDER);

    /*
     * Setup the EMTs.
     */
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU pVCpu = pVM->apCpusR3[idCpu];

        int rc = VMR3ReqCallWait(pVM, idCpu, (PFNRT)nemR3DarwinNativeInitVCpuOnEmt, 3, pVM, pVCpu, idCpu);
        if (RT_FAILURE(rc))
        {
            /* Rollback. */
            while (idCpu--)
                VMR3ReqCallWait(pVM, idCpu, (PFNRT)nemR3DarwinNativeTermVCpuOnEmt, 1, pVCpu);

            return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS, "Call to hv_vcpu_create failed: %Rrc", rc);
        }
    }

    pVM->nem.s.fCreatedEmts = true;
    return VINF_SUCCESS;
}


int nemR3NativeInitCompleted(PVM pVM, VMINITCOMPLETED enmWhat)
{
    RT_NOREF(pVM, enmWhat);
    return VINF_SUCCESS;
}


int nemR3NativeTerm(PVM pVM)
{
    /*
     * Delete the VM.
     */

    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu--)
    {
        PVMCPU pVCpu = pVM->apCpusR3[idCpu];

        /*
         * Apple's documentation states that the vCPU should be destroyed
         * on the thread running the vCPU but as all the other EMTs are gone
         * at this point, destroying the VM would hang.
         *
         * We seem to be at luck here though as destroying apparently works
         * from EMT(0) as well.
         */
        hv_return_t hrc = hv_vcpu_destroy(pVCpu->nem.s.hVCpu);
        Assert(hrc == HV_SUCCESS); RT_NOREF(hrc);
    }

    pVM->nem.s.fCreatedEmts = false;
    if (pVM->nem.s.fCreatedVm)
    {
        hv_return_t hrc = hv_vm_destroy();
        if (hrc != HV_SUCCESS)
            LogRel(("NEM: hv_vm_destroy() failed with %#x\n", hrc));

        pVM->nem.s.fCreatedVm = false;
    }
    return VINF_SUCCESS;
}


/**
 * VM reset notification.
 *
 * @param   pVM         The cross context VM structure.
 */
void nemR3NativeReset(PVM pVM)
{
    RT_NOREF(pVM);
}


/**
 * Reset CPU due to INIT IPI or hot (un)plugging.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the CPU being
 *                      reset.
 * @param   fInitIpi    Whether this is the INIT IPI or hot (un)plugging case.
 */
void nemR3NativeResetCpu(PVMCPU pVCpu, bool fInitIpi)
{
    RT_NOREF(pVCpu, fInitIpi);
}


/**
 * Returns the byte size from the given access SAS value.
 *
 * @returns Number of bytes to transfer.
 * @param   uSas            The SAS value to convert.
 */
DECLINLINE(size_t) nemR3DarwinGetByteCountFromSas(uint8_t uSas)
{
    switch (uSas)
    {
        case ARMV8_EC_ISS_DATA_ABRT_SAS_BYTE:     return sizeof(uint8_t);
        case ARMV8_EC_ISS_DATA_ABRT_SAS_HALFWORD: return sizeof(uint16_t);
        case ARMV8_EC_ISS_DATA_ABRT_SAS_WORD:     return sizeof(uint32_t);
        case ARMV8_EC_ISS_DATA_ABRT_SAS_DWORD:    return sizeof(uint64_t);
        default:
            AssertReleaseFailed();
    }

    return 0;
}


/**
 * Sets the given general purpose register to the given value.
 *
 * @param   pVCpu           The cross context virtual CPU structure of the
 *                          calling EMT.
 * @param   uReg            The register index.
 * @param   f64BitReg       Flag whether to operate on a 64-bit or 32-bit register.
 * @param   fSignExtend     Flag whether to sign extend the value.
 * @param   u64Val          The value.
 */
DECLINLINE(void) nemR3DarwinSetGReg(PVMCPU pVCpu, uint8_t uReg, bool f64BitReg, bool fSignExtend, uint64_t u64Val)
{
    AssertReturnVoid(uReg < 31);

    if (f64BitReg)
        pVCpu->cpum.GstCtx.aGRegs[uReg].x = fSignExtend ? (int64_t)u64Val : u64Val;
    else
        pVCpu->cpum.GstCtx.aGRegs[uReg].w = fSignExtend ? (int32_t)u64Val : u64Val; /** @todo Does this clear the upper half on real hardware? */

    /* Mark the register as not extern anymore. */
    switch (uReg)
    {
        case 0:
            pVCpu->cpum.GstCtx.fExtrn &= ~CPUMCTX_EXTRN_X0;
            break;
        case 1:
            pVCpu->cpum.GstCtx.fExtrn &= ~CPUMCTX_EXTRN_X1;
            break;
        case 2:
            pVCpu->cpum.GstCtx.fExtrn &= ~CPUMCTX_EXTRN_X2;
            break;
        case 3:
            pVCpu->cpum.GstCtx.fExtrn &= ~CPUMCTX_EXTRN_X3;
            break;
        default:
            AssertRelease(!(pVCpu->cpum.GstCtx.fExtrn & CPUMCTX_EXTRN_X4_X28));
            /** @todo We need to import all missing registers in order to clear this flag (or just set it in HV from here). */
    }
}


/**
 * Gets the given general purpose register and returns the value.
 *
 * @returns Value from the given register.
 * @param   pVCpu           The cross context virtual CPU structure of the
 *                          calling EMT.
 * @param   uReg            The register index.
 */
DECLINLINE(uint64_t) nemR3DarwinGetGReg(PVMCPU pVCpu, uint8_t uReg)
{
    AssertReturn(uReg <= ARMV8_AARCH64_REG_ZR, 0);

    if (uReg == ARMV8_AARCH64_REG_ZR)
        return 0;

    /** @todo Import the register if extern. */
    AssertRelease(!(pVCpu->cpum.GstCtx.fExtrn & CPUMCTX_EXTRN_GPRS_MASK));

    return pVCpu->cpum.GstCtx.aGRegs[uReg].x;
}


/**
 * Works on the data abort exception (which will be a MMIO access most of the time).
 *
 * @returns VBox strict status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure of the
 *                          calling EMT.
 * @param   uIss            The instruction specific syndrome value.
 * @param   fInsn32Bit      Flag whether the exception was caused by a 32-bit or 16-bit instruction.
 * @param   GCPtrDataAbrt   The virtual GC address causing the data abort.
 * @param   GCPhysDataAbrt  The physical GC address which caused the data abort.
 */
static VBOXSTRICTRC nemR3DarwinHandleExitExceptionDataAbort(PVM pVM, PVMCPU pVCpu, uint32_t uIss, bool fInsn32Bit,
                                                            RTGCPTR GCPtrDataAbrt, RTGCPHYS GCPhysDataAbrt)
{
    bool fIsv        = RT_BOOL(uIss & ARMV8_EC_ISS_DATA_ABRT_ISV);
    bool fL2Fault    = RT_BOOL(uIss & ARMV8_EC_ISS_DATA_ABRT_S1PTW);
    bool fWrite      = RT_BOOL(uIss & ARMV8_EC_ISS_DATA_ABRT_WNR);
    bool f64BitReg   = RT_BOOL(uIss & ARMV8_EC_ISS_DATA_ABRT_SF);
    bool fSignExtend = RT_BOOL(uIss & ARMV8_EC_ISS_DATA_ABRT_SSE);
    uint8_t uReg     = ARMV8_EC_ISS_DATA_ABRT_SRT_GET(uIss);
    uint8_t uAcc     = ARMV8_EC_ISS_DATA_ABRT_SAS_GET(uIss);
    size_t cbAcc     = nemR3DarwinGetByteCountFromSas(uAcc);
    LogFlowFunc(("fIsv=%RTbool fL2Fault=%RTbool fWrite=%RTbool f64BitReg=%RTbool fSignExtend=%RTbool uReg=%u uAcc=%u GCPtrDataAbrt=%RGv GCPhysDataAbrt=%RGp\n",
                 fIsv, fL2Fault, fWrite, f64BitReg, fSignExtend, uReg, uAcc, GCPtrDataAbrt, GCPhysDataAbrt));

    AssertReturn(fIsv, VERR_NOT_SUPPORTED); /** @todo Implement using IEM when this should occur. */

    EMHistoryAddExit(pVCpu,
                     fWrite
                     ? EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM, EMEXITTYPE_MMIO_WRITE)
                     : EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM, EMEXITTYPE_MMIO_READ),
                     pVCpu->cpum.GstCtx.Pc.u64, ASMReadTSC());

    VBOXSTRICTRC rcStrict = VINF_SUCCESS;
    uint64_t u64Val = 0;
    if (fWrite)
    {
        u64Val = nemR3DarwinGetGReg(pVCpu, uReg);
        rcStrict = PGMPhysWrite(pVM, GCPhysDataAbrt, &u64Val, cbAcc, PGMACCESSORIGIN_HM);
        Log4(("MmioExit/%u: %08RX64: WRITE %#x LB %u, %.*Rhxs -> rcStrict=%Rrc\n",
              pVCpu->idCpu, pVCpu->cpum.GstCtx.Pc.u64, GCPhysDataAbrt, cbAcc, cbAcc,
              &u64Val, VBOXSTRICTRC_VAL(rcStrict) ));
    }
    else
    {
        rcStrict = PGMPhysRead(pVM, GCPhysDataAbrt, &u64Val, cbAcc, PGMACCESSORIGIN_HM);
        Log4(("MmioExit/%u: %08RX64: READ %#x LB %u -> %.*Rhxs rcStrict=%Rrc\n",
              pVCpu->idCpu, pVCpu->cpum.GstCtx.Pc.u64, GCPhysDataAbrt, cbAcc, cbAcc,
              &u64Val, VBOXSTRICTRC_VAL(rcStrict) ));
        if (rcStrict == VINF_SUCCESS)
            nemR3DarwinSetGReg(pVCpu, uReg, f64BitReg, fSignExtend, u64Val);
    }

    if (rcStrict == VINF_SUCCESS)
        pVCpu->cpum.GstCtx.Pc.u64 += fInsn32Bit ? sizeof(uint32_t) : sizeof(uint16_t);

    return rcStrict;
}


/**
 * Works on the trapped MRS, MSR and system instruction exception.
 *
 * @returns VBox strict status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure of the
 *                          calling EMT.
 * @param   uIss            The instruction specific syndrome value.
 * @param   fInsn32Bit      Flag whether the exception was caused by a 32-bit or 16-bit instruction.
 */
static VBOXSTRICTRC nemR3DarwinHandleExitExceptionTrappedSysInsn(PVM pVM, PVMCPU pVCpu, uint32_t uIss, bool fInsn32Bit)
{
    bool fRead   = ARMV8_EC_ISS_AARCH64_TRAPPED_SYS_INSN_DIRECTION_IS_READ(uIss);
    uint8_t uCRm = ARMV8_EC_ISS_AARCH64_TRAPPED_SYS_INSN_CRM_GET(uIss);
    uint8_t uReg = ARMV8_EC_ISS_AARCH64_TRAPPED_SYS_INSN_RT_GET(uIss);
    uint8_t uCRn = ARMV8_EC_ISS_AARCH64_TRAPPED_SYS_INSN_CRN_GET(uIss);
    uint8_t uOp1 = ARMV8_EC_ISS_AARCH64_TRAPPED_SYS_INSN_OP1_GET(uIss);
    uint8_t uOp2 = ARMV8_EC_ISS_AARCH64_TRAPPED_SYS_INSN_OP2_GET(uIss);
    uint8_t uOp0 = ARMV8_EC_ISS_AARCH64_TRAPPED_SYS_INSN_OP0_GET(uIss);
    uint16_t idSysReg = ARMV8_AARCH64_SYSREG_ID_CREATE(uOp0, uOp1, uCRn, uCRm, uOp2);
    LogFlowFunc(("fRead=%RTbool uCRm=%u uReg=%u uCRn=%u uOp1=%u uOp2=%u uOp0=%u idSysReg=%#x\n",
                 fRead, uCRm, uReg, uCRn, uOp1, uOp2, uOp0, idSysReg));

    /** @todo EMEXITTYPE_MSR_READ/EMEXITTYPE_MSR_WRITE are misnomers. */
    EMHistoryAddExit(pVCpu,
                     fRead
                     ? EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM, EMEXITTYPE_MSR_READ)
                     : EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM, EMEXITTYPE_MSR_WRITE),
                     pVCpu->cpum.GstCtx.Pc.u64, ASMReadTSC());

    VBOXSTRICTRC rcStrict = VINF_SUCCESS;
    uint64_t u64Val = 0;
    if (fRead)
    {
        RT_NOREF(pVM);
        rcStrict = CPUMQueryGuestSysReg(pVCpu, idSysReg, &u64Val);
        Log4(("SysInsnExit/%u: %08RX64: READ %u:%u:%u:%u:%u -> %#RX64 rcStrict=%Rrc\n",
              pVCpu->idCpu, pVCpu->cpum.GstCtx.Pc.u64, uOp0, uOp1, uCRn, uCRm, uOp2, u64Val,
              VBOXSTRICTRC_VAL(rcStrict) ));
        if (rcStrict == VINF_SUCCESS)
            nemR3DarwinSetGReg(pVCpu, uReg, true /*f64BitReg*/, false /*fSignExtend*/, u64Val);
    }
    else
    {
        u64Val = nemR3DarwinGetGReg(pVCpu, uReg);
        rcStrict = CPUMSetGuestSysReg(pVCpu, idSysReg, u64Val);
        Log4(("SysInsnExit/%u: %08RX64: WRITE %u:%u:%u:%u:%u %#RX64 -> rcStrict=%Rrc\n",
              pVCpu->idCpu, pVCpu->cpum.GstCtx.Pc.u64, uOp0, uOp1, uCRn, uCRm, uOp2, u64Val,
              VBOXSTRICTRC_VAL(rcStrict) ));
    }

    if (rcStrict == VINF_SUCCESS)
        pVCpu->cpum.GstCtx.Pc.u64 += fInsn32Bit ? sizeof(uint32_t) : sizeof(uint16_t);

    return rcStrict;
}


/**
 * Works on the trapped HVC instruction exception.
 *
 * @returns VBox strict status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure of the
 *                          calling EMT.
 * @param   uIss            The instruction specific syndrome value.
 */
static VBOXSTRICTRC nemR3DarwinHandleExitExceptionTrappedHvcInsn(PVM pVM, PVMCPU pVCpu, uint32_t uIss)
{
    uint16_t u16Imm = ARMV8_EC_ISS_AARCH64_TRAPPED_HVC_INSN_IMM_GET(uIss);
    LogFlowFunc(("u16Imm=%#RX16\n", u16Imm));

#if 0 /** @todo For later */
    EMHistoryAddExit(pVCpu,
                     fRead
                     ? EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM, EMEXITTYPE_MSR_READ)
                     : EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM, EMEXITTYPE_MSR_WRITE),
                     pVCpu->cpum.GstCtx.Pc.u64, ASMReadTSC());
#endif

    RT_NOREF(pVM);
    VBOXSTRICTRC rcStrict = VINF_SUCCESS;
    /** @todo Raise exception to EL1 if PSCI not configured. */
    /** @todo Need a generic mechanism here to pass this to, GIM maybe?. Always return -1 for now (PSCI). */
    nemR3DarwinSetGReg(pVCpu, ARMV8_AARCH64_REG_X0, true /*f64BitReg*/, false /*fSignExtend*/, (uint64_t)-1);

    return rcStrict;
}


/**
 * Handles an exception VM exit.
 *
 * @returns VBox strict status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure of the
 *                          calling EMT.
 * @param   pExit           Pointer to the exit information.
 */
static VBOXSTRICTRC nemR3DarwinHandleExitException(PVM pVM, PVMCPU pVCpu, const hv_vcpu_exit_t *pExit)
{
    uint32_t uEc = ARMV8_ESR_EL2_EC_GET(pExit->exception.syndrome);
    uint32_t uIss = ARMV8_ESR_EL2_ISS_GET(pExit->exception.syndrome);
    bool fInsn32Bit = ARMV8_ESR_EL2_IL_IS_32BIT(pExit->exception.syndrome);

    LogFlowFunc(("pVM=%p pVCpu=%p{.idCpu=%u} uEc=%u{%s} uIss=%#RX32 fInsn32Bit=%RTbool\n",
                 pVM, pVCpu, pVCpu->idCpu, uEc, nemR3DarwinEsrEl2EcStringify(uEc), uIss, fInsn32Bit));

    switch (uEc)
    {
        case ARMV8_ESR_EL2_DATA_ABORT_FROM_LOWER_EL:
            return nemR3DarwinHandleExitExceptionDataAbort(pVM, pVCpu, uIss, fInsn32Bit, pExit->exception.virtual_address,
                                                           pExit->exception.physical_address);
        case ARMV8_ESR_EL2_EC_AARCH64_TRAPPED_SYS_INSN:
            return nemR3DarwinHandleExitExceptionTrappedSysInsn(pVM, pVCpu, uIss, fInsn32Bit);
        case ARMV8_ESR_EL2_EC_AARCH64_HVC_INSN:
            return nemR3DarwinHandleExitExceptionTrappedHvcInsn(pVM, pVCpu, uIss);
        case ARMV8_ESR_EL2_EC_TRAPPED_WFX:
            return VINF_EM_HALT;
        case ARMV8_ESR_EL2_EC_UNKNOWN:
        default:
            LogRel(("NEM/Darwin: Unknown Exception Class in syndrome: uEc=%u{%s} uIss=%#RX32 fInsn32Bit=%RTbool\n",
                    uEc, nemR3DarwinEsrEl2EcStringify(uEc), uIss, fInsn32Bit));
            AssertReleaseFailed();
            return VERR_NOT_IMPLEMENTED;
    }

    return VINF_SUCCESS;
}


/**
 * Handles an exit from hv_vcpu_run().
 *
 * @returns VBox strict status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure of the
 *                          calling EMT.
 */
static VBOXSTRICTRC nemR3DarwinHandleExit(PVM pVM, PVMCPU pVCpu)
{
    int rc = nemR3DarwinCopyStateFromHv(pVM, pVCpu, CPUMCTX_EXTRN_ALL);
    if (RT_FAILURE(rc))
        return rc;

#ifdef LOG_ENABLED
    if (LogIs3Enabled())
        nemR3DarwinLogState(pVM, pVCpu);
#endif

    hv_vcpu_exit_t *pExit = pVCpu->nem.s.pHvExit;
    switch (pExit->reason)
    {
        case HV_EXIT_REASON_CANCELED:
            return VINF_EM_RAW_INTERRUPT;
        case HV_EXIT_REASON_EXCEPTION:
            return nemR3DarwinHandleExitException(pVM, pVCpu, pExit);
        case HV_EXIT_REASON_VTIMER_ACTIVATED:
            pVCpu->nem.s.fVTimerActivated = true;
            return GICPpiSet(pVCpu, NEM_DARWIN_VTIMER_GIC_PPI_IRQ, true /*fAsserted*/);
        default:
            AssertReleaseFailed();
            break;
    }

    return VERR_INVALID_STATE;
}


/**
 * Runs the guest once until an exit occurs.
 *
 * @returns HV status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure.
 */
static hv_return_t nemR3DarwinRunGuest(PVM pVM, PVMCPU pVCpu)
{
    TMNotifyStartOfExecution(pVM, pVCpu);

    hv_return_t hrc = hv_vcpu_run(pVCpu->nem.s.hVCpu);

    TMNotifyEndOfExecution(pVM, pVCpu, ASMReadTSC());

    return hrc;
}


/**
 * Prepares the VM to run the guest.
 *
 * @returns Strict VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   fSingleStepping     Flag whether we run in single stepping mode.
 */
static VBOXSTRICTRC nemR3DarwinPreRunGuest(PVM pVM, PVMCPU pVCpu, bool fSingleStepping)
{
#ifdef LOG_ENABLED
    bool fIrq = false;
    bool fFiq = false;

    if (LogIs3Enabled())
        nemR3DarwinLogState(pVM, pVCpu);
#endif

    /** @todo */ RT_NOREF(fSingleStepping);
    int rc = nemR3DarwinExportGuestState(pVM, pVCpu);
    AssertRCReturn(rc, rc);

    /* Check whether the vTimer interrupt was handled by the guest and we can unmask the vTimer. */
    if (pVCpu->nem.s.fVTimerActivated)
    {
        /* Read the CNTV_CTL_EL0 register. */
        uint64_t u64CntvCtl = 0;

        hv_return_t hrc = hv_vcpu_get_sys_reg(pVCpu->nem.s.hVCpu, HV_SYS_REG_CNTV_CTL_EL0, &u64CntvCtl);
        AssertRCReturn(hrc == HV_SUCCESS, VERR_NEM_IPE_9);

        if (   (u64CntvCtl & (ARMV8_CNTV_CTL_EL0_AARCH64_ENABLE | ARMV8_CNTV_CTL_EL0_AARCH64_IMASK | ARMV8_CNTV_CTL_EL0_AARCH64_ISTATUS))
            != (ARMV8_CNTV_CTL_EL0_AARCH64_ENABLE | ARMV8_CNTV_CTL_EL0_AARCH64_ISTATUS))
        {
            /* Clear the interrupt. */
            GICPpiSet(pVCpu, NEM_DARWIN_VTIMER_GIC_PPI_IRQ, false /*fAsserted*/);

            pVCpu->nem.s.fVTimerActivated = false;
            hrc = hv_vcpu_set_vtimer_mask(pVCpu->nem.s.hVCpu, false /*vtimer_is_masked*/);
            AssertReturn(hrc == HV_SUCCESS, VERR_NEM_IPE_9);
        }
    }

    /* Set the pending interrupt state. */
    if (VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_INTERRUPT_IRQ | VMCPU_FF_INTERRUPT_FIQ))
    {
        hv_return_t hrc = HV_SUCCESS;

        if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INTERRUPT_IRQ))
        {
            hrc = hv_vcpu_set_pending_interrupt(pVCpu->nem.s.hVCpu, HV_INTERRUPT_TYPE_IRQ, true);
            AssertReturn(hrc == HV_SUCCESS, VERR_NEM_IPE_9);
#ifdef LOG_ENABLED
            fIrq = true;
#endif
        }

        if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INTERRUPT_FIQ))
        {
            hrc = hv_vcpu_set_pending_interrupt(pVCpu->nem.s.hVCpu, HV_INTERRUPT_TYPE_FIQ, true);
            AssertReturn(hrc == HV_SUCCESS, VERR_NEM_IPE_9);
#ifdef LOG_ENABLED
            fFiq = true;
#endif
        }
    }
    else
    {
        hv_return_t hrc = hv_vcpu_set_pending_interrupt(pVCpu->nem.s.hVCpu, HV_INTERRUPT_TYPE_IRQ, false);
        AssertReturn(hrc == HV_SUCCESS, VERR_NEM_IPE_9);

        hrc = hv_vcpu_set_pending_interrupt(pVCpu->nem.s.hVCpu, HV_INTERRUPT_TYPE_FIQ, false);
        AssertReturn(hrc == HV_SUCCESS, VERR_NEM_IPE_9);
    }

    LogFlowFunc(("Running vCPU [%s,%s]\n", fIrq ? "I" : "nI", fFiq ? "F" : "nF"));
    pVCpu->nem.s.fEventPending = false;
    return VINF_SUCCESS;
}


/**
 * The normal runloop (no debugging features enabled).
 *
 * @returns Strict VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
static VBOXSTRICTRC nemR3DarwinRunGuestNormal(PVM pVM, PVMCPU pVCpu)
{
    /*
     * The run loop.
     *
     * Current approach to state updating to use the sledgehammer and sync
     * everything every time.  This will be optimized later.
     */

    /*
     * Poll timers and run for a bit.
     */
    /** @todo See if we cannot optimize this TMTimerPollGIP by only redoing
     *        the whole polling job when timers have changed... */
    uint64_t       offDeltaIgnored;
    uint64_t const nsNextTimerEvt = TMTimerPollGIP(pVM, pVCpu, &offDeltaIgnored); NOREF(nsNextTimerEvt);
    VBOXSTRICTRC    rcStrict        = VINF_SUCCESS;
    for (unsigned iLoop = 0;; iLoop++)
    {
        rcStrict = nemR3DarwinPreRunGuest(pVM, pVCpu, false /* fSingleStepping */);
        if (rcStrict != VINF_SUCCESS)
            break;

        hv_return_t hrc = nemR3DarwinRunGuest(pVM, pVCpu);
        if (hrc == HV_SUCCESS)
        {
            /*
             * Deal with the message.
             */
            rcStrict = nemR3DarwinHandleExit(pVM, pVCpu);
            if (rcStrict == VINF_SUCCESS)
            { /* hopefully likely */ }
            else
            {
                LogFlow(("NEM/%u: breaking: nemR3DarwinHandleExit -> %Rrc\n", pVCpu->idCpu, VBOXSTRICTRC_VAL(rcStrict) ));
                STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatBreakOnStatus);
                break;
            }
        }
        else
        {
            AssertLogRelMsgFailedReturn(("hv_vcpu_run()) failed for CPU #%u: %#x \n",
                                        pVCpu->idCpu, hrc), VERR_NEM_IPE_0);
        }
    } /* the run loop */

    return rcStrict;
}


VBOXSTRICTRC nemR3NativeRunGC(PVM pVM, PVMCPU pVCpu)
{
#ifdef LOG_ENABLED
    if (LogIs3Enabled())
        nemR3DarwinLogState(pVM, pVCpu);
#endif

    AssertReturn(NEMR3CanExecuteGuest(pVM, pVCpu), VERR_NEM_IPE_9);

    /*
     * Try switch to NEM runloop state.
     */
    if (VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM, VMCPUSTATE_STARTED))
    { /* likely */ }
    else
    {
        VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM, VMCPUSTATE_STARTED_EXEC_NEM_CANCELED);
        LogFlow(("NEM/%u: returning immediately because canceled\n", pVCpu->idCpu));
        return VINF_SUCCESS;
    }

    VBOXSTRICTRC rcStrict;
#if 0
    if (   !pVCpu->nem.s.fUseDebugLoop
        && !nemR3DarwinAnyExpensiveProbesEnabled()
        && !DBGFIsStepping(pVCpu)
        && !pVCpu->CTX_SUFF(pVM)->dbgf.ro.cEnabledInt3Breakpoints)
#endif
        rcStrict = nemR3DarwinRunGuestNormal(pVM, pVCpu);
#if 0
    else
        rcStrict = nemR3DarwinRunGuestDebug(pVM, pVCpu);
#endif

    if (rcStrict == VINF_EM_RAW_TO_R3)
        rcStrict = VINF_SUCCESS;

    /*
     * Convert any pending HM events back to TRPM due to premature exits.
     *
     * This is because execution may continue from IEM and we would need to inject
     * the event from there (hence place it back in TRPM).
     */
    if (pVCpu->nem.s.fEventPending)
    {
        /** @todo */
    }


    if (!VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED, VMCPUSTATE_STARTED_EXEC_NEM))
        VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED, VMCPUSTATE_STARTED_EXEC_NEM_CANCELED);

    if (pVCpu->cpum.GstCtx.fExtrn & (CPUMCTX_EXTRN_ALL))
    {
        /* Try anticipate what we might need. */
        uint64_t fImport = NEM_DARWIN_CPUMCTX_EXTRN_MASK_FOR_IEM;
        if (   (rcStrict >= VINF_EM_FIRST && rcStrict <= VINF_EM_LAST)
            || RT_FAILURE(rcStrict))
            fImport = CPUMCTX_EXTRN_ALL;
        else if (VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_INTERRUPT_IRQ | VMCPU_FF_INTERRUPT_FIQ
                                          | VMCPU_FF_INTERRUPT_NMI | VMCPU_FF_INTERRUPT_SMI))
            fImport |= IEM_CPUMCTX_EXTRN_XCPT_MASK;

        if (pVCpu->cpum.GstCtx.fExtrn & fImport)
        {
            /* Only import what is external currently. */
            int rc2 = nemR3DarwinCopyStateFromHv(pVM, pVCpu, fImport);
            if (RT_SUCCESS(rc2))
                pVCpu->cpum.GstCtx.fExtrn &= ~fImport;
            else if (RT_SUCCESS(rcStrict))
                rcStrict = rc2;
            if (!(pVCpu->cpum.GstCtx.fExtrn & CPUMCTX_EXTRN_ALL))
                pVCpu->cpum.GstCtx.fExtrn = 0;
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatImportOnReturn);
        }
        else
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatImportOnReturnSkipped);
    }
    else
    {
        STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatImportOnReturnSkipped);
        pVCpu->cpum.GstCtx.fExtrn = 0;
    }

    return rcStrict;
}


VMMR3_INT_DECL(bool) NEMR3CanExecuteGuest(PVM pVM, PVMCPU pVCpu)
{
    RT_NOREF(pVM, pVCpu);
    return true; /** @todo Are there any cases where we have to emulate? */
}


bool nemR3NativeSetSingleInstruction(PVM pVM, PVMCPU pVCpu, bool fEnable)
{
    VMCPU_ASSERT_EMT(pVCpu);
    bool fOld = pVCpu->nem.s.fSingleInstruction;
    pVCpu->nem.s.fSingleInstruction = fEnable;
    pVCpu->nem.s.fUseDebugLoop = fEnable || pVM->nem.s.fUseDebugLoop;
    return fOld;
}


void nemR3NativeNotifyFF(PVM pVM, PVMCPU pVCpu, uint32_t fFlags)
{
    LogFlowFunc(("pVM=%p pVCpu=%p fFlags=%#x\n", pVM, pVCpu, fFlags));

    RT_NOREF(pVM, fFlags);

    hv_return_t hrc = hv_vcpus_exit(&pVCpu->nem.s.hVCpu, 1);
    if (hrc != HV_SUCCESS)
        LogRel(("NEM: hv_vcpus_exit(%u, 1) failed with %#x\n", pVCpu->nem.s.hVCpu, hrc));
}


DECLHIDDEN(bool) nemR3NativeNotifyDebugEventChanged(PVM pVM, bool fUseDebugLoop)
{
    RT_NOREF(pVM, fUseDebugLoop);
    AssertReleaseFailed();
    return false;
}


DECLHIDDEN(bool) nemR3NativeNotifyDebugEventChangedPerCpu(PVM pVM, PVMCPU pVCpu, bool fUseDebugLoop)
{
    RT_NOREF(pVM, pVCpu, fUseDebugLoop);
    return fUseDebugLoop;
}


VMMR3_INT_DECL(int) NEMR3NotifyPhysRamRegister(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, void *pvR3,
                                               uint8_t *pu2State, uint32_t *puNemRange)
{
    RT_NOREF(pVM, puNemRange);

    Log5(("NEMR3NotifyPhysRamRegister: %RGp LB %RGp, pvR3=%p\n", GCPhys, cb, pvR3));
#if defined(VBOX_WITH_PGM_NEM_MODE)
    if (pvR3)
    {
        int rc = nemR3DarwinMap(pVM, GCPhys, pvR3, cb, NEM_PAGE_PROT_READ | NEM_PAGE_PROT_WRITE | NEM_PAGE_PROT_EXECUTE, pu2State);
        if (RT_FAILURE(rc))
        {
            LogRel(("NEMR3NotifyPhysRamRegister: GCPhys=%RGp LB %RGp pvR3=%p rc=%Rrc\n", GCPhys, cb, pvR3, rc));
            return VERR_NEM_MAP_PAGES_FAILED;
        }
    }
    return VINF_SUCCESS;
#else
    RT_NOREF(pVM, GCPhys, cb, pvR3);
    return VERR_NEM_MAP_PAGES_FAILED;
#endif
}


VMMR3_INT_DECL(bool) NEMR3IsMmio2DirtyPageTrackingSupported(PVM pVM)
{
    RT_NOREF(pVM);
    return false;
}


VMMR3_INT_DECL(int) NEMR3NotifyPhysMmioExMapEarly(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags,
                                                  void *pvRam, void *pvMmio2, uint8_t *pu2State, uint32_t *puNemRange)
{
    RT_NOREF(pVM, puNemRange, pvRam, fFlags);

    Log5(("NEMR3NotifyPhysMmioExMapEarly: %RGp LB %RGp fFlags=%#x pvRam=%p pvMmio2=%p pu2State=%p (%d)\n",
          GCPhys, cb, fFlags, pvRam, pvMmio2, pu2State, *pu2State));

#if defined(VBOX_WITH_PGM_NEM_MODE)
    /*
     * Unmap the RAM we're replacing.
     */
    if (fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_REPLACE)
    {
        int rc = nemR3DarwinUnmap(pVM, GCPhys, cb, pu2State);
        if (RT_SUCCESS(rc))
        { /* likely */ }
        else if (pvMmio2)
            LogRel(("NEMR3NotifyPhysMmioExMapEarly: GCPhys=%RGp LB %RGp fFlags=%#x: Unmap -> rc=%Rc(ignored)\n",
                    GCPhys, cb, fFlags, rc));
        else
        {
            LogRel(("NEMR3NotifyPhysMmioExMapEarly: GCPhys=%RGp LB %RGp fFlags=%#x: Unmap -> rc=%Rrc\n",
                    GCPhys, cb, fFlags, rc));
            return VERR_NEM_UNMAP_PAGES_FAILED;
        }
    }

    /*
     * Map MMIO2 if any.
     */
    if (pvMmio2)
    {
        Assert(fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_MMIO2);
        int rc = nemR3DarwinMap(pVM, GCPhys, pvMmio2, cb, NEM_PAGE_PROT_READ | NEM_PAGE_PROT_WRITE | NEM_PAGE_PROT_EXECUTE, pu2State);
        if (RT_FAILURE(rc))
        {
            LogRel(("NEMR3NotifyPhysMmioExMapEarly: GCPhys=%RGp LB %RGp fFlags=%#x pvMmio2=%p: Map -> rc=%Rrc\n",
                    GCPhys, cb, fFlags, pvMmio2, rc));
            return VERR_NEM_MAP_PAGES_FAILED;
        }
    }
    else
        Assert(!(fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_MMIO2));

#else
    RT_NOREF(pVM, GCPhys, cb, pvRam, pvMmio2);
    *pu2State = (fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_REPLACE) ? UINT8_MAX : NEM_DARWIN_PAGE_STATE_UNMAPPED;
#endif
    return VINF_SUCCESS;
}


VMMR3_INT_DECL(int) NEMR3NotifyPhysMmioExMapLate(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags,
                                                 void *pvRam, void *pvMmio2, uint32_t *puNemRange)
{
    RT_NOREF(pVM, GCPhys, cb, fFlags, pvRam, pvMmio2, puNemRange);
    return VINF_SUCCESS;
}


VMMR3_INT_DECL(int) NEMR3NotifyPhysMmioExUnmap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags, void *pvRam,
                                               void *pvMmio2, uint8_t *pu2State, uint32_t *puNemRange)
{
    RT_NOREF(pVM, puNemRange);

    Log5(("NEMR3NotifyPhysMmioExUnmap: %RGp LB %RGp fFlags=%#x pvRam=%p pvMmio2=%p pu2State=%p uNemRange=%#x (%#x)\n",
          GCPhys, cb, fFlags, pvRam, pvMmio2, pu2State, puNemRange, *puNemRange));

    int rc = VINF_SUCCESS;
#if defined(VBOX_WITH_PGM_NEM_MODE)
    /*
     * Unmap the MMIO2 pages.
     */
    /** @todo If we implement aliasing (MMIO2 page aliased into MMIO range),
     *        we may have more stuff to unmap even in case of pure MMIO... */
    if (fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_MMIO2)
    {
        rc = nemR3DarwinUnmap(pVM, GCPhys, cb, pu2State);
        if (RT_FAILURE(rc))
        {
            LogRel2(("NEMR3NotifyPhysMmioExUnmap: GCPhys=%RGp LB %RGp fFlags=%#x: Unmap -> rc=%Rrc\n",
                     GCPhys, cb, fFlags, rc));
            rc = VERR_NEM_UNMAP_PAGES_FAILED;
        }
    }

    /* Ensure the page is masked as unmapped if relevant. */
    Assert(!pu2State || *pu2State == NEM_DARWIN_PAGE_STATE_UNMAPPED);

    /*
     * Restore the RAM we replaced.
     */
    if (fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_REPLACE)
    {
        AssertPtr(pvRam);
        rc = nemR3DarwinMap(pVM, GCPhys, pvRam, cb, NEM_PAGE_PROT_READ | NEM_PAGE_PROT_WRITE | NEM_PAGE_PROT_EXECUTE, pu2State);
        if (RT_SUCCESS(rc))
        { /* likely */ }
        else
        {
            LogRel(("NEMR3NotifyPhysMmioExUnmap: GCPhys=%RGp LB %RGp pvMmio2=%p rc=%Rrc\n", GCPhys, cb, pvMmio2, rc));
            rc = VERR_NEM_MAP_PAGES_FAILED;
        }
    }

    RT_NOREF(pvMmio2);
#else
    RT_NOREF(pVM, GCPhys, cb, fFlags, pvRam, pvMmio2, pu2State);
    if (pu2State)
        *pu2State = UINT8_MAX;
    rc = VERR_NEM_UNMAP_PAGES_FAILED;
#endif
    return rc;
}


VMMR3_INT_DECL(int) NEMR3PhysMmio2QueryAndResetDirtyBitmap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t uNemRange,
                                                           void *pvBitmap, size_t cbBitmap)
{
    RT_NOREF(pVM, GCPhys, cb, uNemRange, pvBitmap, cbBitmap);
    AssertReleaseFailed();
    return VERR_NOT_IMPLEMENTED;
}


VMMR3_INT_DECL(int)  NEMR3NotifyPhysRomRegisterEarly(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, void *pvPages, uint32_t fFlags,
                                                     uint8_t *pu2State, uint32_t *puNemRange)
{
    RT_NOREF(pVM, GCPhys, cb, pvPages, fFlags, puNemRange);

    Log5(("nemR3NativeNotifyPhysRomRegisterEarly: %RGp LB %RGp pvPages=%p fFlags=%#x\n", GCPhys, cb, pvPages, fFlags));
    *pu2State   = UINT8_MAX;
    *puNemRange = 0;
    return VINF_SUCCESS;
}


VMMR3_INT_DECL(int)  NEMR3NotifyPhysRomRegisterLate(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, void *pvPages,
                                                    uint32_t fFlags, uint8_t *pu2State, uint32_t *puNemRange)
{
    Log5(("nemR3NativeNotifyPhysRomRegisterLate: %RGp LB %RGp pvPages=%p fFlags=%#x pu2State=%p (%d) puNemRange=%p (%#x)\n",
          GCPhys, cb, pvPages, fFlags, pu2State, *pu2State, puNemRange, *puNemRange));
    *pu2State = UINT8_MAX;

#if defined(VBOX_WITH_PGM_NEM_MODE)
    /*
     * (Re-)map readonly.
     */
    AssertPtrReturn(pvPages, VERR_INVALID_POINTER);
    int rc = nemR3DarwinMap(pVM, GCPhys, pvPages, cb, NEM_PAGE_PROT_READ | NEM_PAGE_PROT_EXECUTE, pu2State);
    if (RT_FAILURE(rc))
    {
        LogRel(("nemR3NativeNotifyPhysRomRegisterLate: GCPhys=%RGp LB %RGp pvPages=%p fFlags=%#x rc=%Rrc\n",
                GCPhys, cb, pvPages, fFlags, rc));
        return VERR_NEM_MAP_PAGES_FAILED;
    }
    RT_NOREF(fFlags, puNemRange);
    return VINF_SUCCESS;
#else
    RT_NOREF(pVM, GCPhys, cb, pvPages, fFlags, puNemRange);
    return VERR_NEM_MAP_PAGES_FAILED;
#endif
}


VMM_INT_DECL(void) NEMHCNotifyHandlerPhysicalDeregister(PVMCC pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhys, RTGCPHYS cb,
                                                        RTR3PTR pvMemR3, uint8_t *pu2State)
{
    RT_NOREF(pVM);

    Log5(("NEMHCNotifyHandlerPhysicalDeregister: %RGp LB %RGp enmKind=%d pvMemR3=%p pu2State=%p (%d)\n",
          GCPhys, cb, enmKind, pvMemR3, pu2State, *pu2State));

    *pu2State = UINT8_MAX;
#if defined(VBOX_WITH_PGM_NEM_MODE)
    if (pvMemR3)
    {
        int rc = nemR3DarwinMap(pVM, GCPhys, pvMemR3, cb, NEM_PAGE_PROT_READ | NEM_PAGE_PROT_WRITE | NEM_PAGE_PROT_EXECUTE, pu2State);
        AssertLogRelMsgRC(rc, ("NEMHCNotifyHandlerPhysicalDeregister: nemR3DarwinMap(,%p,%RGp,%RGp,) -> %Rrc\n",
                          pvMemR3, GCPhys, cb, rc));
    }
    RT_NOREF(enmKind);
#else
    RT_NOREF(pVM, enmKind, GCPhys, cb, pvMemR3);
    AssertFailed();
#endif
}


VMMR3_INT_DECL(void) NEMR3NotifySetA20(PVMCPU pVCpu, bool fEnabled)
{
    Log(("NEMR3NotifySetA20: fEnabled=%RTbool\n", fEnabled));
    RT_NOREF(pVCpu, fEnabled);
}


void nemHCNativeNotifyHandlerPhysicalRegister(PVMCC pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhys, RTGCPHYS cb)
{
    Log5(("nemHCNativeNotifyHandlerPhysicalRegister: %RGp LB %RGp enmKind=%d\n", GCPhys, cb, enmKind));
    NOREF(pVM); NOREF(enmKind); NOREF(GCPhys); NOREF(cb);
}


void nemHCNativeNotifyHandlerPhysicalModify(PVMCC pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhysOld,
                                            RTGCPHYS GCPhysNew, RTGCPHYS cb, bool fRestoreAsRAM)
{
    Log5(("nemHCNativeNotifyHandlerPhysicalModify: %RGp LB %RGp -> %RGp enmKind=%d fRestoreAsRAM=%d\n",
          GCPhysOld, cb, GCPhysNew, enmKind, fRestoreAsRAM));
    NOREF(pVM); NOREF(enmKind); NOREF(GCPhysOld); NOREF(GCPhysNew); NOREF(cb); NOREF(fRestoreAsRAM);
}


int nemHCNativeNotifyPhysPageAllocated(PVMCC pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, uint32_t fPageProt,
                                       PGMPAGETYPE enmType, uint8_t *pu2State)
{
    Log5(("nemHCNativeNotifyPhysPageAllocated: %RGp HCPhys=%RHp fPageProt=%#x enmType=%d *pu2State=%d\n",
          GCPhys, HCPhys, fPageProt, enmType, *pu2State));
    RT_NOREF(HCPhys, fPageProt, enmType);

    return nemR3DarwinUnmap(pVM, GCPhys, GUEST_PAGE_SIZE, pu2State);
}


VMM_INT_DECL(void) NEMHCNotifyPhysPageProtChanged(PVMCC pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, RTR3PTR pvR3, uint32_t fPageProt,
                                                  PGMPAGETYPE enmType, uint8_t *pu2State)
{
    Log5(("NEMHCNotifyPhysPageProtChanged: %RGp HCPhys=%RHp fPageProt=%#x enmType=%d *pu2State=%d\n",
          GCPhys, HCPhys, fPageProt, enmType, *pu2State));
    RT_NOREF(HCPhys, pvR3, fPageProt, enmType)

    nemR3DarwinUnmap(pVM, GCPhys, GUEST_PAGE_SIZE, pu2State);
}


VMM_INT_DECL(void) NEMHCNotifyPhysPageChanged(PVMCC pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhysPrev, RTHCPHYS HCPhysNew,
                                              RTR3PTR pvNewR3, uint32_t fPageProt, PGMPAGETYPE enmType, uint8_t *pu2State)
{
    Log5(("NEMHCNotifyPhysPageChanged: %RGp HCPhys=%RHp->%RHp fPageProt=%#x enmType=%d *pu2State=%d\n",
          GCPhys, HCPhysPrev, HCPhysNew, fPageProt, enmType, *pu2State));
    RT_NOREF(HCPhysPrev, HCPhysNew, pvNewR3, fPageProt, enmType);

    nemR3DarwinUnmap(pVM, GCPhys, GUEST_PAGE_SIZE, pu2State);
}


/**
 * Interface for importing state on demand (used by IEM).
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context CPU structure.
 * @param   fWhat       What to import, CPUMCTX_EXTRN_XXX.
 */
VMM_INT_DECL(int) NEMImportStateOnDemand(PVMCPUCC pVCpu, uint64_t fWhat)
{
    LogFlowFunc(("pVCpu=%p fWhat=%RX64\n", pVCpu, fWhat));
    STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatImportOnDemand);

    return nemR3DarwinCopyStateFromHv(pVCpu->pVMR3, pVCpu, fWhat);
}


/**
 * Query the CPU tick counter and optionally the TSC_AUX MSR value.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context CPU structure.
 * @param   pcTicks     Where to return the CPU tick count.
 * @param   puAux       Where to return the TSC_AUX register value.
 */
VMM_INT_DECL(int) NEMHCQueryCpuTick(PVMCPUCC pVCpu, uint64_t *pcTicks, uint32_t *puAux)
{
    LogFlowFunc(("pVCpu=%p pcTicks=%RX64 puAux=%RX32\n", pVCpu, pcTicks, puAux));
    STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatQueryCpuTick);

    AssertReleaseFailed();
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Resumes CPU clock (TSC) on all virtual CPUs.
 *
 * This is called by TM when the VM is started, restored, resumed or similar.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context CPU structure of the calling EMT.
 * @param   uPausedTscValue The TSC value at the time of pausing.
 */
VMM_INT_DECL(int) NEMHCResumeCpuTickOnAll(PVMCC pVM, PVMCPUCC pVCpu, uint64_t uPausedTscValue)
{
    LogFlowFunc(("pVM=%p pVCpu=%p uPausedTscValue=%RX64\n", pVCpu, uPausedTscValue));
    VMCPU_ASSERT_EMT_RETURN(pVCpu, VERR_VM_THREAD_NOT_EMT);
    AssertReturn(VM_IS_NEM_ENABLED(pVM), VERR_NEM_IPE_9);

    //AssertReleaseFailed();
    return VINF_SUCCESS;
}


/**
 * Returns features supported by the NEM backend.
 *
 * @returns Flags of features supported by the native NEM backend.
 * @param   pVM             The cross context VM structure.
 */
VMM_INT_DECL(uint32_t) NEMHCGetFeatures(PVMCC pVM)
{
    RT_NOREF(pVM);
    /*
     * Apple's Hypervisor.framework is not supported if the CPU doesn't support nested paging
     * and unrestricted guest execution support so we can safely return these flags here always.
     */
    return NEM_FEAT_F_NESTED_PAGING | NEM_FEAT_F_FULL_GST_EXEC | NEM_FEAT_F_XSAVE_XRSTOR;
}


/** @page pg_nem_darwin NEM/darwin - Native Execution Manager, macOS.
 *
 * @todo Add notes as the implementation progresses...
 */

