/* $Id$ */
/** @file
 * VMX - Internal header file for the VMX code template.
 */

/*
 * Copyright (C) 2006-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VMM_INCLUDED_SRC_include_VMXInternal_h
#define VMM_INCLUDED_SRC_include_VMXInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "HMVMXCommon.h"

#if HC_ARCH_BITS == 32
# error "32-bit hosts are no longer supported. Go back to 6.0 or earlier!"
#endif

/** @def HM_PROFILE_EXIT_DISPATCH
 * Enables profiling of the VM exit handler dispatching. */
#if 0 || defined(DOXYGEN_RUNNING)
# define HM_PROFILE_EXIT_DISPATCH
#endif

RT_C_DECLS_BEGIN


/** @defgroup grp_hm_int       Internal
 * @ingroup grp_hm
 * @internal
 * @{
 */

/** @addtogroup grp_hm_int_vmx  VMX Internal
 * @{ */
/**
 * VMX VMCS information, shared.
 *
 * This structure provides information maintained for and during the executing of a
 * guest (or nested-guest) VMCS (VM control structure) using hardware-assisted VMX.
 *
 * Note! The members here are ordered and aligned based on estimated frequency of
 * usage and grouped to fit within a cache line in hot code paths. Even subtle
 * changes here have a noticeable effect in the bootsector benchmarks. Modify with
 * care.
 */
typedef struct VMXVMCSINFOSHARED
{
    /** @name Real-mode emulation state.
     * @{ */
    /** Set if guest was executing in real mode (extra checks). */
    bool                        fWasInRealMode;
    /** Padding. */
    bool                        afPadding0[7];
    struct
    {
        X86DESCATTR             AttrCS;
        X86DESCATTR             AttrDS;
        X86DESCATTR             AttrES;
        X86DESCATTR             AttrFS;
        X86DESCATTR             AttrGS;
        X86DESCATTR             AttrSS;
        X86EFLAGS               Eflags;
        bool                    fRealOnV86Active;
        bool                    afPadding1[3];
    } RealMode;
    /** @} */

    /** @name LBR MSR data.
     *  @{ */
    /** List of LastBranch-From-IP MSRs. */
    uint64_t                    au64LbrFromIpMsr[32];
    /** List of LastBranch-To-IP MSRs. */
    uint64_t                    au64LbrToIpMsr[32];
    /** The MSR containing the index to the most recent branch record.  */
    uint64_t                    u64LbrTosMsr;
    /** @} */
} VMXVMCSINFOSHARED;
/** Pointer to a VMXVMCSINFOSHARED struct.  */
typedef VMXVMCSINFOSHARED *PVMXVMCSINFOSHARED;
/** Pointer to a const VMXVMCSINFOSHARED struct. */
typedef const VMXVMCSINFOSHARED *PCVMXVMCSINFOSHARED;
AssertCompileSizeAlignment(VMXVMCSINFOSHARED, 8);


/**
 * VMX VMCS information, ring-0 only.
 *
 * This structure provides information maintained for and during the executing of a
 * guest (or nested-guest) VMCS (VM control structure) using hardware-assisted VMX.
 *
 * Note! The members here are ordered and aligned based on estimated frequency of
 * usage and grouped to fit within a cache line in hot code paths. Even subtle
 * changes here have a noticeable effect in the bootsector benchmarks. Modify with
 * care.
 */
