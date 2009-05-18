/* $Id$ */
/** @file
 * PDM - Internal header file.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___PDMInternal_h
#define ___PDMInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/param.h>
#include <VBox/cfgm.h>
#include <VBox/stam.h>
#include <VBox/vusb.h>
#include <VBox/pdmasynccompletion.h>
#include <iprt/critsect.h>
#ifdef IN_RING3
# include <iprt/thread.h>
#endif

__BEGIN_DECLS


/** @defgroup grp_pdm_int       Internal
 * @ingroup grp_pdm
 * @internal
 * @{
 */

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
/** Pointer to a DMAC instance. */
typedef struct PDMDMAC *PPDMDMAC;
/** Pointer to a RTC instance. */
typedef struct PDMRTC *PPDMRTC;

/** Pointer to an USB HUB registration record. */
typedef struct PDMUSBHUB *PPDMUSBHUB;

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
    /** Configuration handle to the instance node. */
    R3PTRTYPE(PCFGMNODE)            pCfgHandle;

    /** R3 pointer to the VM this instance was created for. */
    PVMR3                           pVMR3;
    /** R3 pointer to associated PCI device structure. */
    R3PTRTYPE(struct PCIDevice *)   pPciDeviceR3;
    /** R3 pointer to associated PCI bus structure. */
    R3PTRTYPE(PPDMPCIBUS)           pPciBusR3;

    /** R0 pointer to the VM this instance was created for. */
    PVMR0                           pVMR0;
    /** R0 pointer to associated PCI device structure. */
    R0PTRTYPE(struct PCIDevice *)   pPciDeviceR0;
    /** R0 pointer to associated PCI bus structure. */
    R0PTRTYPE(PPDMPCIBUS)           pPciBusR0;
    /** Alignment padding. */
    RTR0PTR                         Alignment0;

    /** RC pointer to the VM this instance was created for. */
    PVMRC                           pVMRC;
    /** RC pointer to associated PCI device structure. */
    RCPTRTYPE(struct PCIDevice *)   pPciDeviceRC;
    /** RC pointer to associated PCI bus structure. */
    RCPTRTYPE(PPDMPCIBUS)           pPciBusRC;
    /** Alignment padding. */
    RTRCPTR                         Alignment1;
} PDMDEVINSINT;


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
#if HC_ARCH_BITS == 64
    uint32_t                        Alignment0;
#endif
} PDMUSBINSINT;


/**
 * Private driver instance data.
 */
typedef struct PDMDRVINSINT
{
    /** Pointer to the driver instance above.
     * This is NULL for the topmost drive. */
    PPDMDRVINS                      pUp;
    /** Pointer to the driver instance below.
     * This is NULL for the bottommost driver. */
    PPDMDRVINS                      pDown;
    /** Pointer to the logical unit this driver chained on. */
    PPDMLUN                         pLun;
    /** Pointer to driver structure from which this was instantiated. */
    PPDMDRV                         pDrv;
    /** Pointer to the VM this instance was created for. */
    PVM                             pVM;
    /** Flag indicating that the driver is being detached and destroyed.
     * (Helps detect potential recursive detaching.) */
    bool                            fDetaching;
    /** Configuration handle to the instance node. */
    PCFGMNODE                       pCfgHandle;

} PDMDRVINSINT;


/**
 * Private critical section data.
 */
