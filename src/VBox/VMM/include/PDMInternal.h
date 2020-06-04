/* $Id$ */
/** @file
 * PDM - Internal header file.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VMM_INCLUDED_SRC_include_PDMInternal_h
#define VMM_INCLUDED_SRC_include_PDMInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/types.h>
#include <VBox/param.h>
#include <VBox/vmm/cfgm.h>
#include <VBox/vmm/stam.h>
#include <VBox/vusb.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/pdmasynccompletion.h>
#ifdef VBOX_WITH_NETSHAPER
# include <VBox/vmm/pdmnetshaper.h>
#endif
#ifdef VBOX_WITH_PDM_ASYNC_COMPLETION
# include <VBox/vmm/pdmasynccompletion.h>
#endif
#include <VBox/vmm/pdmblkcache.h>
#include <VBox/vmm/pdmcommon.h>
#include <VBox/vmm/pdmtask.h>
#include <VBox/sup.h>
#include <VBox/msi.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#ifdef IN_RING3
# include <iprt/thread.h>
#endif

RT_C_DECLS_BEGIN


/** @defgroup grp_pdm_int       Internal
 * @ingroup grp_pdm
 * @internal
 * @{
 */

/** @def PDM_WITH_R3R0_CRIT_SECT
 * Enables or disabled ring-3/ring-0 critical sections. */
#if defined(DOXYGEN_RUNNING) || 1
# define PDM_WITH_R3R0_CRIT_SECT
#endif

/** @def PDMCRITSECT_STRICT
 * Enables/disables PDM critsect strictness like deadlock detection. */
#if (defined(RT_LOCK_STRICT) && defined(IN_RING3) && !defined(PDMCRITSECT_STRICT)) \
  || defined(DOXYGEN_RUNNING)
# define PDMCRITSECT_STRICT
#endif

/** @def PDMCRITSECT_STRICT
 * Enables/disables PDM read/write critsect strictness like deadlock
 * detection. */
#if (defined(RT_LOCK_STRICT) && defined(IN_RING3) && !defined(PDMCRITSECTRW_STRICT)) \
  || defined(DOXYGEN_RUNNING)
# define PDMCRITSECTRW_STRICT
#endif

/** The maximum device instance (total) size, ring-0/raw-mode capable devices. */
#define PDM_MAX_DEVICE_INSTANCE_SIZE      _4M
/** The maximum device instance (total) size, ring-3 only devices. */
#define PDM_MAX_DEVICE_INSTANCE_SIZE_R3   _8M
/** The maximum size for the DBGF tracing tracking structure allocated for each device. */
#define PDM_MAX_DEVICE_DBGF_TRACING_TRACK PAGE_SIZE



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/** Pointer to a PDM Device. */
typedef struct PDMDEV *PPDMDEV;
/** Pointer to a pointer to a PDM Device. */
typedef PPDMDEV *PPPDMDEV;

/** Pointer to a PDM USB Device. */
typedef struct PDMUSB *PPDMUSB;
/** Pointer to a pointer to a PDM USB Device. */
typedef PPDMUSB *PPPDMUSB;

/** Pointer to a PDM Driver. */
typedef struct PDMDRV *PPDMDRV;
/** Pointer to a pointer to a PDM Driver. */
typedef PPDMDRV *PPPDMDRV;

/** Pointer to a PDM Logical Unit. */
typedef struct PDMLUN *PPDMLUN;
/** Pointer to a pointer to a PDM Logical Unit. */
typedef PPDMLUN *PPPDMLUN;

/** Pointer to a PDM PCI Bus instance. */
typedef struct PDMPCIBUS *PPDMPCIBUS;
/** Pointer to a PDM IOMMU instance. */
typedef struct PDMIOMMU *PPDMIOMMU;
/** Pointer to a DMAC instance. */
typedef struct PDMDMAC *PPDMDMAC;
/** Pointer to a RTC instance. */
typedef struct PDMRTC *PPDMRTC;

/** Pointer to an USB HUB registration record. */
typedef struct PDMUSBHUB *PPDMUSBHUB;

/**
 * Supported asynchronous completion endpoint classes.
 */
typedef enum PDMASYNCCOMPLETIONEPCLASSTYPE
{
    /** File class. */
    PDMASYNCCOMPLETIONEPCLASSTYPE_FILE = 0,
    /** Number of supported classes. */
    PDMASYNCCOMPLETIONEPCLASSTYPE_MAX,
    /** 32bit hack. */
    PDMASYNCCOMPLETIONEPCLASSTYPE_32BIT_HACK = 0x7fffffff
} PDMASYNCCOMPLETIONEPCLASSTYPE;


/**
 * MMIO/IO port registration tracking structure for DBGF tracing.
 */
typedef struct PDMDEVINSDBGFTRACK
{
    /** Flag whether this tracks a IO port or MMIO registration. */
    bool                                fMmio;
    /** Opaque user data passed during registration. */
    void                                *pvUser;
    /** Type dependent data. */
    union
    {
        /** I/O port  registration. */
        struct
        {
            /** IOM I/O port handle. */
            IOMIOPORTHANDLE             hIoPorts;
            /** Original OUT handler of the device. */
            PFNIOMIOPORTNEWOUT          pfnOut;
            /** Original IN handler of the device. */
            PFNIOMIOPORTNEWIN           pfnIn;
            /** Original string OUT handler of the device. */
            PFNIOMIOPORTNEWOUTSTRING    pfnOutStr;
            /** Original string IN handler of the device. */
            PFNIOMIOPORTNEWINSTRING     pfnInStr;
        } IoPort;
        /** MMIO registration. */
        struct
        {
            /** IOM MMIO region handle. */
            IOMMMIOHANDLE               hMmioRegion;
            /** Original MMIO write handler of the device. */
            PFNIOMMMIONEWWRITE          pfnWrite;
            /** Original MMIO read handler of the device. */
            PFNIOMMMIONEWREAD           pfnRead;
            /** Original MMIO fill handler of the device. */
            PFNIOMMMIONEWFILL           pfnFill;
        } Mmio;
    } u;
} PDMDEVINSDBGFTRACK;
/** Pointer to a MMIO/IO port registration tracking structure. */
typedef PDMDEVINSDBGFTRACK *PPDMDEVINSDBGFTRACK;
/** Pointer to a const MMIO/IO port registration tracking structure. */
typedef const PDMDEVINSDBGFTRACK *PCPDMDEVINSDBGFTRACK;


/**
 * Private device instance data, ring-3.
 */
typedef struct PDMDEVINSINTR3
{
    /** Pointer to the next instance.
     * (Head is pointed to by PDM::pDevInstances.) */
    R3PTRTYPE(PPDMDEVINS)           pNextR3;
    /** Pointer to the next per device instance.
     * (Head is pointed to by PDMDEV::pInstances.) */
    R3PTRTYPE(PPDMDEVINS)           pPerDeviceNextR3;
    /** Pointer to device structure. */
    R3PTRTYPE(PPDMDEV)              pDevR3;
    /** Pointer to the list of logical units associated with the device. (FIFO) */
    R3PTRTYPE(PPDMLUN)              pLunsR3;
    /** Pointer to the asynchronous notification callback set while in
     * FNPDMDEVSUSPEND or FNPDMDEVPOWEROFF. */
    R3PTRTYPE(PFNPDMDEVASYNCNOTIFY) pfnAsyncNotify;
    /** Configuration handle to the instance node. */
    R3PTRTYPE(PCFGMNODE)            pCfgHandle;

    /** R3 pointer to the VM this instance was created for. */
    PVMR3                           pVMR3;
    /** DBGF trace event source handle if tracing is configured. */
    DBGFTRACEREVTSRC                hDbgfTraceEvtSrc;
    /** Pointer to the base of the page containing the DBGF tracing tracking structures. */
    PPDMDEVINSDBGFTRACK             paDbgfTraceTrack;
    /** Index of the next entry to use for tracking. */
    uint32_t                        idxDbgfTraceTrackNext;
    /** Maximum number of records fitting into the single page. */
    uint32_t                        cDbgfTraceTrackMax;

    /** Flags, see PDMDEVINSINT_FLAGS_XXX. */
    uint32_t                        fIntFlags;
    /** The last IRQ tag (for tracing it thru clearing). */
    uint32_t                        uLastIrqTag;
    /** The ring-0 device index (for making ring-0 calls). */
    uint32_t                        idxR0Device;
} PDMDEVINSINTR3;


/**
 * Private device instance data, ring-0.
 */
typedef struct PDMDEVINSINTR0
{
    /** Pointer to the VM this instance was created for. */
    R0PTRTYPE(PGVM)                 pGVM;
    /** Pointer to device structure. */
    R0PTRTYPE(struct PDMDEVREGR0 const *) pRegR0;
    /** The ring-0 module reference. */
    RTR0PTR                         hMod;
    /** Pointer to the ring-0 mapping of the ring-3 internal data (for uLastIrqTag). */
    R0PTRTYPE(PDMDEVINSINTR3 *)     pIntR3R0;
    /** Pointer to the ring-0 mapping of the ring-3 instance (for idTracing). */
    R0PTRTYPE(struct PDMDEVINSR3 *) pInsR3R0;
    /** DBGF trace event source handle if tracing is configured. */
    DBGFTRACEREVTSRC                hDbgfTraceEvtSrc;
    /** The device instance memory. */
    RTR0MEMOBJ                      hMemObj;
    /** The ring-3 mapping object. */
    RTR0MEMOBJ                      hMapObj;
    /** The page memory object for tracking MMIO and I/O port registrations when tracing is configured. */
    RTR0MEMOBJ                      hDbgfTraceObj;
    /** Pointer to the base of the page containing the DBGF tracing tracking structures. */
    PPDMDEVINSDBGFTRACK             paDbgfTraceTrack;
    /** Index of the next entry to use for tracking. */
    uint32_t                        idxDbgfTraceTrackNext;
    /** Maximum number of records fitting into the single page. */
    uint32_t                        cDbgfTraceTrackMax;
    /** Index into PDMR0PERVM::apDevInstances. */
    uint32_t                        idxR0Device;
} PDMDEVINSINTR0;


