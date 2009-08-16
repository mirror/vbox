/** @file
 *
 * VBox network devices:
 * Network sniffer filter driver
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
#define LOG_GROUP LOG_GROUP_DRV_NAT
#include <VBox/pdmdrv.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/critsect.h>
#include <VBox/param.h>

#include "Pcap.h"
#include "Builtins.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Block driver instance data.
 */
typedef struct DRVNETSNIFFER
{
    /** The network interface. */
    PDMINETWORKCONNECTOR    INetworkConnector;
    /** The network interface. */
    PDMINETWORKPORT         INetworkPort;
    /** The network config interface. */
    PDMINETWORKCONFIG       INetworkConfig;
    /** The port we're attached to. */
    PPDMINETWORKPORT        pPort;
    /** The config port interface we're attached to. */
    PPDMINETWORKCONFIG      pConfig;
    /** The connector that's attached to us. */
    PPDMINETWORKCONNECTOR   pConnector;
    /** The filename. */
    char                    szFilename[RTPATH_MAX];
    /** The filehandle. */
    RTFILE                  File;
    /** The lock serializing the file access. */
    RTCRITSECT              Lock;
    /** The NanoTS delta we pass to the pcap writers. */
    uint64_t                StartNanoTS;
    /** Pointer to the driver instance. */
    PPDMDRVINS              pDrvIns;

} DRVNETSNIFFER, *PDRVNETSNIFFER;

/** Converts a pointer to NAT::INetworkConnector to a PDRVNETSNIFFER. */
#define PDMINETWORKCONNECTOR_2_DRVNETSNIFFER(pInterface)    ( (PDRVNETSNIFFER)((uintptr_t)pInterface - RT_OFFSETOF(DRVNETSNIFFER, INetworkConnector)) )

/** Converts a pointer to NAT::INetworkPort to a PDRVNETSNIFFER. */
#define PDMINETWORKPORT_2_DRVNETSNIFFER(pInterface)         ( (PDRVNETSNIFFER)((uintptr_t)pInterface - RT_OFFSETOF(DRVNETSNIFFER, INetworkPort)) )

/** Converts a pointer to NAT::INetworkConfig to a PDRVNETSNIFFER. */
#define PDMINETWORKCONFIG_2_DRVNETSNIFFER(pInterface)       ( (PDRVNETSNIFFER)((uintptr_t)pInterface - RT_OFFSETOF(DRVNETSNIFFER, INetworkConfig)) )



/**
 * Send data to the network.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   pvBuf           Data to send.
 * @param   cb              Number of bytes to send.
 * @thread  EMT
 */