typedef struct PDMCRITSECTINT
{
    /** The critical section core which is shared with IPRT. */
    RTCRITSECT                      Core;
    /** Pointer to the next critical section.
     * This chain is used for relocating pVMRC and device cleanup. */
    R3PTRTYPE(struct PDMCRITSECTINT *) pNext;
    /** Owner identifier.
     * This is pDevIns if the owner is a device. Similarily for a driver or service.
     * PDMR3CritSectInit() sets this to point to the critsect itself. */
    RTR3PTR                         pvKey;
    /** Pointer to the VM - R3Ptr. */
    PVMR3                           pVMR3;
    /** Pointer to the VM - R0Ptr. */
    PVMR0                           pVMR0;
    /** Pointer to the VM - GCPtr. */
    PVMRC                           pVMRC;
#if HC_ARCH_BITS == 64
    /** Alignment padding. */
    uint32_t                        padding;
#endif
    /** Event semaphore that is scheduled to be signaled upon leaving the
     * critical section. This is Ring-3 only of course. */
    RTSEMEVENT                      EventToSignal;
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
typedef PDMCRITSECTINT *PPDMCRITSECTINT;


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
#define PDMTHREADINT_DECLARED
#ifdef ___VBox_pdm_h
# error "Invalid header PDM order. Include PDMInternal.h before VBox/pdm.h!"
#endif
__END_DECLS
#include <VBox/pdm.h>
__BEGIN_DECLS

/**
 * PDM Logical Unit.
 *
 * This typically the representation of a physical port on a
 * device, like for instance the PS/2 keyboard port on the
 * keyboard controller device. The LUNs are chained on the
 * device the belong to (PDMDEVINSINT::pLunsR3).
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
 * PDM Device.
 */
typedef struct PDMDEV
{
    /** Pointer to the next device (R3 Ptr). */
    R3PTRTYPE(PPDMDEV)              pNext;
    /** Device name length. (search optimization) */
    RTUINT                          cchName;
    /** Registration structure. */
    R3PTRTYPE(const struct PDMDEVREG *) pDevReg;
    /** Number of instances. */
    RTUINT                          cInstances;
    /** Pointer to chain of instances (R3 Ptr). */
    PPDMDEVINSR3                    pInstances;
} PDMDEV;


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
    R3PTRTYPE(const struct PDMUSBREG *) pUsbReg;
    /** Next instance number. */
    RTUINT                          iNextInstance;
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
    const struct PDMDRVREG *        pDrvReg;
    /** Number of instances. */
    RTUINT                          cInstances;
} PDMDRV;


/**
 * PDM registered PIC device.
 */
typedef struct PDMPIC
{
    /** Pointer to the PIC device instance - R3. */
    PPDMDEVINSR3                    pDevInsR3;
    /** @copydoc PDMPICREG::pfnSetIrqR3 */
    DECLR3CALLBACKMEMBER(void,      pfnSetIrqR3,(PPDMDEVINS pDevIns, int iIrq, int iLevel));
    /** @copydoc PDMPICREG::pfnGetInterruptR3 */
    DECLR3CALLBACKMEMBER(int,       pfnGetInterruptR3,(PPDMDEVINS pDevIns));

    /** Pointer to the PIC device instance - R0. */
    PPDMDEVINSR0                    pDevInsR0;
    /** @copydoc PDMPICREG::pfnSetIrqR3 */
    DECLR0CALLBACKMEMBER(void,      pfnSetIrqR0,(PPDMDEVINS pDevIns, int iIrq, int iLevel));
    /** @copydoc PDMPICREG::pfnGetInterruptR3 */
    DECLR0CALLBACKMEMBER(int,       pfnGetInterruptR0,(PPDMDEVINS pDevIns));

    /** Pointer to the PIC device instance - RC. */
    PPDMDEVINSRC                    pDevInsRC;
    /** @copydoc PDMPICREG::pfnSetIrqR3 */
    DECLRCCALLBACKMEMBER(void,      pfnSetIrqRC,(PPDMDEVINS pDevIns, int iIrq, int iLevel));
    /** @copydoc PDMPICREG::pfnGetInterruptR3 */
    DECLRCCALLBACKMEMBER(int,       pfnGetInterruptRC,(PPDMDEVINS pDevIns));
    /** Alignment padding. */
    RTRCPTR                         RCPtrPadding;
} PDMPIC;


/**
 * PDM registered APIC device.
 */
