/* $Id$ */

/** @file
 * VirtioCore.h - Virtio Declarations
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_VirtIO_VirtioCore_h
#define VBOX_INCLUDED_SRC_VirtIO_VirtioCore_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/ctype.h>
#include <iprt/sg.h>

#ifdef LOG_ENABLED
# define VIRTIO_HEX_DUMP(logLevel, pv, cb, base, title) \
    do { \
        if (LogIsItEnabled(logLevel, LOG_GROUP)) \
            virtioCoreHexDump((pv), (cb), (base), (title)); \
    } while (0)
#else
# define VIRTIO_HEX_DUMP(logLevel, pv, cb, base, title) do { } while (0)
#endif

/** Pointer to the shared VirtIO state. */
typedef struct VIRTIOCORE *PVIRTIOCORE;
/** Pointer to the ring-3 VirtIO state. */
typedef struct VIRTIOCORER3 *PVIRTIOCORER3;
/** Pointer to the ring-0 VirtIO state. */
typedef struct VIRTIOCORER0 *PVIRTIOCORER0;
/** Pointer to the raw-mode VirtIO state. */
typedef struct VIRTIOCORERC *PVIRTIOCORERC;
/** Pointer to the instance data for the current context. */
typedef CTX_SUFF(PVIRTIOCORE) PVIRTIOCORECC;

#define VIRTIO_MAX_VIRTQ_NAME_SIZE          32                   /**< Maximum length of a queue name           */
#define VIRTQ_MAX_ENTRIES                   1024                 /**< Max size (# desc elements) of a virtq    */
#define VIRTQ_MAX_COUNT                     24                   /**< Max queues we allow guest to create      */
#define VIRTIO_NOTIFY_OFFSET_MULTIPLIER     2                    /**< VirtIO Notify Cap. MMIO config param     */
#define VIRTIO_REGION_PCI_CAP               2                    /**< BAR for VirtIO Cap. MMIO (impl specific) */
#define VIRTIO_REGION_MSIX_CAP              0                    /**< Bar for MSI-X handling                   */


/** The following virtioCoreGCPhysChain*() functions mimic the functionality of the related RT s/g functions,
 *  except they work with the data type GCPhys rather than void *
 */
typedef struct VIRTIOSGSEG                                      /**< An S/G entry                              */
{
    RTGCPHYS GCPhys;                                            /**< Pointer to the segment buffer             */
    size_t  cbSeg;                                              /**< Size of the segment buffer                */
} VIRTIOSGSEG;

typedef VIRTIOSGSEG *PVIRTIOSGSEG, **PPVIRTIOSGSEG;
typedef const VIRTIOSGSEG *PCVIRTIOSGSEG;

typedef struct VIRTIOSGBUF
{
    PVIRTIOSGSEG paSegs;                                        /**< Pointer to the scatter/gather array       */
    unsigned  cSegs;                                            /**< Number of segs in scatter/gather array    */
    unsigned  idxSeg;                                           /**< Current segment we are in                 */
    RTGCPHYS  GCPhysCur;                                        /**< Ptr to byte within the current seg        */
    size_t    cbSegLeft;                                        /**< # of bytes left in the current segment    */
} VIRTIOSGBUF;

typedef VIRTIOSGBUF *PVIRTIOSGBUF, **PPVIRTIOSGBUF;
typedef const VIRTIOSGBUF *PCVIRTIOSGBUF;

/**
 * VirtIO buffers are descriptor chains (scatter-gather vectors). Each buffer is described
 * by the index of its head descriptor, which in optionally chains to another descriptor and so on.
 * Each descriptor, e.g. [len, GCPhys] pair in the chain represents either an OUT segment (e.g. guest-to-host)
 * or an IN segment (host-to-guest). A VIRTQBUF is created and retured from a call to virtioCoreR3VirtqAvailBufPeek()
 * or virtioCoreR3VirtqAvailBufGet(). That function consolodates the VirtIO descriptor chain into a
 * representation, where pSgPhysSend is GCPhys s/g buffer containing all of the OUT descriptors and pSgPhysReturn \
 * is a GCPhys s/g buffer containing all of IN descriptors to be filled with data on the host to return to the
 * guest.
 */
typedef struct VIRTQBUF
{
    uint32_t            u32Magic;                                /**< Magic value, VIRTQBUF_MAGIC.             */
    uint16_t            uVirtq;                                  /**< VirtIO index of associated virtq         */
    uint16_t            pad;
    uint32_t volatile   cRefs;                                   /**< Reference counter.                       */
    uint32_t            uHeadIdx;                                /**< Head idx of associated desc chain        */
    size_t              cbPhysSend;                              /**< Total size of src buffer                 */
    PVIRTIOSGBUF        pSgPhysSend;                             /**< Phys S/G buf for data from guest         */
    size_t              cbPhysReturn;                            /**< Total size of dst buffer                 */
    PVIRTIOSGBUF        pSgPhysReturn;                           /**< Phys S/G buf to store result for guest   */

    /** @name Internal (bird combined 5 allocations into a single), fingers off.
     * @{ */
    VIRTIOSGBUF         SgBufIn;
    VIRTIOSGBUF         SgBufOut;
    VIRTIOSGSEG         aSegsIn[VIRTQ_MAX_ENTRIES];
    VIRTIOSGSEG         aSegsOut[VIRTQ_MAX_ENTRIES];
    /** @} */
} VIRTQBUF_T;

/** Pointers to a Virtio descriptor chain. */
typedef VIRTQBUF_T *PVIRTQBUF, **PPVIRTQBUF;

/** Magic value for VIRTQBUF_T::u32Magic. */
#define VIRTQBUF_MAGIC             UINT32_C(0x19600219)

typedef struct VIRTIOPCIPARAMS
{
    uint16_t  uDeviceId;                                         /**< PCI Cfg Device ID                         */
    uint16_t  uClassBase;                                        /**< PCI Cfg Base Class                        */
    uint16_t  uClassSub;                                         /**< PCI Cfg Subclass                          */
    uint16_t  uClassProg;                                        /**< PCI Cfg Programming Interface Class       */
    uint16_t  uSubsystemId;                                      /**< PCI Cfg Card Manufacturer Vendor ID       */
    uint16_t  uInterruptLine;                                    /**< PCI Cfg Interrupt line                    */
    uint16_t  uInterruptPin;                                     /**< PCI Cfg Interrupt pin                     */
} VIRTIOPCIPARAMS, *PVIRTIOPCIPARAMS;