typedef struct VMXVMCSINFO
{
    /** Pointer to the bits we share with ring-3. */
    R3R0PTRTYPE(PVMXVMCSINFOSHARED) pShared;

    /** @name Auxiliary information.
     * @{ */
    /** Host-physical address of the EPTP. */
    RTHCPHYS                    HCPhysEPTP;
    /** The VMCS launch state, see VMX_V_VMCS_LAUNCH_STATE_XXX. */
    uint32_t                    fVmcsState;
    /** The VMCS launch state of the shadow VMCS, see VMX_V_VMCS_LAUNCH_STATE_XXX. */
    uint32_t                    fShadowVmcsState;
    /** The host CPU for which its state has been exported to this VMCS. */
    RTCPUID                     idHostCpuState;
    /** The host CPU on which we last executed this VMCS. */
    RTCPUID                     idHostCpuExec;
    /** Number of guest MSRs in the VM-entry MSR-load area. */
    uint32_t                    cEntryMsrLoad;
    /** Number of guest MSRs in the VM-exit MSR-store area. */
    uint32_t                    cExitMsrStore;
    /** Number of host MSRs in the VM-exit MSR-load area. */
    uint32_t                    cExitMsrLoad;
    /** @} */

    /** @name Cache of execution related VMCS fields.
     *  @{ */
    /** Pin-based VM-execution controls. */
    uint32_t                    u32PinCtls;
    /** Processor-based VM-execution controls. */
    uint32_t                    u32ProcCtls;
    /** Secondary processor-based VM-execution controls. */
    uint32_t                    u32ProcCtls2;
    /** Tertiary processor-based VM-execution controls. */
    uint64_t                    u64ProcCtls3;
    /** VM-entry controls. */
    uint32_t                    u32EntryCtls;
    /** VM-exit controls. */
    uint32_t                    u32ExitCtls;
    /** Exception bitmap. */
    uint32_t                    u32XcptBitmap;
    /** Page-fault exception error-code mask. */
    uint32_t                    u32XcptPFMask;
    /** Page-fault exception error-code match. */
    uint32_t                    u32XcptPFMatch;
    /** Padding. */
    uint32_t                    u32Alignment0;
    /** TSC offset. */
    uint64_t                    u64TscOffset;
    /** VMCS link pointer. */
    uint64_t                    u64VmcsLinkPtr;
    /** CR0 guest/host mask. */
    uint64_t                    u64Cr0Mask;
    /** CR4 guest/host mask. */
    uint64_t                    u64Cr4Mask;
#ifdef IN_RING0
    /** Current VMX_VMCS_HOST_RIP value (only used in HMR0A.asm). */
    uint64_t                    uHostRip;
    /** Current VMX_VMCS_HOST_RSP value (only used in HMR0A.asm). */
    uint64_t                    uHostRsp;
#endif
    /** @} */

    /** @name Host-virtual address of VMCS and related data structures.
     *  @{ */
    /** The VMCS. */
    R3R0PTRTYPE(void *)         pvVmcs;
    /** The shadow VMCS. */
    R3R0PTRTYPE(void *)         pvShadowVmcs;
    /** The virtual-APIC page. */
    R3R0PTRTYPE(uint8_t *)      pbVirtApic;
    /** The MSR bitmap. */
    R3R0PTRTYPE(void *)         pvMsrBitmap;
    /** The VM-entry MSR-load area. */
    R3R0PTRTYPE(void *)         pvGuestMsrLoad;
    /** The VM-exit MSR-store area. */
    R3R0PTRTYPE(void *)         pvGuestMsrStore;
    /** The VM-exit MSR-load area. */
    R3R0PTRTYPE(void *)         pvHostMsrLoad;
    /** @} */

#ifdef IN_RING0
    /** @name Host-physical address of VMCS and related data structures.
     *  @{ */
    /** The VMCS. */
    RTHCPHYS                    HCPhysVmcs;
    /** The shadow VMCS. */
    RTHCPHYS                    HCPhysShadowVmcs;
    /** The virtual APIC page. */
    RTHCPHYS                    HCPhysVirtApic;
    /** The MSR bitmap. */
    RTHCPHYS                    HCPhysMsrBitmap;
    /** The VM-entry MSR-load area. */
    RTHCPHYS                    HCPhysGuestMsrLoad;
    /** The VM-exit MSR-store area. */
    RTHCPHYS                    HCPhysGuestMsrStore;
    /** The VM-exit MSR-load area. */
    RTHCPHYS                    HCPhysHostMsrLoad;
    /** @} */

    /** @name R0-memory objects address for VMCS and related data structures.
     *  @{ */
    /** R0-memory object for VMCS and related data structures. */
    RTR0MEMOBJ                  hMemObj;
    /** @} */
#endif
} VMXVMCSINFO;
/** Pointer to a VMXVMCSINFOR0 struct.  */
typedef VMXVMCSINFO *PVMXVMCSINFO;
/** Pointer to a const VMXVMCSINFO struct. */
typedef const VMXVMCSINFO *PCVMXVMCSINFO;
AssertCompileSizeAlignment(VMXVMCSINFO, 8);
AssertCompileMemberAlignment(VMXVMCSINFO, u32PinCtls,      4);
AssertCompileMemberAlignment(VMXVMCSINFO, u64VmcsLinkPtr,  8);
AssertCompileMemberAlignment(VMXVMCSINFO, pvVmcs,          8);
AssertCompileMemberAlignment(VMXVMCSINFO, pvShadowVmcs,    8);
AssertCompileMemberAlignment(VMXVMCSINFO, pbVirtApic,      8);
AssertCompileMemberAlignment(VMXVMCSINFO, pvMsrBitmap,     8);
AssertCompileMemberAlignment(VMXVMCSINFO, pvGuestMsrLoad,  8);
AssertCompileMemberAlignment(VMXVMCSINFO, pvGuestMsrStore, 8);
AssertCompileMemberAlignment(VMXVMCSINFO, pvHostMsrLoad,   8);
#ifdef IN_RING0
AssertCompileMemberAlignment(VMXVMCSINFO, HCPhysVmcs,      8);
AssertCompileMemberAlignment(VMXVMCSINFO, hMemObj,         8);
#endif


