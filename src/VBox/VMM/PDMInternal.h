/* $Id$ */
/** @file
 * PDM - Internal header file.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __PDMInternal_h__
#define __PDMInternal_h__

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/param.h>
#include <VBox/cfgm.h>
#include <VBox/stam.h>
#include <iprt/critsect.h>

__BEGIN_DECLS


/** @defgroup grp_pdm_int       Internal
 * @ingroup grp_pdm
 * @internal
 * @{
 */

/** Pointer to a PDM Device. */
typedef struct PDMDEV *PPDMDEV;
/** Pointer to a pointer to a PDM Device. */
typedef PPDMDEV *PPPDMDEV;

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


/**
 * Private instance data.
 */
typedef struct PDMDEVINSINT
{
    /** Pointer to the next instance (HC Ptr).
     * (Head is pointed to by PDM::pDevInstances.) */
    HCPTRTYPE(PPDMDEVINS)           pNextHC;
    /** Pointer to the next per device instance (HC Ptr).
     * (Head is pointed to by PDMDEV::pInstances.) */
    HCPTRTYPE(PPDMDEVINS)           pPerDeviceNextHC;

    /** Pointer to device structure - HC Ptr. */
    HCPTRTYPE(PPDMDEV)              pDevHC;

    /** Pointer to the VM this instance was created for - HC Ptr. */
    HCPTRTYPE(PVM)                  pVMHC;
    /** Pointer to the list of logical units associated with the device. (FIFO) */
    HCPTRTYPE(PPDMLUN)              pLunsHC;
    /** Configuration handle to the instance node. */
    HCPTRTYPE(PCFGMNODE)            pCfgHandle;
    /** HC pointer to associated PCI device structure. */
    HCPTRTYPE(struct PCIDevice *)   pPciDeviceHC;
    /** HC pointer to associated PCI bus structure. */
    HCPTRTYPE(PPDMPCIBUS)           pPciBusHC;

    /** GC pointer to associated PCI device structure. */
    GCPTRTYPE(struct PCIDevice *)   pPciDeviceGC;
    /** Pointer to the VM this instance was created for - GC Ptr. */
    GCPTRTYPE(PVM)                  pVMGC;
    /** GC pointer to associated PCI bus structure. */
    GCPTRTYPE(PPDMPCIBUS)           pPciBusGC;
#if GC_ARCH_BITS == 32
    uint32_t                        Alignment0;
#endif 
} PDMDEVINSINT;


/**
 * Private instance data.
 */
typedef struct PDMDRVINSINT
{
    /** Pointer to the driver instance above.
     * This is NULL for the topmost drive. */
    PPDMDRVINS          pUp;
    /** Pointer to the driver instance below.
     * This is NULL for the bottommost driver. */
    PPDMDRVINS          pDown;
    /** Pointer to the logical unit this driver chained on. */
    PPDMLUN             pLun;
    /** Pointer to driver structure from which this was instantiated. */
    PPDMDRV             pDrv;
    /** Pointer to the VM this instance was created for. */
    PVM                 pVM;
    /** Flag indicating that the driver is being detached and destroyed.
     * (Helps detect potential recursive detaching.) */
    bool                fDetaching;
    /** Configuration handle to the instance node. */
    PCFGMNODE           pCfgHandle;

} PDMDRVINSINT;


/**
 * Private critical section data.
 */
