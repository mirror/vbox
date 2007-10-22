/** @file
 *
 * VBox network devices:
 * NAT network transport driver
 */

/*
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
#define LOG_GROUP LOG_GROUP_DRV_NAT
#include "Network/slirp/libslirp.h"
#include <VBox/pdmdrv.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/critsect.h>

#include "Builtins.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Block driver instance data.
 */
typedef struct DRVNAT
{
    /** The network interface. */
    PDMINETWORKCONNECTOR    INetworkConnector;
    /** The port we're attached to. */
    PPDMINETWORKPORT        pPort;
    /** Pointer to the driver instance. */
    PPDMDRVINS              pDrvIns;
    /** Slirp critical section. */
    RTCRITSECT              CritSect;
    /** Link state */
    PDMNETWORKLINKSTATE     enmLinkState;
    /** NAT state for this instance. */
    PNATState               pNATState;
    /** TFTP directory prefix. */
    char                    *pszTFTPPrefix;
    /** Boot file name to provide in the DHCP server response. */
    char                    *pszBootFile;
} DRVNAT, *PDRVNAT;

/** Converts a pointer to NAT::INetworkConnector to a PRDVNAT. */
#define PDMINETWORKCONNECTOR_2_DRVNAT(pInterface)   ( (PDRVNAT)((uintptr_t)pInterface - RT_OFFSETOF(DRVNAT, INetworkConnector)) )


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#if 0
/** If set the thread should terminate. */
static bool             g_fThreadTerm = false;
/** The thread id of the select thread (drvNATSelectThread()). */
static RTTHREAD         g_ThreadSelect;
#endif


/**
 * Send data to the network.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   pvBuf           Data to send.
 * @param   cb              Number of bytes to send.
 * @thread  EMT
 */
