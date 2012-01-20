/* $Id$ */
/** @file
 * VirtualBox Driver Interface to VMM device.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
#include "MouseImpl.h"

#include "Logging.h"

#include <VBox/vmm/pdmdrv.h>
#include <VBox/VMMDev.h>
#include <VBox/shflsvc.h>
#include <iprt/asm.h>

#ifdef VBOX_WITH_HGCM
# include "HGCM.h"
# include "HGCMObjects.h"
# if defined(RT_OS_DARWIN) && defined(VBOX_WITH_CROGL)
#  include <VBox/HostServices/VBoxCrOpenGLSvc.h>
# endif
#endif

//
// defines
//

#ifdef RT_OS_OS2
# define VBOXSHAREDFOLDERS_DLL "VBoxSFld"
#else
# define VBOXSHAREDFOLDERS_DLL "VBoxSharedFolders"
#endif

//
// globals
//


/**
 * VMMDev driver instance data.
 */
typedef struct DRVMAINVMMDEV
{
    /** Pointer to the VMMDev object. */
    VMMDev                      *pVMMDev;
    /** Pointer to the driver instance structure. */
    PPDMDRVINS                  pDrvIns;
    /** Pointer to the VMMDev port interface of the driver/device above us. */
    PPDMIVMMDEVPORT             pUpPort;
    /** Our VMM device connector interface. */
    PDMIVMMDEVCONNECTOR         Connector;

#ifdef VBOX_WITH_HGCM
    /** Pointer to the HGCM port interface of the driver/device above us. */
    PPDMIHGCMPORT               pHGCMPort;
    /** Our HGCM connector interface. */
    PDMIHGCMCONNECTOR           HGCMConnector;
#endif
} DRVMAINVMMDEV, *PDRVMAINVMMDEV;

/** Converts PDMIVMMDEVCONNECTOR pointer to a DRVMAINVMMDEV pointer. */
#define PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface) ( (PDRVMAINVMMDEV) ((uintptr_t)pInterface - RT_OFFSETOF(DRVMAINVMMDEV, Connector)) )

#ifdef VBOX_WITH_HGCM
/** Converts PDMIHGCMCONNECTOR pointer to a DRVMAINVMMDEV pointer. */
#define PDMIHGCMCONNECTOR_2_MAINVMMDEV(pInterface) ( (PDRVMAINVMMDEV) ((uintptr_t)pInterface - RT_OFFSETOF(DRVMAINVMMDEV, HGCMConnector)) )
#endif

//
// constructor / destructor
//
VMMDev::VMMDev(Console *console)
    : mpDrv(NULL),
      mParent(console)
{
    int rc = RTSemEventCreate(&mCredentialsEvent);
    AssertRC(rc);
#ifdef VBOX_WITH_HGCM
    rc = HGCMHostInit ();
    AssertRC(rc);
    m_fHGCMActive = true;
#endif /* VBOX_WITH_HGCM */
    mu32CredentialsFlags = 0;
}

VMMDev::~VMMDev()
{
#ifdef VBOX_WITH_HGCM
    if (hgcmIsActive())
    {
        ASMAtomicWriteBool(&m_fHGCMActive, false);
        HGCMHostShutdown();
    }
#endif /* VBOX_WITH_HGCM */
    RTSemEventDestroy (mCredentialsEvent);
    if (mpDrv)
        mpDrv->pVMMDev = NULL;
    mpDrv = NULL;
}

PPDMIVMMDEVPORT VMMDev::getVMMDevPort()
{
    AssertReturn(mpDrv, NULL);
    return mpDrv->pUpPort;
}



//
// public methods
//

/**
 * Wait on event semaphore for guest credential judgement result.
 */
int VMMDev::WaitCredentialsJudgement(uint32_t u32Timeout, uint32_t *pu32CredentialsFlags)
{
    if (u32Timeout == 0)
    {
        u32Timeout = 5000;
    }

    int rc = RTSemEventWait (mCredentialsEvent, u32Timeout);

    if (RT_SUCCESS(rc))
    {
        *pu32CredentialsFlags = mu32CredentialsFlags;
    }

    return rc;
}

