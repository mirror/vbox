/* $Id$ */
/** @file
 * GIM - Hyper-V, Internal header file.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___GIMHvInternal_h
#define ___GIMHvInternal_h

#include <VBox/vmm/gim.h>
#include <VBox/vmm/cpum.h>


/** @name Hyper-V base feature identification.
 * Features based on current partition privileges (per-VM).
 * @{
 */
/** Virtual processor runtime MSR available. */
#define GIM_HV_BASE_FEAT_VP_RUNTIME_MSR           RT_BIT(0)
/** Partition reference counter MSR available. */
#define GIM_HV_BASE_FEAT_PART_TIME_REF_COUNT_MSR  RT_BIT(1)
/** Basic Synthetic Interrupt Controller MSRs available. */
#define GIM_HV_BASE_FEAT_BASIC_SYNTH_IC           RT_BIT(2)
/** Synthetic Timer MSRs available. */
#define GIM_HV_BASE_FEAT_SYNTH_TIMER_MSRS         RT_BIT(3)
/** APIC access MSRs (EOI, ICR, TPR) available. */
#define GIM_HV_BASE_FEAT_APIC_ACCESS_MSRS         RT_BIT(4)
/** Hypercall MSRs available. */
#define GIM_HV_BASE_FEAT_HYPERCALL_MSRS           RT_BIT(5)
/** Access to VCPU index MSR available. */
#define GIM_HV_BASE_FEAT_VP_ID_MSR                RT_BIT(6)
/** Virtual system reset MSR available. */
#define GIM_HV_BASE_FEAT_VIRT_SYS_RESET_MSR       RT_BIT(7)
/** Statistic pages MSRs available. */
#define GIM_HV_BASE_FEAT_STAT_PAGES_MSR           RT_BIT(8)
/** Paritition reference TSC MSR available. */
#define GIM_HV_BASE_FEAT_PART_REF_TSC_MSR         RT_BIT(9)
/** Virtual guest idle state MSR available. */
#define GIM_HV_BASE_FEAT_GUEST_IDLE_STATE_MSR     RT_BIT(10)
/** Timer frequency MSRs (TSC and APIC) available. */
#define GIM_HV_BASE_FEAT_TIMER_FREQ_MSRS          RT_BIT(11)
/** Debug MSRs available. */
#define GIM_HV_BASE_FEAT_DEBUG_MSRS               RT_BIT(12)
/** @}  */

/** @name Hyper-V partition-creation feature identification.
 * Indicates flags specified during partition creation.
 * @{
 */
/** Create partitions. */
#define GIM_HV_PART_FLAGS_CREATE_PART             RT_BIT(0)
/** Access partition Id. */
#define GIM_HV_PART_FLAGS_ACCESS_PART_ID          RT_BIT(1)
/** Access memory pool. */
#define GIM_HV_PART_FLAGS_ACCESS_MEMORY_POOL      RT_BIT(2)
/** Adjust message buffers. */
#define GIM_HV_PART_FLAGS_ADJUST_MSG_BUFFERS      RT_BIT(3)
/** Post messages. */
#define GIM_HV_PART_FLAGS_POST_MSGS               RT_BIT(4)
/** Signal events. */
#define GIM_HV_PART_FLAGS_SIGNAL_EVENTS           RT_BIT(5)
/** Create port. */
#define GIM_HV_PART_FLAGS_CREATE_PORT             RT_BIT(6)
/** Connect port. */
#define GIM_HV_PART_FLAGS_CONNECT_PORT            RT_BIT(7)
/** Access statistics. */
#define GIM_HV_PART_FLAGS_ACCESS_STATS            RT_BIT(8)
/** Debugging.*/
#define GIM_HV_PART_FLAGS_DEBUGGING               RT_BIT(11)
/** CPU management. */
#define GIM_HV_PART_FLAGS_CPU_MGMT                RT_BIT(12)
/** CPU profiler. */
#define GIM_HV_PART_FLAGS_CPU_PROFILER            RT_BIT(13)
/** Enable expanded stack walking. */
#define GIM_HV_PART_FLAGS_EXPANDED_STACK_WALK     RT_BIT(14)
/** @}  */

