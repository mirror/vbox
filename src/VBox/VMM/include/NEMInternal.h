/* $Id$ */
/** @file
 * NEM - Internal header file.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___NEMInternal_h
#define ___NEMInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/vmm/nem.h>
#include <VBox/vmm/cpum.h> /* For CPUMCPUVENDOR. */
#include <VBox/vmm/stam.h>
#include <VBox/vmm/vmapi.h>
#ifdef RT_OS_WINDOWS
#include <iprt/nt/hyperv.h>
#include <iprt/critsect.h>
#endif

RT_C_DECLS_BEGIN


/** @defgroup grp_nem_int      Internal
 * @ingroup grp_nem
 * @internal
 * @{
 */


#ifdef RT_OS_WINDOWS
/*
 * Windows: Code configuration.
 */
# define NEM_WIN_USE_HYPERCALLS_FOR_PAGES
//# define NEM_WIN_USE_HYPERCALLS_FOR_REGISTERS
//# define NEM_WIN_USE_OUR_OWN_RUN_API
# if defined(NEM_WIN_USE_OUR_OWN_RUN_API) && !defined(NEM_WIN_USE_HYPERCALLS_FOR_REGISTERS)
#  error "NEM_WIN_USE_OUR_OWN_RUN_API requires NEM_WIN_USE_HYPERCALLS_FOR_REGISTERS"
# endif
# if defined(NEM_WIN_USE_OUR_OWN_RUN_API) && !defined(NEM_WIN_USE_HYPERCALLS_FOR_PAGES)
#  error "NEM_WIN_USE_OUR_OWN_RUN_API requires NEM_WIN_USE_HYPERCALLS_FOR_PAGES"
# endif

/**
 * Windows VID I/O control information.
 */
typedef struct NEMWINIOCTL
{
    /** The I/O control function number. */
    uint32_t    uFunction;
    uint32_t    cbInput;
    uint32_t    cbOutput;
} NEMWINIOCTL;

/** @name Windows: Our two-bit physical page state for PGMPAGE
 * @{ */
# define NEM_WIN_PAGE_STATE_NOT_SET     0
# define NEM_WIN_PAGE_STATE_UNMAPPED    1
# define NEM_WIN_PAGE_STATE_READABLE    2
# define NEM_WIN_PAGE_STATE_WRITABLE    3
/** @} */

/** Windows: Checks if a_GCPhys is subject to the limited A20 gate emulation. */
# define NEM_WIN_IS_SUBJECT_TO_A20(a_GCPhys)    ((RTGCPHYS)((a_GCPhys) - _1M) < (RTGCPHYS)_64K)
/** Windows: Checks if a_GCPhys is relevant to the limited A20 gate emulation. */
# define NEM_WIN_IS_RELEVANT_TO_A20(a_GCPhys)    \
    ( ((RTGCPHYS)((a_GCPhys) - _1M) < (RTGCPHYS)_64K) || ((RTGCPHYS)(a_GCPhys) < (RTGCPHYS)_64K) )

/** The CPUMCTX_EXTRN_XXX mask for IEM. */
# define NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM      (  IEM_CPUMCTX_EXTRN_MUST_MASK | CPUMCTX_EXTRN_NEM_WIN_INHIBIT_INT \
                                                  | CPUMCTX_EXTRN_NEM_WIN_INHIBIT_NMI )
/** The CPUMCTX_EXTRN_XXX mask for IEM when raising exceptions. */
# define NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM_XCPT (IEM_CPUMCTX_EXTRN_XCPT_MASK | NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM)

/** @name Windows: Interrupt window flags (NEM_WIN_INTW_F_XXX).
 * @{  */
# define NEM_WIN_INTW_F_NMI             UINT8_C(0x01)
# define NEM_WIN_INTW_F_REGULAR         UINT8_C(0x02)
# define NEM_WIN_INTW_F_PRIO_MASK       UINT8_C(0x3c)
# define NEM_WIN_INTW_F_PRIO_SHIFT      2
/** @} */

#endif /* RT_OS_WINDOWS */


/** Trick to make slickedit see the static functions in the template. */
#ifndef IN_SLICKEDIT
# define NEM_TMPL_STATIC static
#else
# define NEM_TMPL_STATIC
#endif


/**
 * Generic NEM exit type enumeration for use with EMHistoryAddExit.
 *
 * On windows we've got two different set of exit types and they are both jumping
 * around the place value wise, so EM can use their values.
 *
 * @note We only have exit types for exits not covered by EM here.
 */
