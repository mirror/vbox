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

/**
 * @copydoc RAWPCIDEVPORT:: pfnRetain
 */
DECLHIDDEN(void) vboxPciDevRetain(PRAWPCIDEVPORT pThis)
{
}

/**
 * @copydoc RAWPCIDEVPORT:: pfnRelease
 */
DECLHIDDEN(void) vboxPciDevRelease(PRAWPCIDEVPORT pThis)
{
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

    pNew->DevPort.u32Version            = RAWPCIDEVPORT_VERSION;
    pNew->DevPort.pfnRetain             = vboxPciDevRetain;
    pNew->DevPort.pfnRelease            = vboxPciDevRelease;
    pNew->DevPort.u32VersionEnd         = RAWPCIDEVPORT_VERSION;
    
    rc = RTSpinlockCreate(&pNew->hSpinlock);
    if (RT_SUCCESS(rc))
    {
        *ppDevPort = &pNew->DevPort;
        return rc;
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
    rc = RTSemFastMutexRequest(pGlobals->hFastMtx);
    AssertRCReturn(rc, rc);

    rc = vboxPciNewInstance(pGlobals, u32HostAddress, fFlags, ppDevPort);

    RTSemFastMutexRelease(pGlobals->hFastMtx);

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
    int rc = RTSemFastMutexRequest(pGlobals->hFastMtx);
    bool fRc = !pGlobals->pInstanceHead
            && pGlobals->cFactoryRefs <= 0;
    RTSemFastMutexRelease(pGlobals->hFastMtx);
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