int VMMDev::SetCredentialsJudgementResult(uint32_t u32Flags)
{
    mu32CredentialsFlags = u32Flags;

    int rc = RTSemEventSignal (mCredentialsEvent);
    AssertRC(rc);

    return rc;
}


/**
 * Reports Guest Additions status.
 * Called whenever the Additions issue a guest status report request or the VM is reset.
 *
 * @param   pInterface          Pointer to this interface.
 * @param   guestInfo           Pointer to guest information structure
 * @thread  The emulation thread.
 */
DECLCALLBACK(void) vmmdevUpdateGuestStatus(PPDMIVMMDEVCONNECTOR pInterface, const VBoxGuestStatus *guestStatus)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);

    Assert(guestStatus);
    if (!guestStatus)
        return;

    Console *pConsole = pDrv->pVMMDev->getParent();

    /* Store that information in IGuest */
    Guest* guest = pConsole->getGuest();
    Assert(guest);
    if (!guest)
        return;

    guest->setAdditionsStatus(guestStatus->facility, guestStatus->status, guestStatus->flags);
    pConsole->onAdditionsStateChange();
}


/**
 * Reports Guest Additions API and OS version.
 * Called whenever the Additions issue a guest version report request or the VM is reset.
 *
 * @param   pInterface          Pointer to this interface.
 * @param   guestInfo           Pointer to guest information structure.
 * @thread  The emulation thread.
 */
DECLCALLBACK(void) vmmdevUpdateGuestInfo(PPDMIVMMDEVCONNECTOR pInterface, const VBoxGuestInfo *guestInfo)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);

    Assert(guestInfo);
    if (!guestInfo)
        return;

    Console *pConsole = pDrv->pVMMDev->getParent();

    /* Store that information in IGuest */
    Guest* guest = pConsole->getGuest();
    Assert(guest);
    if (!guest)
        return;

    if (guestInfo->interfaceVersion != 0)
    {
        char version[16];
        RTStrPrintf(version, sizeof(version), "%d", guestInfo->interfaceVersion);
        guest->setAdditionsInfo(Bstr(version), guestInfo->osType);

        /*
         * Tell the console interface about the event
         * so that it can notify its consumers.
         */
        pConsole->onAdditionsStateChange();

        if (guestInfo->interfaceVersion < VMMDEV_VERSION)
            pConsole->onAdditionsOutdated();
    }
    else
    {
        /*
         * The guest additions was disabled because of a reset
         * or driver unload.
         */
        guest->setAdditionsInfo(Bstr(), guestInfo->osType); /* Clear interface version + OS type. */
        guest->setAdditionsInfo2("", "", 0); /* Clear Guest Additions version. */
        guest->setAdditionsStatus(VBoxGuestFacilityType_All,
                                  VBoxGuestFacilityStatus_Inactive,
                                  0); /* Flags; not used. */
        pConsole->onAdditionsStateChange();
    }
}

/**
 * Reports the detailed Guest Additions version.
 * Called whenever the Additions issue a guest version report request or the VM is reset.
 *
 * @param   pInterface          Pointer to this interface.
 * @param   pGuestInfo          Pointer to Guest Additions information
 *                              structure.
 * @thread  The emulation thread.
 */
