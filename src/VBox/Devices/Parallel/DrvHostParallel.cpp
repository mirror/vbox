/* $Id$ */
/** @file
 * VirtualBox Host Parallel Port Driver.
 *
 * Contributed by: Alexander Eichner
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_HOST_PARALLEL
#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/pdmthread.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/pipe.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <iprt/uuid.h>

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

#include "VBoxDD.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Host parallel port driver instance data.
 * @implements PDMIHOSTPARALLELCONNECTOR
 */
typedef struct DRVHOSTPARALLEL
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS                    pDrvIns;
    /** Pointer to the char port interface of the driver/device above us. */
    PPDMIHOSTPARALLELPORT         pDrvHostParallelPort;
    /** Our host device interface. */
    PDMIHOSTPARALLELCONNECTOR     IHostParallelConnector;
    /** Device Path */
    char                         *pszDevicePath;
    /** Device Handle */
    RTFILE                        hFileDevice;
    /** Thread waiting for interrupts. */
    PPDMTHREAD                    pMonitorThread;
    /** Wakeup pipe read end. */
    RTPIPE                        hWakeupPipeR;
    /** Wakeup pipe write end. */
    RTPIPE                        hWakeupPipeW;
    /** Current mode the parallel port is in. */
    PDMPARALLELPORTMODE           enmModeCur;
} DRVHOSTPARALLEL, *PDRVHOSTPARALLEL;

/** Converts a pointer to DRVHOSTPARALLEL::IHostDeviceConnector to a PDRHOSTPARALLEL. */
#define PDMIHOSTPARALLELCONNECTOR_2_DRVHOSTPARALLEL(pInterface) ( (PDRVHOSTPARALLEL)((uintptr_t)pInterface - RT_OFFSETOF(DRVHOSTPARALLEL, IHostParallelConnector)) )

/**
 * Changes the current mode of the host parallel port.
 *
 * @returns VBox status code.
 * @param   pThis    The host parallel port instance data.
 * @param   enmMode  The mode to change the port to.
 */
static int drvHostParallelSetMode(PDRVHOSTPARALLEL pThis, PDMPARALLELPORTMODE enmMode)
{
    int iMode = 0;
    int rc = VINF_SUCCESS;
    int rcLnx;

    LogFlow(("%s: mode=%d\n", __FUNCTION__, enmMode));

    if (pThis->enmModeCur != enmMode)
    {
        switch (enmMode)
        {
            case PDM_PARALLEL_PORT_MODE_SPP:
                iMode = IEEE1284_MODE_COMPAT;
                break;
            case PDM_PARALLEL_PORT_MODE_EPP_DATA:
                iMode = IEEE1284_MODE_EPP | IEEE1284_DATA;
                break;
            case PDM_PARALLEL_PORT_MODE_EPP_ADDR:
                iMode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
                break;
            case PDM_PARALLEL_PORT_MODE_ECP:
            case PDM_PARALLEL_PORT_MODE_INVALID:
            default:
                return VERR_NOT_SUPPORTED;
        }

        rcLnx = ioctl(RTFileToNative(pThis->hFileDevice), PPSETMODE, &iMode);
        if (RT_UNLIKELY(rcLnx < 0))
            rc = RTErrConvertFromErrno(errno);
        else
            pThis->enmModeCur = enmMode;
    }

    return rc;
}

/* -=-=-=-=- IBase -=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvHostParallelQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS          pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTPARALLEL    pThis   = PDMINS_2_DATA(pDrvIns, PDRVHOSTPARALLEL);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTPARALLELCONNECTOR, &pThis->IHostParallelConnector);
    return NULL;
}

/* -=-=-=-=- IHostDeviceConnector -=-=-=-=- */