typedef struct PDMAPIC
{
    /** Pointer to the APIC device instance - R3 Ptr. */
    PPDMDEVINSR3                    pDevInsR3;
    /** @copydoc PDMAPICREG::pfnGetInterruptR3 */
    DECLR3CALLBACKMEMBER(int,       pfnGetInterruptR3,(PPDMDEVINS pDevIns));
    /** @copydoc PDMAPICREG::pfnHasPendingIrqR3 */
    DECLR3CALLBACKMEMBER(bool,      pfnHasPendingIrqR3,(PPDMDEVINS pDevIns));
    /** @copydoc PDMAPICREG::pfnSetBaseR3 */
    DECLR3CALLBACKMEMBER(void,      pfnSetBaseR3,(PPDMDEVINS pDevIns, uint64_t u64Base));
    /** @copydoc PDMAPICREG::pfnGetBaseR3 */
    DECLR3CALLBACKMEMBER(uint64_t,  pfnGetBaseR3,(PPDMDEVINS pDevIns));
    /** @copydoc PDMAPICREG::pfnSetTPRR3 */
    DECLR3CALLBACKMEMBER(void,      pfnSetTPRR3,(PPDMDEVINS pDevIns, uint8_t u8TPR));
    /** @copydoc PDMAPICREG::pfnGetTPRR3 */
    DECLR3CALLBACKMEMBER(uint8_t,   pfnGetTPRR3,(PPDMDEVINS pDevIns));
    /** @copydoc PDMAPICREG::pfnWriteMSRR3 */
    DECLR3CALLBACKMEMBER(int,       pfnWriteMSRR3, (PPDMDEVINS pDevIns, VMCPUID idCpu, uint32_t u32Reg, uint64_t u64Value));
    /** @copydoc PDMAPICREG::pfnReadMSRR3 */
    DECLR3CALLBACKMEMBER(int,       pfnReadMSRR3, (PPDMDEVINS pDevIns, VMCPUID idCpu, uint32_t u32Reg, uint64_t *pu64Value));
    /** @copydoc PDMAPICREG::pfnBusDeliverR3 */
    DECLR3CALLBACKMEMBER(int,       pfnBusDeliverR3,(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                     uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode));

    /** Pointer to the APIC device instance - R0 Ptr. */
    PPDMDEVINSR0                    pDevInsR0;
    /** @copydoc PDMAPICREG::pfnGetInterruptR3 */
    DECLR0CALLBACKMEMBER(int,       pfnGetInterruptR0,(PPDMDEVINS pDevIns));
    /** @copydoc PDMAPICREG::pfnHasPendingIrqR3 */
    DECLR0CALLBACKMEMBER(bool,      pfnHasPendingIrqR0,(PPDMDEVINS pDevIns));
    /** @copydoc PDMAPICREG::pfnSetBaseR3 */
    DECLR0CALLBACKMEMBER(void,      pfnSetBaseR0,(PPDMDEVINS pDevIns, uint64_t u64Base));
    /** @copydoc PDMAPICREG::pfnGetBaseR3 */
    DECLR0CALLBACKMEMBER(uint64_t,  pfnGetBaseR0,(PPDMDEVINS pDevIns));
    /** @copydoc PDMAPICREG::pfnSetTPRR3 */
    DECLR0CALLBACKMEMBER(void,      pfnSetTPRR0,(PPDMDEVINS pDevIns, uint8_t u8TPR));
    /** @copydoc PDMAPICREG::pfnGetTPRR3 */
    DECLR0CALLBACKMEMBER(uint8_t,   pfnGetTPRR0,(PPDMDEVINS pDevIns));
     /** @copydoc PDMAPICREG::pfnWriteMSRR3 */
    DECLR0CALLBACKMEMBER(uint32_t,  pfnWriteMSRR0, (PPDMDEVINS pDevIns, VMCPUID idCpu, uint32_t u32Reg, uint64_t u64Value));
    /** @copydoc PDMAPICREG::pfnReadMSRR3 */
    DECLR0CALLBACKMEMBER(uint32_t,  pfnReadMSRR0, (PPDMDEVINS pDevIns, VMCPUID idCpu, uint32_t u32Reg, uint64_t *pu64Value));
    /** @copydoc PDMAPICREG::pfnBusDeliverR3 */
    DECLR0CALLBACKMEMBER(int,       pfnBusDeliverR0,(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                     uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode));

    /** Pointer to the APIC device instance - RC Ptr. */
    PPDMDEVINSRC                    pDevInsRC;
    /** @copydoc PDMAPICREG::pfnGetInterruptR3 */
    DECLRCCALLBACKMEMBER(int,       pfnGetInterruptRC,(PPDMDEVINS pDevIns));
    /** @copydoc PDMAPICREG::pfnHasPendingIrqR3 */
    DECLRCCALLBACKMEMBER(bool,      pfnHasPendingIrqRC,(PPDMDEVINS pDevIns));
    /** @copydoc PDMAPICREG::pfnSetBaseR3 */
    DECLRCCALLBACKMEMBER(void,      pfnSetBaseRC,(PPDMDEVINS pDevIns, uint64_t u64Base));
    /** @copydoc PDMAPICREG::pfnGetBaseR3 */
    DECLRCCALLBACKMEMBER(uint64_t,  pfnGetBaseRC,(PPDMDEVINS pDevIns));
    /** @copydoc PDMAPICREG::pfnSetTPRR3 */
    DECLRCCALLBACKMEMBER(void,      pfnSetTPRRC,(PPDMDEVINS pDevIns, uint8_t u8TPR));
    /** @copydoc PDMAPICREG::pfnGetTPRR3 */
    DECLRCCALLBACKMEMBER(uint8_t,   pfnGetTPRRC,(PPDMDEVINS pDevIns));
    /** @copydoc PDMAPICREG::pfnWriteMSRR3 */
    DECLRCCALLBACKMEMBER(uint32_t,  pfnWriteMSRRC, (PPDMDEVINS pDevIns, VMCPUID idCpu, uint32_t u32Reg, uint64_t u64Value));
    /** @copydoc PDMAPICREG::pfnReadMSRR3 */
    DECLRCCALLBACKMEMBER(uint32_t,  pfnReadMSRRC, (PPDMDEVINS pDevIns, VMCPUID idCpu, uint32_t u32Reg, uint64_t *pu64Value));
    /** @copydoc PDMAPICREG::pfnBusDeliverR3 */
    DECLRCCALLBACKMEMBER(int,       pfnBusDeliverRC,(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                     uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode));
} PDMAPIC;