#define VIRTIO_F_VERSION_1                  RT_BIT_64(32)        /**< Required feature bit for 1.0 devices      */
#define VIRTIO_F_INDIRECT_DESC              RT_BIT_64(28)        /**< Allow descs to point to list of descs     */
#define VIRTIO_F_EVENT_IDX                  RT_BIT_64(29)        /**< Allow notification disable for n elems    */
#define VIRTIO_F_RING_INDIRECT_DESC         RT_BIT_64(28)        /**< Doc bug: Goes under two names in spec     */
#define VIRTIO_F_RING_EVENT_IDX             RT_BIT_64(29)        /**< Doc bug: Goes under two names in spec     */

#define VIRTIO_DEV_INDEPENDENT_FEATURES_OFFERED ( 0 )            /**< TBD: Add VIRTIO_F_INDIRECT_DESC           */

#define VIRTIO_ISR_VIRTQ_INTERRUPT           RT_BIT_32(0)        /**< Virtq interrupt bit of ISR register       */
#define VIRTIO_ISR_DEVICE_CONFIG             RT_BIT_32(1)        /**< Device configuration changed bit of ISR   */
#define DEVICE_PCI_VENDOR_ID_VIRTIO                0x1AF4        /**< Guest driver locates dev via (mandatory)  */
#define DEVICE_PCI_REVISION_ID_VIRTIO                   1        /**< VirtIO 1.0 non-transitional drivers >= 1  */

/** Reserved (*negotiated*) Feature Bits (e.g. device independent features, VirtIO 1.0 spec,section 6) */

#define VIRTIO_MSI_NO_VECTOR                       0xffff        /**< Vector value to disable MSI for queue     */

/** Device Status field constants (from Virtio 1.0 spec) */
#define VIRTIO_STATUS_ACKNOWLEDGE                    0x01        /**< Guest driver: Located this VirtIO device  */
#define VIRTIO_STATUS_DRIVER                         0x02        /**< Guest driver: Can drive this VirtIO dev.  */
#define VIRTIO_STATUS_DRIVER_OK                      0x04        /**< Guest driver: Driver set-up and ready     */
#define VIRTIO_STATUS_FEATURES_OK                    0x08        /**< Guest driver: Feature negotiation done    */
#define VIRTIO_STATUS_FAILED                         0x80        /**< Guest driver: Fatal error, gave up        */
#define VIRTIO_STATUS_DEVICE_NEEDS_RESET             0x40        /**< Device experienced unrecoverable error    */

typedef enum VIRTIOVMSTATECHANGED
{
    kvirtIoVmStateChangedInvalid = 0,
    kvirtIoVmStateChangedReset,
    kvirtIoVmStateChangedSuspend,
    kvirtIoVmStateChangedPowerOff,
    kvirtIoVmStateChangedResume,
    kvirtIoVmStateChangedFor32BitHack = 0x7fffffff
} VIRTIOVMSTATECHANGED;

/** @def Virtio Device PCI Capabilities type codes */
#define VIRTIO_PCI_CAP_COMMON_CFG                       1        /**< Common configuration PCI capability ID    */
#define VIRTIO_PCI_CAP_NOTIFY_CFG                       2        /**< Notification area PCI capability ID       */
#define VIRTIO_PCI_CAP_ISR_CFG                          3        /**< ISR PCI capability id                     */
#define VIRTIO_PCI_CAP_DEVICE_CFG                       4        /**< Device-specific PCI cfg capability ID     */
#define VIRTIO_PCI_CAP_PCI_CFG                          5        /**< PCI CFG capability ID                     */

#define VIRTIO_PCI_CAP_ID_VENDOR                     0x09        /**< Vendor-specific PCI CFG Device Cap. ID    */

/**
 * The following is the PCI capability struct common to all VirtIO capability types
 */
typedef struct virtio_pci_cap
{
    /* All little-endian */
    uint8_t   uCapVndr;                                          /**< Generic PCI field: PCI_CAP_ID_VNDR        */
    uint8_t   uCapNext;                                          /**< Generic PCI field: next ptr.              */
    uint8_t   uCapLen;                                           /**< Generic PCI field: capability length      */
    uint8_t   uCfgType;                                          /**< Identifies the structure.                 */
    uint8_t   uBar;                                              /**< Where to find it.                         */
    uint8_t   uPadding[3];                                       /**< Pad to full dword.                        */
    uint32_t  uOffset;                                           /**< Offset within bar.  (L.E.)                */
    uint32_t  uLength;                                           /**< Length of struct, in bytes. (L.E.)        */
}  VIRTIO_PCI_CAP_T, *PVIRTIO_PCI_CAP_T;

/**
 * VirtIO 1.0 Capabilities' related MMIO-mapped structs:
 *
 * Note: virtio_pci_device_cap is dev-specific, implemented by client. Definition unknown here.
 */
typedef struct virtio_pci_common_cfg
{
    /* Device-specific fields */
    uint32_t  uDeviceFeaturesSelect;                             /**< RW (driver selects device features)       */
    uint32_t  uDeviceFeatures;                                   /**< RO (device reports features to driver)    */
    uint32_t  uDriverFeaturesSelect;                             /**< RW (driver selects driver features)       */
    uint32_t  uDriverFeatures;                                   /**< RW (driver-accepted device features)      */
    uint16_t  uMsixConfig;                                       /**< RW (driver sets MSI-X config vector)      */
    uint16_t  uNumVirtqs;                                        /**< RO (device specifies max queues)          */
    uint8_t   fDeviceStatus;                                     /**< RW (driver writes device status, 0=reset) */
    uint8_t   uConfigGeneration;                                 /**< RO (device changes when changing configs) */

    /* Virtq-specific fields (values reflect (via MMIO) info related to queue indicated by uVirtqSelect. */
    uint16_t  uVirtqSelect;                                      /**< RW (selects queue focus for these fields) */
    uint16_t  uSize;                                             /**< RW (queue size, 0 - 2^n)                  */
    uint16_t  uMsix;                                             /**< RW (driver selects MSI-X queue vector)    */
    uint16_t  uEnable;                                           /**< RW (driver controls usability of queue)   */
    uint16_t  uNotifyOffset;                                     /**< RO (offset into virtqueue; see spec)      */
    uint64_t  GCPhysVirtqDesc;                                   /**< RW (driver writes desc table phys addr)   */
    uint64_t  GCPhysVirtqAvail;                                  /**< RW (driver writes avail ring phys addr)   */
    uint64_t  GCPhysVirtqUsed;                                   /**< RW (driver writes used ring  phys addr)   */
} VIRTIO_PCI_COMMON_CFG_T, *PVIRTIO_PCI_COMMON_CFG_T;

