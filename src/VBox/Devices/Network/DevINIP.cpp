/* $Id$ */
/** @file
 * DevINIP - Internal Network IP stack device/service.
 */

/*
 * Copyright (C) 2007-2009 Sun Microsystems, Inc.
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
#define LOG_GROUP LOG_GROUP_DEV_INIP
#include <iprt/cdefs.h>     /* include early to allow __BEGIN_DECLS hack */
#include <iprt/mem.h>       /* include anything of ours that the lwip headers use. */
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/alloca.h>
/* All lwip header files are not C++ safe. So hack around this. */
__BEGIN_DECLS
#include "lwip/sys.h"
#include "lwip/stats.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "ipv4/lwip/ip.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"
#include "lwip/tcpip.h"
#include "lwip/sockets.h"
#include "netif/etharp.h"
__END_DECLS
#include <VBox/pdmdev.h>
#include <VBox/tm.h>
#include <iprt/string.h>
#include <iprt/assert.h>

#include "../Builtins.h"


/*******************************************************************************
*   Macros and Defines                                                         *
*******************************************************************************/

/** Maximum frame size this device can handle. */
#define DEVINIP_MAX_FRAME 1514


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * Internal Network IP stack device instance data.
 */
typedef struct DEVINTNETIP
{
    /** The base interface. */
    PDMIBASE                IBase;
    /** The network port this device provides. */
    PDMINETWORKPORT         INetworkPort;
    /** The base interface of the network driver below us. */
    PPDMIBASE               pDrvBase;
    /** The connector of the network driver below us. */
    PPDMINETWORKCONNECTOR   pDrv;
    /** Pointer to the device instance. */
    PPDMDEVINSR3            pDevIns;
    /** MAC adress. */
    RTMAC                   MAC;
    /** Static IP address of the interface. */
    char                   *pszIP;
    /** Netmask of the interface. */
    char                   *pszNetmask;
    /** Gateway for the interface. */
    char                   *pszGateway;
    /** lwIP network interface description. */
    struct netif            IntNetIF;
    /** lwIP ARP timer. */
    PTMTIMERR3              ARPTimer;
    /** lwIP TCP fast timer. */
    PTMTIMERR3              TCPFastTimer;
    /** lwIP TCP slow timer. */
    PTMTIMERR3              TCPSlowTimer;
    /** lwIP semaphore to coordinate TCPIP init/terminate. */
    sys_sem_t               LWIPTcpInitSem;
    /** hack: get linking right. remove this eventually, once the device
     * provides a proper interface to all IP stack functions. */
    const void             *pLinkHack;
} DEVINTNETIP, *PDEVINTNETIP;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/

/**
 * Pointer to the (only) instance data in this device.
 */
static PDEVINTNETIP g_pDevINIPData = NULL;

/*
 * really ugly hack to avoid linking problems on unix style platforms
 * using .a libraries for now.
 */
static const PFNRT g_pDevINILinkHack[] =
{
    (PFNRT)lwip_socket,
    (PFNRT)lwip_close,
    (PFNRT)lwip_setsockopt,
    (PFNRT)lwip_recv,
    (PFNRT)lwip_send,
    (PFNRT)lwip_select
};


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(void) devINIPARPTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer);
static DECLCALLBACK(void) devINIPTCPFastTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer);
static DECLCALLBACK(void) devINIPTCPSlowTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer);
static DECLCALLBACK(err_t) devINIPOutput(struct netif *netif, struct pbuf *p,
                                         struct ip_addr *ipaddr);
static DECLCALLBACK(err_t) devINIPOutputRaw(struct netif *netif,
                                            struct pbuf *p);
static DECLCALLBACK(err_t) devINIPInterface(struct netif *netif);

/**
 * ARP cache timeout handling for lwIP.
 *
 * @param   pDevIns     Device instance.
 * @param   pTimer      Pointer to timer.
 */