DECLCALLBACK(void) vmmdevUpdateGuestInfo2(PPDMIVMMDEVCONNECTOR pInterface, const VBoxGuestInfo2 *pGuestInfo)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);
    AssertPtr(pGuestInfo);

    /* Store that information in IGuest. */
    Guest *pGuest = pDrv->pVMMDev->getParent()->getGuest();
    Assert(pGuest);
    if (!pGuest)
        return;

    if (   pGuestInfo->additionsMajor    != 0
        && pGuestInfo->additionsRevision != 0)
    {
        /** @todo r=bird: See comments on space before 'r' in setAdditionsInfo2! */
        char szVersion[32];
        RTStrPrintf(szVersion, sizeof(szVersion), "%d.%d.%dr%ld",
                    pGuestInfo->additionsMajor,
                    pGuestInfo->additionsMinor,
                    pGuestInfo->additionsBuild,
                    pGuestInfo->additionsRevision);

        pGuest->setAdditionsInfo2(szVersion, pGuestInfo->szName, pGuestInfo->additionsRevision);

        /*
         * No need to tell the console interface about the update;
         * vmmdevUpdateGuestInfo takes care of that when called as the
         * last event in the chain.
         */
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
    AssertPtr(pDrv);
    Console *pConsole = pDrv->pVMMDev->getParent();

    /* store that information in IGuest */
    Guest* pGuest = pConsole->getGuest();
    AssertPtr(pGuest);
    if (!pGuest)
        return;

    /*
     * Report our current capabilities (and assume none is active yet).
     */
    pGuest->setSupportedFeatures(newCapabilities);

    /*
     * Tell the console interface about the event
     * so that it can notify its consumers.
     */
    pConsole->onAdditionsStateChange();
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
DECLCALLBACK(void) vmmdevUpdateMouseCapabilities(PPDMIVMMDEVCONNECTOR pInterface, uint32_t fNewCaps)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);
    Console *pConsole = pDrv->pVMMDev->getParent();

    /*
     * Tell the console interface about the event
     * so that it can notify its consumers.
     */
    Mouse *pMouse = pConsole->getMouse();
    if (pMouse)  /** @todo and if not?  Can that actually happen? */
        pMouse->onVMMDevGuestCapsChange(fNewCaps & VMMDEV_MOUSE_GUEST_MASK);
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
    Console *pConsole = pDrv->pVMMDev->getParent();

    /* tell the console about it */
    size_t cbShapeSize = 0;

    if (pShape)
    {
        cbShapeSize = (width + 7) / 8 * height; /* size of the AND mask */
        cbShapeSize = ((cbShapeSize + 3) & ~3) + width * 4 * height; /* + gap + size of the XOR mask */
    }
    com::SafeArray<BYTE> shapeData(cbShapeSize);
    if (pShape)
        ::memcpy(shapeData.raw(), pShape, cbShapeSize);
    pConsole->onMousePointerShapeChange(fVisible, fAlpha, xHot, yHot, width, height, ComSafeArrayAsInParam(shapeData));
}

DECLCALLBACK(int) iface_VideoAccelEnable(PPDMIVMMDEVCONNECTOR pInterface, bool fEnable, VBVAMEMORY *pVbvaMemory)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);
    Console *pConsole = pDrv->pVMMDev->getParent();

    Display *display = pConsole->getDisplay();

    if (display)
    {
        LogSunlover(("MAIN::VMMDevInterface::iface_VideoAccelEnable: %d, %p\n", fEnable, pVbvaMemory));
        return display->VideoAccelEnable(fEnable, pVbvaMemory);
    }

    return VERR_NOT_SUPPORTED;
}
DECLCALLBACK(void) iface_VideoAccelFlush(PPDMIVMMDEVCONNECTOR pInterface)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);
    Console *pConsole = pDrv->pVMMDev->getParent();

    Display *display = pConsole->getDisplay();

    if (display)
    {
        LogSunlover(("MAIN::VMMDevInterface::iface_VideoAccelFlush\n"));
        display->VideoAccelFlush ();
    }
}

DECLCALLBACK(int) vmmdevVideoModeSupported(PPDMIVMMDEVCONNECTOR pInterface, uint32_t display, uint32_t width, uint32_t height,
                                           uint32_t bpp, bool *fSupported)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);
    Console *pConsole = pDrv->pVMMDev->getParent();

    if (!fSupported)
        return VERR_INVALID_PARAMETER;
#ifdef DEBUG_sunlover
    Log(("vmmdevVideoModeSupported: [%d]: %dx%dx%d\n", display, width, height, bpp));
