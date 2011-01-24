/* $Id$ */
/** @file
 * PCI passthrough device emulation.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_PCI
#include <VBox/log.h>
#define PCI_INCLUDE_PRIVATE
#include <VBox/pci.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/log.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/pdmpci.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/uuid.h>

#include "VBoxDD.h"

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The version of the saved state. */
#define PCIRAW_SAVED_STATE_VERSION       1

/*******************************************************************************
 *   Structures and Typedefs                                                    *
 *******************************************************************************/

typedef struct PciRawState
{
    /** Pointer to the device instance. - R3 ptr. */
    PPDMDEVINSR3         pDevInsR3;
    /** The PCI raw helpers - R3 Ptr. */
    PCPDMPCIRAWHLPR3     pPciRawHlpR3;

    /** Pointer to the device instance. - R0 ptr. */
    PPDMDEVINSR0         pDevInsR0;
    /** The PCI raw helpers - R0 Ptr. */
    PCPDMPCIRAWHLPR0     pPciRawHlpR0;

    /** Pointer to the device instance. - RC ptr. */
    PPDMDEVINSRC         pDevInsRC;
    /** The PCI raw helpers - RC Ptr. */
    PCPDMPCIRAWHLPRC     pPciRawHlpRC;

    /* Virtual PCI device */
    PCIDEVICE            aPciDevice;

    /* Device name, as provided by Main */
    char                 szDeviceName[64];

    /* Address of device on the host */
    int32_t              i32HostDeviceAddress;
    /* Address of device in the guest */
    int32_t              i32GuestDeviceAddress;

    /* Global device lock */
    PDMCRITSECT          csLock;
    /**
     * Device port - LUN#0.
     *
     * @implements  PDMIBASE
     * @implements  PDMIPCIRAW
     */
    struct
    {
        /** The base interface for the PCI device port. */
        PDMIBASE                            IBase;
        /** The device port base interface. */
        PDMIPCIRAW                          IDevice;

        /** The base interface of the attached raw PCI driver. */
        R3PTRTYPE(PPDMIBASE)                pDrvBase;
        /** The device interface of the attached raw PCI driver. */
        R3PTRTYPE(PPDMIPCIRAWCONNECTOR)     pDrv;
    } Lun0;
} PciRawState;


/** Pointer to the raw PCI instance data. */
typedef PciRawState *PPciRawState;

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

