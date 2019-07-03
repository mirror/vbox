/* $Id$ $Revision$ $Date$ $Author$ */
/** @file
 * VBox storage devices - Virtio SCSI Driver
 *
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_SCSI

#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmstorageifs.h>
#include <VBox/vmm/pdmcritsect.h>
#include <VBox/version.h>
#include <iprt/errcore.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include "../build/VBoxDD.h"
#include <VBox/scsi.h>
#ifdef IN_RING3
# include <iprt/alloc.h>
# include <iprt/memcache.h>
# include <iprt/param.h>
# include <iprt/uuid.h>
#endif
#include "VBoxSCSI.h"
#include "VBoxDD.h"

/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
#define VIRTIOSCSI_MAX_TARGETS  1

/**
 * Device Instance Data.
 */
typedef struct VIRTIOSCSITARGET
{
    /** Pointer to the owning virtioScsi device instance. - R3 pointer */
    R3PTRTYPE(struct VIRTIOSCSI *)  pVirtioScsiR3;

    /** Pointer to the owning virtioScsi device instance. - RC pointer */
    RCPTRTYPE(struct VIRTIOSCSI *)  pVirtioScsiRC;

    /** Pointer to the attached driver's base interface. */
    R3PTRTYPE(PPDMIBASE)          pDrvBase;

    /** Our base interface. */
    PDMIBASE                      IBase;

    /** LUN of the device. */
    RTUINT                        iLUN;

    /** Flag whether device is present. */
    bool                          fPresent;

    /** Led interface. */
    PDMILEDPORTS                  ILed;

    /** The status LED state for this device. */
    PDMLED                        Led;

} VIRTIOSCSITARGET;
typedef VIRTIOSCSITARGET *PVIRTIOSCSITARGET;


/**
 * Main VirtIO SCSI device state.
 *
 * @extends     PDMPCIDEV
 * @implements  PDMILEDPORTS
 */
typedef struct VIRTIOSCSI
{

    VIRTIOSCSITARGET                  aTargetInstances[VIRTIOSCSI_MAX_TARGETS];

    /** The PCI device structure. */
    PDMPCIDEV                       dev;
    /** Pointer to the device instance - HC ptr */
    PPDMDEVINSR3                    pDevInsR3;
    /** Pointer to the device instance - R0 ptr */
    PPDMDEVINSR0                    pDevInsR0;
    /** Pointer to the device instance - RC ptr. */
    PPDMDEVINSRC                    pDevInsRC;

    /** Queue to send tasks to R3. - HC ptr */
    R3PTRTYPE(PPDMQUEUE)            pNotifierQueueR3;
    /** Queue to send tasks to R3. - HC ptr */
    R0PTRTYPE(PPDMQUEUE)            pNotifierQueueR0;
    /** Queue to send tasks to R3. - RC ptr */
    RCPTRTYPE(PPDMQUEUE)            pNotifierQueueRC;

    /** The support driver session handle. */
    R3R0PTRTYPE(PSUPDRVSESSION)     pSupDrvSession;
    /** Worker thread. */
    R3PTRTYPE(PPDMTHREAD)           pThreadWrk;
    /** The event semaphore the processing thread waits on. */
    SUPSEMEVENT                     hEvtProcess;

    /** The base interface.
     * @todo use PDMDEVINS::IBase  */
    PDMIBASE                        IBase;

    /** Whether R0 is enabled. */
    bool                            fR0Enabled;
    /** Whether RC is enabled. */
    bool                            fGCEnabled;
    /** Number of ports detected */
    uint64_t                        cTargets;

    /** Base address of the I/O ports. */
    RTIOPORT                        IOPortBase;

    /** Base address of the memory mapping. */
    RTGCPHYS                        MMIOBase;

    /** Partner of ILeds. */
    R3PTRTYPE(PPDMILEDCONNECTORS)   pLedsConnector;

} VIRTIOSCSI, *PVIRTIOSCSI;


/**
 * Task state for a CCB request.
 */
