/* $Id$ */
/** @file
 * CPUM - Internal header file.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___CPUMInternal_h
#define ___CPUMInternal_h

#ifndef VBOX_FOR_DTRACE_LIB
# include <VBox/cdefs.h>
# include <VBox/types.h>
# include <VBox/vmm/stam.h>
# include <iprt/x86.h>
#else
# pragma D depends_on library x86.d
# pragma D depends_on library cpumctx.d
#endif




/** @defgroup grp_cpum_int   Internals
 * @ingroup grp_cpum
 * @internal
 * @{
 */

/** Flags and types for CPUM fault handlers
 * @{ */
/** Type: Load DS */
#define CPUM_HANDLER_DS                 1
/** Type: Load ES */
#define CPUM_HANDLER_ES                 2
/** Type: Load FS */
#define CPUM_HANDLER_FS                 3
/** Type: Load GS */
#define CPUM_HANDLER_GS                 4
/** Type: IRET */
#define CPUM_HANDLER_IRET               5
/** Type mask. */
#define CPUM_HANDLER_TYPEMASK           0xff
/** If set EBP points to the CPUMCTXCORE that's being used. */
#define CPUM_HANDLER_CTXCORE_IN_EBP     RT_BIT(31)
/** @} */


/** Use flags (CPUM::fUseFlags).
 * (Don't forget to sync this with CPUMInternal.mac !)
 * @{ */
/** Used the FPU, SSE or such stuff. */
#define CPUM_USED_FPU                   RT_BIT(0)
/** Used the FPU, SSE or such stuff since last we were in REM.
 * REM syncing is clearing this, lazy FPU is setting it. */
#define CPUM_USED_FPU_SINCE_REM         RT_BIT(1)
/** The XMM state was manually restored. (AMD only) */
#define CPUM_USED_MANUAL_XMM_RESTORE    RT_BIT(2)

/** Host OS is using SYSENTER and we must NULL the CS. */
#define CPUM_USE_SYSENTER               RT_BIT(3)
/** Host OS is using SYSENTER and we must NULL the CS. */
#define CPUM_USE_SYSCALL                RT_BIT(4)

/** Debug registers are used by host and that DR7 and DR6 must be saved and
 *  disabled when switching to raw-mode. */
#define CPUM_USE_DEBUG_REGS_HOST        RT_BIT(5)
/** Records that we've saved the host DRx registers.
 * In ring-0 this means all (DR0-7), while in raw-mode context this means DR0-3
 * since DR6 and DR7 are covered by CPUM_USE_DEBUG_REGS_HOST. */
#define CPUM_USED_DEBUG_REGS_HOST       RT_BIT(6)
/** Set to indicate that we should save host DR0-7 and load the hypervisor debug
 * registers in the raw-mode world switchers. (See CPUMRecalcHyperDRx.) */
#define CPUM_USE_DEBUG_REGS_HYPER       RT_BIT(7)
/** Used in ring-0 to indicate that we have loaded the hypervisor debug
 * registers. */
#define CPUM_USED_DEBUG_REGS_HYPER      RT_BIT(8)
/** Used in ring-0 to indicate that we have loaded the guest debug
 * registers (DR0-3 and maybe DR6) for direct use by the guest.
 * DR7 (and AMD-V DR6) are handled via the VMCB. */
#define CPUM_USED_DEBUG_REGS_GUEST      RT_BIT(9)


/** Sync the FPU state on next entry (32->64 switcher only). */
#define CPUM_SYNC_FPU_STATE             RT_BIT(16)
/** Sync the debug state on next entry (32->64 switcher only). */
#define CPUM_SYNC_DEBUG_REGS_GUEST      RT_BIT(17)
/** Sync the debug state on next entry (32->64 switcher only).
 * Almost the same as CPUM_USE_DEBUG_REGS_HYPER in the raw-mode switchers.  */
#define CPUM_SYNC_DEBUG_REGS_HYPER      RT_BIT(18)
/** Host CPU requires fxsave/fxrstor leaky bit handling. */
#define CPUM_USE_FFXSR_LEAKY            RT_BIT(19)
/** @} */

/* Sanity check. */
#ifndef VBOX_FOR_DTRACE_LIB
#if defined(VBOX_WITH_HYBRID_32BIT_KERNEL) && (HC_ARCH_BITS != 32 || R0_ARCH_BITS != 32)
# error "VBOX_WITH_HYBRID_32BIT_KERNEL is only for 32 bit builds."
#endif
#endif


/**
 * MSR read functions.
 */
