/** @file
 *
 * VirtualBox Driver Interface to VMM device
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

#include "VMMDev.h"
#include "ConsoleImpl.h"
#include "DisplayImpl.h"
#include "GuestImpl.h"

#include "Logging.h"

#include <VBox/pdm.h>
#include <VBox/VBoxDev.h>
#include <VBox/VBoxGuest.h>
#include <VBox/cfgm.h>
#include <VBox/err.h>
#include <iprt/asm.h>

#ifdef VBOX_HGCM
#include "hgcm/HGCM.h"
#include "hgcm/HGCMObjects.h"
#endif

//
// defines
//


//
// globals
//


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
#ifdef VBOX_HGCM
    /** Pointer to the HGCM port interface of the driver/device above us. */
    PPDMIHGCMPORT               pHGCMPort;
    /** Our HGCM connector interface. */
    PDMIHGCMCONNECTOR           HGCMConnector;
#endif
} DRVMAINVMMDEV, *PDRVMAINVMMDEV;

/** Converts PDMIVMMDEVCONNECTOR pointer to a DRVMAINVMMDEV pointer. */
#define PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface) ( (PDRVMAINVMMDEV) ((uintptr_t)pInterface - RT_OFFSETOF(DRVMAINVMMDEV, Connector)) )

#ifdef VBOX_HGCM
/** Converts PDMIHGCMCONNECTOR pointer to a DRVMAINVMMDEV pointer. */
#define PDMIHGCMCONNECTOR_2_MAINVMMDEV(pInterface) ( (PDRVMAINVMMDEV) ((uintptr_t)pInterface - RT_OFFSETOF(DRVMAINVMMDEV, HGCMConnector)) )
#endif

//
// constructor / destructor
//
VMMDev::VMMDev(Console *console) : mpDrv(NULL)
{
    mParent = console;
    int rc = RTSemEventCreate(&mCredentialsEvent);
    AssertRC(rc);
#ifdef VBOX_HGCM
    rc = HGCMHostInit ();
    AssertRC(rc);
#endif /* VBOX_HGCM */
    mu32CredentialsFlags = 0;
}

VMMDev::~VMMDev()
{
#ifdef VBOX_HGCM
    HGCMHostShutdown ();
#endif /* VBOX_HGCM */
    RTSemEventDestroy (mCredentialsEvent);
    if (mpDrv)
        mpDrv->pVMMDev = NULL;
    mpDrv = NULL;
}

PPDMIVMMDEVPORT VMMDev::getVMMDevPort()
{
    Assert(mpDrv);
    return mpDrv->pUpPort;
}



//
// public methods
//

/**
 * Wait on event semaphore for guest credential judgement result.
 */
int VMMDev::WaitCredentialsJudgement (uint32_t u32Timeout, uint32_t *pu32CredentialsFlags)
{
    if (u32Timeout == 0)
    {
        u32Timeout = 5000;
    }

    int rc = RTSemEventWait (mCredentialsEvent, u32Timeout);

    if (VBOX_SUCCESS (rc))
    {
        *pu32CredentialsFlags = mu32CredentialsFlags;
    }

    return rc;
}

int VMMDev::SetCredentialsJudgementResult (uint32_t u32Flags)
{
    mu32CredentialsFlags = u32Flags;

    int rc = RTSemEventSignal (mCredentialsEvent);
    AssertRC(rc);

    return rc;
}


/**
 * Report guest OS version.
 * Called whenever the Additions issue a guest version report request.
 *
 * @param   pInterface          Pointer to this interface.
 * @param   guestInfo           Pointer to guest information structure
 * @thread  The emulation thread.
 */
DECLCALLBACK(void) vmmdevUpdateGuestVersion(PPDMIVMMDEVCONNECTOR pInterface, VBoxGuestInfo *guestInfo)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);

    Assert(guestInfo);
    if (!guestInfo)
        return;

    /* store that information in IGuest */
    Guest* guest = pDrv->pVMMDev->getParent()->getGuest();
    Assert(guest);
    if (!guest)
        return;

    char version[20];
    sprintf(version, "%d", guestInfo->additionsVersion);
    guest->setAdditionsVersion(Bstr(version));

    /*
     * Tell the console interface about the event
     * so that it can notify its consumers.
     */
    pDrv->pVMMDev->getParent()->onAdditionsStateChange();
    
    if (guestInfo->additionsVersion < VMMDEV_VERSION)
    {
        pDrv->pVMMDev->getParent()->onAdditionsOutdated();
    }
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
DECLCALLBACK(void) vmmdevUpdateMouseCapabilities(PPDMIVMMDEVCONNECTOR pInterface, uint32_t newCapabilities)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);
    /*
     * Tell the console interface about the event
     * so that it can notify its consumers.
     */
    pDrv->pVMMDev->getParent()->onMouseCapabilityChange(BOOL (newCapabilities & VMMDEV_MOUSEGUESTWANTSABS),
                                                        BOOL (newCapabilities & VMMDEV_MOUSEGUESTNEEDSHOSTCUR));
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
DECLCALLBACK(void) vmmdevUpdatePointerShape(PPDMIVMMDEVCONNECTOR pInterface, bool fVisible, bool fAlpha,
                                            uint32_t xHot, uint32_t yHot,
                                            uint32_t width, uint32_t height,
                                            void *pShape)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);

    /* tell the console about it */
    pDrv->pVMMDev->getParent()->onMousePointerShapeChange(fVisible, fAlpha,
                                                          xHot, yHot, width, height, pShape);
}