#endif
    IFramebuffer *framebuffer = NULL;
    LONG xOrigin = 0;
    LONG yOrigin = 0;
    HRESULT hrc = pConsole->getDisplay()->GetFramebuffer(display, &framebuffer, &xOrigin, &yOrigin);
    if (SUCCEEDED(hrc) && framebuffer)
    {
        framebuffer->VideoModeSupported(width, height, bpp, (BOOL*)fSupported);
        framebuffer->Release();
    }
    else
    {
#ifdef DEBUG_sunlover
        Log(("vmmdevVideoModeSupported: hrc %x, framebuffer %p!!!\n", hrc, framebuffer));
#endif
        *fSupported = true;
    }
    return VINF_SUCCESS;
}

DECLCALLBACK(int) vmmdevGetHeightReduction(PPDMIVMMDEVCONNECTOR pInterface, uint32_t *heightReduction)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);
    Console *pConsole = pDrv->pVMMDev->getParent();

    if (!heightReduction)
        return VERR_INVALID_PARAMETER;
    IFramebuffer *framebuffer = pConsole->getDisplay()->getFramebuffer();
    if (framebuffer)
        framebuffer->COMGETTER(HeightReduction)((ULONG*)heightReduction);
    else
        *heightReduction = 0;
    return VINF_SUCCESS;
}

DECLCALLBACK(int) vmmdevSetCredentialsJudgementResult(PPDMIVMMDEVCONNECTOR pInterface, uint32_t u32Flags)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);

    if (pDrv->pVMMDev)
        return pDrv->pVMMDev->SetCredentialsJudgementResult (u32Flags);

    return VERR_GENERAL_FAILURE;
}

DECLCALLBACK(int) vmmdevSetVisibleRegion(PPDMIVMMDEVCONNECTOR pInterface, uint32_t cRect, PRTRECT pRect)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);
    Console *pConsole = pDrv->pVMMDev->getParent();

    /* Forward to Display, which calls corresponding framebuffers. */
    pConsole->getDisplay()->handleSetVisibleRegion(cRect, pRect);

    return VINF_SUCCESS;
}

DECLCALLBACK(int) vmmdevQueryVisibleRegion(PPDMIVMMDEVCONNECTOR pInterface, uint32_t *pcRect, PRTRECT pRect)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);
    Console *pConsole = pDrv->pVMMDev->getParent();

    /* Forward to Display, which calls corresponding framebuffers. */
    pConsole->getDisplay()->handleQueryVisibleRegion(pcRect, pRect);

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
    Console *pConsole = pDrv->pVMMDev->getParent();
    ULONG          val = 0;

    if (!pulInterval)
        return VERR_INVALID_POINTER;

    /* store that information in IGuest */
    Guest* guest = pConsole->getGuest();
    Assert(guest);
    if (!guest)
        return VERR_GENERAL_FAILURE;

    guest->COMGETTER(StatisticsUpdateInterval)(&val);
    *pulInterval = val;
    return VINF_SUCCESS;
}

/**
 * Query the current balloon size
 *
 * @returns VBox status code.
 * @param   pInterface          Pointer to this interface.
 * @param   pcbBalloon          Balloon size
 * @thread  The emulation thread.
 */
DECLCALLBACK(int) vmmdevQueryBalloonSize(PPDMIVMMDEVCONNECTOR pInterface, uint32_t *pcbBalloon)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);
    Console *pConsole = pDrv->pVMMDev->getParent();
    ULONG          val = 0;

    if (!pcbBalloon)
        return VERR_INVALID_POINTER;

    /* store that information in IGuest */
    Guest* guest = pConsole->getGuest();
    Assert(guest);
    if (!guest)
        return VERR_GENERAL_FAILURE;

    guest->COMGETTER(MemoryBalloonSize)(&val);
    *pcbBalloon = val;
    return VINF_SUCCESS;
}

/**
 * Query the current page fusion setting
 *
 * @returns VBox status code.
 * @param   pInterface          Pointer to this interface.
 * @param   pfPageFusionEnabled Pointer to boolean
 * @thread  The emulation thread.
 */