typedef enum CPUMMSRRDFN
{
    /** Invalid zero value. */
    kCpumMsrRdFn_Invalid = 0,
    /** Return the CPUMMSRRANGE::uValue. */
    kCpumMsrRdFn_FixedValue,
    /** Alias to the MSR range starting at the MSR given by
     * CPUMMSRRANGE::uValue.  Must be used in pair with
     * kCpumMsrWrFn_MsrAlias. */
    kCpumMsrRdFn_MsrAlias,
    /** Write only register, GP all read attempts. */
    kCpumMsrRdFn_WriteOnly,

    kCpumMsrRdFn_Ia32P5McAddr,
    kCpumMsrRdFn_Ia32P5McType,
    kCpumMsrRdFn_Ia32TimestampCounter,
    kCpumMsrRdFn_Ia32PlatformId,            /**< Takes real CPU value for reference. */
    kCpumMsrRdFn_Ia32ApicBase,
    kCpumMsrRdFn_Ia32FeatureControl,
    kCpumMsrRdFn_Ia32BiosSignId,            /**< Range value returned. */
    kCpumMsrRdFn_Ia32SmmMonitorCtl,
    kCpumMsrRdFn_Ia32PmcN,
    kCpumMsrRdFn_Ia32MonitorFilterLineSize,
    kCpumMsrRdFn_Ia32MPerf,
    kCpumMsrRdFn_Ia32APerf,
    kCpumMsrRdFn_Ia32MtrrCap,               /**< Takes real CPU value for reference.  */
    kCpumMsrRdFn_Ia32MtrrPhysBaseN,         /**< Takes register number. */
    kCpumMsrRdFn_Ia32MtrrPhysMaskN,         /**< Takes register number. */
    kCpumMsrRdFn_Ia32MtrrFixed,             /**< Takes CPUMCPU offset. */
    kCpumMsrRdFn_Ia32MtrrDefType,
    kCpumMsrRdFn_Ia32Pat,
    kCpumMsrRdFn_Ia32SysEnterCs,
    kCpumMsrRdFn_Ia32SysEnterEsp,
    kCpumMsrRdFn_Ia32SysEnterEip,
    kCpumMsrRdFn_Ia32McgCap,
    kCpumMsrRdFn_Ia32McgStatus,
    kCpumMsrRdFn_Ia32McgCtl,
    kCpumMsrRdFn_Ia32DebugCtl,
    kCpumMsrRdFn_Ia32SmrrPhysBase,
    kCpumMsrRdFn_Ia32SmrrPhysMask,
    kCpumMsrRdFn_Ia32PlatformDcaCap,
    kCpumMsrRdFn_Ia32CpuDcaCap,
    kCpumMsrRdFn_Ia32Dca0Cap,
    kCpumMsrRdFn_Ia32PerfEvtSelN,           /**< Range value indicates the register number. */
    kCpumMsrRdFn_Ia32PerfStatus,            /**< Range value returned. */
    kCpumMsrRdFn_Ia32PerfCtl,               /**< Range value returned. */
    kCpumMsrRdFn_Ia32FixedCtrN,             /**< Takes register number of start of range. */
    kCpumMsrRdFn_Ia32PerfCapabilities,      /**< Takes reference value. */
    kCpumMsrRdFn_Ia32FixedCtrCtrl,
    kCpumMsrRdFn_Ia32PerfGlobalStatus,      /**< Takes reference value. */
    kCpumMsrRdFn_Ia32PerfGlobalCtrl,
    kCpumMsrRdFn_Ia32PerfGlobalOvfCtrl,
    kCpumMsrRdFn_Ia32PebsEnable,
    kCpumMsrRdFn_Ia32ClockModulation,       /**< Range value returned. */
    kCpumMsrRdFn_Ia32ThermInterrupt,        /**< Range value returned. */
    kCpumMsrRdFn_Ia32ThermStatus,           /**< Range value returned. */
    kCpumMsrRdFn_Ia32Therm2Ctl,             /**< Range value returned. */
    kCpumMsrRdFn_Ia32MiscEnable,            /**< Range value returned. */
    kCpumMsrRdFn_Ia32McCtlStatusAddrMiscN,  /**< Takes bank number. */
    kCpumMsrRdFn_Ia32McNCtl2,               /**< Takes register number of start of range. */
    kCpumMsrRdFn_Ia32DsArea,
    kCpumMsrRdFn_Ia32TscDeadline,
    kCpumMsrRdFn_Ia32X2ApicN,
    kCpumMsrRdFn_Ia32DebugInterface,
    kCpumMsrRdFn_Ia32VmxBase,               /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxPinbasedCtls,       /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxProcbasedCtls,      /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxExitCtls,           /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxEntryCtls,          /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxMisc,               /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxCr0Fixed0,          /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxCr0Fixed1,          /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxCr4Fixed0,          /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxCr4Fixed1,          /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxVmcsEnum,           /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxProcBasedCtls2,     /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxEptVpidCap,         /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxTruePinbasedCtls,   /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxTrueProcbasedCtls,  /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxTrueExitCtls,       /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxTrueEntryCtls,      /**< Takes real value as reference. */

    kCpumMsrRdFn_Amd64Efer,
    kCpumMsrRdFn_Amd64SyscallTarget,
    kCpumMsrRdFn_Amd64LongSyscallTarget,
    kCpumMsrRdFn_Amd64CompSyscallTarget,
    kCpumMsrRdFn_Amd64SyscallFlagMask,
    kCpumMsrRdFn_Amd64FsBase,
    kCpumMsrRdFn_Amd64GsBase,
    kCpumMsrRdFn_Amd64KernelGsBase,
    kCpumMsrRdFn_Amd64TscAux,

    kCpumMsrRdFn_IntelEblCrPowerOn,
    kCpumMsrRdFn_IntelI7CoreThreadCount,
    kCpumMsrRdFn_IntelP4EbcHardPowerOn,
    kCpumMsrRdFn_IntelP4EbcSoftPowerOn,
    kCpumMsrRdFn_IntelP4EbcFrequencyId,
    kCpumMsrRdFn_IntelP6FsbFrequency,       /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelPlatformInfo,
    kCpumMsrRdFn_IntelFlexRatio,            /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelPkgCStConfigControl,
    kCpumMsrRdFn_IntelPmgIoCaptureBase,
    kCpumMsrRdFn_IntelLastBranchFromToN,
    kCpumMsrRdFn_IntelLastBranchFromN,
    kCpumMsrRdFn_IntelLastBranchToN,
    kCpumMsrRdFn_IntelLastBranchTos,
    kCpumMsrRdFn_IntelBblCrCtl,
    kCpumMsrRdFn_IntelBblCrCtl3,
    kCpumMsrRdFn_IntelI7TemperatureTarget,  /**< Range value returned. */
    kCpumMsrRdFn_IntelI7MsrOffCoreResponseN,/**< Takes register number. */
    kCpumMsrRdFn_IntelI7MiscPwrMgmt,
    kCpumMsrRdFn_IntelP6CrN,
    kCpumMsrRdFn_IntelCpuId1FeatureMaskEcdx,
    kCpumMsrRdFn_IntelCpuId1FeatureMaskEax,
    kCpumMsrRdFn_IntelCpuId80000001FeatureMaskEcdx,
    kCpumMsrRdFn_IntelI7SandyAesNiCtl,
    kCpumMsrRdFn_IntelI7TurboRatioLimit,    /**< Returns range value. */
    kCpumMsrRdFn_IntelI7LbrSelect,
    kCpumMsrRdFn_IntelI7SandyErrorControl,
    kCpumMsrRdFn_IntelI7VirtualLegacyWireCap,/**< Returns range value. */
    kCpumMsrRdFn_IntelI7PowerCtl,
    kCpumMsrRdFn_IntelI7SandyPebsNumAlt,
    kCpumMsrRdFn_IntelI7PebsLdLat,
    kCpumMsrRdFn_IntelI7PkgCnResidencyN,     /**< Takes C-state number. */
    kCpumMsrRdFn_IntelI7CoreCnResidencyN,    /**< Takes C-state number. */
    kCpumMsrRdFn_IntelI7SandyVrCurrentConfig,/**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7SandyVrMiscConfig,   /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7SandyRaplPowerUnit,  /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7SandyPkgCnIrtlN,     /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7SandyPkgC2Residency, /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplPkgPowerLimit,   /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplPkgEnergyStatus, /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplPkgPerfStatus,   /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplPkgPowerInfo,    /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplDramPowerLimit,  /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplDramEnergyStatus,/**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplDramPerfStatus,  /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplDramPowerInfo,   /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplPp0PowerLimit,   /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplPp0EnergyStatus, /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplPp0Policy,       /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplPp0PerfStatus,   /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplPp1PowerLimit,   /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplPp1EnergyStatus, /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplPp1Policy,       /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7IvyConfigTdpNominal, /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7IvyConfigTdpLevel1,  /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7IvyConfigTdpLevel2,  /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7IvyConfigTdpControl,
    kCpumMsrRdFn_IntelI7IvyTurboActivationRatio,
    kCpumMsrRdFn_IntelI7UncPerfGlobalCtrl,
    kCpumMsrRdFn_IntelI7UncPerfGlobalStatus,
    kCpumMsrRdFn_IntelI7UncPerfGlobalOvfCtrl,
    kCpumMsrRdFn_IntelI7UncPerfFixedCtrCtrl,
    kCpumMsrRdFn_IntelI7UncPerfFixedCtr,
    kCpumMsrRdFn_IntelI7UncCBoxConfig,
    kCpumMsrRdFn_IntelI7UncArbPerfCtrN,
    kCpumMsrRdFn_IntelI7UncArbPerfEvtSelN,
    kCpumMsrRdFn_IntelCore2EmttmCrTablesN,  /**< Range value returned. */
    kCpumMsrRdFn_IntelCore2SmmCStMiscInfo,
    kCpumMsrRdFn_IntelCore1ExtConfig,
    kCpumMsrRdFn_IntelCore1DtsCalControl,
    kCpumMsrRdFn_IntelCore2PeciControl,

    kCpumMsrRdFn_P6LastBranchFromIp,
    kCpumMsrRdFn_P6LastBranchToIp,
    kCpumMsrRdFn_P6LastIntFromIp,
    kCpumMsrRdFn_P6LastIntToIp,

    kCpumMsrRdFn_AmdFam15hTscRate,
    kCpumMsrRdFn_AmdFam15hLwpCfg,
    kCpumMsrRdFn_AmdFam15hLwpCbAddr,
    kCpumMsrRdFn_AmdFam10hMc4MiscN,
    kCpumMsrRdFn_AmdK8PerfCtlN,
    kCpumMsrRdFn_AmdK8PerfCtrN,
    kCpumMsrRdFn_AmdK8SysCfg,               /**< Range value returned. */
    kCpumMsrRdFn_AmdK8HwCr,
    kCpumMsrRdFn_AmdK8IorrBaseN,
    kCpumMsrRdFn_AmdK8IorrMaskN,
    kCpumMsrRdFn_AmdK8TopOfMemN,
    kCpumMsrRdFn_AmdK8NbCfg1,
    kCpumMsrRdFn_AmdK8McXcptRedir,
    kCpumMsrRdFn_AmdK8CpuNameN,
    kCpumMsrRdFn_AmdK8HwThermalCtrl,        /**< Range value returned. */
    kCpumMsrRdFn_AmdK8SwThermalCtrl,
    kCpumMsrRdFn_AmdK8FidVidControl,        /**< Range value returned. */
    kCpumMsrRdFn_AmdK8FidVidStatus,         /**< Range value returned. */
    kCpumMsrRdFn_AmdK8McCtlMaskN,
    kCpumMsrRdFn_AmdK8SmiOnIoTrapN,
    kCpumMsrRdFn_AmdK8SmiOnIoTrapCtlSts,
    kCpumMsrRdFn_AmdK8IntPendingMessage,
    kCpumMsrRdFn_AmdK8SmiTriggerIoCycle,
    kCpumMsrRdFn_AmdFam10hMmioCfgBaseAddr,
    kCpumMsrRdFn_AmdFam10hTrapCtlMaybe,
    kCpumMsrRdFn_AmdFam10hPStateCurLimit,   /**< Returns range value. */
    kCpumMsrRdFn_AmdFam10hPStateControl,    /**< Returns range value. */
    kCpumMsrRdFn_AmdFam10hPStateStatus,     /**< Returns range value. */
    kCpumMsrRdFn_AmdFam10hPStateN,          /**< Returns range value. This isn't an register index! */
    kCpumMsrRdFn_AmdFam10hCofVidControl,    /**< Returns range value. */
    kCpumMsrRdFn_AmdFam10hCofVidStatus,     /**< Returns range value. */
    kCpumMsrRdFn_AmdFam10hCStateIoBaseAddr,
    kCpumMsrRdFn_AmdFam10hCpuWatchdogTimer,
    kCpumMsrRdFn_AmdK8SmmBase,
    kCpumMsrRdFn_AmdK8SmmAddr,
    kCpumMsrRdFn_AmdK8SmmMask,
    kCpumMsrRdFn_AmdK8VmCr,
    kCpumMsrRdFn_AmdK8IgnNe,
    kCpumMsrRdFn_AmdK8SmmCtl,
    kCpumMsrRdFn_AmdK8VmHSavePa,
    kCpumMsrRdFn_AmdFam10hVmLockKey,
    kCpumMsrRdFn_AmdFam10hSmmLockKey,
    kCpumMsrRdFn_AmdFam10hLocalSmiStatus,
    kCpumMsrRdFn_AmdFam10hOsVisWrkIdLength,
    kCpumMsrRdFn_AmdFam10hOsVisWrkStatus,
    kCpumMsrRdFn_AmdFam16hL2IPerfCtlN,
    kCpumMsrRdFn_AmdFam16hL2IPerfCtrN,
    kCpumMsrRdFn_AmdFam15hNorthbridgePerfCtlN,
    kCpumMsrRdFn_AmdFam15hNorthbridgePerfCtrN,
    kCpumMsrRdFn_AmdK7MicrocodeCtl,         /**< Returns range value. */
    kCpumMsrRdFn_AmdK7ClusterIdMaybe,       /**< Returns range value. */
    kCpumMsrRdFn_AmdK8CpuIdCtlStd07hEbax,
    kCpumMsrRdFn_AmdK8CpuIdCtlStd06hEcx,
    kCpumMsrRdFn_AmdK8CpuIdCtlStd01hEdcx,
    kCpumMsrRdFn_AmdK8CpuIdCtlExt01hEdcx,
    kCpumMsrRdFn_AmdK8PatchLevel,           /**< Returns range value. */
    kCpumMsrRdFn_AmdK7DebugStatusMaybe,
    kCpumMsrRdFn_AmdK7BHTraceBaseMaybe,
    kCpumMsrRdFn_AmdK7BHTracePtrMaybe,
    kCpumMsrRdFn_AmdK7BHTraceLimitMaybe,
    kCpumMsrRdFn_AmdK7HardwareDebugToolCfgMaybe,
    kCpumMsrRdFn_AmdK7FastFlushCountMaybe,
    kCpumMsrRdFn_AmdK7NodeId,
    kCpumMsrRdFn_AmdK7DrXAddrMaskN,      /**< Takes register index. */
    kCpumMsrRdFn_AmdK7Dr0DataMatchMaybe,
    kCpumMsrRdFn_AmdK7Dr0DataMaskMaybe,
    kCpumMsrRdFn_AmdK7LoadStoreCfg,
    kCpumMsrRdFn_AmdK7InstrCacheCfg,
    kCpumMsrRdFn_AmdK7DataCacheCfg,
    kCpumMsrRdFn_AmdK7BusUnitCfg,
    kCpumMsrRdFn_AmdK7DebugCtl2Maybe,
    kCpumMsrRdFn_AmdFam15hFpuCfg,
    kCpumMsrRdFn_AmdFam15hDecoderCfg,
    kCpumMsrRdFn_AmdFam10hBusUnitCfg2,
    kCpumMsrRdFn_AmdFam15hCombUnitCfg,
    kCpumMsrRdFn_AmdFam15hCombUnitCfg2,
    kCpumMsrRdFn_AmdFam15hCombUnitCfg3,
    kCpumMsrRdFn_AmdFam15hExecUnitCfg,
    kCpumMsrRdFn_AmdFam15hLoadStoreCfg2,
    kCpumMsrRdFn_AmdFam10hIbsFetchCtl,
    kCpumMsrRdFn_AmdFam10hIbsFetchLinAddr,
    kCpumMsrRdFn_AmdFam10hIbsFetchPhysAddr,
    kCpumMsrRdFn_AmdFam10hIbsOpExecCtl,
    kCpumMsrRdFn_AmdFam10hIbsOpRip,
    kCpumMsrRdFn_AmdFam10hIbsOpData,
    kCpumMsrRdFn_AmdFam10hIbsOpData2,
    kCpumMsrRdFn_AmdFam10hIbsOpData3,
    kCpumMsrRdFn_AmdFam10hIbsDcLinAddr,
    kCpumMsrRdFn_AmdFam10hIbsDcPhysAddr,
    kCpumMsrRdFn_AmdFam10hIbsCtl,
    kCpumMsrRdFn_AmdFam14hIbsBrTarget,

    /** End of valid MSR read function indexes. */
    kCpumMsrRdFn_End
} CPUMMSRRDFN;