typedef struct PDMCRITSECTINT
{
    /** The critical section core which is shared with IPRT. */
    RTCRITSECT          Core;
    /** Pointer to the next critical section.
     * This chain is used for relocating pVMGC and device cleanup. */
    R3PTRTYPE(struct PDMCRITSECTINT *) pNext;
    /** Owner identifier.
     * This is pDevIns if the owner is a device. Similarily for a driver or service.
     * PDMR3CritSectInit() sets this to point to the critsect itself. */
    RTR3PTR             pvKey;
    /** Pointer to the VM - R3Ptr. */
    R3PTRTYPE(PVM)      pVMR3;
    /** Pointer to the VM - R0Ptr. */
    R0PTRTYPE(PVM)      pVMR0;
    /** Pointer to the VM - GCPtr. */
    GCPTRTYPE(PVM)      pVMGC;
#if GC_ARCH_BITS == 32
    uint32_t            u32Padding;
#endif
    /** R0/GC lock contention. */
    STAMCOUNTER         StatContentionR0GCLock;
    /** R0/GC unlock contention. */
    STAMCOUNTER         StatContentionR0GCUnlock;
    /** R3 lock contention. */
    STAMCOUNTER         StatContentionR3;
    /** Profiling the time the section is locked. */
    STAMPROFILEADV      StatLocked;
} PDMCRITSECTINT, *PPDMCRITSECTINT;


/* Must be included after PDMDEVINSINT is defined. */
#define PDMDEVINSINT_DECLARED
#define PDMDRVINSINT_DECLARED
#define PDMCRITSECTINT_DECLARED
#ifdef __VBox_pdm_h__
# error "Invalid header PDM order. Include PDMInternal.h before VBox/pdm.h!"
#endif
#include <VBox/pdm.h>



/**
 * PDM Logical Unit.
 *
 * This typically the representation of a physical port on a
 * device, like for instance the PS/2 keyboard port on the
 * keyboard controller device. The LUNs are chained on the
 * device the belong to (PDMDEVINSINT::pLunsHC).
 */
typedef struct PDMLUN
{
    /** The LUN - The Logical Unit Number. */
    RTUINT              iLun;
    /** Pointer to the next LUN. */
    PPDMLUN             pNext;
    /** Pointer to the top driver in the driver chain. */
    PPDMDRVINS          pTop;
    /** Pointer to the device instance which the LUN belongs to. */
    PPDMDEVINS          pDevIns;
    /** Pointer to the device base interface. */
    PPDMIBASE           pBase;
    /** Description of this LUN. */
    const char         *pszDesc;
} PDMLUN;


/**
 * PDM Device.
 */
typedef struct PDMDEV
{
    /** Pointer to the next device (HC Ptr). */
    HCPTRTYPE(PPDMDEV)                  pNext;
    /** Device name length. (search optimization) */
    RTUINT                              cchName;
    /** Registration structure. */
    HCPTRTYPE(const struct PDMDEVREG *) pDevReg;
    /** Number of instances. */
    RTUINT                              cInstances;
    /** Pointer to chain of instances (HC Ptr). */
    HCPTRTYPE(PPDMDEVINS)               pInstances;

} PDMDEV;


/**
 * PDM Driver.
 */
typedef struct PDMDRV
{
    /** Pointer to the next device. */
    PPDMDRV                 pNext;
    /** Registration structure. */
    const struct PDMDRVREG *pDrvReg;
    /** Number of instances. */
    RTUINT                  cInstances;

} PDMDRV;


/**
 * PDM registered PIC device.
 */
typedef struct PDMPIC
{
    /** Pointer to the PIC device instance - HC. */
    HCPTRTYPE(PPDMDEVINS)   pDevInsR3;
    /** @copydoc PDMPICREG::pfnSetIrqHC */
    DECLR3CALLBACKMEMBER(void, pfnSetIrqR3,(PPDMDEVINS pDevIns, int iIrq, int iLevel));
    /** @copydoc PDMPICREG::pfnGetInterruptHC */
    DECLR3CALLBACKMEMBER(int, pfnGetInterruptR3,(PPDMDEVINS pDevIns));

    /** Pointer to the PIC device instance - R0. */
    R0PTRTYPE(PPDMDEVINS)   pDevInsR0;
    /** @copydoc PDMPICREG::pfnSetIrqHC */
    DECLR0CALLBACKMEMBER(void, pfnSetIrqR0,(PPDMDEVINS pDevIns, int iIrq, int iLevel));
    /** @copydoc PDMPICREG::pfnGetInterruptHC */
    DECLR0CALLBACKMEMBER(int, pfnGetInterruptR0,(PPDMDEVINS pDevIns));

    /** Pointer to the PIC device instance - GC. */
    GCPTRTYPE(PPDMDEVINS)   pDevInsGC;
    /** @copydoc PDMPICREG::pfnSetIrqHC */
    DECLGCCALLBACKMEMBER(void, pfnSetIrqGC,(PPDMDEVINS pDevIns, int iIrq, int iLevel));
    /** @copydoc PDMPICREG::pfnGetInterruptHC */
    DECLGCCALLBACKMEMBER(int, pfnGetInterruptGC,(PPDMDEVINS pDevIns));
#if GC_ARCH_BITS == 32
    RTGCPTR                         GCPtrPadding; /**< Alignment padding. */
#endif
} PDMPIC;


