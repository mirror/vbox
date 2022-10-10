/* $Id$ */
/** @file
 * VBoxNetDhcpd - DHCP server for host-only and NAT networks.
 */

/*
 * Copyright (C) 2009-2022 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/** @page pg_net_dhcp       VBoxNetDHCP
 *
 * Write a few words...
 *
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/cdefs.h>

/*
 * Need to get host/network order conversion stuff from Windows headers,
 * so we do not define them in LWIP and then try to re-define them in
 * Windows headers.
 */
#ifdef RT_OS_WINDOWS
# include <iprt/win/winsock2.h>
#endif

#include "DhcpdInternal.h"
#include <iprt/param.h>
#include <iprt/errcore.h>

#include <iprt/initterm.h>
#include <iprt/message.h>

#include <iprt/net.h>
#include <iprt/path.h>
#include <iprt/stream.h>

#include <VBox/sup.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/pdmnetinline.h>
#include <VBox/intnet.h>
#include <VBox/intnetinline.h>

#include "VBoxLwipCore.h"
#include "Config.h"
#include "DHCPD.h"
#include "DhcpMessage.h"

extern "C"
{
#include "lwip/sys.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"
#include "netif/etharp.h"
}

#include <iprt/sanitized/string>
#include <vector>
#include <memory>

#ifdef RT_OS_WINDOWS
# include <iprt/win/windows.h>
#endif

#include "IntNetIf.h"

struct delete_pbuf
{
    delete_pbuf() {}
    void operator()(struct pbuf *p) const { pbuf_free(p); }
};

typedef std::unique_ptr<pbuf, delete_pbuf> unique_ptr_pbuf;


class VBoxNetDhcpd
{
    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP(VBoxNetDhcpd);

private:
    PRTLOGGER m_pStderrReleaseLogger;

    /* intnet plumbing */
    INTNETIFCTX    m_hIf;
    PINTNETBUF     m_pIfBuf;

    /* lwip stack connected to the intnet */
    struct netif m_LwipNetif;

    Config *m_Config;

    /* listening pcb */
    struct udp_pcb *m_Dhcp4Pcb;

    DHCPD m_server;

public:
    VBoxNetDhcpd();
    ~VBoxNetDhcpd();

    int main(int argc, char **argv);

private:
    int logInitStderr();

    /*
     * Boilerplate code.
     */
    int ifInit(const RTCString &strNetwork,
               const RTCString &strTrunk = RTCString(),
               INTNETTRUNKTYPE enmTrunkType = kIntNetTrunkType_WhateverNone);

    int ifProcessInput();

    void ifPump();
    int ifInput(void *pvSegFrame, uint32_t cbSegFrame);


    /*
     * lwIP callbacks
     */
    static DECLCALLBACK(void) lwipInitCB(void *pvArg);
    void lwipInit();

    static err_t netifInitCB(netif *pNetif) RT_NOTHROW_PROTO;
    err_t netifInit(netif *pNetif);

    static err_t netifLinkOutputCB(netif *pNetif, pbuf *pPBuf) RT_NOTHROW_PROTO;
    err_t netifLinkOutput(pbuf *pPBuf);

    static void dhcp4RecvCB(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                            ip_addr_t *addr, u16_t port) RT_NOTHROW_PROTO;
    void dhcp4Recv(struct udp_pcb *pcb, struct pbuf *p, ip_addr_t *addr, u16_t port);
};


VBoxNetDhcpd::VBoxNetDhcpd()
  : m_pStderrReleaseLogger(NULL),
    m_hIf(INTNET_HANDLE_INVALID),
    m_pIfBuf(NULL),
    m_LwipNetif(),
    m_Config(NULL),
    m_Dhcp4Pcb(NULL)
{
    logInitStderr();
}


VBoxNetDhcpd::~VBoxNetDhcpd()
{
    if (m_hIf != NULL)
    {
        int rc = IntNetR3IfCtxDestroy(m_hIf);
        AssertRC(rc);
        m_hIf = NULL;
    }
}


/*
 * We don't know the name of the release log file until we parse our
 * configuration because we use network name as basename.  To get
 * early logging to work, start with stderr-only release logger.
 *
 * We disable "sup" for this logger to avoid spam from SUPR3Init().
 */
int VBoxNetDhcpd::logInitStderr()
{
    static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;

    PRTLOGGER pLogger;
    int rc;

    uint32_t fFlags = 0;
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    fFlags |= RTLOGFLAGS_USECRLF;
#endif

    rc = RTLogCreate(&pLogger, fFlags,
                     "all -sup all.restrict -default.restrict",
                     NULL,      /* environment base */
                     RT_ELEMENTS(s_apszGroups), s_apszGroups,
                     RTLOGDEST_STDERR, NULL);
    if (RT_FAILURE(rc))
    {
        RTPrintf("Failed to init stderr logger: %Rrs\n", rc);
        return rc;
    }

    m_pStderrReleaseLogger = pLogger;
    RTLogRelSetDefaultInstance(m_pStderrReleaseLogger);

    return VINF_SUCCESS;
}