/**
 * MSR write functions.
 */
typedef enum CPUMMSRWRFN
{
    /** Invalid zero value. */
    kCpumMsrWrFn_Invalid = 0,
    /** Writes are ignored, the fWrGpMask is observed though. */
    kCpumMsrWrFn_IgnoreWrite,
    /** Writes cause GP(0) to be raised, the fWrGpMask should be UINT64_MAX. */
    kCpumMsrWrFn_ReadOnly,
    /** Alias to the MSR range starting at the MSR given by
     * CPUMMSRRANGE::uValue.  Must be used in pair with
     * kCpumMsrRdFn_MsrAlias. */
    kCpumMsrWrFn_MsrAlias,

    kCpumMsrWrFn_Ia32P5McAddr,
    kCpumMsrWrFn_Ia32P5McType,
    kCpumMsrWrFn_Ia32TimestampCounter,
    kCpumMsrWrFn_Ia32ApicBase,
    kCpumMsrWrFn_Ia32FeatureControl,
    kCpumMsrWrFn_Ia32BiosSignId,
    kCpumMsrWrFn_Ia32BiosUpdateTrigger,
    kCpumMsrWrFn_Ia32SmmMonitorCtl,
    kCpumMsrWrFn_Ia32PmcN,
    kCpumMsrWrFn_Ia32MonitorFilterLineSize,
    kCpumMsrWrFn_Ia32MPerf,
    kCpumMsrWrFn_Ia32APerf,
    kCpumMsrWrFn_Ia32MtrrPhysBaseN,         /**< Takes register number. */
    kCpumMsrWrFn_Ia32MtrrPhysMaskN,         /**< Takes register number. */
    kCpumMsrWrFn_Ia32MtrrFixed,             /**< Takes CPUMCPU offset. */
    kCpumMsrWrFn_Ia32MtrrDefType,
    kCpumMsrWrFn_Ia32Pat,
    kCpumMsrWrFn_Ia32SysEnterCs,
    kCpumMsrWrFn_Ia32SysEnterEsp,
    kCpumMsrWrFn_Ia32SysEnterEip,
    kCpumMsrWrFn_Ia32McgStatus,
    kCpumMsrWrFn_Ia32McgCtl,
    kCpumMsrWrFn_Ia32DebugCtl,
    kCpumMsrWrFn_Ia32SmrrPhysBase,
    kCpumMsrWrFn_Ia32SmrrPhysMask,
    kCpumMsrWrFn_Ia32PlatformDcaCap,
    kCpumMsrWrFn_Ia32Dca0Cap,
    kCpumMsrWrFn_Ia32PerfEvtSelN,           /**< Range value indicates the register number. */
    kCpumMsrWrFn_Ia32PerfStatus,
    kCpumMsrWrFn_Ia32PerfCtl,
    kCpumMsrWrFn_Ia32FixedCtrN,             /**< Takes register number of start of range. */
    kCpumMsrWrFn_Ia32PerfCapabilities,
    kCpumMsrWrFn_Ia32FixedCtrCtrl,
    kCpumMsrWrFn_Ia32PerfGlobalStatus,
    kCpumMsrWrFn_Ia32PerfGlobalCtrl,
    kCpumMsrWrFn_Ia32PerfGlobalOvfCtrl,
    kCpumMsrWrFn_Ia32PebsEnable,
    kCpumMsrWrFn_Ia32ClockModulation,
    kCpumMsrWrFn_Ia32ThermInterrupt,
    kCpumMsrWrFn_Ia32ThermStatus,
    kCpumMsrWrFn_Ia32Therm2Ctl,
    kCpumMsrWrFn_Ia32MiscEnable,
    kCpumMsrWrFn_Ia32McCtlStatusAddrMiscN,  /**< Takes bank number. */
    kCpumMsrWrFn_Ia32McNCtl2,               /**< Takes register number of start of range. */
    kCpumMsrWrFn_Ia32DsArea,
    kCpumMsrWrFn_Ia32TscDeadline,
    kCpumMsrWrFn_Ia32X2ApicN,
    kCpumMsrWrFn_Ia32DebugInterface,

    kCpumMsrWrFn_Amd64Efer,
    kCpumMsrWrFn_Amd64SyscallTarget,
    kCpumMsrWrFn_Amd64LongSyscallTarget,
    kCpumMsrWrFn_Amd64CompSyscallTarget,
    kCpumMsrWrFn_Amd64SyscallFlagMask,
    kCpumMsrWrFn_Amd64FsBase,
    kCpumMsrWrFn_Amd64GsBase,
    kCpumMsrWrFn_Amd64KernelGsBase,
    kCpumMsrWrFn_Amd64TscAux,
    kCpumMsrWrFn_IntelEblCrPowerOn,
    kCpumMsrWrFn_IntelP4EbcHardPowerOn,
    kCpumMsrWrFn_IntelP4EbcSoftPowerOn,
    kCpumMsrWrFn_IntelP4EbcFrequencyId,
    kCpumMsrWrFn_IntelFlexRatio,
    kCpumMsrWrFn_IntelPkgCStConfigControl,
    kCpumMsrWrFn_IntelPmgIoCaptureBase,
    kCpumMsrWrFn_IntelLastBranchFromToN,
    kCpumMsrWrFn_IntelLastBranchFromN,
    kCpumMsrWrFn_IntelLastBranchToN,
    kCpumMsrWrFn_IntelLastBranchTos,
    kCpumMsrWrFn_IntelBblCrCtl,
    kCpumMsrWrFn_IntelBblCrCtl3,
    kCpumMsrWrFn_IntelI7TemperatureTarget,
    kCpumMsrWrFn_IntelI7MsrOffCoreResponseN, /**< Takes register number. */
    kCpumMsrWrFn_IntelI7MiscPwrMgmt,
    kCpumMsrWrFn_IntelP6CrN,
    kCpumMsrWrFn_IntelCpuId1FeatureMaskEcdx,
    kCpumMsrWrFn_IntelCpuId1FeatureMaskEax,
    kCpumMsrWrFn_IntelCpuId80000001FeatureMaskEcdx,
    kCpumMsrWrFn_IntelI7SandyAesNiCtl,
    kCpumMsrWrFn_IntelI7TurboRatioLimit,
    kCpumMsrWrFn_IntelI7LbrSelect,
    kCpumMsrWrFn_IntelI7SandyErrorControl,
    kCpumMsrWrFn_IntelI7PowerCtl,
    kCpumMsrWrFn_IntelI7SandyPebsNumAlt,
    kCpumMsrWrFn_IntelI7PebsLdLat,
    kCpumMsrWrFn_IntelI7SandyVrCurrentConfig,
    kCpumMsrWrFn_IntelI7SandyVrMiscConfig,
    kCpumMsrWrFn_IntelI7SandyPkgCnIrtlN,
    kCpumMsrWrFn_IntelI7RaplPkgPowerLimit,
    kCpumMsrWrFn_IntelI7RaplDramPowerLimit,
    kCpumMsrWrFn_IntelI7RaplPp0PowerLimit,
    kCpumMsrWrFn_IntelI7RaplPp0Policy,
    kCpumMsrWrFn_IntelI7RaplPp1PowerLimit,
    kCpumMsrWrFn_IntelI7RaplPp1Policy,
    kCpumMsrWrFn_IntelI7IvyConfigTdpControl,
    kCpumMsrWrFn_IntelI7IvyTurboActivationRatio,
    kCpumMsrWrFn_IntelI7UncPerfGlobalCtrl,
    kCpumMsrWrFn_IntelI7UncPerfGlobalStatus,
    kCpumMsrWrFn_IntelI7UncPerfGlobalOvfCtrl,
    kCpumMsrWrFn_IntelI7UncPerfFixedCtrCtrl,
    kCpumMsrWrFn_IntelI7UncPerfFixedCtr,
    kCpumMsrWrFn_IntelI7UncArbPerfCtrN,
    kCpumMsrWrFn_IntelI7UncArbPerfEvtSelN,
    kCpumMsrWrFn_IntelCore2EmttmCrTablesN,
    kCpumMsrWrFn_IntelCore2SmmCStMiscInfo,
    kCpumMsrWrFn_IntelCore1ExtConfig,
    kCpumMsrWrFn_IntelCore1DtsCalControl,
    kCpumMsrWrFn_IntelCore2PeciControl,

    kCpumMsrWrFn_P6LastIntFromIp,
    kCpumMsrWrFn_P6LastIntToIp,

    kCpumMsrWrFn_AmdFam15hTscRate,
    kCpumMsrWrFn_AmdFam15hLwpCfg,
    kCpumMsrWrFn_AmdFam15hLwpCbAddr,
    kCpumMsrWrFn_AmdFam10hMc4MiscN,
    kCpumMsrWrFn_AmdK8PerfCtlN,
    kCpumMsrWrFn_AmdK8PerfCtrN,
    kCpumMsrWrFn_AmdK8SysCfg,
    kCpumMsrWrFn_AmdK8HwCr,
    kCpumMsrWrFn_AmdK8IorrBaseN,
    kCpumMsrWrFn_AmdK8IorrMaskN,
    kCpumMsrWrFn_AmdK8TopOfMemN,
    kCpumMsrWrFn_AmdK8NbCfg1,
    kCpumMsrWrFn_AmdK8McXcptRedir,
    kCpumMsrWrFn_AmdK8CpuNameN,
    kCpumMsrWrFn_AmdK8HwThermalCtrl,
    kCpumMsrWrFn_AmdK8SwThermalCtrl,
    kCpumMsrWrFn_AmdK8FidVidControl,
    kCpumMsrWrFn_AmdK8McCtlMaskN,
    kCpumMsrWrFn_AmdK8SmiOnIoTrapN,
    kCpumMsrWrFn_AmdK8SmiOnIoTrapCtlSts,
    kCpumMsrWrFn_AmdK8IntPendingMessage,
    kCpumMsrWrFn_AmdK8SmiTriggerIoCycle,
    kCpumMsrWrFn_AmdFam10hMmioCfgBaseAddr,
    kCpumMsrWrFn_AmdFam10hTrapCtlMaybe,
    kCpumMsrWrFn_AmdFam10hPStateControl,
    kCpumMsrWrFn_AmdFam10hPStateStatus,
    kCpumMsrWrFn_AmdFam10hPStateN,
    kCpumMsrWrFn_AmdFam10hCofVidControl,
    kCpumMsrWrFn_AmdFam10hCofVidStatus,
    kCpumMsrWrFn_AmdFam10hCStateIoBaseAddr,
    kCpumMsrWrFn_AmdFam10hCpuWatchdogTimer,
    kCpumMsrWrFn_AmdK8SmmBase,
    kCpumMsrWrFn_AmdK8SmmAddr,
    kCpumMsrWrFn_AmdK8SmmMask,
    kCpumMsrWrFn_AmdK8VmCr,
    kCpumMsrWrFn_AmdK8IgnNe,
    kCpumMsrWrFn_AmdK8SmmCtl,
    kCpumMsrWrFn_AmdK8VmHSavePa,
    kCpumMsrWrFn_AmdFam10hVmLockKey,
    kCpumMsrWrFn_AmdFam10hSmmLockKey,
    kCpumMsrWrFn_AmdFam10hLocalSmiStatus,
    kCpumMsrWrFn_AmdFam10hOsVisWrkIdLength,
    kCpumMsrWrFn_AmdFam10hOsVisWrkStatus,
    kCpumMsrWrFn_AmdFam16hL2IPerfCtlN,
    kCpumMsrWrFn_AmdFam16hL2IPerfCtrN,
    kCpumMsrWrFn_AmdFam15hNorthbridgePerfCtlN,
    kCpumMsrWrFn_AmdFam15hNorthbridgePerfCtrN,
    kCpumMsrWrFn_AmdK7MicrocodeCtl,
    kCpumMsrWrFn_AmdK7ClusterIdMaybe,
    kCpumMsrWrFn_AmdK8CpuIdCtlStd07hEbax,
    kCpumMsrWrFn_AmdK8CpuIdCtlStd06hEcx,
    kCpumMsrWrFn_AmdK8CpuIdCtlStd01hEdcx,
    kCpumMsrWrFn_AmdK8CpuIdCtlExt01hEdcx,
    kCpumMsrWrFn_AmdK8PatchLoader,
    kCpumMsrWrFn_AmdK7DebugStatusMaybe,
    kCpumMsrWrFn_AmdK7BHTraceBaseMaybe,
    kCpumMsrWrFn_AmdK7BHTracePtrMaybe,
    kCpumMsrWrFn_AmdK7BHTraceLimitMaybe,
    kCpumMsrWrFn_AmdK7HardwareDebugToolCfgMaybe,
    kCpumMsrWrFn_AmdK7FastFlushCountMaybe,
    kCpumMsrWrFn_AmdK7NodeId,
    kCpumMsrWrFn_AmdK7DrXAddrMaskN,      /**< Takes register index. */
    kCpumMsrWrFn_AmdK7Dr0DataMatchMaybe,
    kCpumMsrWrFn_AmdK7Dr0DataMaskMaybe,
    kCpumMsrWrFn_AmdK7LoadStoreCfg,
    kCpumMsrWrFn_AmdK7InstrCacheCfg,
    kCpumMsrWrFn_AmdK7DataCacheCfg,
    kCpumMsrWrFn_AmdK7BusUnitCfg,
    kCpumMsrWrFn_AmdK7DebugCtl2Maybe,
    kCpumMsrWrFn_AmdFam15hFpuCfg,
    kCpumMsrWrFn_AmdFam15hDecoderCfg,
    kCpumMsrWrFn_AmdFam10hBusUnitCfg2,
    kCpumMsrWrFn_AmdFam15hCombUnitCfg,
    kCpumMsrWrFn_AmdFam15hCombUnitCfg2,
    kCpumMsrWrFn_AmdFam15hCombUnitCfg3,
    kCpumMsrWrFn_AmdFam15hExecUnitCfg,
    kCpumMsrWrFn_AmdFam15hLoadStoreCfg2,
    kCpumMsrWrFn_AmdFam10hIbsFetchCtl,
    kCpumMsrWrFn_AmdFam10hIbsFetchLinAddr,
    kCpumMsrWrFn_AmdFam10hIbsFetchPhysAddr,
    kCpumMsrWrFn_AmdFam10hIbsOpExecCtl,
    kCpumMsrWrFn_AmdFam10hIbsOpRip,
    kCpumMsrWrFn_AmdFam10hIbsOpData,
    kCpumMsrWrFn_AmdFam10hIbsOpData2,
    kCpumMsrWrFn_AmdFam10hIbsOpData3,
    kCpumMsrWrFn_AmdFam10hIbsDcLinAddr,
    kCpumMsrWrFn_AmdFam10hIbsDcPhysAddr,
    kCpumMsrWrFn_AmdFam10hIbsCtl,
    kCpumMsrWrFn_AmdFam14hIbsBrTarget,

    /** End of valid MSR write function indexes. */
    kCpumMsrWrFn_End
} CPUMMSRWRFN;

