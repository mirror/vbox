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
#include "../build/VBoxDD.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
#define VIRTIOSCSI_MAX_DEVICES  16

/**
 * Device Instance Data.
 */
typedef struct VIRTIOSCSIDEV
{
    /** Pointer to the owning virtioScsi device instance. - R3 pointer */
    R3PTRTYPE(struct VIRTIOSCSI *)  pVirtioScsiR3;

    /** Pointer to the attached driver's base interface. */
    R3PTRTYPE(PPDMIBASE)          pDrvBase;

    /** Our base interface. */
    PDMIBASE                      IBase;

    /** LUN of the device. */
    RTUINT                        iLUN;

    PDMIMEDIAPORT                 IMediaPort;
    /** Extended media port interface. */

    PDMIMEDIAEXPORT               IMediaExPort;
    /** Led interface. */

    R3PTRTYPE(PPDMIMEDIA)         pDrvMedia;
    /** Pointer to the attached driver's extended media interface. */

    R3PTRTYPE(PPDMIMEDIAEX)       pDrvMediaEx;
    /** The status LED state for this device. */


    /** Flag whether device is present. */
    bool                          fPresent;

} VIRTIOSCSIDEV;
typedef VIRTIOSCSIDEV *PVIRTIOSCSIDEV;


/**
 * Main VirtIO SCSI device state.
 *
 * @extends     PDMPCIDEV
 * @implements  PDMILEDPORTS
 */
typedef struct VIRTIOSCSI
{

    VIRTIOSCSIDEV                  aDevInstances[VIRTIOSCSI_MAX_DEVICES];

    /** The PCI device structure. */
    PDMPCIDEV                       dev;
    /** Pointer to the device instance - HC ptr */
    PPDMDEVINSR3                    pDevInsR3;
    /** Pointer to the device instance - R0 ptr */
    PPDMDEVINSR0                    pDevInsR0;
    /** Pointer to the device instance - RC ptr. */
    PPDMDEVINSRC                    pDevInsRC;

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

} VIRTIOSCSI, *PVIRTIOSCSI;


/**
 * Task state for a CCB request.
 */
typedef struct VIRTIOSCSIREQ
{
    /** PDM extended media interface I/O request hande. */
    PDMMEDIAEXIOREQ                hIoReq;
    /** Device this task is assigned to. */
    PVIRTIOSCSIDEV               pTargetDevice;
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


#if 0
/** @callback_method_impl{FNSSMDEVLIVEEXEC}  */
static DECLCALLBACK(int) virtioScsiR3LiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    RT_NOREF(uPass);
    NOREF(pSSM);
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
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
    NOREF(pThis);
    return VINF_SSM_DONT_CALL_AGAIN;
}

/** @callback_method_impl{FNSSMDEVSAVEEXEC}  */
static DECLCALLBACK(int) virtioScsiR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    RT_NOREF(pSSM);
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    NOREF(pThis);
    return VINF_SUCCESS;
}

/** @callback_method_impl{FNSSMDEVLOADDONE}  */
static DECLCALLBACK(int) virtioScsiR3LoadDone(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    RT_NOREF(pSSM);
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    NOREF(pThis);
    return VINF_SUCCESS;
}
#endif

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
//        virtioScsiR3HwReset(pThis, true);
//   }
}

static DECLCALLBACK(void) virtioScsiR3Relocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    RT_NOREF(offDelta);
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    NOREF(pThis);

