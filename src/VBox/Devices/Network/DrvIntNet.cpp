/* $Id$ */
/** @file
 * DrvIntNet - Internal network transport driver.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_INTNET
#include <VBox/pdmdrv.h>
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
#include <iprt/ctype.h>

#include "../Builtins.h"

#if defined(RT_OS_WINDOWS) && defined(VBOX_WITH_NETFLT)
# include "win/DrvIntNet-win.h"
#endif

#include "DhcpServerRunner.h"

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
    /** The network config interface.
     * Can (in theory at least) be NULL. */
    PPDMINETWORKCONFIG      pConfigIf;
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
    /** Set if the link is down.
     * When the link is down all incoming packets will be dropped. */
    bool volatile           fLinkDown;
    /** Set if data transmission should start immediately and deactivate
     * as late as possible. */
    bool                    fActivateEarlyDeactivateLate;

#ifdef VBOX_WITH_STATISTICS
    /** Profiling packet transmit runs. */
    STAMPROFILE             StatTransmit;
    /** Profiling packet receive runs. */
    STAMPROFILEADV          StatReceive;
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
 * Updates the MAC address on the kernel side.
 *
 * @returns VBox status code.
 * @param   pThis       The driver instance.
 */
static int drvIntNetUpdateMacAddress(PDRVINTNET pThis)
{
    if (!pThis->pConfigIf)
        return VINF_SUCCESS;

    INTNETIFSETMACADDRESSREQ SetMacAddressReq;
    SetMacAddressReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    SetMacAddressReq.Hdr.cbReq = sizeof(SetMacAddressReq);
    SetMacAddressReq.pSession = NIL_RTR0PTR;
    SetMacAddressReq.hIf = pThis->hIf;
    int rc = pThis->pConfigIf->pfnGetMac(pThis->pConfigIf, &SetMacAddressReq.Mac);
    if (RT_SUCCESS(rc))
        rc = pThis->pDrvIns->pDrvHlp->pfnSUPCallVMMR0Ex(pThis->pDrvIns, VMMR0_DO_INTNET_IF_SET_MAC_ADDRESS,
                                                        &SetMacAddressReq, sizeof(SetMacAddressReq));

    Log(("drvIntNetUpdateMacAddress: %.*Rhxs rc=%Rrc\n", sizeof(SetMacAddressReq.Mac), &SetMacAddressReq.Mac, rc));
    return rc;
}


/**
 * Sets the kernel interface active or inactive.
 *
 * Worker for poweron, poweroff, suspend and resume.
 *
 * @returns VBox status code.
 * @param   pThis       The driver instance.
 * @param   fActive     The new state.
 */
static int drvIntNetSetActive(PDRVINTNET pThis, bool fActive)
{
    if (!pThis->pConfigIf)
        return VINF_SUCCESS;

    INTNETIFSETACTIVEREQ SetActiveReq;
    SetActiveReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    SetActiveReq.Hdr.cbReq = sizeof(SetActiveReq);
    SetActiveReq.pSession = NIL_RTR0PTR;
    SetActiveReq.hIf = pThis->hIf;
    SetActiveReq.fActive = fActive;
    int rc = pThis->pDrvIns->pDrvHlp->pfnSUPCallVMMR0Ex(pThis->pDrvIns, VMMR0_DO_INTNET_IF_SET_ACTIVE,
                                                        &SetActiveReq, sizeof(SetActiveReq));

    Log(("drvIntNetUpdateMacAddress: fActive=%d rc=%Rrc\n", fActive, rc));
    AssertRC(rc);
    return rc;
}


/**
 * Writes a frame packet to the buffer.
 *
 * @returns VBox status code.
 * @param   pBuf        The buffer.
 * @param   pRingBuf    The ring buffer to read from.
 * @param   pvFrame     The frame to write.
 * @param   cbFrame     The size of the frame.
 * @remark  This is the same as INTNETRingWriteFrame
 */