/**
 * Private device instance data, raw-mode
 */
typedef struct PDMDEVINSINTRC
{
    /** Pointer to the VM this instance was created for. */
    RGPTRTYPE(PVM)                  pVMRC;
} PDMDEVINSINTRC;


/**
 * Private device instance data.
 */
typedef struct PDMDEVINSINT
{
    /** Pointer to the next instance (HC Ptr).
     * (Head is pointed to by PDM::pDevInstances.) */
    R3PTRTYPE(PPDMDEVINS)           pNextR3;
    /** Pointer to the next per device instance (HC Ptr).
     * (Head is pointed to by PDMDEV::pInstances.) */
    R3PTRTYPE(PPDMDEVINS)           pPerDeviceNextR3;
    /** Pointer to device structure - HC Ptr. */
    R3PTRTYPE(PPDMDEV)              pDevR3;
    /** Pointer to the list of logical units associated with the device. (FIFO) */
    R3PTRTYPE(PPDMLUN)              pLunsR3;
    /** Pointer to the asynchronous notification callback set while in
     * FNPDMDEVSUSPEND or FNPDMDEVPOWEROFF. */
    R3PTRTYPE(PFNPDMDEVASYNCNOTIFY) pfnAsyncNotify;
    /** Configuration handle to the instance node. */
    R3PTRTYPE(PCFGMNODE)            pCfgHandle;

    /** R3 pointer to the VM this instance was created for. */
    PVMR3                           pVMR3;

    /** R0 pointer to the VM this instance was created for. */
    R0PTRTYPE(PVMCC)                pVMR0;

    /** RC pointer to the VM this instance was created for. */
    PVMRC                           pVMRC;

    /** Flags, see PDMDEVINSINT_FLAGS_XXX. */
    uint32_t                        fIntFlags;
    /** The last IRQ tag (for tracing it thru clearing). */
    uint32_t                        uLastIrqTag;
} PDMDEVINSINT;

/** @name PDMDEVINSINT::fIntFlags
 * @{ */
/** Used by pdmR3Load to mark device instances it found in the saved state. */
#define PDMDEVINSINT_FLAGS_FOUND            RT_BIT_32(0)
/** Indicates that the device hasn't been powered on or resumed.
 * This is used by PDMR3PowerOn, PDMR3Resume, PDMR3Suspend and PDMR3PowerOff
 * to make sure each device gets exactly one notification for each of those
 * events.  PDMR3Resume and PDMR3PowerOn also makes use of it to bail out on
 * a failure (already resumed/powered-on devices are suspended).
 * PDMR3PowerOff resets this flag once before going through the devices to make sure
 * every device gets the power off notification even if it was suspended before with
 * PDMR3Suspend.
 */
#define PDMDEVINSINT_FLAGS_SUSPENDED        RT_BIT_32(1)
/** Indicates that the device has been reset already.  Used by PDMR3Reset. */
#define PDMDEVINSINT_FLAGS_RESET            RT_BIT_32(2)
#define PDMDEVINSINT_FLAGS_R0_ENABLED       RT_BIT_32(3)
#define PDMDEVINSINT_FLAGS_RC_ENABLED       RT_BIT_32(4)
/** Set if we've called the ring-0 constructor. */
#define PDMDEVINSINT_FLAGS_R0_CONTRUCT      RT_BIT_32(5)
/** Set if using non-default critical section.  */
#define PDMDEVINSINT_FLAGS_CHANGED_CRITSECT RT_BIT_32(6)
/** @} */


/**
 * Private USB device instance data.
 */
typedef struct PDMUSBINSINT
{
    /** The UUID of this instance. */
    RTUUID                          Uuid;
    /** Pointer to the next instance.
     * (Head is pointed to by PDM::pUsbInstances.) */
    R3PTRTYPE(PPDMUSBINS)           pNext;
    /** Pointer to the next per USB device instance.
     * (Head is pointed to by PDMUSB::pInstances.) */
    R3PTRTYPE(PPDMUSBINS)           pPerDeviceNext;

    /** Pointer to device structure. */
    R3PTRTYPE(PPDMUSB)              pUsbDev;

    /** Pointer to the VM this instance was created for. */
    PVMR3                           pVM;
    /** Pointer to the list of logical units associated with the device. (FIFO) */
    R3PTRTYPE(PPDMLUN)              pLuns;
    /** The per instance device configuration. */
    R3PTRTYPE(PCFGMNODE)            pCfg;
    /** Same as pCfg if the configuration should be deleted when detaching the device. */
    R3PTRTYPE(PCFGMNODE)            pCfgDelete;
    /** The global device configuration. */
    R3PTRTYPE(PCFGMNODE)            pCfgGlobal;

    /** Pointer to the USB hub this device is attached to.
     * This is NULL if the device isn't connected to any HUB. */
    R3PTRTYPE(PPDMUSBHUB)           pHub;
    /** The port number that we're connected to. */
    uint32_t                        iPort;
    /** Indicates that the USB device hasn't been powered on or resumed.
     * See PDMDEVINSINT_FLAGS_SUSPENDED. */
    bool                            fVMSuspended;
    /** Indicates that the USB device has been reset. */
    bool                            fVMReset;
    /** Pointer to the asynchronous notification callback set while in
     * FNPDMDEVSUSPEND or FNPDMDEVPOWEROFF. */
    R3PTRTYPE(PFNPDMUSBASYNCNOTIFY) pfnAsyncNotify;
} PDMUSBINSINT;


/**
 * Private driver instance data.
 */
typedef struct PDMDRVINSINT
{
    /** Pointer to the driver instance above.
     * This is NULL for the topmost drive. */
    R3PTRTYPE(PPDMDRVINS)           pUp;
    /** Pointer to the driver instance below.
     * This is NULL for the bottommost driver. */
    R3PTRTYPE(PPDMDRVINS)           pDown;
    /** Pointer to the logical unit this driver chained on. */
    R3PTRTYPE(PPDMLUN)              pLun;
    /** Pointer to driver structure from which this was instantiated. */
    R3PTRTYPE(PPDMDRV)              pDrv;
    /** Pointer to the VM this instance was created for, ring-3 context. */
    PVMR3                           pVMR3;
    /** Pointer to the VM this instance was created for, ring-0 context. */
    R0PTRTYPE(PVMCC)                pVMR0;
    /** Pointer to the VM this instance was created for, raw-mode context. */
    PVMRC                           pVMRC;
    /** Flag indicating that the driver is being detached and destroyed.
     * (Helps detect potential recursive detaching.) */
    bool                            fDetaching;
    /** Indicates that the driver hasn't been powered on or resumed.
     * See PDMDEVINSINT_FLAGS_SUSPENDED. */
    bool                            fVMSuspended;
    /** Indicates that the driver has been reset already. */
    bool                            fVMReset;
    /** Set if allocated on the hyper heap, false if on the ring-3 heap. */
    bool                            fHyperHeap;
    /** Pointer to the asynchronous notification callback set while in
     * PDMUSBREG::pfnVMSuspend or PDMUSBREG::pfnVMPowerOff. */
    R3PTRTYPE(PFNPDMDRVASYNCNOTIFY) pfnAsyncNotify;
    /** Configuration handle to the instance node. */
    R3PTRTYPE(PCFGMNODE)            pCfgHandle;
    /** Pointer to the ring-0 request handler function. */
    PFNPDMDRVREQHANDLERR0           pfnReqHandlerR0;
} PDMDRVINSINT;


/**
 * Private critical section data.
 */
typedef struct PDMCRITSECTINT
{
    /** The critical section core which is shared with IPRT.
     * @note The semaphore is a SUPSEMEVENT.  */
    RTCRITSECT                      Core;
    /** Pointer to the next critical section.
     * This chain is used for relocating pVMRC and device cleanup. */
    R3PTRTYPE(struct PDMCRITSECTINT *) pNext;
    /** Owner identifier.
     * This is pDevIns if the owner is a device. Similarly for a driver or service.
     * PDMR3CritSectInit() sets this to point to the critsect itself. */
    RTR3PTR                         pvKey;
    /** Pointer to the VM - R3Ptr. */
    PVMR3                           pVMR3;
    /** Pointer to the VM - R0Ptr. */
    R0PTRTYPE(PVMCC)                pVMR0;
    /** Pointer to the VM - GCPtr. */
    PVMRC                           pVMRC;
    /** Set if this critical section is the automatically created default
     * section of a device. */
    bool                            fAutomaticDefaultCritsect;
    /** Set if the critical section is used by a timer or similar.
     * See PDMR3DevGetCritSect.  */
    bool                            fUsedByTimerOrSimilar;
    /** Alignment padding. */
    bool                            afPadding[2];
    /** Support driver event semaphore that is scheduled to be signaled upon leaving
     * the critical section. This is only for Ring-3 and Ring-0. */
    SUPSEMEVENT                     hEventToSignal;
    /** The lock name. */
    R3PTRTYPE(const char *)         pszName;
    /** R0/RC lock contention. */
    STAMCOUNTER                     StatContentionRZLock;
    /** R0/RC unlock contention. */
    STAMCOUNTER                     StatContentionRZUnlock;
    /** R3 lock contention. */
    STAMCOUNTER                     StatContentionR3;
    /** Profiling the time the section is locked. */
    STAMPROFILEADV                  StatLocked;
} PDMCRITSECTINT;
AssertCompileMemberAlignment(PDMCRITSECTINT, StatContentionRZLock, 8);
/** Pointer to private critical section data. */
typedef PDMCRITSECTINT *PPDMCRITSECTINT;

/** Indicates that the critical section is queued for unlock.
 * PDMCritSectIsOwner and PDMCritSectIsOwned optimizations. */
#define PDMCRITSECT_FLAGS_PENDING_UNLOCK    RT_BIT_32(17)


/**
 * Private critical section data.
 */