/**
 * PDM registered I/O APIC device.
 */
typedef struct PDMIOAPIC
{
    /** Pointer to the APIC device instance - R3 Ptr. */
    PPDMDEVINSR3                    pDevInsR3;
    /** @copydoc PDMIOAPICREG::pfnSetIrqR3 */
    DECLR3CALLBACKMEMBER(void,      pfnSetIrqR3,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /** Pointer to the PIC device instance - R0. */
    PPDMDEVINSR0                    pDevInsR0;
    /** @copydoc PDMIOAPICREG::pfnSetIrqR3 */
    DECLR0CALLBACKMEMBER(void,      pfnSetIrqR0,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /** Pointer to the APIC device instance - RC Ptr. */
    PPDMDEVINSRC                    pDevInsRC;
    /** @copydoc PDMIOAPICREG::pfnSetIrqR3 */
    DECLRCCALLBACKMEMBER(void,      pfnSetIrqRC,(PPDMDEVINS pDevIns, int iIrq, int iLevel));
} PDMIOAPIC;

/** Maximum number of PCI busses for a VM. */
#define PDM_PCI_BUSSES_MAX 8

/**
 * PDM PCI Bus instance.
 */
typedef struct PDMPCIBUS
{
    /** PCI bus number. */
    RTUINT          iBus;
    RTUINT          uPadding0; /**< Alignment padding.*/

    /** Pointer to PCI Bus device instance. */
    PPDMDEVINSR3                    pDevInsR3;
    /** @copydoc PDMPCIBUSREG::pfnSetIrqR3 */
    DECLR3CALLBACKMEMBER(void,      pfnSetIrqR3,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel));
    /** @copydoc PDMPCIBUSREG::pfnRegisterR3 */
    DECLR3CALLBACKMEMBER(int,       pfnRegisterR3,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, const char *pszName, int iDev));
    /** @copydoc PDMPCIBUSREG::pfnIORegionRegisterR3 */
    DECLR3CALLBACKMEMBER(int,       pfnIORegionRegisterR3,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iRegion, uint32_t cbRegion,
                                                           PCIADDRESSSPACE enmType, PFNPCIIOREGIONMAP pfnCallback));
    /** @copydoc PDMPCIBUSREG::pfnSetConfigCallbacksR3 */
    DECLR3CALLBACKMEMBER(void,      pfnSetConfigCallbacksR3,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PFNPCICONFIGREAD pfnRead,
                                                             PPFNPCICONFIGREAD ppfnReadOld, PFNPCICONFIGWRITE pfnWrite, PPFNPCICONFIGWRITE ppfnWriteOld));
    /** @copydoc PDMPCIBUSREG::pfnSaveExecR3 */
    DECLR3CALLBACKMEMBER(int,       pfnSaveExecR3,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PSSMHANDLE pSSMHandle));
    /** @copydoc PDMPCIBUSREG::pfnLoadExecR3 */
    DECLR3CALLBACKMEMBER(int,       pfnLoadExecR3,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PSSMHANDLE pSSMHandle));
    /** @copydoc PDMPCIBUSREG::pfnFakePCIBIOSR3 */
    DECLR3CALLBACKMEMBER(int,       pfnFakePCIBIOSR3,(PPDMDEVINS pDevIns));

    /** Pointer to the PIC device instance - R0. */
    R0PTRTYPE(PPDMDEVINS)           pDevInsR0;
    /** @copydoc PDMPCIBUSREG::pfnSetIrqR3 */
    DECLR0CALLBACKMEMBER(void,      pfnSetIrqR0,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel));

    /** Pointer to PCI Bus device instance. */
    PPDMDEVINSRC                    pDevInsRC;
    /** @copydoc PDMPCIBUSREG::pfnSetIrqR3 */
    DECLRCCALLBACKMEMBER(void,      pfnSetIrqRC,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel));
} PDMPCIBUS;


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
    /** Module name. This is used for refering to
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
    RTUINT                          cbItem;
    /** Number of items in the queue. */
    RTUINT                          cItems;
    /** Index to the free head (where we insert). */
    uint32_t volatile               iFreeHead;
    /** Index to the free tail (where we remove). */
    uint32_t volatile               iFreeTail;

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
         * PDMDEVHLPTASKOP_ISA_SET_IRQ and PDMDEVHLPTASKOP_PCI_SET_IRQ.
         */
        struct PDMDEVHLPTASKSETIRQ
        {
            /** The IRQ */
            int                     iIrq;
            /** The new level. */
            int                     iLevel;
        } SetIRQ;
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