/** @name Hyper-V power management feature identification.
 * @{
 */
/** Maximum CPU power state C0. */
#define GIM_HV_PM_MAX_CPU_POWER_STATE_C0          RT_BIT(0)
/** Maximum CPU power state C1. */
#define GIM_HV_PM_MAX_CPU_POWER_STATE_C1          RT_BIT(1)
/** Maximum CPU power state C2. */
#define GIM_HV_PM_MAX_CPU_POWER_STATE_C2          RT_BIT(2)
/** Maximum CPU power state C3. */
#define GIM_HV_PM_MAX_CPU_POWER_STATE_C3          RT_BIT(3)
/** HPET is required to enter C3 power state. */
#define GIM_HV_PM_HPET_REQD_FOR_C3                RT_BIT(4)
/** @}  */

/** @name Hyper-V miscellaneous feature identification.
 * Miscellaneous features available for the current partition.
 * @{
 */
/** MWAIT instruction available. */
#define GIM_HV_MISC_FEAT_MWAIT                    RT_BIT(0)
/** Guest debugging support available. */
#define GIM_HV_MISC_FEAT_GUEST_DEBUGGING          RT_BIT(1)
/** Performance monitor support is available. */
#define GIM_HV_MISC_FEAT_PERF_MON                 RT_BIT(2)
/** Support for physical CPU dynamic partitioning events. */
#define GIM_HV_MISC_FEAT_PCPU_DYN_PART_EVENT      RT_BIT(3)
/** Support for passing hypercall input parameter block via XMM registers. */
#define GIM_HV_MISC_FEAT_XMM_HYPERCALL_INPUT      RT_BIT(4)
/** Support for virtual guest idle state. */
#define GIM_HV_MISC_FEAT_GUEST_IDLE_STATE         RT_BIT(5)
/** Support for hypervisor sleep state. */
#define GIM_HV_MISC_FEAT_HYPERVISOR_SLEEP_STATE   RT_BIT(6)
/** Support for querying NUMA distances. */
#define GIM_HV_MISC_FEAT_QUERY_NUMA_DISTANCE      RT_BIT(7)
/** Support for determining timer frequencies. */
#define GIM_HV_MISC_FEAT_TIMER_FREQ               RT_BIT(8)
/** Support for injecting synthetic machine checks. */
#define GIM_HV_MISC_FEAT_INJECT_SYNTH_MC_XCPT     RT_BIT(9)
/** Support for guest crash MSRs. */
#define GIM_HV_MISC_FEAT_GUEST_CRASH_MSRS         RT_BIT(10)
/** Support for debug MSRs. */
#define GIM_HV_MISC_FEAT_DEBUG_MSRS               RT_BIT(11)
/** Npiep1 Available */ /** @todo What the heck is this? */
#define GIM_HV_MISC_FEAT_NPIEP1                   RT_BIT(12)
/** Disable hypervisor available. */
#define GIM_HV_MISC_FEAT_DISABLE_HYPERVISOR       RT_BIT(13)
/** @}  */

/** @name Hyper-V implementation recommendations.
 * Recommendations from the hypervisor for the guest for optimal performance.
 * @{
 */
/** Use hypercall for address space switches rather than MOV CR3. */
#define GIM_HV_HINT_HYPERCALL_FOR_PROCESS_SWITCH  RT_BIT(0)
/** Use hypercall for local TLB flushes rather than INVLPG/MOV CR3. */
#define GIM_HV_HINT_HYPERCALL_FOR_TLB_FLUSH       RT_BIT(1)
/** Use hypercall for inter-CPU TLB flushes rather than IPIs. */
#define GIM_HV_HINT_HYPERCALL_FOR_TLB_SHOOTDOWN   RT_BIT(2)
/** Use MSRs for APIC access (EOI, ICR, TPR) rather than MMIO. */
#define GIM_HV_HINT_MSR_FOR_APIC_ACCESS           RT_BIT(3)
/** Use hypervisor provided MSR for a system reset. */
#define GIM_HV_HINT_MSR_FOR_SYS_RESET             RT_BIT(4)
/** Relax timer-related checks (watchdogs/deadman timeouts) that rely on
 *  timely deliver of external interrupts. */