static DECLCALLBACK(void) devINIPARPTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer)
{
    PDEVINTNETIP pThis = PDMINS_2_DATA(pDevIns, PDEVINTNETIP);
    LogFlow(("%s: pDevIns=%p pTimer=%p\n", __FUNCTION__, pDevIns, pTimer));
    lwip_etharp_tmr();
    TMTimerSetMillies(pThis->ARPTimer, ARP_TMR_INTERVAL);
    LogFlow(("%s: return\n", __FUNCTION__));
}

/**
 * TCP fast timer handling for lwIP.
 *
 * @param   pDevIns     Device instance.
 * @param   pTimer      Pointer to timer.
 */
static DECLCALLBACK(void) devINIPTCPFastTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer)
{
    PDEVINTNETIP pThis = PDMINS_2_DATA(pDevIns, PDEVINTNETIP);
    LogFlow(("%s: pDevIns=%p pTimer=%p\n", __FUNCTION__, pDevIns, pTimer));
    lwip_tcp_fasttmr();
    TMTimerSetMillies(pThis->TCPFastTimer, TCP_FAST_INTERVAL);
    LogFlow(("%s: return\n", __FUNCTION__));
}

/**
 * TCP slow timer handling for lwIP.
 *
 * @param   pDevIns     Device instance.
 * @param   pTimer      Pointer to timer.
 */
static DECLCALLBACK(void) devINIPTCPSlowTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer)
{
    PDEVINTNETIP pThis = PDMINS_2_DATA(pDevIns, PDEVINTNETIP);
    LogFlow(("%s: pDevIns=%p pTimer=%p\n", __FUNCTION__, pDevIns, pTimer));
    lwip_tcp_slowtmr();
    TMTimerSetMillies(pThis->TCPSlowTimer, TCP_SLOW_INTERVAL);
    LogFlow(("%s: return\n", __FUNCTION__));
}

/**
 * Output a TCP/IP packet on the interface. Uses the generic lwIP ARP
 * code to resolve the address and call the link-level packet function.
 *
 * @returns lwIP error code
 * @param   netif   Interface on which to send IP packet.
 * @param   p       Packet data.
 * @param   ipaddr  Destination IP address.
 */
static DECLCALLBACK(err_t) devINIPOutput(struct netif *netif, struct pbuf *p,
                                         struct ip_addr *ipaddr)
{
    err_t lrc;
    LogFlow(("%s: netif=%p p=%p ipaddr=%#04x\n", __FUNCTION__, netif, p,
             ipaddr->addr));
    lrc = lwip_etharp_output(netif, ipaddr, p);
    LogFlow(("%s: return %d\n", __FUNCTION__, lrc));
    return lrc;
}

/**
 * Output a raw packet on the interface.
 *
 * @returns lwIP error code
 * @param   netif   Interface on which to send frame.
 * @param   p       Frame data.
 */
static DECLCALLBACK(err_t) devINIPOutputRaw(struct netif *netif,
                                            struct pbuf *p)
{
    uint8_t aFrame[DEVINIP_MAX_FRAME], *pbBuf;
    size_t cbBuf;
    struct pbuf *q;
    int rc = VINF_SUCCESS;

    LogFlow(("%s: netif=%p p=%p\n", __FUNCTION__, netif, p));
    Assert(g_pDevINIPData);
    Assert(g_pDevINIPData->pDrv);

    /* Silently ignore packets being sent while lwIP isn't set up. */
    if (!g_pDevINIPData)
        goto out;

#if ETH_PAD_SIZE
    lwip_pbuf_header(p, -ETH_PAD_SIZE);      /* drop the padding word */
#endif

