/* $Id$ */
/** @file
 * NEM - Native execution manager, native ring-3 Linux backend arm64 version.
 */

/*
 * Copyright (C) 2024 Oracle and/or its affiliates.
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
#include <VBox/vmm/nem.h>
#include <VBox/vmm/iem.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/apic.h>
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/trpm.h>
#include "NEMInternal.h"
#include <VBox/vmm/vmcc.h>

#include <iprt/alloca.h>
#include <iprt/string.h>
#include <iprt/system.h>
#include <iprt/armv8.h>

#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <linux/kvm.h>

/** @note This is an experiment right now and therefore is separate from the amd64 KVM NEM backend
 *        We'll see whether it would make sense to merge things later on when things have settled.
 */

/** Core register group. */
#define KVM_ARM64_REG_CORE_GROUP  UINT64_C(0x6030000000100000)
/** System register group. */
#define KVM_ARM64_REG_SYS_GROUP   UINT64_C(0x6030000000130000)
/** System register group. */
#define KVM_ARM64_REG_SIMD_GROUP  UINT64_C(0x6040000000100050)
/** FP register group. */
#define KVM_ARM64_REG_FP_GROUP    UINT64_C(0x6020000000100000)

#define KVM_ARM64_REG_CORE_CREATE(a_idReg)   (KVM_ARM64_REG_CORE_GROUP | ((uint64_t)(a_idReg) & 0xffff))
#define KVM_ARM64_REG_GPR(a_iGpr)            KVM_ARM64_REG_CORE_CREATE((a_iGpr) << 1)
#define KVM_ARM64_REG_SP_EL0                 KVM_ARM64_REG_CORE_CREATE(0x3e)
#define KVM_ARM64_REG_PC                     KVM_ARM64_REG_CORE_CREATE(0x40)
#define KVM_ARM64_REG_PSTATE                 KVM_ARM64_REG_CORE_CREATE(0x42)
#define KVM_ARM64_REG_SP_EL1                 KVM_ARM64_REG_CORE_CREATE(0x44)
#define KVM_ARM64_REG_ELR_EL1                KVM_ARM64_REG_CORE_CREATE(0x46)
#define KVM_ARM64_REG_SPSR_EL1               KVM_ARM64_REG_CORE_CREATE(0x48)
#define KVM_ARM64_REG_SPSR_ABT               KVM_ARM64_REG_CORE_CREATE(0x4a)
#define KVM_ARM64_REG_SPSR_UND               KVM_ARM64_REG_CORE_CREATE(0x4c)
#define KVM_ARM64_REG_SPSR_IRQ               KVM_ARM64_REG_CORE_CREATE(0x4e)
#define KVM_ARM64_REG_SPSR_FIQ               KVM_ARM64_REG_CORE_CREATE(0x50)

/** This maps to our IPRT representation of system register IDs, yay! */
#define KVM_ARM64_REG_SYS_CREATE(a_idSysReg) (KVM_ARM64_REG_SYS_GROUP  | ((uint64_t)(a_idSysReg) & 0xffff))

#define KVM_ARM64_REG_SIMD_CREATE(a_iVecReg) (KVM_ARM64_REG_SIMD_GROUP | (((uint64_t)(a_idReg) << 2) & 0xffff))

#define KVM_ARM64_REG_FP_CREATE(a_idReg)     (KVM_ARM64_REG_FP_GROUP   | ((uint64_t)(a_idReg) & 0xffff))
#define KVM_ARM64_REG_FP_FPSR                KVM_ARM64_REG_FP_CREATE(0xd4)
#define KVM_ARM64_REG_FP_FPCR                KVM_ARM64_REG_FP_CREATE(0xd5)


/** ID registers. */
static const struct
{
    uint64_t         idKvmReg;
    uint32_t         offIdStruct;
} s_aIdRegs[] =
{
    { KVM_ARM64_REG_SYS_CREATE(ARMV8_AARCH64_SYSREG_ID_AA64DFR0_EL1),       RT_UOFFSETOF(CPUMIDREGS, u64RegIdAa64Dfr0El1)  },
    { KVM_ARM64_REG_SYS_CREATE(ARMV8_AARCH64_SYSREG_ID_AA64DFR1_EL1),       RT_UOFFSETOF(CPUMIDREGS, u64RegIdAa64Dfr1El1)  },
    { KVM_ARM64_REG_SYS_CREATE(ARMV8_AARCH64_SYSREG_ID_AA64ISAR0_EL1),      RT_UOFFSETOF(CPUMIDREGS, u64RegIdAa64Isar0El1) },
    { KVM_ARM64_REG_SYS_CREATE(ARMV8_AARCH64_SYSREG_ID_AA64ISAR1_EL1),      RT_UOFFSETOF(CPUMIDREGS, u64RegIdAa64Isar1El1) },
    { KVM_ARM64_REG_SYS_CREATE(ARMV8_AARCH64_SYSREG_ID_AA64MMFR0_EL1),      RT_UOFFSETOF(CPUMIDREGS, u64RegIdAa64Mmfr0El1) },
    { KVM_ARM64_REG_SYS_CREATE(ARMV8_AARCH64_SYSREG_ID_AA64MMFR1_EL1),      RT_UOFFSETOF(CPUMIDREGS, u64RegIdAa64Mmfr1El1) },
    { KVM_ARM64_REG_SYS_CREATE(ARMV8_AARCH64_SYSREG_ID_AA64MMFR2_EL1),      RT_UOFFSETOF(CPUMIDREGS, u64RegIdAa64Mmfr2El1) },
    { KVM_ARM64_REG_SYS_CREATE(ARMV8_AARCH64_SYSREG_ID_AA64PFR0_EL1),       RT_UOFFSETOF(CPUMIDREGS, u64RegIdAa64Pfr0El1)  },
    { KVM_ARM64_REG_SYS_CREATE(ARMV8_AARCH64_SYSREG_ID_AA64PFR1_EL1),       RT_UOFFSETOF(CPUMIDREGS, u64RegIdAa64Pfr1El1)  }
};


/**
 * Worker for nemR3NativeInit that gets the hypervisor capabilities.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   pErrInfo            Where to always return error info.
 */