//    pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
//    pThis->pNotifierQueueRC = PDMQueueRCPtr(pThis->pNotifierQueueR3);
//
//    for (uint32_t i = 0; i < VIRTIOSCSI_MAX_DEVICES; i++)
//   {
//        PVIRTIOSCSIDEVICE pDevice = &pThis->aDevInstances[i];
//
//        pDevice->pvirtio-scsiRC = PDMINS_2_DATA_RCPTR(pDevIns);
//
//    }

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
    Log(("%s: fGCEnabled=%d\n", __FUNCTION__, pThis->fGCEnabled));

    rc = CFGMR3QueryBoolDef(pCfg, "R0Enabled", &pThis->fR0Enabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("virtio-scsi configuration error: failed to read R0Enabled as boolean"));
    Log(("%s: fR0Enabled=%d\n", __FUNCTION__, pThis->fR0Enabled));

    rc = CFGMR3QueryIntegerDef(pCfg, "NumTargets", &pThis->cTargets, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("virtio-scsi configuration error: failed to read NumTargets as integer"));
    Log(("%s: fGCEnabled=%d\n", __FUNCTION__, pThis->fGCEnabled));

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

    /* Set up the compatibility I/O range. */
    rc = virtioScsiR3RegisterISARange(pThis, pThis->uDefaultISABaseCode);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-scsi cannot register ISA I/O handlers"));

    /* Initialize task queue. */
    rc = PDMDevHlpQueueCreate(pDevIns, sizeof(PDMQUEUEITEMCORE), 5, 0,
                              virtioScsiR3NotifyQueueConsumer, true, "virtio-scsiTask", &pThis->pNotifierQueueR3);
    if (RT_FAILURE(rc))
        return rc;
    pThis->pNotifierQueueR0 = PDMQueueR0Ptr(pThis->pNotifierQueueR3);
    pThis->pNotifierQueueRC = PDMQueueRCPtr(pThis->pNotifierQueueR3);

    rc = PDMDevHlpCritSectInit(pDevIns, &pThis->CritSectIntr, RT_SRC_POS, "virtio-scsi-Intr#%u", pDevIns->iInstance);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-scsi: cannot create critical section"));

    /*
     * Create event semaphore and worker thread.
     */
    rc = SUPSemEventCreate(pThis->pSupDrvSession, &pThis->hEvtProcess);
    if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("virtio-scsi: Failed to create SUP event semaphore"));

    char szDevTag[20];
    RTStrPrintf(szDevTag, sizeof(szDevTag), "VIRTIOSCSI-%u", iInstance);

    rc = PDMDevHlpThreadCreate(pDevIns, &pThis->pThreadWrk, pThis, virtioScsiR3Worker,
                               virtioScsiR3WorkerWakeUp, 0, RTTHREADTYPE_IO, szDevTag);
    if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("virtio-scsi: Failed to create worker thread %s"), szDevTag);