/** @copydoc PDMICHARCONNECTOR::pfnWrite */
static DECLCALLBACK(int) drvHostParallelWrite(PPDMIHOSTPARALLELCONNECTOR pInterface, const void *pvBuf, size_t cbWrite, PDMPARALLELPORTMODE enmMode)
{
    PDRVHOSTPARALLEL pThis = PDMIHOSTPARALLELCONNECTOR_2_DRVHOSTPARALLEL(pInterface);
    int rc = VINF_SUCCESS;
    int rcLnx = 0;

    LogFlow(("%s: pvBuf=%#p cbWrite=%d\n", __FUNCTION__, pvBuf, cbWrite));

    rc = drvHostParallelSetMode(pThis, enmMode);
    if (RT_FAILURE(rc))
        return rc;

    if (enmMode == PDM_PARALLEL_PORT_MODE_SPP)
    {
        /* Set the data lines directly. */
        rcLnx = ioctl(RTFileToNative(pThis->hFileDevice), PPWDATA, pvBuf);
    }
    else
    {
        /* Use write interface. */
        rcLnx = write(RTFileToNative(pThis->hFileDevice), pvBuf, cbWrite);
    }
    if (RT_UNLIKELY(rcLnx < 0))
        rc = RTErrConvertFromErrno(errno);

    return rc;
}

static DECLCALLBACK(int) drvHostParallelRead(PPDMIHOSTPARALLELCONNECTOR pInterface, void *pvBuf, size_t cbRead, PDMPARALLELPORTMODE enmMode)
{
    PDRVHOSTPARALLEL pThis = PDMIHOSTPARALLELCONNECTOR_2_DRVHOSTPARALLEL(pInterface);
    int rc = VINF_SUCCESS;
    int rcLnx = 0;

    LogFlow(("%s: pvBuf=%#p cbRead=%d\n", __FUNCTION__, pvBuf, cbRead));

    rc = drvHostParallelSetMode(pThis, enmMode);
    if (RT_FAILURE(rc))
        return rc;

    if (enmMode == PDM_PARALLEL_PORT_MODE_SPP)
    {
        /* Set the data lines directly. */
        rcLnx = ioctl(RTFileToNative(pThis->hFileDevice), PPWDATA, pvBuf);
    }
    else
    {
        /* Use write interface. */
        rcLnx = read(RTFileToNative(pThis->hFileDevice), pvBuf, cbRead);
    }
    if (RT_UNLIKELY(rcLnx < 0))
        rc = RTErrConvertFromErrno(errno);

    return rc;
}

static DECLCALLBACK(int) drvHostParallelSetPortDirection(PPDMIHOSTPARALLELCONNECTOR pInterface, bool fForward)
{
    PDRVHOSTPARALLEL pThis = PDMIHOSTPARALLELCONNECTOR_2_DRVHOSTPARALLEL(pInterface);
    int rc = VINF_SUCCESS;
    int rcLnx = 0;
    int iMode = 0;

    if (!fForward)
        iMode = 1;

    rcLnx = ioctl(RTFileToNative(pThis->hFileDevice), PPDATADIR, &iMode);
    if (RT_UNLIKELY(rcLnx < 0))
        rc = RTErrConvertFromErrno(errno);

    return rc;
}

static DECLCALLBACK(int) drvHostParallelWriteControl(PPDMIHOSTPARALLELCONNECTOR pInterface, uint8_t fReg)
{
    PDRVHOSTPARALLEL pThis = PDMIHOSTPARALLELCONNECTOR_2_DRVHOSTPARALLEL(pInterface);
    int rc = VINF_SUCCESS;
    int rcLnx = 0;

    LogFlow(("%s: fReg=%d\n", __FUNCTION__, fReg));
    rcLnx = ioctl(RTFileToNative(pThis->hFileDevice), PPWCONTROL, &fReg);
    if (RT_UNLIKELY(rcLnx < 0))
        rc = RTErrConvertFromErrno(errno);

    return rc;
}