static DECLCALLBACK(int) drvNetSnifferSend(PPDMINETWORKCONNECTOR pInterface, const void *pvBuf, size_t cb)
{
    PDRVNETSNIFFER pThis = PDMINETWORKCONNECTOR_2_DRVNETSNIFFER(pInterface);

    /* output to sniffer */
    RTCritSectEnter(&pThis->Lock);
    PcapFileFrame(pThis->File, pThis->StartNanoTS, pvBuf, cb, cb);
    RTCritSectLeave(&pThis->Lock);

    /* pass down */
    if (pThis->pConnector)
    {
        int rc = pThis->pConnector->pfnSend(pThis->pConnector, pvBuf, cb);
#if 0
        RTCritSectEnter(&pThis->Lock);
        u64TS = RTTimeProgramNanoTS();
        Hdr.ts_sec = (uint32_t)(u64TS / 1000000000);
        Hdr.ts_usec = (uint32_t)((u64TS / 1000) % 1000000);
        Hdr.incl_len = 0;
        RTFileWrite(pThis->File, &Hdr, sizeof(Hdr), NULL);
        RTCritSectLeave(&pThis->Lock);
#endif
        return rc;
    }
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
static DECLCALLBACK(void) drvNetSnifferSetPromiscuousMode(PPDMINETWORKCONNECTOR pInterface, bool fPromiscuous)
{
    LogFlow(("drvNetSnifferSetPromiscuousMode: fPromiscuous=%d\n", fPromiscuous));
    PDRVNETSNIFFER pThis = PDMINETWORKCONNECTOR_2_DRVNETSNIFFER(pInterface);
    if (pThis->pConnector)
        pThis->pConnector->pfnSetPromiscuousMode(pThis->pConnector, fPromiscuous);
}


/**
 * Notification on link status changes.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   enmLinkState    The new link state.
 * @thread  EMT
 */
static DECLCALLBACK(void) drvNetSnifferNotifyLinkChanged(PPDMINETWORKCONNECTOR pInterface, PDMNETWORKLINKSTATE enmLinkState)
{
    LogFlow(("drvNetSnifferNotifyLinkChanged: enmLinkState=%d\n", enmLinkState));
    PDRVNETSNIFFER pThis = PDMINETWORKCONNECTOR_2_DRVNETSNIFFER(pInterface);
    if (pThis->pConnector)
        pThis->pConnector->pfnNotifyLinkChanged(pThis->pConnector, enmLinkState);
}


/**
 * Check how much data the device/driver can receive data now.
 * This must be called before the pfnRecieve() method is called.
 *
 * @returns Number of bytes the device can receive now.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @thread  EMT
 */
static DECLCALLBACK(int) drvNetSnifferWaitReceiveAvail(PPDMINETWORKPORT pInterface, unsigned cMillies)
{
    PDRVNETSNIFFER pThis = PDMINETWORKPORT_2_DRVNETSNIFFER(pInterface);
    return pThis->pPort->pfnWaitReceiveAvail(pThis->pPort, cMillies);
}


/**
 * Receive data from the network.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   pvBuf           The available data.
 * @param   cb              Number of bytes available in the buffer.
 * @thread  EMT
 */
static DECLCALLBACK(int) drvNetSnifferReceive(PPDMINETWORKPORT pInterface, const void *pvBuf, size_t cb)
{
    PDRVNETSNIFFER pThis = PDMINETWORKPORT_2_DRVNETSNIFFER(pInterface);

    /* output to sniffer */
    RTCritSectEnter(&pThis->Lock);
    PcapFileFrame(pThis->File, pThis->StartNanoTS, pvBuf, cb, cb);
    RTCritSectLeave(&pThis->Lock);

    /* pass up */
    int rc = pThis->pPort->pfnReceive(pThis->pPort, pvBuf, cb);
#if 0
    RTCritSectEnter(&pThis->Lock);
    u64TS = RTTimeProgramNanoTS();
    Hdr.ts_sec = (uint32_t)(u64TS / 1000000000);
    Hdr.ts_usec = (uint32_t)((u64TS / 1000) % 1000000);
    Hdr.incl_len = 0;
    RTFileWrite(pThis->File, &Hdr, sizeof(Hdr), NULL);
    RTCritSectLeave(&pThis->Lock);
#endif
    return rc;
}


/**
 * Gets the current Media Access Control (MAC) address.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   pMac            Where to store the MAC address.
 * @thread  EMT
 */
static DECLCALLBACK(int) drvNetSnifferGetMac(PPDMINETWORKCONFIG pInterface, PRTMAC pMac)
{
    PDRVNETSNIFFER pThis = PDMINETWORKCONFIG_2_DRVNETSNIFFER(pInterface);
    return pThis->pConfig->pfnGetMac(pThis->pConfig, pMac);
}

/**
 * Gets the new link state.
 *
 * @returns The current link state.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @thread  EMT
 */
static DECLCALLBACK(PDMNETWORKLINKSTATE) drvNetSnifferGetLinkState(PPDMINETWORKCONFIG pInterface)
{
    PDRVNETSNIFFER pThis = PDMINETWORKCONFIG_2_DRVNETSNIFFER(pInterface);
    return pThis->pConfig->pfnGetLinkState(pThis->pConfig);
}

/**
 * Sets the new link state.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   enmState        The new link state
 * @thread  EMT
 */
static DECLCALLBACK(int) drvNetSnifferSetLinkState(PPDMINETWORKCONFIG pInterface, PDMNETWORKLINKSTATE enmState)
{
    PDRVNETSNIFFER pThis = PDMINETWORKCONFIG_2_DRVNETSNIFFER(pInterface);
    return pThis->pConfig->pfnSetLinkState(pThis->pConfig, enmState);
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
static DECLCALLBACK(void *) drvNetSnifferQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVNETSNIFFER pThis = PDMINS_2_DATA(pDrvIns, PDRVNETSNIFFER);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_NETWORK_CONNECTOR:
            return &pThis->INetworkConnector;
        case PDMINTERFACE_NETWORK_PORT:
            return &pThis->INetworkPort;
        case PDMINTERFACE_NETWORK_CONFIG:
            return &pThis->INetworkConfig;
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
static DECLCALLBACK(void) drvNetSnifferDestruct(PPDMDRVINS pDrvIns)
{
    PDRVNETSNIFFER pThis = PDMINS_2_DATA(pDrvIns, PDRVNETSNIFFER);

    if (RTCritSectIsInitialized(&pThis->Lock))
        RTCritSectDelete(&pThis->Lock);

    if (pThis->File != NIL_RTFILE)
    {
        RTFileClose(pThis->File);
        pThis->File = NIL_RTFILE;
    }
}


/**
 * Construct a NAT network transport driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvNetSnifferConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle, uint32_t fFlags)
{
    PDRVNETSNIFFER pThis = PDMINS_2_DATA(pDrvIns, PDRVNETSNIFFER);
    LogFlow(("drvNetSnifferConstruct:\n"));

    /*
     * Validate the config.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "File\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

    if (CFGMR3GetFirstChild(pCfgHandle))
        LogRel(("NetSniffer: Found child config entries -- are you trying to redirect ports?\n"));

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                      = pDrvIns;
    pThis->File                         = NIL_RTFILE;
    /* The pcap file *must* start at time offset 0,0. */
    pThis->StartNanoTS                  = RTTimeNanoTS() - RTTimeProgramNanoTS();
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface    = drvNetSnifferQueryInterface;
    /* INetworkConnector */
    pThis->INetworkConnector.pfnSend                = drvNetSnifferSend;
    pThis->INetworkConnector.pfnSetPromiscuousMode  = drvNetSnifferSetPromiscuousMode;
    pThis->INetworkConnector.pfnNotifyLinkChanged   = drvNetSnifferNotifyLinkChanged;
    /* INetworkPort */
    pThis->INetworkPort.pfnWaitReceiveAvail         = drvNetSnifferWaitReceiveAvail;
    pThis->INetworkPort.pfnReceive                  = drvNetSnifferReceive;
    /* INetworkConfig */
    pThis->INetworkConfig.pfnGetMac                 = drvNetSnifferGetMac;
    pThis->INetworkConfig.pfnGetLinkState           = drvNetSnifferGetLinkState;
    pThis->INetworkConfig.pfnSetLinkState           = drvNetSnifferSetLinkState;

    /*
     * Get the filename.
     */
    int rc = CFGMR3QueryString(pCfgHandle, "File", pThis->szFilename, sizeof(pThis->szFilename));
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        RTStrPrintf(pThis->szFilename, sizeof(pThis->szFilename), "./VBox-%x.pcap", RTProcSelf());
    else if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to query \"File\", rc=%Rrc.\n", rc));
        return rc;
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
     * Query the network config interface.
     */
    pThis->pConfig = (PPDMINETWORKCONFIG)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_NETWORK_CONFIG);
    if (!pThis->pConfig)
    {
        AssertMsgFailed(("Configuration error: the above device/driver didn't export the network config interface!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }

    /*
     * Query the network connector interface.
     */
    PPDMIBASE   pBaseDown;
    rc = PDMDrvHlpAttach(pDrvIns, fFlags, &pBaseDown);
    if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        pThis->pConnector = NULL;
    else if (RT_SUCCESS(rc))
    {
        pThis->pConnector = (PPDMINETWORKCONNECTOR)pBaseDown->pfnQueryInterface(pBaseDown, PDMINTERFACE_NETWORK_CONNECTOR);
        if (!pThis->pConnector)
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
     * Create the lock.
     */
    rc = RTCritSectInit(&pThis->Lock);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Open output file / pipe.
     */
    rc = RTFileOpen(&pThis->File, pThis->szFilename,
                    RTFILE_O_WRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_WRITE);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("Netsniffer cannot open '%s' for writing. The directory must exist and it must be writable for the current user"), pThis->szFilename);

    /*
     * Write pcap header.
     * Some time is done since capturing pThis->StartNanoTS so capture the current time again.
     */
    PcapFileHdr(pThis->File, RTTimeNanoTS());

    return VINF_SUCCESS;
}



/**
 * Network sniffer filter driver registration record.
 */
const PDMDRVREG g_DrvNetSniffer =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "NetSniffer",
    /* pszDescription */
    "Network Sniffer Filter Driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_NETWORK,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(DRVNETSNIFFER),
    /* pfnConstruct */
    drvNetSnifferConstruct,
    /* pfnDestruct */
    drvNetSnifferDestruct,
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