typedef struct PDMCRITSECTRWINT
{
    /** The read/write critical section core which is shared with IPRT.
     * @note The semaphores are SUPSEMEVENT and SUPSEMEVENTMULTI.  */
    RTCRITSECTRW                        Core;

    /** Pointer to the next critical section.
     * This chain is used for relocating pVMRC and device cleanup. */
    R3PTRTYPE(struct PDMCRITSECTRWINT *) pNext;
    /** Owner identifier.
     * This is pDevIns if the owner is a device. Similarly for a driver or service.
     * PDMR3CritSectInit() sets this to point to the critsect itself. */
    RTR3PTR                             pvKey;
    /** Pointer to the VM - R3Ptr. */
    PVMR3                               pVMR3;
    /** Pointer to the VM - R0Ptr. */
    R0PTRTYPE(PVMCC)                    pVMR0;
    /** Pointer to the VM - GCPtr. */
    PVMRC                               pVMRC;
#if HC_ARCH_BITS == 64
    /** Alignment padding. */
    RTRCPTR                             RCPtrPadding;
#endif
    /** The lock name. */
    R3PTRTYPE(const char *)             pszName;
    /** R0/RC write lock contention. */
    STAMCOUNTER                         StatContentionRZEnterExcl;
    /** R0/RC write unlock contention. */
    STAMCOUNTER                         StatContentionRZLeaveExcl;
    /** R0/RC read lock contention. */
    STAMCOUNTER                         StatContentionRZEnterShared;
    /** R0/RC read unlock contention. */
    STAMCOUNTER                         StatContentionRZLeaveShared;
    /** R0/RC writes. */
    STAMCOUNTER                         StatRZEnterExcl;
    /** R0/RC reads. */
    STAMCOUNTER                         StatRZEnterShared;
    /** R3 write lock contention. */
    STAMCOUNTER                         StatContentionR3EnterExcl;
    /** R3 read lock contention. */
    STAMCOUNTER                         StatContentionR3EnterShared;
    /** R3 writes. */
    STAMCOUNTER                         StatR3EnterExcl;
    /** R3 reads. */
    STAMCOUNTER                         StatR3EnterShared;
    /** Profiling the time the section is write locked. */
    STAMPROFILEADV                      StatWriteLocked;
} PDMCRITSECTRWINT;
AssertCompileMemberAlignment(PDMCRITSECTRWINT, StatContentionRZEnterExcl, 8);
AssertCompileMemberAlignment(PDMCRITSECTRWINT, Core.u64State, 8);
/** Pointer to private critical section data. */
typedef PDMCRITSECTRWINT *PPDMCRITSECTRWINT;



/**
 * The usual device/driver/internal/external stuff.
 */
typedef enum
{
    /** The usual invalid entry. */
    PDMTHREADTYPE_INVALID = 0,
    /** Device type. */
    PDMTHREADTYPE_DEVICE,
    /** USB Device type. */
    PDMTHREADTYPE_USB,
    /** Driver type. */
    PDMTHREADTYPE_DRIVER,
    /** Internal type. */
    PDMTHREADTYPE_INTERNAL,
    /** External type. */
    PDMTHREADTYPE_EXTERNAL,
    /** The usual 32-bit hack. */
    PDMTHREADTYPE_32BIT_HACK = 0x7fffffff
} PDMTHREADTYPE;


/**
 * The internal structure for the thread.
 */
typedef struct PDMTHREADINT
{
    /** The VM pointer. */
    PVMR3                           pVM;
    /** The event semaphore the thread blocks on when not running. */
    RTSEMEVENTMULTI                 BlockEvent;
    /** The event semaphore the thread sleeps on while running. */
    RTSEMEVENTMULTI                 SleepEvent;
    /** Pointer to the next thread. */
    R3PTRTYPE(struct PDMTHREAD *)   pNext;
    /** The thread type. */
    PDMTHREADTYPE                   enmType;
} PDMTHREADINT;



/* Must be included after PDMDEVINSINT is defined. */
#define PDMDEVINSINT_DECLARED
#define PDMUSBINSINT_DECLARED
#define PDMDRVINSINT_DECLARED
#define PDMCRITSECTINT_DECLARED
#define PDMCRITSECTRWINT_DECLARED
#define PDMTHREADINT_DECLARED
#ifdef ___VBox_pdm_h
# error "Invalid header PDM order. Include PDMInternal.h before VBox/vmm/pdm.h!"
#endif
RT_C_DECLS_END
#include <VBox/vmm/pdm.h>
RT_C_DECLS_BEGIN

/**
 * PDM Logical Unit.
 *
 * This typically the representation of a physical port on a
 * device, like for instance the PS/2 keyboard port on the
 * keyboard controller device. The LUNs are chained on the
 * device they belong to (PDMDEVINSINT::pLunsR3).
 */
typedef struct PDMLUN
{
    /** The LUN - The Logical Unit Number. */
    RTUINT                          iLun;
    /** Pointer to the next LUN. */
    PPDMLUN                         pNext;
    /** Pointer to the top driver in the driver chain. */
    PPDMDRVINS                      pTop;
    /** Pointer to the bottom driver in the driver chain. */
    PPDMDRVINS                      pBottom;
    /** Pointer to the device instance which the LUN belongs to.
     * Either this is set or pUsbIns is set. Both is never set at the same time. */
    PPDMDEVINS                      pDevIns;
    /** Pointer to the USB device instance which the LUN belongs to. */
    PPDMUSBINS                      pUsbIns;
    /** Pointer to the device base interface. */
    PPDMIBASE                       pBase;
    /** Description of this LUN. */
    const char                     *pszDesc;
} PDMLUN;


/**
 * PDM Device, ring-3.
 */
typedef struct PDMDEV
{
    /** Pointer to the next device (R3 Ptr). */
    R3PTRTYPE(PPDMDEV)              pNext;
    /** Device name length. (search optimization) */
    uint32_t                        cchName;
    /** Registration structure. */
    R3PTRTYPE(const struct PDMDEVREGR3 *) pReg;
    /** Number of instances. */
    uint32_t                        cInstances;
    /** Pointer to chain of instances (R3 Ptr). */
    PPDMDEVINSR3                    pInstances;
    /** The search path for raw-mode context modules (';' as separator). */
    char                           *pszRCSearchPath;
    /** The search path for ring-0 context modules (';' as separator). */
    char                           *pszR0SearchPath;
} PDMDEV;


#if 0
/**
 * PDM Device, ring-0.
 */
typedef struct PDMDEVR0
{
    /** Pointer to the next device. */
    R0PTRTYPE(PPDMDEVR0)            pNext;
    /** Device name length. (search optimization) */
    uint32_t                        cchName;
    /** Registration structure. */
    R3PTRTYPE(const struct PDMDEVREGR0 *) pReg;
    /** Number of instances. */
    uint32_t                        cInstances;
    /** Pointer to chain of instances. */
    PPDMDEVINSR0                    pInstances;
} PDMDEVR0;
#endif


/**
 * PDM USB Device.
 */
typedef struct PDMUSB
{
    /** Pointer to the next device (R3 Ptr). */
    R3PTRTYPE(PPDMUSB)              pNext;
    /** Device name length. (search optimization) */
    RTUINT                          cchName;
    /** Registration structure. */
    R3PTRTYPE(const struct PDMUSBREG *) pReg;
    /** Next instance number. */
    uint32_t                        iNextInstance;
    /** Pointer to chain of instances (R3 Ptr). */
    R3PTRTYPE(PPDMUSBINS)           pInstances;
} PDMUSB;


/**
 * PDM Driver.
 */
typedef struct PDMDRV
{
    /** Pointer to the next device. */
    PPDMDRV                         pNext;
    /** Registration structure. */
    const struct PDMDRVREG *        pReg;
    /** Current number of instances. */
    uint32_t                        cInstances;
    /** The next instance number. */
    uint32_t                        iNextInstance;
    /** The search path for raw-mode context modules (';' as separator). */
    char                           *pszRCSearchPath;
    /** The search path for ring-0 context modules (';' as separator). */
    char                           *pszR0SearchPath;
} PDMDRV;


/**
 * PDM registered IOMMU device.
 */
typedef struct PDMIOMMU
{
    /** IOMMU index. */
    uint32_t                    idxIommu;
    uint32_t                    uPadding0; /**< Alignment padding.*/

    /** Pointer to the IOMMU device instance - R3. */
    PPDMDEVINSR3                pDevInsR3;
    /** @copydoc PDMIOMMUREGR3::pfnMemRead */
    DECLR3CALLBACKMEMBER(int,   pfnMemRead,(PPDMDEVINS pDevIns, uint16_t uDevId, uint64_t uDva, size_t cbRead,
                                            PRTGCPHYS pGCPhysSpa));
    /** @copydoc PDMIOMMUREGR3::pfnMemWrite */
    DECLR3CALLBACKMEMBER(int,   pfnMemWrite,(PPDMDEVINS pDevIns, uint16_t uDevId, uint64_t uDva, size_t cbWrite,
                                             PRTGCPHYS pGCPhysSpa));
    /** @copydoc PDMIOMMUREGR3::pfnMsiRemap */
    DECLR3CALLBACKMEMBER(int,   pfnMsiRemap,(PPDMDEVINS pDevIns, uint16_t uDevId, PCMSIMSG pMsiIn, PMSIMSG pMsiOut));
} PDMIOMMU;


/**
 * Ring-0 PDM IOMMU instance data.
 */
typedef struct PDMIOMMUR0
{
    /** IOMMU index. */
    uint32_t                    idxIommu;
    uint32_t                    uPadding0; /**< Alignment padding.*/

    /** Pointer to IOMMU device instance. */
    PPDMDEVINSR0                pDevInsR0;
    /** @copydoc PDMIOMMUREGR0::pfnMemRead */
    DECLR0CALLBACKMEMBER(int,   pfnMemRead,(PPDMDEVINS pDevIns, uint16_t uDevId, uint64_t uDva, size_t cbRead,
                                            PRTGCPHYS pGCPhysSpa));
    /** @copydoc PDMIOMMUREGR3::pfnMemWrite */
    DECLR0CALLBACKMEMBER(int,   pfnMemWrite,(PPDMDEVINS pDevIns, uint16_t uDevId, uint64_t uDva, size_t cbWrite,
                                             PRTGCPHYS pGCPhysSpa));
    /** @copydoc PDMIOMMUREGR3::pfnMsiRemap */
    DECLR0CALLBACKMEMBER(int,   pfnMsiRemap,(PPDMDEVINS pDevIns, uint16_t uDevId, PCMSIMSG pMsiIn, PMSIMSG pMsiOut));
} PDMIOMMUR0;
/** Pointer to a ring-0 IOMMU data. */
typedef PDMIOMMUR0 *PPDMIOMMUR0;
/** Pointer to a const ring-0 IOMMU data. */
typedef const PDMIOMMUR0 *PCPDMIOMMUR0;