typedef struct VIRTIOSCSIREQ
{
    /** Device this task is assigned to. */
    PVIRTIOSCSITARGET              pTargetDevice;
    /** The command control block from the guest. */
//    CCBU                           CCBGuest;
    /** Guest physical address of th CCB. */
    RTGCPHYS                       GCPhysAddrCCB;
    /** Pointer to the R3 sense buffer. */
    uint8_t                        *pbSenseBuffer;
    /** Flag whether this is a request from the BIOS. */
    bool                           fBIOS;
    /** 24-bit request flag (default is 32-bit). */
    bool                           fIs24Bit;
    /** SCSI status code. */
    uint8_t                        u8ScsiSts;
} VIRTIOSCSIREQ;
typedef struct VIRTIOSCSIREQ *PVIRTIOSCSIREQ;

#define VIRTIOSCSI_SAVED_STATE_MINOR_VERSION        0x01
/*********************************************************************************************************************************/

/**
 * virtio-scsi debugger info callback.
 *
 * @param   pDevIns     The device instance.
 * @param   pHlp        The output helpers.
 * @param   pszArgs     The arguments.
 */
static DECLCALLBACK(void) virtioScsiR3Info(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PVIRTIOSCSI   pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    bool        fVerbose = false;

    /* Parse arguments. */
    if (pszArgs)
        fVerbose = strstr(pszArgs, "verbose") != NULL;

    /* Show basic information. */
    pHlp->pfnPrintf(pHlp, "%s#%d: virtio-scsci ",
                    pDevIns->pReg->szName,
                    pDevIns->iInstance);
    pHlp->pfnPrintf(pHlp, "GC=%RTbool R0=%RTbool\n",
                    !!pThis->fGCEnabled, !!pThis->fR0Enabled);
}

/** @callback_method_impl{FNSSMDEVLIVEEXEC}  */
static DECLCALLBACK(int) virtioScsiR3LiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    RT_NOREF(uPass);
    NOREF(pSSM);
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    LogFunc(("callback"));
    NOREF(pThis);
    return VINF_SSM_DONT_CALL_AGAIN;
}

/** @callback_method_impl{FNSSMDEVLOADEEXEC}  */
static DECLCALLBACK(int) virtioScsiR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    RT_NOREF(uPass);
    NOREF(pSSM);
    NOREF(uVersion);
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    LogFunc(("callback"));
    NOREF(pThis);
    return VINF_SSM_DONT_CALL_AGAIN;
}

/** @callback_method_impl{FNSSMDEVSAVEEXEC}  */
static DECLCALLBACK(int) virtioScsiR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    RT_NOREF(pSSM);
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    LogFunc(("callback"));
    NOREF(pThis);
    return VINF_SUCCESS;
}

/** @callback_method_impl{FNSSMDEVLOADDONE}  */
static DECLCALLBACK(int) virtioScsiR3LoadDone(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    RT_NOREF(pSSM);
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    LogFunc(("callback"));
    NOREF(pThis);
    return VINF_SUCCESS;
}

/**
 * Memory mapped I/O Handler for read operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   GCPhysAddr  Physical address (in GC) where the read starts.
 * @param   pv          Where to store the result.
 * @param   cb          Number of bytes read.
 */
PDMBOTHCBDECL(int) virtioScsiMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    RT_NOREF_PV(pDevIns); RT_NOREF_PV(pvUser); RT_NOREF_PV(GCPhysAddr); RT_NOREF_PV(pv); RT_NOREF_PV(cb);

    /* the linux driver does not make use of the MMIO area. */
    AssertMsgFailed(("MMIO Read\n"));
    return VINF_SUCCESS;
}


/**
 * Memory mapped I/O Handler for write operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   GCPhysAddr  Physical address (in GC) where the read starts.
 * @param   pv          Where to fetch the result.
 * @param   cb          Number of bytes to write.
 */
PDMBOTHCBDECL(int) virtioScsiMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb)
{
    RT_NOREF_PV(pDevIns); RT_NOREF_PV(pvUser); RT_NOREF_PV(GCPhysAddr); RT_NOREF_PV(pv); RT_NOREF_PV(cb);

    /* the linux driver does not make use of the MMIO area. */
    AssertMsgFailed(("MMIO Write\n"));
    return VINF_SUCCESS;
}

