/* $Id $ */
/** @file
 * VBoxPci - PCI card passthrough support (Host), Common Code.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/** @page pg_rawpci     VBoxPci - host PCI support
 *
 * This is a kernel module that works as host proxy between guest and
 * PCI hardware.
 *
 */

#define LOG_GROUP LOG_GROUP_DEV_PCI_RAW
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/sup.h>
#include <VBox/version.h>

#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/spinlock.h>
#include <iprt/uuid.h>
#include <iprt/asm.h>
#include <iprt/mem.h>

#include "VBoxPciInternal.h"


#define DEVPORT_2_VBOXRAWPCIINS(pPort) \
    ( (PVBOXRAWPCIINS)((uint8_t *)pPort - RT_OFFSETOF(VBOXRAWPCIINS, DevPort)) )


/**
 * Implements the SUPDRV component factor interface query method.
 *
 * @returns Pointer to an interface. NULL if not supported.
 *
 * @param   pSupDrvFactory      Pointer to the component factory registration structure.
 * @param   pSession            The session - unused.
 * @param   pszInterfaceUuid    The factory interface id.
 */
static DECLCALLBACK(void *) vboxPciQueryFactoryInterface(PCSUPDRVFACTORY pSupDrvFactory, PSUPDRVSESSION pSession, const char *pszInterfaceUuid)
{
    PVBOXRAWPCIGLOBALS pGlobals = (PVBOXRAWPCIGLOBALS)((uint8_t *)pSupDrvFactory - RT_OFFSETOF(VBOXRAWPCIGLOBALS, SupDrvFactory));

    /*
     * Convert the UUID strings and compare them.
     */
    RTUUID UuidReq;
    int rc = RTUuidFromStr(&UuidReq, pszInterfaceUuid);
    if (RT_SUCCESS(rc))
    {
        if (!RTUuidCompareStr(&UuidReq, RAWPCIFACTORY_UUID_STR))
        {
            ASMAtomicIncS32(&pGlobals->cFactoryRefs);
            return &pGlobals->RawPciFactory;
        }
    }
    else
        Log(("VBoxRawPci: rc=%Rrc, uuid=%s\n", rc, pszInterfaceUuid));

    return NULL;
}

DECLINLINE(int) vboxPciDevLock(PVBOXRAWPCIINS pThis)
{
    int rc = RTSemFastMutexRequest(pThis->hFastMtx);
    AssertRC(rc);
    return rc;
}

DECLINLINE(void) vboxPciDevUnlock(PVBOXRAWPCIINS pThis)
{
    RTSemFastMutexRelease(pThis->hFastMtx);
}

DECLINLINE(int) vboxPciGlobalsLock(PVBOXRAWPCIGLOBALS pGlobals)
{
    int rc = RTSemFastMutexRequest(pGlobals->hFastMtx);
    AssertRC(rc);
    return rc;
}

DECLINLINE(void) vboxPciGlobalsUnlock(PVBOXRAWPCIGLOBALS pGlobals)
{
    RTSemFastMutexRelease(pGlobals->hFastMtx);
}

static PVBOXRAWPCIINS vboxPciFindInstanceLocked(PVBOXRAWPCIGLOBALS pGlobals, uint32_t iHostAddress)
{
    PVBOXRAWPCIINS pCur;
    for (pCur = pGlobals->pInstanceHead; pCur != NULL; pCur = pCur->pNext)
    {
        if (iHostAddress == pCur->HostPciAddress)
            return pCur;
    }
    return NULL;
}

static void vboxPciUnlinkInstanceLocked(PVBOXRAWPCIGLOBALS pGlobals, PVBOXRAWPCIINS pToUnlink)
{
    if (pGlobals->pInstanceHead == pToUnlink)
        pGlobals->pInstanceHead = pToUnlink->pNext;
    else
    {
        PVBOXRAWPCIINS pCur;
        for (pCur = pGlobals->pInstanceHead; pCur != NULL; pCur = pCur->pNext)
        {
            if (pCur->pNext == pToUnlink)
            {
                pCur->pNext = pToUnlink->pNext;
                break;
            }
        }
    }
    pToUnlink->pNext = NULL;
}