/**
 * PDM registered APIC device.
 */
typedef struct PDMAPIC
{
    /** Pointer to the APIC device instance - HC Ptr. */
    PPDMDEVINSHC                    pDevInsR3;
    /** @copydoc PDMAPICREG::pfnGetInterruptHC */
    DECLR3CALLBACKMEMBER(int,       pfnGetInterruptR3,(PPDMDEVINS pDevIns));
    /** @copydoc PDMAPICREG::pfnSetBaseHC */
    DECLR3CALLBACKMEMBER(void,      pfnSetBaseR3,(PPDMDEVINS pDevIns, uint64_t u64Base));
    /** @copydoc PDMAPICREG::pfnGetBaseHC */
    DECLR3CALLBACKMEMBER(uint64_t,  pfnGetBaseR3,(PPDMDEVINS pDevIns));
    /** @copydoc PDMAPICREG::pfnSetTPRHC */
    DECLR3CALLBACKMEMBER(void,      pfnSetTPRR3,(PPDMDEVINS pDevIns, uint8_t u8TPR));
    /** @copydoc PDMAPICREG::pfnGetTPRHC */
    DECLR3CALLBACKMEMBER(uint8_t,   pfnGetTPRR3,(PPDMDEVINS pDevIns));
    /** @copydoc PDMAPICREG::pfnBusDeliverHC */
    DECLR3CALLBACKMEMBER(void,      pfnBusDeliverR3,(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                     uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode));

    /** Pointer to the PIC device instance - R0. */
    R0PTRTYPE(PPDMDEVINS)           pDevInsR0;
    /** @copydoc PDMAPICREG::pfnGetInterruptHC */
    DECLR0CALLBACKMEMBER(int,       pfnGetInterruptR0,(PPDMDEVINS pDevIns));
    /** @copydoc PDMAPICREG::pfnSetBaseHC */
    DECLR0CALLBACKMEMBER(void,      pfnSetBaseR0,(PPDMDEVINS pDevIns, uint64_t u64Base));
    /** @copydoc PDMAPICREG::pfnGetBaseHC */
    DECLR0CALLBACKMEMBER(uint64_t,  pfnGetBaseR0,(PPDMDEVINS pDevIns));
    /** @copydoc PDMAPICREG::pfnSetTPRHC */
    DECLR0CALLBACKMEMBER(void,      pfnSetTPRR0,(PPDMDEVINS pDevIns, uint8_t u8TPR));
    /** @copydoc PDMAPICREG::pfnGetTPRHC */
    DECLR0CALLBACKMEMBER(uint8_t,   pfnGetTPRR0,(PPDMDEVINS pDevIns));
    /** @copydoc PDMAPICREG::pfnBusDeliverHC */
    DECLR0CALLBACKMEMBER(void,      pfnBusDeliverR0,(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                     uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode));

    /** Pointer to the APIC device instance - GC Ptr. */
    PPDMDEVINSGC                    pDevInsGC;
    /** @copydoc PDMAPICREG::pfnGetInterruptHC */
    DECLGCCALLBACKMEMBER(int,       pfnGetInterruptGC,(PPDMDEVINS pDevIns));
    /** @copydoc PDMAPICREG::pfnSetBaseHC */
    DECLGCCALLBACKMEMBER(void,      pfnSetBaseGC,(PPDMDEVINS pDevIns, uint64_t u64Base));
    /** @copydoc PDMAPICREG::pfnGetBaseHC */
    DECLGCCALLBACKMEMBER(uint64_t,  pfnGetBaseGC,(PPDMDEVINS pDevIns));
    /** @copydoc PDMAPICREG::pfnSetTPRHC */
    DECLGCCALLBACKMEMBER(void,      pfnSetTPRGC,(PPDMDEVINS pDevIns, uint8_t u8TPR));
    /** @copydoc PDMAPICREG::pfnGetTPRHC */
    DECLGCCALLBACKMEMBER(uint8_t,   pfnGetTPRGC,(PPDMDEVINS pDevIns));
    /** @copydoc PDMAPICREG::pfnBusDeliverHC */
    DECLGCCALLBACKMEMBER(void,      pfnBusDeliverGC,(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                     uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode));
#if GC_ARCH_BITS == 32
    RTGCPTR                         GCPtrPadding; /**< Alignment padding. */
#endif
} PDMAPIC;


