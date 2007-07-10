/** $Id$ */
/** @file
 * VBox network devices: OS/2 TAP network transport driver.
 */

/*
 *
 * Copyright (C) 2006-2007 innotek GmbH
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
 *
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_TUN
#include <VBox/cfgm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/mm.h>
#include <VBox/pdm.h>

#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/asm.h>
#include <iprt/semaphore.h>

#include "Builtins.h"



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * Block driver instance data.
 */
typedef struct DRVTAPOS2
{
    /** The network interface. */
    PDMINETWORKCONNECTOR    INetworkConnector;
    /** The network interface. */
    PPDMINETWORKPORT        pPort;
    /** Pointer to the driver instance. */
    PPDMDRVINS              pDrvIns;
    /** TAP device file handle. */
    RTFILE                  FileDevice;
    /** Receiver thread. */
    PPDMTHREAD              pThread;
    /** We are waiting for more receive buffers. */
    uint32_t volatile       fOutOfSpace;
    /** Event semaphore for blocking on receive. */
    RTSEMEVENT              EventOutOfSpace;

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
    STAMPROFILE             StatTransmit;
    /** Profiling packet receive runs. */
    STAMPROFILEADV          StatReceive;
    STAMPROFILE             StatRecvOverflows;
#endif /* VBOX_WITH_STATISTICS */

#ifdef LOG_ENABLED
    /** The nano ts of the last transfer. */
    uint64_t                u64LastTransferTS;
    /** The nano ts of the last receive. */
    uint64_t                u64LastReceiveTS;
#endif
} DRVTAPOS2, *PDRVTAPOS2;


/** Converts a pointer to TAP::INetworkConnector to a PRDVTAP. */
#define PDMINETWORKCONNECTOR_2_DRVTAPOS2(pInterface) ( (PDRVTAPOS2)((uintptr_t)pInterface - RT_OFFSETOF(DRVTAPOS2, INetworkConnector)) )


/**
 * Send data to the network.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   pvBuf           Data to send.
 * @param   cb              Number of bytes to send.
 * @thread  EMT
 */
