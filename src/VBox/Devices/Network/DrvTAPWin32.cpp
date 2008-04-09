/** @file
 *
 * VBox network devices:
 * Linux/Win32 TUN network transport driver
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
#define LOG_GROUP LOG_GROUP_DRV_TUN
#include <VBox/pdmdrv.h>
#include <VBox/stam.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/asm.h>
#include <iprt/semaphore.h>

#include <windows.h>
#include <VBox/tapwin32.h>

#include "Builtins.h"

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Block driver instance data.
 */
typedef struct
{
    /** The network interface. */
    PDMINETWORKCONNECTOR    INetworkConnector;
    /** The network interface. */
    PPDMINETWORKPORT        pPort;
    /** Pointer to the driver instance. */
    PPDMDRVINS              pDrvIns;
    /** TAP device file handle. */
    HANDLE                  hFile;

    HANDLE                  hEventWrite;
    HANDLE                  hEventRead;

    OVERLAPPED              overlappedRead;
    DWORD                   dwNumberOfBytesRead;
    uint8_t                 readBuffer[4096];

    TAP_VERSION             tapVersion;

    /** The thread handle. NIL_RTTHREAD if no thread. */
    PPDMTHREAD              pThread;
    /** The event semaphore the thread is waiting on. */
    HANDLE                  hHaltAsyncEventSem;

#ifdef DEBUG
    DWORD                   dwLastReadTime;
    DWORD                   dwLastWriteTime;
#endif

#ifdef VBOX_WITH_STATISTICS
    /** Number of sent packets. */
    STAMCOUNTER             StatPktSent;
    /** Number of sent bytes. */
    STAMCOUNTER             StatPktSentBytes;
    /** Number of received packets. */
    STAMCOUNTER             StatPktRecv;
    /** Number of received bytes. */
    STAMCOUNTER             StatPktRecvBytes;
    /** Profiling packet transmit runs. */
    STAMPROFILEADV          StatTransmit;
    /** Profiling packet receive runs. */
    STAMPROFILEADV          StatReceive;
    STAMPROFILE             StatRecvOverflows;
#endif /* VBOX_WITH_STATISTICS */
} DRVTAP, *PDRVTAP;

/** Converts a pointer to TUN::INetworkConnector to a PRDVTUN. */
#define PDMINETWORKCONNECTOR_2_DRVTAP(pInterface) ( (PDRVTAP)((uintptr_t)pInterface - RT_OFFSETOF(DRVTAP, INetworkConnector)) )

/**
 * Send data to the network.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   pvBuf           Data to send.
 * @param   cb              Number of bytes to send.
 * @thread  EMT
 */
static DECLCALLBACK(int) drvTAPW32Send(PPDMINETWORKCONNECTOR pInterface, const void *pvBuf, size_t cb)
{
    OVERLAPPED overlapped;
    DWORD      cbBytesWritten;
    int        rc;
    PDRVTAP    pData = PDMINETWORKCONNECTOR_2_DRVTAP(pInterface);

    Log2(("drvTAPW32Send%d: pvBuf=%p cb=%#x\n"
          "%.*Vhxd\n", pData->pDrvIns->iInstance, pvBuf, cb, cb, pvBuf));

#ifdef DEBUG
    pData->dwLastReadTime = timeGetTime();
    Log(("drvTAPW32Send %d bytes at %08x - delta %x\n", cb, pData->dwLastReadTime, pData->dwLastReadTime - pData->dwLastWriteTime));
#endif

    STAM_COUNTER_INC(&pData->StatPktSent);
    STAM_COUNTER_ADD(&pData->StatPktSentBytes, cb);
    STAM_PROFILE_ADV_START(&pData->StatTransmit, a);

    memset(&overlapped, 0, sizeof(overlapped));
    overlapped.hEvent = pData->hEventWrite;

    rc = VINF_SUCCESS;
    if (WriteFile(pData->hFile, pvBuf, cb, &cbBytesWritten, &overlapped) == FALSE)
    {
        if (GetLastError() == ERROR_IO_PENDING)
        {
            Log(("drvTAPW32Send: IO pending!!\n"));
            rc = WaitForSingleObject(overlapped.hEvent, INFINITE);
            AssertMsg(rc == WAIT_OBJECT_0, ("WaitForSingleObject failed with %x\n", rc));
            rc = VINF_SUCCESS;
        }
        else
        {
            AssertMsgFailed(("WriteFile failed with %d\n", GetLastError()));
            rc = RTErrConvertFromWin32(GetLastError());
        }
    }
    STAM_PROFILE_ADV_STOP(&pData->StatTransmit, a);
    AssertRC(rc);
    return rc;
}


