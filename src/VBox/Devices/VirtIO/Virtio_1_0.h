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
#include <iprt/sg.h>

typedef void * VIRTIOHANDLE;                                     /**< Opaque handle to the VirtIO framework */


/**
 * Important sizing and bounds params for this impl. of VirtIO 1.0 PCI device
 */
 /**
  * TEMPORARY NOTE: Some of these values are experimental during development and will likely change.
  */
#define VIRTIO_MAX_QUEUE_NAME_SIZE          32                   /**< Maximum length of a queue name           */
#define VIRTQ_MAX_SIZE                      1024                 /**< Max size (# desc elements) of a virtq    */
#define VIRTQ_MAX_CNT                       24                   /**< Max queues we allow guest to create      */
#define VIRTIO_NOTIFY_OFFSET_MULTIPLIER     2                    /**< VirtIO Notify Cap. MMIO config param     */
#define VIRTIO_REGION_PCI_CAP               2                    /**< BAR for VirtIO Cap. MMIO (impl specific) */
#define VIRTIO_REGION_MSIX_CAP              0                    /**< Bar for MSI-X handling                   */

#define VIRTIO_HEX_DUMP(logLevel, pv, cb, base, title) \
    do { \
        if (LogIsItEnabled(logLevel, LOG_GROUP)) \
            virtioHexDump((pv), (cb), (base), (title)); \
    } while (0)


/**
 * The following structure holds the pre-processed context of descriptor chain pulled from a virtio queue
 * to conduct a transaction between the client of this virtio implementation and the guest VM's virtio driver.
 * It contains the head index of the descriptor chain, the output data from the client that has been
 * converted to a contiguous virtual memory and a physical memory scatter-gather buffer for use by by
 * the virtio framework to complete the transaction in the final phase of round-trip processing.
 *
 * The client should not modify the contents of this buffer. The primary field of interest to the
 * client is pVirtSrc, which contains the VirtIO "OUT" (to device) buffer from the guest.
 *
 * Typical use is, When the client (worker thread) detects available data on the queue, it pulls the
 * next one of these descriptor chain structs off the queue using virtioQueueGet(), processes the
 * virtual memory buffer pVirtSrc, produces result data to pass back to the guest driver and calls
 * virtioQueuePut() to return the result data to the client.
 */
typedef struct VIRTIO_DESC_CHAIN
{
    uint32_t  uHeadIdx;                                          /**< Head idx of associated desc chain        */
    uint32_t  cbPhysSend;                                        /**< Total size of src buffer                 */
    PRTSGBUF  pSgPhysSend;                                       /**< Phys S/G/ buf for data from guest        */
    uint32_t  cbPhysReturn;                                      /**< Total size of dst buffer                 */
    PRTSGBUF  pSgPhysReturn;                                     /**< Phys S/G buf to store result for guest   */
} VIRTIO_DESC_CHAIN_T, *PVIRTIO_DESC_CHAIN_T, **PPVIRTIO_DESC_CHAIN_T;

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
    uint16_t  uInterruptPin;                                     /**< PCI Cfg Interrupt pin                    */
} VIRTIOPCIPARAMS, *PVIRTIOPCIPARAMS;

/**
 * Implementation-specific client callback to notify client of significant device status
 * changes.
 *
 * @param hVirtio       Handle to VirtIO framework
 * @param fDriverOk     True if guest driver is okay (thus queues, etc... are valid)
 * @param pClient       Pointer to opaque client data (state)
 */
typedef DECLCALLBACK(void)   FNVIRTIOSTATUSCHANGED(VIRTIOHANDLE hVirtio, void *pClient, uint32_t fDriverOk);
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
typedef DECLCALLBACK(int)   FNVIRTIODEVCAPWRITE(PPDMDEVINS pDevIns, uint32_t uOffset, const void *pvBuf, uint32_t cbWrite);
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
typedef DECLCALLBACK(int)   FNVIRTIODEVCAPREAD(PPDMDEVINS pDevIns, uint32_t uOffset, void *pvBuf, uint32_t cbRead);
typedef FNVIRTIODEVCAPREAD *PFNVIRTIODEVCAPREAD;


/** @name VirtIO port I/O callbacks.
 * @{ */
typedef struct VIRTIOCALLBACKS
{
     DECLCALLBACKMEMBER(void, pfnVirtioStatusChanged)(VIRTIOHANDLE hVirtio, void *pClient, uint32_t fDriverOk);
     DECLCALLBACKMEMBER(void, pfnVirtioQueueNotified)(VIRTIOHANDLE hVirtio, void *pClient, uint16_t qIdx);
     DECLCALLBACKMEMBER(int,  pfnVirtioDevCapRead)(PPDMDEVINS pDevIns, uint32_t uOffset, void *pvBuf, uint32_t cbRead);
     DECLCALLBACKMEMBER(int,  pfnVirtioDevCapWrite)(PPDMDEVINS pDevIns, uint32_t uOffset, const void *pvBuf, uint32_t cbWrite);
     DECLCALLBACKMEMBER(int,  pfnSSMDevLiveExec)(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass);
     DECLCALLBACKMEMBER(int,  pfnSSMDevSaveExec)(PPDMDEVINS pDevIns, PSSMHANDLE pSSM);
     DECLCALLBACKMEMBER(int,  pfnSSMDevLoadExec)(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass);
     DECLCALLBACKMEMBER(int,  pfnSSMDevLoadDone)(PPDMDEVINS pDevIns, PSSMHANDLE pSSM);
} VIRTIOCALLBACKS, *PVIRTIOCALLBACKS;
/** @} */

/** API for VirtIO client */