/**
 * MSR range.
 */
typedef struct CPUMMSRRANGE
{
    /** The first MSR. [0] */
    uint32_t    uFirst;
    /** The last MSR. [4] */
    uint32_t    uLast;
    /** The read function (CPUMMSRRDFN). [8] */
    uint16_t    enmRdFn;
    /** The write function (CPUMMSRWRFN). [10] */
    uint16_t    enmWrFn;
    /** The offset of the 64-bit MSR value relative to the start of CPUMCPU.
     * UINT16_MAX if not used by the read and write functions.  [12] */
    uint16_t    offCpumCpu;
    /** Reserved for future hacks. [14] */
    uint16_t    fReserved;
    /** The init/read value. [16]
     * When enmRdFn is kCpumMsrRdFn_INIT_VALUE, this is the value returned on RDMSR.
     * offCpumCpu must be UINT16_MAX in that case, otherwise it must be a valid
     * offset into CPUM. */
    uint64_t    uValue;
    /** The bits to ignore when writing. [24]   */
    uint64_t    fWrIgnMask;
    /** The bits that will cause a GP(0) when writing. [32]
     * This is always checked prior to calling the write function.  Using
     * UINT64_MAX effectively marks the MSR as read-only. */
    uint64_t    fWrGpMask;
    /** The register name, if applicable. [40] */
    char        szName[56];

#ifdef VBOX_WITH_STATISTICS
    /** The number of reads. */
    STAMCOUNTER cReads;
    /** The number of writes. */
    STAMCOUNTER cWrites;
    /** The number of times ignored bits were written. */
    STAMCOUNTER cIgnoredBits;
    /** The number of GPs generated. */
    STAMCOUNTER cGps;
#endif
} CPUMMSRRANGE;
#ifdef VBOX_WITH_STATISTICS
AssertCompileSize(CPUMMSRRANGE, 128);
#else
AssertCompileSize(CPUMMSRRANGE, 96);
#endif
/** Pointer to an MSR range. */
typedef CPUMMSRRANGE *PCPUMMSRRANGE;
/** Pointer to a const MSR range. */
typedef CPUMMSRRANGE const *PCCPUMMSRRANGE;