static int drvIntNetRingWriteFrame(PINTNETBUF pBuf, PINTNETRINGBUF pRingBuf, const void *pvFrame, uint32_t cbFrame)
{
    /*
     * Validate input.
     */
    Assert(pBuf);
    Assert(pRingBuf);
    Assert(pvFrame);
    Assert(cbFrame >= sizeof(RTMAC) * 2);
    uint32_t offWrite = pRingBuf->offWrite;
    Assert(offWrite == RT_ALIGN_32(offWrite, sizeof(INTNETHDR)));
    uint32_t offRead = pRingBuf->offRead;
    Assert(offRead == RT_ALIGN_32(offRead, sizeof(INTNETHDR)));

    const uint32_t cb = RT_ALIGN_32(cbFrame, sizeof(INTNETHDR));
    if (offRead <= offWrite)
    {
        /*
         * Try fit it all before the end of the buffer.
         */
        if (pRingBuf->offEnd - offWrite >= cb + sizeof(INTNETHDR))
        {
            PINTNETHDR pHdr = (PINTNETHDR)((uint8_t *)pBuf + offWrite);
            pHdr->u16Type  = INTNETHDR_TYPE_FRAME;
            pHdr->cbFrame  = cbFrame;
            pHdr->offFrame = sizeof(INTNETHDR);

            memcpy(pHdr + 1, pvFrame, cbFrame);

            offWrite += cb + sizeof(INTNETHDR);
            Assert(offWrite <= pRingBuf->offEnd && offWrite >= pRingBuf->offStart);
            if (offWrite >= pRingBuf->offEnd)
                offWrite = pRingBuf->offStart;
            Log2(("WriteFrame: offWrite: %#x -> %#x (1)\n", pRingBuf->offWrite, offWrite));
            ASMAtomicXchgU32(&pRingBuf->offWrite, offWrite);
            return VINF_SUCCESS;
        }

        /*
         * Try fit the frame at the start of the buffer.
         * (The header fits before the end of the buffer because of alignment.)
         */
        AssertMsg(pRingBuf->offEnd - offWrite >= sizeof(INTNETHDR), ("offEnd=%x offWrite=%x\n", pRingBuf->offEnd, offWrite));
        if (offRead - pRingBuf->offStart > cb) /* not >= ! */
        {
            PINTNETHDR  pHdr = (PINTNETHDR)((uint8_t *)pBuf + offWrite);
            void       *pvFrameOut = (PINTNETHDR)((uint8_t *)pBuf + pRingBuf->offStart);
            pHdr->u16Type  = INTNETHDR_TYPE_FRAME;
            pHdr->cbFrame  = cbFrame;
            pHdr->offFrame = (intptr_t)pvFrameOut - (intptr_t)pHdr;

            memcpy(pvFrameOut, pvFrame, cbFrame);

            offWrite = pRingBuf->offStart + cb;
            ASMAtomicXchgU32(&pRingBuf->offWrite, offWrite);
            Log2(("WriteFrame: offWrite: %#x -> %#x (2)\n", pRingBuf->offWrite, offWrite));
            return VINF_SUCCESS;
        }
    }
    /*
     * The reader is ahead of the writer, try fit it into that space.
     */
    else if (offRead - offWrite > cb + sizeof(INTNETHDR)) /* not >= ! */
    {
        PINTNETHDR pHdr = (PINTNETHDR)((uint8_t *)pBuf + offWrite);
        pHdr->u16Type  = INTNETHDR_TYPE_FRAME;
        pHdr->cbFrame  = cbFrame;
        pHdr->offFrame = sizeof(INTNETHDR);

        memcpy(pHdr + 1, pvFrame, cbFrame);

        offWrite += cb + sizeof(INTNETHDR);
        ASMAtomicXchgU32(&pRingBuf->offWrite, offWrite);
        Log2(("WriteFrame: offWrite: %#x -> %#x (3)\n", pRingBuf->offWrite, offWrite));
        return VINF_SUCCESS;
    }

    /* (it didn't fit) */
    /** @todo stats */
    return VERR_BUFFER_OVERFLOW;
}


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
          "%.*Rhxd\n",
          pvBuf, cb, cb, pvBuf));
#endif

    /*
     * Add the frame to the send buffer and push it onto the network.
     */
    int rc = drvIntNetRingWriteFrame(pThis->pBuf, &pThis->pBuf->Send, pvBuf, cb);
    if (    rc == VERR_BUFFER_OVERFLOW
        &&  pThis->pBuf->cbSend < cb)
    {
        INTNETIFSENDREQ SendReq;
        SendReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
        SendReq.Hdr.cbReq = sizeof(SendReq);
        SendReq.pSession = NIL_RTR0PTR;
        SendReq.hIf = pThis->hIf;
        pThis->pDrvIns->pDrvHlp->pfnSUPCallVMMR0Ex(pThis->pDrvIns, VMMR0_DO_INTNET_IF_SEND, &SendReq, sizeof(SendReq));

        rc = drvIntNetRingWriteFrame(pThis->pBuf, &pThis->pBuf->Send, pvBuf, cb);
    }

    if (RT_SUCCESS(rc))
    {
        INTNETIFSENDREQ SendReq;
        SendReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
        SendReq.Hdr.cbReq = sizeof(SendReq);
        SendReq.pSession = NIL_RTR0PTR;
        SendReq.hIf = pThis->hIf;
        rc = pThis->pDrvIns->pDrvHlp->pfnSUPCallVMMR0Ex(pThis->pDrvIns, VMMR0_DO_INTNET_IF_SEND, &SendReq, sizeof(SendReq));
    }

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
    INTNETIFSETPROMISCUOUSMODEREQ Req;
    Req.Hdr.u32Magic    = SUPVMMR0REQHDR_MAGIC;
    Req.Hdr.cbReq       = sizeof(Req);
    Req.pSession        = NIL_RTR0PTR;
    Req.hIf             = pThis->hIf;
    Req.fPromiscuous    = fPromiscuous;
    int rc = pThis->pDrvIns->pDrvHlp->pfnSUPCallVMMR0Ex(pThis->pDrvIns, VMMR0_DO_INTNET_IF_SET_PROMISCUOUS_MODE, &Req, sizeof(Req));
    LogFlow(("drvIntNetSetPromiscuousMode: fPromiscuous=%RTbool\n", fPromiscuous));
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
 * Wait for space to become available up the driver/device chain.
 *
 * @returns VINF_SUCCESS if space is available.
 * @returns VERR_STATE_CHANGED if the state changed.
 * @returns VBox status code on other errors.
 * @param   pThis       Pointer to the instance data.
 */
