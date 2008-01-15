/** @file
 *
 * VirtualBox Driver Interface to VMM device
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VMMDev.h"
#include "ConsoleImpl.h"
#include "DisplayImpl.h"
#include "GuestImpl.h"

#include "Logging.h"

#include <VBox/pdmdrv.h>
#include <VBox/VBoxDev.h>
#include <VBox/VBoxGuest.h>
#include <VBox/shflsvc.h>
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
    RTStrPrintf(version, sizeof(version), "%d", guestInfo->additionsVersion);
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
 * Update the guest additions capabilities.
 * This is called when the guest additions capabilities change. The new capabilities
 * are given and the connector should update its internal state.
 *
 * @param   pInterface          Pointer to this interface.
 * @param   newCapabilities     New capabilities.
 * @thread  The emulation thread.
 */
DECLCALLBACK(void) vmmdevUpdateGuestCapabilities(PPDMIVMMDEVCONNECTOR pInterface, uint32_t newCapabilities)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);

    /* store that information in IGuest */
    Guest* guest = pDrv->pVMMDev->getParent()->getGuest();
    Assert(guest);
    if (!guest)
        return;

    Assert(!(newCapabilities & ~VMMDEV_GUEST_SUPPORTS_SEAMLESS));

    guest->setSupportsSeamless(BOOL (newCapabilities & VMMDEV_GUEST_SUPPORTS_SEAMLESS));

    /*
     * Tell the console interface about the event
     * so that it can notify its consumers.
     */
    pDrv->pVMMDev->getParent()->onAdditionsStateChange();
    
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
        LogSunlover(("MAIN::VMMDevInterface::iface_VideoAccelEnable: %d, %p\n", fEnable, pVbvaMemory));
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
        LogSunlover(("MAIN::VMMDevInterface::iface_VideoAccelFlush\n"));
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
    if (framebuffer)
        framebuffer->VideoModeSupported(width, height, bpp, (BOOL*)fSupported);
    else
        *fSupported = true;
    return VINF_SUCCESS;
}

DECLCALLBACK(int) vmmdevGetHeightReduction(PPDMIVMMDEVCONNECTOR pInterface, uint32_t *heightReduction)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);

    if (!heightReduction)
        return VERR_INVALID_PARAMETER;
    IFramebuffer *framebuffer = pDrv->pVMMDev->getParent()->getDisplay()->getFramebuffer();
    if (framebuffer)
        framebuffer->COMGETTER(HeightReduction)((ULONG*)heightReduction);
    else
        *heightReduction = 0;
    return VINF_SUCCESS;
}

DECLCALLBACK(int) vmmdevSetCredentialsJudgementResult(PPDMIVMMDEVCONNECTOR pInterface, uint32_t u32Flags)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);

    int rc = pDrv->pVMMDev->SetCredentialsJudgementResult (u32Flags);

    return rc;
}

DECLCALLBACK(int) vmmdevSetVisibleRegion(PPDMIVMMDEVCONNECTOR pInterface, uint32_t cRect, PRTRECT pRect)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);

    if (!cRect)
        return VERR_INVALID_PARAMETER;
    IFramebuffer *framebuffer = pDrv->pVMMDev->getParent()->getDisplay()->getFramebuffer();
    if (framebuffer)
        framebuffer->SetVisibleRegion((BYTE *)pRect, cRect);

    return VINF_SUCCESS;
}

DECLCALLBACK(int) vmmdevQueryVisibleRegion(PPDMIVMMDEVCONNECTOR pInterface, uint32_t *pcRect, PRTRECT pRect)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);

    IFramebuffer *framebuffer = pDrv->pVMMDev->getParent()->getDisplay()->getFramebuffer();
    if (framebuffer)
    {
        ULONG cRect = 0;
        framebuffer->GetVisibleRegion((BYTE *)pRect, cRect, &cRect);

        *pcRect = cRect;
    }

    return VINF_SUCCESS;
}

/**
 * Request the statistics interval
 *
 * @returns VBox status code.
 * @param   pInterface          Pointer to this interface.
 * @param   pulInterval         Pointer to interval in seconds
 * @thread  The emulation thread.
 */
DECLCALLBACK(int) vmmdevQueryStatisticsInterval(PPDMIVMMDEVCONNECTOR pInterface, uint32_t *pulInterval)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);
    ULONG          val = 0;

    if (!pulInterval)
        return VERR_INVALID_POINTER;

    /* store that information in IGuest */
    Guest* guest = pDrv->pVMMDev->getParent()->getGuest();
    Assert(guest);
    if (!guest)
        return VERR_INVALID_PARAMETER; /** @todo wrong error */

    guest->COMGETTER(StatisticsUpdateInterval)(&val);
    *pulInterval = val;
    return VINF_SUCCESS;
}

/**
 * Report new guest statistics
 *
 * @returns VBox status code.
 * @param   pInterface          Pointer to this interface.
 * @param   pGuestStats         Guest statistics
 * @thread  The emulation thread.
 */
