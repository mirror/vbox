/* $Id$ */
/** @file
 * DrvIntNet - Internal network transport driver.
 */

/*
 * Copyright (C) 2006-2010 Sun Microsystems, Inc.
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
#include <VBox/pdmnetifs.h>
#include <VBox/cfgm.h>
#include <VBox/intnet.h>
#include <VBox/intnetinline.h>
#include <VBox/vmm.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/memcache.h>
#include <iprt/net.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/thread.h>
#include <iprt/uuid.h>

#include "../Builtins.h"


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
 * Internal networking driver instance data.
 *
 * @implements  PDMINETWORKUP
 */
typedef struct DRVINTNET
{
    /** The network interface. */
    PDMINETWORKUP                   INetworkUpR3;
    /** The network interface. */
    R3PTRTYPE(PPDMINETWORKDOWN)     pIAboveNet;
    /** The network config interface.
     * Can (in theory at least) be NULL. */
    R3PTRTYPE(PPDMINETWORKCONFIG)   pIAboveConfigR3;
    /** Pointer to the driver instance. */
    PPDMDRVINSR3                    pDrvInsR3;
    /** Pointer to the communication buffer. */
    R3PTRTYPE(PINTNETBUF)           pBufR3;
    /** Interface handle. */
    INTNETIFHANDLE                  hIf;

    /** The thread state. */
    ASYNCSTATE volatile             enmState;
    /** Reader thread. */
    RTTHREAD                        Thread;
    /** Event semaphore the Thread waits on while the VM is suspended. */
    RTSEMEVENT                      hEvtSuspended;
    /** Scatter/gather descriptor cache. */
    RTMEMCACHE                      hSgCache;
    /** Set if the link is down.
     * When the link is down all incoming packets will be dropped. */
    bool volatile                   fLinkDown;
    /** Set if data transmission should start immediately and deactivate
     * as late as possible. */
    bool                            fActivateEarlyDeactivateLate;
    /** Padding. */
    bool                            afReserved[2];
    /** The network name. */
    char                            szNetwork[INTNET_MAX_NETWORK_NAME];

    /** Base interface for ring-0. */
    PDMIBASER0                      IBaseR0;
    /** Base interface for ring-0. */
    PDMIBASERC                      IBaseRC;

#ifdef LOG_ENABLED
    /** The nano ts of the last transfer. */
    uint64_t                        u64LastTransferTS;
    /** The nano ts of the last receive. */
    uint64_t                        u64LastReceiveTS;
#endif
#ifdef VBOX_WITH_STATISTICS
    /** Profiling packet transmit runs. */
    STAMPROFILE                     StatTransmit;
    /** Profiling packet receive runs. */
    STAMPROFILEADV                  StatReceive;
#endif /* VBOX_WITH_STATISTICS */
} DRVINTNET;
/** Pointer to instance data of the internal networking driver. */
typedef DRVINTNET *PDRVINTNET;


#ifdef IN_RING3

/* -=-=-=-=- PDMINETWORKUP -=-=-=-=- */


/**
 * Updates the MAC address on the kernel side.
 *
 * @returns VBox status code.
 * @param   pThis       The driver instance.
 */