/**
 * Port I/O Handler for IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   uPort       Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
PDMBOTHCBDECL(int) virtioScsiIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT uPort, uint32_t *pu32, unsigned cb)
{
//    PVIRTIOSCSI pVirtioScsi = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    unsigned iRegister = uPort % 4;
    NOREF(iRegister);
    NOREF(pDevIns);
    NOREF(pu32);
    RT_NOREF_PV(pvUser); RT_NOREF_PV(cb);

    Assert(cb == 1);

//    return buslogicRegisterRead(pBusLogic, iRegister, pu32);
    return 0;
}


/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   uPort       Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) virtioScsiIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT uPort, uint32_t u32, unsigned cb)
{
//    PVIRTIOSCSI pVirtioScsi = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    unsigned iRegister = uPort % 4;
    uint8_t uVal = (uint8_t)u32;
    NOREF(uVal);
    NOREF(iRegister);
    RT_NOREF2(pvUser, cb);
    NOREF(u32);

    Assert(cb == 1);

//    int rc = buslogicRegisterWrite(pBusLogic, iRegister, (uint8_t)uVal);
    int rc = 0;
    Log2(("#%d %s: pvUser=%#p cb=%d u32=%#x uPort=%#x rc=%Rrc\n",
          pDevIns->iInstance, __FUNCTION__, pvUser, cb, u32, uPort, rc));

    return rc;
}

/**
 * @callback_method_impl{FNPCIIOREGIONMAP}
 */
static DECLCALLBACK(int) devVirtioScsiR3MmioMap(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, uint32_t iRegion,
                                           RTGCPHYS GCPhysAddress, RTGCPHYS cb, PCIADDRESSSPACE enmType)
{
    RT_NOREF(pPciDev, iRegion);
    PVIRTIOSCSI  pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    int   rc = VINF_SUCCESS;

    LogFunc(("Register MMIO area at GCPhysAddr=%RGp cb=%RGp\n", GCPhysAddress, cb));

    Assert(cb >= 32);

    if (enmType == PCI_ADDRESS_SPACE_MEM)
    {
        /* We use the assigned size here, because we currently only support page aligned MMIO ranges. */
        rc = PDMDevHlpMMIORegister(pDevIns, GCPhysAddress, cb, NULL /*pvUser*/,
                                   IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU,
                                   virtioScsiMMIOWrite, virtioScsiMMIORead, "virtio-scsi MMIO");
        if (RT_FAILURE(rc))
            return rc;

        if (pThis->fGCEnabled)
        {
            rc = PDMDevHlpMMIORegisterRC(pDevIns, GCPhysAddress, cb, NIL_RTRCPTR /*pvUser*/,
                                         "virtioScsiMMIOWrite", "virtioScsiMMIORead");
            if (RT_FAILURE(rc))
                return rc;
        }

        pThis->MMIOBase = GCPhysAddress;
    }
    else if (enmType == PCI_ADDRESS_SPACE_IO)
    {
        rc = PDMDevHlpIOPortRegister(pDevIns, (RTIOPORT)GCPhysAddress, 32,
                                     NULL, virtioScsiIOPortWrite, virtioScsiIOPortRead, NULL, NULL, "virtio-scsi PCI");
        if (RT_FAILURE(rc))
            return rc;

        if (pThis->fGCEnabled)
        {
            rc = PDMDevHlpIOPortRegisterRC(pDevIns, (RTIOPORT)GCPhysAddress, 32,
                                           0, "virtioScsiIOPortWrite", "virtioScsiIOPortRead", NULL, NULL, "virtio-scsi PCI");
            if (RT_FAILURE(rc))
                return rc;
        }

        pThis->IOPortBase = (RTIOPORT)GCPhysAddress;
    }
    else
        AssertMsgFailed(("Invalid enmType=%d\n", enmType));

    return rc;
}


/**
 * @copydoc FNPDMDEVRESET
 */
static DECLCALLBACK(void) virtioScsiR3Reset(PPDMDEVINS pDevIns)
{
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    NOREF(pThis);

//    ASMAtomicWriteBool(&pThis->fSignalIdle, true);
//    if (!virtioScsiR3AllAsyncIOIsFinished(pDevIns))
//        PDMDevHlpSetAsyncNotification(pDevIns, virtioScsiR3IsAsyncResetDone);
//    else
//    {
//        ASMAtomicWriteBool(&pThis->fSignalIdle, false);
//   }
}

static DECLCALLBACK(void) virtioScsiR3Relocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
LogFunc(("Relocating virtio-scsci"));
    RT_NOREF(offDelta);
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

    pThis->pDevInsR3 = pDevIns;
