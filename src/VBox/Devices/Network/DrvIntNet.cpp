/** @file
 *
 * VBox network devices:
 * Internal network transport driver
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
#define LOG_GROUP LOG_GROUP_DRV_INTNET
#include <VBox/pdm.h>
#include <VBox/cfgm.h>
#include <VBox/intnet.h>
#include <VBox/vmm.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/thread.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/time.h>

#include <string.h>

#include "Builtins.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The state of the asynchronous thread.
 */
typedef enum ASYNCSTATE
{
    /** The thread is suspended. */
    ASYNCSTATE_SUSPENDED = 1,
    /** The thread is running. */
    ASYNCSTATE_RUNNING,
    /** The thread must (/has) terminate. */
    ASYNCSTATE_TERMINATE,
    /** The usual 32-bit type blowup. */
    ASYNCSTATE_32BIT_HACK = 0x7fffffff
} ASYNCSTATE;

/**
 * Block driver instance data.
 */
typedef struct DRVINTNET
{
    /** The network interface. */
    PDMINETWORKCONNECTOR    INetworkConnector;
    /** The network interface. */
    PPDMINETWORKPORT        pPort;
    /** Pointer to the driver instance. */
    PPDMDRVINS              pDrvIns;
    /** Interface handle. */
    INTNETIFHANDLE          hIf;
    /** Pointer to the communication buffer. */
    PINTNETBUF              pBuf;
    /** The thread state. */
    ASYNCSTATE volatile     enmState;
    /** Reader thread. */
    RTTHREAD                Thread;
    /** Event semaphore the Thread waits on while the VM is suspended. */
    RTSEMEVENT              EventSuspended;
    /** Indicates that we're in waiting for recieve space to become available. */
    bool volatile           fOutOfSpace;
    /** Event semaphore the Thread sleeps on while polling for more
     * buffer space to become available.
     * @todo We really need the network device to signal this! */
    RTSEMEVENT              EventOutOfSpace;
    /** Set if the link is down.
     * When the link is down all incoming packets will be dropped. */
    bool volatile           fLinkDown;

#ifdef VBOX_WITH_STATISTICS
    /** Profiling packet transmit runs. */
    STAMPROFILE             StatTransmit;
    /** Profiling packet receive runs. */
    STAMPROFILEADV          StatReceive;
    /** Number of receive overflows. */
    STAMPROFILE             StatRecvOverflows;
#endif /* VBOX_WITH_STATISTICS */

#ifdef LOG_ENABLED
    /** The nano ts of the last transfer. */
    uint64_t                u64LastTransferTS;
    /** The nano ts of the last receive. */
    uint64_t                u64LastReceiveTS;
#endif
    /** The network name. */
    char                    szNetwork[INTNET_MAX_NETWORK_NAME];
} DRVINTNET, *PDRVINTNET;


/** Converts a pointer to DRVINTNET::INetworkConnector to a PDRVINTNET. */
#define PDMINETWORKCONNECTOR_2_DRVINTNET(pInterface) ( (PDRVINTNET)((uintptr_t)pInterface - RT_OFFSETOF(DRVINTNET, INetworkConnector)) )


/**
 * Send data to the network.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   pvBuf           Data to send.
 * @param   cb              Number of bytes to send.
 * @thread  EMT
 */