DECLCALLBACK(int) iface_VideoAccelEnable(PPDMIVMMDEVCONNECTOR pInterface, bool fEnable, VBVAMEMORY *pVbvaMemory)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);

    Display *display = pDrv->pVMMDev->getParent()->getDisplay();

    if (display)
    {
        LogFlow(("MAIN::VMMDevInterface::iface_VideoAccelEnable: %d, %p\n", fEnable, pVbvaMemory));
        return display->VideoAccelEnable (fEnable, pVbvaMemory);
    }

    return VERR_NOT_SUPPORTED;
}
DECLCALLBACK(void) iface_VideoAccelFlush(PPDMIVMMDEVCONNECTOR pInterface)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);

    Display *display = pDrv->pVMMDev->getParent()->getDisplay();

    if (display)
    {
        LogFlow(("MAIN::VMMDevInterface::iface_VideoAccelFlush\n"));
        display->VideoAccelFlush ();
    }
}

DECLCALLBACK(int) vmmdevVideoModeSupported(PPDMIVMMDEVCONNECTOR pInterface, uint32_t width, uint32_t height,
                                           uint32_t bpp, bool *fSupported)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);

    if (!fSupported)
        return VERR_INVALID_PARAMETER;
    IFramebuffer *framebuffer = pDrv->pVMMDev->getParent()->getDisplay()->getFramebuffer();
    Assert(framebuffer);
    framebuffer->VideoModeSupported(width, height, bpp, (BOOL*)fSupported);
    return VINF_SUCCESS;
}

DECLCALLBACK(int) vmmdevGetHeightReduction(PPDMIVMMDEVCONNECTOR pInterface, uint32_t *heightReduction)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);

    if (!heightReduction)
        return VERR_INVALID_PARAMETER;
    IFramebuffer *framebuffer = pDrv->pVMMDev->getParent()->getDisplay()->getFramebuffer();
    Assert(framebuffer);
    framebuffer->COMGETTER(HeightReduction)((ULONG*)heightReduction);
    return VINF_SUCCESS;
}

DECLCALLBACK(int) vmmdevSetCredentialsJudgementResult(PPDMIVMMDEVCONNECTOR pInterface, uint32_t u32Flags)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);

    int rc = pDrv->pVMMDev->SetCredentialsJudgementResult (u32Flags);

    return rc;
}

#ifdef VBOX_HGCM

/* HGCM connector interface */

static DECLCALLBACK(int) iface_hgcmConnect (PPDMIHGCMCONNECTOR pInterface, PVBOXHGCMCMD pCmd, PHGCMSERVICELOCATION pServiceLocation, uint32_t *pu32ClientID)
{
    LogFlowFunc(("Enter\n"));

    PDRVMAINVMMDEV pDrv = PDMIHGCMCONNECTOR_2_MAINVMMDEV(pInterface);
    
    if (    !pServiceLocation
        || (   pServiceLocation->type != VMMDevHGCMLoc_LocalHost
            && pServiceLocation->type != VMMDevHGCMLoc_LocalHost_Existing))
    {
        return VERR_INVALID_PARAMETER;
    }

    return HGCMGuestConnect (pDrv->pHGCMPort, pCmd, pServiceLocation->u.host.achName, pu32ClientID);
}

static DECLCALLBACK(int) iface_hgcmDisconnect (PPDMIHGCMCONNECTOR pInterface, PVBOXHGCMCMD pCmd, uint32_t u32ClientID)
{
    LogFlowFunc(("Enter\n"));

    PDRVMAINVMMDEV pDrv = PDMIHGCMCONNECTOR_2_MAINVMMDEV(pInterface);

    return HGCMGuestDisconnect (pDrv->pHGCMPort, pCmd, u32ClientID);
}

static DECLCALLBACK(int) iface_hgcmCall (PPDMIHGCMCONNECTOR pInterface, PVBOXHGCMCMD pCmd, uint32_t u32ClientID, uint32_t u32Function,
                                         uint32_t cParms, PVBOXHGCMSVCPARM paParms)
{
    LogFlowFunc(("Enter\n"));

    PDRVMAINVMMDEV pDrv = PDMIHGCMCONNECTOR_2_MAINVMMDEV(pInterface);

    return HGCMGuestCall (pDrv->pHGCMPort, pCmd, u32ClientID, u32Function, cParms, paParms);
}