static int nemR3LnxInitCheckCapabilities(PVM pVM, PRTERRINFO pErrInfo)
{
    AssertReturn(pVM->nem.s.fdKvm != -1, RTErrInfoSet(pErrInfo, VERR_WRONG_ORDER, "Wrong initalization order"));

    /*
     * Capabilities.
     */
    static const struct
    {
        const char *pszName;
        int         iCap;
        uint32_t    offNem      : 24;
        uint32_t    cbNem       : 3;
        uint32_t    fReqNonZero : 1;
        uint32_t    uReserved   : 4;
    } s_aCaps[] =
    {
#define CAP_ENTRY__L(a_Define)           { #a_Define, a_Define,            UINT32_C(0x00ffffff), 0, 0, 0 }
#define CAP_ENTRY__S(a_Define, a_Member) { #a_Define, a_Define, RT_UOFFSETOF(NEM, a_Member), RT_SIZEOFMEMB(NEM, a_Member), 0, 0 }
#define CAP_ENTRY_MS(a_Define, a_Member) { #a_Define, a_Define, RT_UOFFSETOF(NEM, a_Member), RT_SIZEOFMEMB(NEM, a_Member), 1, 0 }
#define CAP_ENTRY__U(a_Number)           { "KVM_CAP_" #a_Number, a_Number, UINT32_C(0x00ffffff), 0, 0, 0 }
#define CAP_ENTRY_ML(a_Number)           { "KVM_CAP_" #a_Number, a_Number, UINT32_C(0x00ffffff), 0, 1, 0 }

        CAP_ENTRY__L(KVM_CAP_IRQCHIP),                       /* 0 */
        CAP_ENTRY__L(KVM_CAP_HLT),
        CAP_ENTRY__L(KVM_CAP_MMU_SHADOW_CACHE_CONTROL),
        CAP_ENTRY_ML(KVM_CAP_USER_MEMORY),
        CAP_ENTRY__L(KVM_CAP_SET_TSS_ADDR),
        CAP_ENTRY__U(5),
        CAP_ENTRY__L(KVM_CAP_VAPIC),
        CAP_ENTRY__L(KVM_CAP_EXT_CPUID),
        CAP_ENTRY__L(KVM_CAP_CLOCKSOURCE),
        CAP_ENTRY__L(KVM_CAP_NR_VCPUS),
        CAP_ENTRY_MS(KVM_CAP_NR_MEMSLOTS, cMaxMemSlots),     /* 10 */
        CAP_ENTRY__L(KVM_CAP_PIT),
        CAP_ENTRY__L(KVM_CAP_NOP_IO_DELAY),
        CAP_ENTRY__L(KVM_CAP_PV_MMU),
        CAP_ENTRY__L(KVM_CAP_MP_STATE),
        CAP_ENTRY__L(KVM_CAP_COALESCED_MMIO),
        CAP_ENTRY__L(KVM_CAP_SYNC_MMU),
        CAP_ENTRY__U(17),
        CAP_ENTRY__L(KVM_CAP_IOMMU),
        CAP_ENTRY__U(19), /* Buggy KVM_CAP_JOIN_MEMORY_REGIONS? */
        CAP_ENTRY__U(20), /* Mon-working KVM_CAP_DESTROY_MEMORY_REGION? */
        CAP_ENTRY__L(KVM_CAP_DESTROY_MEMORY_REGION_WORKS),   /* 21 */
        CAP_ENTRY__L(KVM_CAP_USER_NMI),
#ifdef __KVM_HAVE_GUEST_DEBUG
        CAP_ENTRY__L(KVM_CAP_SET_GUEST_DEBUG),
#endif
#ifdef __KVM_HAVE_PIT
        CAP_ENTRY__L(KVM_CAP_REINJECT_CONTROL),
#endif
        CAP_ENTRY__L(KVM_CAP_IRQ_ROUTING),
        CAP_ENTRY__L(KVM_CAP_IRQ_INJECT_STATUS),
        CAP_ENTRY__U(27),
        CAP_ENTRY__U(28),
        CAP_ENTRY__L(KVM_CAP_ASSIGN_DEV_IRQ),
        CAP_ENTRY__L(KVM_CAP_JOIN_MEMORY_REGIONS_WORKS),     /* 30 */
#ifdef __KVM_HAVE_MCE
        CAP_ENTRY__L(KVM_CAP_MCE),
#endif
        CAP_ENTRY__L(KVM_CAP_IRQFD),
#ifdef __KVM_HAVE_PIT
        CAP_ENTRY__L(KVM_CAP_PIT2),
#endif
        CAP_ENTRY__L(KVM_CAP_SET_BOOT_CPU_ID),
#ifdef __KVM_HAVE_PIT_STATE2
        CAP_ENTRY__L(KVM_CAP_PIT_STATE2),
#endif
        CAP_ENTRY__L(KVM_CAP_IOEVENTFD),
        CAP_ENTRY__L(KVM_CAP_SET_IDENTITY_MAP_ADDR),
#ifdef __KVM_HAVE_XEN_HVM
        CAP_ENTRY__L(KVM_CAP_XEN_HVM),
#endif
        CAP_ENTRY__L(KVM_CAP_ADJUST_CLOCK),
        CAP_ENTRY__L(KVM_CAP_INTERNAL_ERROR_DATA),           /* 40 */
#ifdef __KVM_HAVE_VCPU_EVENTS
        CAP_ENTRY_ML(KVM_CAP_VCPU_EVENTS),
#else
        CAP_ENTRY_MU(41),
#endif
        CAP_ENTRY__L(KVM_CAP_S390_PSW),
        CAP_ENTRY__L(KVM_CAP_PPC_SEGSTATE),
        CAP_ENTRY__L(KVM_CAP_HYPERV),
        CAP_ENTRY__L(KVM_CAP_HYPERV_VAPIC),
        CAP_ENTRY__L(KVM_CAP_HYPERV_SPIN),
        CAP_ENTRY__L(KVM_CAP_PCI_SEGMENT),
        CAP_ENTRY__L(KVM_CAP_PPC_PAIRED_SINGLES),
        CAP_ENTRY__L(KVM_CAP_INTR_SHADOW),
#ifdef __KVM_HAVE_DEBUGREGS
        CAP_ENTRY__L(KVM_CAP_DEBUGREGS),                     /* 50 */
#endif
        CAP_ENTRY__S(KVM_CAP_X86_ROBUST_SINGLESTEP, fRobustSingleStep),
        CAP_ENTRY__L(KVM_CAP_PPC_OSI),
        CAP_ENTRY__L(KVM_CAP_PPC_UNSET_IRQ),
        CAP_ENTRY__L(KVM_CAP_ENABLE_CAP),
        CAP_ENTRY__L(KVM_CAP_PPC_GET_PVINFO),
        CAP_ENTRY__L(KVM_CAP_PPC_IRQ_LEVEL),
        CAP_ENTRY__L(KVM_CAP_ASYNC_PF),
        CAP_ENTRY__L(KVM_CAP_TSC_CONTROL),                   /* 60 */
        CAP_ENTRY__L(KVM_CAP_GET_TSC_KHZ),
        CAP_ENTRY__L(KVM_CAP_PPC_BOOKE_SREGS),
        CAP_ENTRY__L(KVM_CAP_SPAPR_TCE),
        CAP_ENTRY__L(KVM_CAP_PPC_SMT),
        CAP_ENTRY__L(KVM_CAP_PPC_RMA),
        CAP_ENTRY__L(KVM_CAP_MAX_VCPUS),
        CAP_ENTRY__L(KVM_CAP_PPC_HIOR),
        CAP_ENTRY__L(KVM_CAP_PPC_PAPR),
        CAP_ENTRY__L(KVM_CAP_SW_TLB),
        CAP_ENTRY__L(KVM_CAP_ONE_REG),                       /* 70 */
        CAP_ENTRY__L(KVM_CAP_S390_GMAP),
        CAP_ENTRY__L(KVM_CAP_TSC_DEADLINE_TIMER),
        CAP_ENTRY__L(KVM_CAP_S390_UCONTROL),
        CAP_ENTRY__L(KVM_CAP_SYNC_REGS),
        CAP_ENTRY__L(KVM_CAP_PCI_2_3),
        CAP_ENTRY__L(KVM_CAP_KVMCLOCK_CTRL),
        CAP_ENTRY__L(KVM_CAP_SIGNAL_MSI),
        CAP_ENTRY__L(KVM_CAP_PPC_GET_SMMU_INFO),
        CAP_ENTRY__L(KVM_CAP_S390_COW),
        CAP_ENTRY__L(KVM_CAP_PPC_ALLOC_HTAB),                /* 80 */
        CAP_ENTRY__L(KVM_CAP_READONLY_MEM),
        CAP_ENTRY__L(KVM_CAP_IRQFD_RESAMPLE),
        CAP_ENTRY__L(KVM_CAP_PPC_BOOKE_WATCHDOG),
        CAP_ENTRY__L(KVM_CAP_PPC_HTAB_FD),
        CAP_ENTRY__L(KVM_CAP_S390_CSS_SUPPORT),
        CAP_ENTRY__L(KVM_CAP_PPC_EPR),
        CAP_ENTRY__L(KVM_CAP_ARM_PSCI),
        CAP_ENTRY__L(KVM_CAP_ARM_SET_DEVICE_ADDR),
        CAP_ENTRY__L(KVM_CAP_DEVICE_CTRL),
        CAP_ENTRY__L(KVM_CAP_IRQ_MPIC),                      /* 90 */
        CAP_ENTRY__L(KVM_CAP_PPC_RTAS),
        CAP_ENTRY__L(KVM_CAP_IRQ_XICS),
        CAP_ENTRY__L(KVM_CAP_ARM_EL1_32BIT),
        CAP_ENTRY__L(KVM_CAP_SPAPR_MULTITCE),
        CAP_ENTRY__L(KVM_CAP_EXT_EMUL_CPUID),
        CAP_ENTRY__L(KVM_CAP_HYPERV_TIME),
        CAP_ENTRY__L(KVM_CAP_IOAPIC_POLARITY_IGNORED),
        CAP_ENTRY__L(KVM_CAP_ENABLE_CAP_VM),
        CAP_ENTRY__L(KVM_CAP_S390_IRQCHIP),
        CAP_ENTRY__L(KVM_CAP_IOEVENTFD_NO_LENGTH),           /* 100 */
        CAP_ENTRY__L(KVM_CAP_VM_ATTRIBUTES),
        CAP_ENTRY__L(KVM_CAP_ARM_PSCI_0_2),
        CAP_ENTRY__L(KVM_CAP_PPC_FIXUP_HCALL),
        CAP_ENTRY__L(KVM_CAP_PPC_ENABLE_HCALL),
        CAP_ENTRY__L(KVM_CAP_CHECK_EXTENSION_VM),
        CAP_ENTRY__L(KVM_CAP_S390_USER_SIGP),
        CAP_ENTRY__L(KVM_CAP_S390_VECTOR_REGISTERS),
        CAP_ENTRY__L(KVM_CAP_S390_MEM_OP),
        CAP_ENTRY__L(KVM_CAP_S390_USER_STSI),
        CAP_ENTRY__L(KVM_CAP_S390_SKEYS),                    /* 110 */
        CAP_ENTRY__L(KVM_CAP_MIPS_FPU),
        CAP_ENTRY__L(KVM_CAP_MIPS_MSA),
        CAP_ENTRY__L(KVM_CAP_S390_INJECT_IRQ),
        CAP_ENTRY__L(KVM_CAP_S390_IRQ_STATE),
        CAP_ENTRY__L(KVM_CAP_PPC_HWRNG),
        CAP_ENTRY__L(KVM_CAP_DISABLE_QUIRKS),
        CAP_ENTRY__L(KVM_CAP_X86_SMM),
        CAP_ENTRY__L(KVM_CAP_MULTI_ADDRESS_SPACE),
        CAP_ENTRY__L(KVM_CAP_GUEST_DEBUG_HW_BPS),
        CAP_ENTRY__L(KVM_CAP_GUEST_DEBUG_HW_WPS),            /* 120 */
        CAP_ENTRY__L(KVM_CAP_SPLIT_IRQCHIP),
        CAP_ENTRY__L(KVM_CAP_IOEVENTFD_ANY_LENGTH),
        CAP_ENTRY__L(KVM_CAP_HYPERV_SYNIC),
        CAP_ENTRY__L(KVM_CAP_S390_RI),
        CAP_ENTRY__L(KVM_CAP_SPAPR_TCE_64),
        CAP_ENTRY__L(KVM_CAP_ARM_PMU_V3),
        CAP_ENTRY__L(KVM_CAP_VCPU_ATTRIBUTES),
        CAP_ENTRY__L(KVM_CAP_MAX_VCPU_ID),
        CAP_ENTRY__L(KVM_CAP_X2APIC_API),
        CAP_ENTRY__L(KVM_CAP_S390_USER_INSTR0),              /* 130 */
        CAP_ENTRY__L(KVM_CAP_MSI_DEVID),
        CAP_ENTRY__L(KVM_CAP_PPC_HTM),
        CAP_ENTRY__L(KVM_CAP_SPAPR_RESIZE_HPT),
        CAP_ENTRY__L(KVM_CAP_PPC_MMU_RADIX),
        CAP_ENTRY__L(KVM_CAP_PPC_MMU_HASH_V3),
        CAP_ENTRY__L(KVM_CAP_IMMEDIATE_EXIT),
        CAP_ENTRY__L(KVM_CAP_MIPS_VZ),
        CAP_ENTRY__L(KVM_CAP_MIPS_TE),
        CAP_ENTRY__L(KVM_CAP_MIPS_64BIT),
        CAP_ENTRY__L(KVM_CAP_S390_GS),                       /* 140 */
        CAP_ENTRY__L(KVM_CAP_S390_AIS),
        CAP_ENTRY__L(KVM_CAP_SPAPR_TCE_VFIO),
        CAP_ENTRY__L(KVM_CAP_X86_DISABLE_EXITS),
        CAP_ENTRY__L(KVM_CAP_ARM_USER_IRQ),
        CAP_ENTRY__L(KVM_CAP_S390_CMMA_MIGRATION),
        CAP_ENTRY__L(KVM_CAP_PPC_FWNMI),
        CAP_ENTRY__L(KVM_CAP_PPC_SMT_POSSIBLE),
        CAP_ENTRY__L(KVM_CAP_HYPERV_SYNIC2),
        CAP_ENTRY__L(KVM_CAP_HYPERV_VP_INDEX),
        CAP_ENTRY__L(KVM_CAP_S390_AIS_MIGRATION),            /* 150 */
        CAP_ENTRY__L(KVM_CAP_PPC_GET_CPU_CHAR),
        CAP_ENTRY__L(KVM_CAP_S390_BPB),
        CAP_ENTRY__L(KVM_CAP_GET_MSR_FEATURES),
        CAP_ENTRY__L(KVM_CAP_HYPERV_EVENTFD),
        CAP_ENTRY__L(KVM_CAP_HYPERV_TLBFLUSH),
        CAP_ENTRY__L(KVM_CAP_S390_HPAGE_1M),
        CAP_ENTRY__L(KVM_CAP_NESTED_STATE),
        CAP_ENTRY__L(KVM_CAP_ARM_INJECT_SERROR_ESR),
        CAP_ENTRY__L(KVM_CAP_MSR_PLATFORM_INFO),
        CAP_ENTRY__L(KVM_CAP_PPC_NESTED_HV),                 /* 160 */
        CAP_ENTRY__L(KVM_CAP_HYPERV_SEND_IPI),
        CAP_ENTRY__L(KVM_CAP_COALESCED_PIO),
        CAP_ENTRY__L(KVM_CAP_HYPERV_ENLIGHTENED_VMCS),
        CAP_ENTRY__L(KVM_CAP_EXCEPTION_PAYLOAD),
        CAP_ENTRY_MS(KVM_CAP_ARM_VM_IPA_SIZE, cIpaBits),
        CAP_ENTRY__L(KVM_CAP_MANUAL_DIRTY_LOG_PROTECT),
        CAP_ENTRY__L(KVM_CAP_HYPERV_CPUID),
        CAP_ENTRY__L(KVM_CAP_MANUAL_DIRTY_LOG_PROTECT2),
        CAP_ENTRY__L(KVM_CAP_PPC_IRQ_XIVE),
        CAP_ENTRY__L(KVM_CAP_ARM_SVE),                       /* 170 */
        CAP_ENTRY__L(KVM_CAP_ARM_PTRAUTH_ADDRESS),
        CAP_ENTRY__L(KVM_CAP_ARM_PTRAUTH_GENERIC),
        CAP_ENTRY__L(KVM_CAP_PMU_EVENT_FILTER),
        CAP_ENTRY__L(KVM_CAP_ARM_IRQ_LINE_LAYOUT_2),
        CAP_ENTRY__L(KVM_CAP_HYPERV_DIRECT_TLBFLUSH),
        CAP_ENTRY__L(KVM_CAP_PPC_GUEST_DEBUG_SSTEP),
        CAP_ENTRY__L(KVM_CAP_ARM_NISV_TO_USER),
        CAP_ENTRY__L(KVM_CAP_ARM_INJECT_EXT_DABT),
        CAP_ENTRY__L(KVM_CAP_S390_VCPU_RESETS),
        CAP_ENTRY__L(KVM_CAP_S390_PROTECTED),                /* 180 */
        CAP_ENTRY__L(KVM_CAP_PPC_SECURE_GUEST),
        CAP_ENTRY__L(KVM_CAP_HALT_POLL),
        CAP_ENTRY__L(KVM_CAP_ASYNC_PF_INT),
        CAP_ENTRY__L(KVM_CAP_LAST_CPU),
        CAP_ENTRY__L(KVM_CAP_SMALLER_MAXPHYADDR),
        CAP_ENTRY__L(KVM_CAP_S390_DIAG318),
        CAP_ENTRY__L(KVM_CAP_STEAL_TIME),
        CAP_ENTRY__L(KVM_CAP_X86_USER_SPACE_MSR),            /* (since 5.10) */
        CAP_ENTRY__L(KVM_CAP_X86_MSR_FILTER),
        CAP_ENTRY__L(KVM_CAP_ENFORCE_PV_FEATURE_CPUID),      /* 190 */
        CAP_ENTRY__L(KVM_CAP_SYS_HYPERV_CPUID),
        CAP_ENTRY__L(KVM_CAP_DIRTY_LOG_RING),
        CAP_ENTRY__L(KVM_CAP_X86_BUS_LOCK_EXIT),
        CAP_ENTRY__L(KVM_CAP_PPC_DAWR1),
        CAP_ENTRY__L(KVM_CAP_SET_GUEST_DEBUG2),
        CAP_ENTRY__L(KVM_CAP_SGX_ATTRIBUTE),
        CAP_ENTRY__L(KVM_CAP_VM_COPY_ENC_CONTEXT_FROM),
        CAP_ENTRY__L(KVM_CAP_PTP_KVM),
        CAP_ENTRY__U(199),
        CAP_ENTRY__U(200),
        CAP_ENTRY__U(201),
        CAP_ENTRY__U(202),
        CAP_ENTRY__U(203),
        CAP_ENTRY__U(204),
        CAP_ENTRY__U(205),
        CAP_ENTRY__U(206),
        CAP_ENTRY__U(207),
        CAP_ENTRY__U(208),
        CAP_ENTRY__U(209),
        CAP_ENTRY__U(210),
        CAP_ENTRY__U(211),
        CAP_ENTRY__U(212),
        CAP_ENTRY__U(213),
        CAP_ENTRY__U(214),
        CAP_ENTRY__U(215),
        CAP_ENTRY__U(216),
    };

    LogRel(("NEM: KVM capabilities (system):\n"));
    int rcRet = VINF_SUCCESS;
    for (unsigned i = 0; i < RT_ELEMENTS(s_aCaps); i++)
    {
        int rc = ioctl(pVM->nem.s.fdKvm, KVM_CHECK_EXTENSION, s_aCaps[i].iCap);
        if (rc >= 10)
            LogRel(("NEM:   %36s: %#x (%d)\n", s_aCaps[i].pszName, rc, rc));
        else if (rc >= 0)
            LogRel(("NEM:   %36s: %d\n", s_aCaps[i].pszName, rc));
        else
            LogRel(("NEM:   %s failed: %d/%d\n", s_aCaps[i].pszName, rc, errno));
        switch (s_aCaps[i].cbNem)
        {
            case 0:
                break;
            case 1:
            {
                uint8_t *puValue = (uint8_t *)&pVM->nem.padding[s_aCaps[i].offNem];
                AssertReturn(s_aCaps[i].offNem <= sizeof(NEM) - sizeof(*puValue), VERR_NEM_IPE_0);
                *puValue = (uint8_t)rc;
                AssertLogRelMsg((int)*puValue == rc, ("%s: %#x\n", s_aCaps[i].pszName, rc));
                break;
            }
            case 2:
            {
                uint16_t *puValue = (uint16_t *)&pVM->nem.padding[s_aCaps[i].offNem];
                AssertReturn(s_aCaps[i].offNem <= sizeof(NEM) - sizeof(*puValue), VERR_NEM_IPE_0);
                *puValue = (uint16_t)rc;
                AssertLogRelMsg((int)*puValue == rc, ("%s: %#x\n", s_aCaps[i].pszName, rc));
                break;
            }
            case 4:
            {
                uint32_t *puValue = (uint32_t *)&pVM->nem.padding[s_aCaps[i].offNem];
                AssertReturn(s_aCaps[i].offNem <= sizeof(NEM) - sizeof(*puValue), VERR_NEM_IPE_0);
                *puValue = (uint32_t)rc;
                AssertLogRelMsg((int)*puValue == rc, ("%s: %#x\n", s_aCaps[i].pszName, rc));
                break;
            }
            default:
                rcRet = RTErrInfoSetF(pErrInfo, VERR_NEM_IPE_0, "s_aCaps[%u] is bad: cbNem=%#x - %s",
                                      i, s_aCaps[i].pszName, s_aCaps[i].cbNem);
                AssertFailedReturn(rcRet);
        }

        /*
         * Is a require non-zero entry zero or failing?
         */
        if (s_aCaps[i].fReqNonZero && rc <= 0)
            rcRet = RTERRINFO_LOG_REL_ADD_F(pErrInfo, VERR_NEM_MISSING_FEATURE,
                                            "Required capability '%s' is missing!", s_aCaps[i].pszName);
    }

    /*
     * Get per VCpu KVM_RUN MMAP area size.
     */
    int rc = ioctl(pVM->nem.s.fdKvm, KVM_GET_VCPU_MMAP_SIZE, 0UL);
    if ((unsigned)rc < _64M)
    {
        pVM->nem.s.cbVCpuMmap = (uint32_t)rc;
        LogRel(("NEM:   %36s: %#x (%d)\n", "KVM_GET_VCPU_MMAP_SIZE", rc, rc));
    }
    else if (rc < 0)
        rcRet = RTERRINFO_LOG_REL_ADD_F(pErrInfo, VERR_NEM_MISSING_FEATURE, "KVM_GET_VCPU_MMAP_SIZE failed: %d", errno);
    else
        rcRet = RTERRINFO_LOG_REL_ADD_F(pErrInfo, VERR_NEM_INIT_FAILED, "Odd KVM_GET_VCPU_MMAP_SIZE value: %#x (%d)", rc, rc);

    /*
     * Init the slot ID bitmap.
     */
    ASMBitSet(&pVM->nem.s.bmSlotIds[0], 0);         /* don't use slot 0 */
    if (pVM->nem.s.cMaxMemSlots < _32K)
        ASMBitSetRange(&pVM->nem.s.bmSlotIds[0], pVM->nem.s.cMaxMemSlots, _32K);
    ASMBitSet(&pVM->nem.s.bmSlotIds[0], _32K - 1);  /* don't use the last slot */

    return rcRet;
}


