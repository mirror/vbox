/* $Id$ */
/** @file
 * NEM - Native execution manager, native ring-3 Linux backend.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
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
#include "NEMInternal.h"
#include <VBox/vmm/vmcc.h>

#include <iprt/string.h>
#include <iprt/system.h>

#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <linux/kvm.h>



/**
 * Worker for nemR3NativeInit that gets the hypervisor capabilities.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   pErrInfo            Where to always return error info.
 */
static int nemR3LnxInitCheckCapabilities(PVM pVM, PRTERRINFO pErrInfo)
{
    AssertReturn(pVM->nem.s.fdVm != -1, RTErrInfoSet(pErrInfo, VERR_WRONG_ORDER, "Wrong initalization order"));

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
        CAP_ENTRY_ML(KVM_CAP_HLT),
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
        CAP_ENTRY_ML(KVM_CAP_ADJUST_CLOCK),
        CAP_ENTRY__L(KVM_CAP_INTERNAL_ERROR_DATA),           /* 40 */
#ifdef __KVM_HAVE_VCPU_EVENTS
        CAP_ENTRY__L(KVM_CAP_VCPU_EVENTS),
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
#ifdef __KVM_HAVE_XSAVE
        CAP_ENTRY__L(KVM_CAP_XSAVE),
#endif
#ifdef __KVM_HAVE_XCRS
        CAP_ENTRY__L(KVM_CAP_XCRS),
#endif
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
        CAP_ENTRY__L(KVM_CAP_ARM_VM_IPA_SIZE),
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
        CAP_ENTRY__L(KVM_CAP_X86_USER_SPACE_MSR),
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
        int rc = ioctl(pVM->nem.s.fdVm, KVM_CHECK_EXTENSION, s_aCaps[i].iCap);
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

    return rcRet;
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
            return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS,
                              "KVM_CREATE_VCPU failed for VCpu #%u: %d", idCpu, errno);

        /* Map the KVM_RUN area. */
        pVCpu->nem.s.pRun = (struct kvm_run *)mmap(NULL, pVM->nem.s.cbVCpuMmap, PROT_READ | PROT_WRITE, MAP_SHARED,
                                                   pVCpu->nem.s.fdVCpu, 0 /*offset*/);
        if ((void *)pVCpu->nem.s.pRun == MAP_FAILED)
            return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS, "mmap failed for VCpu #%u: %d", idCpu, errno);
    }
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
         * Create an empty VM since it is recommended we check capabilities on
         * the VM rather than the system descriptor.
         */
        int fdVm = ioctl(fdKvm, KVM_CREATE_VM, 0UL /* Type must be zero on x86 */);
        if (fdVm >= 0)
        {
            pVM->nem.s.fdVm = fdVm;

            /*
             * Check capabilities.
             */
            rc = nemR3LnxInitCheckCapabilities(pVM, pErrInfo);
            if (RT_SUCCESS(rc))
            {
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
                        STAMR3RegisterF(pVM, &pNemCpu->StatQueryCpuTick,        STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of TSC queries",                  "/NEM/CPU%u/QueryCpuTick", idCpu);
                    }

                    /*
                     * Success.
                     */
                    return VINF_SUCCESS;
                }

                /*
                 * Bail out.
                 */
            }
            close(fdVm);
            pVM->nem.s.fdVm = -1;
        }
        else
            rc = RTErrInfoSetF(pErrInfo, VERR_NEM_VM_CREATE_FAILED, "KVM_CREATE_VM failed: %u", errno);
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

    return VERR_NOT_IMPLEMENTED;
}


int nemR3NativeInitCompleted(PVM pVM, VMINITCOMPLETED enmWhat)
{
    RT_NOREF(pVM, enmWhat);
    return VINF_SUCCESS;
}


int nemR3NativeTerm(PVM pVM)
{
    RT_NOREF(pVM);
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


VBOXSTRICTRC nemR3NativeRunGC(PVM pVM, PVMCPU pVCpu)
{
    RT_NOREF(pVM, pVCpu);
    return VERR_NOT_IMPLEMENTED;
}


VMMR3_INT_DECL(bool) NEMR3CanExecuteGuest(PVM pVM, PVMCPU pVCpu)
{
    /*
     * Only execute when the A20 gate is enabled as I cannot immediately
     * spot any A20 support in KVM.
     */
    RT_NOREF(pVM);
    Assert(VM_IS_NEM_ENABLED(pVM));
    return PGMPhysIsA20Enabled(pVCpu);
}


bool nemR3NativeSetSingleInstruction(PVM pVM, PVMCPU pVCpu, bool fEnable)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(fEnable);
    return false;
}


/**
 * Forced flag notification call from VMEmt.cpp.
 *
 * This is only called when pVCpu is in the VMCPUSTATE_STARTED_EXEC_NEM state.
 *
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure of the CPU
 *                          to be notified.
 * @param   fFlags          Notification flags, VMNOTIFYFF_FLAGS_XXX.
 */
void nemR3NativeNotifyFF(PVM pVM, PVMCPU pVCpu, uint32_t fFlags)
{
    RT_NOREF(pVM, pVCpu, fFlags);
}