    pbBuf = &aFrame[0];
    cbBuf = 0;
    for (q = p; q != NULL; q = q->next)
    {
        if (cbBuf + q->len <= DEVINIP_MAX_FRAME)
        {
            memcpy(pbBuf, q->payload, q->len);
            pbBuf += q->len;
            cbBuf += q->len;
        }
        else
        {
            LogRel(("INIP: exceeded frame size\n"));
            break;
        }
    }
    if (cbBuf)
        rc = g_pDevINIPData->pDrv->pfnSend(g_pDevINIPData->pDrv,
                                                 &aFrame[0], cbBuf);

#if ETH_PAD_SIZE
    lwip_pbuf_header(p, ETH_PAD_SIZE);       /* reclaim the padding word */
#endif

out:
    err_t lrc = ERR_OK;
    if (RT_FAILURE(rc))
        lrc = ERR_IF;
    LogFlow(("%s: return %d (vbox: %Rrc)\n", __FUNCTION__, rc, lrc));
    return lrc;
}

/**
 * Implements the ethernet interface backend initialization for lwIP.
 *
 * @returns lwIP error code
 * @param   netif   Interface to configure.
 */
static DECLCALLBACK(err_t) devINIPInterface(struct netif *netif)
{
    LogFlow(("%s: netif=%p\n", __FUNCTION__, netif));
    Assert(g_pDevINIPData != NULL);
    netif->state = g_pDevINIPData;
    netif->hwaddr_len = sizeof(g_pDevINIPData->MAC);
    memcpy(netif->hwaddr, &g_pDevINIPData->MAC, sizeof(g_pDevINIPData->MAC));
    netif->mtu = DEVINIP_MAX_FRAME;
    netif->flags = NETIF_FLAG_BROADCAST;
    netif->output = devINIPOutput;
    netif->linkoutput = devINIPOutputRaw;

    lwip_etharp_init();
    TMTimerSetMillies(g_pDevINIPData->ARPTimer, ARP_TMR_INTERVAL);
    LogFlow(("%s: success\n", __FUNCTION__));
    return ERR_OK;
}

/**
 * Wait until data can be received.
 *
 * @returns VBox status code. VINF_SUCCESS means there is at least one receive descriptor available.
 * @param   pInterface  PDM network port interface pointer.
 * @param   cMillies    Number of milliseconds to wait. 0 means return immediately.
 */
static DECLCALLBACK(int) devINIPWaitInputAvail(PPDMINETWORKPORT pInterface, unsigned cMillies)
{
    LogFlow(("%s: pInterface=%p\n", __FUNCTION__, pInterface));
    LogFlow(("%s: return VINF_SUCCESS\n", __FUNCTION__));
    return VINF_SUCCESS;
}

/**
 * Receive data and pass it to lwIP for processing.
 *
 * @returns VBox status code
 * @param   pInterface  PDM network port interface pointer.
 * @param   pvBuf       Pointer to frame data.
 * @param   cb          Frame size.
 */
static DECLCALLBACK(int) devINIPInput(PPDMINETWORKPORT pInterface,
                                      const void *pvBuf, size_t cb)
{
    const uint8_t *pbBuf = (const uint8_t *)pvBuf;
    size_t len = cb;
    const struct eth_hdr *ethhdr;
    struct pbuf *p, *q;
    int rc = VINF_SUCCESS;

    LogFlow(("%s: pInterface=%p pvBuf=%p cb=%lu\n", __FUNCTION__, pInterface,
             pvBuf, cb));
    Assert(g_pDevINIPData);
    Assert(g_pDevINIPData->pDrv);

    /* Silently ignore packets being received while lwIP isn't set up. */
    if (!g_pDevINIPData)
        goto out;

#if ETH_PAD_SIZE
    len += ETH_PAD_SIZE;        /* allow room for Ethernet padding */
#endif

    /* We allocate a pbuf chain of pbufs from the pool. */
    p = lwip_pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (p != NULL)
    {
#if ETH_PAD_SIZE
        lwip_pbuf_header(p, -ETH_PAD_SIZE);      /* drop the padding word */
#endif

        for (q = p; q != NULL; q = q->next)
        {
            /* Fill the buffers, and clean out unused buffer space. */
            memcpy(q->payload, pbBuf, RT_MIN(cb, q->len));
            pbBuf += RT_MIN(cb, q->len);
            if (q->len > cb)
                memset(((uint8_t *)q->payload) + cb, '\0', q->len - cb);
            cb -= RT_MIN(cb, q->len);
        }

        ethhdr = (const struct eth_hdr *)p->payload;
        struct netif *iface = &g_pDevINIPData->IntNetIF;
        err_t lrc;
        switch (htons(ethhdr->type))
        {
            case ETHTYPE_IP:    /* IP packet */
                lwip_pbuf_header(p, -(ssize_t)sizeof(struct eth_hdr));
                lrc = iface->input(p, iface);
                if (lrc)
                    rc = VERR_NET_IO_ERROR;
                break;
            case ETHTYPE_ARP:   /* ARP packet */
                lwip_etharp_arp_input(iface, (struct eth_addr *)iface->hwaddr, p);
                break;
            default:
                lwip_pbuf_free(p);
        }
    }

out:
    LogFlow(("%s: return %Rrc\n", __FUNCTION__, rc));
    return rc;
}