//    pThis->pNotifierQueueRC = PDMQueueRCPtr(pThis->pNotifierQueueR3);

    for (uint32_t i = 0; i < VIRTIOSCSI_MAX_TARGETS; i++)
    {
        PVIRTIOSCSITARGET pTarget = &pThis->aTargetInstances[i];
        pTarget->pVirtioScsiR3 = pThis;;

  }

}

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) virtioScsiR3DeviceQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PVIRTIOSCSITARGET pTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pTarget->IBase);
//    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIAPORT, &pTarget->IMediaPort);
//    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIAEXPORT, &pTarget->IMediaExPort);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMILEDPORTS, &pTarget->ILed);
    return NULL;
}

/**
 * Transmit queue consumer
 * Queue a new async task.
 *
 * @returns Success indicator.
 *          If false the item will not be removed and the flushing will stop.
 * @param   pDevIns     The device instance.
 * @param   pItem       The item to consume. Upon return this item will be freed.
 */
static DECLCALLBACK(bool) virtioScsiR3NotifyQueueConsumer(PPDMDEVINS pDevIns, PPDMQUEUEITEMCORE pItem)
{
    RT_NOREF(pItem);
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

    int rc = SUPSemEventSignal(pThis->pSupDrvSession, pThis->hEvtProcess);
    AssertRC(rc);

    return true;
}

static DECLCALLBACK(int) devVirtIoScsiDestruct(PPDMDEVINS pDevIns)
{
    /*
     * Check the versions here as well since the destructor is *always* called.
     */
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) devVirtioScsiConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{

    PVIRTIOSCSI  pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    int        rc = VINF_SUCCESS;
    bool       fBootable = true;
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

    NOREF(fBootable);
    NOREF(iInstance);

    /*
     * Init instance data (do early because of constructor).
     */
    pThis->pDevInsR3 = pDevIns;
    pThis->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

    PCIDevSetVendorId         (&pThis->dev, 0x1AF4); /* VirtIO */
    PCIDevSetDeviceId         (&pThis->dev, 0x1048); /* SCSI Host */
    PCIDevSetCommand          (&pThis->dev, PCI_COMMAND_IOACCESS | PCI_COMMAND_MEMACCESS);
    PCIDevSetRevisionId       (&pThis->dev, 0x01);
    PCIDevSetClassProg        (&pThis->dev, 0x00); /* SCSI */
    PCIDevSetClassSub         (&pThis->dev, 0x00); /* SCSI */
    PCIDevSetClassBase        (&pThis->dev, 0x01); /* Mass storage */
    PCIDevSetBaseAddress      (&pThis->dev, 0, true  /*IO*/, false /*Pref*/, false /*64-bit*/, 0x00000000);
    PCIDevSetBaseAddress      (&pThis->dev, 1, false /*IO*/, false /*Pref*/, false /*64-bit*/, 0x00000000);
    PCIDevSetSubSystemVendorId(&pThis->dev, 0x104b);
    PCIDevSetSubSystemId      (&pThis->dev, 0x1040);
    PCIDevSetInterruptLine    (&pThis->dev, 0x00);
    PCIDevSetInterruptPin     (&pThis->dev, 0x01);

    /*
     * Validate and read configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg,
                              "GCEnabled\0"
                              "R0Enabled\0"
                              "NumTargets\0"))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("virtio-scsi configuration error: unknown option specified"));

    rc = CFGMR3QueryBoolDef(pCfg, "GCEnabled", &pThis->fGCEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("virtio-scsi configuration error: failed to read GCEnabled as boolean"));
    LogFunc(("fGCEnabled=%d\n", pThis->fGCEnabled));

    rc = CFGMR3QueryBoolDef(pCfg, "R0Enabled", &pThis->fR0Enabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("virtio-scsi configuration error: failed to read R0Enabled as boolean"));
    LogFunc(("R0Enabled=%d\n", pThis->fGCEnabled));

    rc = CFGMR3QueryIntegerDef(pCfg, "NumTargets", &pThis->cTargets, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("virtio-scsi configuration error: failed to read NumTargets as integer"));
    LogFunc(("NumTargets=%d\n", pThis->fGCEnabled));


LogFunc(("Register PCI device\n"));
    rc = PDMDevHlpPCIRegister(pDevIns, &pThis->dev);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-scsi cannot register with PCI bus"));

LogFunc(("Register io region\n"));
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 0, 32, PCI_ADDRESS_SPACE_IO, devVirtioScsiR3MmioMap);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-scsi cannot register PCI I/O address space"));
LogFunc(("Register mmio region\n"));
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 1, 32, PCI_ADDRESS_SPACE_MEM, devVirtioScsiR3MmioMap);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-scsi cannot register PCI mmio address space"));

#if 0
    if (fBootable)
    {
        /* Register I/O port space for BIOS access. */
        rc = PDMDevHlpIOPortRegister(pDevIns, VIRTIOSCSI_BIOS_IO_PORT, 4, NULL,
                                     virtioScsiR3BiosIoPortWrite, virtioScsiR3BiosIoPortRead,
                                     virtioScsiR3BiosIoPortWriteStr, virtioScsiR3BiosIoPortReadStr,
                                     "virtio-scsi BIOS");
        if (RT_FAILURE(rc))
            return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-scsi cannot register BIOS I/O handlers"));
    }