/**
 * PDM registered PIC device.
 */
typedef struct PDMPIC
{
    /** Pointer to the PIC device instance - R3. */
    PPDMDEVINSR3                    pDevInsR3;
    /** @copydoc PDMPICREG::pfnSetIrq */
    DECLR3CALLBACKMEMBER(void,      pfnSetIrqR3,(PPDMDEVINS pDevIns, int iIrq, int iLevel, uint32_t uTagSrc));
    /** @copydoc PDMPICREG::pfnGetInterrupt */
    DECLR3CALLBACKMEMBER(int,       pfnGetInterruptR3,(PPDMDEVINS pDevIns, uint32_t *puTagSrc));

    /** Pointer to the PIC device instance - R0. */
    PPDMDEVINSR0                    pDevInsR0;
    /** @copydoc PDMPICREG::pfnSetIrq */
    DECLR0CALLBACKMEMBER(void,      pfnSetIrqR0,(PPDMDEVINS pDevIns, int iIrq, int iLevel, uint32_t uTagSrc));
    /** @copydoc PDMPICREG::pfnGetInterrupt */
    DECLR0CALLBACKMEMBER(int,       pfnGetInterruptR0,(PPDMDEVINS pDevIns, uint32_t *puTagSrc));

    /** Pointer to the PIC device instance - RC. */
    PPDMDEVINSRC                    pDevInsRC;
    /** @copydoc PDMPICREG::pfnSetIrq */
    DECLRCCALLBACKMEMBER(void,      pfnSetIrqRC,(PPDMDEVINS pDevIns, int iIrq, int iLevel, uint32_t uTagSrc));
    /** @copydoc PDMPICREG::pfnGetInterrupt */
    DECLRCCALLBACKMEMBER(int,       pfnGetInterruptRC,(PPDMDEVINS pDevIns, uint32_t *puTagSrc));
    /** Alignment padding. */
    RTRCPTR                         RCPtrPadding;
} PDMPIC;


/**
 * PDM registered APIC device.
 */
typedef struct PDMAPIC
{
    /** Pointer to the APIC device instance - R3 Ptr. */
    PPDMDEVINSR3                       pDevInsR3;
    /** Pointer to the APIC device instance - R0 Ptr. */
    PPDMDEVINSR0                       pDevInsR0;
    /** Pointer to the APIC device instance - RC Ptr. */
    PPDMDEVINSRC                       pDevInsRC;
    uint8_t                            Alignment[4];
} PDMAPIC;


/**
 * PDM registered I/O APIC device.
 */
typedef struct PDMIOAPIC
{
    /** Pointer to the APIC device instance - R3 Ptr. */
    PPDMDEVINSR3                    pDevInsR3;
    /** @copydoc PDMIOAPICREG::pfnSetIrq */
    DECLR3CALLBACKMEMBER(void,      pfnSetIrqR3,(PPDMDEVINS pDevIns, int iIrq, int iLevel, uint32_t uTagSrc));
    /** @copydoc PDMIOAPICREG::pfnSendMsi */
    DECLR3CALLBACKMEMBER(void,      pfnSendMsiR3,(PPDMDEVINS pDevIns, RTGCPHYS GCAddr, uint32_t uValue, uint32_t uTagSrc));
    /** @copydoc PDMIOAPICREG::pfnSetEoi */
    DECLR3CALLBACKMEMBER(VBOXSTRICTRC, pfnSetEoiR3,(PPDMDEVINS pDevIns, uint8_t u8Vector));

    /** Pointer to the PIC device instance - R0. */
    PPDMDEVINSR0                    pDevInsR0;
    /** @copydoc PDMIOAPICREG::pfnSetIrq */
    DECLR0CALLBACKMEMBER(void,      pfnSetIrqR0,(PPDMDEVINS pDevIns, int iIrq, int iLevel, uint32_t uTagSrc));
    /** @copydoc PDMIOAPICREG::pfnSendMsi */
    DECLR0CALLBACKMEMBER(void,      pfnSendMsiR0,(PPDMDEVINS pDevIns, RTGCPHYS GCAddr, uint32_t uValue, uint32_t uTagSrc));
    /** @copydoc PDMIOAPICREG::pfnSetEoi */
    DECLR0CALLBACKMEMBER(VBOXSTRICTRC, pfnSetEoiR0,(PPDMDEVINS pDevIns, uint8_t u8Vector));

    /** Pointer to the APIC device instance - RC Ptr. */
    PPDMDEVINSRC                    pDevInsRC;
    /** @copydoc PDMIOAPICREG::pfnSetIrq */
    DECLRCCALLBACKMEMBER(void,      pfnSetIrqRC,(PPDMDEVINS pDevIns, int iIrq, int iLevel, uint32_t uTagSrc));
     /** @copydoc PDMIOAPICREG::pfnSendMsi */
    DECLRCCALLBACKMEMBER(void,      pfnSendMsiRC,(PPDMDEVINS pDevIns, RTGCPHYS GCAddr, uint32_t uValue, uint32_t uTagSrc));
     /** @copydoc PDMIOAPICREG::pfnSendMsi */
    DECLRCCALLBACKMEMBER(VBOXSTRICTRC, pfnSetEoiRC,(PPDMDEVINS pDevIns, uint8_t u8Vector));
} PDMIOAPIC;

/** Maximum number of PCI busses for a VM. */
#define PDM_PCI_BUSSES_MAX 8
/** Maximum number of IOMMUs (at most one per PCI bus). */
#define PDM_IOMMUS_MAX     PDM_PCI_BUSSES_MAX


#ifdef IN_RING3
/**
 * PDM registered firmware device.
 */
typedef struct PDMFW
{
    /** Pointer to the firmware device instance. */
    PPDMDEVINSR3                    pDevIns;
    /** Copy of the registration structure. */
    PDMFWREG                        Reg;
} PDMFW;
/** Pointer to a firmware instance. */
typedef PDMFW *PPDMFW;
#endif


/**
 * PDM PCI bus instance.
 */
typedef struct PDMPCIBUS
{
    /** PCI bus number. */
    uint32_t                   iBus;
    uint32_t                   uPadding0; /**< Alignment padding.*/

    /** Pointer to PCI bus device instance. */
    PPDMDEVINSR3               pDevInsR3;
    /** @copydoc PDMPCIBUSREGR3::pfnSetIrqR3 */
    DECLR3CALLBACKMEMBER(void, pfnSetIrqR3,(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, int iIrq, int iLevel, uint32_t uTagSrc));

    /** @copydoc PDMPCIBUSREGR3::pfnRegisterR3 */
    DECLR3CALLBACKMEMBER(int,  pfnRegister,(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, uint32_t fFlags,
                                            uint8_t uPciDevNo, uint8_t uPciFunNo, const char *pszName));
    /** @copydoc PDMPCIBUSREGR3::pfnRegisterMsiR3 */
    DECLR3CALLBACKMEMBER(int,  pfnRegisterMsi,(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, PPDMMSIREG pMsiReg));
    /** @copydoc PDMPCIBUSREGR3::pfnIORegionRegisterR3 */
    DECLR3CALLBACKMEMBER(int,  pfnIORegionRegister,(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, uint32_t iRegion,
                                                    RTGCPHYS cbRegion, PCIADDRESSSPACE enmType, uint32_t fFlags,
                                                    uint64_t hHandle, PFNPCIIOREGIONMAP pfnCallback));
    /** @copydoc PDMPCIBUSREGR3::pfnInterceptConfigAccesses */
    DECLR3CALLBACKMEMBER(void, pfnInterceptConfigAccesses,(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev,
                                                           PFNPCICONFIGREAD pfnRead, PFNPCICONFIGWRITE pfnWrite));
    /** @copydoc PDMPCIBUSREGR3::pfnConfigWrite */
    DECLR3CALLBACKMEMBER(VBOXSTRICTRC, pfnConfigWrite,(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev,
                                                       uint32_t uAddress, unsigned cb, uint32_t u32Value));
    /** @copydoc PDMPCIBUSREGR3::pfnConfigRead */
    DECLR3CALLBACKMEMBER(VBOXSTRICTRC, pfnConfigRead,(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev,
                                                      uint32_t uAddress, unsigned cb, uint32_t *pu32Value));
} PDMPCIBUS;


/**
 * Ring-0 PDM PCI bus instance data.
 */
typedef struct PDMPCIBUSR0
{
    /** PCI bus number. */
    uint32_t                   iBus;
    uint32_t                   uPadding0; /**< Alignment padding.*/
    /** Pointer to PCI bus device instance. */
    PPDMDEVINSR0               pDevInsR0;
    /** @copydoc PDMPCIBUSREGR0::pfnSetIrq */
    DECLR0CALLBACKMEMBER(void, pfnSetIrqR0,(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, int iIrq, int iLevel, uint32_t uTagSrc));
} PDMPCIBUSR0;
/** Pointer to the ring-0 PCI bus data. */
typedef PDMPCIBUSR0 *PPDMPCIBUSR0;


#ifdef IN_RING3
/**
 * PDM registered DMAC (DMA Controller) device.
 */
typedef struct PDMDMAC
{
    /** Pointer to the DMAC device instance. */
    PPDMDEVINSR3                    pDevIns;
    /** Copy of the registration structure. */
    PDMDMACREG                      Reg;
} PDMDMAC;


/**
 * PDM registered RTC (Real Time Clock) device.
 */
typedef struct PDMRTC
{
    /** Pointer to the RTC device instance. */
    PPDMDEVINSR3                    pDevIns;
    /** Copy of the registration structure. */
    PDMRTCREG                       Reg;
} PDMRTC;

#endif /* IN_RING3 */

/**
 * Module type.
 */