typedef struct virtio_pci_notify_cap
{
    struct virtio_pci_cap pciCap;                                /**< Notification MMIO mapping capability      */
    uint32_t uNotifyOffMultiplier;                               /**< notify_off_multiplier                     */
} VIRTIO_PCI_NOTIFY_CAP_T, *PVIRTIO_PCI_NOTIFY_CAP_T;

typedef struct virtio_pci_cfg_cap
{
    struct virtio_pci_cap pciCap;                                /**< Cap. defines the BAR/off/len to access    */
    uint8_t uPciCfgData[4];                                      /**< I/O buf for above cap.                    */
} VIRTIO_PCI_CFG_CAP_T, *PVIRTIO_PCI_CFG_CAP_T;

/**
 * PCI capability data locations (PCI CFG and MMIO).
 */
typedef struct VIRTIO_PCI_CAP_LOCATIONS_T
{
    uint16_t        offMmio;
    uint16_t        cbMmio;
    uint16_t        offPci;
    uint16_t        cbPci;
} VIRTIO_PCI_CAP_LOCATIONS_T;

typedef struct VIRTQUEUE
{
    RTGCPHYS                    GCPhysVirtqDesc;                  /**< (MMIO) PhysAdr per-Q desc structs   GUEST */
    RTGCPHYS                    GCPhysVirtqAvail;                 /**< (MMIO) PhysAdr per-Q avail structs  GUEST */
    RTGCPHYS                    GCPhysVirtqUsed;                  /**< (MMIO) PhysAdr per-Q used structs   GUEST */
    uint16_t                    uNotifyOffset;                    /**< (MMIO) per-Q notify offset           HOST */
    uint16_t                    uMsix;                            /**< (MMIO) Per-queue vector for MSI-X   GUEST */
    uint16_t                    uEnable;                          /**< (MMIO) Per-queue enable             GUEST */
    uint16_t                    uSize;                            /**< (MMIO) Per-queue size          HOST/GUEST */
    uint16_t                    uAvailIdxShadow;                  /**< Consumer's position in avail ring         */
    uint16_t                    uUsedIdxShadow;                   /**< Consumer's position in used ring          */
    bool                        fUsedRingEvent;                   /**< Flags if used idx to notify guest reached */
    uint16_t                    uVirtq;                           /**< Index of this queue                       */
    char                        szName[32];                       /**< Dev-specific name of queue                */
    uint8_t                     padding[3];
} VIRTQUEUE, *PVIRTQUEUE;

/**
 * The core/common state of the VirtIO PCI devices, shared edition.
 */
typedef struct VIRTIOCORE
{
    char                        szInstance[16];                   /**< Instance name, e.g. "VIRTIOSCSI0"         */
    PPDMDEVINS                  pDevInsR0;                        /**< Client device instance                    */
    PPDMDEVINS                  pDevInsR3;                        /**< Client device instance                    */
    VIRTQUEUE                   aVirtqueues[VIRTQ_MAX_COUNT];     /**< (MMIO) VirtIO contexts for queues         */
    uint64_t                    uDeviceFeatures;                  /**< (MMIO) Host features offered         HOST */
    uint64_t                    uDriverFeatures;                  /**< (MMIO) Host features accepted       GUEST */
    uint32_t                    uDeviceFeaturesSelect;            /**< (MMIO) hi/lo select uDeviceFeatures GUEST */
    uint32_t                    uDriverFeaturesSelect;            /**< (MMIO) hi/lo select uDriverFeatures GUEST */
    uint32_t                    uMsixConfig;                      /**< (MMIO) MSI-X vector                 GUEST */
    uint8_t                     fDeviceStatus;                    /**< (MMIO) Device Status                GUEST */
    uint8_t                     uPrevDeviceStatus;                /**< (MMIO) Prev Device Status           GUEST */
    uint8_t                     uConfigGeneration;                /**< (MMIO) Device config sequencer       HOST */

    /** @name The locations of the capability structures in PCI config space and the BAR.
     * @{ */
    VIRTIO_PCI_CAP_LOCATIONS_T  LocPciCfgCap;                     /**< VIRTIO_PCI_CFG_CAP_T                      */
    VIRTIO_PCI_CAP_LOCATIONS_T  LocNotifyCap;                     /**< VIRTIO_PCI_NOTIFY_CAP_T                   */
    VIRTIO_PCI_CAP_LOCATIONS_T  LocCommonCfgCap;                  /**< VIRTIO_PCI_CAP_T                          */
    VIRTIO_PCI_CAP_LOCATIONS_T  LocIsrCap;                        /**< VIRTIO_PCI_CAP_T                          */
    VIRTIO_PCI_CAP_LOCATIONS_T  LocDeviceCap;                     /**< VIRTIO_PCI_CAP_T + custom data.           */
    /** @} */

    uint16_t                    uVirtqSelect;                     /**< (MMIO) queue selector               GUEST */
    bool                        fGenUpdatePending;                /**< If set, update cfg gen after driver reads */
    uint8_t                     uPciCfgDataOff;                   /**< Offset to PCI configuration data area     */
    uint8_t                     uISR;                             /**< Interrupt Status Register.                */
    uint8_t                     fMsiSupport;                      /**< Flag set if using MSI instead of ISR      */
    /** The MMIO handle for the PCI capability region (\#2). */
    IOMMMIOHANDLE               hMmioPciCap;

    /** @name Statistics
     * @{ */
    STAMCOUNTER                 StatDescChainsAllocated;
    STAMCOUNTER                 StatDescChainsFreed;
    STAMCOUNTER                 StatDescChainsSegsIn;
    STAMCOUNTER                 StatDescChainsSegsOut;
    /** @} */
} VIRTIOCORE;

#define MAX_NAME 64

/**
 * The core/common state of the VirtIO PCI devices, ring-3 edition.
 */