static DECLCALLBACK(int) drvIntNetSend(PPDMINETWORKCONNECTOR pInterface, const void *pvBuf, size_t cb)
{
    PDRVINTNET pThis = PDMINETWORKCONNECTOR_2_DRVINTNET(pInterface);
    STAM_PROFILE_START(&pThis->StatTransmit, a);

#ifdef LOG_ENABLED
    uint64_t u64Now = RTTimeProgramNanoTS();
    LogFlow(("drvIntNetSend: %-4d bytes at %llu ns  deltas: r=%llu t=%llu\n",
             cb, u64Now, u64Now - pThis->u64LastReceiveTS, u64Now - pThis->u64LastTransferTS));
    pThis->u64LastTransferTS = u64Now;
    Log2(("drvIntNetSend: pvBuf=%p cb=%#x\n"
          "%.*Vhxd\n",
          pvBuf, cb, cb, pvBuf));
#endif

    /** @todo copy to send buffer, this is not safe. */
    INTNETIFSENDARGS SendArgs;
    SendArgs.hIf = pThis->hIf;
    SendArgs.pvFrame = pvBuf;
    SendArgs.cbFrame = cb;
    int rc = pThis->pDrvIns->pDrvHlp->pfnSUPCallVMMR0Ex(pThis->pDrvIns, VMMR0_DO_INTNET_IF_SEND, &SendArgs, sizeof(SendArgs));

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
static DECLCALLBACK(void) drvIntNetSetPromiscuousMode(PPDMINETWORKCONNECTOR pInterface, bool fPromiscuous)
{
    PDRVINTNET pThis = PDMINETWORKCONNECTOR_2_DRVINTNET(pInterface);
    INTNETIFSETPROMISCUOUSMODEARGS SetArgs = {0};
    SetArgs.hIf          = pThis->hIf;
    SetArgs.fPromiscuous = fPromiscuous;
    int rc = pThis->pDrvIns->pDrvHlp->pfnSUPCallVMMR0Ex(pThis->pDrvIns, VMMR0_DO_INTNET_IF_SET_PROMISCUOUS_MODE, &SetArgs, sizeof(SetArgs));
    LogFlow(("drvIntNetSetPromiscuousMode: fPromiscuous=%d\n", fPromiscuous));
    AssertRC(rc);
}


/**
 * Notification on link status changes.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   enmLinkState    The new link state.
 * @thread  EMT
 */
static DECLCALLBACK(void) drvIntNetNotifyLinkChanged(PPDMINETWORKCONNECTOR pInterface, PDMNETWORKLINKSTATE enmLinkState)
{
    PDRVINTNET pThis = PDMINETWORKCONNECTOR_2_DRVINTNET(pInterface);
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
    LogFlow(("drvIntNetNotifyLinkChanged: enmLinkState=%d %d->%d\n", enmLinkState, pThis->fLinkDown, fLinkDown));
    ASMAtomicXchgSize(&pThis->fLinkDown, fLinkDown);
}


/**
 * More receive buffer has become available.
 *
 * This is called when the NIC frees up receive buffers.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @remark  This function isn't called by pcnet nor yet.
 * @thread  EMT
 */
static DECLCALLBACK(void) drvIntNetNotifyCanReceive(PPDMINETWORKCONNECTOR pInterface)
{
    PDRVINTNET pThis = PDMINETWORKCONNECTOR_2_DRVINTNET(pInterface);
    if (pThis->fOutOfSpace)
    {
        LogFlow(("drvIntNetNotifyCanReceive: signaling\n"));
        RTSemEventSignal(pThis->EventOutOfSpace);
    }
}


/**
 * Wait for space to become available up the driver/device chain.
 *
 * @returns VINF_SUCCESS if space is available.
 * @returns VERR_STATE_CHANGED if the state changed.
 * @returns VBox status code on other errors.
 * @param   pThis       Pointer to the instance data.
 * @param   cbFrame     The frame size.
 */
static int drvIntNetAsyncIoWaitForSpace(PDRVINTNET pThis, size_t cbFrame)
{
    LogFlow(("drvIntNetAsyncIoWaitForSpace: cbFrame=%zu\n", cbFrame));
    STAM_PROFILE_ADV_STOP(&pThis->StatReceive, a);
    STAM_PROFILE_START(&pData->StatRecvOverflows, b);

    ASMAtomicXchgSize(&pThis->fOutOfSpace, true);
    int rc;
    unsigned cYields = 0;
    for (;;)
    {
        /* yield/sleep */
        if (   !RTThreadYield()
            || ++cYields % 100 == 0)
        {
            /** @todo we need a callback from the device which can wake us up here. */
            rc = RTSemEventWait(pThis->EventOutOfSpace, 1);
            if (    VBOX_FAILURE(rc)
                &&  rc != VERR_TIMEOUT)
                break;
        }
        if (pThis->enmState != ASYNCSTATE_RUNNING)
        {
            rc = VERR_STATE_CHANGED;
            break;
        }

        /* retry */
        size_t cbMax = pThis->pPort->pfnCanReceive(pThis->pPort);
        if (cbMax >= cbFrame)
        {
            rc = VINF_SUCCESS;
            break;
        }
    }
    ASMAtomicXchgSize(&pThis->fOutOfSpace, false);

    STAM_PROFILE_STOP(&pThis->StatRecvOverflows, b);
    STAM_PROFILE_ADV_START(&pThis->StatReceive, a);
    LogFlow(("drvIntNetAsyncIoWaitForSpace: returns %Vrc\n", rc));
    return rc;
}


/**
 * Executes async I/O (RUNNING mode).
 *
 * @returns VERR_STATE_CHANGED if the state changed.
 * @returns Appropriate VBox status code (error) on fatal error.
 * @param   pThis       The driver instance data.
 */
static int drvIntNetAsyncIoRun(PDRVINTNET pThis)
{
    PPDMDRVINS pDrvIns = pThis->pDrvIns;
    LogFlow(("drvIntNetAsyncIoRun: pThis=%p\n", pThis));

    /*
     * The running loop - processing received data and waiting for more to arrive.
     */
    STAM_PROFILE_ADV_START(&pThis->StatReceive, a);
    PINTNETBUF      pBuf     = pThis->pBuf;
    PINTNETRINGBUF  pRingBuf = &pThis->pBuf->Recv;
    for (;;)
    {
        /*
         * Process the receive buffer.
         */
        while (INTNETRingGetReadable(pRingBuf) > 0)
        {
            /*
             * Check the state and then inspect the packet.
             */
            if (pThis->enmState != ASYNCSTATE_RUNNING)
            {
                STAM_PROFILE_ADV_STOP(&pThis->StatReceive, a);
                LogFlow(("drvIntNetAsyncIoRun: returns VERR_STATE_CHANGED (state changed - #0)\n"));
                return VERR_STATE_CHANGED;
            }

            PINTNETHDR pHdr = (PINTNETHDR)((uintptr_t)pBuf + pRingBuf->offRead);
            Log2(("pHdr=%p offRead=%#x: %.8Rhxs\n", pHdr, pRingBuf->offRead, pHdr));
            if (    pHdr->u16Type == INTNETHDR_TYPE_FRAME
                &&  !pThis->fLinkDown)
            {
                /*
                 * Check if there is room for the frame and pass it up.
                 */
                size_t cbFrame = pHdr->cbFrame;
                size_t cbMax = pThis->pPort->pfnCanReceive(pThis->pPort);
                if (cbMax >= cbFrame)
                {
#ifdef LOG_ENABLED
                    uint64_t u64Now = RTTimeProgramNanoTS();
                    LogFlow(("drvIntNetAsyncIoRun: %-4d bytes at %llu ns  deltas: r=%llu t=%llu\n",
                             cbFrame, u64Now, u64Now - pThis->u64LastReceiveTS, u64Now - pThis->u64LastTransferTS));
                    pThis->u64LastReceiveTS = u64Now;
                    Log2(("drvIntNetAsyncIoRun: cbFrame=%#x\n"
                          "%.*Vhxd\n",
                          cbFrame, cbFrame, INTNETHdrGetFramePtr(pHdr, pBuf)));
#endif
                    int rc = pThis->pPort->pfnReceive(pThis->pPort, INTNETHdrGetFramePtr(pHdr, pBuf), cbFrame);
                    AssertRC(rc);

                    /* skip to the next frame. */
                    INTNETRingSkipFrame(pBuf, pRingBuf);
                }
                else
                {
                    /*
                     * Wait for sufficient space to become available and then retry.
                     */
                    int rc = drvIntNetAsyncIoWaitForSpace(pThis, cbFrame);
                    if (VBOX_FAILURE(rc))
                    {
                        STAM_PROFILE_ADV_STOP(&pThis->StatReceive, a);
                        LogFlow(("drvIntNetAsyncIoRun: returns %Vrc (wait-for-space)\n", rc));
                        return rc;
                    }
                }
            }
            else
            {
                /*
                 * Link down or unknown frame - skip to the next frame.
                 */
                AssertMsg(pHdr->u16Type == INTNETHDR_TYPE_FRAME, ("Unknown frame type %RX16! offRead=%#x\n",
                                                                  pHdr->u16Type, pRingBuf->offRead));
                INTNETRingSkipFrame(pBuf, pRingBuf);
            }
        } /* while more received data */

        /*
         * Wait for data, checking the state before we block.
         */
        if (pThis->enmState != ASYNCSTATE_RUNNING)
        {
            STAM_PROFILE_ADV_STOP(&pThis->StatReceive, a);
            LogFlow(("drvIntNetAsyncIoRun: returns VINF_SUCCESS (state changed - #1)\n"));
            return VERR_STATE_CHANGED;
        }
        INTNETIFWAITARGS WaitArgs;
        WaitArgs.hIf      = pThis->hIf;
        WaitArgs.cMillies = 30000;      /* don't wait forever, timeout now and then. */
        STAM_PROFILE_ADV_STOP(&pThis->StatReceive, a);
        int rc = pDrvIns->pDrvHlp->pfnSUPCallVMMR0Ex(pDrvIns, VMMR0_DO_INTNET_IF_WAIT, &WaitArgs, sizeof(WaitArgs));
        if (    VBOX_FAILURE(rc)
            &&  rc != VERR_TIMEOUT
            &&  rc != VERR_INTERRUPTED)
        {
            LogFlow(("drvIntNetAsyncIoRun: returns %Vrc\n", rc));
            return rc;
        }
        STAM_PROFILE_ADV_START(&pThis->StatReceive, a);
    }
}


/**
 * Asynchronous I/O thread for handling receive.
 *
 * @returns VINF_SUCCESS (ignored).
 * @param   ThreadSelf      Thread handle.
 * @param   pvUser          Pointer to a DRVINTNET structure.
 */
static DECLCALLBACK(int) drvIntNetAsyncIoThread(RTTHREAD ThreadSelf, void *pvUser)
{
    PDRVINTNET pThis = (PDRVINTNET)pvUser;
    LogFlow(("drvIntNetAsyncIoThread: pThis=%p\n", pThis));
    STAM_PROFILE_ADV_START(&pThis->StatReceive, a);

    /*
     * The main loop - acting on state.
     */
    for (;;)
    {
        ASYNCSTATE enmState = pThis->enmState;
        switch (enmState)
        {
            case ASYNCSTATE_SUSPENDED:
            {
                int rc = RTSemEventWait(pThis->EventSuspended, 30000);
                if (    VBOX_FAILURE(rc)
                    &&  rc != VERR_TIMEOUT)
                {
                    LogFlow(("drvIntNetAsyncIoThread: returns %Vrc\n", rc));
                    return rc;
                }
                break;
            }

            case ASYNCSTATE_RUNNING:
            {
                int rc = drvIntNetAsyncIoRun(pThis);
                if (    rc != VERR_STATE_CHANGED
                    &&  VBOX_FAILURE(rc))
                {
                    LogFlow(("drvIntNetAsyncIoThread: returns %Vrc\n", rc));
                    return rc;
                }
                break;
            }

            default:
                AssertMsgFailed(("Invalid state %d\n", enmState));
            case ASYNCSTATE_TERMINATE:
                LogFlow(("drvIntNetAsyncIoThread: returns VINF_SUCCESS\n"));
                return VINF_SUCCESS;
        }
    }
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
static DECLCALLBACK(void *) drvIntNetQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVINTNET pThis = PDMINS2DATA(pDrvIns, PDRVINTNET);
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
 * Power Off notification.
 *
 * @param   pDrvIns     The driver instance.
 */
static DECLCALLBACK(void) drvIntNetPowerOff(PPDMDRVINS pDrvIns)
{
    LogFlow(("drvIntNetPowerOff\n"));
    PDRVINTNET pThis = PDMINS2DATA(pDrvIns, PDRVINTNET);
    ASMAtomicXchgSize(&pThis->enmState, ASYNCSTATE_SUSPENDED);
}


/**
 * Resume notification.
 *
 * @param   pDrvIns     The driver instance.
 */
static DECLCALLBACK(void) drvIntNetResume(PPDMDRVINS pDrvIns)
{
    LogFlow(("drvIntNetPowerResume\n"));
    PDRVINTNET pThis = PDMINS2DATA(pDrvIns, PDRVINTNET);
    ASMAtomicXchgSize(&pThis->enmState, ASYNCSTATE_RUNNING);
    RTSemEventSignal(pThis->EventSuspended);
}


/**
 * Suspend notification.
 *
 * @param   pDrvIns     The driver instance.
 */
static DECLCALLBACK(void) drvIntNetSuspend(PPDMDRVINS pDrvIns)
{
    LogFlow(("drvIntNetPowerSuspend\n"));
    PDRVINTNET pThis = PDMINS2DATA(pDrvIns, PDRVINTNET);
    ASMAtomicXchgSize(&pThis->enmState, ASYNCSTATE_SUSPENDED);
}


/**
 * Power On notification.
 *
 * @param   pDrvIns     The driver instance.
 */
static DECLCALLBACK(void) drvIntNetPowerOn(PPDMDRVINS pDrvIns)
{
    LogFlow(("drvIntNetPowerOn\n"));
    PDRVINTNET pThis = PDMINS2DATA(pDrvIns, PDRVINTNET);
    ASMAtomicXchgSize(&pThis->enmState, ASYNCSTATE_RUNNING);
    RTSemEventSignal(pThis->EventSuspended);
}


/**
 * Destruct a driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvIntNetDestruct(PPDMDRVINS pDrvIns)
{
    LogFlow(("drvIntNetDestruct\n"));
    PDRVINTNET pThis = PDMINS2DATA(pDrvIns, PDRVINTNET);

    /*
     * Indicate to the thread that it's time to quit.
     */
    ASMAtomicXchgSize(&pThis->enmState, ASYNCSTATE_TERMINATE);
    ASMAtomicXchgSize(&pThis->fLinkDown, true);
    RTSEMEVENT EventOutOfSpace = pThis->EventOutOfSpace;
    pThis->EventOutOfSpace = NIL_RTSEMEVENT;
    RTSEMEVENT EventSuspended = pThis->EventSuspended;
    pThis->EventSuspended = NIL_RTSEMEVENT;

    /*
     * Close the interface
     */
    if (pThis->hIf != INTNET_HANDLE_INVALID)
    {
        INTNETIFCLOSEARGS CloseArgs = {0};
        CloseArgs.hIf = pThis->hIf;
        pThis->hIf = INTNET_HANDLE_INVALID;
        int rc = pDrvIns->pDrvHlp->pfnSUPCallVMMR0Ex(pDrvIns, VMMR0_DO_INTNET_IF_CLOSE, &CloseArgs, sizeof(CloseArgs));
        AssertRC(rc);
    }

    /*
     * Wait for the thread to terminate.
     */
    if (pThis->Thread != NIL_RTTHREAD)
    {
        if (EventOutOfSpace != NIL_RTSEMEVENT)
            RTSemEventSignal(EventOutOfSpace);
        if (EventSuspended != NIL_RTSEMEVENT)
            RTSemEventSignal(EventSuspended);
        int rc = RTThreadWait(pThis->Thread, 5000, NULL);
        AssertRC(rc);
        pThis->Thread = NIL_RTTHREAD;
    }

    /*
     * Destroy the semaphores.
     */
    if (EventOutOfSpace != NIL_RTSEMEVENT)
        RTSemEventDestroy(EventOutOfSpace);
    if (EventSuspended != NIL_RTSEMEVENT)
        RTSemEventDestroy(EventSuspended);
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
static DECLCALLBACK(int) drvIntNetConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVINTNET pThis = PDMINS2DATA(pDrvIns, PDRVINTNET);

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                      = pDrvIns;
    pThis->hIf                          = INTNET_HANDLE_INVALID;
    pThis->Thread                       = NIL_RTTHREAD;
    pThis->EventSuspended               = NIL_RTSEMEVENT;
    pThis->EventOutOfSpace              = NIL_RTSEMEVENT;
    pThis->enmState                     = ASYNCSTATE_SUSPENDED;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface    = drvIntNetQueryInterface;
    /* INetwork */
    pThis->INetworkConnector.pfnSend                = drvIntNetSend;
    pThis->INetworkConnector.pfnSetPromiscuousMode  = drvIntNetSetPromiscuousMode;
    pThis->INetworkConnector.pfnNotifyLinkChanged   = drvIntNetNotifyLinkChanged;
    pThis->INetworkConnector.pfnNotifyCanReceive    = drvIntNetNotifyCanReceive;

    /*
     * Validate the config.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "Network\0ReceiveBufferSize\0SendBufferSize\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

    /*
     * Check that no-one is attached to us.
     */
    int rc = pDrvIns->pDrvHlp->pfnAttach(pDrvIns, NULL);
    if (rc != VERR_PDM_NO_ATTACHED_DRIVER)
    {
        AssertMsgFailed(("Configuration error: Cannot attach drivers to the TAP driver!\n"));
        return VERR_PDM_DRVINS_NO_ATTACH;
    }

    /*
     * Query the network port interface.
     */
    pThis->pPort = (PPDMINETWORKPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_NETWORK_PORT);
    if (!pThis->pPort)
    {
        AssertMsgFailed(("Configuration error: the above device/driver didn't export the network port interface!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }

    /*
     * Read the configuration.
     */
    INTNETOPENARGS OpenArgs;
    memset(&OpenArgs, 0, sizeof(OpenArgs));
    rc = CFGMR3QueryString(pCfgHandle, "Network", OpenArgs.szNetwork, sizeof(OpenArgs.szNetwork));
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: query for \"Network\" string return %Vra.\n", rc));
        return rc;
    }
    strcpy(pThis->szNetwork, OpenArgs.szNetwork);

    rc = CFGMR3QueryU32(pCfgHandle, "ReceiveBufferSize", &OpenArgs.cbRecv);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        OpenArgs.cbRecv = _256K;
    else if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: query for \"ReceiveBufferSize\" uint32_t return %Vra.\n", rc));
        return rc;
    }

    rc = CFGMR3QueryU32(pCfgHandle, "SendBufferSize", &OpenArgs.cbSend);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        OpenArgs.cbSend = _4K;
    else if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: query for \"SendBufferSize\" uint32_t return %Vra.\n", rc));
        return rc;
    }

    /*
     * Create the event semaphores
     */
    rc = RTSemEventCreate(&pThis->EventSuspended);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = RTSemEventCreate(&pThis->EventOutOfSpace);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Create the interface.
     */
    OpenArgs.hIf = INTNET_HANDLE_INVALID;
    rc = pDrvIns->pDrvHlp->pfnSUPCallVMMR0Ex(pDrvIns, VMMR0_DO_INTNET_OPEN, &OpenArgs, sizeof(OpenArgs));
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to open/create the network '%s', cbRecv=%RU32, cbSend=%RU32. rc=%VRc\n",
                         pThis->szNetwork, OpenArgs.cbRecv, OpenArgs.cbSend, rc));
        return rc;
    }
    AssertRelease(OpenArgs.hIf != INTNET_HANDLE_INVALID);
    pThis->hIf = OpenArgs.hIf;
    Log(("IntNet%d: hIf=%RX32 '%s'\n", pDrvIns->iInstance, pThis->hIf, pThis->szNetwork));

    /*
     * Get default buffer.
     */
    INTNETIFGETRING3BUFFERARGS GetRing3BufferArgs = {0};
    GetRing3BufferArgs.hIf = pThis->hIf;
    GetRing3BufferArgs.pRing3Buf = NULL;
    rc = pDrvIns->pDrvHlp->pfnSUPCallVMMR0Ex(pDrvIns, VMMR0_DO_INTNET_IF_GET_RING3_BUFFER, &GetRing3BufferArgs, sizeof(GetRing3BufferArgs));
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to get ring-3 buffer for the newly created interface to '%s'. rc=%VRc\n",
                         pThis->szNetwork, rc));
        return rc;
    }
    AssertRelease(VALID_PTR(GetRing3BufferArgs.pRing3Buf));
    pThis->pBuf = GetRing3BufferArgs.pRing3Buf;

    /*
     * Create the async I/O thread.
     */
    rc = RTThreadCreate(&pThis->Thread, drvIntNetAsyncIoThread, pThis, _128K, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "INTNET");
    if (VBOX_FAILURE(rc))
    {
        AssertRC(rc);
        return rc;
    }

    char szStatName[64];
    RTStrPrintf(szStatName, sizeof(szStatName), "/Net/IntNet%d/Bytes/Received", pDrvIns->iInstance);
    pDrvIns->pDrvHlp->pfnSTAMRegister(pDrvIns, &pThis->pBuf->cbStatRecv,        STAMTYPE_COUNTER, szStatName,   STAMUNIT_BYTES,         "Number of received bytes.");
    RTStrPrintf(szStatName, sizeof(szStatName), "/Net/IntNet%d/Bytes/Sent",     pDrvIns->iInstance);
    pDrvIns->pDrvHlp->pfnSTAMRegister(pDrvIns, &pThis->pBuf->cbStatSend,        STAMTYPE_COUNTER, szStatName,   STAMUNIT_BYTES,         "Number of sent bytes.");
    RTStrPrintf(szStatName, sizeof(szStatName), "/Net/IntNet%d/Packets/Received", pDrvIns->iInstance);
    pDrvIns->pDrvHlp->pfnSTAMRegister(pDrvIns, &pThis->pBuf->cStatRecvs,        STAMTYPE_COUNTER, szStatName,   STAMUNIT_OCCURENCES,    "Number of received packets.");
    RTStrPrintf(szStatName, sizeof(szStatName), "/Net/IntNet%d/Packets/Sent",   pDrvIns->iInstance);
    pDrvIns->pDrvHlp->pfnSTAMRegister(pDrvIns, &pThis->pBuf->cStatSends,        STAMTYPE_COUNTER, szStatName,   STAMUNIT_OCCURENCES,    "Number of sent packets.");
    RTStrPrintf(szStatName, sizeof(szStatName), "/Net/IntNet%d/Packets/Lost",   pDrvIns->iInstance);
    pDrvIns->pDrvHlp->pfnSTAMRegister(pDrvIns, &pThis->pBuf->cStatLost,         STAMTYPE_COUNTER, szStatName,   STAMUNIT_OCCURENCES,    "Number of lost packets.");
    RTStrPrintf(szStatName, sizeof(szStatName), "/Net/IntNet%d/YieldOk",        pDrvIns->iInstance);
    pDrvIns->pDrvHlp->pfnSTAMRegister(pDrvIns, &pThis->pBuf->cStatYieldsOk,     STAMTYPE_COUNTER, szStatName,   STAMUNIT_OCCURENCES,    "Number of times yielding fixed an overflow.");
    RTStrPrintf(szStatName, sizeof(szStatName), "/Net/IntNet%d/YieldNok",       pDrvIns->iInstance);
    pDrvIns->pDrvHlp->pfnSTAMRegister(pDrvIns, &pThis->pBuf->cStatYieldsNok,    STAMTYPE_COUNTER, szStatName,   STAMUNIT_OCCURENCES,    "Number of times yielding didn't help fix an overflow.");

