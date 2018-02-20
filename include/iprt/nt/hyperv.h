/** @file
 * Hyper-V related types and definitions.
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


#ifndef ___iprt_nt_hyperv_h
#define ___iprt_nt_hyperv_h

#include <iprt/types.h>
#include <iprt/assertcompile.h>


/** Hyper-V partition ID. */
typedef uint64_t HV_PARTITION_ID;
/** Invalid Hyper-V partition ID. */
#define HV_PARTITION_ID_INVALID UINT64_C(0)
/** Hyper-V virtual processor index (== VMCPUID). */
typedef uint32_t HV_VP_INDEX;
/** Guest physical address (== RTGCPHYS). */
typedef uint64_t HV_GPA;
/** Guest physical page number. */
typedef uint64_t HV_GPA_PAGE_NUMBER;
/** System(/parent) physical page number. */
typedef uint64_t HV_SPA_PAGE_NUMBER;



/** Hypercall IDs.   */
typedef enum
{
    HvCallReserved0000 = 0,

    HvCallSwitchVirtualAddressSpace,
    HvCallFlushVirtualAddressSpace,
    HvCallFlushVirtualAddressList,
    HvCallGetLogicalProcessorRunTime,
    /* 5, 6 & 7 are deprecated / reserved. */
    HvCallNotifyLongSpinWait = 8,
    HvCallParkLogicalProcessors,        /**< @since v2 */
    HvCallInvokeHypervisorDebugger,     /**< @since v2 - not mentioned in TLFS v5.0b  */
    HvCallSendSyntheticClusterIpi,      /**< @since v? */
    HvCallModifyVtlProtectionMask,      /**< @since v? */
    HvCallEnablePartitionVtl,           /**< @since v? */
    HvCallDisablePartitionVtl,          /**< @since v? */
    HvCallEnableVpVtl,                  /**< @since v? */
    HvCallDisableVpVtl,                 /**< @since v? */
    HvCallVtlCall,                      /**< @since v? */
    HvCallVtlReturn,                    /**< @since v? */
    HvCallFlushVirtualAddressSpaceEx,   /**< @since v? */
    HvCallFlushVirtualAddressListEx,    /**< @since v? */
    HvCallSendSyntheticClusterIpiEx,    /**< @since v? */
    /* Reserved: 0x16..0x3f */

    HvCallCreatePartition = 0x40,
    HvCallInitializePartition,
    HvCallFinalizePartition,
    HvCallDeletePartition,
    HvCallGetPartitionProperty,
    HvCallSetPartitionProperty,
    HvCallGetPartitionId,
    HvCallGetNextChildPartition,
    HvCallDepositMemory,                /**< 0x48 - Repeat call. */
    HvCallWithdrawMemory,               /**< 0x49 - Repeat call. */
    HvCallGetMemoryBalance,
    HvCallMapGpaPages,                  /**< 0X4b - Repeat call. */
    HvCallUnmapGpaPages,                /**< 0X4c - Repeat call. */
    HvCallInstallIntercept,
    HvCallCreateVp,
    HvCallDeleteVp,                     /**< 0x4f - Fast call.  */
    HvCallGetVpRegisters,               /**< 0x50 - Repeat call. */
    HvCallSetVpRegisters,               /**< 0x51 - Repeat call. */
    HvCallTranslateVirtualAddress,
    HvCallReadGpa,
    HvCallWriteGpa,
    HvCallAssertVirtualInterruptV1,
    HvCallClearVirtualInterrupt,        /**< 0x56 - Fast call. */
    HvCallCreatePortV1,
    HvCallDeletePort,                   /**< 0x58 - Fast call. */
    HvCallConnectPortV1,
    HvCallGetPortProperty,
    HvCallDisconnectPort,
    HvCallPostMessage,
    HvCallSignalEvent,
    HvCallSavePartitionState,
    HvCallRestorePartitionState,
    HvCallInitializeEventLogBufferGroup,
    HvCallFinalizeEventLogBufferGroup,
    HvCallCreateEventLogBuffer,
    HvCallDeleteEventLogBuffer,
    HvCallMapEventLogBuffer,
    HvCallUnmapEventLogBuffer,
    HvCallSetEventLogGroupSources,
    HvCallReleaseEventLogBuffer,
    HvCallFlushEventLogBuffer,
    HvCallPostDebugData,
    HvCallRetrieveDebugData,
    HvCallResetDebugSession,
    HvCallMapStatsPage,
    HvCallUnmapStatsPage,
    HvCallMapSparseGpaPages,            /**< @since v2 */
    HvCallSetSystemProperty,            /**< @since v2 */
    HvCallSetPortProperty,              /**< @since v2 */
    /* 0x71..0x75 reserved/deprecated (was v2 test IDs). */
    HvCallAddLogicalProcessor = 0x76,
    HvCallRemoveLogicalProcessor,
    HvCallQueryNumaDistance,
    HvCallSetLogicalProcessorProperty,
    HvCallGetLogicalProcessorProperty,
    HvCallGetSystemProperty,
    HvCallMapDeviceInterrupt,
    HvCallUnmapDeviceInterrupt,
    HvCallRetargetDeviceInterrupt,
    /* 0x7f is reserved. */
    HvCallMapDevicePages = 0x80,
    HvCallUnmapDevicePages,
    HvCallAttachDevice,
    HvCallDetachDevice,
    HvCallNotifyStandbyTransition,
    HvCallPrepareForSleep,
    HvCallPrepareForHibernate,
    HvCallNotifyPartitionEvent,
    HvCallGetLogicalProcessorRegisters,
    HvCallSetLogicalProcessorRegisters,
    HvCallQueryAssociatedLpsforMca,
    HvCallNotifyRingEmpty,
    HvCallInjectSyntheticMachineCheck,
    HvCallScrubPartition,
    HvCallCollectLivedump,
    HvCallDisableHypervisor,
    HvCallModifySparseGpaPages,
    HvCallRegisterInterceptResult,
    HvCallUnregisterInterceptResult,
    /* 0x93 is reserved/undocumented. */
    HvCallAssertVirtualInterrupt = 0x94,
    HvCallCreatePort,
    HvCallConnectPort,
    HvCallGetSpaPageList,
    /* 0x98 is reserved. */
    HvCallStartVirtualProcessor = 0x99,
    HvCallGetVpIndexFromApicId,
    /* 0x9b.. are reserved/undocumented. */
    HvCallFlushGuestPhysicalAddressSpace = 0xaf,
    HvCallFlushGuestPhysicalAddressList,
    /* 0xb1..0xb4 are unknown */
    HvCallCreateCpuGroup = 0xb5,
    HvCallDeleteCpuGroup,
    HvCallGetCpuGroupProperty,
    HvCallSetCpuGroupProperty,
    HvCallGetCpuGroupAffinit,
    HvCallGetNextCpuGroup = 0xba,
    HvCallGetNextCpuGroupPartition,
    HvCallPrecommitGpaPages = 0xbe,
    HvCallUncommitGpaPages,             /**< Happens when VidDestroyGpaRangeCheckSecure/WHvUnmapGpaRange is called. */
    /* 0xc0..0xcb are unknown */
    HvCallQueryVtlProtectionMaskRange = 0xcc,
    HvCallModifyVtlProtectionMaskRange,
    /* 0xce..0xd1 are unknown */
    HvCallAcquireSparseGpaPageHostAccess = 0xd2,
    HvCallReleaseSparseGpaPageHostAccess,
    HvCallCheckSparseGpaPageVtlAccess,
    HvCallAcquireSparseSpaPageHostAccess = 0xd7,
    HvCallReleaseSparseSpaPageHostAccess,
    HvCallAcceptGpaPages,                       /**< 0x18 byte input, zero rep, no output. */

    /** Number of defined hypercalls (varies with version). */
    HvCallCount
} HV_CALL_CODE;
AssertCompile(HvCallSendSyntheticClusterIpiEx == 0x15);
AssertCompile(HvCallMapGpaPages == 0x4b);
AssertCompile(HvCallSetPortProperty == 0x70);
AssertCompile(HvCallRetargetDeviceInterrupt == 0x7e);
AssertCompile(HvCallUnregisterInterceptResult == 0x92);
AssertCompile(HvCallGetSpaPageList == 0x97);
AssertCompile(HvCallFlushGuestPhysicalAddressList == 0xb0);
AssertCompile(HvCallUncommitGpaPages == 0xbf);
AssertCompile(HvCallCount == 0xda);