/**
 * PDM registered I/O APIC device.
 */
typedef struct PDMIOAPIC
{
    /** Pointer to the APIC device instance - HC Ptr. */
    PPDMDEVINSHC                    pDevInsR3;
    /** @copydoc PDMIOAPICREG::pfnSetIrqHC */
    DECLR3CALLBACKMEMBER(void,      pfnSetIrqR3,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /** Pointer to the PIC device instance - R0. */
    R0PTRTYPE(PPDMDEVINS)   pDevInsR0;
    /** @copydoc PDMIOAPICREG::pfnSetIrqHC */
    DECLR0CALLBACKMEMBER(void,      pfnSetIrqR0,(PPDMDEVINS pDevIns, int iIrq, int iLevel));

    /** Pointer to the APIC device instance - GC Ptr. */
    PPDMDEVINSGC                    pDevInsGC;
    /** @copydoc PDMIOAPICREG::pfnSetIrqHC */
    DECLGCCALLBACKMEMBER(void,      pfnSetIrqGC,(PPDMDEVINS pDevIns, int iIrq, int iLevel));
} PDMIOAPIC;


/**
 * PDM PCI Bus instance.
 */
typedef struct PDMPCIBUS
{
    /** PCI bus number. */
    RTUINT          iBus;
    RTUINT          uPadding0; /**< Alignment padding.*/

    /** Pointer to PCI Bus device instance. */
    PPDMDEVINSHC                    pDevInsR3;
    /** @copydoc PDMPCIBUSREG::pfnSetIrqHC */
    DECLR3CALLBACKMEMBER(void,      pfnSetIrqR3,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel));
    /** @copydoc PDMPCIBUSREG::pfnRegisterHC */
    DECLR3CALLBACKMEMBER(int,       pfnRegisterR3,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, const char *pszName, int iDev));
    /** @copydoc PDMPCIBUSREG::pfnIORegionRegisterHC */
    DECLR3CALLBACKMEMBER(int,       pfnIORegionRegisterR3,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iRegion, uint32_t cbRegion,
                                                           PCIADDRESSSPACE enmType, PFNPCIIOREGIONMAP pfnCallback));
    /** @copydoc PDMPCIBUSREG::pfnSaveExecHC */
    DECLR3CALLBACKMEMBER(int,       pfnSaveExecR3,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PSSMHANDLE pSSMHandle));
    /** @copydoc PDMPCIBUSREG::pfnLoadExecHC */
    DECLR3CALLBACKMEMBER(int,       pfnLoadExecR3,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PSSMHANDLE pSSMHandle));
    /** @copydoc PDMPCIBUSREG::pfnFakePCIBIOSHC */
    DECLR3CALLBACKMEMBER(int,       pfnFakePCIBIOSR3,(PPDMDEVINS pDevIns));

    /** Pointer to the PIC device instance - R0. */
    R0PTRTYPE(PPDMDEVINS)           pDevInsR0;
    /** @copydoc PDMPCIBUSREG::pfnSetIrqHC */
    DECLR0CALLBACKMEMBER(void,      pfnSetIrqR0,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel));

    /** Pointer to PCI Bus device instance. */
    PPDMDEVINSGC                    pDevInsGC;
    /** @copydoc PDMPCIBUSREG::pfnSetIrqHC */
    DECLGCCALLBACKMEMBER(void,      pfnSetIrqGC,(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel));
} PDMPCIBUS;