static int drvIntNetAsyncIoWaitForSpace(PDRVINTNET pThis)
{
    LogFlow(("drvIntNetAsyncIoWaitForSpace:\n"));
    STAM_PROFILE_ADV_STOP(&pThis->StatReceive, a);
    int rc = pThis->pPort->pfnWaitReceiveAvail(pThis->pPort, RT_INDEFINITE_WAIT);
    STAM_PROFILE_ADV_START(&pThis->StatReceive, a);
    LogFlow(("drvIntNetAsyncIoWaitForSpace: returns %Rrc\n", rc));
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
                int rc = pThis->pPort->pfnWaitReceiveAvail(pThis->pPort, 0);
                if (rc == VINF_SUCCESS)
                {
#ifdef LOG_ENABLED
                    uint64_t u64Now = RTTimeProgramNanoTS();
                    LogFlow(("drvIntNetAsyncIoRun: %-4d bytes at %llu ns  deltas: r=%llu t=%llu\n",
                             cbFrame, u64Now, u64Now - pThis->u64LastReceiveTS, u64Now - pThis->u64LastTransferTS));
                    pThis->u64LastReceiveTS = u64Now;
                    Log2(("drvIntNetAsyncIoRun: cbFrame=%#x\n"
                          "%.*Rhxd\n",
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
                    rc = drvIntNetAsyncIoWaitForSpace(pThis);
                    if (RT_FAILURE(rc))
                    {
                        if (rc == VERR_INTERRUPTED)
                        {
                            /*
                             * NIC is going down, likely because the VM is being reset. Skip the frame.
                             */
                            AssertMsg(pHdr->u16Type == INTNETHDR_TYPE_FRAME, ("Unknown frame type %RX16! offRead=%#x\n",
                                                                              pHdr->u16Type, pRingBuf->offRead));
                            INTNETRingSkipFrame(pBuf, pRingBuf);
                        }
                        else
                        {
                            STAM_PROFILE_ADV_STOP(&pThis->StatReceive, a);
                            LogFlow(("drvIntNetAsyncIoRun: returns %Rrc (wait-for-space)\n", rc));
                            return rc;
                        }
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
        INTNETIFWAITREQ WaitReq;
        WaitReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
        WaitReq.Hdr.cbReq    = sizeof(WaitReq);
        WaitReq.pSession     = NIL_RTR0PTR;
        WaitReq.hIf          = pThis->hIf;
        WaitReq.cMillies     = 30000; /* 30s - don't wait forever, timeout now and then. */
        STAM_PROFILE_ADV_STOP(&pThis->StatReceive, a);
        int rc = pDrvIns->pDrvHlp->pfnSUPCallVMMR0Ex(pDrvIns, VMMR0_DO_INTNET_IF_WAIT, &WaitReq, sizeof(WaitReq));
        if (    RT_FAILURE(rc)
            &&  rc != VERR_TIMEOUT
            &&  rc != VERR_INTERRUPTED)
        {
            LogFlow(("drvIntNetAsyncIoRun: returns %Rrc\n", rc));
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
                if (    RT_FAILURE(rc)
                    &&  rc != VERR_TIMEOUT)
                {
                    LogFlow(("drvIntNetAsyncIoThread: returns %Rrc\n", rc));
                    return rc;
                }
                break;
            }

            case ASYNCSTATE_RUNNING:
            {
                int rc = drvIntNetAsyncIoRun(pThis);
                if (    rc != VERR_STATE_CHANGED
                    &&  RT_FAILURE(rc))
                {
                    LogFlow(("drvIntNetAsyncIoThread: returns %Rrc\n", rc));
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
    PDRVINTNET pThis = PDMINS_2_DATA(pDrvIns, PDRVINTNET);
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
    PDRVINTNET pThis = PDMINS_2_DATA(pDrvIns, PDRVINTNET);
    if (!pThis->fActivateEarlyDeactivateLate)
    {
        ASMAtomicXchgSize(&pThis->enmState, ASYNCSTATE_SUSPENDED);
        drvIntNetSetActive(pThis, false /* fActive */);
    }
}


/**
 * Resume notification.
 *
 * @param   pDrvIns     The driver instance.
 */
static DECLCALLBACK(void) drvIntNetResume(PPDMDRVINS pDrvIns)
{
    LogFlow(("drvIntNetPowerResume\n"));
    PDRVINTNET pThis = PDMINS_2_DATA(pDrvIns, PDRVINTNET);
    if (!pThis->fActivateEarlyDeactivateLate)
    {
        ASMAtomicXchgSize(&pThis->enmState, ASYNCSTATE_RUNNING);
        RTSemEventSignal(pThis->EventSuspended);
        drvIntNetUpdateMacAddress(pThis); /* (could be a state restore) */
        drvIntNetSetActive(pThis, true /* fActive */);
    }
}


/**
 * Suspend notification.
 *
 * @param   pDrvIns     The driver instance.
 */
static DECLCALLBACK(void) drvIntNetSuspend(PPDMDRVINS pDrvIns)
{
    LogFlow(("drvIntNetPowerSuspend\n"));
    PDRVINTNET pThis = PDMINS_2_DATA(pDrvIns, PDRVINTNET);
    if (!pThis->fActivateEarlyDeactivateLate)
    {
        ASMAtomicXchgSize(&pThis->enmState, ASYNCSTATE_SUSPENDED);
        drvIntNetSetActive(pThis, false /* fActive */);
    }
}


/**
 * Power On notification.
 *
 * @param   pDrvIns     The driver instance.
 */
static DECLCALLBACK(void) drvIntNetPowerOn(PPDMDRVINS pDrvIns)
{
    LogFlow(("drvIntNetPowerOn\n"));
    PDRVINTNET pThis = PDMINS_2_DATA(pDrvIns, PDRVINTNET);
    if (!pThis->fActivateEarlyDeactivateLate)
    {
        ASMAtomicXchgSize(&pThis->enmState, ASYNCSTATE_RUNNING);
        RTSemEventSignal(pThis->EventSuspended);
        drvIntNetUpdateMacAddress(pThis);
        drvIntNetSetActive(pThis, true /* fActive */);
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
static DECLCALLBACK(void) drvIntNetDestruct(PPDMDRVINS pDrvIns)
{
    LogFlow(("drvIntNetDestruct\n"));
    PDRVINTNET pThis = PDMINS_2_DATA(pDrvIns, PDRVINTNET);

    /*
     * Indicate to the thread that it's time to quit.
     */
    ASMAtomicXchgSize(&pThis->enmState, ASYNCSTATE_TERMINATE);
    ASMAtomicXchgSize(&pThis->fLinkDown, true);
    RTSEMEVENT EventSuspended = pThis->EventSuspended;
    pThis->EventSuspended = NIL_RTSEMEVENT;

    /*
     * Close the interface
     */
    if (pThis->hIf != INTNET_HANDLE_INVALID)
    {
        INTNETIFCLOSEREQ CloseReq;
        CloseReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
        CloseReq.Hdr.cbReq = sizeof(CloseReq);
        CloseReq.pSession = NIL_RTR0PTR;
        CloseReq.hIf = pThis->hIf;
        pThis->hIf = INTNET_HANDLE_INVALID;
        int rc = pDrvIns->pDrvHlp->pfnSUPCallVMMR0Ex(pDrvIns, VMMR0_DO_INTNET_IF_CLOSE, &CloseReq, sizeof(CloseReq));
        AssertRC(rc);
    }

    /*
     * Wait for the thread to terminate.
     */
    if (pThis->Thread != NIL_RTTHREAD)
    {
        if (EventSuspended != NIL_RTSEMEVENT)
            RTSemEventSignal(EventSuspended);
        int rc = RTThreadWait(pThis->Thread, 5000, NULL);
        AssertRC(rc);
        pThis->Thread = NIL_RTTHREAD;
    }

    /*
     * Destroy the semaphores.
     */
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
    PDRVINTNET pThis = PDMINS_2_DATA(pDrvIns, PDRVINTNET);
    bool f;

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                      = pDrvIns;
    pThis->hIf                          = INTNET_HANDLE_INVALID;
    pThis->Thread                       = NIL_RTTHREAD;
    pThis->EventSuspended               = NIL_RTSEMEVENT;
    pThis->enmState                     = ASYNCSTATE_SUSPENDED;
    pThis->fActivateEarlyDeactivateLate = false;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface    = drvIntNetQueryInterface;
    /* INetwork */
    pThis->INetworkConnector.pfnSend                = drvIntNetSend;
    pThis->INetworkConnector.pfnSetPromiscuousMode  = drvIntNetSetPromiscuousMode;
    pThis->INetworkConnector.pfnNotifyLinkChanged   = drvIntNetNotifyLinkChanged;

    /*
     * Validate the config.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle,
                              "Network\0"
                              "Trunk\0"
                              "TrunkType\0"
                              "ReceiveBufferSize\0"
                              "SendBufferSize\0"
                              "RestrictAccess\0"
                              "SharedMacOnWire\0"
                              "IgnoreAllPromisc\0"
                              "QuietlyIgnoreAllPromisc\0"
                              "IgnoreClientPromisc\0"
                              "QuietlyIgnoreClientPromisc\0"
                              "IgnoreTrunkWirePromisc\0"
                              "QuietlyIgnoreTrunkWirePromisc\0"
                              "IgnoreTrunkHostPromisc\0"
                              "QuietlyIgnoreTrunkHostPromisc\0"
                              "IsService\0"
                              "DhcpIPAddress\0"
                              "DhcpNetworkMask\0"
                              "DhcpLowerIP\0"
                              "DhcpUpperIP\0"
                              "DhcpMacAddress\0"))
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
    pThis->pConfigIf = (PPDMINETWORKCONFIG)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_NETWORK_CONFIG);

    /*
     * Read the configuration.
     */
    INTNETOPENREQ OpenReq;
    memset(&OpenReq, 0, sizeof(OpenReq));
    OpenReq.Hdr.cbReq = sizeof(OpenReq);
    OpenReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    OpenReq.pSession = NIL_RTR0PTR;

    /** @cfgm{Network, string}
     * The name of the internal network to connect to.
     */
    rc = CFGMR3QueryString(pCfgHandle, "Network", OpenReq.szNetwork, sizeof(OpenReq.szNetwork));
    if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"Network\" value"));
    strcpy(pThis->szNetwork, OpenReq.szNetwork);

    /** @cfgm{TrunkType, uint32_t, kIntNetTrunkType_None}
     * The trunk connection type see INTNETTRUNKTYPE.
     */
    uint32_t u32TrunkType;
    rc = CFGMR3QueryU32(pCfgHandle, "TrunkType", &u32TrunkType);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        u32TrunkType = kIntNetTrunkType_None;
    else if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"TrunkType\" value"));
    OpenReq.enmTrunkType = (INTNETTRUNKTYPE)u32TrunkType;

    /** @cfgm{Trunk, string, ""}
     * The name of the trunk connection.
     */
    rc = CFGMR3QueryString(pCfgHandle, "Trunk", OpenReq.szTrunk, sizeof(OpenReq.szTrunk));
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        OpenReq.szTrunk[0] = '\0';
    else if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"Trunk\" value"));

    /** @cfgm{RestrictAccess, boolean, true}
     * Whether to restrict the access to the network or if it should be public. Everyone on
     * the computer can connect to a public network. Don't change this.
     */
    bool fRestrictAccess;
    rc = CFGMR3QueryBool(pCfgHandle, "RestrictAccess", &fRestrictAccess);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fRestrictAccess = true;
    else if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"RestrictAccess\" value"));
    OpenReq.fFlags = fRestrictAccess ? 0 : INTNET_OPEN_FLAGS_PUBLIC;

    /** @cfgm{IgnoreAllPromisc, boolean, false}
     * When set all request for operating any interface or trunk in promiscuous
     * mode will be ignored. */
    rc = CFGMR3QueryBoolDef(pCfgHandle, "IgnoreAllPromisc", &f, false);
    if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"IgnoreAllPromisc\" value"));
    if (f)
        OpenReq.fFlags |= INTNET_OPEN_FLAGS_IGNORE_PROMISC;

    /** @cfgm{QuietlyIgnoreAllPromisc, boolean, false}
     * When set all request for operating any interface or trunk in promiscuous
     * mode will be ignored.  This differs from IgnoreAllPromisc in that clients
     * won't get VERR_INTNET_INCOMPATIBLE_FLAGS. */
    rc = CFGMR3QueryBoolDef(pCfgHandle, "QuietlyIgnoreAllPromisc", &f, false);
    if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"QuietlyIgnoreAllPromisc\" value"));
    if (f)
        OpenReq.fFlags |= INTNET_OPEN_FLAGS_QUIETLY_IGNORE_PROMISC;

    /** @cfgm{IgnoreClientPromisc, boolean, false}
     * When set all request for operating any non-trunk interface in promiscuous
     * mode will be ignored. */
    rc = CFGMR3QueryBoolDef(pCfgHandle, "IgnoreClientPromisc", &f, false);
    if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"IgnoreClientPromisc\" value"));
    if (f)
        OpenReq.fFlags |= INTNET_OPEN_FLAGS_IGNORE_PROMISC; /** @todo add special flag for this. */

    /** @cfgm{QuietlyIgnoreClientPromisc, boolean, false}
     * When set all request for operating any non-trunk interface promiscuous mode
     * will be ignored.  This differs from IgnoreClientPromisc in that clients won't
     * get VERR_INTNET_INCOMPATIBLE_FLAGS. */
    rc = CFGMR3QueryBoolDef(pCfgHandle, "QuietlyIgnoreClientPromisc", &f, false);
    if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"QuietlyIgnoreClientPromisc\" value"));
    if (f)
        OpenReq.fFlags |= INTNET_OPEN_FLAGS_QUIETLY_IGNORE_PROMISC;  /** @todo add special flag for this. */

    /** @cfgm{IgnoreTrunkWirePromisc, boolean, false}
     * When set all request for operating the trunk-wire connection in promiscuous
     * mode will be ignored. */
    rc = CFGMR3QueryBoolDef(pCfgHandle, "IgnoreTrunkWirePromisc", &f, false);
    if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"IgnoreTrunkWirePromisc\" value"));
    if (f)
        OpenReq.fFlags |= INTNET_OPEN_FLAGS_IGNORE_PROMISC_TRUNK_WIRE;

    /** @cfgm{QuietlyIgnoreTrunkWirePromisc, boolean, false}
     * When set all request for operating any trunk-wire connection promiscuous mode
     * will be ignored.  This differs from IgnoreTrunkWirePromisc in that clients
     * won't get VERR_INTNET_INCOMPATIBLE_FLAGS. */
    rc = CFGMR3QueryBoolDef(pCfgHandle, "QuietlyIgnoreTrunkWirePromisc", &f, false);
    if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"QuietlyIgnoreTrunkWirePromisc\" value"));
    if (f)
        OpenReq.fFlags |= INTNET_OPEN_FLAGS_QUIETLY_IGNORE_PROMISC_TRUNK_WIRE;

    /** @cfgm{IgnoreTrunkHostPromisc, boolean, false}
     * When set all request for operating the trunk-host connection in promiscuous
     * mode will be ignored. */
    rc = CFGMR3QueryBoolDef(pCfgHandle, "IgnoreTrunkHostPromisc", &f, false);
    if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"IgnoreTrunkHostPromisc\" value"));
    if (f)
        OpenReq.fFlags |= INTNET_OPEN_FLAGS_IGNORE_PROMISC_TRUNK_HOST;

    /** @cfgm{QuietlyIgnoreTrunkHostPromisc, boolean, false}
     * When set all request for operating any trunk-host connection promiscuous mode
     * will be ignored.  This differs from IgnoreTrunkHostPromisc in that clients
     * won't get VERR_INTNET_INCOMPATIBLE_FLAGS. */
    rc = CFGMR3QueryBoolDef(pCfgHandle, "QuietlyIgnoreTrunkHostPromisc", &f, false);
    if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"QuietlyIgnoreTrunkHostPromisc\" value"));
    if (f)
        OpenReq.fFlags |= INTNET_OPEN_FLAGS_QUIETLY_IGNORE_PROMISC_TRUNK_HOST;

    /** @todo flags for not sending to the host and for setting the trunk-wire
     *        connection in promiscuous mode. */


    /** @cfgm{SharedMacOnWire, boolean, false}
     * Whether to shared the MAC address of the host interface when using the wire. When
     * attaching to a wireless NIC this option is usally a requirement.
     */
    bool fSharedMacOnWire;
    rc = CFGMR3QueryBoolDef(pCfgHandle, "SharedMacOnWire", &fSharedMacOnWire, false);
    if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"SharedMacOnWire\" value"));
    if (fSharedMacOnWire)
        OpenReq.fFlags |= INTNET_OPEN_FLAGS_SHARED_MAC_ON_WIRE;

    /** @cfgm{ReceiveBufferSize, uint32_t, 218 KB}
     * The size of the receive buffer.
     */
    rc = CFGMR3QueryU32(pCfgHandle, "ReceiveBufferSize", &OpenReq.cbRecv);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        OpenReq.cbRecv = 218 * _1K ;
    else if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"ReceiveBufferSize\" value"));

    /** @cfgm{SendBufferSize, uint32_t, 36 KB}
     * The size of the send (transmit) buffer.
     * This should be more than twice the size of the larges frame size because
     * the ring buffer is very simple and doesn't support splitting up frames
     * nor inserting padding. So, if this is too close to the frame size the
     * header will fragment the buffer such that the frame won't fit on either
     * side of it and the code will get very upset about it all.
     */
    rc = CFGMR3QueryU32(pCfgHandle, "SendBufferSize", &OpenReq.cbSend);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        OpenReq.cbSend = 36*_1K;
    else if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"SendBufferSize\" value"));
    if (OpenReq.cbSend < 32)
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: The \"SendBufferSize\" value is too small"));
    if (OpenReq.cbSend < 16384*2 + 64)
        LogRel(("DrvIntNet: Warning! SendBufferSize=%u, Recommended minimum size %u butes.\n", OpenReq.cbSend, 16384*2 + 64));

    /** @cfgm{IsService, boolean, true}
     * This alterns the way the thread is suspended and resumed. When it's being used by
     * a service such as LWIP/iSCSI it shouldn't suspend immediately like for a NIC.
     */
    rc = CFGMR3QueryBool(pCfgHandle, "IsService", &pThis->fActivateEarlyDeactivateLate);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pThis->fActivateEarlyDeactivateLate = false;
    else if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"IsService\" value"));

    LogRel(("IntNet#%u: szNetwork={%s} enmTrunkType=%d szTrunk={%s} fFlags=%#x cbRecv=%u cbSend=%u\n",
            pDrvIns->iInstance, OpenReq.szNetwork, OpenReq.enmTrunkType, OpenReq.szTrunk, OpenReq.fFlags,
            OpenReq.cbRecv, OpenReq.cbSend));