/**
 * Allocate client context for client to work with VirtIO-provided with queue
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
 * Removes descriptor chain from avail ring of indicated queue and converts the descriptor
 * chain into its OUT (to device) and IN to guest components. Additionally it converts
 * the OUT desc chain data to a contiguous virtual memory buffer for easy consumption
 * by the caller. The caller must return the descriptor chain pointer via virtioQueuePut()
 * and then call virtioQueueSync() at some point to return the data to the guest and
 * complete the transaction.
 *
 * @param hVirtio      - Handle for VirtIO framework
 * @param qIdx         - Queue number
 * @param fRemove      - flags whether to remove desc chain from queue (false = peek)
 * @param ppDescChain  - Address to store pointer to descriptor chain that contains the
 *                       pre-processed transaction information pulled from the virtq.
 *
 * @returns status     VINF_SUCCESS         - Success
 *                     VERR_INVALID_STATE   - VirtIO not in ready state
 */
int virtioQueueGet(VIRTIOHANDLE hVirtio, uint16_t qIdx, PPVIRTIO_DESC_CHAIN_T ppDescChain, bool fRemove);


/**
 * Returns data to the guest to complete a transaction initiated by virtQueueGet().
 * The caller passes in a pointer to a scatter-gather buffer of virtual memory segments
 * and a pointer to the descriptor chain context originally derived from the pulled
 * queue entry, and this function will write the virtual memory s/g buffer into the
 * guest's physical memory free the descriptor chain. The caller handles the freeing
 * (as needed) of the virtual memory buffer.
 *
 * NOTE: This does a write-ahead to the used ring of the guest's queue.
 *       The data written won't be seen by the guest until the next call to virtioQueueSync()
 *
 *
 * @param hVirtio       - Handle for VirtIO framework
 * @param qIdx          - Queue number
 *
 * @param pSgVirtReturn - Points toscatter-gather buffer of virtual memory segments
                          the caller is returning to the guest.
 *
 * @param pDescChain    - This contains the context of the scatter-gather buffer
 *                        originally pulled from the queue.
 *
 * @parame fFence       - If true, put up copy fence (memory barrier) after
 *                        copying to guest phys. mem.
 *
 * @returns              VINF_SUCCESS         - Success
 *                       VERR_INVALID_STATE   - VirtIO not in ready state
 *                       VERR_NOT_AVAILABLE   - Queue is empty
 */

 int virtioQueuePut(VIRTIOHANDLE hVirtio, uint16_t qIdx, PRTSGBUF pSgVirtReturn,
                    PVIRTIO_DESC_CHAIN_T pDescChain, bool fFence);


/**
 * Updates the indicated virtq's "used ring" descriptor index to match the
 * current write-head index, thus exposing the data added to the used ring by all
 * virtioQueuePut() calls since the last sync. This should be called after one or
 * more virtQueuePut() calls to inform the guest driver there is data in the queue.
 * Explicit notifications (e.g. interrupt or MSI-X) will be sent to the guest,
 * depending on VirtIO features negotiated and conditions, otherwise the guest
 * will detect the update by polling. (see VirtIO 1.0
 * specification, Section 2.4 "Virtqueues").
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
 * Return queue enable state
  *
 * @param hVirtio   - Handle for VirtIO framework
 * @param qIdx      - Queue number
 * @param fEnabled  - Flag indicating whether to enable queue or not
 */
bool virtioIsQueueEnabled(VIRTIOHANDLE hVirtio, uint16_t qIdx);

/**
 * Enable or disable queue
 *
 * @param hVirtio   - Handle for VirtIO framework
 * @param qIdx      - Queue number
 * @param fEnabled  - Flag indicating whether to enable queue or not
 */
void virtioQueueEnable(VIRTIOHANDLE hVirtio, uint16_t qIdx, bool fEnabled);

/**
 * Request orderly teardown of VirtIO on host and guest
 * @param hVirtio   - Handle for VirtIO framework
 *
 */
void virtioResetAll(VIRTIOHANDLE hVirtio);

/**
 * This sends notification ('kicks') guest driver to check queues for any new
 * elements in the used queue to process.  It should be called after resuming
 * in case anything was added to the queues during suspend/quiescing and a
 * notification was missed, to prevent the guest from stalling after suspend.
 */
void virtioPropagateResumeNotification(VIRTIOHANDLE hVirtio);

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
 * Get the features VirtIO is running withnow.
 *
 * @returns Features the guest driver has accepted, finalizing the operational features
 *
 */
uint64_t virtioGetNegotiatedFeatures(VIRTIOHANDLE hVirtio);

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
 *
 * This is designed to be invoked by macros that can make contextual assumptions
 * (e.g. implicitly derive MACRO parameters from the invoking function). It is exposed
 * for the VirtIO client doing the device-specific implementation in order to log in a
 * similar fashion accesses to the device-specific MMIO configuration structure. Macros
 * that leverage this function are in Virtio_1_0_impl.h and can be used as an example
 * of how to use this effectively for the device-specific code.
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
void virtioLogMappedIoValue(const char *pszFunc, const char *pszMember, uint32_t uMemberSize,
                            const void *pv, uint32_t cb, uint32_t uOffset,
                            int fWrite, int fHasIndex, uint32_t idx);
/**
 * Does a formatted hex dump using Log(()), recommend using VIRTIO_HEX_DUMP() macro to
 * control enabling of logging efficiently.
 *
 * @param   pv          - pointer to buffer to dump contents of
 * @param   cb          - count of characters to dump from buffer
 * @param   uBase       - base address of per-row address prefixing of hex output
 * @param   pszTitle    - Optional title. If present displays title that lists
 *                        provided text with value of cb to indicate size next to it.
 */
void virtioHexDump(uint8_t *pv, uint32_t cb, uint32_t uBase, const char *pszTitle);

#endif /* !VBOX_INCLUDED_SRC_VirtIO_Virtio_1_0_h */