#ifdef IN_RING3
/**
 * PDM registered DMAC (DMA Controller) device.
 */
typedef struct PDMDMAC
{
    /** Pointer to the DMAC device instance. */
    PPDMDEVINS      pDevIns;
    /** Copy of the registration structure. */
    PDMDMACREG      Reg;
} PDMDMAC;


/**
 * PDM registered RTC (Real Time Clock) device.
 */
typedef struct PDMRTC
{
    /** Pointer to the RTC device instance. */
    PPDMDEVINS      pDevIns;
    /** Copy of the registration structure. */
    PDMRTCREG       Reg;
} PDMRTC;

#endif /* IN_RING3 */

/**
 * Module type.
 */
typedef enum PDMMODTYPE
{
    /** Guest context module. */
    PDMMOD_TYPE_GC,
    /** Ring-0 (host) context module. */
    PDMMOD_TYPE_R0,
    /** Ring-3 (host) context module. */
    PDMMOD_TYPE_R3
} PDMMODTYPE, *PPDMMODTYPE;


/** The module name length including the terminator. */
#define PDMMOD_NAME_LEN     32

/**
 * Loaded module instance.
 */
typedef struct PDMMOD
{
    /** Module name. This is used for refering to
     * the module internally, sort of like a handle. */
    char            szName[PDMMOD_NAME_LEN];
    /** Module type. */
    PDMMODTYPE      eType;
    /** Loader module handle. Not used for R0 modules. */
    RTLDRMOD        hLdrMod;
    /** Loaded address.
     * This is the 'handle' for R0 modules. */
    RTUINTPTR       ImageBase;
    /** Old loaded address.
     * This is used during relocation of GC modules. Not used for R0 modules. */
    RTUINTPTR       OldImageBase;
    /** Where the R3 HC bits are stored.
     * This can be equal to ImageBase but doesn't have to. Not used for R0 modules. */
    void           *pvBits;

    /** Pointer to next module. */
    struct PDMMOD  *pNext;
    /** Module filename. */
    char            szFilename[1];
} PDMMOD;
/** Pointer to loaded module instance. */
typedef PDMMOD *PPDMMOD;