static DECLCALLBACK(int) drvNATSend(PPDMINETWORKCONNECTOR pInterface, const void *pvBuf, size_t cb)
{
    PDRVNAT    pData = PDMINETWORKCONNECTOR_2_DRVNAT(pInterface);

    LogFlow(("drvNATSend: pvBuf=%p cb=%#x\n", pvBuf, cb));
    Log2(("drvNATSend: pvBuf=%p cb=%#x\n"
          "%.*Vhxd\n",
          pvBuf, cb, cb, pvBuf));

    int rc = RTCritSectEnter(&pData->CritSect);
    AssertReleaseRC(rc);

    Assert(pData->enmLinkState == PDMNETWORKLINKSTATE_UP);
    if (pData->enmLinkState == PDMNETWORKLINKSTATE_UP)
        slirp_input(pData->pNATState, (uint8_t *)pvBuf, cb);
    RTCritSectLeave(&pData->CritSect);
    LogFlow(("drvNATSend: end\n"));
    return VINF_SUCCESS;
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
static DECLCALLBACK(void) drvNATSetPromiscuousMode(PPDMINETWORKCONNECTOR pInterface, bool fPromiscuous)
{
    LogFlow(("drvNATSetPromiscuousMode: fPromiscuous=%d\n", fPromiscuous));
    /* nothing to do */
}


/**
 * Notification on link status changes.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   enmLinkState    The new link state.
 * @thread  EMT
 */
static DECLCALLBACK(void) drvNATNotifyLinkChanged(PPDMINETWORKCONNECTOR pInterface, PDMNETWORKLINKSTATE enmLinkState)
{
    PDRVNAT    pData = PDMINETWORKCONNECTOR_2_DRVNAT(pInterface);

    LogFlow(("drvNATNotifyLinkChanged: enmLinkState=%d\n", enmLinkState));

    int rc = RTCritSectEnter(&pData->CritSect);
    AssertReleaseRC(rc);
    pData->enmLinkState = enmLinkState;

    switch (enmLinkState)
    {
        case PDMNETWORKLINKSTATE_UP:
            LogRel(("NAT: link up\n"));
            slirp_link_up(pData->pNATState);
            break;

        case PDMNETWORKLINKSTATE_DOWN:
        case PDMNETWORKLINKSTATE_DOWN_RESUME:
            LogRel(("NAT: link down\n"));
            slirp_link_down(pData->pNATState);
            break;

        default:
            AssertMsgFailed(("drvNATNotifyLinkChanged: unexpected link state %d\n", enmLinkState));
    }
    RTCritSectLeave(&pData->CritSect);
}


/**
 * More receive buffer has become available.
 *
 * This is called when the NIC frees up receive buffers.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @thread  EMT
 */
static DECLCALLBACK(void) drvNATNotifyCanReceive(PPDMINETWORKCONNECTOR pInterface)
{
    LogFlow(("drvNATNotifyCanReceive:\n"));
    /** @todo do something useful here. */
}


/**
 * Poller callback.
 */
static DECLCALLBACK(void) drvNATPoller(PPDMDRVINS pDrvIns)
{
    PDRVNAT pData = PDMINS2DATA(pDrvIns, PDRVNAT);
    fd_set  ReadFDs;
    fd_set  WriteFDs;
    fd_set  XcptFDs;
    int     cFDs = -1;
    FD_ZERO(&ReadFDs);
    FD_ZERO(&WriteFDs);
    FD_ZERO(&XcptFDs);

    int rc = RTCritSectEnter(&pData->CritSect);
    AssertReleaseRC(rc);

    slirp_select_fill(pData->pNATState, &cFDs, &ReadFDs, &WriteFDs, &XcptFDs);

    struct timeval tv = {0, 0}; /* no wait */
    int cReadFDs = select(cFDs + 1, &ReadFDs, &WriteFDs, &XcptFDs, &tv);
    if (cReadFDs >= 0)
        slirp_select_poll(pData->pNATState, &ReadFDs, &WriteFDs, &XcptFDs);

    RTCritSectLeave(&pData->CritSect);
}


/**
 * Function called by slirp to check if it's possible to feed incoming data to the network port.
 * @returns 1 if possible.
 * @returns 0 if not possible.
 */
int slirp_can_output(void *pvUser)
{
    PDRVNAT pData = (PDRVNAT)pvUser;

    Assert(pData);

    /** Happens during termination */
    if (!RTCritSectIsOwner(&pData->CritSect))
        return 0;

    return pData->pPort->pfnCanReceive(pData->pPort);
}


/**
 * Function called by slirp to feed incoming data to the network port.
 */
void slirp_output(void *pvUser, const uint8_t *pu8Buf, int cb)
{
    PDRVNAT pData = (PDRVNAT)pvUser;

    LogFlow(("slirp_output BEGIN %x %d\n", pu8Buf, cb));
    Log2(("slirp_output: pu8Buf=%p cb=%#x (pData=%p)\n"
          "%.*Vhxd\n",
          pu8Buf, cb, pData,
          cb, pu8Buf));

    Assert(pData);

    /** Happens during termination */
    if (!RTCritSectIsOwner(&pData->CritSect))
        return;

    int rc = pData->pPort->pfnReceive(pData->pPort, pu8Buf, cb);
    AssertRC(rc);
    LogFlow(("slirp_output END %x %d\n", pu8Buf, cb));
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
static DECLCALLBACK(void *) drvNATQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVNAT pData = PDMINS2DATA(pDrvIns, PDRVNAT);
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
static DECLCALLBACK(void) drvNATDestruct(PPDMDRVINS pDrvIns)
{
    PDRVNAT pData = PDMINS2DATA(pDrvIns, PDRVNAT);

    LogFlow(("drvNATDestruct:\n"));

    int rc = RTCritSectEnter(&pData->CritSect);
    AssertReleaseRC(rc);
    slirp_term(pData->pNATState);
    pData->pNATState = NULL;
    RTCritSectLeave(&pData->CritSect);

    RTCritSectDelete(&pData->CritSect);
}


/**
 * Sets up the redirectors.
 *
 * @returns VBox status code.
 * @param   pCfgHandle      The drivers configuration handle.
 */
static int drvNATConstructRedir(unsigned iInstance, PDRVNAT pData, PCFGMNODE pCfgHandle)
{
    /*
     * Enumerate redirections.
     */
    for (PCFGMNODE pNode = CFGMR3GetFirstChild(pCfgHandle); pNode; pNode = CFGMR3GetNextChild(pNode))
    {
        /*
         * Validate the port forwarding config.
         */
        if (!CFGMR3AreValuesValid(pNode, "Protocol\0UDP\0HostPort\0GuestPort\0GuestIP\0"))
            return PDMDRV_SET_ERROR(pData->pDrvIns, VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES, N_("Unknown configuration in port forwarding"));

        /* protocol type */
        bool fUDP;
        char szProtocol[32];
        int rc = CFGMR3QueryString(pNode, "Protocol", &szProtocol[0], sizeof(szProtocol));
        if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        {
            rc = CFGMR3QueryBool(pNode, "UDP", &fUDP);
            if (rc == VERR_CFGM_VALUE_NOT_FOUND)
                fUDP = false;
            else if (VBOX_FAILURE(rc))
                return PDMDrvHlpVMSetError(pData->pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"UDP\" boolean returned %Vrc"), iInstance, rc);
        }
        else if (VBOX_SUCCESS(rc))
        {
            if (!RTStrICmp(szProtocol, "TCP"))
                fUDP = false;
            else if (!RTStrICmp(szProtocol, "UDP"))
                fUDP = true;
            else
                return PDMDrvHlpVMSetError(pData->pDrvIns, VERR_INVALID_PARAMETER, RT_SRC_POS, N_("NAT#%d: Invalid configuration value for \"Protocol\": \"%s\""), iInstance, szProtocol);
        }
        else
            return PDMDrvHlpVMSetError(pData->pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"Protocol\" string returned %Vrc"), iInstance, rc);

        /* host port */
        int32_t iHostPort;
        rc = CFGMR3QueryS32(pNode, "HostPort", &iHostPort);
        if (VBOX_FAILURE(rc))
            return PDMDrvHlpVMSetError(pData->pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"HostPort\" integer returned %Vrc"), iInstance, rc);

        /* guest port */
        int32_t iGuestPort;
        rc = CFGMR3QueryS32(pNode, "GuestPort", &iGuestPort);
        if (VBOX_FAILURE(rc))
            return PDMDrvHlpVMSetError(pData->pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"GuestPort\" integer returned %Vrc"), iInstance, rc);

        /* guest address */
        char    szGuestIP[32];
        rc = CFGMR3QueryString(pNode, "GuestIP", &szGuestIP[0], sizeof(szGuestIP));
        if (rc == VERR_CFGM_VALUE_NOT_FOUND)
            strcpy(szGuestIP, "10.0.2.15");
        else if (VBOX_FAILURE(rc))
            return PDMDrvHlpVMSetError(pData->pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"GuestIP\" string returned %Vrc"), iInstance, rc);
        struct in_addr GuestIP;
        if (!inet_aton(szGuestIP, &GuestIP))
            return PDMDrvHlpVMSetError(pData->pDrvIns, VERR_NAT_REDIR_GUEST_IP, RT_SRC_POS, N_("NAT#%d: configuration error: invalid \"GuestIP\"=\"%s\", inet_aton failed"), iInstance, szGuestIP);

        /*
         * Call slirp about it.
         */
        Log(("drvNATConstruct: Redir %d -> %s:%d\n", iHostPort, szGuestIP, iGuestPort));
        if (slirp_redir(pData->pNATState, fUDP, iHostPort, GuestIP, iGuestPort) < 0)
            return PDMDrvHlpVMSetError(pData->pDrvIns, VERR_NAT_REDIR_SETUP, RT_SRC_POS, N_("NAT#%d: configuration error: failed to set up redirection of %d to %s:%d. Probably a conflict with existing services or other rules"), iInstance, iHostPort, szGuestIP, iGuestPort);
    } /* for each redir rule */

    return VINF_SUCCESS;
}