#ifdef RT_OS_DARWIN
    /* Temporary hack: attach to a network with the name 'if=en0' and you're hitting the wire. */
    if (    !OpenReq.szTrunk[0]
        &&   OpenReq.enmTrunkType == kIntNetTrunkType_None
        &&  !strncmp(pThis->szNetwork, "if=en", sizeof("if=en") - 1)
        &&  RT_C_IS_DIGIT(pThis->szNetwork[sizeof("if=en") - 1])
        &&  !pThis->szNetwork[sizeof("if=en")])
    {
        OpenReq.enmTrunkType = kIntNetTrunkType_NetFlt;
        strcpy(OpenReq.szTrunk, &pThis->szNetwork[sizeof("if=") - 1]);
    }
    /* Temporary hack: attach to a network with the name 'wif=en0' and you're on the air. */
    if (    !OpenReq.szTrunk[0]
        &&   OpenReq.enmTrunkType == kIntNetTrunkType_None
        &&  !strncmp(pThis->szNetwork, "wif=en", sizeof("wif=en") - 1)
        &&  RT_C_IS_DIGIT(pThis->szNetwork[sizeof("wif=en") - 1])
        &&  !pThis->szNetwork[sizeof("wif=en")])
    {
        OpenReq.enmTrunkType = kIntNetTrunkType_NetFlt;
        OpenReq.fFlags |= INTNET_OPEN_FLAGS_SHARED_MAC_ON_WIRE;
        strcpy(OpenReq.szTrunk, &pThis->szNetwork[sizeof("wif=") - 1]);
    }