#define GIM_HV_HINT_RELAX_TIME_CHECKS             RT_BIT(5)
/** Use DMA remapping. */
#define GIM_HV_HINT_DMA_REMAPPING                 RT_BIT(6)
/** Use interrupt remapping. */
#define GIM_HV_HINT_INTERRUPT_REMAPPING           RT_BIT(7)
/** Use X2APIC MSRs rather than MMIO. */
#define GIM_HV_HINT_X2APIC_MSRS                   RT_BIT(8)
/** Deprecate Auto EOI (end of interrupt). */
#define GIM_HV_HINT_DEPRECATE_AUTO_EOI            RT_BIT(9)
/** @}  */


/** @name Hyper-V implementation hardware features.
 * Which hardware features are in use by the hypervisor.
 * @{
 */
/** APIC overlay is used. */
#define GIM_HV_HOST_FEAT_AVIC                     RT_BIT(0)
/** MSR bitmaps is used. */
#define GIM_HV_HOST_FEAT_MSR_BITMAP               RT_BIT(1)
/** Architectural performance counter supported. */
#define GIM_HV_HOST_FEAT_PERF_COUNTER             RT_BIT(2)
/** Nested paging is used. */
#define GIM_HV_HOST_FEAT_NESTED_PAGING            RT_BIT(3)
/** DMA remapping is used. */
#define GIM_HV_HOST_FEAT_DMA_REMAPPING            RT_BIT(4)
/** Interrupt remapping is used. */
#define GIM_HV_HOST_FEAT_INTERRUPT_REMAPPING      RT_BIT(5)
/** Memory patrol scrubber is present. */
#define GIM_HV_HOST_FEAT_MEM_PATROL_SCRUBBER      RT_BIT(6)
/** @}  */


/** @name Hyper-V MSRs.
 * @{
 */
/** Start of range 0. */
#define MSR_GIM_HV_RANGE0_START                   UINT32_C(0x40000000)
/** Guest OS identification (R/W) */
#define MSR_GIM_HV_GUEST_OS_ID                    UINT32_C(0x40000000)
/** Enable hypercall interface (R/W) */
#define MSR_GIM_HV_HYPERCALL                      UINT32_C(0x40000001)
/** Virtual processor's (VCPU) index (R) */
#define MSR_GIM_HV_VP_INDEX                       UINT32_C(0x40000002)
/** Reset operation (R/W) */
#define MSR_GIM_HV_RESET                          UINT32_C(0x40000003)
/** End of range 0. */
#define MSR_GIM_HV_RANGE0_END                     MSR_GIM_HV_RESET

/** Start of range 1. */
#define MSR_GIM_HV_RANGE1_START                   UINT32_C(0x40000010)
/** Virtual processor's (VCPU) runtime (R) */
#define MSR_GIM_HV_VP_RUNTIME                     UINT32_C(0x40000010)
/** End of range 1. */
#define MSR_GIM_HV_RANGE1_END                     MSR_GIM_HV_VP_RUNTIME

/** Start of range 2. */
#define MSR_GIM_HV_RANGE2_START                   UINT32_C(0x40000020)
/** Per-VM reference counter (R) */
#define MSR_GIM_HV_TIME_REF_COUNT                 UINT32_C(0x40000020)
/** Per-VM TSC page (R/W) */
#define MSR_GIM_HV_REF_TSC                        UINT32_C(0x40000021)
/** Frequency of TSC in Hz as reported by the hypervisor (R) */
#define MSR_GIM_HV_TSC_FREQ                       UINT32_C(0x40000022)
/** Frequency of LAPIC in Hz as reported by the hypervisor (R) */
#define MSR_GIM_HV_APIC_FREQ                      UINT32_C(0x40000023)
/** End of range 2. */
#define MSR_GIM_HV_RANGE2_END                     MSR_GIM_HV_APIC_FREQ

