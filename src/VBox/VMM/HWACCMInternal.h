/* $Id$ */
/** @file
 * HWACCM - Internal header file.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___HWACCMInternal_h
#define ___HWACCMInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/em.h>
#include <VBox/stam.h>
#include <VBox/dis.h>
#include <VBox/hwaccm.h>
#include <VBox/pgm.h>

__BEGIN_DECLS


/** @defgroup grp_hwaccm_int       Internal
 * @ingroup grp_hwaccm
 * @internal
 * @{
 */


/**
 * Converts a HWACCM pointer into a VM pointer.
 * @returns Pointer to the VM structure the EM is part of.
 * @param   pHWACCM   Pointer to HWACCM instance data.
 */
#define HWACCM2VM(pHWACCM)  ( (PVM)((char*)pHWACCM - pHWACCM->offVM) )

/** Maximum number of exit reason statistics counters. */
#define MAX_EXITREASON_STAT        0x100
#define MASK_EXITREASON_STAT       0xff

/** @name Changed flags
 * These flags are used to keep track of which important registers that
 * have been changed since last they were reset.
 * @{
 */
#define HWACCM_CHANGED_GUEST_FPU                BIT(0)
#define HWACCM_CHANGED_GUEST_CR0                BIT(1)
#define HWACCM_CHANGED_GUEST_CR3                BIT(2)
#define HWACCM_CHANGED_GUEST_CR4                BIT(3)
#define HWACCM_CHANGED_GUEST_GDTR               BIT(4)
#define HWACCM_CHANGED_GUEST_IDTR               BIT(5)
#define HWACCM_CHANGED_GUEST_LDTR               BIT(6)
#define HWACCM_CHANGED_GUEST_TR                 BIT(7)
#define HWACCM_CHANGED_GUEST_SYSENTER_MSR       BIT(8)
#define HWACCM_CHANGED_GUEST_SEGMENT_REGS       BIT(9)
#define HWACCM_CHANGED_GUEST_DEBUG              BIT(10)
#define HWACCM_CHANGED_HOST_CONTEXT             BIT(11)

#define HWACCM_CHANGED_ALL                  (   HWACCM_CHANGED_GUEST_SEGMENT_REGS \
                                            |   HWACCM_CHANGED_GUEST_CR0          \
                                            |   HWACCM_CHANGED_GUEST_CR3          \
                                            |   HWACCM_CHANGED_GUEST_CR4          \
                                            |   HWACCM_CHANGED_GUEST_GDTR         \
                                            |   HWACCM_CHANGED_GUEST_IDTR         \
                                            |   HWACCM_CHANGED_GUEST_LDTR         \
                                            |   HWACCM_CHANGED_GUEST_TR           \
                                            |   HWACCM_CHANGED_GUEST_SYSENTER_MSR \
                                            |   HWACCM_CHANGED_GUEST_FPU          \
                                            |   HWACCM_CHANGED_GUEST_DEBUG        \
                                            |   HWACCM_CHANGED_HOST_CONTEXT)

#define HWACCM_CHANGED_ALL_GUEST            (   HWACCM_CHANGED_GUEST_SEGMENT_REGS \
                                            |   HWACCM_CHANGED_GUEST_CR0          \
                                            |   HWACCM_CHANGED_GUEST_CR3          \
                                            |   HWACCM_CHANGED_GUEST_CR4          \
                                            |   HWACCM_CHANGED_GUEST_GDTR         \
                                            |   HWACCM_CHANGED_GUEST_IDTR         \
                                            |   HWACCM_CHANGED_GUEST_LDTR         \
                                            |   HWACCM_CHANGED_GUEST_TR           \
                                            |   HWACCM_CHANGED_GUEST_SYSENTER_MSR \
                                            |   HWACCM_CHANGED_GUEST_DEBUG        \
                                            |   HWACCM_CHANGED_GUEST_FPU)

/** @} */

/** @name Intercepted traps
 *  Traps that need to be intercepted so we can correctly dispatch them to the guest if required.
 *  Currently #NM and #PF only
 */