/** Hypercall status code. */
typedef uint16_t HV_STATUS;

/** @name Hyper-V Hypercall status codes
 * @{ */
#define HV_STATUS_SUCCESS                                               (0x0000)
#define HV_STATUS_RESERVED_1                                            (0x0001)
#define HV_STATUS_INVALID_HYPERCALL_CODE                                (0x0002)
#define HV_STATUS_INVALID_HYPERCALL_INPUT                               (0x0003)
#define HV_STATUS_INVALID_ALIGNMENT                                     (0x0004)
#define HV_STATUS_INVALID_PARAMETER                                     (0x0005)
#define HV_STATUS_ACCESS_DENIED                                         (0x0006)
#define HV_STATUS_INVALID_PARTITION_STATE                               (0x0007)
#define HV_STATUS_OPERATION_DENIED                                      (0x0008)
#define HV_STATUS_UNKNOWN_PROPERTY                                      (0x0009)
#define HV_STATUS_PROPERTY_VALUE_OUT_OF_RANGE                           (0x000a)
#define HV_STATUS_INSUFFICIENT_MEMORY                                   (0x000b)
#define HV_STATUS_PARTITION_TOO_DEEP                                    (0x000c)
#define HV_STATUS_INVALID_PARTITION_ID                                  (0x000d)
#define HV_STATUS_INVALID_VP_INDEX                                      (0x000e)
#define HV_STATUS_RESERVED_F                                            (0x000f)
#define HV_STATUS_NOT_FOUND                                             (0x0010)
#define HV_STATUS_INVALID_PORT_ID                                       (0x0011)
#define HV_STATUS_INVALID_CONNECTION_ID                                 (0x0012)
#define HV_STATUS_INSUFFICIENT_BUFFERS                                  (0x0013)
#define HV_STATUS_NOT_ACKNOWLEDGED                                      (0x0014)
#define HV_STATUS_INVALID_VP_STATE                                      (0x0015)
#define HV_STATUS_ACKNOWLEDGED                                          (0x0016)
#define HV_STATUS_INVALID_SAVE_RESTORE_STATE                            (0x0017)
#define HV_STATUS_INVALID_SYNIC_STATE                                   (0x0018)
#define HV_STATUS_OBJECT_IN_USE                                         (0x0019)
#define HV_STATUS_INVALID_PROXIMITY_DOMAIN_INFO                         (0x001a)
#define HV_STATUS_NO_DATA                                               (0x001b)
#define HV_STATUS_INACTIVE                                              (0x001c)
#define HV_STATUS_NO_RESOURCES                                          (0x001d)
#define HV_STATUS_FEATURE_UNAVAILABLE                                   (0x001e)
#define HV_STATUS_PARTIAL_PACKET                                        (0x001f)
#define HV_STATUS_PROCESSOR_FEATURE_SSE3_NOT_SUPPORTED                  (0x0020)
#define HV_STATUS_PROCESSOR_FEATURE_LAHFSAHF_NOT_SUPPORTED              (0x0021)
#define HV_STATUS_PROCESSOR_FEATURE_SSSE3_NOT_SUPPORTED                 (0x0022)
#define HV_STATUS_PROCESSOR_FEATURE_SSE4_1_NOT_SUPPORTED                (0x0023)
#define HV_STATUS_PROCESSOR_FEATURE_SSE4_2_NOT_SUPPORTED                (0x0024)
#define HV_STATUS_PROCESSOR_FEATURE_SSE4A_NOT_SUPPORTED                 (0x0025)
#define HV_STATUS_PROCESSOR_FEATURE_XOP_NOT_SUPPORTED                   (0x0026)
#define HV_STATUS_PROCESSOR_FEATURE_POPCNT_NOT_SUPPORTED                (0x0027)
#define HV_STATUS_PROCESSOR_FEATURE_CMPXCHG16B_NOT_SUPPORTED            (0x0028)
#define HV_STATUS_PROCESSOR_FEATURE_ALTMOVCR8_NOT_SUPPORTED             (0x0029)
#define HV_STATUS_PROCESSOR_FEATURE_LZCNT_NOT_SUPPORTED                 (0x002a)
#define HV_STATUS_PROCESSOR_FEATURE_MISALIGNED_SSE_NOT_SUPPORTED        (0x002b)
#define HV_STATUS_PROCESSOR_FEATURE_MMX_EXT_NOT_SUPPORTED               (0x002c)
#define HV_STATUS_PROCESSOR_FEATURE_3DNOW_NOT_SUPPORTED                 (0x002d)
#define HV_STATUS_PROCESSOR_FEATURE_EXTENDED_3DNOW_NOT_SUPPORTED        (0x002e)
#define HV_STATUS_PROCESSOR_FEATURE_PAGE_1GB_NOT_SUPPORTED              (0x002f)
#define HV_STATUS_PROCESSOR_CACHE_LINE_FLUSH_SIZE_INCOMPATIBLE          (0x0030)
#define HV_STATUS_PROCESSOR_FEATURE_XSAVE_NOT_SUPPORTED                 (0x0031)
#define HV_STATUS_PROCESSOR_FEATURE_XSAVEOPT_NOT_SUPPORTED              (0x0032)
#define HV_STATUS_INSUFFICIENT_BUFFER                                   (0x0033)
#define HV_STATUS_PROCESSOR_FEATURE_XSAVE_AVX_NOT_SUPPORTED             (0x0034)
#define HV_STATUS_PROCESSOR_FEATURE_XSAVE_ FEATURE_NOT_SUPPORTED        (0x0035)
#define HV_STATUS_PROCESSOR_XSAVE_SAVE_AREA_INCOMPATIBLE                (0x0036)
#define HV_STATUS_INCOMPATIBLE_PROCESSOR                                (0x0037)
#define HV_STATUS_INSUFFICIENT_DEVICE_DOMAINS                           (0x0038)
#define HV_STATUS_PROCESSOR_FEATURE_AES_NOT_SUPPORTED                   (0x0039)
#define HV_STATUS_PROCESSOR_FEATURE_PCLMULQDQ_NOT_SUPPORTED             (0x003a)
#define HV_STATUS_PROCESSOR_FEATURE_INCOMPATIBLE_XSAVE_FEATURES         (0x003b)
#define HV_STATUS_CPUID_FEATURE_VALIDATION_ERROR                        (0x003c)
#define HV_STATUS_CPUID_XSAVE_FEATURE_VALIDATION_ERROR                  (0x003d)
#define HV_STATUS_PROCESSOR_STARTUP_TIMEOUT                             (0x003e)
#define HV_STATUS_SMX_ENABLED                                           (0x003f)
#define HV_STATUS_PROCESSOR_FEATURE_PCID_NOT_SUPPORTED                  (0x0040)
#define HV_STATUS_INVALID_LP_INDEX                                      (0x0041)
#define HV_STATUS_FEATURE_FMA4_NOT_SUPPORTED                            (0x0042)
#define HV_STATUS_FEATURE_F16C_NOT_SUPPORTED                            (0x0043)
#define HV_STATUS_PROCESSOR_FEATURE_RDRAND_NOT_SUPPORTED                (0x0044)
#define HV_STATUS_PROCESSOR_FEATURE_RDWRFSGS_NOT_SUPPORTED              (0x0045)
#define HV_STATUS_PROCESSOR_FEATURE_SMEP_NOT_SUPPORTED                  (0x0046)
#define HV_STATUS_PROCESSOR_FEATURE_ENHANCED_FAST_STRING_NOT_SUPPORTED  (0x0047)
#define HV_STATUS_PROCESSOR_FEATURE_MOVBE_NOT_SUPPORTED                 (0x0048)
#define HV_STATUS_PROCESSOR_FEATURE_BMI1_NOT_SUPPORTED                  (0x0049)
#define HV_STATUS_PROCESSOR_FEATURE_BMI2_NOT_SUPPORTED                  (0x004a)
#define HV_STATUS_PROCESSOR_FEATURE_HLE_NOT_SUPPORTED                   (0x004b)
#define HV_STATUS_PROCESSOR_FEATURE_RTM_NOT_SUPPORTED                   (0x004c)
#define HV_STATUS_PROCESSOR_FEATURE_XSAVE_FMA_NOT_SUPPORTED             (0x004d)
#define HV_STATUS_PROCESSOR_FEATURE_XSAVE_AVX2_NOT_SUPPORTED            (0x004e)
#define HV_STATUS_PROCESSOR_FEATURE_NPIEP1_NOT_SUPPORTED                (0x004f)
#define HV_STATUS_INVALID_REGISTER_VALUE                                (0x0050)
#define HV_STATUS_PROCESSOR_FEATURE_RDSEED_NOT_SUPPORTED                (0x0052)
#define HV_STATUS_PROCESSOR_FEATURE_ADX_NOT_SUPPORTED                   (0x0053)
#define HV_STATUS_PROCESSOR_FEATURE_SMAP_NOT_SUPPORTED                  (0x0054)
#define HV_STATUS_NX_NOT_DETECTED                                       (0x0055)
#define HV_STATUS_PROCESSOR_FEATURE_INTEL_PREFETCH_NOT_SUPPORTED        (0x0056)
#define HV_STATUS_INVALID_DEVICE_ID                                     (0x0057)
#define HV_STATUS_INVALID_DEVICE_STATE                                  (0x0058)
#define HV_STATUS_PENDING_PAGE_REQUESTS                                 (0x0059)
#define HV_STATUS_PAGE_REQUEST_INVALID                                  (0x0060)
#define HV_STATUS_OPERATION_FAILED                                      (0x0071)
#define HV_STATUS_NOT_ALLOWED_WITH_NESTED_VIRT_ACTIVE                   (0x0072)
/** @} */