/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pDrvIns         Driver instance of the driver which registered the data unit.
 * @param   pSSM            SSM operation handle.
 */
static DECLCALLBACK(int) iface_hgcmSave(PPDMDRVINS pDrvIns, PSSMHANDLE pSSM)
{
    LogFlowFunc(("Enter\n"));
    return HGCMHostSaveState (pSSM);
}


/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pDrvIns         Driver instance of the driver which registered the data unit.
 * @param   pSSM            SSM operation handle.
 * @param   u32Version      Data layout version.
 */
static DECLCALLBACK(int) iface_hgcmLoad(PPDMDRVINS pDrvIns, PSSMHANDLE pSSM, uint32_t u32Version)
{
    LogFlowFunc(("Enter\n"));

    if (u32Version != HGCM_SSM_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    return HGCMHostLoadState (pSSM);
}

int VMMDev::hgcmLoadService (const char *pszServiceName, const char *pszServiceLibrary)
{
    return HGCMHostLoad (pszServiceName, pszServiceLibrary);
}

int VMMDev::hgcmHostCall (const char *pszServiceName, uint32_t u32Function,
                          uint32_t cParms, PVBOXHGCMSVCPARM paParms)
{
    return HGCMHostCall (pszServiceName, u32Function, cParms, paParms);
}
#endif /* HGCM */


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
#ifdef VBOX_HGCM
        case PDMINTERFACE_HGCM_CONNECTOR:
            return &pDrv->HGCMConnector;
#endif
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
    PDRVMAINVMMDEV pData = PDMINS2DATA(pDrvIns, PDRVMAINVMMDEV);
    LogFlow(("VMMDev::drvDestruct: iInstance=%d\n", pDrvIns->iInstance));
#ifdef VBOX_HGCM
    /* HGCM is shut down on the VMMDev destructor. */
#endif /* VBOX_HGCM */
    if (pData->pVMMDev)
    {
        pData->pVMMDev->mpDrv = NULL;
    }
}

/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 */
DECLCALLBACK(void) VMMDev::drvReset(PPDMDRVINS pDrvIns)
{
    LogFlow(("VMMDev::drvReset: iInstance=%d\n", pDrvIns->iInstance));
#ifdef VBOX_HGCM
    HGCMHostReset ();
#endif /* VBOX_HGCM */
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
    pDrvIns->IBase.pfnQueryInterface                  = VMMDev::drvQueryInterface;

    pData->Connector.pfnUpdateGuestVersion            = vmmdevUpdateGuestVersion;
    pData->Connector.pfnUpdateMouseCapabilities       = vmmdevUpdateMouseCapabilities;
    pData->Connector.pfnUpdatePointerShape            = vmmdevUpdatePointerShape;
    pData->Connector.pfnVideoAccelEnable              = iface_VideoAccelEnable;
    pData->Connector.pfnVideoAccelFlush               = iface_VideoAccelFlush;
    pData->Connector.pfnVideoModeSupported            = vmmdevVideoModeSupported;
    pData->Connector.pfnGetHeightReduction            = vmmdevGetHeightReduction;
    pData->Connector.pfnSetCredentialsJudgementResult = vmmdevSetCredentialsJudgementResult;

#ifdef VBOX_HGCM
    pData->HGCMConnector.pfnConnect                   = iface_hgcmConnect;
    pData->HGCMConnector.pfnDisconnect                = iface_hgcmDisconnect;
    pData->HGCMConnector.pfnCall                      = iface_hgcmCall;
#endif

    /*
     * Get the IVMMDevPort interface of the above driver/device.
     */
    pData->pUpPort = (PPDMIVMMDEVPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_VMMDEV_PORT);
    if (!pData->pUpPort)
    {
        AssertMsgFailed(("Configuration error: No VMMDev port interface above!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }

#ifdef VBOX_HGCM
    pData->pHGCMPort = (PPDMIHGCMPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_HGCM_PORT);
    if (!pData->pHGCMPort)
    {
        AssertMsgFailed(("Configuration error: No HGCM port interface above!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }
#endif

    /*
     * Get the Console object pointer and update the mpDrv member.
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

#ifdef VBOX_HGCM
    rc = pData->pVMMDev->hgcmLoadService ("VBoxSharedFolders", "VBoxSharedFolders");
    pData->pVMMDev->fSharedFolderActive = VBOX_SUCCESS(rc);
    if (VBOX_SUCCESS(rc))
    {
        LogRel(("Shared Folders service loaded.\n"));
    }
    else
    {
        LogRel(("Failed to load Shared Folders service %Vrc\n", rc));
    }
    pDrvIns->pDrvHlp->pfnSSMRegister(pDrvIns, "HGCM", 0, HGCM_SSM_VERSION, 4096/* bad guess */, NULL, iface_hgcmSave, NULL, NULL, iface_hgcmLoad, NULL);
#endif /* VBOX_HGCM */

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
    VMMDev::drvReset,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnDetach */
    NULL
};