int VBoxNetDhcpd::ifInit(const RTCString &strNetwork,
                         const RTCString &strTrunk,
                         INTNETTRUNKTYPE enmTrunkType)
{
    if (enmTrunkType == kIntNetTrunkType_Invalid)
        enmTrunkType = kIntNetTrunkType_WhateverNone;

    int rc = IntNetR3IfCtxCreate(&m_hIf, strNetwork.c_str(), enmTrunkType,
                                 strTrunk.c_str(), _128K /*cbSend*/, _256K /*cbRecv*/,
                                 0 /*fFlags*/);
    if (RT_SUCCESS(rc))
    {
        rc = IntNetR3IfCtxQueryBufferPtr(m_hIf, &m_pIfBuf);
        if (RT_SUCCESS(rc))
            rc = IntNetR3IfCtxSetActive(m_hIf, true /*fActive*/);
    }

    return rc;
}


/**
 * Process incoming packages forever.
 *
 * @note This function normally never returns, given that the process is
 *       typically just killed when shutting down a network.
 */
void VBoxNetDhcpd::ifPump()
{
    for (;;)
    {
        /*
         * Wait for input:
         */
        int rc = IntNetR3IfWait(m_hIf, RT_INDEFINITE_WAIT);
        /*
         * Process any pending input before we wait again:
         */
        if (   RT_SUCCESS(rc)
            || rc == VERR_INTERRUPTED
            || rc == VERR_TIMEOUT /* paranoia */)
            ifProcessInput();
        else
        {
            DHCP_LOG_MSG_ERROR(("ifWait failed: %Rrc\n", rc));
            return;
        }
    }
}


int VBoxNetDhcpd::ifProcessInput()
{
    AssertReturn(m_hIf != NULL, VERR_GENERAL_FAILURE);
    AssertReturn(m_pIfBuf != NULL, VERR_GENERAL_FAILURE);

    PCINTNETHDR pHdr = IntNetRingGetNextFrameToRead(&m_pIfBuf->Recv);
    while (pHdr)
    {
        const uint8_t u8Type = pHdr->u8Type;
        void *pvSegFrame;
        uint32_t cbSegFrame;

        if (u8Type == INTNETHDR_TYPE_FRAME)
        {
            pvSegFrame = IntNetHdrGetFramePtr(pHdr, m_pIfBuf);
            cbSegFrame = pHdr->cbFrame;

            ifInput(pvSegFrame, cbSegFrame);
        }
        else if (u8Type == INTNETHDR_TYPE_GSO)
        {
            size_t cbGso = pHdr->cbFrame;
            size_t cbFrame = cbGso - sizeof(PDMNETWORKGSO);

            PCPDMNETWORKGSO pGso = IntNetHdrGetGsoContext(pHdr, m_pIfBuf);
            if (PDMNetGsoIsValid(pGso, cbGso, cbFrame))
            {
                const uint32_t cSegs = PDMNetGsoCalcSegmentCount(pGso, cbFrame);
                for (uint32_t i = 0; i < cSegs; ++i)
                {
                    uint8_t abHdrScratch[256];
                    pvSegFrame = PDMNetGsoCarveSegmentQD(pGso, (uint8_t *)(pGso + 1), cbFrame,
                                                         abHdrScratch,
                                                         i, cSegs,
                                                         &cbSegFrame);
                    ifInput(pvSegFrame, (uint32_t)cbFrame);
                }
            }
        }

        /* Advance: */
        IntNetRingSkipFrame(&m_pIfBuf->Recv);
        pHdr = IntNetRingGetNextFrameToRead(&m_pIfBuf->Recv);
    }

    return VINF_SUCCESS;
}


/**
 * Got a frame from the internal network, feed it to the lwIP stack.
 */