/**
 * VMX per-VCPU transient state.
 *
 * A state structure for holding miscellaneous information across
 * VMX non-root operation and restored after the transition.
 *
 * Note: The members are ordered and aligned such that the most
 * frequently used ones (in the guest execution loop) fall within
 * the first cache line.
 */
typedef struct VMXTRANSIENT
{
   /** Mask of currently read VMCS fields; HMVMX_READ_XXX. */
   uint32_t            fVmcsFieldsRead;
   /** The guest's TPR value used for TPR shadowing. */
   uint8_t             u8GuestTpr;
   uint8_t             abAlignment0[3];

   /** Whether the VM-exit was caused by a page-fault during delivery of an
    *  external interrupt or NMI. */
   bool                fVectoringPF;
   /** Whether the VM-exit was caused by a page-fault during delivery of a
    *  contributory exception or a page-fault. */
   bool                fVectoringDoublePF;
   /** Whether the VM-entry failed or not. */
   bool                fVMEntryFailed;
   /** Whether the TSC_AUX MSR needs to be removed from the auto-load/store MSR
    *  area after VM-exit. */
   bool                fRemoveTscAuxMsr;
   /** Whether TSC-offsetting and VMX-preemption timer was updated before VM-entry. */
   bool                fUpdatedTscOffsettingAndPreemptTimer;
   /** Whether we are currently executing a nested-guest. */
   bool                fIsNestedGuest;
   /** Whether the guest debug state was active at the time of VM-exit. */
   bool                fWasGuestDebugStateActive;
   /** Whether the hyper debug state was active at the time of VM-exit. */
   bool                fWasHyperDebugStateActive;

   /** The basic VM-exit reason. */
   uint32_t            uExitReason;
   /** The VM-exit interruption error code. */
   uint32_t            uExitIntErrorCode;

   /** The host's rflags/eflags. */
   RTCCUINTREG         fEFlags;

   /** The VM-exit exit code qualification. */
   uint64_t            uExitQual;

   /** The VMCS info. object. */
   PVMXVMCSINFO         pVmcsInfo;

   /** The VM-exit interruption-information field. */
   uint32_t            uExitIntInfo;
   /** The VM-exit instruction-length field. */
   uint32_t            cbExitInstr;

   /** The VM-exit instruction-information field. */
   VMXEXITINSTRINFO    ExitInstrInfo;
   /** IDT-vectoring information field. */
   uint32_t            uIdtVectoringInfo;

   /** IDT-vectoring error code. */
   uint32_t            uIdtVectoringErrorCode;
   uint32_t            u32Alignment0;

   /** The Guest-linear address. */
   uint64_t            uGuestLinearAddr;

   /** The Guest-physical address. */
   uint64_t            uGuestPhysicalAddr;

   /** The Guest pending-debug exceptions. */
   uint64_t            uGuestPendingDbgXcpts;

   /** The VM-entry interruption-information field. */
   uint32_t            uEntryIntInfo;
   /** The VM-entry exception error code field. */
   uint32_t            uEntryXcptErrorCode;

   /** The VM-entry instruction length field. */
   uint32_t            cbEntryInstr;
} VMXTRANSIENT;
AssertCompileMemberSize(VMXTRANSIENT, ExitInstrInfo, sizeof(uint32_t));
AssertCompileMemberAlignment(VMXTRANSIENT, fVmcsFieldsRead,        8);
AssertCompileMemberAlignment(VMXTRANSIENT, fVectoringPF,           8);
AssertCompileMemberAlignment(VMXTRANSIENT, uExitReason,            8);
AssertCompileMemberAlignment(VMXTRANSIENT, fEFlags,                8);
AssertCompileMemberAlignment(VMXTRANSIENT, uExitQual,              8);
AssertCompileMemberAlignment(VMXTRANSIENT, pVmcsInfo,              8);
AssertCompileMemberAlignment(VMXTRANSIENT, uExitIntInfo,           8);
AssertCompileMemberAlignment(VMXTRANSIENT, ExitInstrInfo,          8);
AssertCompileMemberAlignment(VMXTRANSIENT, uIdtVectoringErrorCode, 8);
AssertCompileMemberAlignment(VMXTRANSIENT, uGuestLinearAddr,       8);
AssertCompileMemberAlignment(VMXTRANSIENT, uGuestPhysicalAddr,     8);
AssertCompileMemberAlignment(VMXTRANSIENT, uEntryIntInfo,          8);
AssertCompileMemberAlignment(VMXTRANSIENT, cbEntryInstr,           8);
/** Pointer to VMX transient state. */
typedef VMXTRANSIENT *PVMXTRANSIENT;
/** Pointer to a const VMX transient state. */
typedef const VMXTRANSIENT *PCVMXTRANSIENT;