/** Pointer to the main PDM Async completion structure. */
typedef struct PDMASYNCCOMPLETIONMANAGER *PPDMASYNCCOMPLETIONMANAGER;


/**
 * PDM VMCPU Instance data.
 * Changes to this must checked against the padding of the cfgm union in VMCPU!
 */
typedef struct PDMCPU
{
    /** The number of entries in the apQueuedCritSectsLeaves table that's currnetly in use. */
    RTUINT                          cQueuedCritSectLeaves;
    RTUINT                          uPadding0; /**< Alignment padding.*/
    /** Critical sections queued in RC/R0 because of contention preventing leave to complete. (R3 Ptrs)
     * We will return to Ring-3 ASAP, so this queue doesn't have to be very long. */
    R3PTRTYPE(PPDMCRITSECT)         apQueuedCritSectsLeaves[8];
} PDMCPU;

/**
 * Converts a PDM pointer into a VM pointer.
 * @returns Pointer to the VM structure the PDM is part of.
 * @param   pPDM   Pointer to PDM instance data.
 */
#define PDM2VM(pPDM)  ( (PVM)((char*)pPDM - pPDM->offVM) )


/**
 * PDM VM Instance data.
 * Changes to this must checked against the padding of the cfgm union in VM!
 */
typedef struct PDM
{
    /** Offset to the VM structure.
     * See PDM2VM(). */
    RTUINT                          offVM;
    RTUINT                          uPadding0; /**< Alignment padding.*/

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
    /** List of initialized critical sections. (LIFO) */
    R3PTRTYPE(PPDMCRITSECTINT)      pCritSects;
    /** PCI Buses. */
    PDMPCIBUS                       aPciBuses[PDM_PCI_BUSSES_MAX];
    /** The register PIC device. */
    PDMPIC                          Pic;
    /** The registerd APIC device. */
    PDMAPIC                         Apic;
    /** The registerd I/O APIC device. */
    PDMIOAPIC                       IoApic;
    /** The registered DMAC device. */
    R3PTRTYPE(PPDMDMAC)             pDmac;
    /** The registered RTC device. */
    R3PTRTYPE(PPDMRTC)              pRtc;
    /** The registered USB HUBs. (FIFO) */
    R3PTRTYPE(PPDMUSBHUB)           pUsbHubs;

    /** Queue in which devhlp tasks are queued for R3 execution - R3 Ptr. */
    R3PTRTYPE(PPDMQUEUE)            pDevHlpQueueR3;
    /** Queue in which devhlp tasks are queued for R3 execution - R0 Ptr. */
    R0PTRTYPE(PPDMQUEUE)            pDevHlpQueueR0;
    /** Queue in which devhlp tasks are queued for R3 execution - RC Ptr. */
    RCPTRTYPE(PPDMQUEUE)            pDevHlpQueueRC;

    RTUINT                          uPadding1; /**< Alignment padding. */

    /** Linked list of timer driven PDM queues. */
    R3PTRTYPE(struct PDMQUEUE *)    pQueuesTimer;
    /** Linked list of force action driven PDM queues. */
    R3PTRTYPE(struct PDMQUEUE *)    pQueuesForced;
    /** Pointer to the queue which should be manually flushed - R0 Ptr.
     * Only touched by EMT. */
    R0PTRTYPE(struct PDMQUEUE *)    pQueueFlushR0;
    /** Pointer to the queue which should be manually flushed - RC Ptr.
     * Only touched by EMT. */
    RCPTRTYPE(struct PDMQUEUE *)    pQueueFlushRC;
#if HC_ARCH_BITS == 64
    RTRCPTR                         uPadding2;
#endif

    /** Head of the PDM Thread list. (singly linked) */
    R3PTRTYPE(PPDMTHREAD)           pThreads;
    /** Tail of the PDM Thread list. (singly linked) */
    R3PTRTYPE(PPDMTHREAD)           pThreadsTail;

    /** Head of the asychronous tasks managers. (singly linked) */
    R3PTRTYPE(PPDMASYNCCOMPLETIONMANAGER) pAsyncCompletionManagerHead;
    /** Head of the templates. (singly linked) */
    R3PTRTYPE(PPDMASYNCCOMPLETIONTEMPLATE) pAsyncCompletionTemplates;

    /** @name   VMM device heap
     * @{ */
    /** Pointer to the heap base (MMIO2 ring-3 mapping). NULL if not registered. */
    RTR3PTR                         pvVMMDevHeap;
    /** The heap size. */
    RTUINT                          cbVMMDevHeap;
    /** Free space. */
    RTUINT                          cbVMMDevHeapLeft;
    /** The current mapping. NIL_RTGCPHYS if not mapped or registered. */
    RTGCPHYS                        GCPhysVMMDevHeap;
    /** @} */

    /** The PDM lock.
     * This is used to protect everything that deals with interrupts, i.e.
     * the PIC, APIC, IOAPIC and PCI devices pluss some PDM functions. */
    PDMCRITSECT                     CritSect;

    /** Number of times a critical section leave requesed needed to be queued for ring-3 execution. */
    STAMCOUNTER                     StatQueuedCritSectLeaves;
} PDM;
/** Pointer to PDM VM instance data. */
typedef PDM *PPDM;