/** Start of range 3. */
#define MSR_GIM_HV_RANGE3_START                   UINT32_C(0x40000070)
/** Access to APIC EOI (End-Of-Interrupt) register (W) */
#define MSR_GIM_HV_EOI                            UINT32_C(0x40000070)
/** Access to APIC ICR (Interrupt Command) register (R/W) */
#define MSR_GIM_HV_ICR                            UINT32_C(0x40000071)
/** Access to APIC TPR (Task Priority) register (R/W) */
#define MSR_GIM_HV_TPR                            UINT32_C(0x40000072)
/** Enables lazy EOI processing (R/W) */
#define MSR_GIM_HV_APIC_ASSIST_PAGE               UINT32_C(0x40000073)
/** End of range 3. */
#define MSR_GIM_HV_RANGE3_END                     MSR_GIM_HV_APIC_ASSIST_PAGE

/** Start of range 4. */
#define MSR_GIM_HV_RANGE4_START                   UINT32_C(0x40000080)
/** Control behaviour of synthetic interrupt controller (R/W) */
#define MSR_GIM_HV_SCONTROL                       UINT32_C(0x40000080)
/** Synthetic interrupt controller version (R) */
#define MSR_GIM_HV_SVERSION                       UINT32_C(0x40000081)
/** Base address of synthetic interrupt event flag (R/W) */
#define MSR_GIM_HV_SIEFP                          UINT32_C(0x40000082)
/** Base address of synthetic interrupt parameter page (R/W) */
#define MSR_GIM_HV_SIMP                           UINT32_C(0x40000083)
/** End-Of-Message in synthetic interrupt parameter page (W) */
#define MSR_GIM_HV_EOM                            UINT32_C(0x40000084)
/** End of range 4. */
#define MSR_GIM_HV_RANGE4_END                     MSR_GIM_HV_EOM

/** Start of range 5. */
#define MSR_GIM_HV_RANGE5_START                   UINT32_C(0x40000090)
/** Configures synthetic interrupt source 0 (R/W) */
#define MSR_GIM_HV_SINT0                          UINT32_C(0x40000090)
/** Configures synthetic interrupt source 1 (R/W) */
#define MSR_GIM_HV_SINT1                          UINT32_C(0x40000091)
/** Configures synthetic interrupt source 2 (R/W) */
#define MSR_GIM_HV_SINT2                          UINT32_C(0x40000092)
/** Configures synthetic interrupt source 3 (R/W) */
#define MSR_GIM_HV_SINT3                          UINT32_C(0x40000093)
/** Configures synthetic interrupt source 4 (R/W) */
#define MSR_GIM_HV_SINT4                          UINT32_C(0x40000094)
/** Configures synthetic interrupt source 5 (R/W) */
#define MSR_GIM_HV_SINT5                          UINT32_C(0x40000095)
/** Configures synthetic interrupt source 6 (R/W) */
#define MSR_GIM_HV_SINT6                          UINT32_C(0x40000096)
/** Configures synthetic interrupt source 7 (R/W) */
#define MSR_GIM_HV_SINT7                          UINT32_C(0x40000097)
/** Configures synthetic interrupt source 8 (R/W) */
#define MSR_GIM_HV_SINT8                          UINT32_C(0x40000098)
/** Configures synthetic interrupt source 9 (R/W) */
#define MSR_GIM_HV_SINT9                          UINT32_C(0x40000099)
/** Configures synthetic interrupt source 10 (R/W) */
#define MSR_GIM_HV_SINT10                         UINT32_C(0x4000009A)
/** Configures synthetic interrupt source 11 (R/W) */
#define MSR_GIM_HV_SINT11                         UINT32_C(0x4000009B)
/** Configures synthetic interrupt source 12 (R/W) */
#define MSR_GIM_HV_SINT12                         UINT32_C(0x4000009C)
/** Configures synthetic interrupt source 13 (R/W) */
#define MSR_GIM_HV_SINT13                         UINT32_C(0x4000009D)
/** Configures synthetic interrupt source 14 (R/W) */
#define MSR_GIM_HV_SINT14                         UINT32_C(0x4000009E)
/** Configures synthetic interrupt source 15 (R/W) */
#define MSR_GIM_HV_SINT15                         UINT32_C(0x4000009F)
/** End of range 5. */
#define MSR_GIM_HV_RANGE5_END                     MSR_GIM_HV_SINT15

