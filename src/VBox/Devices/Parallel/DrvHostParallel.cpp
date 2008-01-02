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
#include <VBox/pdmthread.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/stream.h>
#include <iprt/semaphore.h>
#include <iprt/file.h>

#ifdef RT_OS_LINUX
# include <sys/ioctl.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <sys/poll.h>
# include <fcntl.h>
# include <unistd.h>
# include <linux/ppdev.h>
# include <linux/parport.h>
# include <errno.h>
#endif

#include "Builtins.h"

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Host parallel port driver instance data.
 */
typedef struct DRVHOSTPARALLEL
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS                    pDrvIns;
    /** Pointer to the char port interface of the driver/device above us. */
    PPDMIHOSTPARALLELPORT         pDrvHostParallelPort;
    /** Our host device interface. */
    PDMIHOSTPARALLELCONNECTOR     IHostParallelConnector;
    /** Our host device port interface. */
    PDMIHOSTPARALLELPORT          IHostParallelPort;
    /** Device Path */
    char                          *pszDevicePath;
    /** Device Handle */
    RTFILE                        FileDevice;
    /** Thread waiting for interrupts. */
    PPDMTHREAD                    pMonitorThread;
    /** Wakeup pipe read end. */
    RTFILE                        WakeupPipeR;
    /** Wakeup pipe write end. */
    RTFILE                        WakeupPipeW;
} DRVHOSTPARALLEL, *PDRVHOSTPARALLEL;

/** Converts a pointer to DRVHOSTPARALLEL::IHostDeviceConnector to a PDRHOSTPARALLEL. */
#define PDMIHOSTPARALLELCONNECTOR_2_DRVHOSTPARALLEL(pInterface) ( (PDRVHOSTPARALLEL)((uintptr_t)pInterface - RT_OFFSETOF(DRVHOSTPARALLEL, IHostParallelConnector)) )
/** Converts a pointer to DRVHOSTPARALLEL::IHostDevicePort to a PDRHOSTPARALLEL. */
#define PDMIHOSTPARALLELPORT_2_DRVHOSTPARALLEL(pInterface) ( (PDRVHOSTPARALLEL)((uintptr_t)pInterface - RT_OFFSETOF(DRVHOSTPARALLEL, IHostParallelPort)) )

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
        case PDMINTERFACE_HOST_PARALLEL_CONNECTOR:
            return &pData->IHostParallelConnector;
        default:
            return NULL;
    }
}

/* -=-=-=-=- IHostDeviceConnector -=-=-=-=- */

