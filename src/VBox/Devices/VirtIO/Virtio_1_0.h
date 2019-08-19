/* $Id$ */
/** @file
 * Virtio_1_0.h - Virtio Declarations
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_VirtIO_Virtio_1_0_h
#define VBOX_INCLUDED_SRC_VirtIO_Virtio_1_0_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/ctype.h>

typedef void * VIRTIOHANDLE;                                     /**< Opaque handle to the VirtIO framework */

#define DISABLE_GUEST_DRIVER 0

/**
 * Important sizing and bounds params for this impl. of VirtIO 1.0 PCI device
 */
 /**
  * TEMPORARY NOTE: Some of these values are experimental during development and will likely change.
  */
#define VIRTIO_MAX_QUEUE_NAME_SIZE          32                   /**< Maximum length of a queue name           */
#define VIRTQ_MAX_SIZE                      1024                 /**< Max size (# desc elements) of a virtq    */
#define VIRTQ_MAX_CNT                       24                   /**< Max queues we allow guest to create      */
#define VIRTQ_DESC_MAX_SIZE                 (2 * 1024 * 1024)
#define VIRTIO_NOTIFY_OFFSET_MULTIPLIER     2                    /**< VirtIO Notify Cap. MMIO config param     */
#define VIRTIOSCSI_REGION_MEM_IO            0                    /**< BAR for MMIO (implementation specific)   */
#define VIRTIOSCSI_REGION_PORT_IO           1                    /**< BAR for PORT I/O (impl specific)         */
#define VIRTIOSCSI_REGION_PCI_CAP           2                    /**< BAR for VirtIO Cap. MMIO (impl specific) */
typedef struct VIRTQ_SEG                                         /**< Describes one segment of a buffer vector */
{
    RTGCPHYS addr;                                               /**< Physical addr. of this segment's buffer  */
    void    *pv;                                                 /**< Buf to hold value to write or read       */
    uint32_t cb;                                                 /**< Number of bytes to write or read         */
} VIRTQ_SEG_T;

    typedef struct VIRTQ_BUF_VECTOR                                  /**< Scatter/gather buffer vector             */
{
    uint32_t    uDescIdx;                                        /**< Desc at head of this vector list         */
    uint32_t    cSegsIn;                                         /**< Count of segments in aSegsIn[]           */
    uint32_t    cSegsOut;                                        /**< Count of segments in aSegsOut[]          */
    VIRTQ_SEG_T aSegsIn[VIRTQ_MAX_SIZE];                         /**< List of segments to write to guest       */
    VIRTQ_SEG_T aSegsOut[VIRTQ_MAX_SIZE];                        /**< List of segments read from guest         */
} VIRTQ_BUF_VECTOR_T, *PVIRTQ_BUF_VECTOR_T;

/**
 * The following structure is used to pass the PCI parameters from the consumer
 * to this generic VirtIO framework. This framework provides the Vendor ID as Virtio.
 */
typedef struct VIRTIOPCIPARAMS
{
    uint16_t  uDeviceId;                                         /**< PCI Cfg Device ID                        */
    uint16_t  uClassBase;                                        /**< PCI Cfg Base Class                       */
    uint16_t  uClassSub;                                         /**< PCI Cfg Subclass                         */
    uint16_t  uClassProg;                                        /**< PCI Cfg Programming Interface Class      */
    uint16_t  uSubsystemId;                                      /**< PCI Cfg Card Manufacturer Vendor ID      */
    uint16_t  uSubsystemVendorId;                                /**< PCI Cfg Chipset Manufacturer Vendor ID   */
    uint16_t  uRevisionId;                                       /**< PCI Cfg Revision ID                      */
    uint16_t  uInterruptLine;                                    /**< PCI Cfg Interrupt line                   */
    uint16_t  uInterruptPin;                                     /**< PCI Cfg InterruptPin                     */
} VIRTIOPCIPARAMS, *PVIRTIOPCIPARAMS;

/**
 * Implementation-specific client callback to notify client of significant device status
 * changes.
 *
 * @param hVirtio       Handle to VirtIO framework
 * @param fDriverOk     True if guest driver is okay (thus queues, etc... are valid)
 * @param pClient       Pointer to opaque client data (state)
 */