/**
 * PDM data kept in the UVM.
 */
typedef struct PDMUSERPERVM
{
    /** Pointer to list of loaded modules. */
    PPDMMOD                         pModules;
    /** @todo move more stuff over here. */
} PDMUSERPERVM;
/** Pointer to the PDM data kept in the UVM. */
typedef PDMUSERPERVM *PPDMUSERPERVM;



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifdef IN_RING3
extern const PDMDRVHLP      g_pdmR3DrvHlp;
extern const PDMDEVHLPR3    g_pdmR3DevHlpTrusted;
extern const PDMDEVHLPR3    g_pdmR3DevHlpUnTrusted;
extern const PDMPICHLPR3    g_pdmR3DevPicHlp;
extern const PDMAPICHLPR3   g_pdmR3DevApicHlp;
extern const PDMIOAPICHLPR3 g_pdmR3DevIoApicHlp;
extern const PDMPCIHLPR3    g_pdmR3DevPciHlp;
extern const PDMDMACHLP     g_pdmR3DevDmacHlp;
extern const PDMRTCHLP      g_pdmR3DevRtcHlp;
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
        Assert(pDevIns->CTX_SUFF(pvInstanceData) == (void *)&pDevIns->achInstanceData[0]); \
    } while (0)
#else
# define PDMDEV_ASSERT_DEVINS(pDevIns)   do { } while (0)
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
#ifdef IN_RING3
int         pdmR3CritSectInit(PVM pVM);
int         pdmR3CritSectTerm(PVM pVM);
void        pdmR3CritSectRelocate(PVM pVM);
int         pdmR3CritSectInitDevice(PVM pVM, PPDMDEVINS pDevIns, PPDMCRITSECT pCritSect, const char *pszName);
int         pdmR3CritSectDeleteDevice(PVM pVM, PPDMDEVINS pDevIns);