DECLHIDDEN(void) vboxPciDevCleanup(PVBOXRAWPCIINS pThis)
{
    pThis->DevPort.pfnDeinit(&pThis->DevPort, 0);

    if (pThis->hFastMtx)
    {
        RTSemFastMutexDestroy(pThis->hFastMtx);
        pThis->hFastMtx = NIL_RTSEMFASTMUTEX;
    }

    if (pThis->hSpinlock)
    {
        RTSpinlockDestroy(pThis->hSpinlock);
        pThis->hSpinlock = NIL_RTSPINLOCK;
    }

    vboxPciGlobalsLock(pThis->pGlobals);
    vboxPciUnlinkInstanceLocked(pThis->pGlobals, pThis);
    vboxPciGlobalsUnlock(pThis->pGlobals);
}


/**
 * @copydoc RAWPCIDEVPORT:: pfnInit
 */
DECLHIDDEN(int) vboxPciDevInit(PRAWPCIDEVPORT pPort, uint32_t fFlags)
{
    PVBOXRAWPCIINS pThis = DEVPORT_2_VBOXRAWPCIINS(pPort);
    int rc;

    vboxPciDevLock(pThis);

    rc = vboxPciOsDevInit(pThis, fFlags);

    vboxPciDevUnlock(pThis);

    return rc;
}

/**
 * @copydoc RAWPCIDEVPORT:: pfnDeinit
 */
DECLHIDDEN(int) vboxPciDevDeinit(PRAWPCIDEVPORT pPort, uint32_t fFlags)
{
    PVBOXRAWPCIINS pThis = DEVPORT_2_VBOXRAWPCIINS(pPort);
    int rc;

    /* Bit racy, better check under lock. */
    if (pThis->iHostIrq != -1)
    {
        pPort->pfnUnregisterIrqHandler(pPort, pThis->iHostIrq);
        pThis->iHostIrq = -1;
    }

    vboxPciDevLock(pThis);

    rc = vboxPciOsDevDeinit(pThis, fFlags);

    vboxPciDevUnlock(pThis);

    return rc;
}


/**
 * @copydoc RAWPCIDEVPORT:: pfnDestroy
 */
DECLHIDDEN(int) vboxPciDevDestroy(PRAWPCIDEVPORT pPort)
{
    PVBOXRAWPCIINS pThis = DEVPORT_2_VBOXRAWPCIINS(pPort);
    int rc;
    
    rc = vboxPciOsDevDestroy(pThis);
    if (rc == VINF_SUCCESS)
    {
        if (pThis->hFastMtx)
        {
            RTSemFastMutexDestroy(pThis->hFastMtx);
            pThis->hFastMtx = NIL_RTSEMFASTMUTEX;
        }
        
        if (pThis->hSpinlock)
        {
            RTSpinlockDestroy(pThis->hSpinlock);
            pThis->hSpinlock = NIL_RTSPINLOCK;
        }
        
        vboxPciGlobalsLock(pThis->pGlobals);
        vboxPciUnlinkInstanceLocked(pThis->pGlobals, pThis);
        vboxPciGlobalsUnlock(pThis->pGlobals);

        RTMemFree(pThis);
    }

    return rc;
}
/**
 * @copydoc RAWPCIDEVPORT:: pfnGetRegionInfo
 */
DECLHIDDEN(int) vboxPciDevGetRegionInfo(PRAWPCIDEVPORT pPort,
                                        int32_t        iRegion,
                                        RTHCPHYS       *pRegionStart,
                                        uint64_t       *pu64RegionSize,
                                        bool           *pfPresent,
                                        uint32_t        *pfFlags)
{
    PVBOXRAWPCIINS pThis = DEVPORT_2_VBOXRAWPCIINS(pPort);
    int rc;

    vboxPciDevLock(pThis);

    rc = vboxPciOsDevGetRegionInfo(pThis, iRegion,
                                   pRegionStart, pu64RegionSize,
                                   pfPresent, pfFlags);
    vboxPciDevUnlock(pThis);

    return rc;
}

/**
 * @copydoc RAWPCIDEVPORT:: pfnMapRegion
 */
DECLHIDDEN(int) vboxPciDevMapRegion(PRAWPCIDEVPORT pPort,
                                    int32_t        iRegion,
                                    RTHCPHYS       RegionStart,
                                    uint64_t       u64RegionSize,
                                    int32_t        fFlags,
                                    RTR0PTR        *pRegionBase)
{
    PVBOXRAWPCIINS pThis = DEVPORT_2_VBOXRAWPCIINS(pPort);
    int rc;

    vboxPciDevLock(pThis);

    rc = vboxPciOsDevMapRegion(pThis, iRegion, RegionStart, u64RegionSize, fFlags, pRegionBase);

    vboxPciDevUnlock(pThis);

    return rc;
}

/**
 * @copydoc RAWPCIDEVPORT:: pfnUnapRegion
 */