DECLCALLBACK(int) vmmdevReportStatistics(PPDMIVMMDEVCONNECTOR pInterface, VBoxGuestStatistics *pGuestStats)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);

    Assert(pGuestStats);
    if (!pGuestStats)
        return VERR_INVALID_POINTER;

    /* store that information in IGuest */
    Guest* guest = pDrv->pVMMDev->getParent()->getGuest();
    Assert(guest);
    if (!guest)
        return VERR_INVALID_PARAMETER; /** @todo wrong error */

    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_CPU_LOAD_IDLE)
        guest->SetStatistic(pGuestStats->u32CpuId, GuestStatisticType_CPULoad_Idle, pGuestStats->u32CpuLoad_Idle);

    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_CPU_LOAD_KERNEL)
        guest->SetStatistic(pGuestStats->u32CpuId, GuestStatisticType_CPULoad_Kernel, pGuestStats->u32CpuLoad_Kernel);

    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_CPU_LOAD_USER)
        guest->SetStatistic(pGuestStats->u32CpuId, GuestStatisticType_CPULoad_User, pGuestStats->u32CpuLoad_User);

    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_THREADS)
        guest->SetStatistic(pGuestStats->u32CpuId, GuestStatisticType_Threads, pGuestStats->u32Threads);

    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_PROCESSES)
        guest->SetStatistic(pGuestStats->u32CpuId, GuestStatisticType_Processes, pGuestStats->u32Processes);

    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_HANDLES)
        guest->SetStatistic(pGuestStats->u32CpuId, GuestStatisticType_Handles, pGuestStats->u32Handles);

    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_MEMORY_LOAD)
        guest->SetStatistic(pGuestStats->u32CpuId, GuestStatisticType_MemoryLoad, pGuestStats->u32MemoryLoad);

    /* Note that reported values are in pages; upper layers expect them in megabytes */
    Assert(pGuestStats->u32PageSize == 4096);
    if (pGuestStats->u32PageSize != 4096)
        pGuestStats->u32PageSize = 4096;

    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_PHYS_MEM_TOTAL)
        guest->SetStatistic(pGuestStats->u32CpuId, GuestStatisticType_PhysMemTotal, (pGuestStats->u32PhysMemTotal + (_1M/pGuestStats->u32PageSize)-1) / (_1M/pGuestStats->u32PageSize));

    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_PHYS_MEM_AVAIL)
        guest->SetStatistic(pGuestStats->u32CpuId, GuestStatisticType_PhysMemAvailable, pGuestStats->u32PhysMemAvail / (_1M/pGuestStats->u32PageSize));

    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_PHYS_MEM_BALLOON)
        guest->SetStatistic(pGuestStats->u32CpuId, GuestStatisticType_PhysMemBalloon, pGuestStats->u32PhysMemBalloon / (_1M/pGuestStats->u32PageSize));

    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_MEM_COMMIT_TOTAL)
        guest->SetStatistic(pGuestStats->u32CpuId, GuestStatisticType_MemCommitTotal, pGuestStats->u32MemCommitTotal / (_1M/pGuestStats->u32PageSize));

    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_MEM_KERNEL_TOTAL)
        guest->SetStatistic(pGuestStats->u32CpuId, GuestStatisticType_MemKernelTotal, pGuestStats->u32MemKernelTotal / (_1M/pGuestStats->u32PageSize));

    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_MEM_KERNEL_PAGED)
        guest->SetStatistic(pGuestStats->u32CpuId, GuestStatisticType_MemKernelPaged, pGuestStats->u32MemKernelPaged / (_1M/pGuestStats->u32PageSize));

    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_MEM_KERNEL_NONPAGED)
        guest->SetStatistic(pGuestStats->u32CpuId, GuestStatisticType_MemKernelNonpaged, pGuestStats->u32MemKernelNonPaged / (_1M/pGuestStats->u32PageSize));

    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_MEM_SYSTEM_CACHE)
        guest->SetStatistic(pGuestStats->u32CpuId, GuestStatisticType_MemSystemCache, pGuestStats->u32MemSystemCache / (_1M/pGuestStats->u32PageSize));
    
    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_PAGE_FILE_SIZE)
        guest->SetStatistic(pGuestStats->u32CpuId, GuestStatisticType_PageFileSize, pGuestStats->u32PageFileSize / (_1M/pGuestStats->u32PageSize));

    /* increase sample number */
    ULONG sample;

    int rc = guest->GetStatistic(0, GuestStatisticType_SampleNumber, &sample);
    if (SUCCEEDED(rc))
        guest->SetStatistic(pGuestStats->u32CpuId, GuestStatisticType_SampleNumber, sample+1);

    return VINF_SUCCESS;
}

/**
 * Inflate or deflate the memory balloon
 *
 * @returns VBox status code.
 * @param   pInterface          Pointer to this interface.
 * @param   fInflate            Inflate or deflate
 * @param   cPages              Number of physical pages (must be 256 as we allocate in 1 MB chunks)
 * @param   aPhysPage           Array of physical page addresses
 * @thread  The emulation thread.
 */
