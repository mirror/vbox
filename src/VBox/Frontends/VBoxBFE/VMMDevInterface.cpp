/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Implementation of VMMDev: driver interface to VMM device
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#define LOG_GROUP LOG_GROUP_MAIN

#ifdef VBOXBFE_WITHOUT_COM
# include "COMDefs.h"
#else
# include <VBox/com/defs.h>
#endif
#include <VBox/pdm.h>
#include <VBox/VBoxDev.h>
#include <VBox/VBoxGuest.h>
#include <VBox/cfgm.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <VBox/log.h>
#include <iprt/asm.h>

#include "VBoxBFE.h"
#include "VMMDevInterface.h"
#include "MouseImpl.h"
#include "DisplayImpl.h"
#include "ConsoleImpl.h"

#ifdef RT_OS_L4
#include <l4/util/util.h> /* for l4_sleep */
#endif
/**
 * VMMDev driver instance data.
 */
typedef struct DRVMAINVMMDEV
{
    /** Pointer to the VMMDev object. */
    VMMDev                     *pVMMDev;
    /** Pointer to the driver instance structure. */
    PPDMDRVINS                  pDrvIns;
    /** Pointer to the VMMDev port interface of the driver/device above us. */
    PPDMIVMMDEVPORT             pUpPort;
    /** Our VMM device connector interface. */
    PDMIVMMDEVCONNECTOR         Connector;
} DRVMAINVMMDEV, *PDRVMAINVMMDEV;

/** Converts PDMIVMMDEVCONNECTOR pointer to a DRVMAINVMMDEV pointer. */
#define PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface) ( (PDRVMAINVMMDEV) ((uintptr_t)pInterface - RT_OFFSETOF(DRVMAINVMMDEV, Connector)) )


PPDMIVMMDEVPORT VMMDev::getVMMDevPort()
{
    Assert(mpDrv);
    return mpDrv->pUpPort;
}

int VMMDev::SetMouseCapabilities(uint32_t mouseCaps)
{
    return mpDrv->pUpPort->pfnSetMouseCapabilities(mpDrv->pUpPort, mouseCaps);
}


int VMMDev::SetAbsoluteMouse(uint32_t mouseXAbs, uint32_t mouseYAbs)
{
    return mpDrv->pUpPort->pfnSetAbsoluteMouse(mpDrv->pUpPort, mouseXAbs, mouseYAbs);
}

void VMMDev::QueryMouseCapabilities(uint32_t *pMouseCaps)
{

    Assert(mpDrv);
    mpDrv->pUpPort->pfnQueryMouseCapabilities(mpDrv->pUpPort, pMouseCaps);
}


/**
 * Report guest OS version.
 * Called whenever the Additions issue a guest version report request.
 *
 * @param   pInterface          Pointer to this interface.
 * @param   guestInfo           Pointer to guest information structure
 * @thread  The emulation thread.
 */
DECLCALLBACK(void) VMMDev::UpdateGuestVersion(PPDMIVMMDEVCONNECTOR pInterface, VBoxGuestInfo *guestInfo)
{
    return;
}

/**
 * Update the guest additions capabilities.
 * This is called when the guest additions capabilities change. The new capabilities
 * are given and the connector should update its internal state.
 *
 * @param   pInterface          Pointer to this interface.
 * @param   newCapabilities     New capabilities.
 * @thread  The emulation thread.
 */
DECLCALLBACK(void) VMMDev::UpdateGuestCapabilities(PPDMIVMMDEVCONNECTOR pInterface, uint32_t newCapabilities)
{
    return;
}

/**
 * Update the mouse capabilities.
 * This is called when the mouse capabilities change. The new capabilities
 * are given and the connector should update its internal state.
 *
 * @param   pInterface          Pointer to this interface.
 * @param   newCapabilities     New capabilities.
 * @thread  The emulation thread.
 */
DECLCALLBACK(void) VMMDev::UpdateMouseCapabilities(PPDMIVMMDEVCONNECTOR pInterface, uint32_t newCapabilities)
{
    /*
     * Tell the console interface about the event so that it can notify its consumers.
     */

    if (gMouse)
    {
        gMouse->setAbsoluteCoordinates(!!(newCapabilities & VMMDEV_MOUSEGUESTWANTSABS));
        gMouse->setNeedsHostCursor(!!(newCapabilities & VMMDEV_MOUSEGUESTNEEDSHOSTCUR));
    }
    if (gConsole)
    {
        gConsole->resetCursor();
    }
}


/**
 * Update the pointer shape or visibility.
 *
 * This is called when the mouse pointer shape changes or pointer is hidden/displaying.
 * The new shape is passed as a caller allocated buffer that will be freed after returning.
 *
 * @param   pInterface          Pointer to this interface.
 * @param   fVisible            Whether the pointer is visible or not.
 * @param   fAlpha              Alpha channel information is present.
 * @param   xHot                Horizontal coordinate of the pointer hot spot.
 * @param   yHot                Vertical coordinate of the pointer hot spot.
 * @param   width               Pointer width in pixels.
 * @param   height              Pointer height in pixels.
 * @param   pShape              The shape buffer. If NULL, then only pointer visibility is being changed.
 * @thread  The emulation thread.
 */
DECLCALLBACK(void) VMMDev::UpdatePointerShape(PPDMIVMMDEVCONNECTOR pInterface, bool fVisible, bool fAlpha,
                                              uint32_t xHot, uint32_t yHot,
                                              uint32_t width, uint32_t height,
                                              void *pShape)
{
    if (gConsole)
        gConsole->onMousePointerShapeChange(fVisible, fAlpha, xHot,
                                            yHot, width, height, pShape);
}

