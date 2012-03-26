/* $Id$ */
/** @file
 * NetShaperFilter - Network shaper filter driver.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_NET_SHAPER

#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/pdmnetifs.h>
#include <VBox/vmm/pdmnetshaper.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/string.h>
#include <iprt/uuid.h>

#include "VBoxDD.h"

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Block driver instance data.
 *
 * @implements  PDMINETWORKUP
 * @implements  PDMINETWORKDOWN
 * @implements  PDMINETWORKCONFIG
 */
typedef struct DRVNETSHAPER
{
    /** The network interface. */
    PDMINETWORKUP           INetworkUp;
    /** The network interface. */
    PDMINETWORKDOWN         INetworkDown;
    /** The network config interface.
     * @todo this is a main interface and shouldn't be here...  */
    PDMINETWORKCONFIG       INetworkConfig;
    /** The port we're attached to. */
    PPDMINETWORKDOWN        pIAboveNet;
    /** The config port interface we're attached to. */
    PPDMINETWORKCONFIG      pIAboveConfig;
    /** The connector that's attached to us. */
    PPDMINETWORKUP          pIBelowNet;
    /** The name of bandwidth group we are attached to. */
    char *                  pszBwGroup;
    /** The filter that represents us at bandwidth group. */
    PDMNSFILTER             Filter;
    /** Pointer to the driver instance. */
    PPDMDRVINS              pDrvIns;
    /** For when we're the leaf driver. */
    RTCRITSECT              XmitLock;

    /** TX: Total number of bytes to allocate. */
    STAMCOUNTER             StatXmitBytesRequested;
    /** TX: Number of bytes delayed. */
    STAMCOUNTER             StatXmitBytesDenied;
    /** TX: Number of bytes allowed to pass. */
    STAMCOUNTER             StatXmitBytesGranted;
    /** TX: Total number of packets being sent. */
    STAMCOUNTER             StatXmitPktsRequested;
    /** TX: Number of packets delayed. */
    STAMCOUNTER             StatXmitPktsDenied;
    /** TX: Number of packets allowed to pass. */
    STAMCOUNTER             StatXmitPktsGranted;
    /** TX: Number of calls to pfnXmitPending. */
    STAMCOUNTER             StatXmitPendingCalled;
} DRVNETSHAPER, *PDRVNETSHAPER;


/**
 * @interface_method_impl{PDMINETWORKUP,pfnBeginXmit}
 */
static DECLCALLBACK(int) drvNetShaperUp_BeginXmit(PPDMINETWORKUP pInterface, bool fOnWorkerThread)
{
    PDRVNETSHAPER pThis = RT_FROM_MEMBER(pInterface, DRVNETSHAPER, INetworkUp);
    if (RT_UNLIKELY(!pThis->pIBelowNet))
    {
        int rc = RTCritSectTryEnter(&pThis->XmitLock);
        if (RT_UNLIKELY(rc == VERR_SEM_BUSY))
            rc = VERR_TRY_AGAIN;
        return rc;
    }
    return pThis->pIBelowNet->pfnBeginXmit(pThis->pIBelowNet, fOnWorkerThread);
}


/**
 * @interface_method_impl{PDMINETWORKUP,pfnAllocBuf}
 */
static DECLCALLBACK(int) drvNetShaperUp_AllocBuf(PPDMINETWORKUP pInterface, size_t cbMin,
                                                  PCPDMNETWORKGSO pGso, PPPDMSCATTERGATHER ppSgBuf)
{
    PDRVNETSHAPER pThis = RT_FROM_MEMBER(pInterface, DRVNETSHAPER, INetworkUp);
    if (RT_UNLIKELY(!pThis->pIBelowNet))
        return VERR_NET_DOWN;
    //LogFlow(("drvNetShaperUp_AllocBuf: cb=%d\n", cbMin));
    STAM_REL_COUNTER_ADD(&pThis->StatXmitBytesRequested, cbMin);
    STAM_REL_COUNTER_INC(&pThis->StatXmitPktsRequested);
    if (!PDMR3NsAllocateBandwidth(&pThis->Filter, cbMin))
    {
        STAM_REL_COUNTER_ADD(&pThis->StatXmitBytesDenied, cbMin);
        STAM_REL_COUNTER_INC(&pThis->StatXmitPktsDenied);
        return VERR_TRY_AGAIN;
    }
    STAM_REL_COUNTER_ADD(&pThis->StatXmitBytesGranted, cbMin);
    STAM_REL_COUNTER_INC(&pThis->StatXmitPktsGranted);
    //LogFlow(("drvNetShaperUp_AllocBuf: got cb=%d\n", cbMin));
    return pThis->pIBelowNet->pfnAllocBuf(pThis->pIBelowNet, cbMin, pGso, ppSgBuf);
}


