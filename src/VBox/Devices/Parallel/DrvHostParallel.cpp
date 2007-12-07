/* $Id$ */
/** @file
 * VirtualBox Host Parallel Port Driver.
 *
 * Contributed by: Alexander Eichner
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_HOST_PARALLEL
#include <VBox/pdmdrv.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/stream.h>
#include <iprt/semaphore.h>

#ifdef RT_OS_LINUX
# include <sys/ioctl.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <unistd.h>
# include <linux/ppdev.h>
# include <linux/parport.h>
#endif

#include "Builtins.h"
#include "ParallelIOCtlCmd.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Host parallel port driver instance data.
 */
typedef struct DRVHOSTPARALLEL
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS                  pDrvIns;
    /** Pointer to the char port interface of the driver/device above us. */
    PPDMIHOSTDEVICEPORT         pDrvHostDevicePort;
    /** Our host device interface. */
    PDMIHOSTDEVICECONNECTOR     IHostDeviceConnector;
    /** Our host device port interface. */
    PDMIHOSTDEVICEPORT          IHostDevicePort;
    /** Device Path */
    char                        *pszDevicePath;
    /** Device Handle */
    RTFILE                      FileDevice;
    /** Flag to notify the receive thread it should terminate. */
    volatile bool               fShutdown;
    /** Receive thread ID. */
    RTTHREAD                    ReceiveThread;
    /** Send thread ID. */
    RTTHREAD                    SendThread;
    /** Send event semephore */
    RTSEMEVENT                  SendSem;

} DRVHOSTPARALLEL, *PDRVHOSTPARALLEL;

/** Converts a pointer to DRVHOSTPARALLEL::IHostDeviceConnector to a PDRHOSTPARALLEL. */
#define PDMIHOSTDEVICECONNECTOR_2_DRVHOSTPARALLEL(pInterface) ( (PDRVHOSTPARALLEL)((uintptr_t)pInterface - RT_OFFSETOF(DRVHOSTPARALLEL, IHostDeviceConnector)) )
/** Converts a pointer to DRVHOSTPARALLEL::IHostDevicePort to a PDRHOSTPARALLEL. */
#define PDMIHOSTDEVICEPORT_2_DRVHOSTPARALLEL(pInterface) ( (PDRVHOSTPARALLEL)((uintptr_t)pInterface - RT_OFFSETOF(DRVHOSTPARALLEL, IHostDevicePort)) )

/* -=-=-=-=- IBase -=-=-=-=- */

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 */
static DECLCALLBACK(void *) drvHostParallelQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS  pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTPARALLEL    pData = PDMINS2DATA(pDrvIns, PDRVHOSTPARALLEL);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_HOST_DEVICE_CONNECTOR:
            return &pData->IHostDeviceConnector;
        default:
            return NULL;
    }
}

/* -=-=-=-=- IHostDeviceConnector -=-=-=-=- */