#elif defined(RT_OS_WINDOWS) && defined(VBOX_WITH_NETFLT)
    if (OpenReq.enmTrunkType == kIntNetTrunkType_NetFlt
            || OpenReq.enmTrunkType == kIntNetTrunkType_NetAdp)
    {
# ifndef VBOX_NETFLT_ONDEMAND_BIND
        /*
         * We have a ndis filter driver started on system boot before the VBoxDrv,
         * tell the filter driver to init VBoxNetFlt functionality.
         */
        rc = drvIntNetWinConstruct(pDrvIns, pCfgHandle, OpenReq.enmTrunkType);
        AssertLogRelMsgRCReturn(rc, ("drvIntNetWinConstruct failed, rc=%Rrc", rc), rc);
# endif

        /*
         * <Describe what this does here or/and in the function docs of drvIntNetWinIfGuidToBindName>.
         */
        char szBindName[INTNET_MAX_TRUNK_NAME];
        rc = drvIntNetWinIfGuidToBindName(OpenReq.szTrunk, szBindName, INTNET_MAX_TRUNK_NAME);
        AssertLogRelMsgRCReturn(rc, ("drvIntNetWinIfGuidToBindName failed, rc=%Rrc", rc), rc);
        strcpy(OpenReq.szTrunk, szBindName);
    }