/**
 * Set promiscuous mode.
 *
 * This is called when the promiscuous mode is set. This means that there doesn't have
 * to be a mode change when it's called.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   fPromiscuous    Set if the adaptor is now in promiscuous mode. Clear if it is not.
 * @thread  EMT
 */
static DECLCALLBACK(void) drvTAPW32SetPromiscuousMode(PPDMINETWORKCONNECTOR pInterface, bool fPromiscuous)
{
    LogFlow(("drvTAPW32SetPromiscuousMode: fPromiscuous=%d\n", fPromiscuous));
    /* nothing to do */
}


/**
 * Notification on link status changes.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   enmLinkState    The new link state.
 * @thread  EMT
 */
static DECLCALLBACK(void) drvTAPW32NotifyLinkChanged(PPDMINETWORKCONNECTOR pInterface, PDMNETWORKLINKSTATE enmLinkState)
{
    LogFlow(("drvNATW32NotifyLinkChanged: enmLinkState=%d\n", enmLinkState));
    /** @todo take action on link down and up. Stop the polling and such like. */
}


/**
 * Async I/O thread for an interface.
 */
static DECLCALLBACK(int) drvTAPW32AsyncIoThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVTAP pData = (PDRVTAP)pvUser;
    HANDLE  haWait[2];
    DWORD   rc = ERROR_SUCCESS, dwNumberOfBytesTransferred;

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    Assert(pData);
    haWait[0] = pData->hEventRead;
    haWait[1] = pData->hHaltAsyncEventSem;

    while(1)
    {
        BOOL  bRet;

        memset(&pData->overlappedRead, 0, sizeof(pData->overlappedRead));
        pData->overlappedRead.hEvent = pData->hEventRead;
        bRet = ReadFile(pData->hFile, pData->readBuffer, sizeof(pData->readBuffer),
                        &dwNumberOfBytesTransferred, &pData->overlappedRead);
        if (bRet == FALSE)
        {
            rc = GetLastError();
            AssertMsg(rc == ERROR_IO_PENDING || rc == ERROR_MORE_DATA, ("ReadFile failed with rc=%d\n", rc));
            if (rc != ERROR_IO_PENDING && rc != ERROR_MORE_DATA)
                break;

            rc = WaitForMultipleObjects(2, &haWait[0], FALSE, INFINITE);
            AssertMsg(rc == WAIT_OBJECT_0 || rc == WAIT_OBJECT_0+1, ("WaitForSingleObject failed with %x\n", rc));

            if (rc != WAIT_OBJECT_0)
                break;  /* asked to quit or fatal error. */

            rc = GetOverlappedResult(pData->hFile, &pData->overlappedRead, &dwNumberOfBytesTransferred, FALSE);
            Assert(rc == TRUE);

            /* If GetOverlappedResult() returned with TRUE, the operation was finished successfully */
        }

        /* Not very nice, but what else can we do? */
        rc = pData->pPort->pfnWaitReceiveAvail(pData->pPort, RT_INDEFINITE_WAIT);
        if (RT_FAILURE(rc))
            break;

        STAM_COUNTER_INC(&pData->StatPktRecv);
        STAM_COUNTER_ADD(&pData->StatPktRecvBytes, dwNumberOfBytesTransferred);
#ifdef DEBUG
        pData->dwLastWriteTime = timeGetTime();
        Log(("drvTAPW32AsyncIo %d bytes at %08x - delta %x\n", dwNumberOfBytesTransferred,
             pData->dwLastWriteTime, pData->dwLastWriteTime - pData->dwLastReadTime));
#endif
        rc = pData->pPort->pfnReceive(pData->pPort, pData->readBuffer, dwNumberOfBytesTransferred);
        AssertRC(rc);
    }

    SetEvent(pData->hHaltAsyncEventSem);
    Log(("drvTAPW32AsyncIo: exit thread!!\n"));
    return VINF_SUCCESS;
}

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 * @thread  Any thread.
 */