static int drvR3IntNetUpdateMacAddress(PDRVINTNET pThis)
{
    if (!pThis->pIAboveConfigR3)
        return VINF_SUCCESS;

    INTNETIFSETMACADDRESSREQ SetMacAddressReq;
    SetMacAddressReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    SetMacAddressReq.Hdr.cbReq = sizeof(SetMacAddressReq);
    SetMacAddressReq.pSession = NIL_RTR0PTR;
    SetMacAddressReq.hIf = pThis->hIf;
    int rc = pThis->pIAboveConfigR3->pfnGetMac(pThis->pIAboveConfigR3, &SetMacAddressReq.Mac);
    if (RT_SUCCESS(rc))
        rc = PDMDrvHlpSUPCallVMMR0Ex(pThis->pDrvInsR3, VMMR0_DO_INTNET_IF_SET_MAC_ADDRESS,
                                     &SetMacAddressReq, sizeof(SetMacAddressReq));

    Log(("drvR3IntNetUpdateMacAddress: %.*Rhxs rc=%Rrc\n", sizeof(SetMacAddressReq.Mac), &SetMacAddressReq.Mac, rc));
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
static int drvR3IntNetSetActive(PDRVINTNET pThis, bool fActive)
{
    if (!pThis->pIAboveConfigR3)
        return VINF_SUCCESS;

    INTNETIFSETACTIVEREQ SetActiveReq;
    SetActiveReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    SetActiveReq.Hdr.cbReq = sizeof(SetActiveReq);
    SetActiveReq.pSession = NIL_RTR0PTR;
    SetActiveReq.hIf = pThis->hIf;
    SetActiveReq.fActive = fActive;
    int rc = PDMDrvHlpSUPCallVMMR0Ex(pThis->pDrvInsR3, VMMR0_DO_INTNET_IF_SET_ACTIVE,
                                     &SetActiveReq, sizeof(SetActiveReq));

    Log(("drvR3IntNetSetActive: fActive=%d rc=%Rrc\n", fActive, rc));
    AssertRC(rc);
    return rc;
}


/**
 * @interface_method_impl{PDMINETWORKUP,pfnAllocBuf}
 */
static DECLCALLBACK(int) drvR3IntNetUp_AllocBuf(PPDMINETWORKUP pInterface, size_t cbMin, PPPDMSCATTERGATHER ppSgBuf)
{
    PDRVINTNET  pThis = RT_FROM_MEMBER(pInterface, DRVINTNET, INetworkUpR3);
    int         rc    = VINF_SUCCESS;
    Assert(cbMin < UINT32_MAX / 2);

    /*
     * Allocate a S/G buffer.
     */
    PPDMSCATTERGATHER pSgBuf = (PPDMSCATTERGATHER)RTMemCacheAlloc(pThis->hSgCache);
    if (pSgBuf)
    {
        PINTNETHDR pHdr;
        rc = INTNETRingAllocateFrame(&pThis->pBufR3->Send, (uint32_t)cbMin,
                                     &pHdr, &pSgBuf->aSegs[0].pvSeg);
#if 1 /** @todo implement VERR_TRY_AGAIN once this moves to EMT. */
        if (    RT_FAILURE(rc)
            &&  pThis->pBufR3->cbSend >= cbMin * 2 + sizeof(INTNETHDR))
        {
            INTNETIFSENDREQ SendReq;
            SendReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
            SendReq.Hdr.cbReq = sizeof(SendReq);
            SendReq.pSession = NIL_RTR0PTR;
            SendReq.hIf = pThis->hIf;
            PDMDrvHlpSUPCallVMMR0Ex(pThis->pDrvInsR3, VMMR0_DO_INTNET_IF_SEND, &SendReq, sizeof(SendReq));

            rc = INTNETRingAllocateFrame(&pThis->pBufR3->Send, (uint32_t)cbMin,
                                         &pHdr, &pSgBuf->aSegs[0].pvSeg);
        }
#endif
        if (RT_SUCCESS(rc))
        {
            pSgBuf->fFlags          = PDMSCATTERGATHER_FLAGS_MAGIC | PDMSCATTERGATHER_FLAGS_OWNER_1;
            pSgBuf->cbUsed          = 0;
            pSgBuf->cbAvailable     = cbMin;
            pSgBuf->pvUser          = pHdr;
            pSgBuf->cSegs           = 1;
            pSgBuf->aSegs[0].cbSeg  = cbMin;

            *ppSgBuf = pSgBuf;
            return VINF_SUCCESS;
        }

        /*
         * Arm the try again stuff.
         */
/** @todo        if (pThis->pBufR3->cbSend >= cbMin * 2 + sizeof(INTNETHDR))
            rc = VERR_TRY_AGAIN;
        else */
            rc = VERR_NO_MEMORY;

        RTMemCacheFree(pThis->hSgCache, pSgBuf);
    }
    else
        rc = VERR_NO_MEMORY;
    /** @todo implement VERR_TRY_AGAIN  */
    return rc;
}

/**
 * @interface_method_impl{PDMINETWORKUP,pfnSendBuf}
 */
static DECLCALLBACK(int) drvR3IntNetUp_SendBuf(PPDMINETWORKUP pInterface, PPDMSCATTERGATHER pSgBuf, bool fOnWorkerThread)
{
    return VERR_NOT_IMPLEMENTED;
}

/**
 * @interface_method_impl{PDMINETWORKUP,pfnSendDeprecated}
 */
static DECLCALLBACK(int) drvR3IntNetUp_SendDeprecated(PPDMINETWORKUP pInterface, const void *pvBuf, size_t cb)
{
    PDRVINTNET pThis = RT_FROM_MEMBER(pInterface, DRVINTNET, INetworkUpR3);
    STAM_PROFILE_START(&pThis->StatTransmit, a);

#ifdef LOG_ENABLED
    uint64_t u64Now = RTTimeProgramNanoTS();
    LogFlow(("drvR3IntNetSend: %-4d bytes at %llu ns  deltas: r=%llu t=%llu\n",
             cb, u64Now, u64Now - pThis->u64LastReceiveTS, u64Now - pThis->u64LastTransferTS));
    pThis->u64LastTransferTS = u64Now;
    Log2(("drvR3IntNetSend: pvBuf=%p cb=%#x\n"
          "%.*Rhxd\n",
          pvBuf, cb, cb, pvBuf));
#endif

    /*
     * Add the frame to the send buffer and push it onto the network.
     */
    int rc = INTNETRingWriteFrame(&pThis->pBufR3->Send, pvBuf, (uint32_t)cb);
    if (    rc == VERR_BUFFER_OVERFLOW
        &&  pThis->pBufR3->cbSend < cb)
    {
        INTNETIFSENDREQ SendReq;
        SendReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
        SendReq.Hdr.cbReq = sizeof(SendReq);
        SendReq.pSession = NIL_RTR0PTR;
        SendReq.hIf = pThis->hIf;
        PDMDrvHlpSUPCallVMMR0Ex(pThis->pDrvInsR3, VMMR0_DO_INTNET_IF_SEND, &SendReq, sizeof(SendReq));

        rc = INTNETRingWriteFrame(&pThis->pBufR3->Send, pvBuf, (uint32_t)cb);
    }

    if (RT_SUCCESS(rc))
    {
        INTNETIFSENDREQ SendReq;
        SendReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
        SendReq.Hdr.cbReq = sizeof(SendReq);
        SendReq.pSession = NIL_RTR0PTR;
        SendReq.hIf = pThis->hIf;
        rc = PDMDrvHlpSUPCallVMMR0Ex(pThis->pDrvInsR3, VMMR0_DO_INTNET_IF_SEND, &SendReq, sizeof(SendReq));
    }

    STAM_PROFILE_STOP(&pThis->StatTransmit, a);
    AssertRC(rc);
    return rc;
}


/**
 * @interface_method_impl{PDMINETWORKUP,pfnSetPromiscuousMode}
 */
static DECLCALLBACK(void) drvR3IntNetUp_SetPromiscuousMode(PPDMINETWORKUP pInterface, bool fPromiscuous)
{
    PDRVINTNET pThis = RT_FROM_MEMBER(pInterface, DRVINTNET, INetworkUpR3);
    INTNETIFSETPROMISCUOUSMODEREQ Req;
    Req.Hdr.u32Magic    = SUPVMMR0REQHDR_MAGIC;
    Req.Hdr.cbReq       = sizeof(Req);
    Req.pSession        = NIL_RTR0PTR;
    Req.hIf             = pThis->hIf;
    Req.fPromiscuous    = fPromiscuous;
    int rc = PDMDrvHlpSUPCallVMMR0Ex(pThis->pDrvInsR3, VMMR0_DO_INTNET_IF_SET_PROMISCUOUS_MODE, &Req, sizeof(Req));
    LogFlow(("drvR3IntNetUp_SetPromiscuousMode: fPromiscuous=%RTbool\n", fPromiscuous));
    AssertRC(rc);
}


/**
 * @interface_method_impl{PDMINETWORKUP,pfnNotifyLinkChanged}
 */
static DECLCALLBACK(void) drvR3IntNetUp_NotifyLinkChanged(PPDMINETWORKUP pInterface, PDMNETWORKLINKSTATE enmLinkState)
{
    PDRVINTNET pThis = RT_FROM_MEMBER(pInterface, DRVINTNET, INetworkUpR3);
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
    LogFlow(("drvR3IntNetUp_NotifyLinkChanged: enmLinkState=%d %d->%d\n", enmLinkState, pThis->fLinkDown, fLinkDown));
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
static int drvR3IntNetAsyncIoWaitForSpace(PDRVINTNET pThis)
{
    LogFlow(("drvR3IntNetAsyncIoWaitForSpace:\n"));
    STAM_PROFILE_ADV_STOP(&pThis->StatReceive, a);
    int rc = pThis->pIAboveNet->pfnWaitReceiveAvail(pThis->pIAboveNet, RT_INDEFINITE_WAIT);
    STAM_PROFILE_ADV_START(&pThis->StatReceive, a);
    LogFlow(("drvR3IntNetAsyncIoWaitForSpace: returns %Rrc\n", rc));
    return rc;
}


/**
 * Executes async I/O (RUNNING mode).
 *
 * @returns VERR_STATE_CHANGED if the state changed.
 * @returns Appropriate VBox status code (error) on fatal error.
 * @param   pThis       The driver instance data.
 */
static int drvR3IntNetAsyncIoRun(PDRVINTNET pThis)
{
    PPDMDRVINS pDrvIns = pThis->pDrvInsR3;
    LogFlow(("drvR3IntNetAsyncIoRun: pThis=%p\n", pThis));

    /*
     * The running loop - processing received data and waiting for more to arrive.
     */
    STAM_PROFILE_ADV_START(&pThis->StatReceive, a);
    PINTNETBUF      pBuf     = pThis->pBufR3;
    PINTNETRINGBUF  pRingBuf = &pBuf->Recv;
    for (;;)
    {
        /*
         * Process the receive buffer.
         */
        PINTNETHDR pHdr;
        while ((pHdr = INTNETRingGetNextFrameToRead(pRingBuf)) != NULL)
        {
            /*
             * Check the state and then inspect the packet.
             */
            if (pThis->enmState != ASYNCSTATE_RUNNING)
            {
                STAM_PROFILE_ADV_STOP(&pThis->StatReceive, a);
                LogFlow(("drvR3IntNetAsyncIoRun: returns VERR_STATE_CHANGED (state changed - #0)\n"));
                return VERR_STATE_CHANGED;
            }

            Log2(("pHdr=%p offRead=%#x: %.8Rhxs\n", pHdr, pRingBuf->offReadX, pHdr));
            if (    pHdr->u16Type == INTNETHDR_TYPE_FRAME
                &&  !pThis->fLinkDown)
            {
                /*
                 * Check if there is room for the frame and pass it up.
                 */
                size_t cbFrame = pHdr->cbFrame;
                int rc = pThis->pIAboveNet->pfnWaitReceiveAvail(pThis->pIAboveNet, 0);
                if (rc == VINF_SUCCESS)
                {
#ifdef LOG_ENABLED
                    uint64_t u64Now = RTTimeProgramNanoTS();
                    LogFlow(("drvR3IntNetAsyncIoRun: %-4d bytes at %llu ns  deltas: r=%llu t=%llu\n",
                             cbFrame, u64Now, u64Now - pThis->u64LastReceiveTS, u64Now - pThis->u64LastTransferTS));
                    pThis->u64LastReceiveTS = u64Now;
                    Log2(("drvR3IntNetAsyncIoRun: cbFrame=%#x\n"
                          "%.*Rhxd\n",
                          cbFrame, cbFrame, INTNETHdrGetFramePtr(pHdr, pBuf)));
#endif
                    rc = pThis->pIAboveNet->pfnReceive(pThis->pIAboveNet, INTNETHdrGetFramePtr(pHdr, pBuf), cbFrame);
                    AssertRC(rc);

                    /* skip to the next frame. */
                    INTNETRingSkipFrame(pRingBuf);
                }
                else
                {
                    /*
                     * Wait for sufficient space to become available and then retry.
                     */
                    rc = drvR3IntNetAsyncIoWaitForSpace(pThis);
                    if (RT_FAILURE(rc))
                    {
                        if (rc == VERR_INTERRUPTED)
                        {
                            /*
                             * NIC is going down, likely because the VM is being reset. Skip the frame.
                             */
                            AssertMsg(pHdr->u16Type == INTNETHDR_TYPE_FRAME, ("Unknown frame type %RX16! offRead=%#x\n",
                                                                              pHdr->u16Type, pRingBuf->offReadX));
                            INTNETRingSkipFrame(pRingBuf);
                        }
                        else
                        {
                            STAM_PROFILE_ADV_STOP(&pThis->StatReceive, a);
                            LogFlow(("drvR3IntNetAsyncIoRun: returns %Rrc (wait-for-space)\n", rc));
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
                                                                  pHdr->u16Type, pRingBuf->offReadX));
                INTNETRingSkipFrame(pRingBuf);
                STAM_REL_COUNTER_INC(&pBuf->cStatBadFrames);
            }
        } /* while more received data */

        /*
         * Wait for data, checking the state before we block.
         */
        if (pThis->enmState != ASYNCSTATE_RUNNING)
        {
            STAM_PROFILE_ADV_STOP(&pThis->StatReceive, a);
            LogFlow(("drvR3IntNetAsyncIoRun: returns VINF_SUCCESS (state changed - #1)\n"));
            return VERR_STATE_CHANGED;
        }
        INTNETIFWAITREQ WaitReq;
        WaitReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
        WaitReq.Hdr.cbReq    = sizeof(WaitReq);
        WaitReq.pSession     = NIL_RTR0PTR;
        WaitReq.hIf          = pThis->hIf;
        WaitReq.cMillies     = 30000; /* 30s - don't wait forever, timeout now and then. */
        STAM_PROFILE_ADV_STOP(&pThis->StatReceive, a);
        int rc = PDMDrvHlpSUPCallVMMR0Ex(pDrvIns, VMMR0_DO_INTNET_IF_WAIT, &WaitReq, sizeof(WaitReq));
        if (    RT_FAILURE(rc)
            &&  rc != VERR_TIMEOUT
            &&  rc != VERR_INTERRUPTED)
        {
            LogFlow(("drvR3IntNetAsyncIoRun: returns %Rrc\n", rc));
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
static DECLCALLBACK(int) drvR3IntNetAsyncIoThread(RTTHREAD ThreadSelf, void *pvUser)
{
    PDRVINTNET pThis = (PDRVINTNET)pvUser;
    LogFlow(("drvR3IntNetAsyncIoThread: pThis=%p\n", pThis));
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
                int rc = RTSemEventWait(pThis->hEvtSuspended, 30000);
                if (    RT_FAILURE(rc)
                    &&  rc != VERR_TIMEOUT)
                {
                    LogFlow(("drvR3IntNetAsyncIoThread: returns %Rrc\n", rc));
                    return rc;
                }
                break;
            }

            case ASYNCSTATE_RUNNING:
            {
                int rc = drvR3IntNetAsyncIoRun(pThis);
                if (    rc != VERR_STATE_CHANGED
                    &&  RT_FAILURE(rc))
                {
                    LogFlow(("drvR3IntNetAsyncIoThread: returns %Rrc\n", rc));
                    return rc;
                }
                break;
            }

            default:
                AssertMsgFailed(("Invalid state %d\n", enmState));
            case ASYNCSTATE_TERMINATE:
                LogFlow(("drvR3IntNetAsyncIoThread: returns VINF_SUCCESS\n"));
                return VINF_SUCCESS;
        }
    }
}

/* -=-=-=-=- PDMIBASERC -=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASERC,pfnQueryInterface}
 */
static DECLCALLBACK(RTRCPTR) drvR3IntNetIBaseRC_QueryInterface(PPDMIBASERC pInterface, const char *pszIID)
{
    PDRVINTNET pThis = RT_FROM_MEMBER(pInterface, DRVINTNET, IBaseRC);

    PDMIBASERC_RETURN_INTERFACE(pThis->pDrvInsR3, pszIID, PDMIBASERC, &pThis->IBaseRC);
    return NIL_RTRCPTR;
}

/* -=-=-=-=- PDMIBASER0 -=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASER0,pfnQueryInterface}
 */
static DECLCALLBACK(RTR0PTR) drvR3IntNetIBaseR0_QueryInterface(PPDMIBASER0 pInterface, const char *pszIID)
{
    PDRVINTNET pThis = RT_FROM_MEMBER(pInterface, DRVINTNET, IBaseR0);

    PDMIBASER0_RETURN_INTERFACE(pThis->pDrvInsR3, pszIID, PDMIBASER0, &pThis->IBaseR0);
    return NIL_RTR0PTR;
}

/* -=-=-=-=- PDMIBASE -=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvR3IntNetIBase_QueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVINTNET pThis   = PDMINS_2_DATA(pDrvIns, PDRVINTNET);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASER0, &pThis->IBaseR0);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASERC, &pThis->IBaseRC);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMINETWORKUP, &pThis->INetworkUpR3);
    return NULL;
}

/* -=-=-=-=- PDMDRVREG -=-=-=-=- */

/**
 * Power Off notification.
 *
 * @param   pDrvIns     The driver instance.
 */
static DECLCALLBACK(void) drvR3IntNetPowerOff(PPDMDRVINS pDrvIns)
{
    LogFlow(("drvR3IntNetPowerOff\n"));
    PDRVINTNET pThis = PDMINS_2_DATA(pDrvIns, PDRVINTNET);
    if (!pThis->fActivateEarlyDeactivateLate)
    {
        ASMAtomicXchgSize(&pThis->enmState, ASYNCSTATE_SUSPENDED);
        drvR3IntNetSetActive(pThis, false /* fActive */);
    }
}


/**
 * Resume notification.
 *
 * @param   pDrvIns     The driver instance.
 */
static DECLCALLBACK(void) drvR3IntNetResume(PPDMDRVINS pDrvIns)
{
    LogFlow(("drvR3IntNetPowerResume\n"));
    PDRVINTNET pThis = PDMINS_2_DATA(pDrvIns, PDRVINTNET);
    if (!pThis->fActivateEarlyDeactivateLate)
    {
        ASMAtomicXchgSize(&pThis->enmState, ASYNCSTATE_RUNNING);
        RTSemEventSignal(pThis->hEvtSuspended);
        drvR3IntNetUpdateMacAddress(pThis); /* (could be a state restore) */
        drvR3IntNetSetActive(pThis, true /* fActive */);
    }
    if (   PDMDrvHlpVMTeleportedAndNotFullyResumedYet(pDrvIns)
        && pThis->pIAboveConfigR3)
    {
        /*
         * We've just been teleported and need to drop a hint to the switch
         * since we're likely to have changed to a different port.  We just
         * push out some ethernet frame that doesn't mean anything to anyone.
         * For this purpose ethertype 0x801e was chosen since it was registered
         * to Sun (dunno what it is/was used for though).
         */
        union
        {
            RTNETETHERHDR   Hdr;
            uint8_t         ab[128];
        } Frame;
        RT_ZERO(Frame);
        Frame.Hdr.DstMac.au16[0] = 0xffff;
        Frame.Hdr.DstMac.au16[1] = 0xffff;
        Frame.Hdr.DstMac.au16[2] = 0xffff;
        Frame.Hdr.EtherType      = RT_H2BE_U16(0x801e);
        int rc = pThis->pIAboveConfigR3->pfnGetMac(pThis->pIAboveConfigR3, &Frame.Hdr.SrcMac);
        if (RT_SUCCESS(rc))
            rc = drvR3IntNetUp_SendDeprecated(&pThis->INetworkUpR3, &Frame, sizeof(Frame));
        if (RT_FAILURE(rc))
            LogRel(("IntNet#%u: Sending dummy frame failed: %Rrc\n", pDrvIns->iInstance, rc));
    }
}


/**
 * Suspend notification.
 *
 * @param   pDrvIns     The driver instance.
 */
static DECLCALLBACK(void) drvR3IntNetSuspend(PPDMDRVINS pDrvIns)
{
    LogFlow(("drvR3IntNetPowerSuspend\n"));
    PDRVINTNET pThis = PDMINS_2_DATA(pDrvIns, PDRVINTNET);
    if (!pThis->fActivateEarlyDeactivateLate)
    {
        ASMAtomicXchgSize(&pThis->enmState, ASYNCSTATE_SUSPENDED);
        drvR3IntNetSetActive(pThis, false /* fActive */);
    }
}


/**
 * Power On notification.
 *
 * @param   pDrvIns     The driver instance.
 */
static DECLCALLBACK(void) drvR3IntNetPowerOn(PPDMDRVINS pDrvIns)
{
    LogFlow(("drvR3IntNetPowerOn\n"));
    PDRVINTNET pThis = PDMINS_2_DATA(pDrvIns, PDRVINTNET);
    if (!pThis->fActivateEarlyDeactivateLate)
    {
        ASMAtomicXchgSize(&pThis->enmState, ASYNCSTATE_RUNNING);
        RTSemEventSignal(pThis->hEvtSuspended);
        drvR3IntNetUpdateMacAddress(pThis);
        drvR3IntNetSetActive(pThis, true /* fActive */);
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
static DECLCALLBACK(void) drvR3IntNetDestruct(PPDMDRVINS pDrvIns)
{
    LogFlow(("drvR3IntNetDestruct\n"));
    PDRVINTNET pThis = PDMINS_2_DATA(pDrvIns, PDRVINTNET);
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

    /*
     * Indicate to the thread that it's time to quit.
     */
    ASMAtomicXchgSize(&pThis->enmState, ASYNCSTATE_TERMINATE);
    ASMAtomicXchgSize(&pThis->fLinkDown, true);
    RTSEMEVENT hEvtSuspended = pThis->hEvtSuspended;
    pThis->hEvtSuspended = NIL_RTSEMEVENT;

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
        int rc = PDMDrvHlpSUPCallVMMR0Ex(pDrvIns, VMMR0_DO_INTNET_IF_CLOSE, &CloseReq, sizeof(CloseReq));
        AssertRC(rc);
    }

    /*
     * Wait for the thread to terminate.
     */
    if (pThis->Thread != NIL_RTTHREAD)
    {
        if (hEvtSuspended != NIL_RTSEMEVENT)
            RTSemEventSignal(hEvtSuspended);
        int rc = RTThreadWait(pThis->Thread, 5000, NULL);
        AssertRC(rc);
        pThis->Thread = NIL_RTTHREAD;
    }

    /*
     * Destroy the semaphore and S/G cache.
     */
    if (hEvtSuspended != NIL_RTSEMEVENT)
        RTSemEventDestroy(hEvtSuspended);

    RTMemCacheDestroy(pThis->hSgCache);
    pThis->hSgCache = NIL_RTMEMCACHE;

    /*
     * Deregister statistics in case we're being detached.
     */
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->pBufR3->Recv.cStatFrames);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->pBufR3->Recv.cbStatWritten);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->pBufR3->Recv.cOverflows);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->pBufR3->Send.cStatFrames);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->pBufR3->Send.cbStatWritten);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->pBufR3->Send.cOverflows);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->pBufR3->cStatYieldsOk);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->pBufR3->cStatYieldsNok);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->pBufR3->cStatLost);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->pBufR3->cStatBadFrames);
#ifdef VBOX_WITH_STATISTICS
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->StatReceive);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->StatTransmit);
#endif
}