/** @name Flags used with HvCallMapGpaPages and HvCallMapSparseGpaPages.
 * @note There seems to be a more flags defined after v2.
 * @{ */
typedef uint32_t HV_MAP_GPA_FLAGS;
#define HV_MAP_GPA_READABLE             UINT32_C(0x0001)
#define HV_MAP_GPA_WRITABLE             UINT32_C(0x0002)
#define HV_MAP_GPA_EXECUTABLE           UINT32_C(0x0004)
/** Seems this have to be set when HV_MAP_GPA_EXECUTABLE is (17101). */
#define HV_MAP_GPA_EXECUTABLE_AGAIN     UINT32_C(0x0008)
/** Dunno what this is yet, but it requires HV_MAP_GPA_DUNNO_1000.
 * The readable bit gets put here when both HV_MAP_GPA_DUNNO_1000 and
 * HV_MAP_GPA_DUNNO_MASK_0700 are clear. */
#define HV_MAP_GPA_DUNNO_ACCESS         UINT32_C(0x0010)
/** Guess work. */
#define HV_MAP_GPA_MAYBE_ACCESS_MASK    UINT32_C(0x001f)
/** Some kind of mask. */
#define HV_MAP_GPA_DUNNO_MASK_0700      UINT32_C(0x0700)
/** Dunno what this is, but required for HV_MAP_GPA_DUNNO_ACCESS. */
#define HV_MAP_GPA_DUNNO_1000           UINT32_C(0x1000)
/** Working with large 2MB pages. */
#define HV_MAP_GPA_LARGE                UINT32_C(0x2000)
/** Valid mask as per build 17101. */
#define HV_MAP_GPA_VALID_MASK           UINT32_C(0x7f1f)
/** @}  */