static DECLCALLBACK(int) drvHostParallelReadControl(PPDMIHOSTPARALLELCONNECTOR pInterface, uint8_t *pfReg)
{
    PDRVHOSTPARALLEL pThis = PDMIHOSTPARALLELCONNECTOR_2_DRVHOSTPARALLEL(pInterface);
    int rc = VINF_SUCCESS;
    int rcLnx = 0;
    uint8_t fReg = 0;

    rcLnx = ioctl(RTFileToNative(pThis->hFileDevice), PPRCONTROL, &fReg);
    if (RT_UNLIKELY(rcLnx < 0))
        rc = RTErrConvertFromErrno(errno);
    else
    {
        LogFlow(("%s: fReg=%d\n", __FUNCTION__, fReg));
        *pfReg = fReg;
    }

    return rc;
}

static DECLCALLBACK(int) drvHostParallelReadStatus(PPDMIHOSTPARALLELCONNECTOR pInterface, uint8_t *pfReg)
{
    PDRVHOSTPARALLEL pThis = PDMIHOSTPARALLELCONNECTOR_2_DRVHOSTPARALLEL(pInterface);
    int rc = VINF_SUCCESS;
    int rcLnx = 0;
    uint8_t fReg = 0;

    rcLnx = ioctl(RTFileToNative(pThis->hFileDevice), PPRSTATUS, &fReg);
    if (RT_UNLIKELY(rcLnx < 0))
        rc = RTErrConvertFromErrno(errno);
    else
    {
        LogFlow(("%s: fReg=%d\n", __FUNCTION__, fReg));
        *pfReg = fReg;
    }

    return rc;
}

static DECLCALLBACK(int) drvHostParallelMonitorThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVHOSTPARALLEL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTPARALLEL);
    struct pollfd aFDs[2];

    /*
     * We can wait for interrupts using poll on linux hosts.
     */
    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        int rc;

        aFDs[0].fd      = RTFileToNative(pThis->hFileDevice);
        aFDs[0].events  = POLLIN;
        aFDs[0].revents = 0;
        aFDs[1].fd      = RTPipeToNative(pThis->hWakeupPipeR);
        aFDs[1].events  = POLLIN | POLLERR | POLLHUP;
        aFDs[1].revents = 0;
        rc = poll(aFDs, RT_ELEMENTS(aFDs), -1);
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
            RTPipeRead(pThis->hWakeupPipeR, &ch, 1, &cbRead);
            continue;
        }

        /* Interrupt occurred. */
        rc = pThis->pDrvHostParallelPort->pfnNotifyInterrupt(pThis->pDrvHostParallelPort);
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
    PDRVHOSTPARALLEL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTPARALLEL);
    size_t cbIgnored;
    return RTPipeWrite(pThis->hWakeupPipeW, "", 1, &cbIgnored);
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
    PDRVHOSTPARALLEL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTPARALLEL);
    LogFlow(("%s: iInstance=%d\n", __FUNCTION__, pDrvIns->iInstance));
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    int rc;

    if (pThis->hFileDevice != NIL_RTFILE)
        ioctl(RTFileToNative(pThis->hFileDevice), PPRELEASE);

    rc = RTPipeClose(pThis->hWakeupPipeW); AssertRC(rc);
    pThis->hWakeupPipeW = NIL_RTPIPE;

    rc = RTPipeClose(pThis->hWakeupPipeR); AssertRC(rc);
    pThis->hWakeupPipeR = NIL_RTPIPE;

    rc = RTFileClose(pThis->hFileDevice); AssertRC(rc);
    pThis->hFileDevice = NIL_RTFILE;

    if (pThis->pszDevicePath)
    {
        MMR3HeapFree(pThis->pszDevicePath);
        pThis->pszDevicePath = NULL;
    }
}

