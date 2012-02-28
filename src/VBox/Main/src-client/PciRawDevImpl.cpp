/* $Id$ */
/** @file
 * VirtualBox Driver Interface to raw PCI device.
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#include "Logging.h"
#include "PciRawDevImpl.h"
#include "PciDeviceAttachmentImpl.h"
#include "ConsoleImpl.h"
#include "MachineImpl.h"

// generated header for events
#include "VBoxEvents.h"

/**
 * PCI raw driver instance data.
 */
typedef struct DRVMAINPCIRAWDEV
{
    /** Pointer to the real PCI raw object. */
    PciRawDev                   *pPciRawDev;
    /** Pointer to the driver instance structure. */
    PPDMDRVINS                  pDrvIns;
    /** Our PCI device connector interface. */
    PDMIPCIRAWCONNECTOR         IConnector;

} DRVMAINPCIRAWDEV, *PDRVMAINPCIRAWDEV;

//
// constructor / destructor
//
PciRawDev::PciRawDev(Console *console)
  : mpDrv(NULL),
    mParent(console)
{
}

PciRawDev::~PciRawDev()
{
}

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
DECLCALLBACK(void *) PciRawDev::drvQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS         pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVMAINPCIRAWDEV  pThis   = PDMINS_2_DATA(pDrvIns, PDRVMAINPCIRAWDEV);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE,            &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIPCIRAWCONNECTOR, &pThis->IConnector);

    return NULL;
}


/**
 * @interface_method_impl{PDMIPCIRAWUP,pfnPciDeviceConstructComplete}
 */
DECLCALLBACK(int) PciRawDev::drvDeviceConstructComplete(PPDMIPCIRAWCONNECTOR pInterface, const char *pcszName,
                                                        uint32_t uHostPciAddress, uint32_t uGuestPciAddress,
                                                        int rc)
{
    PDRVMAINPCIRAWDEV pThis = RT_FROM_CPP_MEMBER(pInterface, DRVMAINPCIRAWDEV, IConnector);
    Console *pConsole = pThis->pPciRawDev->getParent();
    const ComPtr<IMachine>& machine = pConsole->machine();
    ComPtr<IVirtualBox> vbox;

    HRESULT hrc = machine->COMGETTER(Parent)(vbox.asOutParam());
    Assert(SUCCEEDED(hrc));

    ComPtr<IEventSource> es;
    hrc = vbox->COMGETTER(EventSource)(es.asOutParam());
    Assert(SUCCEEDED(hrc));

    Bstr bstrId;
    hrc = machine->COMGETTER(Id)(bstrId.asOutParam());
    Assert(SUCCEEDED(hrc));

    ComObjPtr<PciDeviceAttachment> pda;
    BstrFmt bstrName(pcszName);
    pda.createObject();
    pda->init(machine, bstrName, uHostPciAddress, uGuestPciAddress, TRUE);

    Bstr msg("");
    if (RT_FAILURE(rc))
        msg = BstrFmt("runtime error %Rrc", rc);

    fireHostPciDevicePlugEvent(es, bstrId.raw(), true /* plugged */, RT_SUCCESS(rc) /* success */, pda, msg.raw());

    return VINF_SUCCESS;
}


/**
 * Destruct a PCI raw driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 */
DECLCALLBACK(void) PciRawDev::drvDestruct(PPDMDRVINS pDrvIns)
{
    PDRVMAINPCIRAWDEV pData = PDMINS_2_DATA(pDrvIns, PDRVMAINPCIRAWDEV);
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

    if (pData->pPciRawDev)
        pData->pPciRawDev->mpDrv = NULL;
}


/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 */
DECLCALLBACK(void) PciRawDev::drvReset(PPDMDRVINS pDrvIns)
{
}


/**
 * Construct a raw PCI driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
DECLCALLBACK(int) PciRawDev::drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle, uint32_t fFlags)
{
    PDRVMAINPCIRAWDEV pData = PDMINS_2_DATA(pDrvIns, PDRVMAINPCIRAWDEV);
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "Object\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

    AssertMsgReturn(PDMDrvHlpNoAttach(pDrvIns) == VERR_PDM_NO_ATTACHED_DRIVER,
                    ("Configuration error: Not possible to attach anything to this driver!\n"),
                    VERR_PDM_DRVINS_NO_ATTACH);

    /*
     * IBase.
     */
    pDrvIns->IBase.pfnQueryInterface = PciRawDev::drvQueryInterface;

    /*
     * IConnector.
     */
    pData->IConnector.pfnDeviceConstructComplete = PciRawDev::drvDeviceConstructComplete;

    /*
     * Get the object pointer and update the mpDrv member.
     */
    void *pv;
    int rc = CFGMR3QueryPtr(pCfgHandle, "Object", &pv);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: No \"Object\" value! rc=%Rrc\n", rc));
        return rc;
    }

    pData->pPciRawDev = (PciRawDev*)pv;
    pData->pPciRawDev->mpDrv = pData;

    return VINF_SUCCESS;
}

/**
 * Main raw PCI driver registration record.
 */
const PDMDRVREG PciRawDev::DrvReg =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "MainPciRaw",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Main PCI raw driver (Main as in the API).",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_PCIRAW,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVMAINPCIRAWDEV),
    /* pfnConstruct */
    PciRawDev::drvConstruct,
    /* pfnDestruct */
    PciRawDev::drvDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    PciRawDev::drvReset,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};
