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

#define VIRTQ_MAX_SIZE                      1024
#define VIRTIO_MAX_QUEUES                   256     /** Max queues we allow guest to create      */
#define VIRTQ_DESC_MAX_SIZE                 (2 * 1024 * 1024)

typedef void * VIRTIOHANDLE;                        /** opaque handle to the VirtIO framework */
typedef VIRTIOHANDLE *PVIRTIOHANDLE;

/** Reserved Feature Bits */
#define VIRTIO_F_RING_INDIRECT_DESC         RT_BIT_64(28)
#define VIRTIO_F_RING_IDX                   RT_BIT_64(29)
#define VIRTIO_F_VERSION_1                  RT_BIT_64(32)

#define VIRTIO_MSI_NO_VECTOR                0xffff  /* Vector value used to disable MSI for queue */

/** Device Status field constants (from Virtio 1.0 spec) */
#define VIRTIO_STATUS_ACKNOWLEDGE           0x01    /** Guest found this emulated PCI device     */
#define VIRTIO_STATUS_DRIVER                0x02    /** Guest can drive this PCI device          */
#define VIRTIO_STATUS_DRIVER_OK             0x04    /** Guest VirtIO driver setup and ready      */
#define VIRTIO_STATUS_FEATURES_OK           0x08    /** Guest says feature negotiation complete  */
#define VIRTIO_STATUS_FAILED                0x80    /** Fatal: Something wrong, guest gave up    */
#define VIRTIO_STATUS_DEVICE_NEEDS_RESET    0x40    /** Device experienced unrecoverable error   */

/** virtq related flags */
#define VIRTQ_DESC_F_NEXT                   1       /** Indicates this descriptor chains to next */
#define VIRTQ_DESC_F_WRITE                  2       /** Marks buffer as write-only (default ro)  */
#define VIRTQ_DESC_F_INDIRECT               4       /** Buffer is list of buffer descriptors     */
#define VIRTIO_F_INDIRECT_DESC              28      /** Using indirect descriptors               */
#define VIRTIO_F_EVENT_IDX                  29      /** avail_event, used_event in play          */

#define VIRTQ_USED_F_NO_NOTIFY              1       /** Optimization: Device advises driver (unreliably):
                                                        Don't kick me when adding buffer */

#define VIRTQ_AVAIL_F_NO_INTERRUPT          1       /** Optimization: Driver advises device (unreliably):
                                                        Don't interrupt when buffer consumed */
/**
 * virtq related structs
 *  (struct names follow VirtIO 1.0 spec, typedef use VBox style)
 */
typedef struct virtq_desc
{
    uint64_t  pGcPhysBuf;                   /** addr        GC physical address of buffer   */
    uint32_t  cbBuf;                        /** len         Buffer length                   */
    uint16_t  fFlags;                       /** flags       Buffer specific flags           */
    uint16_t  uNext;                        /** next        Chain idx if VIRTIO_DESC_F_NEXT */
} VIRTQ_DESC_T, *PVIRTQ_DESC_T;

typedef struct virtq_avail  /* VirtIO 1.0 specification formal name of this struct    */
{
    uint16_t  fFlags;                       /** flags       avail ring guest-to-host flags */
    uint16_t  uIdx;                         /** idx         Index of next free ring slot    */
    uint16_t  auRing[1];                    /** ring        Ring: avail guest-to-host bufs  */
    uint16_t  uUsedEventIdx;                /** used_event  (if VIRTQ_USED_F_NO_NOTIFY)     */
} VIRTQ_AVAIL_T, *PVIRTQ_AVAIL_T;

typedef struct virtq_used_elem /* VirtIO 1.0 specification formal name of this struct */
{
    uint32_t  uIdx;                         /** idx         Start of used descriptor chain  */
    uint32_t  cbElem;                       /** len         Total len of used desc chain    */
} VIRTQ_USED_ELEM_T;

typedef struct virt_used  /* VirtIO 1.0 specification formal name of this struct */
{
    uint16_t  fFlags;                       /** flags       used ring host-to-guest flags  */
    uint16_t  uIdx;                         /** idx         Index of next ring slot        */
    VIRTQ_USED_ELEM_T aRing[1];             /** ring        Ring: used host-to-guest bufs  */
    uint16_t  uAvailEventIdx;               /** avail_event if (VIRTQ_USED_F_NO_NOTIFY)    */
} VIRTQ_USED_T, *PVIRTQ_USED_T;


typedef struct virtq
{
    uint16_t cbQueue;                       /** virtq size                                 */
    uint16_t padding[3];                    /** 64-bit-align phys ptrs to start of struct  */
    RTGCPHYS pGcPhysVirtqDescriptors;       /** (struct virtq_desc  *)                     */
    RTGCPHYS pGcPhysVirtqAvail;             /** (struct virtq_avail *)                     */
    RTGCPHYS pGcPhysVirtqUsed;              /** (struct virtq_used  *)                     */
} VIRTQ, *PVIRTQ;

typedef struct VQueue
{
    VIRTQ VirtQ;
    uint16_t  uNextAvailIndex;
    uint16_t  uNextUsedIndex;
    uint32_t  uPageNumber;
    R3PTRTYPE(const char *) pcszName;
} VQUEUE;