#ifdef VBOX_STRICT
#define HWACCM_VMX_TRAP_MASK                BIT(0) | BIT(7) | BIT(14) | BIT(6) | BIT(11) | BIT(12) | BIT(13) | BIT(16)
#define HWACCM_SVM_TRAP_MASK                HWACCM_VMX_TRAP_MASK
#else
#define HWACCM_VMX_TRAP_MASK                BIT(7) | BIT(14)
#define HWACCM_SVM_TRAP_MASK                HWACCM_VMX_TRAP_MASK
#endif
/** @} */


/** @name Maxium resume loops allowed in ring 0 (safety precaution) */
#define HWACCM_MAX_RESUME_LOOPS             1024

/** @name HWACCM SSM version
 */
#define HWACCM_SSM_VERSION                  3

/**
 * HWACCM VM Instance data.
 * Changes to this must checked against the padding of the cfgm union in VM!
 */
typedef struct HWACCM
{
    /** Offset to the VM structure.
     * See HWACCM2VM(). */
    RTUINT                      offVM;

    /** Set when we've initialized VMX or SVM. */
    bool                        fInitialized;
    /** Set when we're using VMX/SVN at that moment. */
    bool                        fActive;

    /** Set when hardware acceleration is allowed. */
    bool                        fAllowed;

    /** HWACCM_CHANGED_* flags. */
    uint32_t                    fContextUseFlags;

    /** Old style FPU reporting trap mask override performed (optimization) */
    uint32_t                    fFPUOldStyleOverride;

    struct
    {
        /** Set by the ring-0 driver to indicate VMX is supported by the CPU. */
        bool                        fSupported;

        /** Set when we've enabled VMX. */
        bool                        fEnabled;

        /** Set if we can use VMXResume to execute guest code. */
        bool                        fResumeVM;

        /** Physical address of the VM control structure (VMCS). */
        RTHCPHYS                    pVMCSPhys;
        /** Virtual address of the VM control structure (VMCS). */
        void                       *pVMCS;

        /** Physical address of the VMXON page. */
        RTHCPHYS                    pVMXONPhys;
        /** Virtual address of the VMXON page. */
        void                       *pVMXON;

        /** Physical address of the TSS page used for real mode emulation. */
        RTHCPHYS                    pRealModeTSSPhys;
        /** Virtual address of the TSS page used for real mode emulation. */
        PVBOXTSS                    pRealModeTSS;

        /** Host CR4 value (set by ring-0 VMX init) */
        uint32_t                    hostCR4;

        /** Current VMX_VMCS_CTRL_PROC_EXEC_CONTROLS. */
        uint64_t                    proc_ctls;

        /** Current CR0 mask. */
        uint64_t                    cr0_mask;
        /** Current CR4 mask. */
        uint64_t                    cr4_mask;

        /** VMX MSR values */
        struct
        {
            uint64_t                feature_ctrl;
            uint64_t                vmx_basic_info;
            uint64_t                vmx_pin_ctls;
            uint64_t                vmx_proc_ctls;
            uint64_t                vmx_exit;
            uint64_t                vmx_entry;
            uint64_t                vmx_misc;
            uint64_t                vmx_cr0_fixed0;
            uint64_t                vmx_cr0_fixed1;
            uint64_t                vmx_cr4_fixed0;
            uint64_t                vmx_cr4_fixed1;
            uint64_t                vmx_vmcs_enum;
        } msr;

        /* Last instruction error */
        uint32_t                    ulLastInstrError;
    } vmx;

    struct
    {
        /** Set by the ring-0 driver to indicate SVM is supported by the CPU. */
        bool                        fSupported;

        /** Set when we've enabled SVM. */
        bool                        fEnabled;

        /** Set if we don't have to flush the TLB on VM entry. */
        bool                        fResumeVM;

        /** Physical address of the VM control block (VMCB). */
        RTHCPHYS                    pVMCBPhys;
        /** Virtual address of the VM control block (VMCB). */
        void                       *pVMCB;

        /** Physical address of the host VM control block (VMCB). */
        RTHCPHYS                    pVMCBHostPhys;
        /** Virtual address of the host VM control block (VMCB). */
        void                       *pVMCBHost;

        /** Physical address of the Host State page. */
        RTHCPHYS                    pHStatePhys;
        /** Virtual address of the Host State page. */
        void                       *pHState;

        /** Physical address of the IO bitmap (12kb). */
        RTHCPHYS                    pIOBitmapPhys;
        /** Virtual address of the IO bitmap. */
        void                       *pIOBitmap;

        /** Physical address of the MSR bitmap (8kb). */
        RTHCPHYS                    pMSRBitmapPhys;
        /** Virtual address of the MSR bitmap. */
        void                       *pMSRBitmap;

        /** SVM revision. */
        uint32_t                    u32Rev;

        /** Maximum ASID allowed. */
        uint32_t                    u32MaxASID;
    } svm;

    struct
    {
        uint32_t                    u32AMDFeatureECX;
        uint32_t                    u32AMDFeatureEDX;
    } cpuid;

    /* Event injection state. */
    struct
    {
        uint32_t                    fPending;
        uint32_t                    errCode;
        uint64_t                    intInfo;
    } Event;

    /** Saved error from detection */
    int32_t                 lLastError;

    /** HWACCMR0Init was run */
    bool                    fHWACCMR0Init;

    /** Currenty shadow paging mode. */
    PGMMODE                 enmShadowMode;

    STAMPROFILEADV          StatEntry;
    STAMPROFILEADV          StatExit;
    STAMPROFILEADV          StatInGC;

    STAMCOUNTER             StatIntInject;

    STAMCOUNTER             StatExitShadowNM;
    STAMCOUNTER             StatExitGuestNM;
    STAMCOUNTER             StatExitShadowPF;
    STAMCOUNTER             StatExitGuestPF;
    STAMCOUNTER             StatExitGuestUD;
    STAMCOUNTER             StatExitGuestSS;
    STAMCOUNTER             StatExitGuestNP;
    STAMCOUNTER             StatExitGuestGP;
    STAMCOUNTER             StatExitGuestDE;
    STAMCOUNTER             StatExitGuestMF;
    STAMCOUNTER             StatExitInvpg;
    STAMCOUNTER             StatExitInvd;
    STAMCOUNTER             StatExitCpuid;
    STAMCOUNTER             StatExitRdtsc;
    STAMCOUNTER             StatExitCRxWrite;
    STAMCOUNTER             StatExitCRxRead;
    STAMCOUNTER             StatExitDRxWrite;
    STAMCOUNTER             StatExitDRxRead;
    STAMCOUNTER             StatExitCLTS;
    STAMCOUNTER             StatExitLMSW;
    STAMCOUNTER             StatExitIOWrite;
    STAMCOUNTER             StatExitIORead;
    STAMCOUNTER             StatExitIOStringWrite;
    STAMCOUNTER             StatExitIOStringRead;
    STAMCOUNTER             StatExitIrqWindow;
    STAMCOUNTER             StatExitMaxResume;
    STAMCOUNTER             StatIntReinject;
    STAMCOUNTER             StatPendingHostIrq;

    STAMCOUNTER             StatSwitchGuestIrq;
    STAMCOUNTER             StatSwitchToR3;

    R3PTRTYPE(PSTAMCOUNTER) pStatExitReason;
    R0PTRTYPE(PSTAMCOUNTER) pStatExitReasonR0;
} HWACCM;
/** Pointer to HWACCM VM instance data. */
typedef HWACCM *PHWACCM;


#ifdef IN_RING0

#ifdef VBOX_STRICT
HWACCMR0DECL(void) HWACCMDumpRegs(PCPUMCTX pCtx);
HWACCMR0DECL(void) HWACCMR0DumpDescriptor(PX86DESCHC  Desc, RTSEL Sel, const char *pszMsg);
#else
#define HWACCMDumpRegs(a)                   do { } while (0)
#define HWACCMR0DumpDescriptor(a, b, c)     do { } while (0)
#endif

#endif

/** @} */

__END_DECLS

#endif