typedef struct VIRTIOCORER3
{
    /** @name Callbacks filled by the device before calling virtioCoreR3Init.
     * @{  */
    /**
     * Implementation-specific client callback to notify client of significant device status
     * changes.
     *
     * @param   pVirtio    Pointer to the shared virtio state.
     * @param   pVirtioCC  Pointer to the ring-3 virtio state.
     * @param   fDriverOk  True if guest driver is okay (thus queues, etc... are
     *                     valid)
     */
    DECLCALLBACKMEMBER(void, pfnStatusChanged,(PVIRTIOCORE pVirtio, PVIRTIOCORECC pVirtioCC, uint32_t fDriverOk));

    /**
     * Implementation-specific client callback to access VirtIO Device-specific capabilities
     * (other VirtIO capabilities and features are handled in VirtIO implementation)
     *
     * @param   pDevIns    The device instance.
     * @param   offCap     Offset within device specific capabilities struct.
     * @param   pvBuf      Buffer in which to save read data.
     * @param   cbToRead   Number of bytes to read.
     */
    DECLCALLBACKMEMBER(int, pfnDevCapRead,(PPDMDEVINS pDevIns, uint32_t offCap, void *pvBuf, uint32_t cbToRead));

    /**
     * Implementation-specific client ballback to access VirtIO Device-specific capabilities
     * (other VirtIO capabilities and features are handled in VirtIO implementation)
     *
     * @param   pDevIns    The device instance.
     * @param   offCap     Offset within device specific capabilities struct.
     * @param   pvBuf      Buffer with the bytes to write.
     * @param   cbToWrite  Number of bytes to write.
     */
    DECLCALLBACKMEMBER(int, pfnDevCapWrite,(PPDMDEVINS pDevIns, uint32_t offCap, const void *pvBuf, uint32_t cbWrite));


    /**
     * When guest-to-host queue notifications are enabled, the guest driver notifies the host
     * that the avail queue has buffers, and this callback informs the client.
     *
     * @param   pVirtio    Pointer to the shared virtio state.
     * @param   pVirtioCC  Pointer to the ring-3 virtio state.
     * @param   uVirtqNbr   Index of the notified queue
     */
    DECLCALLBACKMEMBER(void, pfnVirtqNotified,(PPDMDEVINS pDevIns, PVIRTIOCORE pVirtio, uint16_t uVirtqNbr));

    /** @} */

    R3PTRTYPE(PVIRTIO_PCI_CFG_CAP_T)    pPciCfgCap;                /**< Pointer to struct in PCI config area.     */
    R3PTRTYPE(PVIRTIO_PCI_NOTIFY_CAP_T) pNotifyCap;                /**< Pointer  to struct in PCI config area.    */
    R3PTRTYPE(PVIRTIO_PCI_CAP_T)        pCommonCfgCap;             /**< Pointer to struct in PCI config area.     */
    R3PTRTYPE(PVIRTIO_PCI_CAP_T)        pIsrCap;                   /**< Pointer to struct in PCI config area.     */
    R3PTRTYPE(PVIRTIO_PCI_CAP_T)        pDeviceCap;                /**< Pointer to struct in PCI config area.     */

    uint32_t                    cbDevSpecificCfg;                  /**< Size of client's dev-specific config data */
    R3PTRTYPE(uint8_t *)        pbDevSpecificCfg;                  /**< Pointer to client's struct                */
    R3PTRTYPE(uint8_t *)        pbPrevDevSpecificCfg;              /**< Previous read dev-specific cfg of client  */
    bool                        fGenUpdatePending;                 /**< If set, update cfg gen after driver reads */
    char                        pcszMmioName[MAX_NAME];            /**< MMIO mapping name                         */
} VIRTIOCORER3;

/**
 * The core/common state of the VirtIO PCI devices, ring-0 edition.
 */
typedef struct VIRTIOCORER0
{
    /**
     * When guest-to-host queue notifications are enabled, the guest driver notifies the host
     * that the avail queue has buffers, and this callback informs the client.
     *
     * @param   pVirtio    Pointer to the shared virtio state.
     * @param   pVirtioCC  Pointer to the ring-3 virtio state.
     * @param   uVirtqNbr  Index of the notified queue
     */
    DECLCALLBACKMEMBER(void, pfnVirtqNotified,(PPDMDEVINS pDevIns, PVIRTIOCORE pVirtio, uint16_t uVirtqNbr));

} VIRTIOCORER0;

/**
 * The core/common state of the VirtIO PCI devices, raw-mode edition.
 */
typedef struct VIRTIOCORERC
{
    uint64_t                    uUnusedAtTheMoment;
} VIRTIOCORERC;


/** @typedef VIRTIOCORECC
 * The instance data for the current context. */
typedef CTX_SUFF(VIRTIOCORE) VIRTIOCORECC;


/** @name API for VirtIO parent device
 * @{ */

/**
 * Initiate orderly reset procedure. This is an exposed API for clients that might need it.
 * Invoked by client to reset the device and driver (see VirtIO 1.0 section 2.1.1/2.1.2)
 *
 * @param   pVirtio     Pointer to the virtio state.
 */
void  virtioCoreResetAll(PVIRTIOCORE pVirtio);

/**
 * 'Attaches' the inheriting device-specific code's queue state to VirtIO core
 * queue management, informing the core of the name of the queue associated spec-defined
 * device specific queue number (which also happens to be the index into VirtIO's array
 * of queue structs].  uVirtqNbr is used as the 'handle' for virt queues in this API.
 * They are unambiguous (the VirtIO specification defines how virtq numbers are assigned
 * for each device type), and it helps prevent muddying of the core state by devices
 * and vice versa by eliminating direct access to otherwise private structs.
 *
 * @param   pVirtio     Pointer to the shared virtio state.
 * @param   uVirtqNbr   Virtq number
 * @param   pcszName    Name to give queue
 *
 * @returns VBox status code.
 */
int  virtioCoreR3VirtqAttach(PVIRTIOCORE pVirtio, uint16_t uVirtqNbr, const char *pcszName);

/**
 * Enables or disables a virtq
 *
 * @param   pVirtio     Pointer to the shared virtio state.
 * @param   uVirtqNbr   Virtq number
 * @param   fEnable     Flags whether to enable or disable the virtq
 *
 */
void  virtioCoreVirtqEnable(PVIRTIOCORE pVirtio, uint16_t uVirtqNbr, bool fEnable);

/**
 * Enable or disable notification for the specified queue.
 *
 * I.e. The notification enable mode informs guest driver whether or not to to notify the
 * host device (via MMIO access to the queue notification area described in VirtIO 1.0, Section 4.1.4.4
 * "Notification structure layout" whenever the guest driver adds a new entry to the avail ring of the queue).
 *
 * Note: In the VirtIO world, the device sets flags in the used ring to communicate to the driver how to
 * handle notifications for the avail ring and the drivers sets flags in the avail ring to communicate
 * to the device how to handle sending interrupts for the used ring.
 *
 * @param   pVirtio     Pointer to the shared virtio state.
 * @param   uVirtqNbr   Virtq number
 * @param   fEnable     Selects notification mode (enabled or disabled)
 */