typedef DECLCALLBACK(void)   FNVIRTIOSTATUSCHANGED(VIRTIOHANDLE hVirtio, void *pClient, bool fDriverOk);
typedef FNVIRTIOSTATUSCHANGED *PFNVIRTIOSTATUSCHANGED;

/**
 * When guest-to-host queue notifications are enabled, the guest driver notifies the host
 * that the avail queue has buffers, and this callback informs the client.
 *
 * @param   hVirtio     Handle to the VirtIO framework
 * @param   qIdx        Index of the notified queue
 * @param   pClient     Pointer to opaque client data (state)
 */
typedef DECLCALLBACK(void)   FNVIRTIOQUEUENOTIFIED(VIRTIOHANDLE hVirtio, void *pClient, uint16_t qIdx);
typedef FNVIRTIOQUEUENOTIFIED *PFNVIRTIOQUEUENOTIFIED;

 /**
 * Implementation-specific client callback to access VirtIO Device-specific capabilities
 * (other VirtIO capabilities and features are handled in VirtIO implementation)
 *
 * @param   pDevIns     The device instance.
 * @param   offset      Offset within device specific capabilities struct
 * @param   pvBuf       Buffer in which to save read data
 * @param   cbWrite     Number of bytes to write
 */
typedef DECLCALLBACK(int)   FNVIRTIODEVCAPWRITE(PPDMDEVINS pDevIns, uint32_t uOffset, const void *pvBuf, size_t cbWrite);
typedef FNVIRTIODEVCAPWRITE *PFNVIRTIODEVCAPWRITE;

/**
 * Implementation-specific client ballback to access VirtIO Device-specific capabilities
 * (other VirtIO capabilities and features are handled in VirtIO implementation)
 *
 * @param   pDevIns     The device instance.
 * @param   offset      Offset within device specific capabilities struct
 * @param   pvBuf       Buffer in which to save read data
 * @param   cbRead      Number of bytes to read
 */
typedef DECLCALLBACK(int)   FNVIRTIODEVCAPREAD(PPDMDEVINS pDevIns, uint32_t uOffset, const void *pvBuf, size_t cbRead);
typedef FNVIRTIODEVCAPREAD *PFNVIRTIODEVCAPREAD;


/** @name VirtIO port I/O callbacks.
 * @{ */
typedef struct VIRTIOCALLBACKS
{
     DECLCALLBACKMEMBER(void, pfnVirtioStatusChanged)
                                  (VIRTIOHANDLE hVirtio, void *pClient, bool fDriverOk);
     DECLCALLBACKMEMBER(void, pfnVirtioQueueNotified)
                                  (VIRTIOHANDLE hVirtio, void *pClient, uint16_t qIdx);
     DECLCALLBACKMEMBER(int,  pfnVirtioDevCapRead)
                                  (PPDMDEVINS pDevIns, uint32_t uOffset, const void *pvBuf, size_t cbRead);
     DECLCALLBACKMEMBER(int,  pfnVirtioDevCapWrite)
                                  (PPDMDEVINS pDevIns, uint32_t uOffset, const void *pvBuf, size_t cbWrite);
     DECLCALLBACKMEMBER(int,  pfnSSMDevLiveExec)
                                  (PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass);
     DECLCALLBACKMEMBER(int,  pfnSSMDevSaveExec)
                                  (PPDMDEVINS pDevIns, PSSMHANDLE pSSM);
     DECLCALLBACKMEMBER(int,  pfnSSMDevLoadExec)
                                  (PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass);
     DECLCALLBACKMEMBER(int,  pfnSSMDevLoadDone)
                                  (PPDMDEVINS pDevIns, PSSMHANDLE pSSM);

} VIRTIOCALLBACKS, *PVIRTIOCALLBACKS;
/** @} */

/** API for VirtIO client */