/**
 * @interface_method_impl{PDMINETWORKUP,pfnFreeBuf}
 */
static DECLCALLBACK(int) drvNetShaperUp_FreeBuf(PPDMINETWORKUP pInterface, PPDMSCATTERGATHER pSgBuf)
{
    PDRVNETSHAPER pThis = RT_FROM_MEMBER(pInterface, DRVNETSHAPER, INetworkUp);
    if (RT_UNLIKELY(!pThis->pIBelowNet))
        return VERR_NET_DOWN;
    return pThis->pIBelowNet->pfnFreeBuf(pThis->pIBelowNet, pSgBuf);
}


/**
 * @interface_method_impl{PDMINETWORKUP,pfnSendBuf}
 */
static DECLCALLBACK(int) drvNetShaperUp_SendBuf(PPDMINETWORKUP pInterface, PPDMSCATTERGATHER pSgBuf, bool fOnWorkerThread)
{
    PDRVNETSHAPER pThis = RT_FROM_MEMBER(pInterface, DRVNETSHAPER, INetworkUp);
    if (RT_UNLIKELY(!pThis->pIBelowNet))
        return VERR_NET_DOWN;

    return pThis->pIBelowNet->pfnSendBuf(pThis->pIBelowNet, pSgBuf, fOnWorkerThread);
}


/**
 * @interface_method_impl{PDMINETWORKUP,pfnEndXmit}
 */
static DECLCALLBACK(void) drvNetShaperUp_EndXmit(PPDMINETWORKUP pInterface)
{
    //LogFlow(("drvNetShaperUp_EndXmit:\n"));
    PDRVNETSHAPER pThis = RT_FROM_MEMBER(pInterface, DRVNETSHAPER, INetworkUp);
    if (RT_LIKELY(pThis->pIBelowNet))
        pThis->pIBelowNet->pfnEndXmit(pThis->pIBelowNet);
    else
        RTCritSectLeave(&pThis->XmitLock);
}


/**
 * @interface_method_impl{PDMINETWORKUP,pfnSetPromiscuousMode}
 */
static DECLCALLBACK(void) drvNetShaperUp_SetPromiscuousMode(PPDMINETWORKUP pInterface, bool fPromiscuous)
{
    LogFlow(("drvNetShaperUp_SetPromiscuousMode: fPromiscuous=%d\n", fPromiscuous));
    PDRVNETSHAPER pThis = RT_FROM_MEMBER(pInterface, DRVNETSHAPER, INetworkUp);
    if (pThis->pIBelowNet)
        pThis->pIBelowNet->pfnSetPromiscuousMode(pThis->pIBelowNet, fPromiscuous);
}


/**
 * @interface_method_impl{PDMINETWORKUP,pfnNotifyLinkChanged}
 */
static DECLCALLBACK(void) drvNetShaperUp_NotifyLinkChanged(PPDMINETWORKUP pInterface, PDMNETWORKLINKSTATE enmLinkState)
{
    LogFlow(("drvNetShaperUp_NotifyLinkChanged: enmLinkState=%d\n", enmLinkState));
    PDRVNETSHAPER pThis = RT_FROM_MEMBER(pInterface, DRVNETSHAPER, INetworkUp);
    if (pThis->pIBelowNet)
        pThis->pIBelowNet->pfnNotifyLinkChanged(pThis->pIBelowNet, enmLinkState);
}


/**
 * @interface_method_impl{PDMINETWORKDOWN,pfnWaitReceiveAvail}
 */