/**
 * Construct a NAT network transport driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) drvNATConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVNAT pData = PDMINS2DATA(pDrvIns, PDRVNAT);
    char szNetAddr[16];
    LogFlow(("drvNATConstruct:\n"));

    /*
     * Validate the config.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "PassDomain\0TFTPPrefix\0BootFile\0"))
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES, N_("Unknown NAT configuration option, only supports PassDomain, TFTPPrefix and BootFile"));

    /*
     * Init the static parts.
     */
    pData->pDrvIns                      = pDrvIns;
    pData->pNATState                    = NULL;
    pData->pszTFTPPrefix                = NULL;
    pData->pszBootFile                  = NULL;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface    = drvNATQueryInterface;
    /* INetwork */
    pData->INetworkConnector.pfnSend               = drvNATSend;
    pData->INetworkConnector.pfnSetPromiscuousMode = drvNATSetPromiscuousMode;
    pData->INetworkConnector.pfnNotifyLinkChanged  = drvNATNotifyLinkChanged;
    pData->INetworkConnector.pfnNotifyCanReceive   = drvNATNotifyCanReceive;

    /*
     * Get the configuration settings.
     */
    bool fPassDomain = true;
    int rc = CFGMR3QueryBool(pCfgHandle, "PassDomain", &fPassDomain);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fPassDomain = true;
    else if (VBOX_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"PassDomain\" boolean returned %Vrc"), pDrvIns->iInstance, rc);

    rc = CFGMR3QueryStringAlloc(pCfgHandle, "TFTPPrefix", &pData->pszTFTPPrefix);
    if (VBOX_FAILURE(rc) && rc != VERR_CFGM_VALUE_NOT_FOUND)
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"TFTPPrefix\" string returned %Vrc"), pDrvIns->iInstance, rc);
    rc = CFGMR3QueryStringAlloc(pCfgHandle, "BootFile", &pData->pszBootFile);
    if (VBOX_FAILURE(rc) && rc != VERR_CFGM_VALUE_NOT_FOUND)
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"BootFile\" string returned %Vrc"), pDrvIns->iInstance, rc);

    /*
     * Query the network port interface.
     */
    pData->pPort = (PPDMINETWORKPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_NETWORK_PORT);
    if (!pData->pPort)
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE,
                                N_("Configuration error: the above device/driver didn't export the network port interface!\n"));

    /* Generate a network address for this network card. */
    RTStrPrintf(szNetAddr, sizeof(szNetAddr), "10.0.%d.0", pDrvIns->iInstance + 2);

    /*
     * The slirp lock..
     */
    rc = RTCritSectInit(&pData->CritSect);
    if (VBOX_FAILURE(rc))
        return rc;