DECLHIDDEN(int) vboxPciDevUnmapRegion(PRAWPCIDEVPORT pPort,
                                      int32_t        iRegion,
                                      RTHCPHYS       RegionStart,
                                      uint64_t       u64RegionSize,
                                      RTR0PTR        RegionBase)
{
    PVBOXRAWPCIINS pThis = DEVPORT_2_VBOXRAWPCIINS(pPort);
    int rc;

    vboxPciDevLock(pThis);

    rc = vboxPciOsDevUnmapRegion(pThis, iRegion, RegionStart, u64RegionSize, RegionBase);

    vboxPciDevUnlock(pThis);

    return rc;
}

/**
 * @copydoc RAWPCIDEVPORT:: pfnPciCfgRead
 */
DECLHIDDEN(int) vboxPciDevPciCfgRead(PRAWPCIDEVPORT pPort, uint32_t Register, PCIRAWMEMLOC      *pValue)
{
    PVBOXRAWPCIINS pThis = DEVPORT_2_VBOXRAWPCIINS(pPort);

    int rc;

    vboxPciDevLock(pThis);

    rc = vboxPciOsDevPciCfgRead(pThis, Register, pValue);

    vboxPciDevUnlock(pThis);

    return rc;
}

/**
 * @copydoc RAWPCIDEVPORT:: pfnPciCfgWrite
 */
DECLHIDDEN(int) vboxPciDevPciCfgWrite(PRAWPCIDEVPORT pPort, uint32_t Register, PCIRAWMEMLOC *pValue)
{
    PVBOXRAWPCIINS pThis = DEVPORT_2_VBOXRAWPCIINS(pPort);
    int rc;

    vboxPciDevLock(pThis);

    rc = vboxPciOsDevPciCfgWrite(pThis, Register, pValue);

    vboxPciDevUnlock(pThis);

    return rc;
}

DECLHIDDEN(int) vboxPciDevRegisterIrqHandler(PRAWPCIDEVPORT pPort, PFNRAWPCIISR pfnHandler, void* pIrqContext, int32_t *piHostIrq)
{
    PVBOXRAWPCIINS pThis = DEVPORT_2_VBOXRAWPCIINS(pPort);
    int rc;

    vboxPciDevLock(pThis);

    pThis->pfnIrqHandler = pfnHandler;
    pThis->pIrqContext   = pIrqContext;
    rc = vboxPciOsDevRegisterIrqHandler(pThis, pfnHandler, pIrqContext, piHostIrq);
    if (RT_FAILURE(rc))
    {
        pThis->pfnIrqHandler = NULL;
        pThis->pIrqContext   = NULL;
        pThis->iHostIrq      = -1;
        *piHostIrq = -1;
    }
    else
        pThis->iHostIrq      = *piHostIrq;

    vboxPciDevUnlock(pThis);

    return rc;
}

DECLHIDDEN(int) vboxPciDevUnregisterIrqHandler(PRAWPCIDEVPORT pPort, int32_t iHostIrq)
{
    PVBOXRAWPCIINS pThis = DEVPORT_2_VBOXRAWPCIINS(pPort);
    int rc;

    vboxPciDevLock(pThis);

    Assert(iHostIrq == pThis->iHostIrq);
    rc = vboxPciOsDevUnregisterIrqHandler(pThis, iHostIrq);
    if (RT_SUCCESS(rc))
    {
        pThis->pfnIrqHandler = NULL;
        pThis->pIrqContext   = NULL;
        pThis->iHostIrq = -1;
    }
    vboxPciDevUnlock(pThis);

    return rc;
}

/**
 * Creates a new instance.
 *
 * @returns VBox status code.
 * @param   pGlobals            The globals.
 * @param   pszName             The instance name.
 * @param   ppDevPort           Where to store the pointer to our port interface.
 */