void  virtioCoreVirtqEnableNotify(PVIRTIOCORE pVirtio, uint16_t uVirtqNbr, bool fEnable);

/**
 * Notifies guest (via ISR or MSI-X) of device configuration change
 *
 * @param   pVirtio     Pointer to the shared virtio state.
 */
void  virtioCoreNotifyConfigChanged(PVIRTIOCORE pVirtio);

/**
 * Displays the VirtIO spec-related features offered by the core component,
 * as well as which features have been negotiated and accepted or declined by the guest driver,
 * providing a summary view of the configuration the device is operating with.
 *
 * @param   pVirtio     Pointer to the shared virtio state.
 * @param   pHlp        Pointer to the debug info hlp struct
 */
void  virtioCorePrintFeatures(VIRTIOCORE *pVirtio, PCDBGFINFOHLP pHlp);

/*
 * Debuging assist feature displays the state of the VirtIO core code, which includes
 * an overview of the state of all of the queues.
 *
 * This can be invoked when running the VirtualBox debugger, or from the command line
 * using the command: "VboxManage debugvm <VM name or id> info <device name> [args]"
 *
 * Example:  VBoxManage debugvm myVnetVm info "virtio-net" help
 *
 * This is implemented currently to be invoked by the inheriting device-specific code
 * (see DevVirtioNet for an example, which receives the debugvm callback directly).
 * DevVirtioNet lists the available sub-options if no arguments are provided. In that
 * example this virtq info related function is invoked hierarchically when virtio-net
 * displays its device-specific queue info.
 *
 * @param   pDevIns     The device instance.
 * @param   pHlp        Pointer to the debug info hlp struct
 * @param   pszArgs     Arguments to function
 */
void  virtioCoreR3VirtqInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs, int uVirtqNbr);

/**
 * Returns the number of avail bufs in the virtq.
 *
 * @param   pDevIns     The device instance.
 * @param   pVirtio     Pointer to the shared virtio state.
 * @param   uVirtqNbr   Virtqueue to return the count of buffers available for.
 */
uint16_t virtioCoreVirtqAvailBufCount(PPDMDEVINS pDevIns, PVIRTIOCORE pVirtio, uint16_t uVirtqNbr);

/**
 * This function is identical to virtioCoreR3VirtqAvailBufGet(), except it doesn't 'consume'
 * the buffer from the avail ring of the virtq. The peek operation becomes identical to a get
 * operation if virtioCoreR3VirtqAvailRingNext() is called to consume the buffer from the avail ring,
 * at which point virtioCoreR3VirtqUsedBufPut() must be called to complete the roundtrip
 * transaction by putting the descriptor on the used ring.
 *
 *
 * @param   pDevIns     The device instance.
 * @param   pVirtio     Pointer to the shared virtio state.
 * @param   uVirtqNbr   Virtq number
 * @param   ppVirtqBuf  Address to store pointer to descriptor chain that contains the
 *                      pre-processed transaction information pulled from the virtq.
 *
 * @returns VBox status code:
 * @retval  VINF_SUCCESS         Success
 * @retval  VERR_INVALID_STATE   VirtIO not in ready state (asserted).
 * @retval  VERR_NOT_AVAILABLE   If the queue is empty.
 */
int  virtioCoreR3VirtqAvailBufPeek(PPDMDEVINS pDevIns, PVIRTIOCORE pVirtio, uint16_t uVirtqNbr,
                                   PPVIRTQBUF ppVirtqBuf);

/**
 * This function fetches the next buffer (descriptor chain) from the VirtIO "avail" ring of
 * indicated queue, and converts the buf's s/g vectors into OUT (e.g. guest-to-host)
 * components and and IN (host-to-guest) components.
 *
 * The caller is responsible for GCPhys to host virtual memory conversions. If the
 * virtq buffer being peeked at is "consumed", virtioCoreR3VirtqAvailRingNext() must
 * be called and in that case virtioCoreR3VirtqUsedBufPut() must be called to
 * complete the roundtrip virtq transaction.
 *
 * @param   pDevIns     The device instance.
 * @param   pVirtio     Pointer to the shared virtio state.
 * @param   uVirtqNbr   Virtq number
 * @param   ppVirtqBuf  Address to store pointer to descriptor chain that contains the
 *                      pre-processed transaction information pulled from the virtq.
 *                      Returned reference must be released by calling
 *                      virtioCoreR3VirtqBufRelease().
 * @param   fRemove     flags whether to remove desc chain from queue (false = peek)
 *
 * @returns VBox status code:
 * @retval  VINF_SUCCESS         Success
 * @retval  VERR_INVALID_STATE   VirtIO not in ready state (asserted).
 * @retval  VERR_NOT_AVAILABLE   If the queue is empty.
 */
int  virtioCoreR3VirtqAvailBufGet(PPDMDEVINS pDevIns, PVIRTIOCORE pVirtio, uint16_t uVirtqNbr,
                                  PPVIRTQBUF ppVirtqBuf, bool fRemove);

/**
 * Fetches a specific descriptor chain using avail ring of indicated queue and converts the descriptor
 * chain into its OUT (to device) and IN to guest components.
 *
 * The caller is responsible for GCPhys to host virtual memory conversions and *must*
 * return the virtq buffer using virtioCoreR3VirtqUsedBufPut() to complete the roundtrip
 * virtq transaction.
 * *
 * @param   pDevIns     The device instance.
 * @param   pVirtio     Pointer to the shared virtio state.
 * @param   uVirtqNbr   Virtq number
 * @param   ppVirtqBuf  Address to store pointer to descriptor chain that contains the
 *                      pre-processed transaction information pulled from the virtq.
 *                      Returned reference must be released by calling
 *                      virtioCoreR3VirtqBufRelease().
 * @param   fRemove     flags whether to remove desc chain from queue (false = peek)
 *
 * @returns VBox status code:
 * @retval  VINF_SUCCESS         Success
 * @retval  VERR_INVALID_STATE   VirtIO not in ready state (asserted).
 * @retval  VERR_NOT_AVAILABLE   If the queue is empty.
 */
int virtioCoreR3VirtqAvailBufGet(PPDMDEVINS pDevIns, PVIRTIOCORE pVirtio, uint16_t uVirtqNbr,
                                  uint16_t uHeadIdx, PPVIRTQBUF ppVirtqBuf);