/** @copydoc PDMICHAR::pfnWrite */
static DECLCALLBACK(int) drvHostParallelWrite(PPDMIHOSTPARALLELCONNECTOR pInterface, const void *pvBuf, size_t *cbWrite)
{
    PDRVHOSTPARALLEL pData = PDMIHOSTPARALLELCONNECTOR_2_DRVHOSTPARALLEL(pInterface);
    const unsigned char *pBuffer = (const unsigned char *)pvBuf;

    LogFlow(("%s: pvBuf=%#p cbWrite=%d\n", __FUNCTION__, pvBuf, *cbWrite));

    ioctl(pData->FileDevice, PPWDATA, pBuffer);
    *cbWrite = 1;

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostParallelRead(PPDMIHOSTPARALLELCONNECTOR pInterface, void *pvBuf, size_t *cbRead)
{
    PDRVHOSTPARALLEL pData = PDMIHOSTPARALLELCONNECTOR_2_DRVHOSTPARALLEL(pInterface);
    unsigned char *pBuffer = (unsigned char *)pvBuf;

    LogFlow(("%s: pvBuf=%#p cbRead=%d\n", __FUNCTION__, pvBuf, cbRead));

    ioctl(pData->FileDevice, PPRDATA, pBuffer);
    *cbRead = 1;

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostParallelSetMode(PPDMIHOSTPARALLELCONNECTOR pInterface, PDMPARALLELPORTMODE enmMode)
{
    PDRVHOSTPARALLEL pData = PDMIHOSTPARALLELCONNECTOR_2_DRVHOSTPARALLEL(pInterface);
    int ppdev_mode;

    LogFlow(("%s: mode=%d\n", __FUNCTION__, enmMode));

    switch (enmMode) {
        case PDM_PARALLEL_PORT_MODE_COMPAT:
            ppdev_mode = IEEE1284_MODE_COMPAT;
            break;
        case PDM_PARALLEL_PORT_MODE_EPP:
            ppdev_mode = IEEE1284_MODE_EPP;
            break;
        case PDM_PARALLEL_PORT_MODE_ECP:
            //ppdev_mode = IEEE1284_MODE_ECP;
            break;
    }
 
    ioctl(pData->FileDevice, PPSETMODE, &ppdev_mode);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostParallelWriteControl(PPDMIHOSTPARALLELCONNECTOR pInterface, uint8_t fReg)
{
    PDRVHOSTPARALLEL pData = PDMIHOSTPARALLELCONNECTOR_2_DRVHOSTPARALLEL(pInterface);

    LogFlow(("%s: fReg=%d\n", __FUNCTION__, fReg));

    ioctl(pData->FileDevice, PPWCONTROL, &fReg);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostParallelReadControl(PPDMIHOSTPARALLELCONNECTOR pInterface, uint8_t *pfReg)
{
    PDRVHOSTPARALLEL pData = PDMIHOSTPARALLELCONNECTOR_2_DRVHOSTPARALLEL(pInterface);
    uint8_t fReg;

    ioctl(pData->FileDevice, PPRCONTROL, &fReg);

    LogFlow(("%s: fReg=%d\n", __FUNCTION__, fReg));

    *pfReg = fReg;

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostParallelReadStatus(PPDMIHOSTPARALLELCONNECTOR pInterface, uint8_t *pfReg)
{
    PDRVHOSTPARALLEL pData = PDMIHOSTPARALLELCONNECTOR_2_DRVHOSTPARALLEL(pInterface);
    uint8_t fReg;

    ioctl(pData->FileDevice, PPRSTATUS, &fReg);

    LogFlow(("%s: fReg=%d\n", __FUNCTION__, fReg));

    *pfReg = fReg;

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostParallelMonitorThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVHOSTPARALLEL pData = PDMINS2DATA(pDrvIns, PDRVHOSTPARALLEL);
    struct pollfd aFDs[2];

    /*
     * We can wait for interrupts using poll on linux hosts.
     */
    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        int rc;

        aFDs[0].fd      = pData->FileDevice;
        aFDs[0].events  = POLLIN;
        aFDs[0].revents = 0;
        aFDs[1].fd      = pData->WakeupPipeR;
        aFDs[1].events  = POLLIN | POLLERR | POLLHUP;
        aFDs[1].revents = 0;
        rc = poll(aFDs, ELEMENTS(aFDs), -1);
        if (rc < 0)
        {
            AssertMsgFailed(("poll failed with rc=%d\n", RTErrConvertFromErrno(errno)));
            return RTErrConvertFromErrno(errno);
        }

        if (pThread->enmState != PDMTHREADSTATE_RUNNING)
            break;
        if (rc > 0 && aFDs[1].revents)
        {
            if (aFDs[1].revents & (POLLHUP | POLLERR | POLLNVAL))
                break;
            /* notification to terminate -- drain the pipe */
            char ch;
            size_t cbRead;
            RTFileRead(pData->WakeupPipeR, &ch, 1, &cbRead);
            continue;
        }

        /* Interrupt occured. */
        rc = pData->pDrvHostParallelPort->pfnNotifyInterrupt(pData->pDrvHostParallelPort);
        AssertRC(rc);
    }

    return VINF_SUCCESS;
}

/**
 * Unblock the monitor thread so it can respond to a state change.
 *
 * @returns a VBox status code.
 * @param     pDrvIns     The driver instance.
 * @param     pThread     The send thread.
 */
static DECLCALLBACK(int) drvHostParallelWakeupMonitorThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVHOSTPARALLEL pData = PDMINS2DATA(pDrvIns, PDRVHOSTPARALLEL);

    return RTFileWrite(pData->WakeupPipeW, "", 1, NULL);
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
     * Validate the config.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "DevicePath\0"))
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES,
                                N_("Unknown host parallel configuration option, only supports DevicePath"));

    /*
     * Init basic data members and interfaces.
     */

    /* IBase. */
    pDrvIns->IBase.pfnQueryInterface               = drvHostParallelQueryInterface;
    /* IHostParallelConnector. */
    pData->IHostParallelConnector.pfnWrite         = drvHostParallelWrite;
    pData->IHostParallelConnector.pfnRead          = drvHostParallelRead;
    pData->IHostParallelConnector.pfnSetMode       = drvHostParallelSetMode;
    pData->IHostParallelConnector.pfnWriteControl  = drvHostParallelWriteControl;
    pData->IHostParallelConnector.pfnReadControl   = drvHostParallelReadControl;
    pData->IHostParallelConnector.pfnReadStatus    = drvHostParallelReadStatus;

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
    rc = RTFileOpen(&pData->FileDevice, pData->pszDevicePath, RTFILE_O_OPEN | RTFILE_O_READWRITE);
    if (VBOX_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("Parallel#%d could not open '%s'"), 
                                   pDrvIns->iInstance, pData->pszDevicePath);

    /*
     * Try to get exclusive access to parallel port
     */
    rc = ioctl(pData->FileDevice, PPEXCL);     
    if (rc < 0)
        return PDMDrvHlpVMSetError(pDrvIns, RTErrConvertFromErrno(errno), RT_SRC_POS, 
                                   N_("Parallel#%d could not get exclusive access for parallel port '%s'"
                                      "Be sure that no other process or driver accesses this port"), 
                                   pDrvIns->iInstance, pData->pszDevicePath);

    /*
     * Claim the parallel port
     */
    rc = ioctl(pData->FileDevice, PPCLAIM);
    if (rc < 0)
        return PDMDrvHlpVMSetError(pDrvIns, RTErrConvertFromErrno(errno), RT_SRC_POS, 
                                   N_("Parallel#%d could not claim parallel port '%s'"
                                      "Be sure that no other process or driver accesses this port"), 
                                   pDrvIns->iInstance, pData->pszDevicePath);

    /*
     * Get the IHostParallelPort interface of the above driver/device.
     */
    pData->pDrvHostParallelPort = (PPDMIHOSTPARALLELPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_HOST_PARALLEL_PORT);
    if (!pData->pDrvHostParallelPort)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE, RT_SRC_POS, N_("Parallel#%d has no parallel port interface above"), 
                                   pDrvIns->iInstance);

    /*
     * Create wakeup pipe.
     */
    int aFDs[2];
    if (pipe(aFDs) != 0)
    {
        int rc = RTErrConvertFromErrno(errno);
        AssertRC(rc);
        return rc;
    }
    pData->WakeupPipeR = aFDs[0];
    pData->WakeupPipeW = aFDs[1];

    /*
     * Start waiting for interrupts.
     */
    rc = PDMDrvHlpPDMThreadCreate(pDrvIns, &pData->pMonitorThread, pData, drvHostParallelMonitorThread, drvHostParallelWakeupMonitorThread, 0, 
                                  RTTHREADTYPE_IO, "HostParallel");
    if (VBOX_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("HostParallel#%d cannot create monitor thread"), pDrvIns->iInstance);

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
    PDRVHOSTPARALLEL pData = PDMINS2DATA(pDrvIns, PDRVHOSTPARALLEL);

    LogFlow(("%s: iInstance=%d\n", __FUNCTION__, pDrvIns->iInstance));

    ioctl(pData->FileDevice, PPRELEASE);

    if (pData->WakeupPipeW != NIL_RTFILE)
    {
        int rc = RTFileClose(pData->WakeupPipeW);
        AssertRC(rc);
        pData->WakeupPipeW = NIL_RTFILE;
    }
    if (pData->WakeupPipeR != NIL_RTFILE)
    {
        int rc = RTFileClose(pData->WakeupPipeR);
        AssertRC(rc);
        pData->WakeupPipeR = NIL_RTFILE;
    }
    if (pData->FileDevice != NIL_RTFILE)
    {
        int rc = RTFileClose(pData->FileDevice);
        AssertRC(rc);
        pData->FileDevice = NIL_RTFILE;
    }
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