DECLCALLBACK(int) vmmdevIsPageFusionEnabled(PPDMIVMMDEVCONNECTOR pInterface, bool *pfPageFusionEnabled)
{
    PDRVMAINVMMDEV pDrv = PDMIVMMDEVCONNECTOR_2_MAINVMMDEV(pInterface);
    Console *pConsole = pDrv->pVMMDev->getParent();
    BOOL           val = 0;

    if (!pfPageFusionEnabled)
        return VERR_INVALID_POINTER;

    /* store that information in IGuest */
    Guest* guest = pConsole->getGuest();
    Assert(guest);
    if (!guest)
        return VERR_GENERAL_FAILURE;

    *pfPageFusionEnabled = !!guest->isPageFusionEnabled();
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
    Console *pConsole = pDrv->pVMMDev->getParent();

    Assert(pGuestStats);
    if (!pGuestStats)
        return VERR_INVALID_POINTER;

    /* store that information in IGuest */
    Guest* guest = pConsole->getGuest();
    Assert(guest);
    if (!guest)
        return VERR_GENERAL_FAILURE;

    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_CPU_LOAD_IDLE)
        guest->setStatistic(pGuestStats->u32CpuId, GUESTSTATTYPE_CPUIDLE, pGuestStats->u32CpuLoad_Idle);

    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_CPU_LOAD_KERNEL)
        guest->setStatistic(pGuestStats->u32CpuId, GUESTSTATTYPE_CPUKERNEL, pGuestStats->u32CpuLoad_Kernel);

    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_CPU_LOAD_USER)
        guest->setStatistic(pGuestStats->u32CpuId, GUESTSTATTYPE_CPUUSER, pGuestStats->u32CpuLoad_User);


    /** @todo r=bird: Convert from 4KB to 1KB units?
     *        CollectorGuestHAL::getGuestMemLoad says it returns KB units to
     *        preCollect().  I might be wrong ofc, this is convoluted code... */
    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_PHYS_MEM_TOTAL)
        guest->setStatistic(pGuestStats->u32CpuId, GUESTSTATTYPE_MEMTOTAL, pGuestStats->u32PhysMemTotal);

    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_PHYS_MEM_AVAIL)
        guest->setStatistic(pGuestStats->u32CpuId, GUESTSTATTYPE_MEMFREE, pGuestStats->u32PhysMemAvail);

    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_PHYS_MEM_BALLOON)
        guest->setStatistic(pGuestStats->u32CpuId, GUESTSTATTYPE_MEMBALLOON, pGuestStats->u32PhysMemBalloon);

    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_MEM_SYSTEM_CACHE)
        guest->setStatistic(pGuestStats->u32CpuId, GUESTSTATTYPE_MEMCACHE, pGuestStats->u32MemSystemCache);

    if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_PAGE_FILE_SIZE)
        guest->setStatistic(pGuestStats->u32CpuId, GUESTSTATTYPE_PAGETOTAL, pGuestStats->u32PageFileSize);

    return VINF_SUCCESS;
}

#ifdef VBOX_WITH_HGCM

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

    if (!pDrv->pVMMDev || !pDrv->pVMMDev->hgcmIsActive())
        return VERR_INVALID_STATE;

    return HGCMGuestConnect(pDrv->pHGCMPort, pCmd, pServiceLocation->u.host.achName, pu32ClientID);
}

static DECLCALLBACK(int) iface_hgcmDisconnect (PPDMIHGCMCONNECTOR pInterface, PVBOXHGCMCMD pCmd, uint32_t u32ClientID)
{
    LogSunlover(("Enter\n"));

    PDRVMAINVMMDEV pDrv = PDMIHGCMCONNECTOR_2_MAINVMMDEV(pInterface);

    if (!pDrv->pVMMDev || !pDrv->pVMMDev->hgcmIsActive())
        return VERR_INVALID_STATE;

    return HGCMGuestDisconnect(pDrv->pHGCMPort, pCmd, u32ClientID);
}

static DECLCALLBACK(int) iface_hgcmCall (PPDMIHGCMCONNECTOR pInterface, PVBOXHGCMCMD pCmd, uint32_t u32ClientID, uint32_t u32Function,
                                         uint32_t cParms, PVBOXHGCMSVCPARM paParms)
{
    LogSunlover(("Enter\n"));

    PDRVMAINVMMDEV pDrv = PDMIHGCMCONNECTOR_2_MAINVMMDEV(pInterface);

    if (!pDrv->pVMMDev || !pDrv->pVMMDev->hgcmIsActive())
        return VERR_INVALID_STATE;

    return HGCMGuestCall(pDrv->pHGCMPort, pCmd, u32ClientID, u32Function, cParms, paParms);
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
    return HGCMHostSaveState(pSSM);
}