static DECLCALLBACK(int) drvTAPOs2Send(PPDMINETWORKCONNECTOR pInterface, const void *pvBuf, size_t cb)
{
    PDRVTAPOS2 pData = PDMINETWORKCONNECTOR_2_DRVTAPOS2(pInterface);
    STAM_COUNTER_INC(&pData->StatPktSent);
    STAM_COUNTER_ADD(&pData->StatPktSentBytes, cb);
    STAM_PROFILE_START(&pData->StatTransmit, a);

#ifdef LOG_ENABLED
    uint64_t u64Now = RTTimeProgramNanoTS();
    LogFlow(("drvTAPOs2Send: %-4d bytes at %llu ns  deltas: r=%llu t=%llu\n",
             cb, u64Now, u64Now - pData->u64LastReceiveTS, u64Now - pData->u64LastTransferTS));
    pData->u64LastTransferTS = u64Now;
#endif
    Log2(("drvTAPOs2Send: pvBuf=%p cb=%#x\n"
          "%.*Vhxd\n",
          pvBuf, cb, cb, pvBuf));

    ULONG UnusedParms[10] = { 0,0,0,0, 0,0,0,0, 0,0 };
    ULONG cbParms = sizeof(UnusedParms);
    ULONG cbData = cb;
    int rc = DosDevIOCtl(pData->FileDevice, PROT_CATEGORY, TAP_WRITE_PACKET,
                         &UnusedParms[0], cbParms, &cbParms,
                         pvBuf, cbData, &cbData);
    if (rc)
        rc = RTErrConvertFromOS2(rc);

    STAM_PROFILE_STOP(&pData->StatTransmit, a);
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
static DECLCALLBACK(void) drvTAPOs2SetPromiscuousMode(PPDMINETWORKCONNECTOR pInterface, bool fPromiscuous)
{
    LogFlow(("drvTAPOs2SetPromiscuousMode: fPromiscuous=%d\n", fPromiscuous));
    /* nothing to do */
}


/**
 * Notification on link status changes.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   enmLinkState    The new link state.
 * @thread  EMT
 */
static DECLCALLBACK(void) drvTAPOs2NotifyLinkChanged(PPDMINETWORKCONNECTOR pInterface, PDMNETWORKLINKSTATE enmLinkState)
{
    LogFlow(("drvNATNotifyLinkChanged: enmLinkState=%d\n", enmLinkState));
    /** @todo take action on link down and up. Stop the polling and such like. */
}


/**
 * More receive buffer has become available.
 *
 * This is called when the NIC frees up receive buffers.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @thread  EMT
 */
static DECLCALLBACK(void) drvTAPOs2NotifyCanReceive(PPDMINETWORKCONNECTOR pInterface)
{
    PDRVTAPOS2 pData = PDMINETWORKCONNECTOR_2_DRVTAPOS2(pInterface);

    LogFlow(("drvTAPOs2NotifyCanReceive:\n"));
    /* ensure we wake up only once */
    if (ASMAtomicXchgU32(&pData->fOutOfSpace, false))
        RTSemEventSignal(pData->EventOutOfSpace);
}


/**
 * Asynchronous I/O thread for handling receive.
 *
 * @returns VINF_SUCCESS (ignored).
 * @param   pDrvIns         The driver instance.
 * @param   pThread         The PDM thread structure.
 */
static DECLCALLBACK(int) drvTAPOs2AsyncIoThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVTAPOS2 pData = PDMINS2DATA(pDrvIns, PDRVTAPOS2);
    LogFlow(("drvTAPOs2AsyncIoThread: pData=%p\n", pData));
    Assert(pThread->enmState == PDMTHREADSTATE_INITIALIZING);
    

    /*
     * Outer loop.
     */
    for (;;)
    {
        /*
         *
         */
        PDMR3ThreadSuspend(pThread);
        if (pThread->enmState != PDMTHREADSTATE_RESUMING)
            break;

        {
        }
    }




    STAM_PROFILE_ADV_START(&pData->StatReceive, a);

    /*
     * Polling loop.
     */
    for (;;)
    {
        /*
         * Read/wait the frame.
         */
        char    achBuf[4096];
        ULONG   cbParm = ;
        ULONG   cbRead = 0;
        int     LanNumber;

        int rc = DosDevIOCtl(pData->FileDevice, PROT_CATEGORY, TAP_CANCEL_READ,
                             &UnusedParms[0], cbParm, &cbParm,
                             &achBuf[0], cbRead, &cbRead);
        if (rc == NO_ERROR)
        {
            AssertMsg(cbRead <= 1536, ("cbRead=%d\n", cbRead));

            /*
             * Wait for the device to have space for this frame.
             */
            size_t cbMax = pData->pPort->pfnCanReceive(pData->pPort);
            if (cbMax < cbRead)
            {
                /** @todo receive overflow handling needs serious improving! */
                STAM_PROFILE_ADV_STOP(&pData->StatReceive, a);
                STAM_PROFILE_START(&pData->StatRecvOverflows, b);
                while (   cbMax < cbRead
                       && pData->enmState != ASYNCSTATE_TERMINATE)
                {
                    LogFlow(("drvTAPOs2AsyncIoThread: cbMax=%d cbRead=%d waiting...\n", cbMax, cbRead));
#if 1
                    /* We get signalled by the network driver. 50ms is just for sanity */
                    ASMAtomicXchgU32(&pData->fOutOfSpace, true);
                    RTSemEventWait(pData->EventOutOfSpace, 50);
#else
                    RTThreadSleep(1);
#endif
                    cbMax = pData->pPort->pfnCanReceive(pData->pPort);
                }
                ASMAtomicXchgU32(&pData->fOutOfSpace, false);
                STAM_PROFILE_STOP(&pData->StatRecvOverflows, b);
                STAM_PROFILE_ADV_START(&pData->StatReceive, a);
                if (pData->enmState == ASYNCSTATE_TERMINATE)
                    break;
            }

            /*
             * Pass the data up.
             */
#ifdef LOG_ENABLED
            uint64_t u64Now = RTTimeProgramNanoTS();
            LogFlow(("drvTAPOs2AsyncIoThread: %-4d bytes at %llu ns  deltas: r=%llu t=%llu\n",
                     cbRead, u64Now, u64Now - pData->u64LastReceiveTS, u64Now - pData->u64LastTransferTS));
            pData->u64LastReceiveTS = u64Now;
#endif
            Log2(("drvTAPOs2AsyncIoThread: cbRead=%#x\n"
                  "%.*Vhxd\n",
                  cbRead, cbRead, achBuf));
            STAM_COUNTER_INC(&pData->StatPktRecv);
            STAM_COUNTER_ADD(&pData->StatPktRecvBytes, cbRead);
            rc = pData->pPort->pfnReceive(pData->pPort, achBuf, cbRead);
            AssertRC(rc);
        }
        else
        {
            LogFlow(("drvTAPOs2AsyncIoThread: RTFileRead -> %Vrc\n", rc));
            if (rc == VERR_INVALID_HANDLE)
                break;
            RTThreadYield();
        }
    }

    LogFlow(("drvTAPOs2AsyncIoThread: returns %Vrc\n", VINF_SUCCESS));
    STAM_PROFILE_ADV_STOP(&pData->StatReceive, a);
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
static DECLCALLBACK(void *) drvTAPOs2QueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVTAPOS2 pData = PDMINS2DATA(pDrvIns, PDRVTAPOS2);
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
static DECLCALLBACK(void) drvTAPOs2Destruct(PPDMDRVINS pDrvIns)
{
    LogFlow(("drvTAPOs2Destruct\n"));
    PDRVTAPOS2 pData = PDMINS2DATA(pDrvIns, PDRVTAPOS2);

    /*
     * Destroy the event semaphore.
     */
    if (pData->EventOutOfSpace != NIL_RTSEMEVENTMULTI)
    {
        rc = RTSemEventDestroy(pData->EventOutOfSpace);
        AssertRC(rc);
        pData->EventOutOfSpace = NIL_RTSEMEVENTMULTI;
    }
}


/**
 * Construct a TAP network transport driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) drvTAPOs2Construct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVTAPOS2 pData = PDMINS2DATA(pDrvIns, PDRVTAPOS2);

    /*
     * Init the static parts.
     */
    pData->pDrvIns                      = pDrvIns;
    pData->FileDevice                   = NIL_RTFILE;
    pData->Thread                       = NIL_RTTHREAD;
    pData->enmState                     = ASYNCSTATE_RUNNING;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface    = drvTAPOs2QueryInterface;
    /* INetwork */
    pData->INetworkConnector.pfnSend                = drvTAPOs2Send;
    pData->INetworkConnector.pfnSetPromiscuousMode  = drvTAPOs2SetPromiscuousMode;
    pData->INetworkConnector.pfnNotifyLinkChanged   = drvTAPOs2NotifyLinkChanged;
    pData->INetworkConnector.pfnNotifyCanReceive    = drvTAPOs2NotifyCanReceive;

    /*
     * Validate the config.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "Device\0InitProg\0TermProg\0FileHandle\0"))
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES, "");

    /*
     * Check that no-one is attached to us.
     */
    int rc = pDrvIns->pDrvHlp->pfnAttach(pDrvIns, NULL);
    if (rc != VERR_PDM_NO_ATTACHED_DRIVER)
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_DRVINS_NO_ATTACH,
                                N_("Configuration error: Cannot attach drivers to the TAP driver!"));

    /*
     * Query the network port interface.
     */
    pData->pPort = (PPDMINETWORKPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_NETWORK_PORT);
    if (!pData->pPort)
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE,
                                N_("Configuration error: The above device/driver didn't export the network port interface!"));

    /*
     * Read the configuration.
     */
    int32_t iFile;
    rc = CFGMR3QueryS32(pCfgHandle, "FileHandle", &iFile);
    if (VBOX_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Query for \"FileHandle\" 32-bit signed integer failed!"));
    pData->FileDevice = (RTFILE)iFile;
    if (!RTFileIsValid(pData->FileDevice))
        return PDMDrvHlpVMSetError(pDrvIns, VERR_INVALID_HANDLE, RT_SRC_POS,
                                   N_("The TAP file handle %RTfile is not valid!"), pData->FileDevice);

    /*
     * Make sure the descriptor is non-blocking and valid.
     *
     * We should actually query if it's a TAP device, but I haven't
     * found any way to do that.
     */
    if (fcntl(pData->FileDevice, F_SETFL, O_NONBLOCK) == -1)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_HOSTIF_IOCTL, RT_SRC_POS,
                                   N_("Configuration error: Failed to configure /dev/net/tun. errno=%d"), errno);
    Log(("drvTAPOs2Contruct: %d (from fd)\n", pData->FileDevice));
    rc = VINF_SUCCESS;

    /*
     * Create the out-of-space semaphore and the async receiver thread. 
     */
    rc = RTSemEventCreate(&pData->EventOutOfSpace);
    AssertRCReturn(rc, rc);

    rc = PDMDrvHlpThreadCreate(pDrvIns, &pData->pThread, pData, drvTAPOs2AsyncIoThread, drvTAPOs2WakeupThread,
                               0, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "TAP");
    AssertRCReturn(rc, rc);