/**
 * Queries and logs the supported register list from KVM.
 *
 * @returns VBox status code.
 * @param   fdVCpu              The file descriptor number of vCPU 0.
 */
static int nemR3LnxLogRegList(int fdVCpu)
{
    struct KVM_REG_LIST
    {
        uint64_t cRegs;
        uint64_t aRegs[1024];
    } RegList; RT_ZERO(RegList);

    RegList.cRegs = RT_ELEMENTS(RegList.aRegs);
    int rcLnx = ioctl(fdVCpu, KVM_GET_REG_LIST, &RegList);
    if (rcLnx != 0)
        return RTErrConvertFromErrno(errno);

    LogRel(("NEM: KVM vCPU registers:\n"));

    for (uint32_t i = 0; i < RegList.cRegs; i++)
        LogRel(("NEM:   %36s: %#RX64\n", "Unknown" /** @todo */, RegList.aRegs[i]));

    return VINF_SUCCESS;
}


DECL_FORCE_INLINE(int) nemR3LnxKvmSetQueryReg(PVMCPUCC pVCpu, bool fQuery, uint64_t idKvmReg, uint64_t *pu64)
{
    struct kvm_one_reg Reg;
    Reg.id   = idKvmReg;
    Reg.addr = (uintptr_t)pu64;

    /*
     * Who thought that this API was a good idea? Supporting to query/set just one register
     * at a time is horribly inefficient.
     */
    int rcLnx = ioctl(pVCpu->nem.s.fdVCpu, fQuery ? KVM_GET_ONE_REG : KVM_SET_ONE_REG, &Reg);
    if (!rcLnx)
        return 0;

    return RTErrConvertFromErrno(-rcLnx);
}

DECL_INLINE(int) nemR3LnxKvmQueryReg(PVMCPUCC pVCpu, uint64_t idKvmReg, uint64_t *pu64)
{
    return nemR3LnxKvmSetQueryReg(pVCpu, true /*fQuery*/, idKvmReg, pu64);
}


DECL_FORCE_INLINE(int) nemR3LnxKvmSetReg(PVMCPUCC pVCpu, uint64_t idKvmReg, const uint64_t *pu64)
{
    return nemR3LnxKvmSetQueryReg(pVCpu, false /*fQuery*/, idKvmReg, pu64);
}


/**
 * Does the early setup of a KVM VM.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   pErrInfo            Where to always return error info.
 */
static int nemR3LnxInitSetupVm(PVM pVM, PRTERRINFO pErrInfo)
{
    AssertReturn(pVM->nem.s.fdVm != -1, RTErrInfoSet(pErrInfo, VERR_WRONG_ORDER, "Wrong initalization order"));

    /*
     * Create the VCpus.
     */
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU pVCpu = pVM->apCpusR3[idCpu];

        /* Create it. */
        pVCpu->nem.s.fdVCpu = ioctl(pVM->nem.s.fdVm, KVM_CREATE_VCPU, (unsigned long)idCpu);
        if (pVCpu->nem.s.fdVCpu < 0)
            return RTErrInfoSetF(pErrInfo, VERR_NEM_VM_CREATE_FAILED, "KVM_CREATE_VCPU failed for VCpu #%u: %d", idCpu, errno);

        /* Map the KVM_RUN area. */
        pVCpu->nem.s.pRun = (struct kvm_run *)mmap(NULL, pVM->nem.s.cbVCpuMmap, PROT_READ | PROT_WRITE, MAP_SHARED,
                                                   pVCpu->nem.s.fdVCpu, 0 /*offset*/);
        if ((void *)pVCpu->nem.s.pRun == MAP_FAILED)
            return RTErrInfoSetF(pErrInfo, VERR_NEM_VM_CREATE_FAILED, "mmap failed for VCpu #%u: %d", idCpu, errno);

        /* Initialize the vCPU. */
        struct kvm_vcpu_init VCpuInit; RT_ZERO(VCpuInit);
        VCpuInit.target = KVM_ARM_TARGET_GENERIC_V8;
        /** @todo Enable features. */
        if (ioctl(pVCpu->nem.s.fdVCpu, KVM_ARM_VCPU_INIT, &VCpuInit) != 0)
            return RTErrInfoSetF(pErrInfo, VERR_NEM_VM_CREATE_FAILED, "KVM_ARM_VCPU_INIT failed for VCpu #%u: %d", idCpu, errno);

#if 0
        uint32_t fFeatures = 0; /** @todo SVE */
        if (ioctl(pVCpu->nem.s.fdVCpu, KVM_ARM_VCPU_FINALIZE, &fFeatures) != 0)
            return RTErrInfoSetF(pErrInfo, VERR_NEM_VM_CREATE_FAILED, "KVM_ARM_VCPU_FINALIZE failed for VCpu #%u: %d", idCpu, errno);
#endif

        if (idCpu == 0)
        {
            /* Query the supported register list and log it. */
            int rc = nemR3LnxLogRegList(pVCpu->nem.s.fdVCpu);
            if (RT_FAILURE(rc))
                return RTErrInfoSetF(pErrInfo, VERR_NEM_VM_CREATE_FAILED, "Querying the supported register list failed with %Rrc", rc);

            /* Need to query the ID registers and populate CPUM. */
            CPUMIDREGS IdRegs; RT_ZERO(IdRegs);
            for (uint32_t i = 0; i < RT_ELEMENTS(s_aIdRegs); i++)
            {
                uint64_t *pu64 = (uint64_t *)((uint8_t *)&IdRegs + s_aIdRegs[i].offIdStruct);
                rc = nemR3LnxKvmQueryReg(pVCpu, s_aIdRegs[i].idKvmReg, pu64);
                if (RT_FAILURE(rc))
                    return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS,
                                      "Querying register %#x failed: %Rrc", s_aIdRegs[i].idKvmReg, rc);
            }

            rc = CPUMR3PopulateFeaturesByIdRegisters(pVM, &IdRegs);
            if (RT_FAILURE(rc))
                return rc;
        }
    }
    return VINF_SUCCESS;
}