RT_C_DECLS_BEGIN
PDMBOTHCBDECL(int) pcirawMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
PDMBOTHCBDECL(int) pcirawMMIORead (PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
PDMBOTHCBDECL(int) pcirawIOPortWrite (PPDMDEVINS pDevIns, void *pvUser,
                                      RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int) pcirawIOPortRead (PPDMDEVINS pDevIns, void *pvUser,
                                     RTIOPORT Port, uint32_t *pu32, unsigned cb);
RT_C_DECLS_END

DECLINLINE(int) pcirawLock(PciRawState* pThis, int rcBusy)
{
    return PDMCritSectEnter(&pThis->csLock, rcBusy);
}

DECLINLINE(void) pcirawUnlock(PciRawState* pThis)
{
    PDMCritSectLeave(&pThis->csLock);
}


PDMBOTHCBDECL(int)  pcirawMMIORead(PPDMDEVINS pDevIns,
                                   void *     pvUser,
                                   RTGCPHYS   GCPhysAddr,
                                   void *     pv,
                                   unsigned   cb)
{
    PciRawState * pThis  = PDMINS_2_DATA(pDevIns, PciRawState*);
    int         rc     = VINF_SUCCESS;

    LogFlow(("pcirawMMIORead: %llx (%x)\n", (uint64_t)GCPhysAddr, cb));

    rc = pcirawLock(pThis, VINF_IOM_HC_MMIO_READ);
    if (RT_UNLIKELY(rc != VINF_SUCCESS))
        return rc;

    switch (cb)
    {
        case 1:
        case 2:
        case 4:
        case 8:
        {
            break;
        }

        default:
            AssertReleaseMsgFailed(("cb=%d\n", cb)); /* for now we assume simple accesses. */
            rc = VINF_SUCCESS;
    }

    pcirawUnlock(pThis);

    return rc;
}

PDMBOTHCBDECL(int) pcirawMMIOWrite(PPDMDEVINS pDevIns,
                                   void  *    pvUser,
                                   RTGCPHYS   GCPhysAddr,
                                   void *     pv,
                                   unsigned   cb)
{
    PciRawState *pThis = PDMINS_2_DATA(pDevIns, PciRawState*);
    int rc = VINF_SUCCESS;

    LogFlow(("pcirawMMIOWrite: %llx (%d) <- %x\n",
             (uint64_t)GCPhysAddr, cb, *(uint32_t*)pv));

    rc = pcirawLock(pThis, VINF_IOM_HC_MMIO_WRITE);
    if (RT_UNLIKELY(rc != VINF_SUCCESS))
        return rc;

    switch (cb)
    {
        case 1:
        case 2:
        case 4:
        case 8:
        {
            break;
        }

        default:
            AssertReleaseMsgFailed(("cb=%d\n", cb)); /* for now we assume simple accesses. */
            rc = VERR_INTERNAL_ERROR;
    }

    pcirawUnlock(pThis);

    return rc;
}

PDMBOTHCBDECL(int) pcirawIOPortWrite (PPDMDEVINS pDevIns, void *pvUser,
                                      RTIOPORT Port, uint32_t u32, unsigned cb)
{
    return VINF_SUCCESS;
}

PDMBOTHCBDECL(int) pcirawIOPortRead (PPDMDEVINS pDevIns, void *pvUser,
                                     RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    return VINF_SUCCESS;
}

#ifdef IN_RING3
/**
 * @copydoc FNSSMDEVLIVEEXEC
 */
static DECLCALLBACK(int) pcirawLiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    PciRawState *pThis = PDMINS_2_DATA(pDevIns, PciRawState *);


    return VINF_SSM_DONT_CALL_AGAIN;
}

/**
 * Saves a state of the raw PCI device. Do nothing yet.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to save the state to.
 */
static DECLCALLBACK(int) pcirawSaveExec(PPDMDEVINS pDevIns,
                                        PSSMHANDLE pSSM)
{
    PciRawState *pThis = PDMINS_2_DATA(pDevIns, PciRawState *);

    /* The config. */
    pcirawLiveExec(pDevIns, pSSM, SSM_PASS_FINAL);

    return VINF_SUCCESS;
}

/**
 * Loads a state of the raw PCI device state.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to the saved state.
 * @param   uVersion    The data unit version number.
 * @param   uPass           The data pass.
 */
static DECLCALLBACK(int) pcirawLoadExec(PPDMDEVINS pDevIns,
                                        PSSMHANDLE pSSM,
                                        uint32_t   uVersion,
                                        uint32_t   uPass)
{
    PciRawState *pThis = PDMINS_2_DATA(pDevIns, PciRawState *);
    int        rc;

    if (uVersion != PCIRAW_SAVED_STATE_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    return VINF_SUCCESS;
}

/**
 * Relocation notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 * @param   offDelta    The delta relative to the old address.
 */
static DECLCALLBACK(void) pcirawRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    PciRawState *pThis = PDMINS_2_DATA(pDevIns, PciRawState *);
    unsigned i;
    LogFlow(("pcirawRelocate:\n"));

    pThis->pDevInsRC    = PDMDEVINS_2_RCPTR(pDevIns);
    pThis->pPciRawHlpRC   = pThis->pPciRawHlpR3->pfnGetRCHelpers(pDevIns);
}

/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void) pcirawReset(PPDMDEVINS pDevIns)
{
    PciRawState *pThis = PDMINS_2_DATA(pDevIns, PciRawState *);
    unsigned i;

    LogFlow(("pcirawReset:\n"));
}