#endif

    /* Initialize per device instance. */
    for (unsigned i = 0; i < RT_ELEMENTS(pThis->aDevInstances); i++)
    {
        PVIRTIOSCSIDEV pDevice = &pThis->aDevInstances[i];

        char *pszName;
        if (RTStrAPrintf(&pszName, "Device%u", i) < 0)
            AssertLogRelFailedReturn(VERR_NO_MEMORY);

        /* Initialize static parts of the device. */
        pDevice->iLUN = i;
        pDevice->pVirtioScsiR3 = pThis;
Log(("Attaching: Lun=%d, pszName=%s\n",  pDevice->iLUN, pszName));
        /* Attach SCSI driver. */
        rc = PDMDevHlpDriverAttach(pDevIns, pDevice->iLUN, &pDevice->IBase, &pDevice->pDrvBase, pszName);
Log(("rc=%d\n", rc));
        if (RT_SUCCESS(rc))
        {
#if 0
            /* Query the media interface. */
            pDevice->pDrvMedia = PDMIBASE_QUERY_INTERFACE(pDevice->pDrvBase, PDMIMEDIA);
            AssertMsgReturn(VALID_PTR(pDevice->pDrvMedia),
                            ("virtio-scsi configuration error: LUN#%d misses the basic media interface!\n", pDevice->iLUN),
                            VERR_PDM_MISSING_INTERFACE);

            /* Get the extended media interface. */
            pDevice->pDrvMediaEx = PDMIBASE_QUERY_INTERFACE(pDevice->pDrvBase, PDMIMEDIAEX);
            AssertMsgReturn(VALID_PTR(pDevice->pDrvMediaEx),
                            ("virtio-scsi configuration error: LUN#%d misses the extended media interface!\n", pDevice->iLUN),
                            VERR_PDM_MISSING_INTERFACE);

#endif
            rc = pDevice->pDrvMediaEx->pfnIoReqAllocSizeSet(pDevice->pDrvMediaEx, sizeof(VIRTIOSCSIREQ));
            if (RT_FAILURE(rc))
                return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                           N_("virtio-scsi configuration error: LUN#%u: Failed to set I/O request size!"),
                                           pDevice->iLUN);

            pDevice->fPresent = true;
        }
        else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        {
            pDevice->fPresent    = false;
            pDevice->pDrvBase    = NULL;
//            pDevice->pDrvMedia   = NULL;
//            pDevice->pDrvMediaEx = NULL;
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
    {
//        pThis->pLedsConnector = PDMIBASE_QUERY_INTERFACE(pBase, PDMILEDCONNECTORS);
//        pThis->pMediaNotify = PDMIBASE_QUERY_INTERFACE(pBase, PDMIMEDIANOTIFY);
    }
    else if (rc != VERR_PDM_NO_ATTACHED_DRIVER)
    {
        AssertMsgFailed(("Failed to attach to status driver. rc=%Rrc\n", rc));
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-scsi cannot attach to status driver"));
    }

    rc = PDMDevHlpSSMRegisterEx(pDevIns, VIRTIOSCSI_SAVED_STATE_MINOR_VERSION, sizeof(*pThis), NULL,
                                NULL, virtioScsiR3LiveExec, NULL,
                                NULL, virtioScsiR3SaveExec, NULL,
                                NULL, virtioScsiR3LoadExec, virtioScsiR3LoadDone);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-scsi cannot register save state handlers"));

//#if 0
    /*
     * Register the debugger info callback.
     */
    char szTmp[128];
    RTStrPrintf(szTmp, sizeof(szTmp), "%s%d", pDevIns->pReg->szName, pDevIns->iInstance);
    PDMDevHlpDBGFInfoRegister(pDevIns, szTmp, "virtio-scsi HBA info", virtioScsiR3Info);


    rc = virtioScsiR3HwReset(pThis, true);
    AssertMsgRC(rc, ("hardware reset of virtio-scsi host adapter failed rc=%Rrc\n", rc));
#endif

    return rc;
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
    PVIRTIOSCSIDEV pDevice = &pThis->aDevInstances[iLUN];
    int rc;

LogFlowFunc(("iLun=%d"))

    AssertMsgReturn(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG,
                    ("virtio-scsi: Device does not support hotplugging\n"),
                    VERR_INVALID_PARAMETER);

    /* the usual paranoia */
    AssertRelease(!pDevice->pDrvBase);
    AssertRelease(!pDevice->pDrvMedia);
    AssertRelease(!pDevice->pDrvMediaEx);
    Assert(pDevice->iLUN == iLUN);

    /*
     * Try attach the SCSI driver and get the interfaces,
     * required as well as optional.
     */
    rc = PDMDevHlpDriverAttach(pDevIns, pDevice->iLUN, &pDevice->IBase, &pDevice->pDrvBase, NULL);
    if (RT_SUCCESS(rc))
    {
        /* Query the media interface. */
        pDevice->pDrvMedia = PDMIBASE_QUERY_INTERFACE(pDevice->pDrvBase, PDMIMEDIA);
        AssertMsgReturn(VALID_PTR(pDevice->pDrvMedia),
                        ("virtio-scsi configuration error: LUN#%d misses the basic media interface!\n", pDevice->iLUN),
                        VERR_PDM_MISSING_INTERFACE);

        /* Get the extended media interface. */
        pDevice->pDrvMediaEx = PDMIBASE_QUERY_INTERFACE(pDevice->pDrvBase, PDMIMEDIAEX);
        AssertMsgReturn(VALID_PTR(pDevice->pDrvMediaEx),
                        ("virtio-scsi configuration error: LUN#%d misses the extended media interface!\n", pDevice->iLUN),
                        VERR_PDM_MISSING_INTERFACE);

        rc = pDevice->pDrvMediaEx->pfnIoReqAllocSizeSet(pDevice->pDrvMediaEx, sizeof(VIRTIOSCSIREQ));
        AssertMsgRCReturn(rc, ("virtio-scsi configuration error: LUN#%u: Failed to set I/O request size!", pDevice->iLUN),
                          rc);
        pDevice->fPresent = true;
    }
    else
        AssertMsgFailed(("Failed to attach LUN#%d. rc=%Rrc\n", pDevice->iLUN, rc));

    if (RT_FAILURE(rc))
    {
        pDevice->fPresent    = false;
        pDevice->pDrvBase    = NULL;
        pDevice->pDrvMedia   = NULL;
        pDevice->pDrvMediaEx = NULL;
    }
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
    PVIRTIOSCSIDEV pDevice = &pThis->aDevInstances[iLUN];

    Log(("%s:\n", __FUNCTION__));

    AssertMsg(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG,
              ("virtio-scsi: Device does not support hotplugging\n"));

    /*
     * Zero some important members.
     */
    pDevice->fPresent    = false;
    pDevice->pDrvBase    = NULL;
//    pDevice->pDrvMedia   = NULL;
//    pDevice->pDrvMediaEx = NULL;

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
    sizeof(VIRTIOSCSIDEV),
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