/**
 * CPU features and quirks.
 * This is mostly exploded CPUID info.
 */
typedef struct CPUMFEATURES
{
    /** The CPU vendor (CPUMCPUVENDOR). */
    uint8_t         enmCpuVendor;
    /** The CPU family. */
    uint8_t         uFamily;
    /** The CPU model. */
    uint8_t         uModel;
    /** The CPU stepping. */
    uint8_t         uStepping;
    /** The microarchitecture. */
    CPUMMICROARCH   enmMicroarch;
    /** The maximum physical address with of the CPU. */
    uint8_t         cMaxPhysAddrWidth;
    /** Alignment padding.  */
    uint8_t         abPadding[3];

    /** Supports MSRs.  */
    uint32_t        fMsr : 1;
    /** Supports the page size extension (4/2 MB pages). */
    uint32_t        fPse : 1;
    /** Supports 36-bit page size extension (4 MB pages can map memory above
     *  4GB). */
    uint32_t        fPse36 : 1;
    /** Supports physical address extension (PAE).  */
    uint32_t        fPae : 1;
    /** Page attribute table (PAT) support (page level cache control). */
    uint32_t        fPat : 1;
    /** Supports the FXSAVE and FXRSTOR instructions. */
    uint32_t        fFxSaveRstor : 1;
    /** Intel SYSENTER/SYSEXIT support */
    uint32_t        fSysEnter : 1;
    /** First generation APIC. */
    uint32_t        fApic : 1;
    /** Second generation APIC. */
    uint32_t        fX2Apic : 1;
    /** Hypervisor present. */
    uint32_t        fHypervisorPresent : 1;
    /** MWAIT & MONITOR instructions supported. */
    uint32_t        fMonitorMWait : 1;

    /** AMD64: Supports long mode.  */
    uint32_t        fLongMode : 1;
    /** AMD64: SYSCALL/SYSRET support. */
    uint32_t        fSysCall : 1;
    /** AMD64: No-execute page table bit. */
    uint32_t        fNoExecute : 1;
    /** AMD64: Supports LAHF & SAHF instructions in 64-bit mode. */
    uint32_t        fLahfSahf : 1;
    /** AMD64: Supports RDTSCP. */
    uint32_t        fRdTscP : 1;

    /** Indicates that FPU instruction and data pointers may leak.
     * This generally applies to recent AMD CPUs, where the FPU IP and DP pointer
     * is only saved and restored if an exception is pending.   */
    uint32_t        fLeakyFxSR : 1;

    /** Alignment padding.  */
    uint32_t        fPadding : 9;

    uint64_t        auPadding[2];
} CPUMFEATURES;
AssertCompileSize(CPUMFEATURES, 32);
/** Pointer to a CPU feature structure. */
typedef CPUMFEATURES *PCPUMFEATURES;
/** Pointer to a const CPU feature structure. */
typedef CPUMFEATURES const *PCCPUMFEATURES;