static int vboxPciNewInstance(PVBOXRAWPCIGLOBALS pGlobals,
                              uint32_t           u32HostAddress,
                              uint32_t           fFlags,
                              PRAWPCIDEVPORT     *ppDevPort)
{
    int             rc;
    PVBOXRAWPCIINS  pNew = (PVBOXRAWPCIINS)RTMemAllocZ(sizeof(*pNew));
    if (!pNew)
        return VERR_NO_MEMORY;

    pNew->pGlobals                      = pGlobals;
    pNew->hSpinlock                     = NIL_RTSPINLOCK;
    pNew->cRefs                         = 1;
    pNew->pNext                         = NULL;
    pNew->HostPciAddress                = u32HostAddress;
    pNew->iHostIrq                      = -1;

    pNew->DevPort.u32Version            = RAWPCIDEVPORT_VERSION;

    pNew->DevPort.pfnInit               = vboxPciDevInit;
    pNew->DevPort.pfnDeinit             = vboxPciDevDeinit;
    pNew->DevPort.pfnDestroy            = vboxPciDevDestroy;
    pNew->DevPort.pfnGetRegionInfo      = vboxPciDevGetRegionInfo;
    pNew->DevPort.pfnMapRegion          = vboxPciDevMapRegion;
    pNew->DevPort.pfnUnmapRegion        = vboxPciDevUnmapRegion;
    pNew->DevPort.pfnPciCfgRead         = vboxPciDevPciCfgRead;
    pNew->DevPort.pfnPciCfgWrite        = vboxPciDevPciCfgWrite;
    pNew->DevPort.pfnPciCfgRead         = vboxPciDevPciCfgRead;
    pNew->DevPort.pfnPciCfgWrite        = vboxPciDevPciCfgWrite;
    pNew->DevPort.pfnRegisterIrqHandler = vboxPciDevRegisterIrqHandler;
    pNew->DevPort.pfnUnregisterIrqHandler = vboxPciDevUnregisterIrqHandler;
    pNew->DevPort.u32VersionEnd         = RAWPCIDEVPORT_VERSION;

    rc = RTSpinlockCreate(&pNew->hSpinlock);

    if (RT_SUCCESS(rc))
    {
        rc = RTSemFastMutexCreate(&pNew->hFastMtx);
        if (RT_SUCCESS(rc))
        {
            rc = pNew->DevPort.pfnInit(&pNew->DevPort, fFlags);
            if (RT_SUCCESS(rc))
            {
                *ppDevPort = &pNew->DevPort;

                pNew->pNext = pGlobals->pInstanceHead;
                pGlobals->pInstanceHead = pNew;
            }
            else
            {
                RTSemFastMutexDestroy(pNew->hFastMtx);
                RTSpinlockDestroy(pNew->hSpinlock);
                RTMemFree(pNew);
            }
            return rc;
        }
    }

    return rc;
}

/**
 * @copydoc RAWPCIFACTORY::pfnCreateAndConnect
 */
static DECLCALLBACK(int) vboxPciFactoryCreateAndConnect(PRAWPCIFACTORY       pFactory,
                                                        uint32_t             u32HostAddress,
                                                        uint32_t             fFlags,
                                                        PRAWPCIDEVPORT       *ppDevPort)
{
    PVBOXRAWPCIGLOBALS pGlobals = (PVBOXRAWPCIGLOBALS)((uint8_t *)pFactory - RT_OFFSETOF(VBOXRAWPCIGLOBALS, RawPciFactory));
    int rc;

    LogFlow(("vboxPciFactoryCreateAndConnect: PCI=%x fFlags=%#x\n", u32HostAddress, fFlags));
    Assert(pGlobals->cFactoryRefs > 0);
    rc = vboxPciGlobalsLock(pGlobals);
    AssertRCReturn(rc, rc);

    /* First search if there's no existing instance with same host device
     * address - if so - we cannot continue.
     */
    if (vboxPciFindInstanceLocked(pGlobals, u32HostAddress) != NULL)
    {
        rc = VERR_RESOURCE_BUSY;
        goto unlock;
    }

    rc = vboxPciNewInstance(pGlobals, u32HostAddress, fFlags, ppDevPort);

unlock:
    vboxPciGlobalsUnlock(pGlobals);

    return rc;
}

/**
 * @copydoc RAWPCIFACTORY::pfnRelease
 */
static DECLCALLBACK(void) vboxPciFactoryRelease(PRAWPCIFACTORY pFactory)
{
    PVBOXRAWPCIGLOBALS pGlobals = (PVBOXRAWPCIGLOBALS)((uint8_t *)pFactory - RT_OFFSETOF(VBOXRAWPCIGLOBALS, RawPciFactory));

    int32_t cRefs = ASMAtomicDecS32(&pGlobals->cFactoryRefs);
    Assert(cRefs >= 0); NOREF(cRefs);
    LogFlow(("vboxPciFactoryRelease: cRefs=%d (new)\n", cRefs));
}

static DECLHIDDEN(bool) vboxPciCanUnload(PVBOXRAWPCIGLOBALS pGlobals)
{
    int rc = vboxPciGlobalsLock(pGlobals);
    bool fRc = !pGlobals->pInstanceHead
            && pGlobals->cFactoryRefs <= 0;
    vboxPciGlobalsUnlock(pGlobals);
    AssertRC(rc);
    return fRc;
}