/**
 * Construct a host parallel driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvHostParallelConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVHOSTPARALLEL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTPARALLEL);
    LogFlow(("%s: iInstance=%d\n", __FUNCTION__, pDrvIns->iInstance));
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Init basic data members and interfaces.
     *
     * Must be done before returning any failure because we've got a destructor.
     */
    pThis->hFileDevice  = NIL_RTFILE;
    pThis->hWakeupPipeR = NIL_RTPIPE;
    pThis->hWakeupPipeW = NIL_RTPIPE;

    /* IBase. */
    pDrvIns->IBase.pfnQueryInterface                  = drvHostParallelQueryInterface;
    /* IHostParallelConnector. */
    pThis->IHostParallelConnector.pfnWrite            = drvHostParallelWrite;
    pThis->IHostParallelConnector.pfnRead             = drvHostParallelRead;
    pThis->IHostParallelConnector.pfnSetPortDirection = drvHostParallelSetPortDirection;
    pThis->IHostParallelConnector.pfnWriteControl     = drvHostParallelWriteControl;
    pThis->IHostParallelConnector.pfnReadControl      = drvHostParallelReadControl;
    pThis->IHostParallelConnector.pfnReadStatus       = drvHostParallelReadStatus;

    /*
     * Validate the config.
     */
    if (!CFGMR3AreValuesValid(pCfg, "DevicePath\0"))
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES,
                                N_("Unknown host parallel configuration option, only supports DevicePath"));

    /*
     * Query configuration.
     */
    /* Device */
    int rc = CFGMR3QueryStringAlloc(pCfg, "DevicePath", &pThis->pszDevicePath);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: query for \"DevicePath\" string returned %Rra.\n", rc));
        return rc;
    }

    /*
     * Open the device
     */
    rc = RTFileOpen(&pThis->hFileDevice, pThis->pszDevicePath, RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("Parallel#%d could not open '%s'"),
                                   pDrvIns->iInstance, pThis->pszDevicePath);

    /*
     * Try to get exclusive access to parallel port
     */
    rc = ioctl(RTFileToNative(pThis->hFileDevice), PPEXCL);
    if (rc < 0)
        return PDMDrvHlpVMSetError(pDrvIns, RTErrConvertFromErrno(errno), RT_SRC_POS,
                                   N_("Parallel#%d could not get exclusive access for parallel port '%s'"
                                      "Be sure that no other process or driver accesses this port"),
                                   pDrvIns->iInstance, pThis->pszDevicePath);

    /*
     * Claim the parallel port
     */
    rc = ioctl(RTFileToNative(pThis->hFileDevice), PPCLAIM);
    if (rc < 0)
        return PDMDrvHlpVMSetError(pDrvIns, RTErrConvertFromErrno(errno), RT_SRC_POS,
                                   N_("Parallel#%d could not claim parallel port '%s'"
                                      "Be sure that no other process or driver accesses this port"),
                                   pDrvIns->iInstance, pThis->pszDevicePath);

    /*
     * Get the IHostParallelPort interface of the above driver/device.
     */
    pThis->pDrvHostParallelPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIHOSTPARALLELPORT);
    if (!pThis->pDrvHostParallelPort)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE, RT_SRC_POS, N_("Parallel#%d has no parallel port interface above"),
                                   pDrvIns->iInstance);

    /*
     * Create wakeup pipe.
     */
    rc = RTPipeCreate(&pThis->hWakeupPipeR, &pThis->hWakeupPipeW, 0 /*fFlags*/);
    AssertRCReturn(rc, rc);

    /*
     * Start in SPP mode.
     */
    pThis->enmModeCur = PDM_PARALLEL_PORT_MODE_INVALID;
    rc = drvHostParallelSetMode(pThis, PDM_PARALLEL_PORT_MODE_SPP);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("HostParallel#%d cannot change mode of parallel mode to SPP"), pDrvIns->iInstance);

    /*
     * Start waiting for interrupts.
     */
    rc = PDMDrvHlpThreadCreate(pDrvIns, &pThis->pMonitorThread, pThis, drvHostParallelMonitorThread, drvHostParallelWakeupMonitorThread, 0,
                               RTTHREADTYPE_IO, "ParMon");
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("HostParallel#%d cannot create monitor thread"), pDrvIns->iInstance);


    return VINF_SUCCESS;
}

/**
 * Char driver registration record.
 */
const PDMDRVREG g_DrvHostParallel =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "HostParallel",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Parallel host driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_CHAR,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVHOSTPARALLEL),
    /* pfnConstruct */
    drvHostParallelConstruct,
    /* pfnDestruct */
    drvHostParallelDestruct,
    /* pfnRelocate */
    NULL,
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