/** Input for HvCallMapGpaPages. */
typedef struct
{
    HV_PARTITION_ID     TargetPartitionId;
    HV_GPA_PAGE_NUMBER  TargetGpaBase;
    HV_MAP_GPA_FLAGS    MapFlags;
    uint32_t            u32ExplicitPadding;
    /* The repeating part: */
    HV_SPA_PAGE_NUMBER  PageList[RT_FLEXIBLE_ARRAY];
} HV_INPUT_MAP_GPA_PAGES;
AssertCompileMemberOffset(HV_INPUT_MAP_GPA_PAGES, PageList, 24);
/** Pointer to the input for HvCallMapGpaPages. */
typedef HV_INPUT_MAP_GPA_PAGES *PHV_INPUT_MAP_GPA_PAGES;


/** A parent to guest mapping pair for HvCallMapSparseGpaPages. */
typedef struct
{
    HV_GPA_PAGE_NUMBER TargetGpaPageNumber;
    HV_SPA_PAGE_NUMBER SourceSpaPageNumber;
} HV_GPA_MAPPING;
/** Pointer to a parent->guest mapping pair for HvCallMapSparseGpaPages. */
typedef HV_GPA_MAPPING *PHV_GPA_MAPPING;

/** Input for HvCallMapSparseGpaPages. */
typedef struct
{
    HV_PARTITION_ID     TargetPartitionId;
    HV_MAP_GPA_FLAGS    MapFlags;
    uint32_t            u32ExplicitPadding;
    /* The repeating part: */
    HV_GPA_MAPPING      PageList[RT_FLEXIBLE_ARRAY];
} HV_INPUT_MAP_SPARSE_GPA_PAGES;
AssertCompileMemberOffset(HV_INPUT_MAP_SPARSE_GPA_PAGES, PageList, 16);
/** Pointer to the input for HvCallMapSparseGpaPages. */
typedef HV_INPUT_MAP_SPARSE_GPA_PAGES *PHV_INPUT_MAP_SPARSE_GPA_PAGES;


