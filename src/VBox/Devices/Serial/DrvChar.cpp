/** @file
 *
 * VBox stream I/O devices:
 * Generic char driver
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



/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_CHAR
#include <VBox/pdm.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/stream.h>

#include "Builtins.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * Char driver instance data.
 */
typedef struct DRVCHAR
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS                  pDrvIns;
    /** Pointer to the char port interface of the driver/device above us. */
    PPDMICHARPORT               pDrvCharPort;
    /** Pointer to the stream interface of the driver below us. */
    PPDMISTREAM                 pDrvStream;
    /** Our char interface. */
    PDMICHAR                    IChar;
    /** Flag to notify the receive thread it should terminate. */
    volatile bool               fShutdown;
    /** Receive thread ID. */
    RTTHREAD                    ReceiveThread;
} DRVCHAR, *PDRVCHAR;


/** Converts a pointer to DRVCHAR::IChar to a PDRVCHAR. */
#define PDMICHAR_2_DRVCHAR(pInterface) ( (PDRVCHAR)((uintptr_t)pInterface - RT_OFFSETOF(DRVCHAR, IChar)) )


/* -=-=-=-=- IBase -=-=-=-=- */

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 */
static DECLCALLBACK(void *) drvCharQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS  pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVCHAR    pData = PDMINS2DATA(pDrvIns, PDRVCHAR);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_CHAR:
            return &pData->IChar;
        default:
            return NULL;
    }
}


/* -=-=-=-=- IChar -=-=-=-=- */

/** @copydoc PDMICHAR::pfnWrite */
static DECLCALLBACK(int) drvCharWrite(PPDMICHAR pInterface, const void *pvBuf, size_t cbWrite)
{
    PDRVCHAR pData = PDMICHAR_2_DRVCHAR(pInterface);
    int rc = VINF_SUCCESS;

    LogFlow(("%s: pvBuf=%#p cbWrite=%d\n", __FUNCTION__, pvBuf, cbWrite));

    /*
     * Write the character to the attached stream (if present).
     */
    if (pData->pDrvStream)
    {
        const char *pBuffer = (const char *)pvBuf;
        size_t cbProcessed = cbWrite;

        while (cbWrite)
        {
            rc = pData->pDrvStream->pfnWrite(pData->pDrvStream, pBuffer, &cbProcessed);
            if (VBOX_SUCCESS(rc))
            {
                Assert(cbProcessed);
                cbWrite -= cbProcessed;
                pBuffer += cbProcessed;
            }
            else if (rc == VERR_TIMEOUT)
            {
                /* Normal case, just means that the stream didn't accept a new
                 * character before the timeout elapsed. Just retry. */
                rc = VINF_SUCCESS;
            }
            else
                AssertRC(rc);
        }
    }
    else
        rc = VERR_PDM_NO_ATTACHED_DRIVER;

    LogFlow(("%s: returns rc=%Vrc\n", __FUNCTION__, rc));
    return rc;
}


/* -=-=-=-=- receive thread -=-=-=-=- */

/**
 * Receive thread loop.
 *
 * @returns 0 on success.
 * @param   ThreadSelf  Thread handle to this thread.
 * @param   pvUser      User argument.
 */