DECLCALLBACK(int) iface_VideoAccelEnable(PPDMIVMMDEVCONNECTOR pInterface, bool fEnable, VBVAMEMORY *pVbvaMemory)
{
    LogFlow(("VMMDev::VideoAccelEnable: %d, %p\n", fEnable, pVbvaMemory));
    if (gDisplay)
        gDisplay->VideoAccelEnable (fEnable, pVbvaMemory);
    return VINF_SUCCESS;
}

DECLCALLBACK(void) iface_VideoAccelFlush(PPDMIVMMDEVCONNECTOR pInterface)
{
    if (gDisplay)
        gDisplay->VideoAccelFlush ();
}

DECLCALLBACK(int) iface_SetVisibleRegion(PPDMIVMMDEVCONNECTOR pInterface, uint32_t cRect, PRTRECT pRect)
{
    /* not implemented */
    return VINF_SUCCESS;
}

DECLCALLBACK(int) iface_QueryVisibleRegion(PPDMIVMMDEVCONNECTOR pInterface, uint32_t *pcRect, PRTRECT pRect)
{
    /* not implemented */
    return VINF_SUCCESS;
}

DECLCALLBACK(int) VMMDev::VideoModeSupported(PPDMIVMMDEVCONNECTOR pInterface, uint32_t width, uint32_t height,
                                             uint32_t bpp, bool *fSupported)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);
    (void)pDrv;

    if (!fSupported)
        return VERR_INVALID_PARAMETER;
    *fSupported = true;
    return VINF_SUCCESS;
}

DECLCALLBACK(int) VMMDev::GetHeightReduction(PPDMIVMMDEVCONNECTOR pInterface, uint32_t *heightReduction)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);
    (void)pDrv;

    if (!heightReduction)
        return VERR_INVALID_PARAMETER;
    /* XXX hard-coded */
    *heightReduction = 18;
    return VINF_SUCCESS;
}

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 */
DECLCALLBACK(void *) VMMDev::drvQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVMAINVMMDEV pDrv = PDMINS2DATA(pDrvIns, PDRVMAINVMMDEV);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_VMMDEV_CONNECTOR:
            return &pDrv->Connector;
        default:
            return NULL;
    }
}


/**
 * Destruct a VMMDev driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 */
DECLCALLBACK(void) VMMDev::drvDestruct(PPDMDRVINS pDrvIns)
{
    /*PDRVMAINVMMDEV pData = PDMINS2DATA(pDrvIns, PDRVMAINVMMDEV); - unused variables makes gcc bitch. */
}


/**
 * Construct a VMMDev driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
DECLCALLBACK(int) VMMDev::drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVMAINVMMDEV pData = PDMINS2DATA(pDrvIns, PDRVMAINVMMDEV);
    LogFlow(("Keyboard::drvConstruct: iInstance=%d\n", pDrvIns->iInstance));

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "Object\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

    PPDMIBASE pBaseIgnore;
    int rc = pDrvIns->pDrvHlp->pfnAttach(pDrvIns, &pBaseIgnore);
    if (rc != VERR_PDM_NO_ATTACHED_DRIVER)
    {
        AssertMsgFailed(("Configuration error: Not possible to attach anything to this driver!\n"));
        return VERR_PDM_DRVINS_NO_ATTACH;
    }

    /*
     * IBase.
     */
    pDrvIns->IBase.pfnQueryInterface            = VMMDev::drvQueryInterface;

    pData->Connector.pfnUpdateGuestVersion      = VMMDev::UpdateGuestVersion;
    pData->Connector.pfnUpdateGuestCapabilities = VMMDev::UpdateGuestCapabilities;
    pData->Connector.pfnUpdateMouseCapabilities = VMMDev::UpdateMouseCapabilities;
    pData->Connector.pfnUpdatePointerShape      = VMMDev::UpdatePointerShape;
    pData->Connector.pfnVideoAccelEnable        = iface_VideoAccelEnable;
    pData->Connector.pfnVideoAccelFlush         = iface_VideoAccelFlush;
    pData->Connector.pfnVideoModeSupported      = VMMDev::VideoModeSupported;
    pData->Connector.pfnGetHeightReduction      = VMMDev::GetHeightReduction;
    pData->Connector.pfnSetVisibleRegion        = iface_SetVisibleRegion;
    pData->Connector.pfnQueryVisibleRegion      = iface_QueryVisibleRegion;

    /*
     * Get the IVMMDevPort interface of the above driver/device.
     */
    pData->pUpPort = (PPDMIVMMDEVPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_VMMDEV_PORT);
    if (!pData->pUpPort)
    {
        AssertMsgFailed(("Configuration error: No VMMDev port interface above!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }

    /*
     * Get the VMMDev object pointer and update the mpDrv member.
     */
    void *pv;
    rc = CFGMR3QueryPtr(pCfgHandle, "Object", &pv);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: No/bad \"Object\" value! rc=%Vrc\n", rc));
        return rc;
    }

    pData->pVMMDev = (VMMDev*)pv;        /** @todo Check this cast! */
    pData->pVMMDev->mpDrv = pData;
    return VINF_SUCCESS;
}


/**
 * VMMDevice driver registration record.
 */
const PDMDRVREG VMMDev::DrvReg =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "MainVMMDev",
    /* pszDescription */
    "Main VMMDev driver (Main as in the API).",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_VMMDEV,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVMAINVMMDEV),
    /* pfnConstruct */
    VMMDev::drvConstruct,
    /* pfnDestruct */
    VMMDev::drvDestruct,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnDetach */
    NULL
};