/** @callback_method_impl{FNVMMEMTRENDEZVOUS}   */
static DECLCALLBACK(VBOXSTRICTRC) nemR3LnxFixThreadPoke(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    RT_NOREF(pVM, pvUser);
    int rc = RTThreadControlPokeSignal(pVCpu->hThread, true /*fEnable*/);
    AssertLogRelRC(rc);
    return VINF_SUCCESS;
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
    RT_NOREF(pVM, fFallback, fForced);
    /*
     * Some state init.
     */
    pVM->nem.s.fdKvm = -1;
    pVM->nem.s.fdVm  = -1;
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PNEMCPU pNemCpu = &pVM->apCpusR3[idCpu]->nem.s;
        pNemCpu->fdVCpu = -1;
    }

    /*
     * Error state.
     * The error message will be non-empty on failure and 'rc' will be set too.
     */
    RTERRINFOSTATIC ErrInfo;
    PRTERRINFO pErrInfo = RTErrInfoInitStatic(&ErrInfo);

    /*
     * Open kvm subsystem so we can issue system ioctls.
     */
    int rc;
    int fdKvm = open("/dev/kvm", O_RDWR | O_CLOEXEC);
    if (fdKvm >= 0)
    {
        pVM->nem.s.fdKvm = fdKvm;

        /*
         * Check capabilities.
         */
        rc = nemR3LnxInitCheckCapabilities(pVM, pErrInfo);
        if (RT_SUCCESS(rc))
        {
            /*
             * Create an empty VM since it is recommended we check capabilities on
             * the VM rather than the system descriptor.
             */
            int fdVm = ioctl(fdKvm, KVM_CREATE_VM, pVM->nem.s.cIpaBits);
            if (fdVm >= 0)
            {
                pVM->nem.s.fdVm = fdVm;

                /*
                 * Set up the VM (more on this later).
                 */
                rc = nemR3LnxInitSetupVm(pVM, pErrInfo);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Set ourselves as the execution engine and make config adjustments.
                     */
                    VM_SET_MAIN_EXECUTION_ENGINE(pVM, VM_EXEC_ENGINE_NATIVE_API);
                    Log(("NEM: Marked active!\n"));
                    PGMR3EnableNemMode(pVM);

                    /*
                     * Register release statistics
                     */
                    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
                    {
                        PNEMCPU pNemCpu = &pVM->apCpusR3[idCpu]->nem.s;
                        STAMR3RegisterF(pVM, &pNemCpu->StatImportOnDemand,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of on-demand state imports",      "/NEM/CPU%u/ImportOnDemand", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatImportOnReturn,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of state imports on loop return", "/NEM/CPU%u/ImportOnReturn", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatImportOnReturnSkipped, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of skipped state imports on loop return", "/NEM/CPU%u/ImportOnReturnSkipped", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatImportPendingInterrupt, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of times an interrupt was pending when importing from KVM", "/NEM/CPU%u/ImportPendingInterrupt", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExportPendingInterrupt, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of times an interrupt was pending when exporting to KVM", "/NEM/CPU%u/ExportPendingInterrupt", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatFlushExitOnReturn,   STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of times a KVM_EXIT_IO or KVM_EXIT_MMIO was flushed before returning to EM", "/NEM/CPU%u/FlushExitOnReturn", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatFlushExitOnReturn1Loop, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of times a KVM_EXIT_IO or KVM_EXIT_MMIO was flushed before returning to EM", "/NEM/CPU%u/FlushExitOnReturn-01-loop", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatFlushExitOnReturn2Loops, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of times a KVM_EXIT_IO or KVM_EXIT_MMIO was flushed before returning to EM", "/NEM/CPU%u/FlushExitOnReturn-02-loops", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatFlushExitOnReturn3Loops, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of times a KVM_EXIT_IO or KVM_EXIT_MMIO was flushed before returning to EM", "/NEM/CPU%u/FlushExitOnReturn-03-loops", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatFlushExitOnReturn4PlusLoops, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of times a KVM_EXIT_IO or KVM_EXIT_MMIO was flushed before returning to EM", "/NEM/CPU%u/FlushExitOnReturn-04-to-7-loops", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatQueryCpuTick,        STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of TSC queries",                  "/NEM/CPU%u/QueryCpuTick", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitTotal,           STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "All exits",                  "/NEM/CPU%u/Exit", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitIo,              STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "KVM_EXIT_IO",                "/NEM/CPU%u/Exit/Io", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitMmio,            STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "KVM_EXIT_MMIO",              "/NEM/CPU%u/Exit/Mmio", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitIntr,            STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "KVM_EXIT_INTR",              "/NEM/CPU%u/Exit/Intr", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitHypercall,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "KVM_EXIT_HYPERCALL",         "/NEM/CPU%u/Exit/Hypercall", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitDebug,           STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "KVM_EXIT_DEBUG",             "/NEM/CPU%u/Exit/Debug", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitBusLock,         STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "KVM_EXIT_BUS_LOCK",          "/NEM/CPU%u/Exit/BusLock", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitInternalErrorEmulation, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "KVM_EXIT_INTERNAL_ERROR/EMULATION", "/NEM/CPU%u/Exit/InternalErrorEmulation", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitInternalErrorFatal,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "KVM_EXIT_INTERNAL_ERROR/*", "/NEM/CPU%u/Exit/InternalErrorFatal", idCpu);
                    }

                    /*
                     * Success.
                     */
                    return VINF_SUCCESS;
                }
                close(fdVm);
                pVM->nem.s.fdVm = -1;
            }
            else
                rc = RTErrInfoSetF(pErrInfo, VERR_NEM_VM_CREATE_FAILED, "KVM_CREATE_VM failed: %u", errno);
        }
        close(fdKvm);
        pVM->nem.s.fdKvm = -1;
    }
    else if (errno == EACCES)
        rc = RTErrInfoSet(pErrInfo, VERR_ACCESS_DENIED, "Do not have access to open /dev/kvm for reading & writing.");
    else if (errno == ENOENT)
        rc = RTErrInfoSet(pErrInfo, VERR_NOT_SUPPORTED, "KVM is not availble (/dev/kvm does not exist)");
    else
        rc = RTErrInfoSetF(pErrInfo, RTErrConvertFromErrno(errno), "Failed to open '/dev/kvm': %u", errno);

    /*
     * We only fail if in forced mode, otherwise just log the complaint and return.
     */
    Assert(RTErrInfoIsSet(pErrInfo));
    if (   (fForced || !fFallback)
        && pVM->bMainExecutionEngine != VM_EXEC_ENGINE_NATIVE_API)
        return VMSetError(pVM, RT_SUCCESS_NP(rc) ? VERR_NEM_NOT_AVAILABLE : rc, RT_SRC_POS, "%s", pErrInfo->pszMsg);
    LogRel(("NEM: Not available: %s\n", pErrInfo->pszMsg));
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
    AssertReturn(pVM->nem.s.fdKvm >= 0, VERR_WRONG_ORDER);
    AssertReturn(pVM->nem.s.fdVm >= 0, VERR_WRONG_ORDER);
    AssertReturn(pVM->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API, VERR_WRONG_ORDER);

    /** @todo */

    return VINF_SUCCESS;
}


int nemR3NativeInitCompleted(PVM pVM, VMINITCOMPLETED enmWhat)
{
    /*
     * Make RTThreadPoke work again (disabled for avoiding unnecessary
     * critical section issues in ring-0).
     */
    if (enmWhat == VMINITCOMPLETED_RING3)
        VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ALL_AT_ONCE, nemR3LnxFixThreadPoke, NULL);

    return VINF_SUCCESS;
}