static DECLCALLBACK(int) pcirawAttach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PciRawState *pThis = PDMINS_2_DATA(pDevIns, PciRawState *);

    LogFlow(("pcirawAttach: LUN%d\n", iLUN));
    /* Not yet used */
    return 0;
}

static DECLCALLBACK(void) pcirawDetach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PciRawState *pThis = PDMINS_2_DATA(pDevIns, PciRawState *);

    LogFlow(("pcirawDetach: LUN%d\n", iLUN));
    /* Not yet used */
}

/**
 * Initialization routine.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static int pcirawInit(PPDMDEVINS pDevIns, PciBusAddress hostAddress, PciBusAddress guestAddress)
{
    unsigned   i;
    int        rc;
    PciRawState *pThis = PDMINS_2_DATA(pDevIns, PciRawState *);

    memset(pThis, 0, sizeof(*pThis));

    pThis->pDevInsR3 = pDevIns;
    pThis->pDevInsR0  = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC  = PDMDEVINS_2_RCPTR(pDevIns);

    pThis->i32HostDeviceAddress = hostAddress.asLong();
    pThis->i32GuestDeviceAddress = guestAddress.asLong();

    pcirawReset(pDevIns);

    return VINF_SUCCESS;
}
static uint32_t pcirawConfigRead(PPCIDEVICE pPciDev, uint32_t Address, unsigned cb)
{
    PPDMDEVINS pDevIns = pPciDev->pDevIns;
    PciRawState*  pThis = PDMINS_2_DATA(pDevIns, PciRawState *);

    Log2(("rawpci: PCI config read: 0x%x (%d)\n", Address, cb));

    return 0;
}

static void pcirawConfigWrite(PPCIDEVICE pPciDev, uint32_t Address, uint32_t u32Value, unsigned cb)
{
    PPDMDEVINS  pDevIns = pPciDev->pDevIns;
    PciRawState  *pThis = PDMINS_2_DATA(pDevIns, PciRawState *);

    Log2(("rawpci: PCI config write: 0x%x -> 0x%x (%d)\n", u32Value, Address, cb));
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) pcirawQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PciRawState* pThis = RT_FROM_CPP_MEMBER(pInterface, PciRawState, Lun0.IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE,   &pThis->Lun0.IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIPCIRAW, &pThis->Lun0.IDevice);
    return NULL;
}

static DECLCALLBACK(int) pcirawFoo(PPDMIPCIRAW pInterface)
{
    return 0;

}

/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) pcirawConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PciRawState  *pThis = PDMINS_2_DATA(pDevIns, PciRawState *);
    int          rc;
    bool         fRCEnabled = false;
    bool         fR0Enabled = false;
    PDMPCIRAWREG PciRawReg;

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg,
                              "GCEnabled\0"
                              "R0Enabled\0"
                              "DeviceName\0"
                              "GuestPCIBusNo\0"
                              "GuestPCIDeviceNo\0"
                              "GuestPCIFunctionNo\0"
                              "HostPCIBusNo\0"
                              "HostPCIDeviceNo\0"
                              "HostPCIFunctionNo\0"
                              ))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    /* Query configuration. */
    rc = CFGMR3QueryBoolDef(pCfg, "GCEnabled", &fRCEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"GCEnabled\" as a bool failed"));

    rc = CFGMR3QueryBoolDef(pCfg, "R0Enabled", &fR0Enabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: failed to read R0Enabled as boolean"));


    rc = CFGMR3QueryString(pCfg, "DeviceName", pThis->szDeviceName, sizeof(pThis->szDeviceName));
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: failed to read DeviceName as string"));

    PciBusAddress hostAddress, guestAddress;

    /* Obtain device address info */
    uint32_t u32Bus, u32Device, u32Fn;

    do {
        rc = CFGMR3QueryU32(pCfg, "HostPCIBusNo", &u32Bus);
        if (RT_FAILURE(rc))
        {
            PDMDEV_SET_ERROR(pDevIns, rc,
                             N_("Configuration error: Querying \"HostPCIBusNo\" as a int failed"));
            break;
        }
        rc = CFGMR3QueryU32(pCfg, "HostPCIDeviceNo", &u32Device);
        if (RT_FAILURE(rc))
        {
            PDMDEV_SET_ERROR(pDevIns, rc,
                             N_("Configuration error: Querying \"HostPCIDeviceNo\" as a int failed"));
            break;
        }
        rc = CFGMR3QueryU32(pCfg, "HostPCIFunctionNo", &u32Fn);
        if (RT_FAILURE(rc))
        {
            PDMDEV_SET_ERROR(pDevIns, rc,
                             N_("Configuration error: Querying \"HostPCIFunctionNo\" as a int failed"));
            break;
        }
        hostAddress = PciBusAddress(u32Bus, u32Device, u32Fn);

        rc = CFGMR3QueryU32(pCfg, "GuestPCIBusNo", &u32Bus);
        if (RT_FAILURE(rc))
        {
            PDMDEV_SET_ERROR(pDevIns, rc,
                             N_("Configuration error: Querying \"GuestPCIBusNo\" as a int failed"));
            break;
        }
        rc = CFGMR3QueryU32(pCfg, "GuestPCIDeviceNo", &u32Device);
        if (RT_FAILURE(rc))
        {
            PDMDEV_SET_ERROR(pDevIns, rc,
                             N_("Configuration error: Querying \"GuestPCIDeviceNo\" as a int failed"));
            break;
        }
        rc = CFGMR3QueryU32(pCfg, "GuestPCIFunctionNo", &u32Fn);
        if (RT_FAILURE(rc))
        {
            PDMDEV_SET_ERROR(pDevIns, rc,
                         N_("Configuration error: Querying \"GuestPCIFunctionNo\" as a int failed"));
            break;
        }
        guestAddress = PciBusAddress(u32Bus, u32Device, u32Fn);

        /* Initialize the device state */
        rc = pcirawInit(pDevIns, hostAddress, guestAddress);
        if (RT_FAILURE(rc))
            break;

        pThis->pDevInsR3 = pDevIns;
        pThis->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
        pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

        /*
         * Register the raw device and get helpers.
         */
        PciRawReg.u32Version  = PDM_PCIRAWREG_VERSION;
        rc = PDMDevHlpPciRawRegister(pDevIns, &PciRawReg, &pThis->pPciRawHlpR3);
        if (RT_FAILURE(rc))
        {
            AssertMsgRC(rc, ("Cannot PciRawRegister: %Rrc\n", rc));
            break;
        }

        /*
         * Initialize critical section.
         */
        rc = PDMDevHlpCritSectInit(pDevIns, &pThis->csLock, RT_SRC_POS, "PCIRAW");
        if (RT_FAILURE(rc))
        {
            PDMDEV_SET_ERROR(pDevIns, rc, N_("Raw PCI device cannot initialize critical section"));
            break;
        }

        /* Mark device as passthrough */
        PCISetPassthrough(&pThis->aPciDevice);

        /* IBase */
        pThis->Lun0.IBase.pfnQueryInterface = pcirawQueryInterface;
        pThis->Lun0.IDevice.pfnFoo          = pcirawFoo;

        /*
         * Attach to the Main driver.
         */
        rc = pDevIns->pHlpR3->pfnDriverAttach(pDevIns, 0/*iLun*/, &pThis->Lun0.IBase, &pThis->Lun0.pDrvBase, "Device Port");
        if (RT_FAILURE(rc))
        {
            rc = PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS, N_("Raw PCI failed to attach Main driver"));
            break;
        }

        pThis->Lun0.pDrv = PDMIBASE_QUERY_INTERFACE(pThis->Lun0.pDrvBase, PDMIPCIRAWCONNECTOR);
        if (!pThis->Lun0.pDrv)
        {
            rc = PDMDevHlpVMSetError(pDevIns, VERR_PDM_MISSING_INTERFACE, RT_SRC_POS, N_("Raw PCI failed to query interface"));
            break;
        }

        /* Just a safety measure, this data shall never be reached */
        PCIDevSetVendorId(&pThis->aPciDevice, 0xdead);
        PCIDevSetDeviceId(&pThis->aPciDevice, 0xbeef);

        rc = PDMDevHlpPCIRegister(pDevIns, &pThis->aPciDevice);
        if (RT_FAILURE(rc))
            break;

        PDMDevHlpPCISetConfigCallbacks(pDevIns, &pThis->aPciDevice,
                                       pcirawConfigRead,  NULL /* we don't care about old ones */,
                                       pcirawConfigWrite, NULL /* we don't care about old ones */);

        /*
         * Register IO/MMIO ranges for the guest, basing on real device ranges.
         */
        for (int iRegion = 0; iRegion < VBOX_PCI_NUM_REGIONS; iRegion++)
        {
            RTHCPHYS RegStart;
            uint64_t iRegSize;
            bool     fMmio;

            if (pThis->Lun0.pDrv->pfnGetRegionInfo(pThis->Lun0.pDrv, hostAddress.asLong(), iRegion,
                                                   &RegStart, &iRegSize, &fMmio))
            {
                /* If region is present, register callbacks in guest.
                   @todo: replace it with direct region access with remap */
                // @todo: check if host's PA make sense for the guest
                if (fMmio)
                {
                    rc = PDMDevHlpMMIORegister(pDevIns, RegStart, iRegSize, NULL,
                                               pcirawMMIOWrite, pcirawMMIORead, NULL,
                                               "Raw PCI MMIO regions");
                    if (RT_FAILURE(rc))
                    {
                        AssertMsgRC(rc, ("Cannot register MMIO: %Rrc\n", rc));
                        break;
                    }
                }
                else
                {
                    rc = PDMDevHlpIOPortRegister(pDevIns, (RTIOPORT)RegStart, iRegSize, NULL,
                                                 pcirawIOPortWrite, pcirawIOPortRead, NULL, NULL,
                                                 "Raw PCI IO regions");
                    if (RT_FAILURE(rc))
                    {
                        AssertMsgRC(rc, ("Cannot register IO: %Rrc\n", rc));
                        break;
                    }
                }
            }
        }

        if (RT_FAILURE(rc))
            break;

        if (fRCEnabled)
        {
            pThis->pPciRawHlpRC = pThis->pPciRawHlpR3->pfnGetRCHelpers(pDevIns);
            if (!pThis->pPciRawHlpRC)
            {
                AssertReleaseMsgFailed(("cannot get RC helper\n"));
                rc = VERR_INTERNAL_ERROR;
                break;
            }
        }

        if (fR0Enabled)
        {
            pThis->pPciRawHlpR0 = pThis->pPciRawHlpR3->pfnGetR0Helpers(pDevIns);
            if (!pThis->pPciRawHlpR0)
            {
                AssertReleaseMsgFailed(("cannot get R0 helper\n"));
                rc = VERR_INTERNAL_ERROR;
                break;
            }
        }

        /* Register SSM callbacks */
        rc = PDMDevHlpSSMRegister3(pDevIns, PCIRAW_SAVED_STATE_VERSION, sizeof(*pThis), pcirawLiveExec, pcirawSaveExec, pcirawLoadExec);
        if (RT_FAILURE(rc))
            break;
    } while (0);

    /* Notify Main about result of PCI device attach attempt */
    if (pThis->Lun0.pDrv != NULL)
        pThis->Lun0.pDrv->pfnPciDeviceConstructComplete(pThis->Lun0.pDrv, hostAddress.asLong(), guestAddress.asLong(),
                                                        rc, pThis->szDeviceName);

    return rc;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DevicePciRaw =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "pciraw",
    /* szRCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "Raw PCI wrapper Device",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32_64 | PDM_DEVREG_FLAGS_PAE36 | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_HOST_DEV,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(PciRawState),
    /* pfnConstruct */
    pcirawConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    pcirawRelocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    pcirawReset,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnQueryInterface. */
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

#endif /* IN_RING3 */

#endif /* VBOX_DEVICE_STRUCT_TESTCASE */