#endif /* WINDOWS && NETFLT */

    /*
     * Create the event semaphores
     */
    rc = RTSemEventCreate(&pThis->EventSuspended);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Create the interface.
     */
    OpenReq.hIf = INTNET_HANDLE_INVALID;
    rc = pDrvIns->pDrvHlp->pfnSUPCallVMMR0Ex(pDrvIns, VMMR0_DO_INTNET_OPEN, &OpenReq, sizeof(OpenReq));
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("Failed to open/create the internal network '%s'"), pThis->szNetwork);
    AssertRelease(OpenReq.hIf != INTNET_HANDLE_INVALID);
    pThis->hIf = OpenReq.hIf;
    Log(("IntNet%d: hIf=%RX32 '%s'\n", pDrvIns->iInstance, pThis->hIf, pThis->szNetwork));

    if(rc != VINF_ALREADY_INITIALIZED)
    {
        /* new network gets created, check if we need to launch a DHCP server for it */
        char ip[16], mask[16], lowerIp[16], upperIp[16], mac[13];
        rc = CFGMR3QueryString(pCfgHandle, "DhcpIPAddress", ip, sizeof(ip));
        if (RT_SUCCESS(rc))
        {
            /* this means we have DHCP server enabled */
            rc = CFGMR3QueryString(pCfgHandle, "DhcpNetworkMask", mask, sizeof(mask));
            if (RT_FAILURE(rc))
                return PDMDRV_SET_ERROR(pDrvIns, rc,
                                    N_("Configuration error: Failed to get the \"DhcpNetworkMask\" value"));

            rc = CFGMR3QueryString(pCfgHandle, "DhcpLowerIP", lowerIp, sizeof(lowerIp));
            if (RT_FAILURE(rc))
                return PDMDRV_SET_ERROR(pDrvIns, rc,
                                    N_("Configuration error: Failed to get the \"DhcpLowerIP\" value"));

            rc = CFGMR3QueryString(pCfgHandle, "DhcpUpperIP", upperIp, sizeof(upperIp));
            if (RT_FAILURE(rc))
                return PDMDRV_SET_ERROR(pDrvIns, rc,
                                    N_("Configuration error: Failed to get the \"DhcpUpperIP\" value"));

            rc = CFGMR3QueryString(pCfgHandle, "DhcpMacAddress", mac, sizeof(mac));
            if (RT_FAILURE(rc))
                return PDMDRV_SET_ERROR(pDrvIns, rc,
                                    N_("Configuration error: Failed to get the \"DhcpMacAddress\" value"));


            DhcpServerRunner dhcp;
            dhcp.setOption(DHCPCFG_NETNAME, OpenReq.szNetwork);
            dhcp.setOption(DHCPCFG_TRUNKNAME, OpenReq.szTrunk);
            switch(OpenReq.enmTrunkType)
            {
            case kIntNetTrunkType_WhateverNone:
                dhcp.setOption(DHCPCFG_TRUNKTYPE, TRUNKTYPE_WHATEVER);
                break;
            case kIntNetTrunkType_NetFlt:
                dhcp.setOption(DHCPCFG_TRUNKTYPE, TRUNKTYPE_NETFLT);
                break;
            case kIntNetTrunkType_NetAdp:
                dhcp.setOption(DHCPCFG_TRUNKTYPE, TRUNKTYPE_NETADP);
                break;
            case kIntNetTrunkType_SrvNat:
                dhcp.setOption(DHCPCFG_TRUNKTYPE, TRUNKTYPE_SRVNAT);
                break;
            default:
                AssertFailed();
                break;
            }
        //temporary hack for testing
            //    DHCPCFG_NAME
            dhcp.setOption(DHCPCFG_MACADDRESS, mac);
            dhcp.setOption(DHCPCFG_IPADDRESS,  ip);
        //        DHCPCFG_LEASEDB,
        //        DHCPCFG_VERBOSE,
        //        DHCPCFG_GATEWAY,
            dhcp.setOption(DHCPCFG_LOWERIP,  lowerIp);
            dhcp.setOption(DHCPCFG_UPPERIP,  upperIp);
            dhcp.setOption(DHCPCFG_NETMASK,  mask);

        //        DHCPCFG_HELP,
        //        DHCPCFG_VERSION,
        //        DHCPCFG_NOTOPT_MAXVAL
            dhcp.setOption(DHCPCFG_BEGINCONFIG,  "");
            dhcp.start();

            dhcp.detachFromServer(); /* need to do this to avoid server shutdown on runner destruction */
        }
    }

    /*
     * Get default buffer.
     */
    INTNETIFGETRING3BUFFERREQ GetRing3BufferReq;
    GetRing3BufferReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    GetRing3BufferReq.Hdr.cbReq = sizeof(GetRing3BufferReq);
    GetRing3BufferReq.pSession = NIL_RTR0PTR;
    GetRing3BufferReq.hIf = pThis->hIf;
    GetRing3BufferReq.pRing3Buf = NULL;
    rc = pDrvIns->pDrvHlp->pfnSUPCallVMMR0Ex(pDrvIns, VMMR0_DO_INTNET_IF_GET_RING3_BUFFER, &GetRing3BufferReq, sizeof(GetRing3BufferReq));
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("Failed to get ring-3 buffer for the newly created interface to '%s'"), pThis->szNetwork);
    AssertRelease(VALID_PTR(GetRing3BufferReq.pRing3Buf));
    pThis->pBuf = GetRing3BufferReq.pRing3Buf;

    /*
     * Create the async I/O thread.
     * Note! Using a PDM thread here doesn't fit with the IsService=true operation.
     */
    rc = RTThreadCreate(&pThis->Thread, drvIntNetAsyncIoThread, pThis, _128K, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "INTNET");
    if (RT_FAILURE(rc))
    {
        AssertRC(rc);
        return rc;
    }

    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->pBuf->cbStatRecv,       STAMTYPE_COUNTER,   STAMVISIBILITY_ALWAYS,  STAMUNIT_BYTES, "Number of received bytes.",    "/Net/IntNet%d/Bytes/Received", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->pBuf->cbStatSend,       STAMTYPE_COUNTER,   STAMVISIBILITY_ALWAYS,  STAMUNIT_BYTES, "Number of sent bytes.",        "/Net/IntNet%d/Bytes/Sent", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->pBuf->cStatRecvs,       STAMTYPE_COUNTER,   STAMVISIBILITY_ALWAYS,  STAMUNIT_BYTES, "Number of received packets.",  "/Net/IntNet%d/Packets/Received", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->pBuf->cStatSends,       STAMTYPE_COUNTER,   STAMVISIBILITY_ALWAYS,  STAMUNIT_BYTES, "Number of sent packets.",      "/Net/IntNet%d/Packets/Sent", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->pBuf->cStatLost,        STAMTYPE_COUNTER,   STAMVISIBILITY_ALWAYS,  STAMUNIT_BYTES, "Number of sent packets.",      "/Net/IntNet%d/Packets/Lost", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->pBuf->cStatYieldsNok,   STAMTYPE_COUNTER,   STAMVISIBILITY_ALWAYS,  STAMUNIT_BYTES, "Number of times yielding didn't help fix an overflow.",  "/Net/IntNet%d/YieldNok", pDrvIns->iInstance);
#ifdef VBOX_WITH_STATISTICS
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatReceive,            STAMTYPE_PROFILE,   STAMVISIBILITY_ALWAYS,  STAMUNIT_TICKS_PER_CALL, "Profiling packet receive runs.",  "/Net/IntNet%d/Receive", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatTransmit,           STAMTYPE_PROFILE,   STAMVISIBILITY_ALWAYS,  STAMUNIT_TICKS_PER_CALL, "Profiling packet transmit runs.", "/Net/IntNet%d/Transmit", pDrvIns->iInstance);
#endif

    /*
     * Activate data transmission as early as possible
     */
    if (pThis->fActivateEarlyDeactivateLate)
    {
        ASMAtomicXchgSize(&pThis->enmState, ASYNCSTATE_RUNNING);
        RTSemEventSignal(pThis->EventSuspended);
        drvIntNetUpdateMacAddress(pThis);
        drvIntNetSetActive(pThis, true /* fActive */);
    }

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