typedef enum PDMMODTYPE
{
    /** Raw-mode (RC) context module. */
    PDMMOD_TYPE_RC,
    /** Ring-0 (host) context module. */
    PDMMOD_TYPE_R0,
    /** Ring-3 (host) context module. */
    PDMMOD_TYPE_R3
} PDMMODTYPE;


/** The module name length including the terminator. */
#define PDMMOD_NAME_LEN             32

/**
 * Loaded module instance.
 */
typedef struct PDMMOD
{
    /** Module name. This is used for referring to
     * the module internally, sort of like a handle. */
    char                            szName[PDMMOD_NAME_LEN];
    /** Module type. */
    PDMMODTYPE                      eType;
    /** Loader module handle. Not used for R0 modules. */
    RTLDRMOD                        hLdrMod;
    /** Loaded address.
     * This is the 'handle' for R0 modules. */
    RTUINTPTR                       ImageBase;
    /** Old loaded address.
     * This is used during relocation of GC modules. Not used for R0 modules. */
    RTUINTPTR                       OldImageBase;
    /** Where the R3 HC bits are stored.
     * This can be equal to ImageBase but doesn't have to. Not used for R0 modules. */
    void                           *pvBits;

    /** Pointer to next module. */
    struct PDMMOD                  *pNext;
    /** Module filename. */
    char                            szFilename[1];
} PDMMOD;
/** Pointer to loaded module instance. */
typedef PDMMOD *PPDMMOD;



/** Extra space in the free array. */
#define PDMQUEUE_FREE_SLACK         16

/**
 * Queue type.
 */
typedef enum PDMQUEUETYPE
{
    /** Device consumer. */
    PDMQUEUETYPE_DEV = 1,
    /** Driver consumer. */
    PDMQUEUETYPE_DRV,
    /** Internal consumer. */
    PDMQUEUETYPE_INTERNAL,
    /** External consumer. */
    PDMQUEUETYPE_EXTERNAL
} PDMQUEUETYPE;

/** Pointer to a PDM Queue. */
typedef struct PDMQUEUE *PPDMQUEUE;

/**
 * PDM Queue.
 */
typedef struct PDMQUEUE
{
    /** Pointer to the next queue in the list. */
    R3PTRTYPE(PPDMQUEUE)            pNext;
    /** Type specific data. */
    union
    {
        /** PDMQUEUETYPE_DEV */
        struct
        {
            /** Pointer to consumer function. */
            R3PTRTYPE(PFNPDMQUEUEDEV)   pfnCallback;
            /** Pointer to the device instance owning the queue. */
            R3PTRTYPE(PPDMDEVINS)       pDevIns;
        } Dev;
        /** PDMQUEUETYPE_DRV */
        struct
        {
            /** Pointer to consumer function. */
            R3PTRTYPE(PFNPDMQUEUEDRV)   pfnCallback;
            /** Pointer to the driver instance owning the queue. */
            R3PTRTYPE(PPDMDRVINS)       pDrvIns;
        } Drv;
        /** PDMQUEUETYPE_INTERNAL */
        struct
        {
            /** Pointer to consumer function. */
            R3PTRTYPE(PFNPDMQUEUEINT)   pfnCallback;
        } Int;
        /** PDMQUEUETYPE_EXTERNAL */
        struct
        {
            /** Pointer to consumer function. */
            R3PTRTYPE(PFNPDMQUEUEEXT)   pfnCallback;
            /** Pointer to user argument. */
            R3PTRTYPE(void *)           pvUser;
        } Ext;
    } u;
    /** Queue type. */
    PDMQUEUETYPE                    enmType;
    /** The interval between checking the queue for events.
     * The realtime timer below is used to do the waiting.
     * If 0, the queue will use the VM_FF_PDM_QUEUE forced action. */
    uint32_t                        cMilliesInterval;
    /** Interval timer. Only used if cMilliesInterval is non-zero. */
    PTMTIMERR3                      pTimer;
    /** Pointer to the VM - R3. */
    PVMR3                           pVMR3;
    /** LIFO of pending items - R3. */
    R3PTRTYPE(PPDMQUEUEITEMCORE) volatile pPendingR3;
    /** Pointer to the VM - R0. */
    PVMR0                           pVMR0;
    /** LIFO of pending items - R0. */
    R0PTRTYPE(PPDMQUEUEITEMCORE) volatile pPendingR0;
    /** Pointer to the GC VM and indicator for GC enabled queue.
     * If this is NULL, the queue cannot be used in GC.
     */
    PVMRC                           pVMRC;
    /** LIFO of pending items - GC. */
    RCPTRTYPE(PPDMQUEUEITEMCORE) volatile pPendingRC;

    /** Item size (bytes). */
    uint32_t                        cbItem;
    /** Number of items in the queue. */
    uint32_t                        cItems;
    /** Index to the free head (where we insert). */
    uint32_t volatile               iFreeHead;
    /** Index to the free tail (where we remove). */
    uint32_t volatile               iFreeTail;

    /** Unique queue name. */
    R3PTRTYPE(const char *)         pszName;
#if HC_ARCH_BITS == 32
    RTR3PTR                         Alignment1;
#endif
    /** Stat: Times PDMQueueAlloc fails. */
    STAMCOUNTER                     StatAllocFailures;
    /** Stat: PDMQueueInsert calls. */
    STAMCOUNTER                     StatInsert;
    /** Stat: Queue flushes. */
    STAMCOUNTER                     StatFlush;
    /** Stat: Queue flushes with pending items left over. */
    STAMCOUNTER                     StatFlushLeftovers;
#ifdef VBOX_WITH_STATISTICS
    /** State: Profiling the flushing. */
    STAMPROFILE                     StatFlushPrf;
    /** State: Pending items. */
    uint32_t volatile               cStatPending;
    uint32_t volatile               cAlignment;
#endif

    /** Array of pointers to free items. Variable size. */
    struct PDMQUEUEFREEITEM
    {
        /** Pointer to the free item - HC Ptr. */
        R3PTRTYPE(PPDMQUEUEITEMCORE) volatile   pItemR3;
        /** Pointer to the free item - HC Ptr. */
        R0PTRTYPE(PPDMQUEUEITEMCORE) volatile   pItemR0;
        /** Pointer to the free item - GC Ptr. */
        RCPTRTYPE(PPDMQUEUEITEMCORE) volatile   pItemRC;
#if HC_ARCH_BITS == 64
        RTRCPTR                                 Alignment0;
#endif
    }                               aFreeItems[1];
} PDMQUEUE;

/** @name PDM::fQueueFlushing
 * @{ */
/** Used to make sure only one EMT will flush the queues.
 * Set when an EMT is flushing queues, clear otherwise.  */
#define PDM_QUEUE_FLUSH_FLAG_ACTIVE_BIT     0
/** Indicating there are queues with items pending.
 * This is make sure we don't miss inserts happening during flushing.  The FF
 * cannot be used for this since it has to be cleared immediately to prevent
 * other EMTs from spinning. */
#define PDM_QUEUE_FLUSH_FLAG_PENDING_BIT    1
/** @}  */


/** @name PDM task structures.
 * @{ */

/**
 * A asynchronous user mode task.
 */
typedef struct PDMTASK
{
    /** Task owner type. */
    PDMTASKTYPE volatile        enmType;
    /** Queue flags. */
    uint32_t volatile           fFlags;
    /** User argument for the callback. */
    R3PTRTYPE(void *) volatile  pvUser;
    /** The callback (will be cast according to enmType before callout). */
    R3PTRTYPE(PFNRT) volatile   pfnCallback;
    /** The owner identifier. */
    R3PTRTYPE(void *) volatile  pvOwner;
    /** Task name. */
    R3PTRTYPE(const char *)     pszName;
    /** Number of times already triggered when PDMTaskTrigger was called. */
    uint32_t volatile           cAlreadyTrigged;
    /** Number of runs. */
    uint32_t                    cRuns;
} PDMTASK;
/** Pointer to a PDM task. */
typedef PDMTASK *PPDMTASK;

/**
 * A task set.
 *
 * This is served by one task executor thread.
 */
typedef struct PDMTASKSET
{
    /** Magic value (PDMTASKSET_MAGIC). */
    uint32_t                u32Magic;
    /** Set if this task set works for ring-0 and raw-mode. */
    bool                    fRZEnabled;
    /** Number of allocated taks. */
    uint8_t volatile        cAllocated;
    /** Base handle value for this set. */
    uint16_t                uHandleBase;
    /** The task executor thread. */
    R3PTRTYPE(RTTHREAD)     hThread;
    /** Event semaphore for waking up the thread when fRZEnabled is set. */
    SUPSEMEVENT             hEventR0;
    /** Event semaphore for waking up the thread when fRZEnabled is clear. */
    R3PTRTYPE(RTSEMEVENT)   hEventR3;
    /** The VM pointer.   */
    PVM                     pVM;
    /** Padding so fTriggered is in its own cacheline. */
    uint64_t                au64Padding2[3];

    /** Bitmask of triggered tasks. */
    uint64_t volatile       fTriggered;
    /** Shutdown thread indicator. */
    bool volatile           fShutdown;
    /** Padding.   */
    bool volatile           afPadding3[3];
    /** Task currently running, UINT32_MAX if idle. */
    uint32_t volatile       idxRunning;
    /** Padding so fTriggered and fShutdown are in their own cacheline. */
    uint64_t volatile       au64Padding3[6];

    /** The individual tasks.  (Unallocated tasks have NULL pvOwner.)  */
    PDMTASK                 aTasks[64];
} PDMTASKSET;
AssertCompileMemberAlignment(PDMTASKSET, fTriggered, 64);
AssertCompileMemberAlignment(PDMTASKSET, aTasks, 64);
/** Magic value for PDMTASKSET::u32Magic. */
#define PDMTASKSET_MAGIC        UINT32_C(0x19320314)
/** Pointer to a task set. */
typedef PDMTASKSET *PPDMTASKSET;

/** @} */


/**
 * Queue device helper task operation.
 */