/**
 * CPU info
 */
typedef struct CPUMINFO
{
    /** The number of MSR ranges (CPUMMSRRANGE) in the array pointed to below. */
    uint32_t                    cMsrRanges;
    /** Mask applied to ECX before looking up the MSR for a RDMSR/WRMSR
     * instruction.  Older hardware has been observed to ignore higher bits. */
    uint32_t                    fMsrMask;

    /** The number of CPUID leaves (CPUMCPUIDLEAF) in the array pointed to below. */
    uint32_t                    cCpuIdLeaves;
    /** The index of the first extended CPUID leaf in the array.
     *  Set to cCpuIdLeaves if none present. */
    uint32_t                    iFirstExtCpuIdLeaf;
    /** Alignment padding.  */
    uint32_t                    uPadding;
    /** How to handle unknown CPUID leaves. */
    CPUMUKNOWNCPUID             enmUnknownCpuIdMethod;
    /** For use with CPUMUKNOWNCPUID_DEFAULTS. */
    CPUMCPUID                   DefCpuId;

    /** Scalable bus frequency used for reporting other frequencies. */
    uint64_t                    uScalableBusFreq;

    /** Pointer to the MSR ranges (ring-0 pointer). */
    R0PTRTYPE(PCPUMMSRRANGE)    paMsrRangesR0;
    /** Pointer to the CPUID leaves (ring-0 pointer). */
    R0PTRTYPE(PCPUMCPUIDLEAF)   paCpuIdLeavesR0;

    /** Pointer to the MSR ranges (ring-3 pointer). */
    R3PTRTYPE(PCPUMMSRRANGE)    paMsrRangesR3;
    /** Pointer to the CPUID leaves (ring-3 pointer). */
    R3PTRTYPE(PCPUMCPUIDLEAF)   paCpuIdLeavesR3;

    /** Pointer to the MSR ranges (raw-mode context pointer). */
    RCPTRTYPE(PCPUMMSRRANGE)    paMsrRangesRC;
    /** Pointer to the CPUID leaves (raw-mode context pointer). */
    RCPTRTYPE(PCPUMCPUIDLEAF)   paCpuIdLeavesRC;
} CPUMINFO;
/** Pointer to a CPU info structure. */
typedef CPUMINFO *PCPUMINFO;
/** Pointer to a const CPU info structure. */
typedef CPUMINFO const *CPCPUMINFO;