static DECLHIDDEN(int) vboxPciInitIdc(PVBOXRAWPCIGLOBALS pGlobals)
{
    int rc;
    Assert(!pGlobals->fIDCOpen);

    /*
     * Establish a connection to SUPDRV and register our component factory.
     */
    rc = SUPR0IdcOpen(&pGlobals->SupDrvIDC, 0 /* iReqVersion = default */, 0 /* iMinVersion = default */, NULL, NULL, NULL);
    if (RT_SUCCESS(rc))
    {
        rc = SUPR0IdcComponentRegisterFactory(&pGlobals->SupDrvIDC, &pGlobals->SupDrvFactory);
        if (RT_SUCCESS(rc))
        {
            pGlobals->fIDCOpen = true;
            Log(("VBoxRawPci: pSession=%p\n", SUPR0IdcGetSession(&pGlobals->SupDrvIDC)));
            return rc;
        }

        /* bail out. */
        LogRel(("VBoxRawPci: Failed to register component factory, rc=%Rrc\n", rc));
        SUPR0IdcClose(&pGlobals->SupDrvIDC);
    }

    return rc;
}

/**
 * Try to close the IDC connection to SUPDRV if established.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_WRONG_ORDER if we're busy.
 *
 * @param   pGlobals        Pointer to the globals.
 */
DECLHIDDEN(int) vboxPciDeleteIdc(PVBOXRAWPCIGLOBALS pGlobals)
{
    int rc;

    Assert(pGlobals->hFastMtx != NIL_RTSEMFASTMUTEX);

    /*
     * Check before trying to deregister the factory.
     */
    if (!vboxPciCanUnload(pGlobals))
        return VERR_WRONG_ORDER;

    if (!pGlobals->fIDCOpen)
        rc = VINF_SUCCESS;
    else
    {
        /*
         * Disconnect from SUPDRV.
         */
        rc = SUPR0IdcComponentDeregisterFactory(&pGlobals->SupDrvIDC, &pGlobals->SupDrvFactory);
        AssertRC(rc);
        SUPR0IdcClose(&pGlobals->SupDrvIDC);
        pGlobals->fIDCOpen = false;
    }

    return rc;
}


/**
 * Initializes the globals.
 *
 * @returns VBox status code.
 * @param   pGlobals        Pointer to the globals.
 */
DECLHIDDEN(int) vboxPciInitGlobals(PVBOXRAWPCIGLOBALS pGlobals)
{
    /*
     * Initialize the common portions of the structure.
     */
    int rc = RTSemFastMutexCreate(&pGlobals->hFastMtx);
    if (RT_SUCCESS(rc))
    {
        pGlobals->pInstanceHead = NULL;
        pGlobals->RawPciFactory.pfnRelease = vboxPciFactoryRelease;
        pGlobals->RawPciFactory.pfnCreateAndConnect = vboxPciFactoryCreateAndConnect;
        memcpy(pGlobals->SupDrvFactory.szName, "VBoxRawPci", sizeof("VBoxRawPci"));
        pGlobals->SupDrvFactory.pfnQueryFactoryInterface = vboxPciQueryFactoryInterface;
        pGlobals->fIDCOpen = false;
    }
    return rc;
}


/**
 * Deletes the globals.
 *
 *
 * @param   pGlobals        Pointer to the globals.
 */
DECLHIDDEN(void) vboxPciDeleteGlobals(PVBOXRAWPCIGLOBALS pGlobals)
{
    Assert(!pGlobals->fIDCOpen);

    /*
     * Release resources.
     */
    if (pGlobals->hFastMtx)
    {
        RTSemFastMutexDestroy(pGlobals->hFastMtx);
        pGlobals->hFastMtx = NIL_RTSEMFASTMUTEX;
    }
}


int  vboxPciInit(PVBOXRAWPCIGLOBALS pGlobals)
{

    /*
     * Initialize the common portions of the structure.
     */
    int rc = vboxPciInitGlobals(pGlobals);
    if (RT_SUCCESS(rc))
    {
        rc = vboxPciInitIdc(pGlobals);
        if (RT_SUCCESS(rc))
            return rc;

        /* bail out. */
        vboxPciDeleteGlobals(pGlobals);
    }

    return rc;
}

void vboxPciShutdown(PVBOXRAWPCIGLOBALS pGlobals)
{
    int rc = vboxPciDeleteIdc(pGlobals);

    if (RT_SUCCESS(rc))
        vboxPciDeleteGlobals(pGlobals);
}