typedef enum PDMDEVHLPTASKOP
{
    /** The usual invalid 0 entry. */
    PDMDEVHLPTASKOP_INVALID = 0,
    /** ISASetIrq */
    PDMDEVHLPTASKOP_ISA_SET_IRQ,
    /** PCISetIrq */
    PDMDEVHLPTASKOP_PCI_SET_IRQ,
    /** PCISetIrq */
    PDMDEVHLPTASKOP_IOAPIC_SET_IRQ,
    /** The usual 32-bit hack. */
    PDMDEVHLPTASKOP_32BIT_HACK = 0x7fffffff
} PDMDEVHLPTASKOP;

/**
 * Queued Device Helper Task.
 */
typedef struct PDMDEVHLPTASK
{
    /** The queue item core (don't touch). */
    PDMQUEUEITEMCORE                Core;
    /** Pointer to the device instance (R3 Ptr). */
    PPDMDEVINSR3                    pDevInsR3;
    /** This operation to perform. */
    PDMDEVHLPTASKOP                 enmOp;
#if HC_ARCH_BITS == 64
    uint32_t                        Alignment0;
#endif
    /** Parameters to the operation. */
    union PDMDEVHLPTASKPARAMS
    {
        /**
         * PDMDEVHLPTASKOP_ISA_SET_IRQ and PDMDEVHLPTASKOP_IOAPIC_SET_IRQ.
         */
        struct PDMDEVHLPTASKISASETIRQ
        {
            /** The IRQ */
            int                     iIrq;
            /** The new level. */
            int                     iLevel;
            /** The IRQ tag and source. */
            uint32_t                uTagSrc;
        } IsaSetIRQ, IoApicSetIRQ;

        /**
         * PDMDEVHLPTASKOP_PCI_SET_IRQ
         */
        struct PDMDEVHLPTASKPCISETIRQ
        {
            /** Pointer to the PCI device (R3 Ptr). */
            R3PTRTYPE(PPDMPCIDEV)   pPciDevR3;
            /** The IRQ */
            int                     iIrq;
            /** The new level. */
            int                     iLevel;
            /** The IRQ tag and source. */
            uint32_t                uTagSrc;
        } PciSetIRQ;

        /** Expanding the structure. */
        uint64_t    au64[3];
    } u;
} PDMDEVHLPTASK;
/** Pointer to a queued Device Helper Task. */
typedef PDMDEVHLPTASK *PPDMDEVHLPTASK;
/** Pointer to a const queued Device Helper Task. */
typedef const PDMDEVHLPTASK *PCPDMDEVHLPTASK;



/**
 * An USB hub registration record.
 */
typedef struct PDMUSBHUB
{
    /** The USB versions this hub support.
     * Note that 1.1 hubs can take on 2.0 devices. */
    uint32_t                        fVersions;
    /** The number of ports on the hub. */
    uint32_t                        cPorts;
    /** The number of available ports (0..cPorts). */
    uint32_t                        cAvailablePorts;
    /** The driver instance of the hub. */
    PPDMDRVINS                      pDrvIns;
    /** Copy of the to the registration structure. */
    PDMUSBHUBREG                    Reg;

    /** Pointer to the next hub in the list. */
    struct PDMUSBHUB               *pNext;
} PDMUSBHUB;

/** Pointer to a const USB HUB registration record. */
typedef const PDMUSBHUB *PCPDMUSBHUB;

/** Pointer to a PDM Async I/O template. */
typedef struct PDMASYNCCOMPLETIONTEMPLATE *PPDMASYNCCOMPLETIONTEMPLATE;

/** Pointer to the main PDM Async completion endpoint class. */
typedef struct PDMASYNCCOMPLETIONEPCLASS *PPDMASYNCCOMPLETIONEPCLASS;

/** Pointer to the global block cache structure. */
typedef struct PDMBLKCACHEGLOBAL *PPDMBLKCACHEGLOBAL;

/**
 * PDM VMCPU Instance data.
 * Changes to this must checked against the padding of the pdm union in VMCPU!
 */
typedef struct PDMCPU
{
    /** The number of entries in the apQueuedCritSectsLeaves table that's currently
     * in use. */
    uint32_t                        cQueuedCritSectLeaves;
    uint32_t                        uPadding0; /**< Alignment padding.*/
    /** Critical sections queued in RC/R0 because of contention preventing leave to
     * complete. (R3 Ptrs)
     * We will return to Ring-3 ASAP, so this queue doesn't have to be very long. */
    R3PTRTYPE(PPDMCRITSECT)         apQueuedCritSectLeaves[8];

    /** The number of entries in the apQueuedCritSectRwExclLeaves table that's
     * currently in use. */
    uint32_t                        cQueuedCritSectRwExclLeaves;
    uint32_t                        uPadding1; /**< Alignment padding.*/
    /** Read/write critical sections queued in RC/R0 because of contention
     * preventing exclusive leave to complete. (R3 Ptrs)
     * We will return to Ring-3 ASAP, so this queue doesn't have to be very long. */
    R3PTRTYPE(PPDMCRITSECTRW)       apQueuedCritSectRwExclLeaves[8];

    /** The number of entries in the apQueuedCritSectsRwShrdLeaves table that's
     * currently in use. */
    uint32_t                        cQueuedCritSectRwShrdLeaves;
    uint32_t                        uPadding2; /**< Alignment padding.*/
    /** Read/write critical sections queued in RC/R0 because of contention
     * preventing shared leave to complete. (R3 Ptrs)
     * We will return to Ring-3 ASAP, so this queue doesn't have to be very long. */
    R3PTRTYPE(PPDMCRITSECTRW)       apQueuedCritSectRwShrdLeaves[8];
} PDMCPU;


/**
 * PDM VM Instance data.
 * Changes to this must checked against the padding of the cfgm union in VM!
 */
typedef struct PDM
{
    /** The PDM lock.
     * This is used to protect everything that deals with interrupts, i.e.
     * the PIC, APIC, IOAPIC and PCI devices plus some PDM functions. */
    PDMCRITSECT                     CritSect;
    /** The NOP critical section.
     * This is a dummy critical section that will not do any thread
     * serialization but instead let all threads enter immediately and
     * concurrently. */
    PDMCRITSECT                     NopCritSect;

    /** The ring-0 capable task sets (max 128). */
    PDMTASKSET                      aTaskSets[2];
    /** Pointer to task sets (max 512). */
    R3PTRTYPE(PPDMTASKSET)          apTaskSets[8];

    /** PCI Buses. */
    PDMPCIBUS                       aPciBuses[PDM_PCI_BUSSES_MAX];
    /** IOMMU devices. */
    PDMIOMMU                        aIommus[PDM_IOMMUS_MAX];
    /** The register PIC device. */
    PDMPIC                          Pic;
    /** The registered APIC device. */
    PDMAPIC                         Apic;
    /** The registered I/O APIC device. */
    PDMIOAPIC                       IoApic;
    /** The registered HPET device. */
    PPDMDEVINSR3                    pHpet;

    /** List of registered devices. (FIFO) */
    R3PTRTYPE(PPDMDEV)              pDevs;
    /** List of devices instances. (FIFO) */
    R3PTRTYPE(PPDMDEVINS)           pDevInstances;
    /** List of registered USB devices. (FIFO) */
    R3PTRTYPE(PPDMUSB)              pUsbDevs;
    /** List of USB devices instances. (FIFO) */
    R3PTRTYPE(PPDMUSBINS)           pUsbInstances;
    /** List of registered drivers. (FIFO) */
    R3PTRTYPE(PPDMDRV)              pDrvs;
    /** The registered firmware device (can be NULL). */
    R3PTRTYPE(PPDMFW)               pFirmware;
    /** The registered DMAC device. */
    R3PTRTYPE(PPDMDMAC)             pDmac;
    /** The registered RTC device. */
    R3PTRTYPE(PPDMRTC)              pRtc;
    /** The registered USB HUBs. (FIFO) */
    R3PTRTYPE(PPDMUSBHUB)           pUsbHubs;

    /** @name Queues
     * @{ */
    /** Queue in which devhlp tasks are queued for R3 execution - R3 Ptr. */
    R3PTRTYPE(PPDMQUEUE)            pDevHlpQueueR3;
    /** Queue in which devhlp tasks are queued for R3 execution - R0 Ptr. */
    R0PTRTYPE(PPDMQUEUE)            pDevHlpQueueR0;
    /** Queue in which devhlp tasks are queued for R3 execution - RC Ptr. */
    RCPTRTYPE(PPDMQUEUE)            pDevHlpQueueRC;
    /** Pointer to the queue which should be manually flushed - RC Ptr.
     * Only touched by EMT. */
    RCPTRTYPE(struct PDMQUEUE *)    pQueueFlushRC;
    /** Pointer to the queue which should be manually flushed - R0 Ptr.
     * Only touched by EMT. */
    R0PTRTYPE(struct PDMQUEUE *)    pQueueFlushR0;
    /** Bitmask controlling the queue flushing.
     * See PDM_QUEUE_FLUSH_FLAG_ACTIVE and PDM_QUEUE_FLUSH_FLAG_PENDING. */
    uint32_t volatile               fQueueFlushing;
    /** @} */

    /** The current IRQ tag (tracing purposes). */
    uint32_t volatile               uIrqTag;

    /** Pending reset flags (PDMVMRESET_F_XXX). */
    uint32_t volatile               fResetFlags;

    /** Set by pdmR3LoadExec for use in assertions. */
    bool                            fStateLoaded;
    /** Alignment padding. */
    bool                            afPadding[3];

    /** The tracing ID of the next device instance.
     *
     * @remarks We keep the device tracing ID seperate from the rest as these are
     *          then more likely to end up with the same ID from one run to
     *          another, making analysis somewhat easier.  Drivers and USB devices
     *          are more volatile and can be changed at runtime, thus these are much
     *          less likely to remain stable, so just heap them all together. */
    uint32_t                        idTracingDev;
    /** The tracing ID of the next driver instance, USB device instance or other
     * PDM entity requiring an ID. */
    uint32_t                        idTracingOther;

    /** @name   VMM device heap
     * @{ */
    /** The heap size. */
    uint32_t                        cbVMMDevHeap;
    /** Free space. */
    uint32_t                        cbVMMDevHeapLeft;
    /** Pointer to the heap base (MMIO2 ring-3 mapping). NULL if not registered. */
    RTR3PTR                         pvVMMDevHeap;
    /** Ring-3 mapping/unmapping notification callback for the user. */
    PFNPDMVMMDEVHEAPNOTIFY          pfnVMMDevHeapNotify;
    /** The current mapping. NIL_RTGCPHYS if not mapped or registered. */
    RTGCPHYS                        GCPhysVMMDevHeap;
    /** @} */

    /** Number of times a critical section leave request needed to be queued for ring-3 execution. */
    STAMCOUNTER                     StatQueuedCritSectLeaves;
} PDM;
AssertCompileMemberAlignment(PDM, CritSect, 8);
AssertCompileMemberAlignment(PDM, aTaskSets, 64);
AssertCompileMemberAlignment(PDM, StatQueuedCritSectLeaves, 8);
AssertCompileMemberAlignment(PDM, GCPhysVMMDevHeap, sizeof(RTGCPHYS));
/** Pointer to PDM VM instance data. */
typedef PDM *PPDM;