/**
 * Signals the end of lwIP TCPIP initialization.
 *
 * @param   arg     opaque argument, here the pointer to the semaphore.
 */
static DECLCALLBACK(void) devINIPTcpipInitDone(void *arg)
{
    sys_sem_t *sem = (sys_sem_t *)arg;
    lwip_sys_sem_signal(*sem);
}


/**
 * Queries an interface to the device.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the device.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 * @thread  Any thread.
 */
static DECLCALLBACK(void *) devINIPQueryInterface(PPDMIBASE pInterface,
                                                  PDMINTERFACE enmInterface)
{
    PDEVINTNETIP pThis = (PDEVINTNETIP)((uintptr_t)pInterface - RT_OFFSETOF(DEVINTNETIP, IBase));
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pThis->IBase;
        case PDMINTERFACE_NETWORK_PORT:
            return &pThis->INetworkPort;
        default:
            return NULL;
    }
}


/**
 * Construct an internal networking IP stack device instance.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 *                      If the registration structure is needed, pDevIns->pDevReg points to it.
 * @param   iInstance   Instance number. Use this to figure out which registers
 * and such to use.
 *                      he device number is also found in pDevIns->iInstance, but since it's
 *                      likely to be freqently used PDM passes it as parameter.
 * @param   pCfgHandle  Configuration node handle for the device. Use this to obtain the configuration
 *                      of the device instance. It's also found in pDevIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) devINIPConstruct(PPDMDEVINS pDevIns, int iInstance,
                                          PCFGMNODE pCfgHandle)
{
    PDEVINTNETIP pThis = PDMINS_2_DATA(pDevIns, PDEVINTNETIP);
    int rc = VINF_SUCCESS;
    LogFlow(("%s: pDevIns=%p iInstance=%d pCfgHandle=%p\n", __FUNCTION__,
             pDevIns, iInstance, pCfgHandle));

    Assert(iInstance == 0);

    /*
     * Validate the config.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "MAC\0IP\0Netmask\0Gateway\0"))
    {
        rc = PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                              N_("Unknown Internal Networking IP configuration option"));
        goto out;
    }

    /*
     * Init the static parts.
     */
    pThis->pszIP                            = NULL;
    pThis->pszNetmask                       = NULL;
    pThis->pszGateway                       = NULL;
    /* Pointer to device instance */
    pThis->pDevIns                          = pDevIns;
    /* IBase */
    pThis->IBase.pfnQueryInterface          = devINIPQueryInterface;
    /* INetworkPort */
    pThis->INetworkPort.pfnWaitReceiveAvail = devINIPWaitInputAvail;
    pThis->INetworkPort.pfnReceive          = devINIPInput;

    /*
     * Get the configuration settings.
     */
    rc = CFGMR3QueryBytes(pCfgHandle, "MAC", &pThis->MAC, sizeof(pThis->MAC));
    if (rc == VERR_CFGM_NOT_BYTES)
    {
        char szMAC[64];
        rc = CFGMR3QueryString(pCfgHandle, "MAC", &szMAC[0], sizeof(szMAC));
        if (RT_SUCCESS(rc))
        {
            char *macStr = &szMAC[0];
            char *pMac = (char *)&pThis->MAC;
            for (uint32_t i = 0; i < 6; i++)
            {
                if (   !*macStr || !*(macStr + 1)
                    || *macStr == ':' || *(macStr + 1) == ':')
                {
                    rc = PDMDEV_SET_ERROR(pDevIns,
                                          VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                          N_("Configuration error: Invalid \"MAC\" value"));
                    goto out;
                }
                char c1 = *macStr++ - '0';
                if (c1 > 9)
                    c1 -= 7;
                char c2 = *macStr++ - '0';
                if (c2 > 9)
                    c2 -= 7;
                *pMac++ = ((c1 & 0x0f) << 4) | (c2 & 0x0f);
                if (i != 5 && *macStr == ':')
                    macStr++;
            }
        }
    }
    if (RT_FAILURE(rc))
    {
        PDMDEV_SET_ERROR(pDevIns, rc,
                         N_("Configuration error: Failed to get the \"MAC\" value"));
        goto out;
    }
    rc = CFGMR3QueryStringAlloc(pCfgHandle, "IP", &pThis->pszIP);
    if (RT_FAILURE(rc))
    {
        PDMDEV_SET_ERROR(pDevIns, rc,
                         N_("Configuration error: Failed to get the \"IP\" value"));
        goto out;
    }
    rc = CFGMR3QueryStringAlloc(pCfgHandle, "Netmask", &pThis->pszNetmask);
    if (RT_FAILURE(rc))
    {
        PDMDEV_SET_ERROR(pDevIns, rc,
                         N_("Configuration error: Failed to get the \"Netmask\" value"));
        goto out;
    }
    rc = CFGMR3QueryStringAlloc(pCfgHandle, "Gateway", &pThis->pszGateway);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        rc = VINF_SUCCESS;
    if (RT_FAILURE(rc))
    {
        PDMDEV_SET_ERROR(pDevIns, rc,
                         N_("Configuration error: Failed to get the \"Gateway\" value"));
        goto out;
    }

    /*
     * Attach driver and query the network connector interface.
     */
    rc = PDMDevHlpDriverAttach(pDevIns, 0, &pThis->IBase, &pThis->pDrvBase,
                               "Network Port");
    if (RT_FAILURE(rc))
    {
        pThis->pDrvBase = NULL;
        pThis->pDrv = NULL;
        goto out;
    }
    else
    {
        pThis->pDrv = (PPDMINETWORKCONNECTOR)pThis->pDrvBase->pfnQueryInterface(pThis->pDrvBase, PDMINTERFACE_NETWORK_CONNECTOR);
        if (!pThis->pDrv)
        {
            AssertMsgFailed(("Failed to obtain the PDMINTERFACE_NETWORK_CONNECTOR interface!\n"));
            rc = VERR_PDM_MISSING_INTERFACE_BELOW;
            goto out;
        }
    }

    struct ip_addr ipaddr, netmask, gw;
    struct in_addr ip;

    if (!inet_aton(pThis->pszIP, &ip))
    {
        rc = PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                              N_("Configuration error: Invalid \"IP\" value"));
        goto out;
    }
    memcpy(&ipaddr, &ip, sizeof(ipaddr));
    if (!inet_aton(pThis->pszNetmask, &ip))
    {
        rc = PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                              N_("Configuration error: Invalid \"Netmask\" value"));
        goto out;
    }
    memcpy(&netmask, &ip, sizeof(netmask));
    if (pThis->pszGateway)
    {
        if (!inet_aton(pThis->pszGateway, &ip))
        {
            rc = PDMDEV_SET_ERROR(pDevIns,
                                  VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                  N_("Configuration error: Invalid \"Gateway\" value"));
            goto out;
        }
        memcpy(&gw, &ip, sizeof(gw));
    }
    else
    {
        inet_aton(pThis->pszIP, &ip);
        memcpy(&gw, &ip, sizeof(gw));
    }

    /*
     * Initialize lwIP.
     */
    lwip_stats_init();
    lwip_sys_init();