typedef VQUEUE *PVQUEUE;

typedef struct VQueueElemSeg
{
    RTGCPHYS addr;
    void    *pv;
    uint32_t cb;
} VQUEUESEG;

typedef struct VQueueElem
{
    uint32_t  idx;
    uint32_t  cSegsIn;
    uint32_t  cSegsOut;
    VQUEUESEG aSegsIn[VIRTQ_MAX_SIZE];
    VQUEUESEG aSegsOut[VIRTQ_MAX_SIZE];
} VQUEUEELEM;
typedef VQUEUEELEM *PVQUEUEELEM;

/**
 * The following structure is used to pass the PCI parameters from the consumer
 * to this generic VirtIO framework
 */
typedef struct VIRTIOPCIPARAMS
{
    uint16_t  uDeviceId;
    uint16_t  uClassBase;
    uint16_t  uClassSub;
    uint16_t  uClassProg;
    uint16_t  uSubsystemId;
    uint16_t  uSubsystemVendorId;
    uint16_t  uRevisionId;
    uint16_t  uInterruptLine;
    uint16_t  uInterruptPin;
} VIRTIOPCIPARAMS, *PVIRTIOPCIPARAMS;

 /**
 * VirtIO Device-specific capabilities read callback
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
 * VirtIO Device-specific capabilities read callback
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
     DECLCALLBACKMEMBER(int,      pfnVirtioDevCapRead)
                                      (PPDMDEVINS pDevIns, uint32_t uOffset, const void *pvBuf, size_t cbRead);
     DECLCALLBACKMEMBER(int,      pfnVirtioDevCapWrite)
                                      (PPDMDEVINS pDevIns, uint32_t uOffset, const void *pvBuf, size_t cbWrite);
} VIRTIOCALLBACKS, *PVIRTIOCALLBACKS;
/** @} */

PVQUEUE virtioAddQueue(         VIRTIOHANDLE hVirtio, uint32_t uSize, const char *pcszName);
int     virtioDestruct(         VIRTIOHANDLE hVirtio);
int     virtioReset(            VIRTIOHANDLE hVirtio);
void    virtioSetReadLed(       VIRTIOHANDLE hVirtio, bool fOn);
void    virtioSetWriteLed(      VIRTIOHANDLE hVirtio, bool fOn);
int     virtioRaiseInterrupt(   VIRTIOHANDLE hVirtio, int rcBusy, uint8_t uint8_tIntCause);
//void    virtioNotifyDriver(     VIRTIOHANDLE hVirtio);
void    virtioSetHostFeatures(  VIRTIOHANDLE hVirtio, uint64_t uDeviceFeatures);

bool    virtQueueSkip(          VIRTIOHANDLE hVirtio, PVQUEUE pQueue);
bool    virtQueueGet(           VIRTIOHANDLE hVirtio, PVQUEUE pQueue, PVQUEUEELEM pElem, bool fRemove = true);
void    virtQueuePut(           VIRTIOHANDLE hVirtio, PVQUEUE pQueue, PVQUEUEELEM pElem, uint32_t len, uint32_t uReserved = 0);
void    virtQueueNotify(        VIRTIOHANDLE hVirtio, PVQUEUE pQueue);
void    virtQueueSync(          VIRTIOHANDLE hVirtio, PVQUEUE pQueue);
void    vringSetNotification(   VIRTIOHANDLE hVirtio, PVIRTQ pVirtQ, bool fEnabled);

/**
 * Formats the logging of a memory-mapped I/O input or output value
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
void virtioLogMappedIoValue(const char *pszFunc, const char *pszMember,
                            const void *pv, uint32_t cb, uint32_t uOffset,
                            bool fWrite, bool fHasIndex, uint32_t idx);

int     virtioConstruct(
            PPDMDEVINS           pDevIns,
            PVIRTIOHANDLE        phVirtio,
            int                  iInstance,
            PVIRTIOPCIPARAMS     pPciParams,
            const char           *pcszNameFmt,
            uint32_t             nQueues,
            uint32_t             uVirtioCapBar,
            uint64_t             uDeviceFeatures,
            PFNVIRTIODEVCAPREAD  devCapReadCallback,
            PFNVIRTIODEVCAPWRITE devCapWriteCallback,
            uint16_t             cbDevSpecificCap,
            void                 *pDevSpecificCap,
            uint32_t             uNotifyOffMultiplier);

//PCIDevGetCapabilityList
//PCIDevSetCapabilityList
//DECLINLINE(void) PDMPciDevSetCapabilityList(PPDMPCIDEV pPciDev, uint8_t u8Offset)
//    PDMPciDevSetByte(pPciDev, VBOX_PCI_CAPABILITY_LIST, u8Offset);
//DECLINLINE(uint8_t) PDMPciDevGetCapabilityList(PPDMPCIDEV pPciDev)
//    return PDMPciDevGetByte(pPciDev, VBOX_PCI_CAPABILITY_LIST);

#endif /* !VBOX_INCLUDED_SRC_VirtIO_Virtio_1_0_h */