/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pDrvIns         Driver instance of the driver which registered the data unit.
 * @param   pSSM            SSM operation handle.
 * @param   uVersion        Data layout version.
 * @param   uPass           The data pass.
 */
static DECLCALLBACK(int) iface_hgcmLoad(PPDMDRVINS pDrvIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    LogFlowFunc(("Enter\n"));

    if (uVersion != HGCM_SSM_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    return HGCMHostLoadState(pSSM);
}

int VMMDev::hgcmLoadService(const char *pszServiceLibrary, const char *pszServiceName)
{
    if (!hgcmIsActive())
        return VERR_INVALID_STATE;

    return HGCMHostLoad(pszServiceLibrary, pszServiceName);
}

int VMMDev::hgcmHostCall(const char *pszServiceName, uint32_t u32Function,
                         uint32_t cParms, PVBOXHGCMSVCPARM paParms)
{
    if (!hgcmIsActive())
        return VERR_INVALID_STATE;
    return HGCMHostCall (pszServiceName, u32Function, cParms, paParms);
}

void VMMDev::hgcmShutdown(void)
{
    ASMAtomicWriteBool(&m_fHGCMActive, false);
    HGCMHostShutdown();
}

# ifdef VBOX_WITH_CRHGSMI
int VMMDev::hgcmHostSvcHandleCreate (const char *pszServiceName, HGCMCVSHANDLE * phSvc)
{
    if (!hgcmIsActive())
        return VERR_INVALID_STATE;
    return HGCMHostSvcHandleCreate(pszServiceName, phSvc);
}

int VMMDev::hgcmHostSvcHandleDestroy (HGCMCVSHANDLE hSvc)
{
    if (!hgcmIsActive())
        return VERR_INVALID_STATE;
    return HGCMHostSvcHandleDestroy(hSvc);
}

int VMMDev::hgcmHostFastCallAsync (HGCMCVSHANDLE hSvc, uint32_t function, PVBOXHGCMSVCPARM pParm, PHGCMHOSTFASTCALLCB pfnCompletion, void *pvCompletion)
{
    if (!hgcmIsActive())
        return VERR_INVALID_STATE;
    return HGCMHostFastCallAsync(hSvc, function, pParm, pfnCompletion, pvCompletion);
}
# endif

#endif /* HGCM */


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
DECLCALLBACK(void *) VMMDev::drvQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS      pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVMAINVMMDEV  pDrv    = PDMINS_2_DATA(pDrvIns, PDRVMAINVMMDEV);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIVMMDEVCONNECTOR, &pDrv->Connector);
#ifdef VBOX_WITH_HGCM
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHGCMCONNECTOR, &pDrv->HGCMConnector);
#endif
    return NULL;
}

/**
 * Destruct a VMMDev driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 */
DECLCALLBACK(void) VMMDev::drvDestruct(PPDMDRVINS pDrvIns)
{
    PDRVMAINVMMDEV pData = PDMINS_2_DATA(pDrvIns, PDRVMAINVMMDEV);
    LogFlow(("VMMDev::drvDestruct: iInstance=%d\n", pDrvIns->iInstance));
#ifdef VBOX_WITH_HGCM
    /* HGCM is shut down on the VMMDev destructor. */
#endif /* VBOX_WITH_HGCM */
    if (pData->pVMMDev)
        pData->pVMMDev->mpDrv = NULL;
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
#ifdef VBOX_WITH_HGCM
    HGCMHostReset ();
#endif /* VBOX_WITH_HGCM */
}