/**
 * Returns data to the guest to complete a transaction initiated by virtioCoreR3VirtqAvailBufGet(),
 * or virtioCoreR3VirtqAvailBufPeek()/virtioCoreR3VirtqBufSync() call pairs to complete each
 * intervening a roundtrip transaction, wherein I/O transactions are always initiated by
 * the guest and completed by the host. In other words, for the host to send any data to the
 * guest, the guest must provide buffers, for the host to fill, via the avail ring of the
 * virtq.
 *
 * At some some point virtioCoreR3VirtqUsedRingSync() must be called to return data to the guest,
 * completing all pending virtioCoreR3VirtqAvailBufPut() transactions that have accumulated since
 * the last call to virtioCoreR3VirtqUsedRingSync()

 * @note This does a write-ahead to the used ring of the guest's queue. The data
 *       written won't be seen by the guest until the next call to virtioCoreVirtqSyncUsedRing()
 *
 *
 * @param   pDevIns         The device instance (for reading).
 * @param   pVirtio         Pointer to the shared virtio state.
 * @param   uVirtqNbr       Virtq number
 *
 * @param   pSgVirtReturn   Points to scatter-gather buffer of virtual memory
 *                          segments the caller is returning to the guest.
 *
 * @param   pVirtqBuf       This contains the context of the scatter-gather
 *                          buffer originally pulled from the queue.
 *
 * @param   fFence          If true, put up copy fence (memory barrier) after
 *                          copying to guest phys. mem.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS       Success
 * @retval  VERR_INVALID_STATE VirtIO not in ready state
 * @retval  VERR_NOT_AVAILABLE Virtq is empty
 *
 * @note    This function will not release any reference to pVirtqBuf.  The
 *          caller must take care of that.
 */
int virtioCoreR3VirtqUsedBufPut(PPDMDEVINS pDevIns, PVIRTIOCORE pVirtio, uint16_t uVirtqNbr, PRTSGBUF pSgVirtReturn,
                                 PVIRTQBUF pVirtqBuf, bool fFence);
/**
 * Advance index of avail ring to next entry in specified virtq (see virtioCoreR3VirtqAvailBufPeek())
 *
 * @param   pVirtio      Pointer to the virtio state.
 * @param   uVirtqNbr    Index of queue
 */
int virtioCoreR3VirtqAvailBufNext(PVIRTIOCORE pVirtio, uint16_t uVirtqNbr);

/**
 * Add some bytes to a virtq (s/g) buffer, converting them from virtual memory to GCPhys
 *
 * To be performant it is left to the caller to validate the size of the buffer with regard
 * to data being pulled from it to avoid underruns.
 *
 * @param   pVirtio     Pointer to the shared virtio state.
 * @param   pVirtqBuf   output: virtq buffer
 * @param   pv          input: virtual memory buffer to receive bytes
 * @param   cb          number of bytes to add to the s/g buffer.
 */
void virtioCoreR3VirqBufFill(PVIRTIOCORE pVirtio, PVIRTQBUF pVirtqBuf, void *pv, size_t cb);

/**
 * Extract some bytes out of a virtq (s/g) buffer, converting them from GCPhys to virtual memory
 *
 * To be performant it is left to the caller to validate the size of the buffer with regard
 * to data being pulled from it to avoid underruns.
 *
 * @param   pVirtio     Pointer to the shared virtio state.
 * @param   pVirtqBuf   input: virtq buffer
 * @param   pv          output: virtual memory buffer to receive bytes
 * @param   cb          number of bytes to Drain from buffer
 */
void virtioCoreR3VirtqBufDrain(PVIRTIOCORE pVirtio, PVIRTQBUF pVirtqBuf, void *pv, size_t cb);

/**
 * Updates indicated virtq's "used ring" descriptor index to match "shadow" index that tracks
 * pending buffers added to the used ring, thus exposing all the data added by virtioCoreR3VirtqUsedBufPut()
 * to the "used ring" since the last virtioCoreVirtqSyncUsedRing().
 *
 * This *must* be invoked after one or more virtioCoreR3VirtqUsedBufPut() calls to inform guest driver
 * there is data in the queue. If enabled by guest, IRQ or MSI-X signalling will notify guest
 * proactively, otherwise guest detect updates by polling. (see VirtIO 1.0, Section 2.4 "Virtqueues").
 *
 * @param   pDevIns     The device instance.
 * @param   pVirtio     Pointer to the shared virtio state.
 * @param   uVirtqNbr   Virtq number
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS       Success
 * @retval  VERR_INVALID_STATE VirtIO not in ready state
 */
int  virtioCoreVirtqSyncUsedRing(PPDMDEVINS pDevIns, PVIRTIOCORE pVirtio, uint16_t uVirtqNbr);

/**
 * Retains a reference to the given descriptor chain.
 *
 * @param   pVirtqBuf      The descriptor chain to reference.
 *
 * @returns New reference count.
 * @retval  UINT32_MAX on invalid parameter.
 */
uint32_t virtioCoreR3VirtqBufRetain(PVIRTQBUF pVirtqBuf);

/**
 * Releases a reference to the given descriptor chain.
 *
 * @param   pVirtio         Pointer to the shared virtio state.
 * @param   pVirtqBuf       The descriptor chain to reference.  NULL is quietly
 *                          ignored (returns 0).
 * @returns New reference count.
 * @retval  0 if freed or invalid parameter.
 */
uint32_t virtioCoreR3VirtqBufRelease(PVIRTIOCORE pVirtio, PVIRTQBUF pVirtqBuf);

/**
 * Return queue enable state
 *
 * @param   pVirtio      Pointer to the virtio state.
 * @param   uVirtqNbr    Virtq number.
 *
 * @returns true or false indicating queue is enabled or not.
 */
DECLINLINE(bool) virtioCoreIsVirtqEnabled(PVIRTIOCORE pVirtio, uint16_t uVirtqNbr)
{
    Assert(uVirtqNbr < RT_ELEMENTS(pVirtio->aVirtqueues));
    return pVirtio->aVirtqueues[uVirtqNbr].uEnable != 0;
}

/**
 * Get name of queue, via uVirtqNbr, assigned during virtioCoreR3VirtqAttach()
 *
 * @param   pVirtio     Pointer to the virtio state.
 * @param   uVirtqNbr   Virtq number.
 *
 * @returns Pointer to read-only queue name.
 */
DECLINLINE(const char *) virtioCoreVirtqGetName(PVIRTIOCORE pVirtio, uint16_t uVirtqNbr)
{
    Assert((size_t)uVirtqNbr < RT_ELEMENTS(pVirtio->aVirtqueues));
    return pVirtio->aVirtqueues[uVirtqNbr].szName;
}