#if 0
    rc = RTSemEventCreate(&g_EventSem);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Start the select thread. (it'll block on the sem)
         */
        g_fThreadTerm = false;
        rc = RTThreadCreate(&g_ThreadSelect, drvNATSelectThread, 0, NULL, "NATSEL");
        if (VBOX_SUCCESS(rc))
        {
#endif
            /*
             * Initialize slirp.
             */
            rc = slirp_init(&pData->pNATState, &szNetAddr[0], fPassDomain, pData->pszTFTPPrefix, pData->pszBootFile, pData);
            if (VBOX_SUCCESS(rc))
            {
                int rc2 = drvNATConstructRedir(pDrvIns->iInstance, pData, pCfgHandle);
                if (VBOX_SUCCESS(rc2))
                {
                    pDrvIns->pDrvHlp->pfnPDMPollerRegister(pDrvIns, drvNATPoller);

                    pData->enmLinkState = PDMNETWORKLINKSTATE_UP;
#if 0
                    RTSemEventSignal(g_EventSem);
                    RTThreadSleep(0);
#endif
                    /* might return VINF_NAT_DNS */
                    return rc;
                }
                /* failure path */
                slirp_term(pData->pNATState);
                pData->pNATState = NULL;
            }
            else
            {
	      PDMDRV_SET_ERROR(pDrvIns, rc, N_("Unknown error during NAT networking setup: "));
    	      AssertMsgFailed(("Add error message for rc=%d (%Vrc)\n", rc, rc));
            }
#if 0
            g_fThreadTerm = true;
            RTSemEventSignal(g_EventSem);
            RTThreadSleep(0);
        }
        RTSemEventDestroy(g_EventSem);
        g_EventSem = NULL;
    }
#endif
    RTCritSectDelete(&pData->CritSect);
    return rc;
}



/**
 * NAT network transport driver registration record.
 */
const PDMDRVREG g_DrvNAT =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "NAT",
    /* pszDescription */
    "NAT Network Transport Driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_NETWORK,
    /* cMaxInstances */
    16,
    /* cbInstance */
    sizeof(DRVNAT),
    /* pfnConstruct */
    drvNATConstruct,
    /* pfnDestruct */
    drvNATDestruct,
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
