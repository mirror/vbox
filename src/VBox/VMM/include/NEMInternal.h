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

RT_C_DECLS_BEGIN


/** @defgroup grp_nem_int      Internal
 * @ingroup grp_nem
 * @internal
 * @{
 */

/**
 * NEM VM Instance data.
 */
typedef struct NEM
{
    /** NEM_MAGIC. */
    uint32_t                    u32Magic;

    /** Set if enabled. */
    bool                        fEnabled;
#ifdef RT_OS_WINDOWS
    /** Set if we've created the EMTs. */
    bool                        fCreatedEmts;
    /** WHvRunVpExitReasonX64Cpuid is supported. */
    bool                        fExtendedMsrExit;
    /** WHvRunVpExitReasonX64MsrAccess is supported. */
    bool                        fExtendedCpuIdExit;
    /** WHvRunVpExitReasonException is supported. */
    bool                        fExtendedXcptExit;
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
#endif

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

} NEMCPU;
/** Pointer to NEM VMCPU instance data. */
typedef NEMCPU *PNEMCPU;

/** NEMCPU::u32Magic value. */
#define NEMCPU_MAGIC            UINT32_C(0x4d454e20)
/** NEMCPU::u32Magic value after termination. */
#define NEMCPU_MAGIC_DEAD       UINT32_C(0xdead2222)

#ifdef IN_RING3
int     nemR3NativeInit(PVM pVM, bool fFallback, bool fForced);
int     nemR3NativeInitAfterCPUM(PVM pVM);
int     nemR3NativeInitCompleted(PVM pVM, VMINITCOMPLETED enmWhat);
int     nemR3NativeTerm(PVM pVM);
void    nemR3NativeReset(PVM pVM);
void    nemR3NativeResetCpu(PVMCPU pVCpu);
int     nemR3NativeNotifyPhysRamRegister(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb);
int     nemR3NativeNotifyPhysMmioExMap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags, void *pvMmio2);
int     nemR3NativeNotifyPhysMmioExUnmap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags);
int     nemR3NativeNotifyPhysRomRegisterEarly(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags);
int     nemR3NativeNotifyPhysRomRegisterLate(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags);
void    nemR3NativeNotifySetA20(PVMCPU pVCpu, bool fEnabled);
/* NEMHCNotifyXxxx for ring-3: */
void    nemR3NativeNotifyHandlerPhysicalRegister(PVM pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhys, RTGCPHYS cb);
void    nemR3NativeNotifyHandlerPhysicalDeregister(PVM pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhys, RTGCPHYS cb,
                                                   int fRestoreAsRAM, bool fRestoreAsRAM2);
void    nemR3NativeNotifyHandlerPhysicalModify(PVM pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhysOld,
                                               RTGCPHYS GCPhysNew, RTGCPHYS cb, bool fRestoreAsRAM);
int     nemR3NativeNotifyPhysPageAllocated(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, uint32_t fPageProt,
                                           PGMPAGETYPE enmType, uint8_t *pu2State);
void    nemR3NativeNotifyPhysPageProtChanged(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, uint32_t fPageProt,
                                             PGMPAGETYPE enmType, uint8_t *pu2State);
void    nemR3NativeNotifyPhysPageChanged(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhysPrev, RTHCPHYS HCPhysNew, uint32_t fPageProt,
                                         PGMPAGETYPE enmType, uint8_t *pu2State);
#endif


/** @} */

RT_C_DECLS_END

#endif