typedef enum NEMEXITTYPE
{
    /* windows: */
    NEMEXITTYPE_UNRECOVERABLE_EXCEPTION = 1,
    NEMEXITTYPE_INVALID_VP_REGISTER_VALUE,
    NEMEXITTYPE_INTTERRUPT_WINDOW,
    NEMEXITTYPE_HALT,
    NEMEXITTYPE_XCPT_UD,
    NEMEXITTYPE_XCPT_DB,
    NEMEXITTYPE_XCPT_BP,
    NEMEXITTYPE_CANCELED
} NEMEXITTYPE;


/**
 * NEM VM Instance data.
 */
typedef struct NEM
{
    /** NEM_MAGIC. */
    uint32_t                    u32Magic;

    /** Set if enabled. */
    bool                        fEnabled;
    /** Set if long mode guests are allowed. */
    bool                        fAllow64BitGuests;
#ifdef RT_OS_WINDOWS
    /** Set if we've created the EMTs. */
    bool                        fCreatedEmts : 1;
    /** WHvRunVpExitReasonX64Cpuid is supported. */
    bool                        fExtendedMsrExit : 1;
    /** WHvRunVpExitReasonX64MsrAccess is supported. */
    bool                        fExtendedCpuIdExit : 1;
    /** WHvRunVpExitReasonException is supported. */
    bool                        fExtendedXcptExit : 1;
    /** Set if we've started more than one CPU and cannot mess with A20. */
    bool                        fA20Fixed : 1;
    /** Set if A20 is enabled. */
    bool                        fA20Enabled : 1;
    /** The reported CPU vendor.   */
    CPUMCPUVENDOR               enmCpuVendor;
    /** Cache line flush size as a power of two. */
    uint8_t                     cCacheLineFlushShift;
    /** The result of WHvCapabilityCodeProcessorFeatures. */
    union
    {
        /** 64-bit view. */
        uint64_t                u64;
# ifdef _WINHVAPIDEFS_H_
        /** Interpreed features. */
        WHV_PROCESSOR_FEATURES  u;
# endif
    } uCpuFeatures;

    /** The partition handle. */
# ifdef _WINHVAPIDEFS_H_
    WHV_PARTITION_HANDLE
# else
    RTHCUINTPTR
# endif
                                hPartition;
    /** The device handle for the partition, for use with Vid APIs or direct I/O
     * controls. */
    RTR3PTR                     hPartitionDevice;
    /** The Hyper-V partition ID.   */
    uint64_t                    idHvPartition;

    /** Number of currently mapped pages. */
    uint32_t volatile           cMappedPages;

    /** Info about the VidGetHvPartitionId I/O control interface. */
    NEMWINIOCTL                 IoCtlGetHvPartitionId;
    /** Info about the VidStartVirtualProcessor I/O control interface. */
    NEMWINIOCTL                 IoCtlStartVirtualProcessor;
    /** Info about the VidStopVirtualProcessor I/O control interface. */
    NEMWINIOCTL                 IoCtlStopVirtualProcessor;
    /** Info about the VidStopVirtualProcessor I/O control interface. */
    NEMWINIOCTL                 IoCtlMessageSlotHandleAndGetNext;

    /** Statistics updated by NEMR0UpdateStatistics. */
    struct
    {
        uint64_t                cPagesAvailable;
        uint64_t                cPagesInUse;
    } R0Stats;
#endif /* RT_OS_WINDOWS */
} NEM;
/** Pointer to NEM VM instance data. */
typedef NEM *PNEM;

/** NEM::u32Magic value. */
#define NEM_MAGIC               UINT32_C(0x004d454e)
/** NEM::u32Magic value after termination. */
#define NEM_MAGIC_DEAD          UINT32_C(0xdead1111)


/**
 * NEM VMCPU Instance data.
 */