#endif

    /* Initialize task queue. */
    rc = PDMDevHlpQueueCreate(pDevIns, sizeof(PDMQUEUEITEMCORE), 5, 0,
                              virtioScsiR3NotifyQueueConsumer, true, "VirtioTask", &pThis->pNotifierQueueR3);
    if (RT_FAILURE(rc))
        return rc;

    pThis->pNotifierQueueR0 = PDMQueueR0Ptr(pThis->pNotifierQueueR3);
    pThis->pNotifierQueueRC = PDMQueueRCPtr(pThis->pNotifierQueueR3);

    /* Initialize per device instance. */
    for (unsigned i = 0; i < VIRTIOSCSI_MAX_TARGETS; i++)
    {
        PVIRTIOSCSITARGET pTarget = &pThis->aTargetInstances[i];
        char *pszName;

        if (RTStrAPrintf(&pszName, "Device%u", i) < 0)
            AssertLogRelFailedReturn(VERR_NO_MEMORY);

        /* Initialize static parts of the device. */
        pTarget->iLUN = i;
        pTarget->pVirtioScsiR3 = pThis;
        pTarget->pVirtioScsiRC = PDMINS_2_DATA_RCPTR(pDevIns);
        pTarget->Led.u32Magic = PDMLED_MAGIC;
        pTarget->IBase.pfnQueryInterface                 = virtioScsiR3DeviceQueryInterface;
/*      pTarget->IMediaPort.pfnQueryDeviceLocation       = virtioScsiR3QueryDeviceLocation;
        pTarget->IMediaExPort.pfnIoReqCompleteNotify     = virtioScsiR3IoReqCompleteNotify;
        pTarget->IMediaExPort.pfnIoReqCopyFromBuf        = virtioScsiR3IoReqCopyFromBuf;
        pTarget->IMediaExPort.pfnIoReqCopyToBuf          = virtioScsiR3IoReqCopyToBuf;
        pTarget->IMediaExPort.pfnIoReqQueryBuf           = NULL;
        pTarget->IMediaExPort.pfnIoReqQueryDiscardRanges = NULL;
        pTarget->IMediaExPort.pfnIoReqStateChanged       = virtioScsiR3IoReqStateChanged;
        pTarget->IMediaExPort.pfnMediumEjected           = virtioScsiR3MediumEjected;
        pTarget->ILed.pfnQueryStatusLed                  = virtioScsiR3DeviceQueryStatusLed;
*/
        LogFunc(("Attaching: Lun=%d, pszName=%s\n",  pTarget->iLUN, pszName));
        /* Attach SCSI driver. */
        AssertReturn(pTarget->iLUN < RT_ELEMENTS(pThis->aTargetInstances), VERR_PDM_NO_SUCH_LUN);
        rc = PDMDevHlpDriverAttach(pDevIns, pTarget->iLUN, &pTarget->IBase, &pTarget->pDrvBase, pszName);
        if (RT_SUCCESS(rc))
            pTarget->fPresent = true;
        else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        {
            pTarget->fPresent    = false;
            pTarget->pDrvBase    = NULL;
            rc = VINF_SUCCESS;
            Log(("virtio-scsi: no driver attached to device %s\n", pszName));
        }
        else
        {
            AssertLogRelMsgFailed(("virtio-scsi: Failed to attach %s\n", pszName));
            return rc;
        }
    }