#ifdef VBOX_WITH_STATISTICS
    /*
     * Statistics.
     */
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatPktSent,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,        "Number of sent packets.",          "/Drivers/TAP%d/Packets/Sent", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatPktSentBytes,  STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,             "Number of sent bytes.",            "/Drivers/TAP%d/Bytes/Sent", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatPktRecv,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,        "Number of received packets.",      "/Drivers/TAP%d/Packets/Received", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatPktRecvBytes,  STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,             "Number of received bytes.",        "/Drivers/TAP%d/Bytes/Received", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatTransmit,      STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL,    "Profiling packet transmit runs.",  "/Drivers/TAP%d/Transmit", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatReceive,       STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL,    "Profiling packet receive runs.",   "/Drivers/TAP%d/Receive", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatRecvOverflows, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_OCCURENCE, "Profiling packet receive overflows.", "/Drivers/TAP%d/RecvOverflows", pDrvIns->iInstance);
#endif /* VBOX_WITH_STATISTICS */

    return rc;
}


/**
 * TAP network transport driver registration record.
 */
const PDMDRVREG g_DrvHostInterface =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "HostInterface",
    /* pszDescription */
    "TAP Network Transport Driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_NETWORK,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVTAPOS2),
    /* pfnConstruct */
    drvTAPOs2Construct,
    /* pfnDestruct */
    drvTAPOs2Destruct,
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