int nemR3NativeTerm(PVM pVM)
{
    /*
     * Per-cpu data
     */
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU pVCpu = pVM->apCpusR3[idCpu];

        if (pVCpu->nem.s.fdVCpu != -1)
        {
            close(pVCpu->nem.s.fdVCpu);
            pVCpu->nem.s.fdVCpu = -1;
        }
        if (pVCpu->nem.s.pRun)
        {
            munmap(pVCpu->nem.s.pRun, pVM->nem.s.cbVCpuMmap);
            pVCpu->nem.s.pRun = NULL;
        }
    }

    /*
     * Global data.
     */
    if (pVM->nem.s.fdVm != -1)
    {
        close(pVM->nem.s.fdVm);
        pVM->nem.s.fdVm = -1;
    }

    if (pVM->nem.s.fdKvm != -1)
    {
        close(pVM->nem.s.fdKvm);
        pVM->nem.s.fdKvm = -1;
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


/*********************************************************************************************************************************
*   Memory management                                                                                                            *
*********************************************************************************************************************************/


/**
 * Allocates a memory slot ID.
 *
 * @returns Slot ID on success, UINT16_MAX on failure.
 */
static uint16_t nemR3LnxMemSlotIdAlloc(PVM pVM)
{
    /* Use the hint first. */
    uint16_t idHint = pVM->nem.s.idPrevSlot;
    if (idHint < _32K - 1)
    {
        int32_t idx = ASMBitNextClear(&pVM->nem.s.bmSlotIds, _32K, idHint);
        Assert(idx < _32K);
        if (idx > 0 && !ASMAtomicBitTestAndSet(&pVM->nem.s.bmSlotIds, idx))
            return pVM->nem.s.idPrevSlot = (uint16_t)idx;
    }

    /*
     * Search the whole map from the start.
     */
    int32_t idx = ASMBitFirstClear(&pVM->nem.s.bmSlotIds, _32K);
    Assert(idx < _32K);
    if (idx > 0 && !ASMAtomicBitTestAndSet(&pVM->nem.s.bmSlotIds, idx))
        return pVM->nem.s.idPrevSlot = (uint16_t)idx;

    Assert(idx < 0 /*shouldn't trigger unless there is a race */);
    return UINT16_MAX; /* caller is expected to assert. */
}


/**
 * Frees a memory slot ID
 */
static void nemR3LnxMemSlotIdFree(PVM pVM, uint16_t idSlot)
{
    if (RT_LIKELY(idSlot < _32K && ASMAtomicBitTestAndClear(&pVM->nem.s.bmSlotIds, idSlot)))
    { /*likely*/ }
    else
        AssertMsgFailed(("idSlot=%u (%#x)\n", idSlot, idSlot));
}



VMMR3_INT_DECL(int) NEMR3NotifyPhysRamRegister(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, void *pvR3,
                                               uint8_t *pu2State, uint32_t *puNemRange)
{
    uint16_t idSlot = nemR3LnxMemSlotIdAlloc(pVM);
    AssertLogRelReturn(idSlot < _32K, VERR_NEM_MAP_PAGES_FAILED);

    Log5(("NEMR3NotifyPhysRamRegister: %RGp LB %RGp, pvR3=%p pu2State=%p (%d) puNemRange=%p (%d) - idSlot=%#x\n",
          GCPhys, cb, pvR3, pu2State, pu2State, puNemRange, *puNemRange, idSlot));

    struct kvm_userspace_memory_region Region;
    Region.slot             = idSlot;
    Region.flags            = 0;
    Region.guest_phys_addr  = GCPhys;
    Region.memory_size      = cb;
    Region.userspace_addr   = (uintptr_t)pvR3;

    int rc = ioctl(pVM->nem.s.fdVm, KVM_SET_USER_MEMORY_REGION, &Region);
    if (rc == 0)
    {
        *pu2State   = 0;
        *puNemRange = idSlot;
        return VINF_SUCCESS;
    }

    LogRel(("NEMR3NotifyPhysRamRegister: %RGp LB %RGp, pvR3=%p, idSlot=%#x failed: %u/%u\n", GCPhys, cb, pvR3, idSlot, rc, errno));
    nemR3LnxMemSlotIdFree(pVM, idSlot);
    return VERR_NEM_MAP_PAGES_FAILED;
}


VMMR3_INT_DECL(bool) NEMR3IsMmio2DirtyPageTrackingSupported(PVM pVM)
{
    RT_NOREF(pVM);
    return true;
}


VMMR3_INT_DECL(int) NEMR3NotifyPhysMmioExMapEarly(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags,
                                                  void *pvRam, void *pvMmio2, uint8_t *pu2State, uint32_t *puNemRange)
{
    Log5(("NEMR3NotifyPhysMmioExMapEarly: %RGp LB %RGp fFlags=%#x pvRam=%p pvMmio2=%p pu2State=%p (%d) puNemRange=%p (%#x)\n",
          GCPhys, cb, fFlags, pvRam, pvMmio2, pu2State, *pu2State, puNemRange, puNemRange ? *puNemRange : UINT32_MAX));
    RT_NOREF(pvRam);

    if (fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_REPLACE)
    {
        /** @todo implement splitting and whatnot of ranges if we want to be 100%
         *        conforming (just modify RAM registrations in MM.cpp to test). */
        AssertLogRelMsgFailedReturn(("%RGp LB %RGp fFlags=%#x pvRam=%p pvMmio2=%p\n", GCPhys, cb, fFlags, pvRam, pvMmio2),
                                    VERR_NEM_MAP_PAGES_FAILED);
    }

    /*
     * Register MMIO2.
     */
    if (fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_MMIO2)
    {
        AssertReturn(pvMmio2, VERR_NEM_MAP_PAGES_FAILED);
        AssertReturn(puNemRange, VERR_NEM_MAP_PAGES_FAILED);

        uint16_t idSlot = nemR3LnxMemSlotIdAlloc(pVM);
        AssertLogRelReturn(idSlot < _32K, VERR_NEM_MAP_PAGES_FAILED);

        struct kvm_userspace_memory_region Region;
        Region.slot             = idSlot;
        Region.flags            = fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_TRACK_DIRTY_PAGES ? KVM_MEM_LOG_DIRTY_PAGES : 0;
        Region.guest_phys_addr  = GCPhys;
        Region.memory_size      = cb;
        Region.userspace_addr   = (uintptr_t)pvMmio2;

        int rc = ioctl(pVM->nem.s.fdVm, KVM_SET_USER_MEMORY_REGION, &Region);
        if (rc == 0)
        {
            *pu2State   = 0;
            *puNemRange = idSlot;
            Log5(("NEMR3NotifyPhysMmioExMapEarly: %RGp LB %RGp fFlags=%#x pvMmio2=%p - idSlot=%#x\n",
                  GCPhys, cb, fFlags, pvMmio2, idSlot));
            return VINF_SUCCESS;
        }

        nemR3LnxMemSlotIdFree(pVM, idSlot);
        AssertLogRelMsgFailedReturn(("%RGp LB %RGp fFlags=%#x, pvMmio2=%p, idSlot=%#x failed: %u/%u\n",
                                     GCPhys, cb, fFlags, pvMmio2, idSlot, errno, rc),
                                    VERR_NEM_MAP_PAGES_FAILED);
    }

    /* MMIO, don't care. */
    *pu2State   = 0;
    *puNemRange = UINT32_MAX;
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
    Log5(("NEMR3NotifyPhysMmioExUnmap: %RGp LB %RGp fFlags=%#x pvRam=%p pvMmio2=%p pu2State=%p puNemRange=%p (%#x)\n",
          GCPhys, cb, fFlags, pvRam, pvMmio2, pu2State, puNemRange, *puNemRange));
    RT_NOREF(pVM, GCPhys, cb, fFlags, pvRam, pvMmio2, pu2State);

    if (fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_REPLACE)
    {
        /** @todo implement splitting and whatnot of ranges if we want to be 100%
         *        conforming (just modify RAM registrations in MM.cpp to test). */
        AssertLogRelMsgFailedReturn(("%RGp LB %RGp fFlags=%#x pvRam=%p pvMmio2=%p\n", GCPhys, cb, fFlags, pvRam, pvMmio2),
                                    VERR_NEM_UNMAP_PAGES_FAILED);
    }

    if (fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_MMIO2)
    {
        uint32_t const idSlot = *puNemRange;
        AssertReturn(idSlot > 0 && idSlot < _32K, VERR_NEM_IPE_4);
        AssertReturn(ASMBitTest(pVM->nem.s.bmSlotIds, idSlot), VERR_NEM_IPE_4);

        struct kvm_userspace_memory_region Region;
        Region.slot             = idSlot;
        Region.flags            = 0;
        Region.guest_phys_addr  = GCPhys;
        Region.memory_size      = 0;    /* this deregisters it. */
        Region.userspace_addr   = (uintptr_t)pvMmio2;

        int rc = ioctl(pVM->nem.s.fdVm, KVM_SET_USER_MEMORY_REGION, &Region);
        if (rc == 0)
        {
            if (pu2State)
                *pu2State = 0;
            *puNemRange = UINT32_MAX;
            nemR3LnxMemSlotIdFree(pVM, idSlot);
            return VINF_SUCCESS;
        }

        AssertLogRelMsgFailedReturn(("%RGp LB %RGp fFlags=%#x, pvMmio2=%p, idSlot=%#x failed: %u/%u\n",
                                     GCPhys, cb, fFlags, pvMmio2, idSlot, errno, rc),
                                    VERR_NEM_UNMAP_PAGES_FAILED);
    }

    if (pu2State)
        *pu2State = UINT8_MAX;
    return VINF_SUCCESS;
}


VMMR3_INT_DECL(int) NEMR3PhysMmio2QueryAndResetDirtyBitmap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t uNemRange,
                                                           void *pvBitmap, size_t cbBitmap)
{
    AssertReturn(uNemRange > 0 && uNemRange < _32K, VERR_NEM_IPE_4);
    AssertReturn(ASMBitTest(pVM->nem.s.bmSlotIds, uNemRange), VERR_NEM_IPE_4);

    RT_NOREF(GCPhys, cbBitmap);

    struct kvm_dirty_log DirtyLog;
    DirtyLog.slot         = uNemRange;
    DirtyLog.padding1     = 0;
    DirtyLog.dirty_bitmap = pvBitmap;

    int rc = ioctl(pVM->nem.s.fdVm, KVM_GET_DIRTY_LOG, &DirtyLog);
    AssertLogRelMsgReturn(rc == 0, ("%RGp LB %RGp idSlot=%#x failed: %u/%u\n", GCPhys, cb, uNemRange, errno, rc),
                          VERR_NEM_QUERY_DIRTY_BITMAP_FAILED);

    return VINF_SUCCESS;
}


VMMR3_INT_DECL(int)  NEMR3NotifyPhysRomRegisterEarly(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, void *pvPages, uint32_t fFlags,
                                                     uint8_t *pu2State, uint32_t *puNemRange)
{
    Log5(("NEMR3NotifyPhysRomRegisterEarly: %RGp LB %RGp pvPages=%p fFlags=%#x\n", GCPhys, cb, pvPages, fFlags));
    *pu2State = UINT8_MAX;

    /* Don't support puttint ROM where there is already RAM.  For
       now just shuffle the registrations till it works... */
    AssertLogRelMsgReturn(!(fFlags & NEM_NOTIFY_PHYS_ROM_F_REPLACE), ("%RGp LB %RGp fFlags=%#x\n", GCPhys, cb, fFlags),
                          VERR_NEM_MAP_PAGES_FAILED);

    /** @todo figure out how to do shadow ROMs.   */

    /*
     * We only allocate a slot number here in case we need to use it to
     * fend of physical handler fun.
     */
    uint16_t idSlot = nemR3LnxMemSlotIdAlloc(pVM);
    AssertLogRelReturn(idSlot < _32K, VERR_NEM_MAP_PAGES_FAILED);

    *pu2State   = 0;
    *puNemRange = idSlot;
    Log5(("NEMR3NotifyPhysRomRegisterEarly: %RGp LB %RGp fFlags=%#x pvPages=%p - idSlot=%#x\n",
          GCPhys, cb, fFlags, pvPages, idSlot));
    RT_NOREF(GCPhys, cb, fFlags, pvPages);
    return VINF_SUCCESS;
}


VMMR3_INT_DECL(int)  NEMR3NotifyPhysRomRegisterLate(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, void *pvPages,
                                                    uint32_t fFlags, uint8_t *pu2State, uint32_t *puNemRange)
{
    Log5(("NEMR3NotifyPhysRomRegisterLate: %RGp LB %RGp pvPages=%p fFlags=%#x pu2State=%p (%d) puNemRange=%p (%#x)\n",
          GCPhys, cb, pvPages, fFlags, pu2State, *pu2State, puNemRange, *puNemRange));

    AssertPtrReturn(pvPages, VERR_NEM_IPE_5);

    uint32_t const idSlot = *puNemRange;
    AssertReturn(idSlot > 0 && idSlot < _32K, VERR_NEM_IPE_4);
    AssertReturn(ASMBitTest(pVM->nem.s.bmSlotIds, idSlot), VERR_NEM_IPE_4);

    *pu2State = UINT8_MAX;

    /*
     * Do the actual setting of the user pages here now that we've
     * got a valid pvPages (typically isn't available during the early
     * notification, unless we're replacing RAM).
     */
    struct kvm_userspace_memory_region Region;
    Region.slot             = idSlot;
    Region.flags            = 0;
    Region.guest_phys_addr  = GCPhys;
    Region.memory_size      = cb;
    Region.userspace_addr   = (uintptr_t)pvPages;

    int rc = ioctl(pVM->nem.s.fdVm, KVM_SET_USER_MEMORY_REGION, &Region);
    if (rc == 0)
    {
        *pu2State   = 0;
        Log5(("NEMR3NotifyPhysRomRegisterEarly: %RGp LB %RGp fFlags=%#x pvPages=%p - idSlot=%#x\n",
              GCPhys, cb, fFlags, pvPages, idSlot));
        return VINF_SUCCESS;
    }
    AssertLogRelMsgFailedReturn(("%RGp LB %RGp fFlags=%#x, pvPages=%p, idSlot=%#x failed: %u/%u\n",
                                 GCPhys, cb, fFlags, pvPages, idSlot, errno, rc),
                                VERR_NEM_MAP_PAGES_FAILED);
}