VMMR3_INT_DECL(int) NEMR3NotifyPhysRamRegister(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, void *pvR3,
                                               uint8_t *pu2State, uint32_t *puNemRange)
{
    Log5(("NEMR3NotifyPhysRamRegister: %RGp LB %RGp, pvR3=%p pu2State=%p (%d) puNemRange=%p (%d)\n",
          GCPhys, cb, pvR3, pu2State, pu2State, puNemRange, *puNemRange));
    *pu2State = UINT8_MAX;
    RT_NOREF(pVM, GCPhys, cb, pvR3, puNemRange);
    return VINF_SUCCESS;
}


VMMR3_INT_DECL(bool) NEMR3IsMmio2DirtyPageTrackingSupported(PVM pVM)
{
    RT_NOREF(pVM);
    return false;
}


VMMR3_INT_DECL(int) NEMR3NotifyPhysMmioExMapEarly(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags,
                                                  void *pvRam, void *pvMmio2, uint8_t *pu2State, uint32_t *puNemRange)
{
    Log5(("NEMR3NotifyPhysMmioExMapEarly: %RGp LB %RGp fFlags=%#x pvRam=%p pvMmio2=%p pu2State=%p (%d) puNemRange=%p (%#x)\n",
          GCPhys, cb, fFlags, pvRam, pvMmio2, pu2State, *pu2State, puNemRange, puNemRange ? *puNemRange : UINT32_MAX));
    RT_NOREF(pVM, GCPhys, cb, fFlags, pvRam, pvMmio2, puNemRange);
    *pu2State = UINT8_MAX;
    return VINF_SUCCESS;
}


VMMR3_INT_DECL(int) NEMR3NotifyPhysMmioExMapLate(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags,
                                                 void *pvRam, void *pvMmio2, uint32_t *puNemRange)
{
    RT_NOREF(pVM, GCPhys, cb, fFlags, pvRam, pvMmio2, puNemRange);
    return VINF_SUCCESS;
}


VMMR3_INT_DECL(int) NEMR3NotifyPhysMmioExUnmap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags, void *pvRam,
                                               void *pvMmio2, uint8_t *pu2State)
{
    Log5(("NEMR3NotifyPhysMmioExUnmap: %RGp LB %RGp fFlags=%#x pvRam=%p pvMmio2=%p pu2State=%p\n",
          GCPhys, cb, fFlags, pvRam, pvMmio2, pu2State));
    RT_NOREF(pVM, GCPhys, cb, fFlags, pvRam, pvMmio2, pu2State);
    if (pu2State)
        *pu2State = UINT8_MAX;
    return VINF_SUCCESS;
}


VMMR3_INT_DECL(int) NEMR3PhysMmio2QueryAndResetDirtyBitmap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t uNemRange,
                                                           void *pvBitmap, size_t cbBitmap)
{
    RT_NOREF(pVM, GCPhys, cb, uNemRange, pvBitmap, cbBitmap);
    AssertFailed();
    return VERR_NOT_IMPLEMENTED;
}


VMMR3_INT_DECL(int)  NEMR3NotifyPhysRomRegisterEarly(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, void *pvPages, uint32_t fFlags,
                                                     uint8_t *pu2State)
{
    Log5(("NEMR3NotifyPhysRomRegisterEarly: %RGp LB %RGp pvPages=%p fFlags=%#x\n", GCPhys, cb, pvPages, fFlags));
    *pu2State = UINT8_MAX;
    RT_NOREF(pVM, GCPhys, cb, pvPages, fFlags);
    return VINF_SUCCESS;
}


VMMR3_INT_DECL(int)  NEMR3NotifyPhysRomRegisterLate(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, void *pvPages,
                                                    uint32_t fFlags, uint8_t *pu2State)
{
    Log5(("NEMR3NotifyPhysRomRegisterLate: %RGp LB %RGp pvPages=%p fFlags=%#x pu2State=%p\n",
          GCPhys, cb, pvPages, fFlags, pu2State));
    *pu2State = UINT8_MAX;
    RT_NOREF(pVM, GCPhys, cb, pvPages, fFlags);
    return VINF_SUCCESS;
}


/**
 * Called when the A20 state changes.
 *
 * @param   pVCpu           The CPU the A20 state changed on.
 * @param   fEnabled        Whether it was enabled (true) or disabled.
 */
VMMR3_INT_DECL(void) NEMR3NotifySetA20(PVMCPU pVCpu, bool fEnabled)
{
    Log(("nemR3NativeNotifySetA20: fEnabled=%RTbool\n", fEnabled));
    Assert(VM_IS_NEM_ENABLED(pVCpu->CTX_SUFF(pVM)));
    RT_NOREF(pVCpu, fEnabled);
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

    RT_NOREF(pVCpu, fWhat);
    return VERR_NOT_IMPLEMENTED;
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
    // KVM_SET_CLOCK?
    RT_NOREF(pVM, pVCpu, uPausedTscValue);
    return VERR_NOT_IMPLEMENTED;
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


/** @page pg_nem_linux NEM/linux - Native Execution Manager, Linux.
 *
 * This is using KVM.
 *
 */