/** Start of range 6. */
#define MSR_GIM_HV_RANGE6_START                   UINT32_C(0x400000B0)
/** Configures register for synthetic timer 0 (R/W) */
#define MSR_GIM_HV_STIMER0_CONFIG                 UINT32_C(0x400000B0)
/** Expiration time or period for synthetic timer 0 (R/W) */
#define MSR_GIM_HV_STIMER0_COUNT                  UINT32_C(0x400000B1)
/** Configures register for synthetic timer 1 (R/W) */
#define MSR_GIM_HV_STIMER1_CONFIG                 UINT32_C(0x400000B2)
/** Expiration time or period for synthetic timer 1 (R/W) */
#define MSR_GIM_HV_STIMER1_COUNT                  UINT32_C(0x400000B3)
/** Configures register for synthetic timer 2 (R/W) */
#define MSR_GIM_HV_STIMER2_CONFIG                 UINT32_C(0x400000B4)
/** Expiration time or period for synthetic timer 2 (R/W) */
#define MSR_GIM_HV_STIMER2_COUNT                  UINT32_C(0x400000B5)
/** Configures register for synthetic timer 3 (R/W) */
#define MSR_GIM_HV_STIMER3_CONFIG                 UINT32_C(0x400000B6)
/** Expiration time or period for synthetic timer 3 (R/W) */
#define MSR_GIM_HV_STIMER3_COUNT                  UINT32_C(0x400000B7)
/** End of range 6. */
#define MSR_GIM_HV_RANGE6_END                     MSR_GIM_HV_STIMER3_COUNT

/** Start of range 7. */
#define MSR_GIM_HV_RANGE7_START                   UINT32_C(0x400000C1)
/** Trigger to transition to power state C1 (R) */
#define MSR_GIM_HV_POWER_STATE_TRIGGER_C1         UINT32_C(0x400000C1)
/** Trigger to transition to power state C2 (R) */
#define MSR_GIM_HV_POWER_STATE_TRIGGER_C2         UINT32_C(0x400000C2)
/** Trigger to transition to power state C3 (R) */
#define MSR_GIM_HV_POWER_STATE_TRIGGER_C3         UINT32_C(0x400000C3)
/** End of range 7. */
#define MSR_GIM_HV_RANGE7_END                     MSR_GIM_HV_POWER_STATE_TRIGGER_C3

/** Start of range 8. */
#define MSR_GIM_HV_RANGE8_START                   UINT32_C(0x400000D1)
/** Configure the recipe for power state transitions to C1 (R/W) */
#define MSR_GIM_HV_POWER_STATE_CONFIG_C1          UINT32_C(0x400000D1)
/** Configure the recipe for power state transitions to C2 (R/W) */
#define MSR_GIM_HV_POWER_STATE_CONFIG_C2          UINT32_C(0x400000D2)
/** Configure the recipe for power state transitions to C3 (R/W) */
#define MSR_GIM_HV_POWER_STATE_CONFIG_C3          UINT32_C(0x400000D3)
/** End of range 8. */
#define MSR_GIM_HV_RANGE8_END                     MSR_GIM_HV_POWER_STATE_CONFIG_C3