DECLCALLBACK(int) vmmdevChangeMemoryBalloon(PPDMIVMMDEVCONNECTOR pInterface, bool fInflate, uint32_t cPages, RTGCPHYS *aPhysPage)
{
    if (    cPages != VMMDEV_MEMORY_BALLOON_CHUNK_PAGES
        ||  !aPhysPage)
        return VERR_INVALID_PARAMETER;

    Log(("vmmdevChangeMemoryBalloon @todo\n"));
    return VINF_SUCCESS;
}

#ifdef VBOX_HGCM

/* HGCM connector interface */

static DECLCALLBACK(int) iface_hgcmConnect (PPDMIHGCMCONNECTOR pInterface, PVBOXHGCMCMD pCmd, PHGCMSERVICELOCATION pServiceLocation, uint32_t *pu32ClientID)
{
    LogSunlover(("Enter\n"));

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
    LogSunlover(("Enter\n"));

    PDRVMAINVMMDEV pDrv = PDMIHGCMCONNECTOR_2_MAINVMMDEV(pInterface);

    return HGCMGuestDisconnect (pDrv->pHGCMPort, pCmd, u32ClientID);
}

static DECLCALLBACK(int) iface_hgcmCall (PPDMIHGCMCONNECTOR pInterface, PVBOXHGCMCMD pCmd, uint32_t u32ClientID, uint32_t u32Function,
                                         uint32_t cParms, PVBOXHGCMSVCPARM paParms)
{
    LogSunlover(("Enter\n"));

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
    LogSunlover(("Enter\n"));
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

void VMMDev::hgcmShutdown (void)
{
    HGCMHostShutdown ();
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
    if (!CFGMR3AreValuesValid(pCfgHandle, "Object\0OpenGLEnabled\0"))
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
    pData->Connector.pfnUpdateGuestCapabilities       = vmmdevUpdateGuestCapabilities;
    pData->Connector.pfnUpdateMouseCapabilities       = vmmdevUpdateMouseCapabilities;
    pData->Connector.pfnUpdatePointerShape            = vmmdevUpdatePointerShape;
    pData->Connector.pfnVideoAccelEnable              = iface_VideoAccelEnable;
    pData->Connector.pfnVideoAccelFlush               = iface_VideoAccelFlush;
    pData->Connector.pfnVideoModeSupported            = vmmdevVideoModeSupported;
    pData->Connector.pfnGetHeightReduction            = vmmdevGetHeightReduction;
    pData->Connector.pfnSetCredentialsJudgementResult = vmmdevSetCredentialsJudgementResult;
    pData->Connector.pfnSetVisibleRegion              = vmmdevSetVisibleRegion;
    pData->Connector.pfnQueryVisibleRegion            = vmmdevQueryVisibleRegion;
    pData->Connector.pfnReportStatistics              = vmmdevReportStatistics;
    pData->Connector.pfnQueryStatisticsInterval       = vmmdevQueryStatisticsInterval;
    pData->Connector.pfnChangeMemoryBalloon           = vmmdevChangeMemoryBalloon;

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
    rc = pData->pVMMDev->hgcmLoadService ("VBoxSharedFolders",
#ifdef RT_OS_OS2
                                          "VBoxSFld");
#else
                                          "VBoxSharedFolders");
#endif
    pData->pVMMDev->fSharedFolderActive = VBOX_SUCCESS(rc);
    if (VBOX_SUCCESS(rc))
    {
        PPDMLED       pLed;
        PPDMILEDPORTS pLedPort;

        LogRel(("Shared Folders service loaded.\n"));
        pLedPort = (PPDMILEDPORTS)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_LED_PORTS);
        if (!pLedPort)
        {
            AssertMsgFailed(("Configuration error: No LED port interface above!\n"));
            return VERR_PDM_MISSING_INTERFACE_ABOVE;
        }
        rc = pLedPort->pfnQueryStatusLed(pLedPort, 0, &pLed);
        if (VBOX_SUCCESS(rc) && pLed)
        {
            VBOXHGCMSVCPARM  parm;

            parm.type = VBOX_HGCM_SVC_PARM_PTR;
            parm.u.pointer.addr = pLed;
            parm.u.pointer.size = sizeof(*pLed);

            rc = HGCMHostCall("VBoxSharedFolders", SHFL_FN_SET_STATUS_LED, 1, &parm);
        }
        else
            AssertMsgFailed(("pfnQueryStatusLed failed with %Vrc (pLed=%x)\n", rc, pLed));
    }
    else
    {
        LogRel(("Failed to load Shared Folders service %Vrc\n", rc));
    }

    bool fEnabled;

    /* Check CFGM option. */
    rc = CFGMR3QueryBool(pCfgHandle, "OpenGLEnabled", &fEnabled);
    if (    VBOX_SUCCESS(rc)
        &&  fEnabled)
    {
        rc = pData->pVMMDev->hgcmLoadService ("VBoxSharedOpenGL", "VBoxSharedOpenGL");
        if (VBOX_SUCCESS(rc))
        {
            LogRel(("Shared OpenGL service loaded.\n"));
        }
        else
        {
            LogRel(("Failed to load Shared OpenGL service %Vrc\n", rc));
        }
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