/**
 * PDM data kept in the ring-0 GVM.
 */
typedef struct PDMR0PERVM
{
    /** PCI Buses, ring-0 data. */
    PDMPCIBUSR0                     aPciBuses[PDM_PCI_BUSSES_MAX];
    /** IOMMUs, ring-0 data. */
    PDMIOMMUR0                      aIommus[PDM_IOMMUS_MAX];
    /** Number of valid ring-0 device instances (apDevInstances). */
    uint32_t                        cDevInstances;
    uint32_t                        u32Padding;
    /** Pointer to ring-0 device instances. */
    R0PTRTYPE(struct PDMDEVINSR0 *) apDevInstances[190];
} PDMR0PERVM;


/**
 * PDM data kept in the UVM.
 */
typedef struct PDMUSERPERVM
{
    /** @todo move more stuff over here. */

    /** Linked list of timer driven PDM queues.
     * Currently serialized by PDM::CritSect.  */
    R3PTRTYPE(struct PDMQUEUE *)    pQueuesTimer;
    /** Linked list of force action driven PDM queues.
     * Currently serialized by PDM::CritSect. */
    R3PTRTYPE(struct PDMQUEUE *)    pQueuesForced;

    /** Lock protecting the lists below it. */
    RTCRITSECT                      ListCritSect;
    /** Pointer to list of loaded modules. */
    PPDMMOD                         pModules;
    /** List of initialized critical sections. (LIFO) */
    R3PTRTYPE(PPDMCRITSECTINT)      pCritSects;
    /** List of initialized read/write critical sections. (LIFO) */
    R3PTRTYPE(PPDMCRITSECTRWINT)    pRwCritSects;
    /** Head of the PDM Thread list. (singly linked) */
    R3PTRTYPE(PPDMTHREAD)           pThreads;
    /** Tail of the PDM Thread list. (singly linked) */
    R3PTRTYPE(PPDMTHREAD)           pThreadsTail;

    /** @name   PDM Async Completion
     * @{ */
    /** Pointer to the array of supported endpoint classes. */
    PPDMASYNCCOMPLETIONEPCLASS      apAsyncCompletionEndpointClass[PDMASYNCCOMPLETIONEPCLASSTYPE_MAX];
    /** Head of the templates. Singly linked, protected by ListCritSect. */
    R3PTRTYPE(PPDMASYNCCOMPLETIONTEMPLATE) pAsyncCompletionTemplates;
    /** @} */

    /** Global block cache data. */
    R3PTRTYPE(PPDMBLKCACHEGLOBAL)   pBlkCacheGlobal;
#ifdef VBOX_WITH_NETSHAPER
    /** Pointer to network shaper instance. */
    R3PTRTYPE(PPDMNETSHAPER)        pNetShaper;
#endif /* VBOX_WITH_NETSHAPER */

} PDMUSERPERVM;
/** Pointer to the PDM data kept in the UVM. */
typedef PDMUSERPERVM *PPDMUSERPERVM;



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifdef IN_RING3
extern const PDMDRVHLPR3    g_pdmR3DrvHlp;
extern const PDMDEVHLPR3    g_pdmR3DevHlpTrusted;
# ifdef VBOX_WITH_DBGF_TRACING
extern const PDMDEVHLPR3    g_pdmR3DevHlpTracing;
# endif
extern const PDMDEVHLPR3    g_pdmR3DevHlpUnTrusted;
extern const PDMPICHLP      g_pdmR3DevPicHlp;
extern const PDMIOAPICHLP   g_pdmR3DevIoApicHlp;
extern const PDMFWHLPR3     g_pdmR3DevFirmwareHlp;
extern const PDMPCIHLPR3    g_pdmR3DevPciHlp;
extern const PDMIOMMUHLPR3  g_pdmR3DevIommuHlp;
extern const PDMDMACHLP     g_pdmR3DevDmacHlp;
extern const PDMRTCHLP      g_pdmR3DevRtcHlp;
extern const PDMHPETHLPR3   g_pdmR3DevHpetHlp;
extern const PDMPCIRAWHLPR3 g_pdmR3DevPciRawHlp;
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @def PDMDEV_ASSERT_DEVINS
 * Asserts the validity of the device instance.
 */
#ifdef VBOX_STRICT
# define PDMDEV_ASSERT_DEVINS(pDevIns)   \
    do { \
        AssertPtr(pDevIns); \
        Assert(pDevIns->u32Version == PDM_DEVINS_VERSION); \
        Assert(pDevIns->CTX_SUFF(pvInstanceDataFor) == (void *)&pDevIns->achInstanceData[0]); \
    } while (0)
#else
# define PDMDEV_ASSERT_DEVINS(pDevIns)   do { } while (0)
#endif

/** @def PDMDRV_ASSERT_DRVINS
 * Asserts the validity of the driver instance.
 */
#ifdef VBOX_STRICT
# define PDMDRV_ASSERT_DRVINS(pDrvIns) \
    do { \
        AssertPtr(pDrvIns); \
        Assert(pDrvIns->u32Version == PDM_DRVINS_VERSION); \
        Assert(pDrvIns->CTX_SUFF(pvInstanceData) == (void *)&pDrvIns->achInstanceData[0]); \
    } while (0)
#else
# define PDMDRV_ASSERT_DRVINS(pDrvIns)   do { } while (0)
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
#ifdef IN_RING3
bool        pdmR3IsValidName(const char *pszName);

int         pdmR3CritSectBothInitStats(PVM pVM);
void        pdmR3CritSectBothRelocate(PVM pVM);
int         pdmR3CritSectBothDeleteDevice(PVM pVM, PPDMDEVINS pDevIns);
int         pdmR3CritSectBothDeleteDriver(PVM pVM, PPDMDRVINS pDrvIns);
int         pdmR3CritSectInitDevice(        PVM pVM, PPDMDEVINS pDevIns, PPDMCRITSECT pCritSect, RT_SRC_POS_DECL,
                                            const char *pszNameFmt, va_list va);
int         pdmR3CritSectInitDeviceAuto(    PVM pVM, PPDMDEVINS pDevIns, PPDMCRITSECT pCritSect, RT_SRC_POS_DECL,
                                            const char *pszNameFmt, ...);
int         pdmR3CritSectInitDriver(        PVM pVM, PPDMDRVINS pDrvIns, PPDMCRITSECT pCritSect, RT_SRC_POS_DECL,
                                            const char *pszNameFmt, ...);
int         pdmR3CritSectRwInitDevice(      PVM pVM, PPDMDEVINS pDevIns, PPDMCRITSECTRW pCritSect, RT_SRC_POS_DECL,
                                            const char *pszNameFmt, va_list va);
int         pdmR3CritSectRwInitDeviceAuto(  PVM pVM, PPDMDEVINS pDevIns, PPDMCRITSECTRW pCritSect, RT_SRC_POS_DECL,
                                            const char *pszNameFmt, ...);
int         pdmR3CritSectRwInitDriver(      PVM pVM, PPDMDRVINS pDrvIns, PPDMCRITSECTRW pCritSect, RT_SRC_POS_DECL,
                                            const char *pszNameFmt, ...);

int         pdmR3DevInit(PVM pVM);
int         pdmR3DevInitComplete(PVM pVM);
PPDMDEV     pdmR3DevLookup(PVM pVM, const char *pszName);
int         pdmR3DevFindLun(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun, PPDMLUN *ppLun);
DECLCALLBACK(bool) pdmR3DevHlpQueueConsumer(PVM pVM, PPDMQUEUEITEMCORE pItem);

int         pdmR3UsbLoadModules(PVM pVM);
int         pdmR3UsbInstantiateDevices(PVM pVM);
PPDMUSB     pdmR3UsbLookup(PVM pVM, const char *pszName);
int         pdmR3UsbRegisterHub(PVM pVM, PPDMDRVINS pDrvIns, uint32_t fVersions, uint32_t cPorts, PCPDMUSBHUBREG pUsbHubReg, PPCPDMUSBHUBHLP ppUsbHubHlp);
int         pdmR3UsbVMInitComplete(PVM pVM);

int         pdmR3DrvInit(PVM pVM);
int         pdmR3DrvInstantiate(PVM pVM, PCFGMNODE pNode, PPDMIBASE pBaseInterface, PPDMDRVINS pDrvAbove,
                                PPDMLUN pLun, PPDMIBASE *ppBaseInterface);
int         pdmR3DrvDetach(PPDMDRVINS pDrvIns, uint32_t fFlags);
void        pdmR3DrvDestroyChain(PPDMDRVINS pDrvIns, uint32_t fFlags);
PPDMDRV     pdmR3DrvLookup(PVM pVM, const char *pszName);

int         pdmR3LdrInitU(PUVM pUVM);
void        pdmR3LdrTermU(PUVM pUVM);
char       *pdmR3FileR3(const char *pszFile, bool fShared);
int         pdmR3LoadR3U(PUVM pUVM, const char *pszFilename, const char *pszName);

void        pdmR3QueueRelocate(PVM pVM, RTGCINTPTR offDelta);

int         pdmR3TaskInit(PVM pVM);
void        pdmR3TaskTerm(PVM pVM);