/**
 * Construct a VMMDev driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
DECLCALLBACK(int) VMMDev::drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle, uint32_t fFlags)
{
    PDRVMAINVMMDEV pData = PDMINS_2_DATA(pDrvIns, PDRVMAINVMMDEV);
    LogFlow(("Keyboard::drvConstruct: iInstance=%d\n", pDrvIns->iInstance));

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
    pDrvIns->IBase.pfnQueryInterface                  = VMMDev::drvQueryInterface;

    pData->Connector.pfnUpdateGuestStatus             = vmmdevUpdateGuestStatus;
    pData->Connector.pfnUpdateGuestInfo               = vmmdevUpdateGuestInfo;
    pData->Connector.pfnUpdateGuestInfo2              = vmmdevUpdateGuestInfo2;
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
    pData->Connector.pfnQueryBalloonSize              = vmmdevQueryBalloonSize;
    pData->Connector.pfnIsPageFusionEnabled           = vmmdevIsPageFusionEnabled;

#ifdef VBOX_WITH_HGCM
    pData->HGCMConnector.pfnConnect                   = iface_hgcmConnect;
    pData->HGCMConnector.pfnDisconnect                = iface_hgcmDisconnect;
    pData->HGCMConnector.pfnCall                      = iface_hgcmCall;
#endif

    /*
     * Get the IVMMDevPort interface of the above driver/device.
     */
    pData->pUpPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIVMMDEVPORT);
    AssertMsgReturn(pData->pUpPort, ("Configuration error: No VMMDev port interface above!\n"), VERR_PDM_MISSING_INTERFACE_ABOVE);

#ifdef VBOX_WITH_HGCM
    pData->pHGCMPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIHGCMPORT);
    AssertMsgReturn(pData->pHGCMPort, ("Configuration error: No HGCM port interface above!\n"), VERR_PDM_MISSING_INTERFACE_ABOVE);
#endif

    /*
     * Get the Console object pointer and update the mpDrv member.
     */
    void *pv;
    int rc = CFGMR3QueryPtr(pCfgHandle, "Object", &pv);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: No/bad \"Object\" value! rc=%Rrc\n", rc));
        return rc;
    }

    pData->pVMMDev = (VMMDev*)pv;        /** @todo Check this cast! */
    pData->pVMMDev->mpDrv = pData;

#ifdef VBOX_WITH_HGCM
    rc = pData->pVMMDev->hgcmLoadService(VBOXSHAREDFOLDERS_DLL,
                                         "VBoxSharedFolders");
    pData->pVMMDev->fSharedFolderActive = RT_SUCCESS(rc);
    if (RT_SUCCESS(rc))
    {
        PPDMLED       pLed;
        PPDMILEDPORTS pLedPort;

        LogRel(("Shared Folders service loaded.\n"));
        pLedPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMILEDPORTS);
        AssertMsgReturn(pLedPort, ("Configuration error: No LED port interface above!\n"), VERR_PDM_MISSING_INTERFACE_ABOVE);
        rc = pLedPort->pfnQueryStatusLed(pLedPort, 0, &pLed);
        if (RT_SUCCESS(rc) && pLed)
        {
            VBOXHGCMSVCPARM  parm;

            parm.type = VBOX_HGCM_SVC_PARM_PTR;
            parm.u.pointer.addr = pLed;
            parm.u.pointer.size = sizeof(*pLed);

            rc = HGCMHostCall("VBoxSharedFolders", SHFL_FN_SET_STATUS_LED, 1, &parm);
        }
        else
            AssertMsgFailed(("pfnQueryStatusLed failed with %Rrc (pLed=%x)\n", rc, pLed));
    }
    else
        LogRel(("Failed to load Shared Folders service %Rrc\n", rc));

    rc = PDMDrvHlpSSMRegisterEx(pDrvIns, HGCM_SSM_VERSION, 4096 /* bad guess */,
                                NULL, NULL, NULL,
                                NULL, iface_hgcmSave, NULL,
                                NULL, iface_hgcmLoad, NULL);
    if (RT_FAILURE(rc))
        return rc;

#endif /* VBOX_WITH_HGCM */

    return VINF_SUCCESS;
}


/**
 * VMMDevice driver registration record.
 */
const PDMDRVREG VMMDev::DrvReg =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "HGCM",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
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
    /* pfnRelocate */
    NULL,
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
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