typedef struct NEMCPU
{
    /** NEMCPU_MAGIC. */
    uint32_t                    u32Magic;
    /** Whether \#UD needs to be intercepted and presented to GIM. */
    bool                        fGIMTrapXcptUD : 1;
#ifdef RT_OS_WINDOWS
    /** The current state of the interrupt windows (NEM_WIN_INTW_F_XXX). */
    uint8_t                     fCurrentInterruptWindows;
    /** The desired state of the interrupt windows (NEM_WIN_INTW_F_XXX). */
    uint8_t                     fDesiredInterruptWindows;
    /** Last copy of HV_X64_VP_EXECUTION_STATE::InterruptShadow. */
    bool                        fLastInterruptShadow : 1;
    /** Pending APIC base value.
     * This is set to UINT64_MAX when not pending  */
    uint64_t                    uPendingApicBase;
# ifdef NEM_WIN_USE_OUR_OWN_RUN_API
    /** Pending VINF_NEM_CHANGE_PGM_MODE, VINF_NEM_FLUSH_TLB or VINF_NEM_UPDATE_APIC_BASE. */
    int32_t                     rcPending;
    /** The VID_MSHAGN_F_XXX flags.
     * Either VID_MSHAGN_F_HANDLE_MESSAGE | VID_MSHAGN_F_GET_NEXT_MESSAGE or zero. */
    uint32_t                    fHandleAndGetFlags;
    /** What VidMessageSlotMap returns and is used for passing exit info. */
    RTR3PTR                     pvMsgSlotMapping;
# endif
    /** The windows thread handle. */
    RTR3PTR                     hNativeThreadHandle;
    /** Parameters for making Hyper-V hypercalls. */
    union
    {
        uint8_t                 ab[64];
        /** Arguments for NEMR0MapPages (HvCallMapGpaPages). */
        struct
        {
            RTGCPHYS            GCPhysSrc;
            RTGCPHYS            GCPhysDst; /**< Same as GCPhysSrc except maybe when the A20 gate is disabled. */
            uint32_t            cPages;
            HV_MAP_GPA_FLAGS    fFlags;
        }                       MapPages;
        /** Arguments for NEMR0UnmapPages (HvCallUnmapGpaPages). */
        struct
        {
            RTGCPHYS            GCPhys;
            uint32_t            cPages;
        }                       UnmapPages;
        /** Result from NEMR0QueryCpuTick. */
        struct
        {
            uint64_t            cTicks;
            uint32_t            uAux;
        }                       QueryCpuTick;
        /** Input and output for NEMR0DoExperiment. */
        struct
        {
            uint32_t            uItem;
            bool                fSuccess;
            uint64_t            uStatus;
            uint64_t            uLoValue;
            uint64_t            uHiValue;
        }                       Experiment;
    } Hypercall;
    /** I/O control buffer, we always use this for I/O controls. */
    union
    {
        uint8_t                 ab[64];
        HV_PARTITION_ID         idPartition;
        HV_VP_INDEX             idCpu;
# ifdef VID_MSHAGN_F_GET_NEXT_MESSAGE
        VID_IOCTL_INPUT_MESSAGE_SLOT_HANDLE_AND_GET_NEXT MsgSlotHandleAndGetNext;
# endif
    } uIoCtlBuf;

    /** @name Statistics
     * @{ */
    STAMCOUNTER                 StatExitPortIo;
    STAMCOUNTER                 StatExitMemUnmapped;
    STAMCOUNTER                 StatExitMemIntercept;
    STAMCOUNTER                 StatExitHalt;
    STAMCOUNTER                 StatExitInterruptWindow;
    STAMCOUNTER                 StatExitCpuId;
    STAMCOUNTER                 StatExitMsr;
    STAMCOUNTER                 StatExitException;
    STAMCOUNTER                 StatExitExceptionBp;
    STAMCOUNTER                 StatExitExceptionDb;
    STAMCOUNTER                 StatExitExceptionUd;
    STAMCOUNTER                 StatExitExceptionUdHandled;
    STAMCOUNTER                 StatExitUnrecoverable;
    STAMCOUNTER                 StatGetMsgTimeout;
    STAMCOUNTER                 StatStopCpuSuccess;
    STAMCOUNTER                 StatStopCpuPending;
    STAMCOUNTER                 StatStopCpuPendingOdd;
    STAMCOUNTER                 StatCancelChangedState;
    STAMCOUNTER                 StatCancelAlertedThread;
    STAMCOUNTER                 StatBreakOnCancel;
    STAMCOUNTER                 StatBreakOnFFPre;
    STAMCOUNTER                 StatBreakOnFFPost;
    STAMCOUNTER                 StatBreakOnStatus;
    STAMCOUNTER                 StatImportOnDemand;
    STAMCOUNTER                 StatImportOnReturn;
    STAMCOUNTER                 StatImportOnReturnSkipped;
    STAMCOUNTER                 StatQueryCpuTick;
    /** @} */
#endif /* RT_OS_WINDOWS */
} NEMCPU;
/** Pointer to NEM VMCPU instance data. */
typedef NEMCPU *PNEMCPU;

/** NEMCPU::u32Magic value. */
#define NEMCPU_MAGIC            UINT32_C(0x4d454e20)
/** NEMCPU::u32Magic value after termination. */
#define NEMCPU_MAGIC_DEAD       UINT32_C(0xdead2222)


#ifdef IN_RING0
# ifdef RT_OS_WINDOWS
/**
 * Windows: Hypercall input/ouput page info.
 */
typedef struct NEMR0HYPERCALLDATA
{
    /** Host physical address of the hypercall input/output page. */
    RTHCPHYS                    HCPhysPage;
    /** Pointer to the hypercall input/output page. */
    uint8_t                    *pbPage;
    /** Handle to the memory object of the hypercall input/output page. */
    RTR0MEMOBJ                  hMemObj;
} NEMR0HYPERCALLDATA;
/** Pointer to a Windows hypercall input/output page info. */
typedef NEMR0HYPERCALLDATA *PNEMR0HYPERCALLDATA;
# endif /* RT_OS_WINDOWS */