int         pdmR3DevInit(PVM pVM);
PPDMDEV     pdmR3DevLookup(PVM pVM, const char *pszName);
int         pdmR3DevFindLun(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun, PPDMLUN *ppLun);
DECLCALLBACK(bool) pdmR3DevHlpQueueConsumer(PVM pVM, PPDMQUEUEITEMCORE pItem);

int         pdmR3UsbLoadModules(PVM pVM);
int         pdmR3UsbInstantiateDevices(PVM pVM);
PPDMUSB     pdmR3UsbLookup(PVM pVM, const char *pszName);
int         pdmR3UsbFindLun(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun, PPDMLUN *ppLun);
int         pdmR3UsbRegisterHub(PVM pVM, PPDMDRVINS pDrvIns, uint32_t fVersions, uint32_t cPorts, PCPDMUSBHUBREG pUsbHubReg, PPCPDMUSBHUBHLP ppUsbHubHlp);
int         pdmR3UsbVMInitComplete(PVM pVM);

int         pdmR3DrvInit(PVM pVM);
int         pdmR3DrvDetach(PPDMDRVINS pDrvIns);
void        pdmR3DrvDestroyChain(PPDMDRVINS pDrvIns);
PPDMDRV     pdmR3DrvLookup(PVM pVM, const char *pszName);

int         pdmR3LdrInitU(PUVM pUVM);
void        pdmR3LdrTermU(PUVM pUVM);
char *      pdmR3FileR3(const char *pszFile, bool fShared = false);
int         pdmR3LoadR3U(PUVM pUVM, const char *pszFilename, const char *pszName);

void        pdmR3QueueRelocate(PVM pVM, RTGCINTPTR offDelta);

int         pdmR3ThreadCreateDevice(PVM pVM, PPDMDEVINS pDevIns, PPPDMTHREAD ppThread, void *pvUser, PFNPDMTHREADDEV pfnThread,
                                    PFNPDMTHREADWAKEUPDEV pfnWakeup, size_t cbStack, RTTHREADTYPE enmType, const char *pszName);
int         pdmR3ThreadCreateUsb(PVM pVM, PPDMDRVINS pUsbIns, PPPDMTHREAD ppThread, void *pvUser, PFNPDMTHREADUSB pfnThread,
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
#endif

#endif /* IN_RING3 */

void        pdmLock(PVM pVM);
int         pdmLockEx(PVM pVM, int rc);
void        pdmUnlock(PVM pVM);

/** @} */

__END_DECLS

#endif