/** Input for HvCallUnmapGpaPages. */
typedef struct
{
    HV_PARTITION_ID     TargetPartitionId;
    HV_GPA_PAGE_NUMBER  TargetGpaBase;
    /** This field is either an omission in the 7600 WDK or a later additions.
     *  Anyway, not quite sure what it does.  Bit 2 seems to indicate 2MB pages. */
    uint64_t            fFlags;
} HV_INPUT_UNMAP_GPA_PAGES;
AssertCompileSize(HV_INPUT_UNMAP_GPA_PAGES, 24);
/** Pointer to the input for HvCallUnmapGpaPages. */
typedef HV_INPUT_UNMAP_GPA_PAGES *PHV_INPUT_UNMAP_GPA_PAGES;



/** Cache types used by HvCallReadGpa and HvCallWriteGpa. */
typedef enum
{
    HvCacheTypeX64Uncached = 0,
    HvCacheTypeX64WriteCombining,
    /* 2 & 3 are undefined. */
    HvCacheTypeX64WriteThrough = 4,
    HvCacheTypeX64WriteProtected,
    HvCacheTypeX64WriteBack
} HV_CACHE_TYPE;

/** Control flags for HvCallReadGpa and HvCallWriteGpa. */
typedef union
{
    uint64_t            AsUINT64;
    struct
    {
        uint64_t        CacheType : 8;      /**< HV_CACHE_TYPE */
        uint64_t        Reserved  : 56;
    };
} HV_ACCESS_GPA_CONTROL_FLAGS;