static DECLCALLBACK(int) drvNetShaperDown_WaitReceiveAvail(PPDMINETWORKDOWN pInterface, RTMSINTERVAL cMillies)
{
    PDRVNETSHAPER pThis = RT_FROM_MEMBER(pInterface, DRVNETSHAPER, INetworkDown);
    return pThis->pIAboveNet->pfnWaitReceiveAvail(pThis->pIAboveNet, cMillies);
}


/**
 * @interface_method_impl{PDMINETWORKDOWN,pfnReceive}
 */
static DECLCALLBACK(int) drvNetShaperDown_Receive(PPDMINETWORKDOWN pInterface, const void *pvBuf, size_t cb)
{
    PDRVNETSHAPER pThis = RT_FROM_MEMBER(pInterface, DRVNETSHAPER, INetworkDown);
    return pThis->pIAboveNet->pfnReceive(pThis->pIAboveNet, pvBuf, cb);
}


/**
 * @interface_method_impl{PDMINETWORKDOWN,pfnXmitPending}
 */
static DECLCALLBACK(void) drvNetShaperDown_XmitPending(PPDMINETWORKDOWN pInterface)
{
    PDRVNETSHAPER pThis = RT_FROM_MEMBER(pInterface, DRVNETSHAPER, INetworkDown);
    STAM_REL_COUNTER_INC(&pThis->StatXmitPendingCalled);
    pThis->pIAboveNet->pfnXmitPending(pThis->pIAboveNet);
}


/**
 * Gets the current Media Access Control (MAC) address.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   pMac            Where to store the MAC address.
 * @thread  EMT
 */
static DECLCALLBACK(int) drvNetShaperDownCfg_GetMac(PPDMINETWORKCONFIG pInterface, PRTMAC pMac)
{
    PDRVNETSHAPER pThis = RT_FROM_MEMBER(pInterface, DRVNETSHAPER, INetworkConfig);
    return pThis->pIAboveConfig->pfnGetMac(pThis->pIAboveConfig, pMac);
}

/**
 * Gets the new link state.
 *
 * @returns The current link state.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @thread  EMT
 */
static DECLCALLBACK(PDMNETWORKLINKSTATE) drvNetShaperDownCfg_GetLinkState(PPDMINETWORKCONFIG pInterface)
{
    PDRVNETSHAPER pThis = RT_FROM_MEMBER(pInterface, DRVNETSHAPER, INetworkConfig);
    return pThis->pIAboveConfig->pfnGetLinkState(pThis->pIAboveConfig);
}

/**
 * Sets the new link state.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   enmState        The new link state
 * @thread  EMT
 */
static DECLCALLBACK(int) drvNetShaperDownCfg_SetLinkState(PPDMINETWORKCONFIG pInterface, PDMNETWORKLINKSTATE enmState)
{
    PDRVNETSHAPER pThis = RT_FROM_MEMBER(pInterface, DRVNETSHAPER, INetworkConfig);
    return pThis->pIAboveConfig->pfnSetLinkState(pThis->pIAboveConfig, enmState);
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvNetShaperQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS     pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVNETSHAPER  pThis   = PDMINS_2_DATA(pDrvIns, PDRVNETSHAPER);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMINETWORKUP, &pThis->INetworkUp);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMINETWORKDOWN, &pThis->INetworkDown);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMINETWORKCONFIG, &pThis->INetworkConfig);
    return NULL;
}


/**
 * @interface_method_impl{PDMDRVREG,pfnDetach}
 */
static DECLCALLBACK(void) drvNetShaperDetach(PPDMDRVINS pDrvIns, uint32_t fFlags)
{
    PDRVNETSHAPER pThis = PDMINS_2_DATA(pDrvIns, PDRVNETSHAPER);

    LogFlow(("drvNetShaperDetach: pDrvIns: %p, fFlags: %u\n", pDrvIns, fFlags));
    RTCritSectEnter(&pThis->XmitLock);
    pThis->pIBelowNet = NULL;
    RTCritSectLeave(&pThis->XmitLock);
}


/**
 * @interface_method_impl{PDMDRVREG,pfnAttach}
 */