VMMR3_INT_DECL(void) NEMR3NotifySetA20(PVMCPU pVCpu, bool fEnabled)
{
    Log(("nemR3NativeNotifySetA20: fEnabled=%RTbool\n", fEnabled));
    Assert(VM_IS_NEM_ENABLED(pVCpu->CTX_SUFF(pVM)));
    RT_NOREF(pVCpu, fEnabled);
}


VMM_INT_DECL(void) NEMHCNotifyHandlerPhysicalDeregister(PVMCC pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhys, RTGCPHYS cb,
                                                        RTR3PTR pvMemR3, uint8_t *pu2State)
{
    Log5(("NEMHCNotifyHandlerPhysicalDeregister: %RGp LB %RGp enmKind=%d pvMemR3=%p pu2State=%p (%d)\n",
          GCPhys, cb, enmKind, pvMemR3, pu2State, *pu2State));

    *pu2State = UINT8_MAX;
    RT_NOREF(pVM, enmKind, GCPhys, cb, pvMemR3);
}


void nemHCNativeNotifyHandlerPhysicalRegister(PVMCC pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhys, RTGCPHYS cb)
{
    Log5(("nemHCNativeNotifyHandlerPhysicalRegister: %RGp LB %RGp enmKind=%d\n", GCPhys, cb, enmKind));
    RT_NOREF(pVM, enmKind, GCPhys, cb);
}


void nemHCNativeNotifyHandlerPhysicalModify(PVMCC pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhysOld,
                                            RTGCPHYS GCPhysNew, RTGCPHYS cb, bool fRestoreAsRAM)
{
    Log5(("nemHCNativeNotifyHandlerPhysicalModify: %RGp LB %RGp -> %RGp enmKind=%d fRestoreAsRAM=%d\n",
          GCPhysOld, cb, GCPhysNew, enmKind, fRestoreAsRAM));
    RT_NOREF(pVM, enmKind, GCPhysOld, GCPhysNew, cb, fRestoreAsRAM);
}


int nemHCNativeNotifyPhysPageAllocated(PVMCC pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, uint32_t fPageProt,
                                       PGMPAGETYPE enmType, uint8_t *pu2State)
{
    Log5(("nemHCNativeNotifyPhysPageAllocated: %RGp HCPhys=%RHp fPageProt=%#x enmType=%d *pu2State=%d\n",
          GCPhys, HCPhys, fPageProt, enmType, *pu2State));
    RT_NOREF(pVM, GCPhys, HCPhys, fPageProt, enmType, pu2State);
    return VINF_SUCCESS;
}


VMM_INT_DECL(void) NEMHCNotifyPhysPageProtChanged(PVMCC pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, RTR3PTR pvR3, uint32_t fPageProt,
                                                  PGMPAGETYPE enmType, uint8_t *pu2State)
{
    Log5(("NEMHCNotifyPhysPageProtChanged: %RGp HCPhys=%RHp fPageProt=%#x enmType=%d *pu2State=%d\n",
          GCPhys, HCPhys, fPageProt, enmType, *pu2State));
    Assert(VM_IS_NEM_ENABLED(pVM));
    RT_NOREF(pVM, GCPhys, HCPhys, pvR3, fPageProt, enmType, pu2State);

}


VMM_INT_DECL(void) NEMHCNotifyPhysPageChanged(PVMCC pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhysPrev, RTHCPHYS HCPhysNew,
                                              RTR3PTR pvNewR3, uint32_t fPageProt, PGMPAGETYPE enmType, uint8_t *pu2State)
{
    Log5(("nemHCNativeNotifyPhysPageChanged: %RGp HCPhys=%RHp->%RHp pvNewR3=%p fPageProt=%#x enmType=%d *pu2State=%d\n",
          GCPhys, HCPhysPrev, HCPhysNew, pvNewR3, fPageProt, enmType, *pu2State));
    Assert(VM_IS_NEM_ENABLED(pVM));
    RT_NOREF(pVM, GCPhys, HCPhysPrev, HCPhysNew, pvNewR3, fPageProt, enmType, pu2State);
}


/*********************************************************************************************************************************
*   CPU State                                                                                                                    *
*********************************************************************************************************************************/

/**
 * Worker that imports selected state from KVM.
 */
static int nemHCLnxImportState(PVMCPUCC pVCpu, uint64_t fWhat, PCPUMCTX pCtx, struct kvm_run *pRun)
{
    fWhat &= pVCpu->cpum.GstCtx.fExtrn;
    if (!fWhat)
        return VINF_SUCCESS;

    RT_NOREF(pRun);

    /** @todo */

    /*
     * Update the external mask.
     */
    pCtx->fExtrn &= ~fWhat;
    pVCpu->cpum.GstCtx.fExtrn &= ~fWhat;
    if (!(pVCpu->cpum.GstCtx.fExtrn & CPUMCTX_EXTRN_ALL))
        pVCpu->cpum.GstCtx.fExtrn = 0;

    return VINF_SUCCESS;
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
    STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatImportOnDemand);
    return nemHCLnxImportState(pVCpu, fWhat, &pVCpu->cpum.GstCtx, pVCpu->nem.s.pRun);
}


/**
 * Exports state to KVM.
 */
static int nemHCLnxExportState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, struct kvm_run *pRun)
{
    uint64_t const fExtrn = ~pCtx->fExtrn & CPUMCTX_EXTRN_ALL;
    Assert((~fExtrn & CPUMCTX_EXTRN_ALL) != CPUMCTX_EXTRN_ALL);

    RT_NOREF(pVM, pVCpu, pCtx, pRun);
    /** @todo */

    /*
     * KVM now owns all the state.
     */
    pCtx->fExtrn = CPUMCTX_EXTRN_KEEPER_NEM | CPUMCTX_EXTRN_ALL;
    return VINF_SUCCESS;
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
    STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatQueryCpuTick);
    // KVM_GET_CLOCK?
    RT_NOREF(pVCpu, pcTicks, puAux);
    return VINF_SUCCESS;
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
    // KVM_SET_CLOCK?
    RT_NOREF(pVM, pVCpu, uPausedTscValue);
    return VINF_SUCCESS;
}


VMM_INT_DECL(uint32_t) NEMHCGetFeatures(PVMCC pVM)
{
    RT_NOREF(pVM);
    return NEM_FEAT_F_NESTED_PAGING
         | NEM_FEAT_F_FULL_GST_EXEC;
}



/*********************************************************************************************************************************
*   Execution                                                                                                                    *
*********************************************************************************************************************************/


VMMR3_INT_DECL(bool) NEMR3CanExecuteGuest(PVM pVM, PVMCPU pVCpu)
{
    RT_NOREF(pVM, pVCpu);
    Assert(VM_IS_NEM_ENABLED(pVM));
    return true;
}


bool nemR3NativeSetSingleInstruction(PVM pVM, PVMCPU pVCpu, bool fEnable)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(fEnable);
    return false;
}


void nemR3NativeNotifyFF(PVM pVM, PVMCPU pVCpu, uint32_t fFlags)
{
    int rc = RTThreadPoke(pVCpu->hThread);
    LogFlow(("nemR3NativeNotifyFF: #%u -> %Rrc\n", pVCpu->idCpu, rc));
    AssertRC(rc);
    RT_NOREF(pVM, fFlags);
}


DECLHIDDEN(bool) nemR3NativeNotifyDebugEventChanged(PVM pVM, bool fUseDebugLoop)
{
    RT_NOREF(pVM, fUseDebugLoop);
    return false;
}


DECLHIDDEN(bool) nemR3NativeNotifyDebugEventChangedPerCpu(PVM pVM, PVMCPU pVCpu, bool fUseDebugLoop)
{
    RT_NOREF(pVM, pVCpu, fUseDebugLoop);
    return false;
}


/**
 * Deals with pending interrupt FFs prior to executing guest code.
 */
static VBOXSTRICTRC nemHCLnxHandleInterruptFF(PVM pVM, PVMCPU pVCpu, struct kvm_run *pRun)
{
    RT_NOREF_PV(pVM);

    /*
     * Do not doing anything if TRPM has something pending already as we can
     * only inject one event per KVM_RUN call.  This can only happend if we
     * can directly from the loop in EM, so the inhibit bits must be internal.
     */
    if (!TRPMHasTrap(pVCpu))
    { /* semi likely */ }
    else
    {
        Log8(("nemHCLnxHandleInterruptFF: TRPM has an pending event already\n"));
        return VINF_SUCCESS;
    }

    RT_NOREF(pRun);

    /*
     * In KVM the CPUMCTX_EXTRN_INHIBIT_INT and CPUMCTX_EXTRN_INHIBIT_NMI states
     * are tied together with interrupt and NMI delivery, so we must get and
     * synchronize these all in one go and set both CPUMCTX_EXTRN_INHIBIT_XXX flags.
     * If we don't we may lose the interrupt/NMI we marked pending here when the
     * state is exported again before execution.
     */
    struct kvm_vcpu_events KvmEvents = {0};
    int rcLnx = ioctl(pVCpu->nem.s.fdVCpu, KVM_GET_VCPU_EVENTS, &KvmEvents);
    AssertLogRelMsgReturn(rcLnx == 0, ("rcLnx=%d errno=%d\n", rcLnx, errno), VERR_NEM_IPE_5);

    /* KVM will own the INT + NMI inhibit state soon: */
    pVCpu->cpum.GstCtx.fExtrn = (pVCpu->cpum.GstCtx.fExtrn & ~CPUMCTX_EXTRN_KEEPER_MASK)
                              | CPUMCTX_EXTRN_KEEPER_NEM;

    /*
     * Now, update the state.
     */
    /** @todo skip when possible...   */
    rcLnx = ioctl(pVCpu->nem.s.fdVCpu, KVM_SET_VCPU_EVENTS, &KvmEvents);
    AssertLogRelMsgReturn(rcLnx == 0, ("rcLnx=%d errno=%d\n", rcLnx, errno), VERR_NEM_IPE_5);

    return VINF_SUCCESS;
}


#if 0
/**
 * Handles KVM_EXIT_INTERNAL_ERROR.
 */