/** Extra space in the free array. */
#define PDMQUEUE_FREE_SLACK     16

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
    HCPTRTYPE(PPDMQUEUE)    pNext;
    /** Type specific data. */
    union
    {
        /** PDMQUEUETYPE_DEV */
        struct
        {
            /** Pointer to consumer function. */
            HCPTRTYPE(PFNPDMQUEUEDEV)   pfnCallback;
            /** Pointer to the device instance owning the queue. */
            HCPTRTYPE(PPDMDEVINS)       pDevIns;
        } Dev;
        /** PDMQUEUETYPE_DRV */
        struct
        {
            /** Pointer to consumer function. */
            HCPTRTYPE(PFNPDMQUEUEDRV)   pfnCallback;
            /** Pointer to the driver instance owning the queue. */
            HCPTRTYPE(PPDMDRVINS)       pDrvIns;
        } Drv;
        /** PDMQUEUETYPE_INTERNAL */
        struct
        {
            /** Pointer to consumer function. */
            HCPTRTYPE(PFNPDMQUEUEINT)   pfnCallback;
        } Int;
        /** PDMQUEUETYPE_EXTERNAL */
        struct
        {
            /** Pointer to consumer function. */
            HCPTRTYPE(PFNPDMQUEUEEXT)   pfnCallback;
            /** Pointer to user argument. */
            HCPTRTYPE(void *)           pvUser;
        } Ext;
    } u;
    /** Queue type. */
    PDMQUEUETYPE                            enmType;
    /** The interval between checking the queue for events.
     * The realtime timer below is used to do the waiting.
     * If 0, the queue will use the VM_FF_PDM_QUEUE forced action. */
    uint32_t                                cMilliesInterval;
    /** Interval timer. Only used if cMilliesInterval is non-zero. */
    PTMTIMERHC                              pTimer;
    /** Pointer to the VM. */
    HCPTRTYPE(PVM)                          pVMHC;
    /** LIFO of pending items - HC. */
    HCPTRTYPE(PPDMQUEUEITEMCORE) volatile   pPendingHC;
    /** Pointer to the GC VM and indicator for GC enabled queue.
     * If this is NULL, the queue cannot be used in GC.
     */
    GCPTRTYPE(PVM)                          pVMGC;
    /** LIFO of pending items - GC. */
    GCPTRTYPE(PPDMQUEUEITEMCORE)            pPendingGC;
    /** Item size (bytes). */
    RTUINT                                  cbItem;
    /** Number of items in the queue. */
    RTUINT                                  cItems;
    /** Index to the free head (where we insert). */
    uint32_t volatile                       iFreeHead;
    /** Index to the free tail (where we remove). */
    uint32_t volatile                       iFreeTail;
    /** Array of pointers to free items. Variable size. */
    struct PDMQUEUEFREEITEM
    {
        /** Pointer to the free item - HC Ptr. */
        HCPTRTYPE(PPDMQUEUEITEMCORE) volatile pItemHC;
        /** Pointer to the free item - GC Ptr. */
        GCPTRTYPE(PPDMQUEUEITEMCORE) volatile pItemGC;
#if HC_ARCH_BITS == 64 && GC_ARCH_BITS == 32
        uint32_t                              Alignment0;
#endif 
    }                                       aFreeItems[1];
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
    PDMQUEUEITEMCORE        Core;
    /** Pointer to the device instance (HC Ptr). */
    HCPTRTYPE(PPDMDEVINS)   pDevInsHC;
    /** This operation to perform. */
    PDMDEVHLPTASKOP         enmOp;
#if HC_ARCH_BITS == 64
    uint32_t                Alignment0;
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
            int iIrq;
            /** The new level. */
            int iLevel;
        } SetIRQ;
    } u;
} PDMDEVHLPTASK;
/** Pointer to a queued Device Helper Task. */
typedef PDMDEVHLPTASK *PPDMDEVHLPTASK;
/** Pointer to a const queued Device Helper Task. */
typedef const PDMDEVHLPTASK *PCPDMDEVHLPTASK;



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

    /** Pointer to list of loaded modules. This is HC only! */
    HCPTRTYPE(PPDMMOD)              pModules;

    /** List of registered devices. (FIFO) */
    HCPTRTYPE(PPDMDEV)              pDevs;
    /** List of devices instances. (FIFO) */
    HCPTRTYPE(PPDMDEVINS)           pDevInstances;
    /** List of registered drivers. (FIFO) */
    HCPTRTYPE(PPDMDRV)              pDrvs;
    /** List of initialized critical sections. (LIFO) */
    HCPTRTYPE(PPDMCRITSECTINT)      pCritSects;
    /** PCI Buses. */
    PDMPCIBUS                       aPciBuses[1];
    /** The register PIC device. */
    PDMPIC                          Pic;
    /** The registerd APIC device. */
    PDMAPIC                         Apic;
    /** The registerd I/O APIC device. */
    PDMIOAPIC                       IoApic;
    /** The registered DMAC device. */
    HCPTRTYPE(PPDMDMAC)             pDmac;
    /** The registered RTC device. */
    HCPTRTYPE(PPDMRTC)              pRtc;

    /** Queue in which devhlp tasks are queued for R3 execution - HC Ptr. */
    HCPTRTYPE(PPDMQUEUE)            pDevHlpQueueHC;
    /** Queue in which devhlp tasks are queued for R3 execution - GC Ptr. */
    GCPTRTYPE(PPDMQUEUE)            pDevHlpQueueGC;

    /** The number of entries in the apQueuedCritSectsLeaves table that's currnetly in use. */
    RTUINT                          cQueuedCritSectLeaves;
    /** Critical sections queued in GC/R0 because of contention preventing leave to complete. (R3 Ptrs)
     * We will return to Ring-3 ASAP, so this queue doesn't has to be very long. */
    R3PTRTYPE(PPDMCRITSECT)         apQueuedCritSectsLeaves[8];

    /** Linked list of timer driven PDM queues. */
    HCPTRTYPE(struct PDMQUEUE *)    pQueuesTimer;
    /** Linked list of force action driven PDM queues. */
    HCPTRTYPE(struct PDMQUEUE *)    pQueuesForced;
    /** Pointer to the queue which should be manually flushed - HCPtr.
     * Only touched by EMT. */
    HCPTRTYPE(struct PDMQUEUE *)    pQueueFlushHC;
    /** Pointer to the queue which should be manually flushed - GCPtr. */
    GCPTRTYPE(struct PDMQUEUE *)    pQueueFlushGC;

    /** TEMPORARY HACKS FOR NETWORK POLLING.
     * @todo fix NAT and kill this!
     * @{ */
    RTUINT                          cPollers;
    HCPTRTYPE(PFNPDMDRVPOLLER)      apfnPollers[16];
    HCPTRTYPE(PPDMDRVINS)           aDrvInsPollers[16];
    PTMTIMERHC                      pTimerPollers;
    /** @} */