int VBoxNetDhcpd::ifInput(void *pvFrame, uint32_t cbFrame)
{
    if (pvFrame == NULL)
        return VERR_INVALID_PARAMETER;

    if (   cbFrame <= sizeof(RTNETETHERHDR)
        || cbFrame > UINT16_MAX - ETH_PAD_SIZE)
        return VERR_INVALID_PARAMETER;

    struct pbuf *p = pbuf_alloc(PBUF_RAW, (u16_t)cbFrame + ETH_PAD_SIZE, PBUF_POOL);
    if (RT_UNLIKELY(p == NULL))
        return VERR_NO_MEMORY;

    /*
     * The code below is inlined version of:
     *
     *   pbuf_header(p, -ETH_PAD_SIZE); // hide padding
     *   pbuf_take(p, pvFrame, cbFrame);
     *   pbuf_header(p, ETH_PAD_SIZE);  // reveal padding
     */
    struct pbuf *q = p;
    uint8_t *pbChunk = (uint8_t *)pvFrame;
    do
    {
        uint8_t *payload = (uint8_t *)q->payload;
        size_t len = q->len;

#if ETH_PAD_SIZE
        if (RT_LIKELY(q == p))  /* single pbuf is large enough */
        {
            payload += ETH_PAD_SIZE;
            len -= ETH_PAD_SIZE;
        }
#endif
        memcpy(payload, pbChunk, len);
        pbChunk += len;
        q = q->next;
    } while (RT_UNLIKELY(q != NULL));

    m_LwipNetif.input(p, &m_LwipNetif);
    return VINF_SUCCESS;
}


/**
 * Got a frame from the lwIP stack, feed it to the internal network.
 */
err_t VBoxNetDhcpd::netifLinkOutput(pbuf *pPBuf)
{
    if (pPBuf->tot_len < sizeof(struct eth_hdr)) /* includes ETH_PAD_SIZE */
        return ERR_ARG;

    PINTNETHDR pHdr;
    void *pvFrame;
    u16_t cbFrame = pPBuf->tot_len - ETH_PAD_SIZE;
    int rc = IntNetRingAllocateFrame(&m_pIfBuf->Send, cbFrame, &pHdr, &pvFrame);
    if (RT_FAILURE(rc))
        return ERR_MEM;

    pbuf_copy_partial(pPBuf, pvFrame, cbFrame, ETH_PAD_SIZE);
    IntNetRingCommitFrameEx(&m_pIfBuf->Send, pHdr, cbFrame);

    IntNetR3IfSend(m_hIf);
    return ERR_OK;
}


/* static */ DECLCALLBACK(void) VBoxNetDhcpd::lwipInitCB(void *pvArg)
{
    AssertPtrReturnVoid(pvArg);

    VBoxNetDhcpd *self = static_cast<VBoxNetDhcpd *>(pvArg);
    self->lwipInit();
}


/* static */ err_t VBoxNetDhcpd::netifInitCB(netif *pNetif) RT_NOTHROW_DEF
{
    AssertPtrReturn(pNetif, ERR_ARG);

    VBoxNetDhcpd *self = static_cast<VBoxNetDhcpd *>(pNetif->state);
    return self->netifInit(pNetif);
}


/* static */ err_t VBoxNetDhcpd::netifLinkOutputCB(netif *pNetif, pbuf *pPBuf) RT_NOTHROW_DEF
{
    AssertPtrReturn(pNetif, ERR_ARG);
    AssertPtrReturn(pPBuf, ERR_ARG);

    VBoxNetDhcpd *self = static_cast<VBoxNetDhcpd *>(pNetif->state);
    AssertPtrReturn(self, ERR_IF);

    return self->netifLinkOutput(pPBuf);
}


/* static */ void VBoxNetDhcpd::dhcp4RecvCB(void *arg, struct udp_pcb *pcb,
                                            struct pbuf *p,
                                            ip_addr_t *addr, u16_t port) RT_NOTHROW_DEF
{
    AssertPtrReturnVoid(arg);

    VBoxNetDhcpd *self = static_cast<VBoxNetDhcpd *>(arg);
    self->dhcp4Recv(pcb, p, addr, port);
    pbuf_free(p);
}





int VBoxNetDhcpd::main(int argc, char **argv)
{
    /*
     * Register string format types.
     */
    ClientId::registerFormat();
    Binding::registerFormat();

    /*
     * Parse the command line into a configuration object.
     */
    m_Config = Config::create(argc, argv);
    if (m_Config == NULL)
        return VERR_GENERAL_FAILURE;

    /*
     * Initialize the server.
     */
    int rc = m_server.init(m_Config);
    if (RT_SUCCESS(rc))
    {
        /* connect to the intnet */
        rc = ifInit(m_Config->getNetwork(), m_Config->getTrunk(), m_Config->getTrunkType());
        if (RT_SUCCESS(rc))
        {
            /* setup lwip */
            rc = vboxLwipCoreInitialize(lwipInitCB, this);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Pump packets more or less for ever.
                 */
                ifPump();
            }
            else
                DHCP_LOG_MSG_ERROR(("Terminating - vboxLwipCoreInitialize failed: %Rrc\n", rc));
        }
        else
            DHCP_LOG_MSG_ERROR(("Terminating - ifInit failed: %Rrc\n", rc));
    }
    else
        DHCP_LOG_MSG_ERROR(("Terminating - Dhcpd::init failed: %Rrc\n", rc));
    return rc;
}


