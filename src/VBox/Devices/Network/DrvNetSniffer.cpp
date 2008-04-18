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
    /** The port we're attached to. */
    PPDMINETWORKPORT        pPort;
    /** The connector that's attached to us. */
    PPDMINETWORKCONNECTOR   pConnector;
    /** The filename. */
    char                    szFilename[RTPATH_MAX];
    /** The filehandle. */
    RTFILE                  File;
    /** The lock serializing the file access. */
    RTCRITSECT              Lock;
    /** Pointer to the driver instance. */
    PPDMDRVINS              pDrvIns;

} DRVNETSNIFFER, *PDRVNETSNIFFER;

/** Converts a pointer to NAT::INetworkConnector to a PDRVNETSNIFFER. */
#define PDMINETWORKCONNECTOR_2_DRVNETSNIFFER(pInterface)    ( (PDRVNETSNIFFER)((uintptr_t)pInterface - RT_OFFSETOF(DRVNETSNIFFER, INetworkConnector)) )

/** Converts a pointer to NAT::INetworkPort to a PDRVNETSNIFFER. */
#define PDMINETWORKPORT_2_DRVNETSNIFFER(pInterface)         ( (PDRVNETSNIFFER)((uintptr_t)pInterface - RT_OFFSETOF(DRVNETSNIFFER, INetworkPort)) )


/* "libpcap" magic */
#define PCAP_MAGIC  0xa1b2c3d4

/* "libpcap" file header (minus magic number). */
struct pcap_hdr
{
    uint16_t    version_major;  /* major version number                         = 2 */
    uint16_t    version_minor;  /* minor version number                         = 4 */
    int32_t     thiszone;       /* GMT to local correction                      = 0 */
    uint32_t    sigfigs;        /* accuracy of timestamps                       = 0 */
    uint32_t    snaplen;        /* max length of captured packets, in octets    = 0xffff */
    uint32_t    network;        /* data link type                               = 01 */
};

/* "libpcap" record header. */
struct pcaprec_hdr
{
    uint32_t    ts_sec;         /* timestamp seconds */
    uint32_t    ts_usec;        /* timestamp microseconds */
    uint32_t    incl_len;       /* number of octets of packet saved in file */
    uint32_t    orig_len;       /* actual length of packet */
};