/** Results codes for HvCallReadGpa and HvCallWriteGpa. */
typedef enum
{
    HvAccessGpaSuccess = 0,
    HvAccessGpaUnmapped,
    HvAccessGpaReadIntercept,
    HvAccessGpaWriteIntercept,
    HvAccessGpaIllegalOverlayAccess
} HV_ACCESS_GPA_RESULT_CODE;

/** The result of HvCallReadGpa and HvCallWriteGpa. */
typedef union
{
    uint64_t                        AsUINT64;
    struct
    {
        HV_ACCESS_GPA_RESULT_CODE   ResultCode;
        uint32_t                    Reserved;
    };
} HV_ACCESS_GPA_RESULT;


/** Input for HvCallReadGpa. */
typedef struct
{
    HV_PARTITION_ID             PartitionId;
    HV_VP_INDEX                 VpIndex;
    uint32_t                    ByteCount;
    HV_GPA                      BaseGpa;
    HV_ACCESS_GPA_CONTROL_FLAGS ControlFlags;
} HV_INPUT_READ_GPA;
AssertCompileSize(HV_INPUT_READ_GPA, 32);
/** Pointer to the input for HvCallReadGpa. */
typedef HV_INPUT_READ_GPA *PHV_INPUT_READ_GPA;

/** Output for HvCallReadGpa. */
typedef struct
{
    HV_ACCESS_GPA_RESULT        AccessResult;
    uint8_t                     Data[16];
} HV_OUTPUT_READ_GPA;
AssertCompileSize(HV_OUTPUT_READ_GPA, 24);
/** Pointer to the output for HvCallReadGpa. */
typedef HV_OUTPUT_READ_GPA *PHV_OUTPUT_READ_GPA;


/** Input for HvCallWriteGpa. */
typedef struct
{
    HV_PARTITION_ID             PartitionId;
    HV_VP_INDEX                 VpIndex;
    uint32_t                    ByteCount;
    HV_GPA                      BaseGpa;
    HV_ACCESS_GPA_CONTROL_FLAGS ControlFlags;
    uint8_t                     Data[16];
} HV_INPUT_WRITE_GPA;
AssertCompileSize(HV_INPUT_READ_GPA, 32);
/** Pointer to the input for HvCallWriteGpa. */
typedef HV_INPUT_READ_GPA *PHV_INPUT_READ_GPA;

/** Output for HvCallWriteGpa. */
typedef struct
{
    HV_ACCESS_GPA_RESULT        AccessResult;
} HV_OUTPUT_WRITE_GPA;
AssertCompileSize(HV_OUTPUT_WRITE_GPA, 8);
/** Pointer to the output for HvCallWriteGpa. */
typedef HV_OUTPUT_WRITE_GPA *PHV_OUTPUT_WRITE_GPA;


#endif