/**
 * NEM GVMCPU instance data.
 */
typedef struct NEMR0PERVCPU
{
# ifdef RT_OS_WINDOWS
    /** Hypercall input/ouput page. */
    NEMR0HYPERCALLDATA          HypercallData;
# else
    uint32_t                    uDummy;
# endif
} NEMR0PERVCPU;

/**
 * NEM GVM instance data.
 */
typedef struct NEMR0PERVM
{
# ifdef RT_OS_WINDOWS
    /** The partition ID. */
    uint64_t                    idHvPartition;
    /** I/O control context. */
    PSUPR0IOCTLCTX              pIoCtlCtx;
    /** Delta to add to convert a ring-0 pointer to a ring-3 one.   */
    uintptr_t                   offRing3ConversionDelta;
    /** Info about the VidGetHvPartitionId I/O control interface. */
    NEMWINIOCTL                 IoCtlGetHvPartitionId;
    /** Info about the VidStartVirtualProcessor I/O control interface. */
    NEMWINIOCTL                 IoCtlStartVirtualProcessor;
    /** Info about the VidStopVirtualProcessor I/O control interface. */
    NEMWINIOCTL                 IoCtlStopVirtualProcessor;
    /** Info about the VidStopVirtualProcessor I/O control interface. */
    NEMWINIOCTL                 IoCtlMessageSlotHandleAndGetNext;

    /** Hypercall input/ouput page for non-EMT. */
    NEMR0HYPERCALLDATA          HypercallData;
    /** Critical section protecting use of HypercallData. */
    RTCRITSECT                  HypercallDataCritSect;

# else
    uint32_t                    uDummy;
# endif
} NEMR0PERVM;

#endif /* IN_RING*/


#ifdef IN_RING3
int     nemR3NativeInit(PVM pVM, bool fFallback, bool fForced);
int     nemR3NativeInitAfterCPUM(PVM pVM);
int     nemR3NativeInitCompleted(PVM pVM, VMINITCOMPLETED enmWhat);
int     nemR3NativeTerm(PVM pVM);
void    nemR3NativeReset(PVM pVM);
void    nemR3NativeResetCpu(PVMCPU pVCpu, bool fInitIpi);
VBOXSTRICTRC    nemR3NativeRunGC(PVM pVM, PVMCPU pVCpu);
bool            nemR3NativeCanExecuteGuest(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
bool            nemR3NativeSetSingleInstruction(PVM pVM, PVMCPU pVCpu, bool fEnable);
void            nemR3NativeNotifyFF(PVM pVM, PVMCPU pVCpu, uint32_t fFlags);

int     nemR3NativeNotifyPhysRamRegister(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb);
int     nemR3NativeNotifyPhysMmioExMap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags, void *pvMmio2);
int     nemR3NativeNotifyPhysMmioExUnmap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags);
int     nemR3NativeNotifyPhysRomRegisterEarly(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags);
int     nemR3NativeNotifyPhysRomRegisterLate(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags);
void    nemR3NativeNotifySetA20(PVMCPU pVCpu, bool fEnabled);
#endif

void    nemHCNativeNotifyHandlerPhysicalRegister(PVM pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhys, RTGCPHYS cb);
void    nemHCNativeNotifyHandlerPhysicalDeregister(PVM pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhys, RTGCPHYS cb,
                                                   int fRestoreAsRAM, bool fRestoreAsRAM2);
void    nemHCNativeNotifyHandlerPhysicalModify(PVM pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhysOld,
                                               RTGCPHYS GCPhysNew, RTGCPHYS cb, bool fRestoreAsRAM);
int     nemHCNativeNotifyPhysPageAllocated(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, uint32_t fPageProt,
                                           PGMPAGETYPE enmType, uint8_t *pu2State);
void    nemHCNativeNotifyPhysPageProtChanged(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, uint32_t fPageProt,
                                             PGMPAGETYPE enmType, uint8_t *pu2State);
void    nemHCNativeNotifyPhysPageChanged(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhysPrev, RTHCPHYS HCPhysNew, uint32_t fPageProt,
                                         PGMPAGETYPE enmType, uint8_t *pu2State);


#ifdef RT_OS_WINDOWS
/** Maximum number of pages we can map in a single NEMR0MapPages call. */
# define NEM_MAX_MAP_PAGES      ((PAGE_SIZE - RT_UOFFSETOF(HV_INPUT_MAP_GPA_PAGES, PageList)) / sizeof(HV_SPA_PAGE_NUMBER))
/** Maximum number of pages we can unmap in a single NEMR0UnmapPages call. */
# define NEM_MAX_UNMAP_PAGES    4095

#endif
/** @} */

RT_C_DECLS_END

#endif