#ifdef VBOX_WITH_PDM_LOCK
    /** The PDM lock.
     * This is used to protect everything that deals with interrupts, i.e.
     * the PIC, APIC, IOAPIC and PCI devices pluss some PDM functions. */
    PDMCRITSECT                     CritSect;
#endif


    /** Number of times a critical section leave requesed needed to be queued for ring-3 execution. */
    STAMCOUNTER                     StatQueuedCritSectLeaves;
} PDM;
/** Pointer to PDM VM instance data. */
typedef PDM *PPDM;



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifdef IN_RING3
extern const PDMDRVHLP g_pdmR3DrvHlp;
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
int         pdmR3CritSectInit(PVM pVM);
int         pdmR3CritSectTerm(PVM pVM);
void        pdmR3CritSectRelocate(PVM pVM);
int         pdmR3CritSectInitDevice(PVM pVM, PPDMDEVINS pDevIns, PPDMCRITSECT pCritSect, const char *pszName);
int         pdmR3CritSectDeleteDevice(PVM pVM, PPDMDEVINS pDevIns);

int         pdmR3DevInit(PVM pVM);
PPDMDEV     pdmR3DevLookup(PVM pVM, const char *pszName);
int         pdmR3DevFindLun(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun, PPDMLUN *ppLun);

int         pdmR3DrvInit(PVM pVM);
int         pdmR3DrvDetach(PPDMDRVINS pDrvIns);
PPDMDRV     pdmR3DrvLookup(PVM pVM, const char *pszName);

int         pdmR3LdrInit(PVM pVM);
void        pdmR3LdrTerm(PVM pVM);
char *      pdmR3FileR3(const char *pszFile);
int         pdmR3LoadR3(PVM pVM, const char *pszFilename, const char *pszName);

void        pdmR3QueueRelocate(PVM pVM, RTGCINTPTR offDelta);

#ifdef VBOX_WITH_PDM_LOCK
void        pdmLock(PVM pVM);
int         pdmLockEx(PVM pVM, int rc);
void        pdmUnlock(PVM pVM);
#else
# define pdmLock(pVM)       do {} while (0)
# define pdmLockEx(pVM, rc) (VINF_SUCCESS)
# define pdmUnlock(pVM)     do {} while (0)
#endif

/** @} */

__END_DECLS

#endif