/**
 * The saved host CPU state.
 *
 * @remark  The special VBOX_WITH_HYBRID_32BIT_KERNEL checks here are for the 10.4.x series
 *          of Mac OS X where the OS is essentially 32-bit but the cpu mode can be 64-bit.
 */
typedef struct CPUMHOSTCTX
{
    /** FPU state. (16-byte alignment)
     * @remark On x86, the format isn't necessarily X86FXSTATE (not important). */
    X86FXSTATE      fpu;

    /** General purpose register, selectors, flags and more
     * @{ */
#if HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    /** General purpose register ++
     * { */
    /*uint64_t        rax; - scratch*/
    uint64_t        rbx;
    /*uint64_t        rcx; - scratch*/
    /*uint64_t        rdx; - scratch*/
    uint64_t        rdi;
    uint64_t        rsi;
    uint64_t        rbp;
    uint64_t        rsp;
    /*uint64_t        r8; - scratch*/
    /*uint64_t        r9; - scratch*/
    uint64_t        r10;
    uint64_t        r11;
    uint64_t        r12;
    uint64_t        r13;
    uint64_t        r14;
    uint64_t        r15;
    /*uint64_t        rip; - scratch*/
    uint64_t        rflags;
#endif

#if HC_ARCH_BITS == 32
    /*uint32_t        eax; - scratch*/
    uint32_t        ebx;
    /*uint32_t        ecx; - scratch*/
    /*uint32_t        edx; - scratch*/
    uint32_t        edi;
    uint32_t        esi;
    uint32_t        ebp;
    X86EFLAGS       eflags;
    /*uint32_t        eip; - scratch*/
    /* lss pair! */
    uint32_t        esp;
#endif
    /** @} */

    /** Selector registers
     * @{ */
    RTSEL           ss;
    RTSEL           ssPadding;
    RTSEL           gs;
    RTSEL           gsPadding;
    RTSEL           fs;
    RTSEL           fsPadding;
    RTSEL           es;
    RTSEL           esPadding;
    RTSEL           ds;
    RTSEL           dsPadding;
    RTSEL           cs;
    RTSEL           csPadding;
    /** @} */

#if HC_ARCH_BITS == 32 && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    /** Control registers.
     * @{ */
    uint32_t        cr0;
    /*uint32_t        cr2; - scratch*/
    uint32_t        cr3;
    uint32_t        cr4;
    /** @} */

    /** Debug registers.
     * @{ */
    uint32_t        dr0;
    uint32_t        dr1;
    uint32_t        dr2;
    uint32_t        dr3;
    uint32_t        dr6;
    uint32_t        dr7;
    /** @} */

    /** Global Descriptor Table register. */
    X86XDTR32       gdtr;
    uint16_t        gdtrPadding;
    /** Interrupt Descriptor Table register. */
    X86XDTR32       idtr;
    uint16_t        idtrPadding;
    /** The task register. */
    RTSEL           ldtr;
    RTSEL           ldtrPadding;
    /** The task register. */
    RTSEL           tr;
    RTSEL           trPadding;
    uint32_t        SysEnterPadding;

    /** The sysenter msr registers.
     * This member is not used by the hypervisor context. */
    CPUMSYSENTER    SysEnter;

    /** MSRs
     * @{ */
    uint64_t        efer;
    /** @} */

    /* padding to get 64byte aligned size */
    uint8_t         auPadding[16+32];

#elif HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)

    /** Control registers.
     * @{ */
    uint64_t        cr0;
    /*uint64_t        cr2; - scratch*/
    uint64_t        cr3;
    uint64_t        cr4;
    uint64_t        cr8;
    /** @} */

    /** Debug registers.
     * @{ */
    uint64_t        dr0;
    uint64_t        dr1;
    uint64_t        dr2;
    uint64_t        dr3;
    uint64_t        dr6;
    uint64_t        dr7;
    /** @} */

    /** Global Descriptor Table register. */
    X86XDTR64       gdtr;
    uint16_t        gdtrPadding;
    /** Interrupt Descriptor Table register. */
    X86XDTR64       idtr;
    uint16_t        idtrPadding;
    /** The task register. */
    RTSEL           ldtr;
    RTSEL           ldtrPadding;
    /** The task register. */
    RTSEL           tr;
    RTSEL           trPadding;

    /** MSRs
     * @{ */
    CPUMSYSENTER    SysEnter;
    uint64_t        FSbase;
    uint64_t        GSbase;
    uint64_t        efer;
    /** @} */

    /* padding to get 32byte aligned size */
# ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    uint8_t         auPadding[16];
# else
    uint8_t         auPadding[8+32];
# endif

#else
# error HC_ARCH_BITS not defined
#endif
} CPUMHOSTCTX;
/** Pointer to the saved host CPU state. */
typedef CPUMHOSTCTX *PCPUMHOSTCTX;


/**
 * CPUM Data (part of VM)
 */
typedef struct CPUM
{
    /** Offset from CPUM to CPUMCPU for the first CPU. */
    uint32_t                offCPUMCPU0;

    /** Use flags.
     * These flags indicates which CPU features the host uses.
     */
    uint32_t                fHostUseFlags;

    /** Host CPU Features - ECX */
    struct
    {
        /** edx part */
        X86CPUIDFEATEDX     edx;
        /** ecx part */
        X86CPUIDFEATECX     ecx;
    } CPUFeatures;
    /** Host extended CPU features. */
    struct
    {
        /** edx part */
        uint32_t            edx;
        /** ecx part */
        uint32_t            ecx;
    } CPUFeaturesExt;

    /** CR4 mask */
    struct
    {
        uint32_t            AndMask; /**< @todo Move these to the per-CPU structure and fix the switchers. Saves a register! */
        uint32_t            OrMask;
    } CR4;

    /** The (more) portable CPUID level.  */
    uint8_t                 u8PortableCpuIdLevel;
    /** Indicates that a state restore is pending.
     * This is used to verify load order dependencies (PGM). */
    bool                    fPendingRestore;
    uint8_t                 abPadding[HC_ARCH_BITS == 64 ? 6 : 2];

    /** The standard set of CpuId leaves. */
    CPUMCPUID               aGuestCpuIdStd[6];
    /** The extended set of CpuId leaves. */
    CPUMCPUID               aGuestCpuIdExt[10];
    /** The centaur set of CpuId leaves. */
    CPUMCPUID               aGuestCpuIdCentaur[4];
    /** The hypervisor specific set of CpuId leaves. */
    CPUMCPUID               aGuestCpuIdHyper[4];
    /** The default set of CpuId leaves. */
    CPUMCPUID               GuestCpuIdDef;

#if HC_ARCH_BITS == 32
    uint8_t                 abPadding2[4];
#endif

    /** Guest CPU info. */
    CPUMINFO                GuestInfo;
    /** Guest CPU feature information. */
    CPUMFEATURES            GuestFeatures;
    /** Host CPU feature information. */
    CPUMFEATURES            HostFeatures;

    /** @name MSR statistics.
     * @{ */
    STAMCOUNTER             cMsrWrites;
    STAMCOUNTER             cMsrWritesToIgnoredBits;
    STAMCOUNTER             cMsrWritesRaiseGp;
    STAMCOUNTER             cMsrWritesUnknown;
    STAMCOUNTER             cMsrReads;
    STAMCOUNTER             cMsrReadsRaiseGp;
    STAMCOUNTER             cMsrReadsUnknown;
    /** @} */
} CPUM;
/** Pointer to the CPUM instance data residing in the shared VM structure. */
typedef CPUM *PCPUM;

/**
 * CPUM Data (part of VMCPU)
 */
typedef struct CPUMCPU
{
    /**
     * Hypervisor context.
     * Aligned on a 64-byte boundary.
     */
    CPUMCTX                 Hyper;

    /**
     * Saved host context. Only valid while inside GC.
     * Aligned on a 64-byte boundary.
     */
    CPUMHOSTCTX             Host;

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    uint8_t                 aMagic[56];
    uint64_t                uMagic;
#endif

    /**
     * Guest context.
     * Aligned on a 64-byte boundary.
     */
    CPUMCTX                 Guest;

    /**
     * Guest context - misc MSRs
     * Aligned on a 64-byte boundary.
     */
    CPUMCTXMSRS             GuestMsrs;

    /** Use flags.
     * These flags indicates both what is to be used and what has been used.
     */
    uint32_t                fUseFlags;

    /** Changed flags.
     * These flags indicates to REM (and others) which important guest
     * registers which has been changed since last time the flags were cleared.
     * See the CPUM_CHANGED_* defines for what we keep track of.
     */
    uint32_t                fChanged;

    /** Offset from CPUM to CPUMCPU. */
    uint32_t                offCPUM;

    /** Temporary storage for the return code of the function called in the
     * 32-64 switcher. */
    uint32_t                u32RetCode;

#ifdef VBOX_WITH_VMMR0_DISABLE_LAPIC_NMI
    /** The address of the APIC mapping, NULL if no APIC.
     * Call CPUMR0SetLApic to update this before doing a world switch. */
    RTHCPTR                 pvApicBase;
    /** Used by the world switcher code to store which vectors needs restoring on
     * the way back. */
    uint32_t                fApicDisVectors;
    /** Set if the CPU has the X2APIC mode enabled.
     * Call CPUMR0SetLApic to update this before doing a world switch. */
    bool                    fX2Apic;
#else
    uint8_t                 abPadding3[(HC_ARCH_BITS == 64 ? 8 : 4) + 4 + 1];
#endif

    /** Have we entered raw-mode? */
    bool                    fRawEntered;
    /** Have we entered the recompiler? */
    bool                    fRemEntered;

    /** Align the structure on a 64-byte boundary. */
    uint8_t                 abPadding2[64 - 16 - (HC_ARCH_BITS == 64 ? 8 : 4) - 4 - 1 - 2];
} CPUMCPU;
/** Pointer to the CPUMCPU instance data residing in the shared VMCPU structure. */
typedef CPUMCPU *PCPUMCPU;

#ifndef VBOX_FOR_DTRACE_LIB
RT_C_DECLS_BEGIN

PCPUMCPUIDLEAF      cpumCpuIdGetLeaf(PVM pVM, uint32_t uLeaf, uint32_t uSubLeaf);

#ifdef IN_RING3
int                 cpumR3DbgInit(PVM pVM);
PCPUMCPUIDLEAF      cpumR3CpuIdGetLeaf(PCPUMCPUIDLEAF paLeaves, uint32_t cLeaves, uint32_t uLeaf, uint32_t uSubLeaf);
bool                cpumR3CpuIdGetLeafLegacy(PCPUMCPUIDLEAF paLeaves, uint32_t cLeaves, uint32_t uLeaf, uint32_t uSubLeaf,
                                             PCPUMCPUID pLeagcy);
int                 cpumR3CpuIdInsert(PCPUMCPUIDLEAF *ppaLeaves, uint32_t *pcLeaves, PCPUMCPUIDLEAF pNewLeaf);
void                cpumR3CpuIdRemoveRange(PCPUMCPUIDLEAF paLeaves, uint32_t *pcLeaves, uint32_t uFirst, uint32_t uLast);
int                 cpumR3CpuIdExplodeFeatures(PCCPUMCPUIDLEAF paLeaves, uint32_t cLeaves, PCPUMFEATURES pFeatures);
int                 cpumR3DbGetCpuInfo(const char *pszName, PCPUMINFO pInfo);
int                 cpumR3MsrRangesInsert(PCPUMMSRRANGE *ppaMsrRanges, uint32_t *pcMsrRanges, PCCPUMMSRRANGE pNewRange);
int                 cpumR3MsrApplyFudge(PVM pVM);
int                 cpumR3MsrRegStats(PVM pVM);
int                 cpumR3MsrStrictInitChecks(void);
PCPUMMSRRANGE       cpumLookupMsrRange(PVM pVM, uint32_t idMsr);
#endif

#ifdef IN_RC
DECLASM(int)        cpumHandleLazyFPUAsm(PCPUMCPU pCPUM);
#endif

#ifdef IN_RING0
DECLASM(int)        cpumR0SaveHostRestoreGuestFPUState(PCPUMCPU pCPUM);
DECLASM(int)        cpumR0SaveGuestRestoreHostFPUState(PCPUMCPU pCPUM);
DECLASM(int)        cpumR0SaveHostFPUState(PCPUMCPU pCPUM);
DECLASM(int)        cpumR0RestoreHostFPUState(PCPUMCPU pCPUM);
DECLASM(void)       cpumR0LoadFPU(PCPUMCTX pCtx);
DECLASM(void)       cpumR0SaveFPU(PCPUMCTX pCtx);
DECLASM(void)       cpumR0LoadXMM(PCPUMCTX pCtx);
DECLASM(void)       cpumR0SaveXMM(PCPUMCTX pCtx);
DECLASM(void)       cpumR0SetFCW(uint16_t u16FCW);
DECLASM(uint16_t)   cpumR0GetFCW(void);
DECLASM(void)       cpumR0SetMXCSR(uint32_t u32MXCSR);
DECLASM(uint32_t)   cpumR0GetMXCSR(void);
DECLASM(void)       cpumR0LoadDRx(uint64_t const *pa4Regs);
DECLASM(void)       cpumR0SaveDRx(uint64_t *pa4Regs);
#endif

RT_C_DECLS_END
#endif /* !VBOX_FOR_DTRACE_LIB */

/** @} */

#endif