static DECLCALLBACK(int) drvNetShaperAttach(PPDMDRVINS pDrvIns, uint32_t fFlags)
{
    PDRVNETSHAPER pThis = PDMINS_2_DATA(pDrvIns, PDRVNETSHAPER);
    LogFlow(("drvNetShaperAttach/#%#x: fFlags=%#x\n", pDrvIns->iInstance, fFlags));
    RTCritSectEnter(&pThis->XmitLock);

    /*
     * Query the network connector interface.
     */
    PPDMIBASE   pBaseDown;
    int rc = PDMDrvHlpAttach(pDrvIns, fFlags, &pBaseDown);
    if (   rc == VERR_PDM_NO_ATTACHED_DRIVER
        || rc == VERR_PDM_CFG_MISSING_DRIVER_NAME)
    {
        pThis->pIBelowNet = NULL;
        rc = VINF_SUCCESS;
    }
    else if (RT_SUCCESS(rc))
    {
        pThis->pIBelowNet = PDMIBASE_QUERY_INTERFACE(pBaseDown, PDMINETWORKUP);
        if (pThis->pIBelowNet)
            rc = VINF_SUCCESS;
        else
        {
            AssertMsgFailed(("Configuration error: the driver below didn't export the network connector interface!\n"));
            rc = VERR_PDM_MISSING_INTERFACE_BELOW;
        }
    }
    else
        AssertMsgFailed(("Failed to attach to driver below! rc=%Rrc\n", rc));

    RTCritSectLeave(&pThis->XmitLock);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDRVREG,pfnDestruct}
 */
static DECLCALLBACK(void) drvNetShaperDestruct(PPDMDRVINS pDrvIns)
{
    PDRVNETSHAPER pThis = PDMINS_2_DATA(pDrvIns, PDRVNETSHAPER);
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

    PDMDrvHlpNetShaperDetach(pDrvIns, &pThis->Filter);

    if (RTCritSectIsInitialized(&pThis->XmitLock))
        RTCritSectDelete(&pThis->XmitLock);
}


/**
 * @interface_method_impl{Construct a NAT network transport driver instance,
 *                       PDMDRVREG,pfnDestruct}
 */