/**
 * Allocate client context for client to work with VirtIO-provided with queue
 * As a side effect creates a buffer vector a client can get a pointer to
 * with a call to virtioQueueBufVec()
 *
 * @param  hVirtio   - Handle to VirtIO framework
 * @param  qIdx      - Queue number
 * @param  pcszName  - Name to give queue
 *
 * @returns status     VINF_SUCCESS         - Success
 *                     VERR_INVALID_STATE   - VirtIO not in ready state
 *                     VERR_NO_MEMORY       - Out of memory
 *
 * @returns status. If false, the call failed and the client should call virtioResetAll()
 */
int virtioQueueAttach(VIRTIOHANDLE hVirtio, uint16_t qIdx, const char *pcszName);
/**
 * Detaches from queue and release resources
 *
 * @param hVirtio   - Handle for VirtIO framework
 * @param qIdx      - Queue number
 *
 */
int virtioQueueDetach(VIRTIOHANDLE hVirtio, uint16_t qIdx);

/**
 * Return pointer to buffer vector object associated with queue
 *
 * @param hVirtio   - Handle for VirtIO framework
 * @param qIdx      - Queue number
 *
 * @returns           Pointer pBufVec if success, else NULL
 */
PVIRTQ_BUF_VECTOR_T virtioQueueGetBuffer(VIRTIOHANDLE hVirtio, uint16_t qIdx);

/**
 * Get name of queue, by qIdx, assigned at virtioQueueAttach()
 *
 * @param hVirtio   - Handle for VirtIO framework
 * @param qIdx      - Queue number
 *
 * @returns          Success: Returns pointer to queue name
 *                   Failure: Returns "<null>" (never returns NULL pointer).
 */
const char *virtioQueueGetName(VIRTIOHANDLE hVirtio, uint16_t qIdx);

/**
 * Removes descriptor [chain] from queue and converts it to scatter/gather data
 * in the vector whose pointer was returned from VirtioQueueAttach.
 *
 * @param hVirtio   - Handle for VirtIO framework
 * @param qIdx      - Queue number
 *
 * @returns status    VINF_SUCCESS         - Success
 *                    VERR_INVALID_STATE   - VirtIO not in ready state
 */
int virtioQueueGet(VIRTIOHANDLE hVirtio, uint16_t qIdx, bool fRemove);

/**
 * Same as virtioQueueGet() but leaves the item on the avail ring of the queue.
 *
 * @param hVirtio   - Handle for VirtIO framework
 * @param qIdx      - Queue number
 *
 * @returns           VINF_SUCCESS         - Success
 *                    VERR_INVALID_STATE   - VirtIO not in ready state
 *                    VERR_NOT_AVAILABLE   - Queue is empty
 */
int virtioQueuePeek(VIRTIOHANDLE hVirtio, uint16_t qIdx);

/**
 * Writes scatter/gather segment contained in queue's bufVec.aSegsIn[] array to
 * physical memory assigned by the guest driver. The data won't be seen by the
 * driver until the next virtioQueueSync() call.
 *
 * @param hVirtio   - Handle for VirtIO framework
 * @param qIdx      - Queue number
 *
 * @returns           VINF_SUCCESS         - Success
 *                    VERR_INVALID_STATE   - VirtIO not in ready state
 *                    VERR_NOT_AVAILABLE   - Queue is empty
 */
int virtioQueuePut(VIRTIOHANDLE hVirtio, uint16_t qIdx, uint32_t cb);

/**
 * Updates virtq's "used ring" descriptor index to match the current bufVec's
 * index, thus exposing the data added to the used ring by all virtioQueuePut()
 * calls since the last sync. This should be called after one or more virtQueuePut()
 * calls to inform the guest driver there is data in the queue. Explicit
 * notifications will be sent to the guest depending on VirtIO features negotiated
 * and conditions.
 *
 * @param hVirtio   - Handle for VirtIO framework
 * @param qIdx      - Queue number
 *
 * @returns           VINF_SUCCESS         - Success
 *                    VERR_INVALID_STATE   - VirtIO not in ready state
 */
int virtioQueueSync(VIRTIOHANDLE hVirtio, uint16_t qIdx);