/**
 * VMX statistics structure.
 */
typedef struct VMXSTATISTICS
{
    /* These two comes because they are accessed from assembly and we don't
       want to detail all the stats in the assembly version of this structure. */
    STAMCOUNTER             StatVmxWriteHostRip;
    STAMCOUNTER             StatVmxWriteHostRsp;
    STAMCOUNTER             StatVmxVmLaunch;
    STAMCOUNTER             StatVmxVmResume;

    STAMPROFILEADV          StatEntry;
    STAMPROFILEADV          StatPreExit;
    STAMPROFILEADV          StatExitHandling;
    STAMPROFILEADV          StatExitIO;
    STAMPROFILEADV          StatExitMovCRx;
    STAMPROFILEADV          StatExitXcptNmi;
    STAMPROFILEADV          StatExitVmentry;
    STAMPROFILEADV          StatImportGuestState;
    STAMPROFILEADV          StatExportGuestState;
    STAMPROFILEADV          StatLoadGuestFpuState;
    STAMPROFILEADV          StatInGC;
    STAMPROFILEADV          StatPoke;
    STAMPROFILEADV          StatSpinPoke;
    STAMPROFILEADV          StatSpinPokeFailed;

    STAMCOUNTER             StatInjectInterrupt;
    STAMCOUNTER             StatInjectXcpt;
    STAMCOUNTER             StatInjectReflect;
    STAMCOUNTER             StatInjectConvertDF;
    STAMCOUNTER             StatInjectInterpret;
    STAMCOUNTER             StatInjectReflectNPF;

    STAMCOUNTER             StatExitAll;
    STAMCOUNTER             StatNestedExitAll;
    STAMCOUNTER             StatExitShadowNM;
    STAMCOUNTER             StatExitGuestNM;
    STAMCOUNTER             StatExitShadowPF;       /**< Misleading, currently used for MMIO \#PFs as well. */
    STAMCOUNTER             StatExitShadowPFEM;
    STAMCOUNTER             StatExitGuestPF;
    STAMCOUNTER             StatExitGuestUD;
    STAMCOUNTER             StatExitGuestSS;
    STAMCOUNTER             StatExitGuestNP;
    STAMCOUNTER             StatExitGuestTS;
    STAMCOUNTER             StatExitGuestOF;
    STAMCOUNTER             StatExitGuestGP;
    STAMCOUNTER             StatExitGuestDE;
    STAMCOUNTER             StatExitGuestDF;
    STAMCOUNTER             StatExitGuestBR;
    STAMCOUNTER             StatExitGuestAC;
    STAMCOUNTER             StatExitGuestACSplitLock;
    STAMCOUNTER             StatExitGuestDB;
    STAMCOUNTER             StatExitGuestMF;
    STAMCOUNTER             StatExitGuestBP;
    STAMCOUNTER             StatExitGuestXF;
    STAMCOUNTER             StatExitGuestXcpUnk;
    STAMCOUNTER             StatExitDRxWrite;
    STAMCOUNTER             StatExitDRxRead;
    STAMCOUNTER             StatExitCR0Read;
    STAMCOUNTER             StatExitCR2Read;
    STAMCOUNTER             StatExitCR3Read;
    STAMCOUNTER             StatExitCR4Read;
    STAMCOUNTER             StatExitCR8Read;
    STAMCOUNTER             StatExitCR0Write;
    STAMCOUNTER             StatExitCR2Write;
    STAMCOUNTER             StatExitCR3Write;
    STAMCOUNTER             StatExitCR4Write;
    STAMCOUNTER             StatExitCR8Write;
    STAMCOUNTER             StatExitRdmsr;
    STAMCOUNTER             StatExitWrmsr;
    STAMCOUNTER             StatExitClts;
    STAMCOUNTER             StatExitXdtrAccess;
    STAMCOUNTER             StatExitLmsw;
    STAMCOUNTER             StatExitIOWrite;
    STAMCOUNTER             StatExitIORead;
    STAMCOUNTER             StatExitIOStringWrite;
    STAMCOUNTER             StatExitIOStringRead;
    STAMCOUNTER             StatExitIntWindow;
    STAMCOUNTER             StatExitExtInt;
    STAMCOUNTER             StatExitHostNmiInGC;
    STAMCOUNTER             StatExitHostNmiInGCIpi;
    STAMCOUNTER             StatExitPreemptTimer;
    STAMCOUNTER             StatExitTprBelowThreshold;
    STAMCOUNTER             StatExitTaskSwitch;
    STAMCOUNTER             StatExitApicAccess;
    STAMCOUNTER             StatExitReasonNpf;

    STAMCOUNTER             StatNestedExitReasonNpf;

    STAMCOUNTER             StatFlushPage;
    STAMCOUNTER             StatFlushPageManual;
    STAMCOUNTER             StatFlushPhysPageManual;
    STAMCOUNTER             StatFlushTlb;
    STAMCOUNTER             StatFlushTlbNstGst;
    STAMCOUNTER             StatFlushTlbManual;
    STAMCOUNTER             StatFlushTlbWorldSwitch;
    STAMCOUNTER             StatNoFlushTlbWorldSwitch;
    STAMCOUNTER             StatFlushEntire;
    STAMCOUNTER             StatFlushAsid;
    STAMCOUNTER             StatFlushNestedPaging;
    STAMCOUNTER             StatFlushTlbInvlpgVirt;
    STAMCOUNTER             StatFlushTlbInvlpgPhys;
    STAMCOUNTER             StatTlbShootdown;
    STAMCOUNTER             StatTlbShootdownFlush;

    STAMCOUNTER             StatSwitchPendingHostIrq;
    STAMCOUNTER             StatSwitchTprMaskedIrq;
    STAMCOUNTER             StatSwitchGuestIrq;
    STAMCOUNTER             StatSwitchHmToR3FF;
    STAMCOUNTER             StatSwitchVmReq;
    STAMCOUNTER             StatSwitchPgmPoolFlush;
    STAMCOUNTER             StatSwitchDma;
    STAMCOUNTER             StatSwitchExitToR3;
    STAMCOUNTER             StatSwitchLongJmpToR3;
    STAMCOUNTER             StatSwitchMaxResumeLoops;
    STAMCOUNTER             StatSwitchHltToR3;
    STAMCOUNTER             StatSwitchApicAccessToR3;
    STAMCOUNTER             StatSwitchPreempt;
    STAMCOUNTER             StatSwitchNstGstVmexit;

    STAMCOUNTER             StatTscParavirt;
    STAMCOUNTER             StatTscOffset;
    STAMCOUNTER             StatTscIntercept;

    STAMCOUNTER             StatDRxArmed;
    STAMCOUNTER             StatDRxContextSwitch;
    STAMCOUNTER             StatDRxIoCheck;

    STAMCOUNTER             StatExportMinimal;
    STAMCOUNTER             StatExportFull;
    STAMCOUNTER             StatLoadGuestFpu;
    STAMCOUNTER             StatExportHostState;

    STAMCOUNTER             StatVmxCheckBadRmSelBase;
    STAMCOUNTER             StatVmxCheckBadRmSelLimit;
    STAMCOUNTER             StatVmxCheckBadRmSelAttr;
    STAMCOUNTER             StatVmxCheckBadV86SelBase;
    STAMCOUNTER             StatVmxCheckBadV86SelLimit;
    STAMCOUNTER             StatVmxCheckBadV86SelAttr;
    STAMCOUNTER             StatVmxCheckRmOk;
    STAMCOUNTER             StatVmxCheckBadSel;
    STAMCOUNTER             StatVmxCheckBadRpl;
    STAMCOUNTER             StatVmxCheckPmOk;

    STAMCOUNTER             StatVmxPreemptionRecalcingDeadline;
    STAMCOUNTER             StatVmxPreemptionRecalcingDeadlineExpired;
    STAMCOUNTER             StatVmxPreemptionReusingDeadline;
    STAMCOUNTER             StatVmxPreemptionReusingDeadlineExpired;

#ifdef VBOX_WITH_STATISTICS
    STAMCOUNTER             aStatExitReason[MAX_EXITREASON_STAT];
    STAMCOUNTER             aStatNestedExitReason[MAX_EXITREASON_STAT];
    STAMCOUNTER             aStatInjectedIrqs[256];
    STAMCOUNTER             aStatInjectedXcpts[X86_XCPT_LAST + 1];
#endif
#ifdef HM_PROFILE_EXIT_DISPATCH
    STAMPROFILEADV          StatExitDispatch;
#endif
} VMXSTATISTICS;
/** Pointer to the VMX statistics. */
typedef VMXSTATISTICS *PVMXSTATISTICS;
/** Pointer to a const VMX statistics structure. */
typedef const VMXSTATISTICS *PCVMXSTATISTICS;

/** @} */

/** @} */

RT_C_DECLS_END

#endif /* !VMM_INCLUDED_SRC_include_VMXInternal_h */