static DECLCALLBACK(int) drvNetShaperConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVNETSHAPER pThis = PDMINS_2_DATA(pDrvIns, PDRVNETSHAPER);
    LogFlow(("drvNetShaperConstruct:\n"));
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Init the static parts.
     */
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface                = drvNetShaperQueryInterface;
    /* INetworkUp */
    pThis->INetworkUp.pfnBeginXmit                  = drvNetShaperUp_BeginXmit;
    pThis->INetworkUp.pfnAllocBuf                   = drvNetShaperUp_AllocBuf;
    pThis->INetworkUp.pfnFreeBuf                    = drvNetShaperUp_FreeBuf;
    pThis->INetworkUp.pfnSendBuf                    = drvNetShaperUp_SendBuf;
    pThis->INetworkUp.pfnEndXmit                    = drvNetShaperUp_EndXmit;
    pThis->INetworkUp.pfnSetPromiscuousMode         = drvNetShaperUp_SetPromiscuousMode;
    pThis->INetworkUp.pfnNotifyLinkChanged          = drvNetShaperUp_NotifyLinkChanged;
    /* INetworkDown */
    pThis->INetworkDown.pfnWaitReceiveAvail         = drvNetShaperDown_WaitReceiveAvail;
    pThis->INetworkDown.pfnReceive                  = drvNetShaperDown_Receive;
    pThis->INetworkDown.pfnXmitPending              = drvNetShaperDown_XmitPending;
    /* INetworkConfig */
    pThis->INetworkConfig.pfnGetMac                 = drvNetShaperDownCfg_GetMac;
    pThis->INetworkConfig.pfnGetLinkState           = drvNetShaperDownCfg_GetLinkState;
    pThis->INetworkConfig.pfnSetLinkState           = drvNetShaperDownCfg_SetLinkState;

    /*
     * Create the locks.
     */
    int rc = RTCritSectInit(&pThis->XmitLock);
    AssertRCReturn(rc, rc);

    /*
     * Validate the config.
     */
    if (!CFGMR3AreValuesValid(pCfg, "BwGroup\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

    /*
     * Find the bandwidth group we have to attach to.
     */
    rc = CFGMR3QueryStringAlloc(pCfg, "BwGroup", &pThis->pszBwGroup);
    if (RT_FAILURE(rc) && rc != VERR_CFGM_VALUE_NOT_FOUND)
    {
        rc = PDMDRV_SET_ERROR(pDrvIns, rc,
                              N_("DrvNetShaper: Configuration error: Querying \"BwGroup\" as string failed"));
        return rc;
    }
    else
        rc = VINF_SUCCESS;

    rc = PDMDrvHlpNetShaperAttach(pDrvIns, pThis->pszBwGroup, &pThis->Filter);
    if (RT_FAILURE(rc))
    {
        rc = PDMDRV_SET_ERROR(pDrvIns, rc,
                              N_("DrvNetShaper: Configuration error: Failed to attach to bandwidth group"));
        return rc;
    }

    /*
     * Query the network port interface.
     */
    pThis->pIAboveNet = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMINETWORKDOWN);
    if (!pThis->pIAboveNet)
    {
        AssertMsgFailed(("Configuration error: the above device/driver didn't export the network port interface!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }

    /*
     * Query the network config interface.
     */
    pThis->pIAboveConfig = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMINETWORKCONFIG);
    if (!pThis->pIAboveConfig)
    {
        AssertMsgFailed(("Configuration error: the above device/driver didn't export the network config interface!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }

    /*
     * Query the network connector interface.
     */
    PPDMIBASE   pBaseDown;
    rc = PDMDrvHlpAttach(pDrvIns, fFlags, &pBaseDown);
    if (   rc == VERR_PDM_NO_ATTACHED_DRIVER
        || rc == VERR_PDM_CFG_MISSING_DRIVER_NAME)
        pThis->pIBelowNet = NULL;
    else if (RT_SUCCESS(rc))
    {
        pThis->pIBelowNet = PDMIBASE_QUERY_INTERFACE(pBaseDown, PDMINETWORKUP);
        if (!pThis->pIBelowNet)
        {
            AssertMsgFailed(("Configuration error: the driver below didn't export the network connector interface!\n"));
            return VERR_PDM_MISSING_INTERFACE_BELOW;
        }
    }
    else
    {
        AssertMsgFailed(("Failed to attach to driver below! rc=%Rrc\n", rc));
        return rc;
    }

    /*
     * Register statistics.
     */
    PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->StatXmitBytesRequested, "Bytes/Tx/Requested",   STAMUNIT_BYTES, "Number of requested TX bytes.");
    PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->StatXmitBytesDenied,    "Bytes/Tx/Denied",      STAMUNIT_BYTES, "Number of denied TX bytes.");
    PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->StatXmitBytesGranted,   "Bytes/Tx/Granted",     STAMUNIT_BYTES, "Number of granted TX bytes.");

    PDMDrvHlpSTAMRegCounter(pDrvIns, &pThis->StatXmitPktsRequested,  "Packets/Tx/Requested", "Number of requested TX packets.");
    PDMDrvHlpSTAMRegCounter(pDrvIns, &pThis->StatXmitPktsDenied,     "Packets/Tx/Denied",    "Number of denied TX packets.");
    PDMDrvHlpSTAMRegCounter(pDrvIns, &pThis->StatXmitPktsGranted,    "Packets/Tx/Granted",   "Number of granted TX packets.");
    PDMDrvHlpSTAMRegCounter(pDrvIns, &pThis->StatXmitPendingCalled,  "Tx/WakeUp",            "Number of wakeup TX calls.");

    return VINF_SUCCESS;
}



/**
 * Network sniffer filter driver registration record.
 */
const PDMDRVREG g_DrvNetShaper =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "NetShaper",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Network Shaper Filter Driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_NETWORK,
    /* cMaxInstances */
    UINT32_MAX,
    /* cbInstance */
    sizeof(DRVNETSHAPER),
    /* pfnConstruct */
    drvNetShaperConstruct,
    /* pfnDestruct */
    drvNetShaperDestruct,
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
    drvNetShaperAttach,
    /* pfnDetach */
    drvNetShaperDetach,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

