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
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_TUN
#include <VBox/pdmdrv.h>

#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/alloca.h>
#include <iprt/asm.h>
#include <iprt/semaphore.h>

#include "Builtins.h"

#define INCL_BASE
#include <os2.h>
#include "DrvTAPOs2.h"



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
    RTFILE                  hDevice;
    /** Out LAN number. */
    int32_t                 iLan;
    /** The LAN number we're connected to. -1 if not connected. */
    int32_t                 iConnectedTo;
    /** Receiver thread. */
    PPDMTHREAD              pThread;
    /** Event semaphore for blocking on receive. */
    RTSEMEVENT              EventOutOfSpace;
    /** We are checking or waiting for more receive buffers. */
    bool volatile           fMaybeOutOfSpace;
    /** Set if the link is down.
     * When the link is down all incoming packets will be dropped. */
    bool volatile           fLinkDown;
    /** The log and thread name. */
    char                    szName[16];
    /** The \DEV\TAP$ device name. */
    char                    szDevice[32];

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
    PDRVTAPOS2 pThis = PDMINETWORKCONNECTOR_2_DRVTAPOS2(pInterface);
    STAM_COUNTER_INC(&pThis->StatPktSent);
    STAM_COUNTER_ADD(&pThis->StatPktSentBytes, cb);
    STAM_PROFILE_START(&pThis->StatTransmit, a);

    /* 
     * If the pvBuf is a high address, we'll have to copy it onto a 
     * stack buffer of the tap driver will trap.
     */
    if ((uintptr_t)pvBuf >= _1M*512)
    {
        void *pvBufCopy = alloca(cb);
        memcpy(pvBufCopy, pvBuf, cb);
        pvBuf = pvBufCopy;
    }

#ifdef LOG_ENABLED
    uint64_t u64Now = RTTimeProgramNanoTS();
    LogFlow(("%s: Send: %-4d bytes at %RU64 ns  deltas: recv=%RU64 xmit=%RU64\n", pThis->szName,
             cb, u64Now, u64Now - pThis->u64LastReceiveTS, u64Now - pThis->u64LastTransferTS));
    pThis->u64LastTransferTS = u64Now;
    Log2(("%s Send: pvBuf=%p cb=%#zx\n"
          "%.*Vhxd\n",
          pThis->szName, pvBuf, cb, cb, pvBuf));
#endif

    ULONG Parm[2] = { ~0UL, ~0UL }; /* mysterious output */
    ULONG cbParm = sizeof(Parm);
    ULONG cbData = cb;
    int rc = DosDevIOCtl(pThis->hDevice, PROT_CATEGORY, TAP_WRITE_PACKET,
                         &Parm[0], cbParm, &cbParm,
                         (void *)pvBuf, cbData, &cbData);
    if (RT_UNLIKELY(rc || Parm[0]))
    {
        static unsigned cComplaints = 0;
        if (cComplaints++ < 256)
            LogRel(("%s: send failed. rc=%d Parm={%ld,%ld} cb=%d\n", 
                    pThis->szName, rc, Parm[0], Parm[1], cb));
        if (rc)
            rc = RTErrConvertFromOS2(rc);
        else
            rc = VERR_IO_GEN_FAILURE;
    }
    Log3(("%s: Send completed %d ns\n", pThis->szName, RTTimeProgramNanoTS() - pThis->u64LastTransferTS));

    STAM_PROFILE_STOP(&pThis->StatTransmit, a);
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
    PDRVTAPOS2 pThis = PDMINETWORKCONNECTOR_2_DRVTAPOS2(pInterface);
    LogFlow(("%s: SetPromiscuousMode: fPromiscuous=%d\n", pThis->szName, fPromiscuous));
    NOREF(pThis);
    /** @todo is it always in promiscuous mode? */
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
    PDRVTAPOS2 pThis = PDMINETWORKCONNECTOR_2_DRVTAPOS2(pInterface);
    bool fLinkDown;
    switch (enmLinkState)
    {
        case PDMNETWORKLINKSTATE_DOWN:
        case PDMNETWORKLINKSTATE_DOWN_RESUME:
            fLinkDown = true;
            break;
        default:
            AssertMsgFailed(("enmLinkState=%d\n", enmLinkState));
        case PDMNETWORKLINKSTATE_UP:
            fLinkDown = false;
            break;
    }
    LogFlow(("%s: NotifyLinkChanged: enmLinkState=%d %d->%d\n", pThis->szName, pThis->fLinkDown, fLinkDown));
    ASMAtomicXchgBool(&pThis->fLinkDown, fLinkDown);
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
    PDRVTAPOS2 pThis = PDMINETWORKCONNECTOR_2_DRVTAPOS2(pInterface);

    /* don't waste time signalling the semaphore unnecessary */
    if (!pThis->fMaybeOutOfSpace)
        LogFlow(("%s: NotifyCanReceive: fMaybeOutOfSpace=false\n", pThis->szName));
    else
    {
        LogFlow(("%s: NotifyCanReceive: fMaybeOutOfSpace=true\n", pThis->szName));
        RTSemEventSignal(pThis->EventOutOfSpace);
    }
}