void VBoxNetDhcpd::lwipInit()
{
    err_t error;

    ip_addr_t addr, mask;
    ip4_addr_set_u32(&addr, m_Config->getIPv4Address().u);
    ip4_addr_set_u32(&mask, m_Config->getIPv4Netmask().u);

    netif *pNetif = netif_add(&m_LwipNetif,
                              &addr, &mask,
                              IP_ADDR_ANY,               /* gateway */
                              this,                      /* state */
                              VBoxNetDhcpd::netifInitCB, /* netif_init_fn */
                              tcpip_input);              /* netif_input_fn */
    if (pNetif == NULL)
        return;

    netif_set_up(pNetif);
    netif_set_link_up(pNetif);

    m_Dhcp4Pcb = udp_new();
    if (RT_UNLIKELY(m_Dhcp4Pcb == NULL))
        return;                 /* XXX? */

    ip_set_option(m_Dhcp4Pcb, SOF_BROADCAST);
    udp_recv(m_Dhcp4Pcb, dhcp4RecvCB, this);

    error = udp_bind(m_Dhcp4Pcb, IP_ADDR_ANY, RTNETIPV4_PORT_BOOTPS);
    if (error != ERR_OK)
    {
        udp_remove(m_Dhcp4Pcb);
        m_Dhcp4Pcb = NULL;
        return;                 /* XXX? */
    }
}


err_t VBoxNetDhcpd::netifInit(netif *pNetif)
{
    pNetif->hwaddr_len = sizeof(RTMAC);
    memcpy(pNetif->hwaddr, &m_Config->getMacAddress(), sizeof(RTMAC));

    pNetif->mtu = 1500;

    pNetif->flags = NETIF_FLAG_BROADCAST
                  | NETIF_FLAG_ETHARP
                  | NETIF_FLAG_ETHERNET;

    pNetif->linkoutput = netifLinkOutputCB;
    pNetif->output = etharp_output;

    netif_set_default(pNetif);
    return ERR_OK;
}


void VBoxNetDhcpd::dhcp4Recv(struct udp_pcb *pcb, struct pbuf *p,
                             ip_addr_t *addr, u16_t port)
{
    RT_NOREF(pcb, addr, port);

    if (RT_UNLIKELY(p->next != NULL))
        return;                 /* XXX: we want it in one chunk */

    bool broadcasted = ip_addr_cmp(ip_current_dest_addr(), &ip_addr_broadcast)
                    || ip_addr_cmp(ip_current_dest_addr(), &ip_addr_any);

    try
    {
        DhcpClientMessage *msgIn = DhcpClientMessage::parse(broadcasted, p->payload, p->len);
        if (msgIn == NULL)
            return;

        std::unique_ptr<DhcpClientMessage> autoFreeMsgIn(msgIn);

        DhcpServerMessage *msgOut = m_server.process(*msgIn);
        if (msgOut == NULL)
            return;

        std::unique_ptr<DhcpServerMessage> autoFreeMsgOut(msgOut);

        ip_addr_t dst = { msgOut->dst().u };
        if (ip_addr_cmp(&dst, &ip_addr_any))
            ip_addr_copy(dst, ip_addr_broadcast);

        octets_t data;
        int rc = msgOut->encode(data);
        if (RT_FAILURE(rc))
            return;

        unique_ptr_pbuf q ( pbuf_alloc(PBUF_RAW, (u16_t)data.size(), PBUF_RAM) );
        if (!q)
            return;

        err_t error = pbuf_take(q.get(), &data.front(), (u16_t)data.size());
        if (error != ERR_OK)
            return;

        error = udp_sendto(pcb, q.get(), &dst, RTNETIPV4_PORT_BOOTPC);
        if (error != ERR_OK)
            return;
    }
    catch (std::bad_alloc &)
    {
        LogRel(("VBoxNetDhcpd::dhcp4Recv: Caught std::bad_alloc!\n"));
    }
}




/*
 * Entry point.
 */
extern "C" DECLEXPORT(int) TrustedMain(int argc, char **argv)
{
    VBoxNetDhcpd Dhcpd;
    int rc = Dhcpd.main(argc, argv);
    return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


#ifndef VBOX_WITH_HARDENING

int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, RTR3INIT_FLAGS_SUPLIB);
    if (RT_SUCCESS(rc))
        return TrustedMain(argc, argv);
    return RTMsgInitFailure(rc);
}


# ifdef RT_OS_WINDOWS
/** (We don't want a console usually.) */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    RT_NOREF(hInstance, hPrevInstance, lpCmdLine, nCmdShow);

    return main(__argc, __argv);
}
# endif /* RT_OS_WINDOWS */

#endif /* !VBOX_WITH_HARDENING */