static DECLCALLBACK(void *) drvTAPW32QueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVTAP pData = PDMINS2DATA(pDrvIns, PDRVTAP);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_NETWORK_CONNECTOR:
            return &pData->INetworkConnector;
        default:
            return NULL;
    }
}


/**
 * Destruct a driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvTAPW32Destruct(PPDMDRVINS pDrvIns)
{
    PDRVTAP pData = PDMINS2DATA(pDrvIns, PDRVTAP);
    TAP_MEDIASTATUS mediastatus;
    DWORD dwLength;

    LogFlow(("drvTAPW32Destruct\n"));

    /** @todo this isn't a safe method to notify the async thread; it might be using the instance
     *        data after we've been destroyed; could wait for it to terminate, but that's not
     *        without risks either.
     */
    SetEvent(pData->hHaltAsyncEventSem);

    /* Yield or else our async thread will never acquire the event semaphore */
    RTThreadSleep(16);
    /* Wait for the async thread to quit; up to half a second */
    WaitForSingleObject(pData->hHaltAsyncEventSem, 500);

    mediastatus.fConnect = FALSE;
    BOOL ret = DeviceIoControl(pData->hFile, TAP_IOCTL_SET_MEDIA_STATUS,
                               &mediastatus, sizeof(mediastatus), NULL, 0, &dwLength, NULL);
    Assert(ret);

    CloseHandle(pData->hEventWrite);
    CancelIo(pData->hFile);
    CloseHandle(pData->hFile);
}