/** Start of range 9. */
#define MSR_GIM_HV_RANGE9_START                   UINT32_C(0x400000E0)
/** Map the guest's retail partition stats page (R/W) */
#define MSR_GIM_HV_STATS_PART_RETAIL_PAGE         UINT32_C(0x400000E0)
/** Map the guest's internal partition stats page (R/W) */
#define MSR_GIM_HV_STATS_PART_INTERNAL_PAGE       UINT32_C(0x400000E1)
/** Map the guest's retail VP stats page (R/W) */
#define MSR_GIM_HV_STATS_VP_RETAIL_PAGE           UINT32_C(0x400000E2)
/** Map the guest's internal VP stats page (R/W) */
#define MSR_GIM_HV_STATS_VP_INTERNAL_PAGE         UINT32_C(0x400000E3)
/** End of range 9. */
#define MSR_GIM_HV_RANGE9_END                     MSR_GIM_HV_STATS_VP_INTERNAL_PAGE

/** Start of range 10. */
#define MSR_GIM_HV_RANGE10_START                  UINT32_C(0x400000F0)
/** Trigger the guest's transition to idle power state (R) */
#define MSR_GIM_HV_GUEST_IDLE                     UINT32_C(0x400000F0)
/** Synthetic debug control. */
#define MSR_GIM_HV_SYNTH_DEBUG_CONTROL            UINT32_C(0x400000F1)
/** Synthetic debug status. */
#define MSR_GIM_HV_SYNTH_DEBUG_STATUS             UINT32_C(0x400000F2)
/** Synthetic debug send buffer. */
#define MSR_GIM_HV_SYNTH_DEBUG_SEND_BUFFER        UINT32_C(0x400000F3)
/** Synthetic debug receive buffer. */
#define MSR_GIM_HV_SYNTH_DEBUG_RECEIVE_BUFFER     UINT32_C(0x400000F4)
/** Synthetic debug pending buffer. */
#define MSR_GIM_HV_SYNTH_DEBUG_PENDING_BUFFER     UINT32_C(0x400000F5)
/** End of range 10. */
#define MSR_GIM_HV_RANGE10_END                    MSR_GIM_HV_SYNTH_DEBUG_PENDING_BUFFER

/** Start of range 11. */
#define MSR_GIM_HV_RANGE11_START                  UINT32_C(0x40000100)
/** Guest crash MSR 0. */
#define MSR_GIM_HV_CRASH_P0                       UINT32_C(0x40000100)
/** Guest crash MSR 1. */
#define MSR_GIM_HV_CRASH_P1                       UINT32_C(0x40000101)
/** Guest crash MSR 2. */
#define MSR_GIM_HV_CRASH_P2                       UINT32_C(0x40000102)
/** Guest crash MSR 3. */
#define MSR_GIM_HV_CRASH_P3                       UINT32_C(0x40000103)
/** Guest crash MSR 4. */
#define MSR_GIM_HV_CRASH_P4                       UINT32_C(0x40000104)
/** Guest crash control. */
#define MSR_GIM_HV_CRASH_CTL                      UINT32_C(0x40000105)
/** End of range 11. */
#define MSR_GIM_HV_RANGE11_END                    MSR_GIM_HV_CRASH_CTL
/** @} */

AssertCompile(MSR_GIM_HV_RANGE0_START  <= MSR_GIM_HV_RANGE0_END);
AssertCompile(MSR_GIM_HV_RANGE1_START  <= MSR_GIM_HV_RANGE1_END);
AssertCompile(MSR_GIM_HV_RANGE2_START  <= MSR_GIM_HV_RANGE2_END);
AssertCompile(MSR_GIM_HV_RANGE3_START  <= MSR_GIM_HV_RANGE3_END);
AssertCompile(MSR_GIM_HV_RANGE4_START  <= MSR_GIM_HV_RANGE4_END);
AssertCompile(MSR_GIM_HV_RANGE5_START  <= MSR_GIM_HV_RANGE5_END);
AssertCompile(MSR_GIM_HV_RANGE6_START  <= MSR_GIM_HV_RANGE6_END);
AssertCompile(MSR_GIM_HV_RANGE7_START  <= MSR_GIM_HV_RANGE7_END);
AssertCompile(MSR_GIM_HV_RANGE8_START  <= MSR_GIM_HV_RANGE8_END);
AssertCompile(MSR_GIM_HV_RANGE9_START  <= MSR_GIM_HV_RANGE9_END);
AssertCompile(MSR_GIM_HV_RANGE10_START <= MSR_GIM_HV_RANGE10_END);
AssertCompile(MSR_GIM_HV_RANGE11_START <= MSR_GIM_HV_RANGE11_END);