static VBOXSTRICTRC nemR3LnxHandleInternalError(PVMCPU pVCpu, struct kvm_run *pRun)
{
    Log(("NEM: KVM_EXIT_INTERNAL_ERROR! suberror=%#x (%d) ndata=%u data=%.*Rhxs\n", pRun->internal.suberror,
         pRun->internal.suberror, pRun->internal.ndata, sizeof(pRun->internal.data), &pRun->internal.data[0]));

    /*
     * Deal with each suberror, returning if we don't want IEM to handle it.
     */
    switch (pRun->internal.suberror)
    {
        case KVM_INTERNAL_ERROR_EMULATION:
        {
            EMHistoryAddExit(pVCpu, EMEXIT_MAKE_FT(EMEXIT_F_KIND_NEM, NEMEXITTYPE_INTERNAL_ERROR_EMULATION),
                             pRun->s.regs.regs.rip + pRun->s.regs.sregs.cs.base, ASMReadTSC());
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitInternalErrorEmulation);
            break;
        }

        case KVM_INTERNAL_ERROR_SIMUL_EX:
        case KVM_INTERNAL_ERROR_DELIVERY_EV:
        case KVM_INTERNAL_ERROR_UNEXPECTED_EXIT_REASON:
        default:
        {
            EMHistoryAddExit(pVCpu, EMEXIT_MAKE_FT(EMEXIT_F_KIND_NEM, NEMEXITTYPE_INTERNAL_ERROR_FATAL),
                             pRun->s.regs.regs.rip + pRun->s.regs.sregs.cs.base, ASMReadTSC());
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitInternalErrorFatal);
            const char *pszName;
            switch (pRun->internal.suberror)
            {
                case KVM_INTERNAL_ERROR_EMULATION:              pszName = "KVM_INTERNAL_ERROR_EMULATION"; break;
                case KVM_INTERNAL_ERROR_SIMUL_EX:               pszName = "KVM_INTERNAL_ERROR_SIMUL_EX"; break;
                case KVM_INTERNAL_ERROR_DELIVERY_EV:            pszName = "KVM_INTERNAL_ERROR_DELIVERY_EV"; break;
                case KVM_INTERNAL_ERROR_UNEXPECTED_EXIT_REASON: pszName = "KVM_INTERNAL_ERROR_UNEXPECTED_EXIT_REASON"; break;
                default:                                        pszName = "unknown"; break;
            }
            LogRel(("NEM: KVM_EXIT_INTERNAL_ERROR! suberror=%#x (%s) ndata=%u data=%.*Rhxs\n", pRun->internal.suberror, pszName,
                    pRun->internal.ndata, sizeof(pRun->internal.data), &pRun->internal.data[0]));
            return VERR_NEM_IPE_0;
        }
    }

    /*
     * Execute instruction in IEM and try get on with it.
     */
    Log2(("nemR3LnxHandleInternalError: Executing instruction at %04x:%08RX64 in IEM\n",
          pRun->s.regs.sregs.cs.selector, pRun->s.regs.regs.rip));
    VBOXSTRICTRC rcStrict = nemHCLnxImportState(pVCpu,
                                                IEM_CPUMCTX_EXTRN_MUST_MASK | CPUMCTX_EXTRN_INHIBIT_INT
                                                 | CPUMCTX_EXTRN_INHIBIT_NMI,
                                                &pVCpu->cpum.GstCtx, pRun);
    if (RT_SUCCESS(rcStrict))
        rcStrict = IEMExecOne(pVCpu);
    return rcStrict;
}
#endif


/**
 * Handles KVM_EXIT_MMIO.
 */
static VBOXSTRICTRC nemHCLnxHandleExitMmio(PVMCC pVM, PVMCPUCC pVCpu, struct kvm_run *pRun)
{
    /*
     * Input validation.
     */
    Assert(pRun->mmio.len <= sizeof(pRun->mmio.data));
    Assert(pRun->mmio.is_write <= 1);

#if 0
    /*
     * We cannot easily act on the exit history here, because the MMIO port
     * exit is stateful and the instruction will be completed in the next
     * KVM_RUN call.  There seems no way to circumvent this.
     */
    EMHistoryAddExit(pVCpu,
                     pRun->mmio.is_write
                     ? EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM, EMEXITTYPE_MMIO_WRITE)
                     : EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM, EMEXITTYPE_MMIO_READ),
                     pRun->s.regs.regs.pc, ASMReadTSC());
#endif

    /*
     * Do the requested job.
     */
    VBOXSTRICTRC rcStrict;
    if (pRun->mmio.is_write)
    {
        rcStrict = PGMPhysWrite(pVM, pRun->mmio.phys_addr, pRun->mmio.data, pRun->mmio.len, PGMACCESSORIGIN_HM);
        Log4(("MmioExit/%u:WRITE %#x LB %u, %.*Rhxs -> rcStrict=%Rrc\n",
              pVCpu->idCpu,
              pRun->mmio.phys_addr, pRun->mmio.len, pRun->mmio.len, pRun->mmio.data, VBOXSTRICTRC_VAL(rcStrict) ));
    }
    else
    {
        rcStrict = PGMPhysRead(pVM, pRun->mmio.phys_addr, pRun->mmio.data, pRun->mmio.len, PGMACCESSORIGIN_HM);
        Log4(("MmioExit/%u: READ %#x LB %u -> %.*Rhxs rcStrict=%Rrc\n",
              pVCpu->idCpu,
              pRun->mmio.phys_addr, pRun->mmio.len, pRun->mmio.len, pRun->mmio.data, VBOXSTRICTRC_VAL(rcStrict) ));
    }
    return rcStrict;
}


static VBOXSTRICTRC nemHCLnxHandleExit(PVMCC pVM, PVMCPUCC pVCpu, struct kvm_run *pRun, bool *pfStatefulExit)
{
    STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitTotal);
    RT_NOREF(pfStatefulExit);
    switch (pRun->exit_reason)
    {
        case KVM_EXIT_EXCEPTION:
            AssertFailed();
            break;

        case KVM_EXIT_MMIO:
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitMmio);
            *pfStatefulExit = true;
            return nemHCLnxHandleExitMmio(pVM, pVCpu, pRun);

        case KVM_EXIT_INTR: /* EINTR */
            //EMHistoryAddExit(pVCpu, EMEXIT_MAKE_FT(EMEXIT_F_KIND_NEM, NEMEXITTYPE_INTERRUPTED),
            //                 pRun->s.regs.regs.pc, ASMReadTSC());
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitIntr);
            Log5(("Intr/%u\n", pVCpu->idCpu));
            return VINF_SUCCESS;

#if 0
        case KVM_EXIT_HYPERCALL:
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitHypercall);
            AssertFailed();
            break;

        case KVM_EXIT_DEBUG:
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitDebug);
            AssertFailed();
            break;

        case KVM_EXIT_SYSTEM_EVENT:
            AssertFailed();
            break;
        case KVM_EXIT_IOAPIC_EOI:
            AssertFailed();
            break;
        case KVM_EXIT_HYPERV:
            AssertFailed();
            break;

        case KVM_EXIT_DIRTY_RING_FULL:
            AssertFailed();
            break;
        case KVM_EXIT_AP_RESET_HOLD:
            AssertFailed();
            break;
        case KVM_EXIT_X86_BUS_LOCK:
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitBusLock);
            AssertFailed();
            break;


        case KVM_EXIT_SHUTDOWN:
            AssertFailed();
            break;

        case KVM_EXIT_FAIL_ENTRY:
            LogRel(("NEM: KVM_EXIT_FAIL_ENTRY! hardware_entry_failure_reason=%#x cpu=%#x\n",
                    pRun->fail_entry.hardware_entry_failure_reason, pRun->fail_entry.cpu));
            EMHistoryAddExit(pVCpu, EMEXIT_MAKE_FT(EMEXIT_F_KIND_NEM, NEMEXITTYPE_FAILED_ENTRY),
                             pRun->s.regs.regs.pc, ASMReadTSC());
            return VERR_NEM_IPE_1;

        case KVM_EXIT_INTERNAL_ERROR:
            /* we're counting sub-reasons inside the function. */
            return nemR3LnxHandleInternalError(pVCpu, pRun);
#endif

        /*
         * Foreign and unknowns.
         */
#if 0
        case KVM_EXIT_IO:
            AssertLogRelMsgFailedReturn(("KVM_EXIT_IO on VCpu #%u at %RX64!\n", pVCpu->idCpu, pRun->s.regs.pc), VERR_NEM_IPE_1);
        case KVM_EXIT_NMI:
            AssertLogRelMsgFailedReturn(("KVM_EXIT_NMI on VCpu #%u at %RX64!\n", pVCpu->idCpu, pRun->s.regs.pc), VERR_NEM_IPE_1);
        case KVM_EXIT_EPR:
            AssertLogRelMsgFailedReturn(("KVM_EXIT_EPR on VCpu #%u at %RX64!\n", pVCpu->idCpu, pRun->s.regs.pc), VERR_NEM_IPE_1);
        case KVM_EXIT_WATCHDOG:
            AssertLogRelMsgFailedReturn(("KVM_EXIT_WATCHDOG on VCpu #%u at %RX64!\n", pVCpu->idCpu, pRun->s.regs.pc), VERR_NEM_IPE_1);
        case KVM_EXIT_ARM_NISV:
            AssertLogRelMsgFailedReturn(("KVM_EXIT_ARM_NISV on VCpu #%u at %RX64!\n", pVCpu->idCpu, pRun->s.regs.pc), VERR_NEM_IPE_1);
        case KVM_EXIT_S390_STSI:
            AssertLogRelMsgFailedReturn(("KVM_EXIT_S390_STSI on VCpu #%u at %RX64!\n", pVCpu->idCpu, pRun->s.regs.pc), VERR_NEM_IPE_1);
        case KVM_EXIT_S390_TSCH:
            AssertLogRelMsgFailedReturn(("KVM_EXIT_S390_TSCH on VCpu #%u at %RX64!\n", pVCpu->idCpu, pRun->s.regs.pc), VERR_NEM_IPE_1);
        case KVM_EXIT_OSI:
            AssertLogRelMsgFailedReturn(("KVM_EXIT_OSI on VCpu #%u at %RX64!\n", pVCpu->idCpu, pRun->s.regs.pc), VERR_NEM_IPE_1);
        case KVM_EXIT_PAPR_HCALL:
            AssertLogRelMsgFailedReturn(("KVM_EXIT_PAPR_HCALL on VCpu #%u at %RX64!\n", pVCpu->idCpu, pRun->s.regs.pc), VERR_NEM_IPE_1);
        case KVM_EXIT_S390_UCONTROL:
            AssertLogRelMsgFailedReturn(("KVM_EXIT_S390_UCONTROL on VCpu #%u at %RX64!\n", pVCpu->idCpu, pRun->s.regs.pc), VERR_NEM_IPE_1);
        case KVM_EXIT_DCR:
            AssertLogRelMsgFailedReturn(("KVM_EXIT_DCR on VCpu #%u at %RX64!\n", pVCpu->idCpu, pRun->s.regs.pc), VERR_NEM_IPE_1);
        case KVM_EXIT_S390_SIEIC:
            AssertLogRelMsgFailedReturn(("KVM_EXIT_S390_SIEIC on VCpu #%u at %RX64!\n", pVCpu->idCpu, pRun->s.regs.pc), VERR_NEM_IPE_1);
        case KVM_EXIT_S390_RESET:
            AssertLogRelMsgFailedReturn(("KVM_EXIT_S390_RESET on VCpu #%u at %RX64!\n", pVCpu->idCpu, pRun->s.regs.pc), VERR_NEM_IPE_1);
        case KVM_EXIT_UNKNOWN:
            AssertLogRelMsgFailedReturn(("KVM_EXIT_UNKNOWN on VCpu #%u at %RX64!\n", pVCpu->idCpu, pRun->s.regs.pc), VERR_NEM_IPE_1);
        case KVM_EXIT_XEN:
            AssertLogRelMsgFailedReturn(("KVM_EXIT_XEN on VCpu #%u at %RX64!\n", pVCpu->idCpu, pRun->s.regs.pc), VERR_NEM_IPE_1);