/**
 * Receiver thread.
 *
 * @returns VBox status code. Returning failure will naturally terminate the thread.
 * @param   pDrvIns     The pcnet device instance.
 * @param   pThread     The thread.
 */
static DECLCALLBACK(int) drvTAPOs2ReceiveThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVTAPOS2 pThis = PDMINS2DATA(pDrvIns, PDRVTAPOS2);

    /*
     * No initialization work to do, just return immediately.
     */
    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;
    Assert(pThread->enmState == PDMTHREADSTATE_RUNNING);

    /*
     * Loop while the thread is running, quit immediately when 
     * we're supposed to suspend or terminate.
     */
    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        /*
         * Read a frame, this will block for a while if nothing to read.
         */
        char    abBuf[4096];
        ULONG   Parm[2] = { ~0UL, ~0UL };   /* mysterious output */
        ULONG   cbParm = sizeof(Parm);      /* this one is actually ignored... */
        ULONG   cbBuf = sizeof(abBuf);

        int rc = DosDevIOCtl(pThis->hDevice, PROT_CATEGORY, TAP_READ_PACKET,
                             &Parm[0], cbParm, &cbParm,
                             &abBuf[0], cbBuf, &cbBuf);
        if (pThread->enmState != PDMTHREADSTATE_RUNNING)
            break;
        const size_t cbRead = Parm[1];
        if (    !rc
            &&  !Parm[0]
            &&  cbRead > 0 /* cbRead */)
        {
            AssertMsg(cbRead <= 1536, ("cbRead=%d\n", cbRead));

            /*
             * Wait for the device to have some room.
             */
            ASMAtomicXchgBool(&pThis->fMaybeOutOfSpace, true);
            size_t cbMax = pThis->pPort->pfnCanReceive(pThis->pPort);
            if (cbMax == 0)
            {
                STAM_PROFILE_ADV_STOP(&pThis->StatReceive, a);
                STAM_PROFILE_START(&pThis->StatRecvOverflows, b);
                while (   cbMax == 0
                       && pThread->enmState == PDMTHREADSTATE_RUNNING)
                {
                    LogFlow(("%s: ReceiveThread: cbMax=%d cbRead=%d waiting...\n", pThis->szName, cbMax, cbRead));
                    RTSemEventWait(pThis->EventOutOfSpace, 50);
                    cbMax = pThis->pPort->pfnCanReceive(pThis->pPort);
                }
                STAM_PROFILE_STOP(&pThis->StatRecvOverflows, b);
                STAM_PROFILE_ADV_START(&pThis->StatReceive, a);
            }
            ASMAtomicXchgBool(&pThis->fMaybeOutOfSpace, false);
            if (pThread->enmState != PDMTHREADSTATE_RUNNING)
                break; /* just drop it, no big deal. */

            /*
             * Pass the data up.
             */
#ifdef LOG_ENABLED
            uint64_t u64Now = RTTimeProgramNanoTS();
            LogFlow(("%s: ReceiveThread: %-4d bytes at %RU64 ns  deltas: recv=%RU64 xmit=%RU64\n", pThis->szName,
                     cbRead, u64Now, u64Now - pThis->u64LastReceiveTS, u64Now - pThis->u64LastTransferTS));
            pThis->u64LastReceiveTS = u64Now;
#endif
            Log2(("%s: ReceiveThread: cbRead=%#x\n"
                  "%.*Vhxd\n",
                  pThis->szName, cbRead, cbRead, abBuf));
            STAM_COUNTER_INC(&pThis->StatPktRecv);
            STAM_COUNTER_ADD(&pThis->StatPktRecvBytes, cbRead);
            rc = pThis->pPort->pfnReceive(pThis->pPort, abBuf, cbRead);
            AssertRC(rc);
        }
        /* we'll be returning ~1 per second with no data; rc=0 Parm[0] = 1, Parm[1] = 0. */
        else if (rc)
        {
            LogFlow(("%s: ReceiveThread: DoDevIOCtl -> %s Parm={%ld, %ld}\n", 
                     pThis->szName, rc, Parm[0], Parm[1]));
            rc = RTErrConvertFromOS2(rc);
            if (rc == VERR_INVALID_HANDLE)
                return rc;
            RTThreadYield();
        }
    }

    /* The thread is being suspended or terminated. */
    return VINF_SUCCESS;
}