/** @name Hyper-V MSR - Reset (MSR_GIM_HV_RESET).
 * @{
 */
/** The hypercall enable bit. */
#define MSR_GIM_HV_RESET_BIT                      RT_BIT_64(0)
/** Whether the hypercall-page is enabled or not. */
#define MSR_GIM_HV_RESET_IS_SET(a)                RT_BOOL((a) & MSR_GIM_HV_RESET_BIT)
/** @} */

/** @name Hyper-V MSR - Hypercall (MSR_GIM_HV_HYPERCALL).
 * @{
 */
/** Guest-physical page frame number of the hypercall-page. */
#define MSR_GIM_HV_HYPERCALL_GUEST_PFN(a)         ((a) >> 12)
/** The hypercall enable bit. */
#define MSR_GIM_HV_HYPERCALL_ENABLE_BIT           RT_BIT_64(0)
/** Whether the hypercall-page is enabled or not. */
#define MSR_GIM_HV_HYPERCALL_IS_ENABLED(a)        RT_BOOL((a) & MSR_GIM_HV_HYPERCALL_ENABLE_BIT)
/** @} */

/** @name Hyper-V MSR - Reference TSC (MSR_GIM_HV_REF_TSC).
 * @{
 */
/** Guest-physical page frame number of the TSC-page. */
#define MSR_GIM_HV_REF_TSC_GUEST_PFN(a)           ((a) >> 12)
/** The TSC-page enable bit. */
#define MSR_GIM_HV_REF_TSC_ENABLE_BIT             RT_BIT_64(0)
/** Whether the TSC-page is enabled or not. */
#define MSR_GIM_HV_REF_TSC_IS_ENABLED(a)          RT_BOOL((a) & MSR_GIM_HV_REF_TSC_ENABLE_BIT)
/** @} */

/** Hyper-V page size.  */
#define GIM_HV_PAGE_SIZE                          0x1000

/**
 * MMIO2 region indices.
 */
/** The hypercall page region. */
#define GIM_HV_HYPERCALL_PAGE_REGION_IDX          UINT8_C(0)
/** The TSC page region. */
#define GIM_HV_REF_TSC_PAGE_REGION_IDX            UINT8_C(1)
/** The maximum region index (must be <= UINT8_MAX). */
#define GIM_HV_REGION_IDX_MAX                     GIM_HV_REF_TSC_PAGE_REGION_IDX

/**
 * Hyper-V TSC (HV_REFERENCE_TSC_PAGE) structure placed in the TSC reference
 * page.
 */
typedef struct GIMHVREFTSC
{
    uint32_t volatile   u32TscSequence;
    uint32_t            uReserved0;
    uint64_t volatile   u64TscScale;
    int64_t  volatile   i64TscOffset;
} GIMHVTSCPAGE;
/** Pointer to Hyper-V reference TSC. */
typedef GIMHVREFTSC *PGIMHVREFTSC;
/** Pointer to a const Hyper-V reference TSC. */
typedef GIMHVREFTSC const *PCGIMHVREFTSC;


/**
 * GIM Hyper-V VM Instance data.
 * Changes to this must checked against the padding of the gim union in VM!
 */