/**
 * Check if the associated queue is empty
 *
 * @param hVirtio   - Handle for VirtIO framework
 * @param qIdx      - Queue number
 *
 * @returns           true     - Queue is empty or unavailable.
 *                    false    - Queue is available and has entries
 */
bool virtioQueueIsEmpty (VIRTIOHANDLE hVirtio, uint16_t qIdx);

/**
 * Request orderly teardown of VirtIO on host and guest
 */
void virtioResetAll(VIRTIOHANDLE hVirtio);

/** CLIENT MUST CALL ON RELOCATE CALLBACK! */
void virtioRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta);

/**
 * Constructor sets up the PCI device controller and VirtIO state
 *
 * @param   pDevIns                  Device instance data
 * @param   pClientContext           Opaque client context (such as state struct)
 * @param   pVirtio                  Device State
 * @param   pPciParams               Values to populate industry standard PCI Configuration Space data structure
 * @param   pcszInstance             Device instance name
 * @param   uDevSpecificFeatures     VirtIO device-specific features offered by client
 * @param   devCapReadCallback       Client handler to call upon guest read to device specific capabilities.
 * @param   devCapWriteCallback      Client handler to call upon guest write to device specific capabilities.
 * @param   devStatusChangedCallback Client handler to call for major device status changes
 * @param   queueNotifiedCallback    Client handler for guest-to-host notifications that avail queue has ring data
 * @param   ssmLiveExecCallback      Client handler for SSM live exec
 * @param   ssmSaveExecCallback      Client handler for SSM save exec
 * @param   ssmLoadExecCallback      Client handler for SSM load exec
 * @param   ssmLoadDoneCallback      Client handler for SSM load done
 * @param   cbDevSpecificCfg         Size of virtio_pci_device_cap device-specific configuration struct
 * @param   pDevSpecificCfg          Address of client's VirtIO dev-specific configuration struct
 */
int  virtioConstruct(PPDMDEVINS             pDevIns,
                     void                  *pClientContext,
                     VIRTIOHANDLE          *phVirtio,
                     PVIRTIOPCIPARAMS       pPciParams,
                     const char            *pcszInstance,
                     uint64_t               uDevSpecificFeatures,
                     PFNVIRTIODEVCAPREAD    devCapReadCallback,
                     PFNVIRTIODEVCAPWRITE   devCapWriteCallback,
                     PFNVIRTIOSTATUSCHANGED devStatusChangedCallback,
                     PFNVIRTIOQUEUENOTIFIED queueNotifiedCallback,
                     PFNSSMDEVLIVEEXEC      ssmLiveExecCallback,
                     PFNSSMDEVSAVEEXEC      ssmSaveExecCallback,
                     PFNSSMDEVLOADEXEC      ssmLoadExecCallback,
                     PFNSSMDEVLOADDONE      ssmLoadDoneCallback,
                     uint16_t               cbDevSpecificCfg,
                     void                  *pDevSpecificCfg);

/**
 * Log memory-mapped I/O input or output value.
 * This is designed to be invoked by macros that can make contextual assumptions
 * (e.g. implicitly derive some of the parameters). It is exposed for the VirtIO
 * client doing the device-specific implementation in order to log in a similar fashion
 * accesses to the device-specific MMIO configuration structure. Macros that leverage
 * this function are in Virtio_1_0_impl.h and can be used as an example of how to use
 * this effectively for the device-specific code.
 *
 * @param   pszFunc     - To avoid displaying this function's name via __FUNCTION__ or LogFunc()
 * @param   pszMember   - Name of struct member
 * @param   pv          - pointer to value
 * @param   cb          - size of value
 * @param   uOffset     - offset into member where value starts
 * @param   fWrite      - True if write I/O
 * @param   fHasIndex   - True if the member is indexed
 * @param   idx         - The index if fHasIndex
 */
void virtioLogMappedIoValue(const char *pszFunc, const char *pszMember, size_t uMemberSize,
                            const void *pv, uint32_t cb, uint32_t uOffset,
                            bool fWrite, bool fHasIndex, uint32_t idx);

#endif /* !VBOX_INCLUDED_SRC_VirtIO_Virtio_1_0_h */