struct pcaprec_hdr_init
{
    uint32_t        u32Magic;
    struct pcap_hdr pcap;
#ifdef LOG_ENABLED
    pcaprec_hdr     rec;
#endif
};


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
    PDRVNETSNIFFER pData = PDMINETWORKCONNECTOR_2_DRVNETSNIFFER(pInterface);

    /* output to sniffer */
    struct pcaprec_hdr  Hdr;
    uint64_t u64TS = RTTimeProgramNanoTS();
    Hdr.ts_sec = (uint32_t)(u64TS / 1000000000);
    Hdr.ts_usec = (uint32_t)((u64TS / 1000) % 1000000);
    Hdr.incl_len = cb;
    Hdr.orig_len = cb;
    RTCritSectEnter(&pData->Lock);
    RTFileWrite(pData->File, &Hdr, sizeof(Hdr), NULL);
    RTFileWrite(pData->File, pvBuf, cb, NULL);
    RTCritSectLeave(&pData->Lock);

    /* pass down */
    if (pData->pConnector)
    {
        int rc = pData->pConnector->pfnSend(pData->pConnector, pvBuf, cb);
#if 0
        RTCritSectEnter(&pData->Lock);
        u64TS = RTTimeProgramNanoTS();
        Hdr.ts_sec = (uint32_t)(u64TS / 1000000000);
        Hdr.ts_usec = (uint32_t)((u64TS / 1000) % 1000000);
        Hdr.incl_len = 0;
        RTFileWrite(pData->File, &Hdr, sizeof(Hdr), NULL);
        RTCritSectLeave(&pData->Lock);
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
    PDRVNETSNIFFER pData = PDMINETWORKCONNECTOR_2_DRVNETSNIFFER(pInterface);
    if (pData->pConnector)
        pData->pConnector->pfnSetPromiscuousMode(pData->pConnector, fPromiscuous);
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
    PDRVNETSNIFFER pData = PDMINETWORKCONNECTOR_2_DRVNETSNIFFER(pInterface);
    if (pData->pConnector)
        pData->pConnector->pfnNotifyLinkChanged(pData->pConnector, enmLinkState);
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
    PDRVNETSNIFFER pData = PDMINETWORKPORT_2_DRVNETSNIFFER(pInterface);
    return pData->pPort->pfnWaitReceiveAvail(pData->pPort, cMillies);
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
    PDRVNETSNIFFER pData = PDMINETWORKPORT_2_DRVNETSNIFFER(pInterface);

    /* output to sniffer */
    struct pcaprec_hdr  Hdr;
    uint64_t u64TS = RTTimeProgramNanoTS();
    Hdr.ts_sec = (uint32_t)(u64TS / 1000000000);
    Hdr.ts_usec = (uint32_t)((u64TS / 1000) % 1000000);
    Hdr.incl_len = cb;
    Hdr.orig_len = cb;
    RTCritSectEnter(&pData->Lock);
    RTFileWrite(pData->File, &Hdr, sizeof(Hdr), NULL);
    RTFileWrite(pData->File, pvBuf, cb, NULL);
    RTCritSectLeave(&pData->Lock);

    /* pass up */
    int rc = pData->pPort->pfnReceive(pData->pPort, pvBuf, cb);
#if 0
    RTCritSectEnter(&pData->Lock);
    u64TS = RTTimeProgramNanoTS();
    Hdr.ts_sec = (uint32_t)(u64TS / 1000000000);
    Hdr.ts_usec = (uint32_t)((u64TS / 1000) % 1000000);
    Hdr.incl_len = 0;
    RTFileWrite(pData->File, &Hdr, sizeof(Hdr), NULL);
    RTCritSectLeave(&pData->Lock);
#endif
    return rc;
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
    PDRVNETSNIFFER pData = PDMINS2DATA(pDrvIns, PDRVNETSNIFFER);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_NETWORK_CONNECTOR:
            return &pData->INetworkConnector;
        case PDMINTERFACE_NETWORK_PORT:
            return &pData->INetworkPort;
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
    PDRVNETSNIFFER pData = PDMINS2DATA(pDrvIns, PDRVNETSNIFFER);

    if (RTCritSectIsInitialized(&pData->Lock))
        RTCritSectDelete(&pData->Lock);

    if (pData->File != NIL_RTFILE)
    {
        RTFileClose(pData->File);
        pData->File = NIL_RTFILE;
    }
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
static DECLCALLBACK(int) drvNetSnifferConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVNETSNIFFER pData = PDMINS2DATA(pDrvIns, PDRVNETSNIFFER);
    LogFlow(("drvNetSnifferConstruct:\n"));

    /*
     * Validate the config.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "File\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

    /*
     * Init the static parts.
     */
    pData->pDrvIns                      = pDrvIns;
    pData->File                         = NIL_RTFILE;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface    = drvNetSnifferQueryInterface;
    /* INetworkConnector */
    pData->INetworkConnector.pfnSend                = drvNetSnifferSend;
    pData->INetworkConnector.pfnSetPromiscuousMode  = drvNetSnifferSetPromiscuousMode;
    pData->INetworkConnector.pfnNotifyLinkChanged   = drvNetSnifferNotifyLinkChanged;
    /* INetworkPort */
    pData->INetworkPort.pfnWaitReceiveAvail         = drvNetSnifferWaitReceiveAvail;
    pData->INetworkPort.pfnReceive                  = drvNetSnifferReceive;

    /*
     * Get the filename.
     */
    int rc = CFGMR3QueryString(pCfgHandle, "File", pData->szFilename, sizeof(pData->szFilename));
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        RTStrPrintf(pData->szFilename, sizeof(pData->szFilename), "./VBox-%x.pcap", RTProcSelf());
    else if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to query \"File\", rc=%Vrc.\n", rc));
        return rc;
    }

    /*
     * Query the network port interface.
     */
    pData->pPort = (PPDMINETWORKPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_NETWORK_PORT);
    if (!pData->pPort)
    {
        AssertMsgFailed(("Configuration error: the above device/driver didn't export the network port interface!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }

    /*
     * Query the network connector interface.
     */
    PPDMIBASE   pBaseDown;
    rc = pDrvIns->pDrvHlp->pfnAttach(pDrvIns, &pBaseDown);
    if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        pData->pConnector = NULL;
    else if (VBOX_SUCCESS(rc))
    {
        pData->pConnector = (PPDMINETWORKCONNECTOR)pBaseDown->pfnQueryInterface(pBaseDown, PDMINTERFACE_NETWORK_CONNECTOR);
        if (!pData->pConnector)
        {
            AssertMsgFailed(("Configuration error: the driver below didn't export the network connector interface!\n"));
            return VERR_PDM_MISSING_INTERFACE_BELOW;
        }
    }
    else
    {
        AssertMsgFailed(("Failed to attach to driver below! rc=%Vrc\n", rc));
        return rc;
    }

    /*
     * Create the lock.
     */
    rc = RTCritSectInit(&pData->Lock);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Open output file / pipe.
     */
    rc = RTFileOpen(&pData->File, pData->szFilename,
                    RTFILE_O_WRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_WRITE);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to create file '%s' for writing. rc=%Vrc\n", pData->szFilename, rc));
        return rc;
    }

    /*
     * Write pcap header.
     */
#ifdef LOG_ENABLED
    pcaprec_hdr_init Hdr = 
    { 
        PCAP_MAGIC, 
        { 2, 4, 0, 0, 0xffff, 1 }, 
        { 0, 1, 0, 60} 
    }; /* force ethereal to start at 0.000000. */
#else
    pcaprec_hdr_init Hdr =
    {
        PCAP_MAGIC,
        { 2, 4, 0, 0, 0xffff, 1 } 
    }; /* this is just to make it happy, not to be correct. */
#endif

    RTFileWrite(pData->File, &Hdr, sizeof(Hdr), NULL);

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
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL
};