/**
 * Unblock the send thread so it can respond to a state change.
 *
 * @returns VBox status code.
 * @param   pDrvIns     The pcnet device instance.
 * @param   pThread     The send thread.
 */
static DECLCALLBACK(int) drvTAPOs2WakeupReceiveThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVTAPOS2 pThis = PDMINS2DATA(pDrvIns, PDRVTAPOS2);
    LogFlow(("%s: WakeupReceiveThread\n", pThis->szName));

    /* cancel any pending reads */
    ULONG   Parm[2] = { ~0UL, ~0UL };   /* mysterious output */
    ULONG   cbParm = sizeof(Parm);
    ULONG   Data = pThis->iLan;         /* right? */
    ULONG   cbData = sizeof(Data);
    int orc = DosDevIOCtl(pThis->hDevice, PROT_CATEGORY, TAP_CANCEL_READ,
                          &Parm[0], cbParm, &cbParm,
                          &Data, cbData, &cbData);
    AssertMsg(orc == 0, ("%d\n", orc)); NOREF(orc);

    /* wake it up if it's waiting for receive buffers. */
    if (pThis->fMaybeOutOfSpace)
        RTSemEventSignal(pThis->EventOutOfSpace);

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
    PDRVTAPOS2 pThis = PDMINS2DATA(pDrvIns, PDRVTAPOS2);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_NETWORK_CONNECTOR:
            return &pThis->INetworkConnector;
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
    PDRVTAPOS2 pThis = PDMINS2DATA(pDrvIns, PDRVTAPOS2);
    LogFlow(("%s: Destruct\n", pThis->szName));

    /* PDM will destroy the thread for us, it's suspended right now. */

    /*
     * Destroy the out-of-space event semaphore.
     */
    if (pThis->EventOutOfSpace != NIL_RTSEMEVENTMULTI)
    {
        int rc = RTSemEventDestroy(pThis->EventOutOfSpace);
        AssertRC(rc);
        pThis->EventOutOfSpace = NIL_RTSEMEVENTMULTI;
    }

    /*
     * Disconnect from the lan if we made a connection and close it.
     */
    if (pThis->iConnectedTo != -1)
    {
        ULONG Parm[2] = { ~0UL, ~0UL }; /* mysterious output */
        ULONG cbParm = sizeof(Parm);
        ULONG Data = pThis->iConnectedTo;
        ULONG cbData = sizeof(Data);
        int orc = DosDevIOCtl(pThis->hDevice, PROT_CATEGORY, TAP_DISCONNECT_NIC,
                              &Parm, cbParm, &cbParm,
                              &Data, cbData, &cbData);
        if (    orc
            ||  Parm[0])
            LogRel(("%s: Failed to disconnect %d from %d! orc=%d Parm={%ld,%ld}\n", 
                    pThis->szName, pThis->iLan, pThis->iConnectedTo, orc, Parm[0], Parm[1]));
        pThis->iConnectedTo = -1;
    }

    if (pThis->hDevice != NIL_RTFILE)
    {
        int rc = RTFileClose(pThis->hDevice);
        AssertRC(rc);
        pThis->hDevice = NIL_RTFILE;
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
    PDRVTAPOS2 pThis = PDMINS2DATA(pDrvIns, PDRVTAPOS2);

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                      = pDrvIns;
    pThis->hDevice                      = NIL_RTFILE;
    pThis->iLan                         = -1;
    pThis->iConnectedTo                 = -1;
    pThis->pThread                      = NULL;
    RTStrPrintf(pThis->szName, sizeof(pThis->szName), "TAP%d", pDrvIns->iInstance);
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface    = drvTAPOs2QueryInterface;
    /* INetwork */
    pThis->INetworkConnector.pfnSend                = drvTAPOs2Send;
    pThis->INetworkConnector.pfnSetPromiscuousMode  = drvTAPOs2SetPromiscuousMode;
    pThis->INetworkConnector.pfnNotifyLinkChanged   = drvTAPOs2NotifyLinkChanged;
    pThis->INetworkConnector.pfnNotifyCanReceive    = drvTAPOs2NotifyCanReceive;

    /*
     * Validate the config.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "Device\0ConnectTo\0"))
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
    pThis->pPort = (PPDMINETWORKPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_NETWORK_PORT);
    if (!pThis->pPort)
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE,
                                N_("Configuration error: The above device/driver didn't export the network port interface!"));

    /*
     * Read the configuration.
     */
    rc = CFGMR3QueryString(pCfgHandle, "Device", &pThis->szDevice[0], sizeof(pThis->szDevice));
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        strcpy(pThis->szDevice, "\\DEV\\TAP$");
    else if (VBOX_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Query for \"Device\" failed!"));

    int32_t iConnectTo;
    rc = CFGMR3QueryS32(pCfgHandle, "ConnectTo", &iConnectTo);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        iConnectTo = -1;
    else if (VBOX_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Query for \"ConnectTo\" failed!"));

    /*
     * Open the device.
     * Keep in mind that the destructor is always called!
     */
    rc = RTFileOpen(&pThis->hDevice, pThis->szDevice, RTFILE_O_DENY_NONE | RTFILE_O_READ);
    if (VBOX_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("Failed to open tap device '%s'!"), pThis->szDevice);

    ULONG Parm[2] = { ~0UL, ~0UL }; /* mysterious output */
    ULONG cbParm = sizeof(Parm);
    ULONG Data = ~0UL;
    ULONG cbData = sizeof(Data);
    int orc = DosDevIOCtl(pThis->hDevice, PROT_CATEGORY, TAP_GET_LAN_NUMBER,
                          &Parm, cbParm, &cbParm,
                          &Data, cbData, &cbData);
    if (orc)
        rc = RTErrConvertFromOS2(orc);
    else if (Parm[0])
        rc = VERR_GENERAL_FAILURE;
    if (VBOX_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("Failed to query LanNumber! orc=%d Parm={%ld,%ld}\n"), 
                                   orc, Parm[0], Parm[1]);
    pThis->iLan = (int32_t)Data;
    Log(("%s: iLan=%d Parm[1]=%ld\n", pThis->szName, pThis->iLan, Parm[1]));

    /* 
     * Connect it requested.
     */
    if (iConnectTo != -1)
    {
        if (iConnectTo == pThis->iLan)
            return PDMDrvHlpVMSetError(pDrvIns, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                       N_("Cannot connect to ourself (%d)"), iConnectTo);

        Parm[0] = Parm[1] = ~0UL; /* mysterious output */
        cbParm = sizeof(Parm);
        Data = iConnectTo;
        cbData = sizeof(Data);
        int orc = DosDevIOCtl(pThis->hDevice, PROT_CATEGORY, TAP_CONNECT_NIC,
                              &Parm, cbParm, &cbParm,
                              &Data, cbData, &cbData);
        if (orc)
            rc = RTErrConvertFromOS2(orc);
        else if (Parm[0])
            rc = VERR_GENERAL_FAILURE;
        if (VBOX_FAILURE(rc))
            return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                       N_("Failed to connect %d to %d! orc=%d Parm={%ld,%ld}\n"), 
                                       pThis->iLan, iConnectTo, orc, Parm[0], Parm[1]);
        Log(("%s: Connected to %d\n", pThis->szName, iConnectTo));
        pThis->iConnectedTo = iConnectTo;
    }

    /* 
     * Log the config.
     */
    Parm[0] = Parm[1] = ~0UL; /* mysterious output */
    PDMMAC Mac;
    cbParm = sizeof(Parm);
    cbData = sizeof(Mac);
    orc = DosDevIOCtl(pThis->hDevice, PROT_CATEGORY, TAP_READ_MAC_ADDRESS,
                      &Parm[0], cbParm, &cbParm,
                      &Mac, cbData, &cbData);
    if (    !orc 
        &&  !Parm[0]
      /*&&  !Parm[1]?*/)
        LogRel(("%s: iLan=%d iConnectedTo=%d Mac=%02x:%02x:%02x:%02x:%02x:%02x\n", 
                pThis->szName, pThis->iLan, pThis->iConnectedTo, 
                Mac.au8[0], Mac.au8[1], Mac.au8[2], Mac.au8[3], Mac.au8[4], Mac.au8[5]));
    else
        LogRel(("%s: iLan=%d iConnectedTo Mac=failed - orc=%d Parm={%ld,%ld}\n", 
                pThis->szName, pThis->iLan, pThis->iConnectedTo, Parm[0], Parm[1]));

    /*
     * Create the out-of-space semaphore and the async receiver thread. 
     */
    rc = RTSemEventCreate(&pThis->EventOutOfSpace);
    AssertRCReturn(rc, rc);

    rc = PDMDrvHlpPDMThreadCreate(pDrvIns, &pThis->pThread, pThis, drvTAPOs2ReceiveThread, drvTAPOs2WakeupReceiveThread,
                                  0, RTTHREADTYPE_IO, pThis->szName);
    AssertRCReturn(rc, rc);

#ifdef VBOX_WITH_STATISTICS
    /*
     * Statistics.
     */
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatPktSent,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,        "Number of sent packets.",          "/Drivers/TAP%d/Packets/Sent", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatPktSentBytes,  STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,             "Number of sent bytes.",            "/Drivers/TAP%d/Bytes/Sent", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatPktRecv,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,        "Number of received packets.",      "/Drivers/TAP%d/Packets/Received", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatPktRecvBytes,  STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,             "Number of received bytes.",        "/Drivers/TAP%d/Bytes/Received", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatTransmit,      STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL,    "Profiling packet transmit runs.",  "/Drivers/TAP%d/Transmit", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatReceive,       STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL,    "Profiling packet receive runs.",   "/Drivers/TAP%d/Receive", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatRecvOverflows, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_OCCURENCE, "Profiling packet receive overflows.", "/Drivers/TAP%d/RecvOverflows", pDrvIns->iInstance);
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