#if MEM_LIBC_MALLOC == 0
    lwip_mem_init();
#endif
    lwip_memp_init();
    lwip_pbuf_init();
    lwip_netif_init();
    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL, devINIPARPTimer, "lwIP ARP", &pThis->ARPTimer);
    if (RT_FAILURE(rc))
        goto out;
    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL, devINIPTCPFastTimer, "lwIP fast TCP", &pThis->TCPFastTimer);
    if (RT_FAILURE(rc))
        goto out;
    TMTimerSetMillies(pThis->TCPFastTimer, TCP_FAST_INTERVAL);
    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL, devINIPTCPSlowTimer, "lwIP slow TCP", &pThis->TCPSlowTimer);
    if (RT_FAILURE(rc))
        goto out;
    TMTimerSetMillies(pThis->TCPFastTimer, TCP_SLOW_INTERVAL);
    pThis->LWIPTcpInitSem = lwip_sys_sem_new(0);
    {
        lwip_tcpip_init(devINIPTcpipInitDone, &pThis->LWIPTcpInitSem);
        lwip_sys_sem_wait(pThis->LWIPTcpInitSem);
    }

    /*
     * Set up global pointer to interface data.
     */
    g_pDevINIPData = pThis;

    struct netif *ret;
    pThis->IntNetIF.name[0] = 'I';
    pThis->IntNetIF.name[1] = 'N';
    ret = netif_add(&pThis->IntNetIF, &ipaddr, &netmask, &gw, NULL,
                    devINIPInterface, lwip_tcpip_input);
    if (!ret)
    {
        rc = VERR_NET_NO_NETWORK;
        goto out;
    }

    lwip_netif_set_default(&pThis->IntNetIF);
    lwip_netif_set_up(&pThis->IntNetIF);

    /* link hack */
    pThis->pLinkHack = g_pDevINILinkHack;