typedef struct GIMHV
{
    /** Guest OS identity MSR. */
    uint64_t                    u64GuestOsIdMsr;
    /** Hypercall MSR. */
    uint64_t                    u64HypercallMsr;
    /** Reference TSC page MSR. */
    uint64_t                    u64TscPageMsr;

    /** Basic features. */
    uint32_t                    uBaseFeat;
    /** Partition flags. */
    uint32_t                    uPartFlags;
    /** Power management features. */
    uint32_t                    uPowMgmtFeat;
    /** Miscellaneous features. */
    uint32_t                    uMiscFeat;
    /** Hypervisor hints to the guest. */
    uint32_t                    uHyperHints;
    /** Hypervisor capabilities. */
    uint32_t                    uHyperCaps;

    /** Per-VM R0 Spinlock for protecting EMT writes to the TSC page. */
    RTSPINLOCK                  hSpinlockR0;
#if HC_ARCH_BITS == 32
    uint32_t                    u32Alignment1;
#endif

    /** Array of MMIO2 regions. */
    GIMMMIO2REGION              aMmio2Regions[GIM_HV_REGION_IDX_MAX + 1];
} GIMHV;
/** Pointer to per-VM GIM Hyper-V instance data. */
typedef GIMHV *PGIMHV;
/** Pointer to const per-VM GIM Hyper-V instance data. */
typedef GIMHV const *PCGIMHV;
AssertCompileMemberAlignment(GIMHV, aMmio2Regions, 8);
AssertCompileMemberAlignment(GIMHV, hSpinlockR0, sizeof(uintptr_t));

RT_C_DECLS_BEGIN

#ifdef IN_RING0
VMMR0_INT_DECL(int)             GIMR0HvInitVM(PVM pVM);
VMMR0_INT_DECL(int)             GIMR0HvTermVM(PVM pVM);
VMMR0_INT_DECL(int)             GIMR0HvUpdateParavirtTsc(PVM pVM, uint64_t u64Offset);
#endif /* IN_RING0 */

#ifdef IN_RING3
VMMR3_INT_DECL(int)             GIMR3HvInit(PVM pVM);
VMMR3_INT_DECL(int)             GIMR3HvInitCompleted(PVM pVM);
VMMR3_INT_DECL(int)             GIMR3HvTerm(PVM pVM);
VMMR3_INT_DECL(void)            GIMR3HvRelocate(PVM pVM, RTGCINTPTR offDelta);
VMMR3_INT_DECL(void)            GIMR3HvReset(PVM pVM);
VMMR3_INT_DECL(PGIMMMIO2REGION) GIMR3HvGetMmio2Regions(PVM pVM, uint32_t *pcRegions);
VMMR3_INT_DECL(int)             GIMR3HvSave(PVM pVM, PSSMHANDLE pSSM);
VMMR3_INT_DECL(int)             GIMR3HvLoad(PVM pVM, PSSMHANDLE pSSM, uint32_t uSSMVersion);

VMMR3_INT_DECL(int)             GIMR3HvDisableTscPage(PVM pVM);
VMMR3_INT_DECL(int)             GIMR3HvEnableTscPage(PVM pVM, RTGCPHYS GCPhysTscPage, bool fUseThisTscSequence, uint32_t uTscSequence);
VMMR3_INT_DECL(int)             GIMR3HvDisableHypercallPage(PVM pVM);
VMMR3_INT_DECL(int)             GIMR3HvEnableHypercallPage(PVM pVM, RTGCPHYS GCPhysHypercallPage);
#endif /* IN_RING3 */

VMM_INT_DECL(bool)              GIMHvIsParavirtTscEnabled(PVM pVM);
VMM_INT_DECL(bool)              GIMHvAreHypercallsEnabled(PVMCPU pVCpu);
VMM_INT_DECL(int)               GIMHvHypercall(PVMCPU pVCpu, PCPUMCTX pCtx);
VMM_INT_DECL(int)               GIMHvReadMsr(PVMCPU pVCpu, uint32_t idMsr, PCCPUMMSRRANGE pRange, uint64_t *puValue);
VMM_INT_DECL(int)               GIMHvWriteMsr(PVMCPU pVCpu, uint32_t idMsr, PCCPUMMSRRANGE pRange, uint64_t uRawValue);

RT_C_DECLS_END

#endif /* ___GIMHvInternal_h */