#endif
        default:
            AssertLogRelMsgFailedReturn(("Unknown exit reason %u on VCpu #%u!\n", pRun->exit_reason, pVCpu->idCpu), VERR_NEM_IPE_1);
    }
    RT_NOREF(pVM, pVCpu);
    return VERR_NOT_IMPLEMENTED;
}


VBOXSTRICTRC nemR3NativeRunGC(PVM pVM, PVMCPU pVCpu)
{
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

    /*
     * The run loop.
     */
    struct kvm_run * const  pRun                = pVCpu->nem.s.pRun;
    const bool              fSingleStepping     = DBGFIsStepping(pVCpu);
    VBOXSTRICTRC            rcStrict            = VINF_SUCCESS;
    bool                    fStatefulExit       = false;  /* For MMIO and IO exits. */
    for (unsigned iLoop = 0;; iLoop++)
    {
        /*
         * Pending interrupts or such?  Need to check and deal with this prior
         * to the state syncing.
         */
        if (VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_INTERRUPT_IRQ | VMCPU_FF_INTERRUPT_FIQ
                                     | VMCPU_FF_INTERRUPT_NMI))
        {
            /* Try inject interrupt. */
            rcStrict = nemHCLnxHandleInterruptFF(pVM, pVCpu, pRun);
            if (rcStrict == VINF_SUCCESS)
            { /* likely */ }
            else
            {
                LogFlow(("NEM/%u: breaking: nemHCLnxHandleInterruptFF -> %Rrc\n", pVCpu->idCpu, VBOXSTRICTRC_VAL(rcStrict) ));
                STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatBreakOnStatus);
                break;
            }
        }

        /*
         * Ensure KVM has the whole state.
         */
        if ((pVCpu->cpum.GstCtx.fExtrn & CPUMCTX_EXTRN_ALL) != CPUMCTX_EXTRN_ALL)
        {
            int rc2 = nemHCLnxExportState(pVM, pVCpu, &pVCpu->cpum.GstCtx, pRun);
            AssertRCReturn(rc2, rc2);
        }

        /*
         * Poll timers and run for a bit.
         *
         * With the VID approach (ring-0 or ring-3) we can specify a timeout here,
         * so we take the time of the next timer event and uses that as a deadline.
         * The rounding heuristics are "tuned" so that rhel5 (1K timer) will boot fine.
         */
        /** @todo See if we cannot optimize this TMTimerPollGIP by only redoing
         *        the whole polling job when timers have changed... */
        uint64_t       offDeltaIgnored;
        uint64_t const nsNextTimerEvt = TMTimerPollGIP(pVM, pVCpu, &offDeltaIgnored); NOREF(nsNextTimerEvt);
        if (   !VM_FF_IS_ANY_SET(pVM, VM_FF_EMT_RENDEZVOUS | VM_FF_TM_VIRTUAL_SYNC)
            && !VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_HM_TO_R3_MASK))
        {
            if (VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM_WAIT, VMCPUSTATE_STARTED_EXEC_NEM))
            {
                //LogFlow(("NEM/%u: Entry @ %04x:%08RX64 IF=%d EFL=%#RX64 SS:RSP=%04x:%08RX64 cr0=%RX64\n",
                //         pVCpu->idCpu, pRun->s.regs.sregs.cs.selector, pRun->s.regs.regs.rip,
                //         !!(pRun->s.regs.regs.rflags & X86_EFL_IF), pRun->s.regs.regs.rflags,
                //         pRun->s.regs.sregs.ss.selector, pRun->s.regs.regs.rsp, pRun->s.regs.sregs.cr0));
                TMNotifyStartOfExecution(pVM, pVCpu);

                int rcLnx = ioctl(pVCpu->nem.s.fdVCpu, KVM_RUN, 0UL);

                VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM, VMCPUSTATE_STARTED_EXEC_NEM_WAIT);
                TMNotifyEndOfExecution(pVM, pVCpu, ASMReadTSC());

#if 0 //def LOG_ENABLED
                if (LogIsFlowEnabled())
                {
                    struct kvm_mp_state MpState = {UINT32_MAX};
                    ioctl(pVCpu->nem.s.fdVCpu, KVM_GET_MP_STATE, &MpState);
                    LogFlow(("NEM/%u: Exit  @ %04x:%08RX64 IF=%d EFL=%#RX64 CR8=%#x Reason=%#x IrqReady=%d Flags=%#x %#lx\n", pVCpu->idCpu,
                             pRun->s.regs.sregs.cs.selector, pRun->s.regs.regs.rip, pRun->if_flag,
                             pRun->s.regs.regs.rflags, pRun->s.regs.sregs.cr8, pRun->exit_reason,
                             pRun->ready_for_interrupt_injection, pRun->flags, MpState.mp_state));
                }
#endif
                fStatefulExit = false;
                if (RT_LIKELY(rcLnx == 0 || errno == EINTR))
                {
                    /*
                     * Deal with the exit.
                     */
                    rcStrict = nemHCLnxHandleExit(pVM, pVCpu, pRun, &fStatefulExit);
                    if (rcStrict == VINF_SUCCESS)
                    { /* hopefully likely */ }
                    else
                    {
                        LogFlow(("NEM/%u: breaking: nemHCLnxHandleExit -> %Rrc\n", pVCpu->idCpu, VBOXSTRICTRC_VAL(rcStrict) ));
                        STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatBreakOnStatus);
                        break;
                    }
                }
                else
                {
                    int rc2 = RTErrConvertFromErrno(errno);
                    AssertLogRelMsgFailedReturn(("KVM_RUN failed: rcLnx=%d errno=%u rc=%Rrc\n", rcLnx, errno, rc2), rc2);
                }

                /*
                 * If no relevant FFs are pending, loop.
                 */
                if (   !VM_FF_IS_ANY_SET(   pVM,   !fSingleStepping ? VM_FF_HP_R0_PRE_HM_MASK    : VM_FF_HP_R0_PRE_HM_STEP_MASK)
                    && !VMCPU_FF_IS_ANY_SET(pVCpu, !fSingleStepping ? VMCPU_FF_HP_R0_PRE_HM_MASK : VMCPU_FF_HP_R0_PRE_HM_STEP_MASK) )
                { /* likely */ }
                else
                {

                    /** @todo Try handle pending flags, not just return to EM loops.  Take care
                     *        not to set important RCs here unless we've handled an exit. */
                    LogFlow(("NEM/%u: breaking: pending FF (%#x / %#RX64)\n",
                             pVCpu->idCpu, pVM->fGlobalForcedActions, (uint64_t)pVCpu->fLocalForcedActions));
                    STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatBreakOnFFPost);
                    break;
                }
            }
            else
            {
                LogFlow(("NEM/%u: breaking: canceled %d (pre exec)\n", pVCpu->idCpu, VMCPU_GET_STATE(pVCpu) ));
                STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatBreakOnCancel);
                break;
            }
        }
        else
        {
            LogFlow(("NEM/%u: breaking: pending FF (pre exec)\n", pVCpu->idCpu));
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatBreakOnFFPre);
            break;
        }
    } /* the run loop */


    /*
     * If the last exit was stateful, commit the state we provided before
     * returning to the EM loop so we have a consistent state and can safely
     * be rescheduled and whatnot.  This may require us to make multiple runs
     * for larger MMIO and I/O operations. Sigh^3.
     *
     * Note! There is no 'ing way to reset the kernel side completion callback
     *       for these stateful i/o exits.  Very annoying interface.
     */
    /** @todo check how this works with string I/O and string MMIO. */
    if (fStatefulExit && RT_SUCCESS(rcStrict))
    {
        STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatFlushExitOnReturn);
        uint32_t const uOrgExit = pRun->exit_reason;
        for (uint32_t i = 0; ; i++)
        {
            pRun->immediate_exit = 1;
            int rcLnx = ioctl(pVCpu->nem.s.fdVCpu, KVM_RUN, 0UL);
            Log(("NEM/%u: Flushed stateful exit -> %d/%d exit_reason=%d\n", pVCpu->idCpu, rcLnx, errno, pRun->exit_reason));
            if (rcLnx == -1 && errno == EINTR)
            {
                switch (i)
                {
                    case 0: STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatFlushExitOnReturn1Loop); break;
                    case 1: STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatFlushExitOnReturn2Loops); break;
                    case 2: STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatFlushExitOnReturn3Loops); break;
                    default: STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatFlushExitOnReturn4PlusLoops); break;
                }
                break;
            }
            AssertLogRelMsgBreakStmt(rcLnx == 0 && pRun->exit_reason == uOrgExit,
                                     ("rcLnx=%d errno=%d exit_reason=%d uOrgExit=%d\n", rcLnx, errno, pRun->exit_reason, uOrgExit),
                                     rcStrict = VERR_NEM_IPE_6);
            VBOXSTRICTRC rcStrict2 = nemHCLnxHandleExit(pVM, pVCpu, pRun, &fStatefulExit);
            if (rcStrict2 == VINF_SUCCESS || rcStrict2 == rcStrict)
            { /* likely */ }
            else if (RT_FAILURE(rcStrict2))
            {
                rcStrict = rcStrict2;
                break;
            }
            else
            {
                AssertLogRelMsgBreakStmt(rcStrict == VINF_SUCCESS,
                                         ("rcStrict=%Rrc rcStrict2=%Rrc\n", VBOXSTRICTRC_VAL(rcStrict), VBOXSTRICTRC_VAL(rcStrict2)),
                                         rcStrict = VERR_NEM_IPE_7);
                rcStrict = rcStrict2;
            }
        }
        pRun->immediate_exit = 0;
    }

    /*
     * If the CPU is running, make sure to stop it before we try sync back the
     * state and return to EM.  We don't sync back the whole state if we can help it.
     */
    if (!VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED, VMCPUSTATE_STARTED_EXEC_NEM))
        VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED, VMCPUSTATE_STARTED_EXEC_NEM_CANCELED);

    if (pVCpu->cpum.GstCtx.fExtrn & CPUMCTX_EXTRN_ALL)
    {
        /* Try anticipate what we might need. */
        uint64_t fImport = IEM_CPUMCTX_EXTRN_MUST_MASK /*?*/;
        if (   (rcStrict >= VINF_EM_FIRST && rcStrict <= VINF_EM_LAST)
            || RT_FAILURE(rcStrict))
            fImport = CPUMCTX_EXTRN_ALL;
        else if (VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_INTERRUPT_IRQ | VMCPU_FF_INTERRUPT_FIQ))
            fImport |= IEM_CPUMCTX_EXTRN_XCPT_MASK;

        if (pVCpu->cpum.GstCtx.fExtrn & fImport)
        {
            int rc2 = nemHCLnxImportState(pVCpu, fImport, &pVCpu->cpum.GstCtx, pRun);
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
        pVCpu->cpum.GstCtx.fExtrn = 0;
        STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatImportOnReturnSkipped);
    }

    LogFlow(("NEM/%u: %08RX64 => %Rrc\n", pVCpu->idCpu, pVCpu->cpum.GstCtx.Pc, VBOXSTRICTRC_VAL(rcStrict) ));
    return rcStrict;
}


/** @page pg_nem_linux NEM/linux - Native Execution Manager, Linux.
 *
 * This is using KVM.
 *
 */