#if 0
    /*
     * Attach status driver (optional).
     */

    PPDMIBASE pBase;
    rc = PDMDevHlpDriverAttach(pDevIns, PDM_STATUS_LUN, &pThis->IBase, &pBase, "Status Port");
    if (RT_SUCCESS(rc))
        pThis->pLedsConnector = PDMIBASE_QUERY_INTERFACE(pBase, PDMILEDCONNECTORS);
    else if (rc != VERR_PDM_NO_ATTACHED_DRIVER)
    {
        AssertMsgFailed(("Failed to attach to status driver. rc=%Rrc\n", rc));
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-scsi cannot attach to status driver"));
    }
#endif

    rc = PDMDevHlpSSMRegisterEx(pDevIns, VIRTIOSCSI_SAVED_STATE_MINOR_VERSION, sizeof(*pThis), NULL,
                                NULL, virtioScsiR3LiveExec, NULL,
                                NULL, virtioScsiR3SaveExec, NULL,
                                NULL, virtioScsiR3LoadExec, virtioScsiR3LoadDone);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-scsi cannot register save state handlers"));
    /*
     * Register the debugger info callback.
     */
    char szTmp[128];
    RTStrPrintf(szTmp, sizeof(szTmp), "%s%d", pDevIns->pReg->szName, pDevIns->iInstance);
    PDMDevHlpDBGFInfoRegister(pDevIns, szTmp, "virtio-scsi HBA info", virtioScsiR3Info);

    return rc;
}


/**
 * Detach notification.
 *
 * One harddisk at one port has been unplugged.
 * The VM is suspended at this point.
 *
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being detached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 */
static DECLCALLBACK(void) devVirtioScsiR3Detach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    RT_NOREF(fFlags);
    PVIRTIOSCSI       pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    PVIRTIOSCSITARGET pTarget = &pThis->aTargetInstances[iLUN];

    Log(("%s:\n", __FUNCTION__));

    AssertMsg(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG,
              ("virtio-scsi: Device does not support hotplugging\n"));

    /*
     * Zero some important members.
     */
    pTarget->fPresent    = false;
    pTarget->pDrvBase    = NULL;
}


/**
 * Attach command.
 *
 * This is called when we change block driver.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being detached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 */
static DECLCALLBACK(int)  devVirtioScsiR3Attach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PVIRTIOSCSI       pThis   = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    PVIRTIOSCSITARGET pTarget = &pThis->aTargetInstances[iLUN];
    int rc;

LogFlowFunc(("iLun=%d"));

    AssertMsgReturn(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG,
                    ("virtio-scsi: Device does not support hotplugging\n"),
                    VERR_INVALID_PARAMETER);

    /* the usual paranoia */
    AssertRelease(!pTarget->pDrvBase);
    Assert(pTarget->iLUN == iLUN);

    /*
     * Try attach the SCSI driver and get the interfaces,
     * required as well as optional.
     */
    rc = PDMDevHlpDriverAttach(pDevIns, pTarget->iLUN, &pTarget->IBase, &pTarget->pDrvBase, NULL);
    if (RT_SUCCESS(rc))
        pTarget->fPresent = true;
    else
        AssertMsgFailed(("Failed to attach LUN#%d. rc=%Rrc\n", pTarget->iLUN, rc));

    if (RT_FAILURE(rc))
    {
        pTarget->fPresent    = false;
        pTarget->pDrvBase    = NULL;
    }
    return rc;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceVirtioSCSI =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "virtio-scsi",
    /* szRCMod */
    "VBoxDDRC.rc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "Virtio SCSI Device.",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS,
    /* fClass */
    PDM_DEVREG_CLASS_MISC,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(VIRTIOSCSI),
    /* pfnConstruct */
    devVirtioScsiConstruct,
    /* pfnDestruct */
    devVirtIoScsiDestruct,
    /* pfnRelocate */
    virtioScsiR3Relocate,
    /* pfnMemSetup */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    virtioScsiR3Reset,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    devVirtioScsiR3Attach,
    /* pfnDetach */
    devVirtioScsiR3Detach,
    /* pfnQueryInterface */
    NULL,
    /* pfnInitComplete */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};