static DECLCALLBACK(int) drvCharReceiveLoop(RTTHREAD ThreadSelf, void *pvUser)
{
    PDRVCHAR pData = (PDRVCHAR)pvUser;
    char aBuffer[256], *pBuffer;
    size_t cbRemaining, cbProcessed;
    int rc;

    cbRemaining = 0;
    pBuffer = aBuffer;
    while (!pData->fShutdown)
    {
        if (!cbRemaining)
        {
            /* Get block of data from stream driver. */
            if (pData->pDrvStream)
            {
                cbRemaining = sizeof(aBuffer);
                rc = pData->pDrvStream->pfnRead(pData->pDrvStream, aBuffer, &cbRemaining);
                AssertRC(rc);
            }
            else
            {
                cbRemaining = 0;
                RTThreadSleep(100);
            }
            pBuffer = aBuffer;
        }
        else
        {
            /* Send data to guest. */
            cbProcessed = cbRemaining;
            rc = pData->pDrvCharPort->pfnNotifyRead(pData->pDrvCharPort, pBuffer, &cbProcessed);
            if (VBOX_SUCCESS(rc))
            {
                Assert(cbProcessed);
                pBuffer += cbProcessed;
                cbRemaining -= cbProcessed;
            }
            else if (rc == VERR_TIMEOUT)
            {
                /* Normal case, just means that the guest didn't accept a new
                 * character before the timeout elapsed. Just retry. */
                rc = VINF_SUCCESS;
            }
            else
                AssertRC(rc);
        }
    }

    pData->ReceiveThread = NIL_RTTHREAD;

    return VINF_SUCCESS;
}


/* -=-=-=-=- driver interface -=-=-=-=- */

/**
 * Construct a char driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed,
 *                      pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to
 *                      obtain the configuration of the driver instance. It's
 *                      also found in pDrvIns->pCfgHandle as it's expected to
 *                      be used frequently in this function.
 */
static DECLCALLBACK(int) drvCharConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVCHAR pData = PDMINS2DATA(pDrvIns, PDRVCHAR);
    LogFlow(("%s: iInstance=%d\n", __FUNCTION__, pDrvIns->iInstance));

    /*
     * Init basic data members and interfaces.
     */
    pData->ReceiveThread                    = NIL_RTTHREAD;
    pData->fShutdown                        = false;
    /* IBase. */
    pDrvIns->IBase.pfnQueryInterface        = drvCharQueryInterface;
    /* IChar. */
    pData->IChar.pfnWrite                   = drvCharWrite;


    /*
     * Get the ICharPort interface of the above driver/device.
     */
    pData->pDrvCharPort = (PPDMICHARPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_CHAR_PORT);
    if (!pData->pDrvCharPort)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE, RT_SRC_POS, N_("Char#%d has no char port interface above"), pDrvIns->iInstance);

    /*
     * Attach driver below and query its stream interface.
     */
    PPDMIBASE pBase;
    int rc = pDrvIns->pDrvHlp->pfnAttach(pDrvIns, &pBase);
    if (VBOX_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("Char#%d failed to attach driver below"), pDrvIns->iInstance);
    pData->pDrvStream = (PPDMISTREAM)pBase->pfnQueryInterface(pBase, PDMINTERFACE_STREAM);
    if (!pData->pDrvStream)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_PDM_MISSING_INTERFACE_BELOW, RT_SRC_POS, N_("Char#%d has no stream interface below"), pDrvIns->iInstance);

    rc = RTThreadCreate(&pData->ReceiveThread, drvCharReceiveLoop, (void *)pData, 0, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "Char");
    if (VBOX_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("Char#%d cannot create receive thread"), pDrvIns->iInstance);

    return VINF_SUCCESS;
}


/**
 * Destruct a char driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that
 * any non-VM resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvCharDestruct(PPDMDRVINS pDrvIns)
{
    PDRVCHAR     pData = PDMINS2DATA(pDrvIns, PDRVCHAR);

    LogFlow(("%s: iInstance=%d\n", __FUNCTION__, pDrvIns->iInstance));

    pData->fShutdown = true;
    RTThreadWait(pData->ReceiveThread, 1000, NULL);
    if (pData->ReceiveThread != NIL_RTTHREAD)
        LogRel(("Char%d: receive thread did not terminate\n", pDrvIns->iInstance));
}


/**
 * Char driver registration record.
 */
const PDMDRVREG g_DrvChar =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "Char",
    /* pszDescription */
    "Generic char driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_CHAR,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVCHAR),
    /* pfnConstruct */
    drvCharConstruct,
    /* pfnDestruct */
    drvCharDestruct,
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
    NULL,
    /** pfnPowerOff */
    NULL
};