/**
 * Construct a TUN network transport driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) drvTAPW32Construct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVTAP pData = PDMINS2DATA(pDrvIns, PDRVTAP);

    /*
     * Init the static parts.
     */
    pData->pDrvIns                      = pDrvIns;
    pData->hFile                        = INVALID_HANDLE_VALUE;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface    = drvTAPW32QueryInterface;
    /* INetwork */
    pData->INetworkConnector.pfnSend                = drvTAPW32Send;
    pData->INetworkConnector.pfnSetPromiscuousMode  = drvTAPW32SetPromiscuousMode;
    pData->INetworkConnector.pfnNotifyLinkChanged   = drvTAPW32NotifyLinkChanged;

    /*
     * Validate the config.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "Device\0HostInterfaceName\0GUID\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

    /*
     * Check that no-one is attached to us.
     */
    int rc = pDrvIns->pDrvHlp->pfnAttach(pDrvIns, NULL);
    if (rc != VERR_PDM_NO_ATTACHED_DRIVER)
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_DRVINS_NO_ATTACH,
                                N_("Configuration error: Cannot attach drivers to the TUN driver"));

    /*
     * Query the network port interface.
     */
    pData->pPort = (PPDMINETWORKPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_NETWORK_PORT);
    if (!pData->pPort)
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE,
                                N_("Configuration error: the above device/driver didn't export the network port interface"));

    /*
     * Read the configuration.
     */
    char *pszHostDriver = NULL;
    rc = CFGMR3QueryStringAlloc(pCfgHandle, "HostInterfaceName", &pszHostDriver);
    if (VBOX_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: query for \"HostInterfaceName\" failed"));

    TAP_MEDIASTATUS mediastatus;
    DWORD length;
    char szFullDriverName[256];
    char szDriverGUID[256] = {0};

    rc = CFGMR3QueryBytes(pCfgHandle, "GUID", szDriverGUID, sizeof(szDriverGUID));
    if (VBOX_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("Configuration error: could not query GUID"));

    RTStrPrintfEx(NULL, NULL, szFullDriverName, sizeof(szFullDriverName), "\\\\.\\Global\\%s.tap", szDriverGUID);

    pData->hFile = CreateFile(szFullDriverName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED, 0);

    if (pData->hFile == INVALID_HANDLE_VALUE)
    {
        rc = GetLastError();

        AssertMsgFailed(("Configuration error: TAP device name %s is not valid! (rc=%d)\n", szFullDriverName, rc));
        if (rc == ERROR_SHARING_VIOLATION)
            return VERR_PDM_HIF_SHARING_VIOLATION;

        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_HIF_OPEN_FAILED,
                                N_("Failed to open Host Interface Networking device driver"));
    }

    BOOL ret = DeviceIoControl(pData->hFile, TAP_IOCTL_GET_VERSION, &pData->tapVersion, sizeof (pData->tapVersion),
                               &pData->tapVersion, sizeof(pData->tapVersion), &length, NULL);
    if (ret == FALSE)
    {
        CloseHandle(pData->hFile);
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_HIF_INVALID_VERSION,
                                N_("Failed to get the Host Interface Networking device driver version"));;
    }
    LogRel(("TAP version %d.%d\n", pData->tapVersion.major, pData->tapVersion.minor));

    /* Must be at least version 8.1 */
    if (    pData->tapVersion.major != 8
        ||  pData->tapVersion.minor < 1)
    {
        CloseHandle(pData->hFile);
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_HIF_INVALID_VERSION,
                                N_("Invalid Host Interface Networking device driver version"));;
    }

    mediastatus.fConnect = TRUE;
    ret = DeviceIoControl(pData->hFile, TAP_IOCTL_SET_MEDIA_STATUS, &mediastatus, sizeof(mediastatus), NULL, 0, &length, NULL);
    if (ret == FALSE)
    {
        CloseHandle(pData->hFile);
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;
    }

    if (pszHostDriver)
        MMR3HeapFree(pszHostDriver);

    pData->hEventWrite = CreateEvent(NULL, FALSE, FALSE, NULL);
    pData->hEventRead  = CreateEvent(NULL, FALSE, FALSE, NULL);
    memset(&pData->overlappedRead, 0, sizeof(pData->overlappedRead));

    pData->hHaltAsyncEventSem = CreateEvent(NULL, FALSE, FALSE, NULL);

    /* Create asynchronous thread */
    rc = PDMDrvHlpPDMThreadCreate(pDrvIns, &pData->pThread, pData, drvTAPW32AsyncIoThread, drvTAPW32AsyncIoWakeup, 128 * _1K, RTTHREADTYPE_IO, "TAP");
    AssertRCReturn(rc, rc);

    Assert(pData->hThread != NIL_RTTHREAD && pData->hHaltAsyncEventSem != NULL);

#ifdef VBOX_WITH_STATISTICS
    /*
     * Statistics.
     */
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatPktSent,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,     "Number of sent packets.",         "/Drivers/TAP%d/Packets/Sent", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatPktSentBytes, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,          "Number of sent bytes.",           "/Drivers/TAP%d/Bytes/Sent", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatPktRecv,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,     "Number of received packets.",     "/Drivers/TAP%d/Packets/Received", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatPktRecvBytes, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,          "Number of received bytes.",       "/Drivers/TAP%d/Bytes/Received", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatTransmit,     STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling packet transmit runs.", "/Drivers/TAP%d/Transmit", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatReceive,      STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling packet receive runs.",  "/Drivers/TAP%d/Receive", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatRecvOverflows,STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_OCCURENCE, "Profiling packet receive overflows.", "/Drivers/TAP%d/RecvOverflows", pDrvIns->iInstance);
#endif

    return rc;
}


/**
 * Host Interface network transport driver registration record.
 */
const PDMDRVREG g_DrvHostInterface =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "HostInterface",
    /* pszDescription */
    "Host Interface Network Transport Driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_NETWORK,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVTAP),
    /* pfnConstruct */
    drvTAPW32Construct,
    /* pfnDestruct */
    drvTAPW32Destruct,
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
    /* pfnPowerOff */
    NULL
};