out:
    LogFlow(("%s: return %Rrc\n", __FUNCTION__, rc));
    return rc;
}


/**
 * Destruct a device instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(int) devINIPDestruct(PPDMDEVINS pDevIns)
{
    PDEVINTNETIP pThis = PDMINS_2_DATA(pDevIns, PDEVINTNETIP);

    LogFlow(("%s: pDevIns=%p\n", __FUNCTION__, pDevIns));

    if (g_pDevINIPData != NULL)
    {
        netif_set_down(&pThis->IntNetIF);
        netif_remove(&pThis->IntNetIF);
        tcpip_terminate();
        lwip_sys_sem_wait(pThis->LWIPTcpInitSem);
        lwip_sys_sem_free(pThis->LWIPTcpInitSem);
    }

    if (pThis->pszIP)
        MMR3HeapFree(pThis->pszIP);
    if (pThis->pszNetmask)
        MMR3HeapFree(pThis->pszNetmask);
    if (pThis->pszGateway)
        MMR3HeapFree(pThis->pszGateway);

    LogFlow(("%s: success\n", __FUNCTION__));
    return VINF_SUCCESS;
}


/**
 * Query whether lwIP is initialized or not. Since there is only a single
 * instance of this device ever for a VM, it can be a global function.
 *
 * @returns True if lwIP is initialized.
 */
bool DevINIPConfigured(void)
{
    return g_pDevINIPData != NULL;
}


/**
 * Internal network IP stack device registration record.
 */
const PDMDEVREG g_DeviceINIP =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szDeviceName */
    "IntNetIP",
    /* szRCMod/szR0Mod */
    "",
    "",
    /* pszDescription */
    "Internal Network IP stack device",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS,
    /* fClass. As this is used by the storage devices, it must come earlier. */
    PDM_DEVREG_CLASS_VMM_DEV,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(DEVINTNETIP),
    /* pfnConstruct */
    devINIPConstruct,
    /* pfnDestruct */
    devINIPDestruct,
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
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnQueryInterface */
    NULL,
    /* pfnInitComplete */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};