/**
 * Get features VirtIO is running with now. This is used by device-specific code to identify
 * the the device's operational configuration after features have been negotiated with guest
 * VirtIO driver.  In the feature negotiation scheme, the host offers features it can support
 * and guest accepts among those offered which features it can or will enable. The accepted
 * features define the operational mode of VirtIO device. A mask reflecting the generic
 * Virtio (core) features and device-specific features are offered in the core-initialization
 * call by the device-specific code.
 *
 * @param   pVirtio     Pointer to the virtio state.
 *
 * @returns Features the guest driver has accepted, finalizing the operational features
 */
DECLINLINE(uint64_t) virtioCoreGetNegotiatedFeatures(PVIRTIOCORE pVirtio)
{
    return pVirtio->uDriverFeatures;
}

/**
 * Get the the name of the VM state change associated with the enumeration variable
 *
 * @param enmState       VM state (enumeration value)
 *
 * @returns associated text.
 */
const char *virtioCoreGetStateChangeText(VIRTIOVMSTATECHANGED enmState);

/**
 * Log memory-mapped I/O input or output value.
 *
 * This is to be invoked by macros that assume they are invoked in functions with
 * the relevant arguments. (See Virtio_1_0.cpp).
 *
 * It is exposed via the API so inheriting device-specific clients can provide similar
 * logging capabilities for a consistent look-and-feel.
 *
 * @param   pszFunc     To avoid displaying this function's name via __FUNCTION__ or LogFunc()
 * @param   pszMember   Name of struct member
 * @param   pv          pointer to value
 * @param   cb          size of value
 * @param   uOffset     offset into member where value starts
 * @param   fWrite      True if write I/O
 * @param   fHasIndex   True if the member is indexed
 * @param   idx         The index if fHasIndex
 */
void virtioCoreLogMappedIoValue(const char *pszFunc, const char *pszMember, uint32_t uMemberSize,
                                const void *pv, uint32_t cb, uint32_t uOffset,
                                int fWrite, int fHasIndex, uint32_t idx);

/**
 * Debug assist for any consumer device code that inherits VIRTIOCORE
 *
 * Does a formatted hex dump using Log(()), recommend using VIRTIO_HEX_DUMP() macro to
 * control enabling of logging efficiently.
 *
 * @param   pv          pointer to buffer to dump contents of
 * @param   cb          count of characters to dump from buffer
 * @param   uBase       base address of per-row address prefixing of hex output
 * @param   pszTitle    Optional title. If present displays title that lists
 *                      provided text with value of cb to indicate size next to it.
 */
void virtioCoreHexDump(uint8_t *pv, uint32_t cb, uint32_t uBase, const char *pszTitle);

/**
 * Debug assist for any consumer device code that inherits VIRTIOCORE
&
 * Do a hex dump of memory in guest physical context
 *
 * @param   GCPhys      pointer to buffer to dump contents of
 * @param   cb          count of characters to dump from buffer
 * @param   uBase       base address of per-row address prefixing of hex output
 * @param   pszTitle    Optional title. If present displays title that lists
 *                      provided text with value of cb to indicate size next to it.
 */
void virtioCoreGCPhysHexDump(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, uint16_t cb, uint32_t uBase, const char *pszTitle);

/**
 * The following API is functions identically to the similarly-named calls pertaining to the RTSGBUF
 */

/**
 * Calculate the length of a GCPhys s/g buffer by tallying the size of each segment.
 *
 * @param   pGcSgBuf        Guest Context (GCPhys) S/G buffer to calculate length of
 */
DECLINLINE(size_t) virtioCoreGCPhysChainCalcBufSize(PCVIRTIOSGBUF pGcSgBuf)
{
    size_t   cb = 0;
    unsigned i  = pGcSgBuf->cSegs;
    while (i-- > 0)
        cb += pGcSgBuf->paSegs[i].cbSeg;
    return cb;
}

void     virtioCoreGCPhysChainInit(PVIRTIOSGBUF pGcSgBuf, PVIRTIOSGSEG paSegs, size_t cSegs);
void     virtioCoreGCPhysChainReset(PVIRTIOSGBUF pGcSgBuf);
RTGCPHYS virtioCoreGCPhysChainGetNextSeg(PVIRTIOSGBUF pGcSgBuf, size_t *pcbSeg);
RTGCPHYS virtioCoreGCPhysChainAdvance(PVIRTIOSGBUF pGcSgBuf, size_t cbAdvance);
size_t   virtioCoreGCPhysChainCalcBufSize(PCVIRTIOSGBUF pGcSgBuf);


/** Misc VM and PDM boilerplate */
int      virtioCoreR3SaveExec(PVIRTIOCORE pVirtio, PCPDMDEVHLPR3 pHlp, PSSMHANDLE pSSM);
int      virtioCoreR3LoadExec(PVIRTIOCORE pVirtio, PCPDMDEVHLPR3 pHlp, PSSMHANDLE pSSM);
void     virtioCoreR3VmStateChanged(PVIRTIOCORE pVirtio, VIRTIOVMSTATECHANGED enmState);
void     virtioCoreR3Term(PPDMDEVINS pDevIns, PVIRTIOCORE pVirtio, PVIRTIOCORECC pVirtioCC);
int      virtioCoreR3Init(PPDMDEVINS pDevIns, PVIRTIOCORE pVirtio, PVIRTIOCORECC pVirtioCC, PVIRTIOPCIPARAMS pPciParams,
                          const char *pcszInstance, uint64_t fDevSpecificFeatures, void *pvDevSpecificCfg, uint16_t cbDevSpecificCfg);
int      virtioCoreRZInit(PPDMDEVINS pDevIns, PVIRTIOCORE pVirtio);
const char *virtioCoreGetStateChangeText(VIRTIOVMSTATECHANGED enmState);

/*
 * The following macros assist with handling/logging MMIO accesses to VirtIO dev-specific config area,
 * in a way that enhances code readability and debug logging consistency.
 *
 * cb, pv and fWrite are implicit parameters and must be defined by the invoker.
 */

#ifdef LOG_ENABLED