#ifdef VBOX_WITH_STATISTICS
    RTStrPrintf(szStatName, sizeof(szStatName), "/Net/IntNet%d/Receive",        pDrvIns->iInstance);
    pDrvIns->pDrvHlp->pfnSTAMRegister(pDrvIns, &pThis->StatReceive,             STAMTYPE_PROFILE, szStatName,   STAMUNIT_TICKS_PER_CALL, "Profiling packet receive runs.");
    RTStrPrintf(szStatName, sizeof(szStatName), "/Net/IntNet%d/RecvOverflows",  pDrvIns->iInstance);
    pDrvIns->pDrvHlp->pfnSTAMRegister(pDrvIns, &pThis->StatRecvOverflows,       STAMTYPE_PROFILE, szStatName,   STAMUNIT_TICKS_PER_OCCURENCE, "Profiling packet receive overflows.");
    RTStrPrintf(szStatName, sizeof(szStatName), "/Net/IntNet%d/Transmit",       pDrvIns->iInstance);
    pDrvIns->pDrvHlp->pfnSTAMRegister(pDrvIns, &pThis->StatTransmit,            STAMTYPE_PROFILE, szStatName,   STAMUNIT_TICKS_PER_CALL, "Profiling packet transmit runs.");
#endif

    return rc;
}


/**
 * Internal networking transport driver registration record.
 */
const PDMDRVREG g_DrvIntNet =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "IntNet",
    /* pszDescription */
    "Internal Networking Transport Driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_NETWORK,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVINTNET),
    /* pfnConstruct */
    drvIntNetConstruct,
    /* pfnDestruct */
    drvIntNetDestruct,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    drvIntNetPowerOn,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    drvIntNetSuspend,
    /* pfnResume */
    drvIntNetResume,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    drvIntNetPowerOff
};