/** @copydoc PDMICHAR::pfnWrite */
static DECLCALLBACK(int) drvHostParallelWrite(PPDMIHOSTDEVICECONNECTOR pInterface, const void *pvBuf, size_t *cbWrite)
{
    PDRVHOSTPARALLEL pData = PDMIHOSTDEVICECONNECTOR_2_DRVHOSTPARALLEL(pInterface);
    const unsigned char *pBuffer = (const unsigned char *)pvBuf;

    LogFlow(("%s: pvBuf=%#p cbWrite=%d\n", __FUNCTION__, pvBuf, *cbWrite));

    ioctl(pData->FileDevice, PPWDATA, pBuffer);

    RTSemEventSignal(pData->SendSem);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostParallelRead(PPDMIHOSTDEVICECONNECTOR pInterface, void *pvBuf, size_t *cbRead)
{
    PDRVHOSTPARALLEL pData = PDMIHOSTDEVICECONNECTOR_2_DRVHOSTPARALLEL(pInterface);
    unsigned char *pBuffer = (unsigned char *)pvBuf;

    LogFlow(("%s: pvBuf=%#p cbRead=%d\n", __FUNCTION__, pvBuf, cbRead));

    ioctl(pData->FileDevice, PPRDATA, pBuffer);
    *cbRead = 1;

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostParallelIOCtl(PPDMIHOSTDEVICECONNECTOR pInterface, RTUINT uCommand,
                                              void *pvData)
{
    PDRVHOSTPARALLEL pData = PDMIHOSTDEVICECONNECTOR_2_DRVHOSTPARALLEL(pInterface);
    unsigned long ioctlCommand;

    LogFlow(("%s: uCommand=%d pvData=%#p\n", __FUNCTION__, uCommand, pvData));

    switch (uCommand) {
        case LPT_IOCTL_COMMAND_SET_CONTROL:
            ioctlCommand = PPWCONTROL;
            break;
        case LPT_IOCTL_COMMAND_GET_CONTROL:
            ioctlCommand = PPRCONTROL;
            break;
        default:
            AssertMsgFailed(("uCommand = %d?\n"));
            return VERR_INVALID_PARAMETER;
    }

    ioctl(pData->FileDevice, ioctlCommand, pvData);

    return VINF_SUCCESS;
}

/**
 * Construct a host parallel driver instance.
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
static DECLCALLBACK(int) drvHostParallelConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVHOSTPARALLEL pData = PDMINS2DATA(pDrvIns, PDRVHOSTPARALLEL);
    LogFlow(("%s: iInstance=%d\n", __FUNCTION__, pDrvIns->iInstance));

    /*
     * Init basic data members and interfaces.
     */
    pData->ReceiveThread                    = NIL_RTTHREAD;
    pData->fShutdown                        = false;
    /* IBase. */
    pDrvIns->IBase.pfnQueryInterface        = drvHostParallelQueryInterface;
    /* IChar. */
    pData->IHostDeviceConnector.pfnWrite                   = drvHostParallelWrite;
    pData->IHostDeviceConnector.pfnIOCtl                   = drvHostParallelIOCtl;
    pData->IHostDeviceConnector.pfnRead                    = drvHostParallelRead;

    /*
     * Query configuration.
     */
    /* Device */
    int rc = CFGMR3QueryStringAlloc(pCfgHandle, "DevicePath", &pData->pszDevicePath);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: query for \"DevicePath\" string returned %Vra.\n", rc));
        return rc;
    }

    /*
     * Open the device
     */
    pData->FileDevice = open(pData->pszDevicePath, O_RDWR | O_NONBLOCK);
    if (pData->FileDevice < 0) {

    }

    /*
     * Try to get exclusive access to parallel port
     */
    if (ioctl(pData->FileDevice, PPEXCL) < 0) {
    }

    /*
     * Claim the parallel port
     */
    if (ioctl(pData->FileDevice, PPCLAIM) < 0) {
    }

    /*
     * Get the IHostDevicePort interface of the above driver/device.
     */
    pData->pDrvHostDevicePort = (PPDMIHOSTDEVICEPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_HOST_DEVICE_PORT);
    if (!pData->pDrvHostDevicePort)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE, RT_SRC_POS, N_("Parallel#%d has no parallel port interface above"), pDrvIns->iInstance);

    rc = RTSemEventCreate(&pData->SendSem);
    AssertRC(rc);

    return VINF_SUCCESS;
}


/**
 * Destruct a host parallel driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that
 * any non-VM resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvHostParallelDestruct(PPDMDRVINS pDrvIns)
{
    PDRVHOSTPARALLEL     pData = PDMINS2DATA(pDrvIns, PDRVHOSTPARALLEL);

    LogFlow(("%s: iInstance=%d\n", __FUNCTION__, pDrvIns->iInstance));

    pData->fShutdown = true;
    if (pData->ReceiveThread)
    {
        RTThreadWait(pData->ReceiveThread, 1000, NULL);
        if (pData->ReceiveThread != NIL_RTTHREAD)
            LogRel(("Parallel%d: receive thread did not terminate\n", pDrvIns->iInstance));
    }

    RTSemEventSignal(pData->SendSem);
    RTSemEventDestroy(pData->SendSem);
    pData->SendSem = NIL_RTSEMEVENT;

    if (pData->SendThread)
    {
        RTThreadWait(pData->SendThread, 1000, NULL);
        if (pData->SendThread != NIL_RTTHREAD)
            LogRel(("Parallel%d: send thread did not terminate\n", pDrvIns->iInstance));
    }

    ioctl(pData->FileDevice, PPRELEASE);
    close(pData->FileDevice);
}

/**
 * Char driver registration record.
 */
const PDMDRVREG g_DrvHostParallel =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "HostParallel",
    /* pszDescription */
    "Parallel host driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_CHAR,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVHOSTPARALLEL),
    /* pfnConstruct */
    drvHostParallelConstruct,
    /* pfnDestruct */
    drvHostParallelDestruct,
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