# define VIRTIO_DEV_CONFIG_LOG_ACCESS(member, tCfgStruct, uOffsetOfAccess) \
    if (LogIs7Enabled()) { \
        uint32_t uMbrOffset = uOffsetOfAccess - RT_UOFFSETOF(tCfgStruct, member); \
        uint32_t uMbrSize   = RT_SIZEOFMEMB(tCfgStruct, member); \
        virtioCoreLogMappedIoValue(__FUNCTION__, #member, uMbrSize, pv, cb, uMbrOffset, fWrite, false, 0); \
    }

# define VIRTIO_DEV_CONFIG_LOG_INDEXED_ACCESS(member, tCfgStruct, uOffsetOfAccess, uIdx) \
    if (LogIs7Enabled()) { \
        uint32_t uMbrOffset = uOffsetOfAccess - RT_UOFFSETOF(tCfgStruct, member); \
        uint32_t uMbrSize   = RT_SIZEOFMEMB(tCfgStruct, member); \
        virtioCoreLogMappedIoValue(__FUNCTION__, #member, uMbrSize,  pv, cb, uMbrOffset, fWrite, true, uIdx); \
    }
#else
# define VIRTIO_DEV_CONFIG_LOG_ACCESS(member, tCfgStruct, uMbrOffset) do { } while (0)
# define VIRTIO_DEV_CONFIG_LOG_INDEXED_ACCESS(member, tCfgStruct, uMbrOffset, uIdx) do { } while (0)
#endif

DECLINLINE(bool) virtioCoreMatchMember(uint32_t uOffset, uint32_t cb, uint32_t uMemberOff,
                                       size_t uMemberSize, bool fSubFieldMatch)
{
    /* Test for 8-byte field always (accessed as two 32-bit components) */
    if (uMemberSize == 8)
        return (cb == sizeof(uint32_t)) && (uOffset == uMemberOff || uOffset == (uMemberOff + sizeof(uint32_t)));

    if (fSubFieldMatch)
        return (uOffset >= uMemberOff) && (cb <= uMemberSize - (uOffset - uMemberOff));

    /* Test for exact match */
    return (uOffset == uMemberOff) && (cb == uMemberSize);
}

/**
 * Yields boolean true if uOffsetOfAccess falls within bytes of specified member of config struct
 */
#define VIRTIO_DEV_CONFIG_SUBMATCH_MEMBER(member, tCfgStruct, uOffsetOfAccess) \
            virtioCoreMatchMember(uOffsetOfAccess, cb, \
                                  RT_UOFFSETOF(tCfgStruct, member),  \
                                  RT_SIZEOFMEMB(tCfgStruct, member), true /* fSubfieldMatch */)

#define VIRTIO_DEV_CONFIG_MATCH_MEMBER(member, tCfgStruct, uOffsetOfAccess) \
            virtioCoreMatchMember(uOffsetOfAccess, cb, \
                                  RT_UOFFSETOF(tCfgStruct, member),  \
                                  RT_SIZEOFMEMB(tCfgStruct, member), false /* fSubfieldMatch */)

/**
 * Copy reads or copy writes specified member field of config struct (based on fWrite),
 * the memory described by cb and pv.
 *
 * cb, pv and fWrite are implicit parameters and must be defined by the invoker.
 */
#define VIRTIO_DEV_CONFIG_ACCESS(member, tCfgStruct, uOffsetOfAccess, pCfgStruct) \
    do \
    { \
        uint32_t uOffsetInMember = uOffsetOfAccess - RT_UOFFSETOF(tCfgStruct, member); \
        if (fWrite) \
            memcpy(((char *)&(pCfgStruct)->member) + uOffsetInMember, pv, cb); \
        else \
            memcpy(pv, ((const char *)&(pCfgStruct)->member) + uOffsetInMember, cb); \
        VIRTIO_DEV_CONFIG_LOG_ACCESS(member, tCfgStruct, uOffsetOfAccess); \
    } while(0)

/**
 * Copies bytes into memory described by cb, pv from the specified member field of the config struct.
 * The operation is a nop and logs error if implied parameter fWrite is true.
 *
 * cb, pv and fWrite are implicit parameters and must be defined by the invoker.
 */
#define VIRTIO_DEV_CONFIG_ACCESS_READONLY(member, tCfgStruct, uOffsetOfAccess, pCfgStruct) \
    do \
    { \
        uint32_t uOffsetInMember = uOffsetOfAccess - RT_UOFFSETOF(tCfgStruct, member); \
        if (fWrite) \
            LogFunc(("Guest attempted to write readonly virtio config struct (member %s)\n", #member)); \
        else \
        { \
            memcpy(pv, ((const char *)&(pCfgStruct)->member) + uOffsetInMember, cb); \
            VIRTIO_DEV_CONFIG_LOG_ACCESS(member, tCfgStruct, uOffsetOfAccess); \
        } \
    } while(0)

/**
 * Copies into or out of specified member field of config struct (based on fWrite),
 * the memory described by cb and pv.
 *
 * cb, pv and fWrite are implicit parameters and must be defined by the invoker.
 */
#define VIRTIO_DEV_CONFIG_ACCESS_INDEXED(member, uIdx, tCfgStruct, uOffsetOfAccess, pCfgStruct) \
    do \
    { \
        uint32_t uOffsetInMember = uOffsetOfAccess - RT_UOFFSETOF(tCfgStruct, member); \
        if (fWrite) \
            memcpy(((char *)&(pCfgStruct[uIdx].member)) + uOffsetInMember, pv, cb); \
        else \
            memcpy(pv, ((const char *)&(pCfgStruct[uIdx].member)) + uOffsetInMember, cb); \
        VIRTIO_DEV_CONFIG_LOG_INDEXED_ACCESS(member, tCfgStruct, uOffsetOfAccess, uIdx); \
    } while(0)

/**
 * Copies bytes into memory described by cb, pv from the specified member field of the config struct.
 * The operation is a nop and logs error if implied parameter fWrite is true.
 *
 * cb, pv and fWrite are implicit parameters and must be defined by the invoker.
 */
#define VIRTIO_DEV_CONFIG_ACCESS_INDEXED_READONLY(member, uidx, tCfgStruct, uOffsetOfAccess, pCfgStruct) \
    do \
    { \
        uint32_t uOffsetInMember = uOffsetOfAccess - RT_UOFFSETOF(tCfgStruct, member); \
        if (fWrite) \
            LogFunc(("Guest attempted to write readonly virtio config struct (member %s)\n", #member)); \
        else \
        { \
            memcpy(pv, ((const char *)&(pCfgStruct[uIdx].member)) + uOffsetInMember, cb); \
            VIRTIO_DEV_CONFIG_LOG_INDEXED_ACCESS(member, tCfgStruct, uOffsetOfAccess, uIdx); \
        } \
    } while(0)

/** @} */

/** @name API for VirtIO parent device
 * @{ */

#endif /* !VBOX_INCLUDED_SRC_VirtIO_VirtioCore_h */