int         pdmR3ThreadCreateDevice(PVM pVM, PPDMDEVINS pDevIns, PPPDMTHREAD ppThread, void *pvUser, PFNPDMTHREADDEV pfnThread,
                                    PFNPDMTHREADWAKEUPDEV pfnWakeup, size_t cbStack, RTTHREADTYPE enmType, const char *pszName);
int         pdmR3ThreadCreateUsb(PVM pVM, PPDMUSBINS pUsbIns, PPPDMTHREAD ppThread, void *pvUser, PFNPDMTHREADUSB pfnThread,
                                 PFNPDMTHREADWAKEUPUSB pfnWakeup, size_t cbStack, RTTHREADTYPE enmType, const char *pszName);
int         pdmR3ThreadCreateDriver(PVM pVM, PPDMDRVINS pDrvIns, PPPDMTHREAD ppThread, void *pvUser, PFNPDMTHREADDRV pfnThread,
                                    PFNPDMTHREADWAKEUPDRV pfnWakeup, size_t cbStack, RTTHREADTYPE enmType, const char *pszName);
int         pdmR3ThreadDestroyDevice(PVM pVM, PPDMDEVINS pDevIns);
int         pdmR3ThreadDestroyUsb(PVM pVM, PPDMUSBINS pUsbIns);
int         pdmR3ThreadDestroyDriver(PVM pVM, PPDMDRVINS pDrvIns);
void        pdmR3ThreadDestroyAll(PVM pVM);
int         pdmR3ThreadResumeAll(PVM pVM);
int         pdmR3ThreadSuspendAll(PVM pVM);

#ifdef VBOX_WITH_PDM_ASYNC_COMPLETION
int         pdmR3AsyncCompletionInit(PVM pVM);
int         pdmR3AsyncCompletionTerm(PVM pVM);
void        pdmR3AsyncCompletionResume(PVM pVM);
int         pdmR3AsyncCompletionTemplateCreateDevice(PVM pVM, PPDMDEVINS pDevIns, PPPDMASYNCCOMPLETIONTEMPLATE ppTemplate, PFNPDMASYNCCOMPLETEDEV pfnCompleted, const char *pszDesc);
int         pdmR3AsyncCompletionTemplateCreateDriver(PVM pVM, PPDMDRVINS pDrvIns, PPPDMASYNCCOMPLETIONTEMPLATE ppTemplate,
                                                     PFNPDMASYNCCOMPLETEDRV pfnCompleted, void *pvTemplateUser, const char *pszDesc);
int         pdmR3AsyncCompletionTemplateCreateUsb(PVM pVM, PPDMUSBINS pUsbIns, PPPDMASYNCCOMPLETIONTEMPLATE ppTemplate, PFNPDMASYNCCOMPLETEUSB pfnCompleted, const char *pszDesc);
int         pdmR3AsyncCompletionTemplateDestroyDevice(PVM pVM, PPDMDEVINS pDevIns);
int         pdmR3AsyncCompletionTemplateDestroyDriver(PVM pVM, PPDMDRVINS pDrvIns);
int         pdmR3AsyncCompletionTemplateDestroyUsb(PVM pVM, PPDMUSBINS pUsbIns);
#endif

#ifdef VBOX_WITH_NETSHAPER
int         pdmR3NetShaperInit(PVM pVM);
int         pdmR3NetShaperTerm(PVM pVM);
#endif

int         pdmR3BlkCacheInit(PVM pVM);
void        pdmR3BlkCacheTerm(PVM pVM);
int         pdmR3BlkCacheResume(PVM pVM);

#endif /* IN_RING3 */

void        pdmLock(PVMCC pVM);
int         pdmLockEx(PVMCC pVM, int rc);
void        pdmUnlock(PVMCC pVM);

#if defined(IN_RING3) || defined(IN_RING0)
void        pdmCritSectRwLeaveSharedQueued(PPDMCRITSECTRW pThis);
void        pdmCritSectRwLeaveExclQueued(PPDMCRITSECTRW pThis);
#endif

#if defined IN_RING0
DECLHIDDEN(bool)               pdmR0IsaSetIrq(PGVM pGVM, int iIrq, int iLevel, uint32_t uTagSrc);
#endif

#ifdef VBOX_WITH_DBGF_TRACING
# if defined(IN_RING3)
DECLHIDDEN(DECLCALLBACK(int))  pdmR3DevHlpTracing_IoPortCreateEx(PPDMDEVINS pDevIns, RTIOPORT cPorts, uint32_t fFlags, PPDMPCIDEV pPciDev,
                                                                 uint32_t iPciRegion, PFNIOMIOPORTNEWOUT pfnOut, PFNIOMIOPORTNEWIN pfnIn,
                                                                 PFNIOMIOPORTNEWOUTSTRING pfnOutStr, PFNIOMIOPORTNEWINSTRING pfnInStr, RTR3PTR pvUser,
                                                                 const char *pszDesc, PCIOMIOPORTDESC paExtDescs, PIOMIOPORTHANDLE phIoPorts);
DECLHIDDEN(DECLCALLBACK(int))  pdmR3DevHlpTracing_IoPortMap(PPDMDEVINS pDevIns, IOMIOPORTHANDLE hIoPorts, RTIOPORT Port);
DECLHIDDEN(DECLCALLBACK(int))  pdmR3DevHlpTracing_IoPortUnmap(PPDMDEVINS pDevIns, IOMIOPORTHANDLE hIoPorts);
DECLHIDDEN(DECLCALLBACK(int))  pdmR3DevHlpTracing_MmioCreateEx(PPDMDEVINS pDevIns, RTGCPHYS cbRegion,
                                                               uint32_t fFlags, PPDMPCIDEV pPciDev, uint32_t iPciRegion,
                                                               PFNIOMMMIONEWWRITE pfnWrite, PFNIOMMMIONEWREAD pfnRead, PFNIOMMMIONEWFILL pfnFill,
                                                               void *pvUser, const char *pszDesc, PIOMMMIOHANDLE phRegion);
DECLHIDDEN(DECLCALLBACK(int))  pdmR3DevHlpTracing_MmioMap(PPDMDEVINS pDevIns, IOMMMIOHANDLE hRegion, RTGCPHYS GCPhys);
DECLHIDDEN(DECLCALLBACK(int))  pdmR3DevHlpTracing_MmioUnmap(PPDMDEVINS pDevIns, IOMMMIOHANDLE hRegion);
DECLHIDDEN(DECLCALLBACK(int))  pdmR3DevHlpTracing_PhysRead(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead, uint32_t fFlags);
DECLHIDDEN(DECLCALLBACK(int))  pdmR3DevHlpTracing_PhysWrite(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite, uint32_t fFlags);
DECLHIDDEN(DECLCALLBACK(int))  pdmR3DevHlpTracing_PCIPhysRead(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead, uint32_t fFlags);
DECLHIDDEN(DECLCALLBACK(int))  pdmR3DevHlpTracing_PCIPhysWrite(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite, uint32_t fFlags);
DECLHIDDEN(DECLCALLBACK(void)) pdmR3DevHlpTracing_PCISetIrq(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, int iIrq, int iLevel);
DECLHIDDEN(DECLCALLBACK(void)) pdmR3DevHlpTracing_PCISetIrqNoWait(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, int iIrq, int iLevel);
DECLHIDDEN(DECLCALLBACK(void)) pdmR3DevHlpTracing_ISASetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel);
DECLHIDDEN(DECLCALLBACK(void)) pdmR3DevHlpTracing_ISASetIrqNoWait(PPDMDEVINS pDevIns, int iIrq, int iLevel);
DECLHIDDEN(DECLCALLBACK(void)) pdmR3DevHlpTracing_IoApicSendMsi(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, uint32_t uValue);
# elif defined(IN_RING0)
DECLHIDDEN(DECLCALLBACK(int))  pdmR0DevHlpTracing_IoPortSetUpContextEx(PPDMDEVINS pDevIns, IOMIOPORTHANDLE hIoPorts,
                                                                       PFNIOMIOPORTNEWOUT pfnOut, PFNIOMIOPORTNEWIN pfnIn,
                                                                       PFNIOMIOPORTNEWOUTSTRING pfnOutStr, PFNIOMIOPORTNEWINSTRING pfnInStr,
                                                                       void *pvUser);
DECLHIDDEN(DECLCALLBACK(int))  pdmR0DevHlpTracing_MmioSetUpContextEx(PPDMDEVINS pDevIns, IOMMMIOHANDLE hRegion, PFNIOMMMIONEWWRITE pfnWrite,
                                                                     PFNIOMMMIONEWREAD pfnRead, PFNIOMMMIONEWFILL pfnFill, void *pvUser);
DECLHIDDEN(DECLCALLBACK(int))  pdmR0DevHlpTracing_PhysRead(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead, uint32_t fFlags);
DECLHIDDEN(DECLCALLBACK(int))  pdmR0DevHlpTracing_PhysWrite(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite, uint32_t fFlags);
DECLHIDDEN(DECLCALLBACK(int))  pdmR0DevHlpTracing_PCIPhysRead(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead, uint32_t fFlags);
DECLHIDDEN(DECLCALLBACK(int))  pdmR0DevHlpTracing_PCIPhysWrite(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite, uint32_t fFlags);
DECLHIDDEN(DECLCALLBACK(void)) pdmR0DevHlpTracing_PCISetIrq(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, int iIrq, int iLevel);
DECLHIDDEN(DECLCALLBACK(void)) pdmR0DevHlpTracing_PCISetIrqNoWait(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, int iIrq, int iLevel);
DECLHIDDEN(DECLCALLBACK(void)) pdmR0DevHlpTracing_ISASetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel);
DECLHIDDEN(DECLCALLBACK(void)) pdmR0DevHlpTracing_ISASetIrqNoWait(PPDMDEVINS pDevIns, int iIrq, int iLevel);
DECLHIDDEN(DECLCALLBACK(void)) pdmR0DevHlpTracing_IoApicSendMsi(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, uint32_t uValue);
# else
# error "Invalid environment selected"
# endif
#endif


/** @} */

RT_C_DECLS_END

#endif /* !VMM_INCLUDED_SRC_include_PDMInternal_h */