/**
 * Construct a TAP network transport driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvR3IntNetConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVINTNET pThis = PDMINS_2_DATA(pDrvIns, PDRVINTNET);
    bool f;
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Init the static parts.
     */
    pThis->pDrvInsR3                                = pDrvIns;
    pThis->hIf                                      = INTNET_HANDLE_INVALID;
    pThis->Thread                                   = NIL_RTTHREAD;
    pThis->hEvtSuspended                            = NIL_RTSEMEVENT;
    pThis->hSgCache                                 = NIL_RTMEMCACHE;
    pThis->enmState                                 = ASYNCSTATE_SUSPENDED;
    pThis->fActivateEarlyDeactivateLate             = false;
    /* IBase* */
    pDrvIns->IBase.pfnQueryInterface                = drvR3IntNetIBase_QueryInterface;
    pThis->IBaseR0.pfnQueryInterface                = drvR3IntNetIBaseR0_QueryInterface;
    pThis->IBaseRC.pfnQueryInterface                = drvR3IntNetIBaseRC_QueryInterface;
    /* INetworkUp */
    pThis->INetworkUpR3.pfnAllocBuf                 = drvR3IntNetUp_AllocBuf;
    pThis->INetworkUpR3.pfnSendBuf                  = drvR3IntNetUp_SendBuf;
    pThis->INetworkUpR3.pfnSendDeprecated           = drvR3IntNetUp_SendDeprecated;
    pThis->INetworkUpR3.pfnSetPromiscuousMode       = drvR3IntNetUp_SetPromiscuousMode;
    pThis->INetworkUpR3.pfnNotifyLinkChanged        = drvR3IntNetUp_NotifyLinkChanged;

    /*
     * Validate the config.
     */
    if (!CFGMR3AreValuesValid(pCfg,
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
                              "IsService\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

    /*
     * Check that no-one is attached to us.
     */
    AssertMsgReturn(PDMDrvHlpNoAttach(pDrvIns) == VERR_PDM_NO_ATTACHED_DRIVER,
                    ("Configuration error: Not possible to attach anything to this driver!\n"),
                    VERR_PDM_DRVINS_NO_ATTACH);

    /*
     * Query the network port interface.
     */
    pThis->pIAboveNet = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMINETWORKDOWN);
    if (!pThis->pIAboveNet)
    {
        AssertMsgFailed(("Configuration error: the above device/driver didn't export the network port interface!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }
    pThis->pIAboveConfigR3 = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMINETWORKCONFIG);

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
    int rc = CFGMR3QueryString(pCfg, "Network", OpenReq.szNetwork, sizeof(OpenReq.szNetwork));
    if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"Network\" value"));
    strcpy(pThis->szNetwork, OpenReq.szNetwork);

    /** @cfgm{TrunkType, uint32_t, kIntNetTrunkType_None}
     * The trunk connection type see INTNETTRUNKTYPE.
     */
    uint32_t u32TrunkType;
    rc = CFGMR3QueryU32(pCfg, "TrunkType", &u32TrunkType);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        u32TrunkType = kIntNetTrunkType_None;
    else if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"TrunkType\" value"));
    OpenReq.enmTrunkType = (INTNETTRUNKTYPE)u32TrunkType;

    /** @cfgm{Trunk, string, ""}
     * The name of the trunk connection.
     */
    rc = CFGMR3QueryString(pCfg, "Trunk", OpenReq.szTrunk, sizeof(OpenReq.szTrunk));
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
    rc = CFGMR3QueryBool(pCfg, "RestrictAccess", &fRestrictAccess);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fRestrictAccess = true;
    else if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"RestrictAccess\" value"));
    OpenReq.fFlags = fRestrictAccess ? 0 : INTNET_OPEN_FLAGS_PUBLIC;

    /** @cfgm{IgnoreAllPromisc, boolean, false}
     * When set all request for operating any interface or trunk in promiscuous
     * mode will be ignored. */
    rc = CFGMR3QueryBoolDef(pCfg, "IgnoreAllPromisc", &f, false);
    if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"IgnoreAllPromisc\" value"));
    if (f)
        OpenReq.fFlags |= INTNET_OPEN_FLAGS_IGNORE_PROMISC;

    /** @cfgm{QuietlyIgnoreAllPromisc, boolean, false}
     * When set all request for operating any interface or trunk in promiscuous
     * mode will be ignored.  This differs from IgnoreAllPromisc in that clients
     * won't get VERR_INTNET_INCOMPATIBLE_FLAGS. */
    rc = CFGMR3QueryBoolDef(pCfg, "QuietlyIgnoreAllPromisc", &f, false);
    if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"QuietlyIgnoreAllPromisc\" value"));
    if (f)
        OpenReq.fFlags |= INTNET_OPEN_FLAGS_QUIETLY_IGNORE_PROMISC;

    /** @cfgm{IgnoreClientPromisc, boolean, false}
     * When set all request for operating any non-trunk interface in promiscuous
     * mode will be ignored. */
    rc = CFGMR3QueryBoolDef(pCfg, "IgnoreClientPromisc", &f, false);
    if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"IgnoreClientPromisc\" value"));
    if (f)
        OpenReq.fFlags |= INTNET_OPEN_FLAGS_IGNORE_PROMISC; /** @todo add special flag for this. */

    /** @cfgm{QuietlyIgnoreClientPromisc, boolean, false}
     * When set all request for operating any non-trunk interface promiscuous mode
     * will be ignored.  This differs from IgnoreClientPromisc in that clients won't
     * get VERR_INTNET_INCOMPATIBLE_FLAGS. */
    rc = CFGMR3QueryBoolDef(pCfg, "QuietlyIgnoreClientPromisc", &f, false);
    if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"QuietlyIgnoreClientPromisc\" value"));
    if (f)
        OpenReq.fFlags |= INTNET_OPEN_FLAGS_QUIETLY_IGNORE_PROMISC;  /** @todo add special flag for this. */

    /** @cfgm{IgnoreTrunkWirePromisc, boolean, false}
     * When set all request for operating the trunk-wire connection in promiscuous
     * mode will be ignored. */
    rc = CFGMR3QueryBoolDef(pCfg, "IgnoreTrunkWirePromisc", &f, false);
    if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"IgnoreTrunkWirePromisc\" value"));
    if (f)
        OpenReq.fFlags |= INTNET_OPEN_FLAGS_IGNORE_PROMISC_TRUNK_WIRE;

    /** @cfgm{QuietlyIgnoreTrunkWirePromisc, boolean, false}
     * When set all request for operating any trunk-wire connection promiscuous mode
     * will be ignored.  This differs from IgnoreTrunkWirePromisc in that clients
     * won't get VERR_INTNET_INCOMPATIBLE_FLAGS. */
    rc = CFGMR3QueryBoolDef(pCfg, "QuietlyIgnoreTrunkWirePromisc", &f, false);
    if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"QuietlyIgnoreTrunkWirePromisc\" value"));
    if (f)
        OpenReq.fFlags |= INTNET_OPEN_FLAGS_QUIETLY_IGNORE_PROMISC_TRUNK_WIRE;

    /** @cfgm{IgnoreTrunkHostPromisc, boolean, false}
     * When set all request for operating the trunk-host connection in promiscuous
     * mode will be ignored. */
    rc = CFGMR3QueryBoolDef(pCfg, "IgnoreTrunkHostPromisc", &f, false);
    if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"IgnoreTrunkHostPromisc\" value"));
    if (f)
        OpenReq.fFlags |= INTNET_OPEN_FLAGS_IGNORE_PROMISC_TRUNK_HOST;

    /** @cfgm{QuietlyIgnoreTrunkHostPromisc, boolean, false}
     * When set all request for operating any trunk-host connection promiscuous mode
     * will be ignored.  This differs from IgnoreTrunkHostPromisc in that clients
     * won't get VERR_INTNET_INCOMPATIBLE_FLAGS. */
    rc = CFGMR3QueryBoolDef(pCfg, "QuietlyIgnoreTrunkHostPromisc", &f, false);
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
    rc = CFGMR3QueryBoolDef(pCfg, "SharedMacOnWire", &fSharedMacOnWire, false);
    if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"SharedMacOnWire\" value"));
    if (fSharedMacOnWire)
        OpenReq.fFlags |= INTNET_OPEN_FLAGS_SHARED_MAC_ON_WIRE;

    /** @cfgm{ReceiveBufferSize, uint32_t, 218 KB}
     * The size of the receive buffer.
     */
    rc = CFGMR3QueryU32(pCfg, "ReceiveBufferSize", &OpenReq.cbRecv);
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
    rc = CFGMR3QueryU32(pCfg, "SendBufferSize", &OpenReq.cbSend);
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
    rc = CFGMR3QueryBool(pCfg, "IsService", &pThis->fActivateEarlyDeactivateLate);
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
#endif /* DARWIN */

    /*
     * Create the event semaphore and S/G cache.
     */
    rc = RTSemEventCreate(&pThis->hEvtSuspended);
    if (RT_FAILURE(rc))
        return rc;
    rc = RTMemCacheCreate(&pThis->hSgCache, sizeof(PDMSCATTERGATHER), 0, UINT32_MAX, NULL, NULL, pThis, 0);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Create the interface.
     */
    OpenReq.hIf = INTNET_HANDLE_INVALID;
    rc = PDMDrvHlpSUPCallVMMR0Ex(pDrvIns, VMMR0_DO_INTNET_OPEN, &OpenReq, sizeof(OpenReq));
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("Failed to open/create the internal network '%s'"), pThis->szNetwork);
    AssertRelease(OpenReq.hIf != INTNET_HANDLE_INVALID);
    pThis->hIf = OpenReq.hIf;
    Log(("IntNet%d: hIf=%RX32 '%s'\n", pDrvIns->iInstance, pThis->hIf, pThis->szNetwork));

    /*
     * Get default buffer.
     */
    INTNETIFGETRING3BUFFERREQ GetRing3BufferReq;
    GetRing3BufferReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    GetRing3BufferReq.Hdr.cbReq = sizeof(GetRing3BufferReq);
    GetRing3BufferReq.pSession = NIL_RTR0PTR;
    GetRing3BufferReq.hIf = pThis->hIf;
    GetRing3BufferReq.pRing3Buf = NULL;
    rc = PDMDrvHlpSUPCallVMMR0Ex(pDrvIns, VMMR0_DO_INTNET_IF_GET_RING3_BUFFER, &GetRing3BufferReq, sizeof(GetRing3BufferReq));
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("Failed to get ring-3 buffer for the newly created interface to '%s'"), pThis->szNetwork);
    AssertRelease(VALID_PTR(GetRing3BufferReq.pRing3Buf));
    pThis->pBufR3 = GetRing3BufferReq.pRing3Buf;

    /*
     * Create the async I/O thread.
     * Note! Using a PDM thread here doesn't fit with the IsService=true operation.
     */
    rc = RTThreadCreate(&pThis->Thread, drvR3IntNetAsyncIoThread, pThis, _128K, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "INTNET");
    if (RT_FAILURE(rc))
    {
        AssertRC(rc);
        return rc;
    }
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->pBufR3->Recv.cbStatWritten, STAMTYPE_COUNTER,   STAMVISIBILITY_ALWAYS,  STAMUNIT_BYTES, "Number of received bytes.",    "/Net/IntNet%d/Bytes/Received", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->pBufR3->Send.cbStatWritten, STAMTYPE_COUNTER,   STAMVISIBILITY_ALWAYS,  STAMUNIT_BYTES, "Number of sent bytes.",        "/Net/IntNet%d/Bytes/Sent", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->pBufR3->Recv.cOverflows,    STAMTYPE_COUNTER,   STAMVISIBILITY_ALWAYS,  STAMUNIT_COUNT, "Number overflows.",            "/Net/IntNet%d/Overflows/Recv", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->pBufR3->Send.cOverflows,    STAMTYPE_COUNTER,   STAMVISIBILITY_ALWAYS,  STAMUNIT_COUNT, "Number overflows.",            "/Net/IntNet%d/Overflows/Sent", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->pBufR3->Recv.cStatFrames,   STAMTYPE_COUNTER,   STAMVISIBILITY_ALWAYS,  STAMUNIT_COUNT, "Number of received packets.",  "/Net/IntNet%d/Packets/Received", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->pBufR3->Send.cStatFrames,   STAMTYPE_COUNTER,   STAMVISIBILITY_ALWAYS,  STAMUNIT_COUNT, "Number of sent packets.",      "/Net/IntNet%d/Packets/Sent", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->pBufR3->cStatLost,          STAMTYPE_COUNTER,   STAMVISIBILITY_ALWAYS,  STAMUNIT_COUNT, "Number of lost packets.",      "/Net/IntNet%d/Packets/Lost", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->pBufR3->cStatYieldsNok,     STAMTYPE_COUNTER,   STAMVISIBILITY_ALWAYS,  STAMUNIT_COUNT, "Number of times yielding helped fix an overflow.",      "/Net/IntNet%d/YieldOk", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->pBufR3->cStatYieldsOk,      STAMTYPE_COUNTER,   STAMVISIBILITY_ALWAYS,  STAMUNIT_COUNT, "Number of times yielding didn't help fix an overflow.", "/Net/IntNet%d/YieldNok", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->pBufR3->cStatBadFrames,     STAMTYPE_COUNTER,   STAMVISIBILITY_ALWAYS,  STAMUNIT_COUNT, "Number of bad frames seed by the consumers.",           "/Net/IntNet%d/BadFrames", pDrvIns->iInstance);
#ifdef VBOX_WITH_STATISTICS
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatReceive,                STAMTYPE_PROFILE,   STAMVISIBILITY_ALWAYS,  STAMUNIT_TICKS_PER_CALL, "Profiling packet receive runs.",  "/Net/IntNet%d/Receive", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatTransmit,               STAMTYPE_PROFILE,   STAMVISIBILITY_ALWAYS,  STAMUNIT_TICKS_PER_CALL, "Profiling packet transmit runs.", "/Net/IntNet%d/Transmit", pDrvIns->iInstance);
#endif

    /*
     * Activate data transmission as early as possible
     */
    if (pThis->fActivateEarlyDeactivateLate)
    {
        ASMAtomicXchgSize(&pThis->enmState, ASYNCSTATE_RUNNING);
        RTSemEventSignal(pThis->hEvtSuspended);
        drvR3IntNetUpdateMacAddress(pThis);
        drvR3IntNetSetActive(pThis, true /* fActive */);
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
    /* szName */
    "IntNet",
    /* szRCMod */
    "VBoxDD",
    /* szR0Mod */
    "VBoxDD",
    /* pszDescription */
    "Internal Networking Transport Driver",
    /* fFlags */
#ifdef VBOX_WITH_R0_AND_RC_DRIVERS
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DRVREG_FLAGS_R0 | PDM_DRVREG_FLAGS_RC,
#else
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
#endif
    /* fClass. */
    PDM_DRVREG_CLASS_NETWORK,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVINTNET),
    /* pfnConstruct */
    drvR3IntNetConstruct,
    /* pfnDestruct */
    drvR3IntNetDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    drvR3IntNetPowerOn,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    drvR3IntNetSuspend,
    /* pfnResume */
    drvR3IntNetResume,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    drvR3IntNetPowerOff,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

#endif /* IN_RING3 */

