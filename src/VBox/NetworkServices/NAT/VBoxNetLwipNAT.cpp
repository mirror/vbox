/* $Id$ */
/** @file
 * VBoxNetNAT - NAT Service for connecting to IntNet.
 */

/*
 * Copyright (C) 2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "winutils.h"

#include <VBox/com/com.h>
#include <VBox/com/listeners.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/VirtualBox.h>
#include <VBox/com/NativeEventQueue.h>

#include <iprt/net.h>
#include <iprt/initterm.h>
#include <iprt/alloca.h>
#ifndef RT_OS_WINDOWS
# include <arpa/inet.h>
#endif
#include <iprt/err.h>
#include <iprt/time.h>
#include <iprt/timer.h>
#include <iprt/thread.h>
#include <iprt/stream.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/pipe.h>
#include <iprt/getopt.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/req.h>
#include <iprt/file.h>
#include <iprt/semaphore.h>
#include <iprt/cpp/utils.h>
#define LOG_GROUP LOG_GROUP_NAT_SERVICE
#include <VBox/log.h>

#include <VBox/sup.h>
#include <VBox/intnet.h>
#include <VBox/intnetinline.h>
#include <VBox/vmm/pdmnetinline.h>
#include <VBox/vmm/vmm.h>
#include <VBox/version.h>

#ifndef RT_OS_WINDOWS
# include <sys/poll.h>
# include <sys/socket.h>
# include <netinet/in.h>
# ifdef RT_OS_LINUX
#  include <linux/icmp.h>       /* ICMP_FILTER */
# endif
# include <netinet/icmp6.h>
#endif

#include <vector>
#include <string>

#include "../NetLib/VBoxNetLib.h"
#include "../NetLib/VBoxNetBaseService.h"
#include "VBoxLwipCore.h"

extern "C"
{
/* bunch of LWIP headers */
#include "lwip/opt.h"
#ifdef LWIP_SOCKET
# undef LWIP_SOCKET
# define LWIP_SOCKET 0
#endif
#include "lwip/sys.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/ethip6.h"
#include "lwip/nd6.h"           // for proxy_na_hook
#include "lwip/mld6.h"
#include "lwip/tcpip.h"
#include "netif/etharp.h"

#include "proxy.h"
#include "pxremap.h"
#include "portfwd.h"
}

#include "../NetLib/VBoxPortForwardString.h"

static RTGETOPTDEF g_aGetOptDef[] =
{
    { "--port-forward4",           'p',   RTGETOPT_REQ_STRING },
    { "--port-forward6",           'P',   RTGETOPT_REQ_STRING }
};

typedef struct NATSEVICEPORTFORWARDRULE
{
    PORTFORWARDRULE Pfr;
    fwspec         FWSpec;
} NATSEVICEPORTFORWARDRULE, *PNATSEVICEPORTFORWARDRULE;

typedef std::vector<NATSEVICEPORTFORWARDRULE> VECNATSERVICEPF;
typedef VECNATSERVICEPF::iterator ITERATORNATSERVICEPF;
typedef VECNATSERVICEPF::const_iterator CITERATORNATSERVICEPF;


class VBoxNetLwipNAT;


class NATNetworkListener
{
public:
    NATNetworkListener():m_pNAT(NULL){}

    HRESULT init(VBoxNetLwipNAT *pNAT)
    {
        AssertPtrReturn(pNAT, E_INVALIDARG);

        m_pNAT = pNAT;
        return S_OK;
    }

    HRESULT init()
    {
        m_pNAT = NULL;
        return S_OK;
    }

    void uninit() { m_pNAT = NULL; }

    STDMETHOD(HandleEvent)(VBoxEventType_T aEventType, IEvent *pEvent);

private:
    VBoxNetLwipNAT *m_pNAT;
};
typedef ListenerImpl<NATNetworkListener, VBoxNetLwipNAT *> NATNetworkListenerImpl;
VBOX_LISTENER_DECLARE(NATNetworkListenerImpl)


class VBoxNetLwipNAT: public VBoxNetBaseService
{
    friend class NATNetworkListener;
  public:
    VBoxNetLwipNAT(SOCKET icmpsock4, SOCKET icmpsock6);
    virtual ~VBoxNetLwipNAT();
    void usage(){                /* @todo: should be implemented */ };
    int run();
    virtual int init(void);
    /* @todo: when configuration would be really needed */
    virtual int parseOpt(int rc, const RTGETOPTUNION& getOptVal);
    /* VBoxNetNAT always needs Main */
    virtual bool isMainNeeded() const { return true; }
    virtual int processFrame(void *, size_t);
    virtual int processGSO(PCPDMNETWORKGSO, size_t);
    virtual int processUDP(void *, size_t) { return VERR_IGNORED; }

   private:
    struct proxy_options m_ProxyOptions;
    struct sockaddr_in m_src4;
    struct sockaddr_in6 m_src6;
    /**
     * place for registered local interfaces.
     */
    ip4_lomap m_lo2off[10];
    ip4_lomap_desc m_loOptDescriptor;

    uint16_t m_u16Mtu;
    netif m_LwipNetIf;
    /* Queues */
    RTREQQUEUE  hReqIntNet;
    /* thread where we're waiting for a frames, no semaphores needed */
    RTTHREAD hThrIntNetRecv;

    /* Our NAT network descriptor in Main */
    ComPtr<INATNetwork> m_net;
    ComObjPtr<NATNetworkListenerImpl> m_listener;

    ComPtr<IHost> m_host;
    ComObjPtr<NATNetworkListenerImpl> m_vboxListener;
    static INTNETSEG aXmitSeg[64];

    STDMETHOD(HandleEvent)(VBoxEventType_T aEventType, IEvent *pEvent);

    const char **getHostNameservers();

    RTSEMEVENT hSemSVC;
    /* Only for debug needs, by default NAT service should load rules from SVC
     * on startup, and then on sync them on events.
     */
    bool fDontLoadRulesOnStartup;
    static void onLwipTcpIpInit(void *arg);
    static void onLwipTcpIpFini(void *arg);
    static err_t netifInit(netif *pNetif);
    static err_t netifLinkoutput(netif *pNetif, pbuf *pBuf);
    static int intNetThreadRecv(RTTHREAD, void *);

    VECNATSERVICEPF m_vecPortForwardRule4;
    VECNATSERVICEPF m_vecPortForwardRule6;

    static int natServicePfRegister(NATSEVICEPORTFORWARDRULE& natServicePf);
    static int natServiceProcessRegisteredPf(VECNATSERVICEPF& vecPf);
};


static VBoxNetLwipNAT *g_pLwipNat;
INTNETSEG VBoxNetLwipNAT::aXmitSeg[64];

STDMETHODIMP NATNetworkListener::HandleEvent(VBoxEventType_T aEventType, IEvent *pEvent)
{
    if (m_pNAT)
        return m_pNAT->HandleEvent(aEventType, pEvent);
    else
        return E_FAIL;
}



STDMETHODIMP VBoxNetLwipNAT::HandleEvent(VBoxEventType_T aEventType,
                                                  IEvent *pEvent)
{
    HRESULT hrc = S_OK;
    switch (aEventType)
    {
        case VBoxEventType_OnNATNetworkSetting:
        {
            ComPtr<INATNetworkSettingEvent> evSettings(pEvent);
            // XXX: only handle IPv6 default route for now

            if (!m_ProxyOptions.ipv6_enabled)
            {
                break;
            }

            BOOL fIPv6DefaultRoute = FALSE;
            hrc = evSettings->COMGETTER(AdvertiseDefaultIPv6RouteEnabled)(&fIPv6DefaultRoute);
            AssertReturn(SUCCEEDED(hrc), hrc);

            if (m_ProxyOptions.ipv6_defroute == fIPv6DefaultRoute)
            {
                break;
            }

            m_ProxyOptions.ipv6_defroute = fIPv6DefaultRoute;
            tcpip_callback_with_block(proxy_rtadvd_do_quick, &m_LwipNetIf, 0);

            break;
        }
        
        case VBoxEventType_OnNATNetworkPortForward:
        {
            com::Bstr name, strHostAddr, strGuestAddr;
            LONG lHostPort, lGuestPort;
            BOOL fCreateFW, fIPv6FW;
            NATProtocol_T proto = NATProtocol_TCP;


            ComPtr<INATNetworkPortForwardEvent> pfEvt = pEvent;

            hrc = pfEvt->COMGETTER(Name)(name.asOutParam());
            AssertReturn(SUCCEEDED(hrc), hrc);

            hrc = pfEvt->COMGETTER(Proto)(&proto);
            AssertReturn(SUCCEEDED(hrc), hrc);

            hrc = pfEvt->COMGETTER(HostIp)(strHostAddr.asOutParam());
            AssertReturn(SUCCEEDED(hrc), hrc);

            hrc = pfEvt->COMGETTER(HostPort)(&lHostPort);
            AssertReturn(SUCCEEDED(hrc), hrc);

            hrc = pfEvt->COMGETTER(GuestIp)(strGuestAddr.asOutParam());
            AssertReturn(SUCCEEDED(hrc), hrc);

            hrc = pfEvt->COMGETTER(GuestPort)(&lGuestPort);
            AssertReturn(SUCCEEDED(hrc), hrc);

            hrc = pfEvt->COMGETTER(Create)(&fCreateFW);
            AssertReturn(SUCCEEDED(hrc), hrc);

            hrc = pfEvt->COMGETTER(Ipv6)(&fIPv6FW);
            AssertReturn(SUCCEEDED(hrc), hrc);

            VECNATSERVICEPF& rules = (fIPv6FW ?
                                      m_vecPortForwardRule6 :
                                      m_vecPortForwardRule4);

            NATSEVICEPORTFORWARDRULE r;

            RT_ZERO(r);

            if (name.length() > sizeof(r.Pfr.szPfrName))
            {
                hrc = E_INVALIDARG;
                goto port_forward_done;
            }

            r.Pfr.fPfrIPv6 = fIPv6FW;

            switch (proto)
            {
                case NATProtocol_TCP:
                    r.Pfr.iPfrProto = IPPROTO_TCP;
                    break;
                case NATProtocol_UDP:
                    r.Pfr.iPfrProto = IPPROTO_UDP;
                    break;
                default:
                    goto port_forward_done;
            }


            RTStrPrintf(r.Pfr.szPfrName, sizeof(r.Pfr.szPfrName),
                      "%s", com::Utf8Str(name).c_str());

            RTStrPrintf(r.Pfr.szPfrHostAddr, sizeof(r.Pfr.szPfrHostAddr),
                      "%s", com::Utf8Str(strHostAddr).c_str());

            /* XXX: limits should be checked */
            r.Pfr.u16PfrHostPort = (uint16_t)lHostPort;

            RTStrPrintf(r.Pfr.szPfrGuestAddr, sizeof(r.Pfr.szPfrGuestAddr),
                      "%s", com::Utf8Str(strGuestAddr).c_str());

            /* XXX: limits should be checked */
            r.Pfr.u16PfrGuestPort = (uint16_t)lGuestPort;

            if (fCreateFW) /* Addition */
            {
                rules.push_back(r);

                natServicePfRegister(rules.back());
            }
            else /* Deletion */
            {
                ITERATORNATSERVICEPF it;
                for (it = rules.begin(); it != rules.end(); ++it)
                {
                    /* compare */
                    NATSEVICEPORTFORWARDRULE& natFw = *it;
                    if (   natFw.Pfr.iPfrProto == r.Pfr.iPfrProto
                        && natFw.Pfr.u16PfrHostPort == r.Pfr.u16PfrHostPort
                        && (strncmp(natFw.Pfr.szPfrHostAddr, r.Pfr.szPfrHostAddr, INET6_ADDRSTRLEN) == 0)
                        && natFw.Pfr.u16PfrGuestPort == r.Pfr.u16PfrGuestPort
                        && (strncmp(natFw.Pfr.szPfrGuestAddr, r.Pfr.szPfrGuestAddr, INET6_ADDRSTRLEN) == 0))
                    {
                        fwspec *pFwCopy = (fwspec *)RTMemAllocZ(sizeof(fwspec));
                        if (!pFwCopy)
                        {
                            break;
                        }
                        memcpy(pFwCopy, &natFw.FWSpec, sizeof(fwspec));

                        /* We shouldn't care about pFwCopy this memory will be freed when
                         * will message will arrive to the destination.
                         */
                        portfwd_rule_del(pFwCopy);

                        rules.erase(it);
                        break;
                    }
                } /* loop over vector elements */
            } /* condition add or delete */
        port_forward_done:
            /* clean up strings */
            name.setNull();
            strHostAddr.setNull();
            strGuestAddr.setNull();
            break;
        }

        case VBoxEventType_OnHostNameResolutionConfigurationChange:
        {
            const char **ppcszNameServers = getHostNameservers();
            err_t error;

            error = tcpip_callback_with_block(pxdns_set_nameservers,
                                              ppcszNameServers,
                                              /* :block */ 0);
            if (error != ERR_OK && ppcszNameServers != NULL)
            {
                RTMemFree(ppcszNameServers);
            }
            break;
        }
    }
    return hrc;
}


void VBoxNetLwipNAT::onLwipTcpIpInit(void* arg)
{
    AssertPtrReturnVoid(arg);
    VBoxNetLwipNAT *pNat = static_cast<VBoxNetLwipNAT *>(arg);

    HRESULT hrc = com::Initialize();
    Assert(!FAILED(hrc));

    proxy_arp_hook = pxremap_proxy_arp;
    proxy_ip4_divert_hook = pxremap_ip4_divert;

    proxy_na_hook = pxremap_proxy_na;
    proxy_ip6_divert_hook = pxremap_ip6_divert;

    /* lwip thread */
    RTNETADDRIPV4 network;
    RTNETADDRIPV4 address = g_pLwipNat->getIpv4Address();
    RTNETADDRIPV4 netmask = g_pLwipNat->getIpv4Netmask();
    network.u = address.u & netmask.u;

    ip_addr LwipIpAddr, LwipIpNetMask, LwipIpNetwork;

    memcpy(&LwipIpAddr, &address, sizeof(ip_addr));
    memcpy(&LwipIpNetMask, &netmask, sizeof(ip_addr));
    memcpy(&LwipIpNetwork, &network, sizeof(ip_addr));

    netif *pNetif = netif_add(&g_pLwipNat->m_LwipNetIf /* Lwip Interface */,
                              &LwipIpAddr /* IP address*/,
                              &LwipIpNetMask /* Network mask */,
                              &LwipIpAddr /* gateway address, @todo: is self IP acceptable? */,
                              g_pLwipNat /* state */,
                              VBoxNetLwipNAT::netifInit /* netif_init_fn */,
                              lwip_tcpip_input /* netif_input_fn */);

    AssertPtrReturnVoid(pNetif);

    netif_set_up(pNetif);
    netif_set_link_up(pNetif);

    if (pNat->m_ProxyOptions.ipv6_enabled) {
        /*
         * XXX: lwIP currently only ever calls mld6_joingroup() in
         * nd6_tmr() for fresh tentative addresses, which is a wrong place
         * to do it - but I'm not keen on fixing this properly for now
         * (with correct handling of interface up and down transitions,
         * etc).  So stick it here as a kludge.
         */
        for (int i = 0; i <= 1; ++i) {
            ip6_addr_t *paddr = netif_ip6_addr(pNetif, i);

            ip6_addr_t solicited_node_multicast_address;
            ip6_addr_set_solicitednode(&solicited_node_multicast_address,
                                       paddr->addr[3]);
            mld6_joingroup(paddr, &solicited_node_multicast_address);
        }

        /*
         * XXX: We must join the solicited-node multicast for the
         * addresses we do IPv6 NA-proxy for.  We map IPv6 loopback to
         * proxy address + 1.  We only need the low 24 bits, and those are
         * fixed.
         */
        {
            ip6_addr_t solicited_node_multicast_address;

            ip6_addr_set_solicitednode(&solicited_node_multicast_address,
                                       /* last 24 bits of the address */
                                       PP_HTONL(0x00000002));
            mld6_netif_joingroup(pNetif,  &solicited_node_multicast_address);
        }
    }

    proxy_init(&g_pLwipNat->m_LwipNetIf, &g_pLwipNat->m_ProxyOptions);

    natServiceProcessRegisteredPf(g_pLwipNat->m_vecPortForwardRule4);
    natServiceProcessRegisteredPf(g_pLwipNat->m_vecPortForwardRule6);
}


void VBoxNetLwipNAT::onLwipTcpIpFini(void* arg)
{
    AssertPtrReturnVoid(arg);
    VBoxNetLwipNAT *pThis = (VBoxNetLwipNAT *)arg;

    /* XXX: proxy finalization */
    netif_set_link_down(&g_pLwipNat->m_LwipNetIf);
    netif_set_down(&g_pLwipNat->m_LwipNetIf);
    netif_remove(&g_pLwipNat->m_LwipNetIf);

}

/*
 * Callback for netif_add() to initialize the interface.
 */
err_t VBoxNetLwipNAT::netifInit(netif *pNetif)
{
    err_t rcLwip = ERR_OK;

    AssertPtrReturn(pNetif, ERR_ARG);

    VBoxNetLwipNAT *pNat = static_cast<VBoxNetLwipNAT *>(pNetif->state);
    AssertPtrReturn(pNat, ERR_ARG);

    LogFlowFunc(("ENTER: pNetif[%c%c%d]\n", pNetif->name[0], pNetif->name[1], pNetif->num));
    /* validity */
    AssertReturn(   pNetif->name[0] == 'N'
                 && pNetif->name[1] == 'T', ERR_ARG);


    pNetif->hwaddr_len = sizeof(RTMAC);
    RTMAC mac = g_pLwipNat->getMacAddress();
    memcpy(pNetif->hwaddr, &mac, sizeof(RTMAC));

    pNat->m_u16Mtu = 1500; // XXX: FIXME
    pNetif->mtu = pNat->m_u16Mtu;

    pNetif->flags = NETIF_FLAG_BROADCAST
      | NETIF_FLAG_ETHARP                /* Don't bother driver with ARP and let Lwip resolve ARP handling */
      | NETIF_FLAG_ETHERNET;             /* Lwip works with ethernet too */

    pNetif->linkoutput = netifLinkoutput; /* ether-level-pipe */
    pNetif->output = lwip_etharp_output; /* ip-pipe */

    if (pNat->m_ProxyOptions.ipv6_enabled) {
        pNetif->output_ip6 = ethip6_output;

        /* IPv6 link-local address in slot 0 */
        netif_create_ip6_linklocal_address(pNetif, /* :from_mac_48bit */ 1);
        netif_ip6_addr_set_state(pNetif, 0, IP6_ADDR_PREFERRED); // skip DAD

        /*
         * RFC 4193 Locally Assigned Global ID (ULA) in slot 1
         * [fd17:625c:f037:XXXX::1] where XXXX, 16 bit Subnet ID, are two
         * bytes from the middle of the IPv4 address, e.g. :dead: for
         * 10.222.173.1
         */
        u8_t nethi = ip4_addr2(&pNetif->ip_addr);
        u8_t netlo = ip4_addr3(&pNetif->ip_addr);

        ip6_addr_t *paddr = netif_ip6_addr(pNetif, 1);
        IP6_ADDR(paddr, 0,   0xFD, 0x17,   0x62, 0x5C);
        IP6_ADDR(paddr, 1,   0xF0, 0x37,  nethi, netlo);
        IP6_ADDR(paddr, 2,   0x00, 0x00,   0x00, 0x00);
        IP6_ADDR(paddr, 3,   0x00, 0x00,   0x00, 0x01);
        netif_ip6_addr_set_state(pNetif, 1, IP6_ADDR_PREFERRED);

#if LWIP_IPV6_SEND_ROUTER_SOLICIT
        pNetif->rs_count = 0;
#endif
    }

    LogFlowFunc(("LEAVE: %d\n", rcLwip));
    return rcLwip;
}


/**
 * Intnet-recv thread
 */
int VBoxNetLwipNAT::intNetThreadRecv(RTTHREAD, void *)
{
    int rc = VINF_SUCCESS;

    /* 1. initialization and connection */
    HRESULT hrc = com::Initialize();
    if (FAILED(hrc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to initialize COM!");

    g_pLwipNat->doReceiveLoop();
    /* 3. deinitilization and termination */
    LogFlowFuncLeaveRC(rc);
    return rc;
}


err_t VBoxNetLwipNAT::netifLinkoutput(netif *pNetif, pbuf *pPBuf)
{
    AssertPtrReturn(pNetif, ERR_ARG);
    AssertPtrReturn(pPBuf, ERR_ARG);
    AssertReturn((void *)g_pLwipNat == pNetif->state, ERR_ARG);

    LogFlowFunc(("ENTER: pNetif[%c%c%d], pPbuf:%p\n",
                 pNetif->name[0],
                 pNetif->name[1],
                 pNetif->num,
                 pPBuf));

    RT_ZERO(VBoxNetLwipNAT::aXmitSeg);
    
    unsigned idx = 0;
    for( struct pbuf *pPBufPtr = pPBuf;
         pPBufPtr;
         pPBufPtr = pPBufPtr->next, ++idx) 
    {
        AssertReturn(idx < RT_ELEMENTS(VBoxNetLwipNAT::aXmitSeg), ERR_MEM);
        VBoxNetLwipNAT::aXmitSeg[idx].pv = pPBufPtr->payload; 
        VBoxNetLwipNAT::aXmitSeg[idx].cb = pPBufPtr->len;
    }

    int rc = g_pLwipNat->sendBufferOnWire(VBoxNetLwipNAT::aXmitSeg, idx, pPBuf->tot_len);
    AssertRCReturn(rc, ERR_IF);

    g_pLwipNat->flushWire();

    LogFlowFunc(("LEAVE: %d\n", ERR_OK));
    return ERR_OK;
}


VBoxNetLwipNAT::VBoxNetLwipNAT(SOCKET icmpsock4, SOCKET icmpsock6) : VBoxNetBaseService("VBoxNetNAT", "nat-network")
{
    LogFlowFuncEnter();

    m_ProxyOptions.ipv6_enabled = 0;
    m_ProxyOptions.ipv6_defroute = 0;
    m_ProxyOptions.icmpsock4 = icmpsock4;
    m_ProxyOptions.icmpsock6 = icmpsock6;
    m_ProxyOptions.tftp_root = NULL;
    m_ProxyOptions.src4 = NULL;
    m_ProxyOptions.src6 = NULL;
    memset(&m_src4, 0, sizeof(m_src4));
    memset(&m_src6, 0, sizeof(m_src6));
    m_src4.sin_family = AF_INET;
    m_src6.sin6_family = AF_INET6;
#if HAVE_SA_LEN
    m_src4.sin_len = sizeof(m_src4);
    m_src6.sin6_len = sizeof(m_src6);
#endif
    m_ProxyOptions.nameservers = NULL;

    m_LwipNetIf.name[0] = 'N';
    m_LwipNetIf.name[1] = 'T';

    RTMAC mac;
    mac.au8[0] = 0x52;
    mac.au8[1] = 0x54;
    mac.au8[2] = 0;
    mac.au8[3] = 0x12;
    mac.au8[4] = 0x35;
    mac.au8[5] = 0;
    setMacAddress(mac);

    RTNETADDRIPV4 address;
    address.u     = RT_MAKE_U32_FROM_U8( 10,  0,  2,  2); // NB: big-endian
    setIpv4Address(address);
    
    address.u     = RT_H2N_U32_C(0xffffff00);
    setIpv4Netmask(address);

    fDontLoadRulesOnStartup = false;

    for(unsigned int i = 0; i < RT_ELEMENTS(g_aGetOptDef); ++i)
        addCommandLineOption(&g_aGetOptDef[i]);

    LogFlowFuncLeave();
}


VBoxNetLwipNAT::~VBoxNetLwipNAT()
{
    if (m_ProxyOptions.tftp_root != NULL)
    {
        RTStrFree((char *)m_ProxyOptions.tftp_root);
    }
}


int VBoxNetLwipNAT::natServicePfRegister(NATSEVICEPORTFORWARDRULE& natPf)
{
    int lrc = 0;
    int rc = VINF_SUCCESS;
    int socketSpec = SOCK_STREAM;
    char *pszHostAddr;
    int sockFamily = (natPf.Pfr.fPfrIPv6 ? PF_INET6 : PF_INET);

    switch(natPf.Pfr.iPfrProto)
    {
        case IPPROTO_TCP:
            socketSpec = SOCK_STREAM;
            break;
        case IPPROTO_UDP:
            socketSpec = SOCK_DGRAM;
            break;
        default:
            return VERR_IGNORED; /* Ah, just ignore the garbage */
    }

    pszHostAddr = natPf.Pfr.szPfrHostAddr;

    /* XXX: workaround for inet_pton and an empty ipv4 address
     * in rule declaration.
     */
    if (   sockFamily == PF_INET
        && pszHostAddr[0] == 0)
        pszHostAddr = (char *)"0.0.0.0"; /* XXX: fix it! without type cast */


    lrc = fwspec_set(&natPf.FWSpec,
                     sockFamily,
                     socketSpec,
                     pszHostAddr,
                     natPf.Pfr.u16PfrHostPort,
                     natPf.Pfr.szPfrGuestAddr,
                     natPf.Pfr.u16PfrGuestPort);

    AssertReturn(!lrc, VERR_IGNORED);

    fwspec *pFwCopy = (fwspec *)RTMemAllocZ(sizeof(fwspec));
    AssertPtrReturn(pFwCopy, VERR_IGNORED);

    /*
     * We need pass the copy, because we can't be sure
     * how much this pointer will be valid in LWIP environment.
     */
    memcpy(pFwCopy, &natPf.FWSpec, sizeof(fwspec));

    lrc = portfwd_rule_add(pFwCopy);

    AssertReturn(!lrc, VERR_IGNORED);

    return VINF_SUCCESS;
}


int VBoxNetLwipNAT::natServiceProcessRegisteredPf(VECNATSERVICEPF& vecRules){
    ITERATORNATSERVICEPF it;
    for (it = vecRules.begin();
         it != vecRules.end(); ++it)
    {
        int rc = natServicePfRegister((*it));
        if (RT_FAILURE(rc))
        {
            LogRel(("PF: %s is ignored\n", (*it).Pfr.szPfrName));
            continue;
        }
    }
    return VINF_SUCCESS;
}


int VBoxNetLwipNAT::init()
{
    HRESULT hrc;
    LogFlowFuncEnter();


    /* virtualbox initialized in super class */

    int rc = ::VBoxNetBaseService::init();
    AssertRCReturn(rc, rc);

    std::string networkName = getNetwork();
    hrc = virtualbox->FindNATNetworkByName(com::Bstr(networkName.c_str()).raw(),
                                                  m_net.asOutParam());
    AssertComRCReturn(hrc, VERR_NOT_FOUND);

    hrc = m_listener.createObject();
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);

    hrc = m_listener->init(new NATNetworkListener(), this);
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);

    ComPtr<IEventSource> esNet;
    hrc = m_net->COMGETTER(EventSource)(esNet.asOutParam());
    AssertComRC(hrc);

    com::SafeArray<VBoxEventType_T> aNetEvents;
    aNetEvents.push_back(VBoxEventType_OnNATNetworkPortForward);
    aNetEvents.push_back(VBoxEventType_OnNATNetworkSetting);
    hrc = esNet->RegisterListener(m_listener, ComSafeArrayAsInParam(aNetEvents), true);
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);


    // resolver changes are reported on vbox but are retrieved from
    // host so stash a pointer for future lookups
    hrc = virtualbox->COMGETTER(Host)(m_host.asOutParam());
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);

    hrc = m_vboxListener.createObject();
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);

    hrc = m_vboxListener->init(new NATNetworkListener(), this);
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);

    ComPtr<IEventSource> esVBox;
    hrc = virtualbox->COMGETTER(EventSource)(esVBox.asOutParam());
    AssertComRC(hrc);

    com::SafeArray<VBoxEventType_T> aVBoxEvents;
    aVBoxEvents.push_back(VBoxEventType_OnHostNameResolutionConfigurationChange);
    hrc = esVBox->RegisterListener(m_vboxListener, ComSafeArrayAsInParam(aVBoxEvents), true);
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);


    BOOL fIPv6Enabled = FALSE;
    hrc = m_net->COMGETTER(IPv6Enabled)(&fIPv6Enabled);
    AssertComRCReturn(hrc, VERR_NOT_FOUND);

    BOOL fIPv6DefaultRoute = FALSE;
    if (fIPv6Enabled)
    {
        hrc = m_net->COMGETTER(AdvertiseDefaultIPv6RouteEnabled)(&fIPv6DefaultRoute);
        AssertComRCReturn(hrc, VERR_NOT_FOUND);
    }

    m_ProxyOptions.ipv6_enabled = fIPv6Enabled;
    m_ProxyOptions.ipv6_defroute = fIPv6DefaultRoute;


    com::Bstr bstrSourceIp4Key = com::BstrFmt("NAT/%s/SourceIp4", networkName.c_str());
    com::Bstr bstrSourceIpX;
    hrc = virtualbox->GetExtraData(bstrSourceIp4Key.raw(), bstrSourceIpX.asOutParam());
    if (SUCCEEDED(hrc))
    {
        RTNETADDRIPV4 addr;
        rc = RTNetStrToIPv4Addr(com::Utf8Str(bstrSourceIpX).c_str(), &addr);
        if (RT_SUCCESS(rc))
        {
            RT_ZERO(m_src4);

            m_src4.sin_addr.s_addr = addr.u;
            m_ProxyOptions.src4 = &m_src4;

            bstrSourceIpX.setNull();
        }
    }

    if (!fDontLoadRulesOnStartup)
    {
        /* XXX: extract function and do not duplicate */
        com::SafeArray<BSTR> rules;
        hrc = m_net->COMGETTER(PortForwardRules4)(ComSafeArrayAsOutParam(rules));
        Assert(SUCCEEDED(hrc));

        size_t idxRules = 0;
        for (idxRules = 0; idxRules < rules.size(); ++idxRules)
        {
            Log(("%d-rule: %ls\n", idxRules, rules[idxRules]));
            NATSEVICEPORTFORWARDRULE Rule;
            RT_ZERO(Rule);
            rc = netPfStrToPf(com::Utf8Str(rules[idxRules]).c_str(), 0, &Rule.Pfr);
            AssertRC(rc);
            m_vecPortForwardRule4.push_back(Rule);
        }

        rules.setNull();
        hrc = m_net->COMGETTER(PortForwardRules6)(ComSafeArrayAsOutParam(rules));
        Assert(SUCCEEDED(hrc));

        for (idxRules = 0; idxRules < rules.size(); ++idxRules)
        {
            Log(("%d-rule: %ls\n", idxRules, rules[idxRules]));
            NATSEVICEPORTFORWARDRULE Rule;
            netPfStrToPf(com::Utf8Str(rules[idxRules]).c_str(), 1, &Rule.Pfr);
            m_vecPortForwardRule6.push_back(Rule);
        }
    } /* if (!fDontLoadRulesOnStartup) */

    com::SafeArray<BSTR> strs;
    int count_strs;
    hrc = m_net->COMGETTER(LocalMappings)(ComSafeArrayAsOutParam(strs));
    if (   SUCCEEDED(hrc)
           && (count_strs = strs.size()))
    {
        unsigned int j = 0;
        int i;

        for (i = 0; i < count_strs && j < RT_ELEMENTS(m_lo2off); ++i)
        {
            char szAddr[17];
            RTNETADDRIPV4 ip4addr;
            char *pszTerm;
            uint32_t u32Off;
            com::Utf8Str strLo2Off(strs[i]);
            const char *pszLo2Off = strLo2Off.c_str();

            RT_ZERO(szAddr);

            pszTerm = RTStrStr(pszLo2Off, "=");

            if (   !pszTerm
                || (pszTerm - pszLo2Off) >= 17)
                continue;

            memcpy(szAddr, pszLo2Off, (pszTerm - pszLo2Off));
            rc = RTNetStrToIPv4Addr(szAddr, &ip4addr);
            if (RT_FAILURE(rc))
                continue;

            u32Off = RTStrToUInt32(pszTerm + 1);
            if (u32Off == 0)
                continue;

            ip4_addr_set_u32(&m_lo2off[j].loaddr, ip4addr.u);
            m_lo2off[j].off = u32Off;
            ++j;
        }

        m_loOptDescriptor.lomap = m_lo2off;
        m_loOptDescriptor.num_lomap = j;
        m_ProxyOptions.lomap_desc = &m_loOptDescriptor;
    }

    com::Bstr bstr;
    hrc = virtualbox->COMGETTER(HomeFolder)(bstr.asOutParam());
    AssertComRCReturn(hrc, VERR_NOT_FOUND);
    if (!bstr.isEmpty())
    {
        com::Utf8Str strTftpRoot(com::Utf8StrFmt("%ls%c%s",
                                     bstr.raw(), RTPATH_DELIMITER, "TFTP"));
        char *pszStrTemp;       // avoid const char ** vs char **
        rc = RTStrUtf8ToCurrentCP(&pszStrTemp, strTftpRoot.c_str());
        AssertRC(rc);
        m_ProxyOptions.tftp_root = pszStrTemp;
    }

    m_ProxyOptions.nameservers = getHostNameservers();

    /* end of COM initialization */

    rc = RTSemEventCreate(&hSemSVC);
    AssertRCReturn(rc, rc);

    rc = RTReqQueueCreate(&hReqIntNet);
    AssertRCReturn(rc, rc);

    rc = g_pLwipNat->tryGoOnline();
    if (RT_FAILURE(rc))
    {
        return rc;
    }

    vboxLwipCoreInitialize(VBoxNetLwipNAT::onLwipTcpIpInit, this);

    rc = RTThreadCreate(&g_pLwipNat->hThrIntNetRecv, /* thread handle*/
                        VBoxNetLwipNAT::intNetThreadRecv,  /* routine */
                        NULL, /* user data */
                        128 * _1K, /* stack size */
                        RTTHREADTYPE_IO, /* type */
                        0, /* flags, @todo: waitable ?*/
                        "INTNET-RECV");
    AssertRCReturn(rc,rc);



    LogFlowFuncLeaveRC(rc);
    return rc;
}


const char **VBoxNetLwipNAT::getHostNameservers()
{
    HRESULT hrc;

    if (m_host.isNull())
    {
        return NULL;
    }

    com::SafeArray<BSTR> aNameServers;
    hrc = m_host->COMGETTER(NameServers)(ComSafeArrayAsOutParam(aNameServers));
    if (FAILED(hrc))
    {
        return NULL;
    }

    const size_t cNameServers = aNameServers.size();
    if (cNameServers == 0)
    {
        return NULL;
    }

    const char **ppcszNameServers =
        (const char **)RTMemAllocZ(sizeof(char *) * (cNameServers + 1));
    if (ppcszNameServers == NULL)
    {
        return NULL;
    }

    size_t idxLast = 0;
    for (size_t i = 0; i < cNameServers; ++i)
    {
        com::Utf8Str strNameServer(aNameServers[i]);
        ppcszNameServers[idxLast] = RTStrDup(strNameServer.c_str());
        if (ppcszNameServers[idxLast] != NULL)
        {
            ++idxLast;
        }
    }

    if (idxLast == 0)
    {
        RTMemFree(ppcszNameServers);
        return NULL;
    }

    return ppcszNameServers;
}


int VBoxNetLwipNAT::parseOpt(int rc, const RTGETOPTUNION& Val)
{
    switch (rc)
    {
        case 'p':
        case 'P':
        {
            NATSEVICEPORTFORWARDRULE Rule;
            VECNATSERVICEPF& rules = (rc == 'P'?
                                        m_vecPortForwardRule6
                                      : m_vecPortForwardRule4);

            fDontLoadRulesOnStartup = true;

            RT_ZERO(Rule);

            int irc = netPfStrToPf(Val.psz, (rc == 'P'), &Rule.Pfr);
            rules.push_back(Rule);
            return VINF_SUCCESS;
        }
        default:;
    }
    return VERR_NOT_FOUND;
}


int VBoxNetLwipNAT::processFrame(void *pvFrame, size_t cbFrame)
{
    AssertReturn(pvFrame && cbFrame, VERR_INVALID_PARAMETER);

    struct  pbuf *pPbufHdr, *pPbuf;
    pPbufHdr = pPbuf = pbuf_alloc(PBUF_RAW, cbFrame, PBUF_POOL);

    AssertMsgReturn(pPbuf, ("NAT: Can't allocate send buffer cbFrame=%u\n", cbFrame), VERR_INTERNAL_ERROR);
    AssertReturn(pPbufHdr->tot_len == cbFrame, VERR_INTERNAL_ERROR);
                    
    uint8_t *pu8Frame = (uint8_t *)pvFrame;
    while(pPbuf)
    {
        memcpy(pPbuf->payload, pu8Frame, pPbuf->len);
        pu8Frame += pPbuf->len;
        pPbuf = pPbuf->next;
    }

    m_LwipNetIf.input(pPbufHdr, &m_LwipNetIf);
    
    return VINF_SUCCESS;
}


int VBoxNetLwipNAT::processGSO(PCPDMNETWORKGSO pGso, size_t cbFrame)
{
    if (!PDMNetGsoIsValid(pGso, cbFrame,
                          cbFrame - sizeof(PDMNETWORKGSO)))
        return VERR_INVALID_PARAMETER;
    
    cbFrame -= sizeof(PDMNETWORKGSO);
    uint8_t         abHdrScratch[256];
    uint32_t const  cSegs = PDMNetGsoCalcSegmentCount(pGso,
                                                      cbFrame);
    for (size_t iSeg = 0; iSeg < cSegs; iSeg++)
    {
        uint32_t cbSegFrame;
        void    *pvSegFrame =
          PDMNetGsoCarveSegmentQD(pGso,
                                  (uint8_t *)(pGso + 1),
                                  cbFrame,
                                  abHdrScratch,
                                  iSeg,
                                  cSegs,
                                  &cbSegFrame);

        struct pbuf *pPbuf = pbuf_alloc(PBUF_RAW, cbSegFrame, PBUF_POOL);

        AssertMsgReturn(pPbuf, ("NAT: Can't allocate send buffer cbFrame=%u\n", cbSegFrame), VERR_INTERNAL_ERROR);
        AssertReturn(!pPbuf->next && pPbuf->len == cbSegFrame, VERR_INTERNAL_ERROR);

        memcpy(pPbuf->payload, pvSegFrame, cbSegFrame);
        m_LwipNetIf.input(pPbuf, &g_pLwipNat->m_LwipNetIf);
    }

    return VINF_SUCCESS;
}


int VBoxNetLwipNAT::run()
{
    /* EventQueue processing from VBoxHeadless.cpp */
    com::NativeEventQueue         *gEventQ = NULL;
    gEventQ = com::NativeEventQueue::getMainEventQueue();
    while(true)
    {
        /* XXX:todo: graceful termination */
        gEventQ->processEventQueue(0);
        gEventQ->processEventQueue(500);
    }

    vboxLwipCoreFinalize(VBoxNetLwipNAT::onLwipTcpIpFini, this);

    m_vecPortForwardRule4.clear();
    m_vecPortForwardRule6.clear();

    return VINF_SUCCESS;
}


/**
 *  Entry point.
 */
extern "C" DECLEXPORT(int) TrustedMain(int argc, char **argv, char **envp)
{
    LogFlowFuncEnter();

    NOREF(envp);

#ifdef RT_OS_WINDOWS
    WSADATA wsaData;
    int err;

    err = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (err)
    {
        fprintf(stderr, "wsastartup: failed (%d)\n", err);
        return 1;
    }
#endif

    SOCKET icmpsock4 = INVALID_SOCKET;
    SOCKET icmpsock6 = INVALID_SOCKET;
#ifndef RT_OS_DARWIN
    const int icmpstype = SOCK_RAW;
#else
    /* on OS X it's not privileged */
    const int icmpstype = SOCK_DGRAM;
#endif

    icmpsock4 = socket(AF_INET, icmpstype, IPPROTO_ICMP);
    if (icmpsock4 == INVALID_SOCKET)
    {
        perror("IPPROTO_ICMP");
    }

    if (icmpsock4 != INVALID_SOCKET)
    {
#ifdef ICMP_FILTER              //  Linux specific
        struct icmp_filter flt = {
            ~(uint32_t)(
                  (1U << ICMP_ECHOREPLY)
                | (1U << ICMP_DEST_UNREACH)
                | (1U << ICMP_TIME_EXCEEDED)
            )
        };

        int status = setsockopt(icmpsock4, SOL_RAW, ICMP_FILTER,
                                &flt, sizeof(flt));
        if (status < 0)
        {
            perror("ICMP_FILTER");
        }
#endif
    }

    icmpsock6 = socket(AF_INET6, icmpstype, IPPROTO_ICMPV6);
    if (icmpsock6 == INVALID_SOCKET)
    {
        perror("IPPROTO_ICMPV6");
    }

    if (icmpsock6 != INVALID_SOCKET)
    {
#ifdef ICMP6_FILTER             // Windows doesn't support RFC 3542 API
        /*
         * XXX: We do this here for now, not in pxping.c, to avoid
         * name clashes between lwIP and system headers.
         */
        struct icmp6_filter flt;
        ICMP6_FILTER_SETBLOCKALL(&flt);

        ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY, &flt);

        ICMP6_FILTER_SETPASS(ICMP6_DST_UNREACH, &flt);
        ICMP6_FILTER_SETPASS(ICMP6_PACKET_TOO_BIG, &flt);
        ICMP6_FILTER_SETPASS(ICMP6_TIME_EXCEEDED, &flt);
        ICMP6_FILTER_SETPASS(ICMP6_PARAM_PROB, &flt);

        int status = setsockopt(icmpsock6, IPPROTO_ICMPV6, ICMP6_FILTER,
                                &flt, sizeof(flt));
        if (status < 0)
        {
            perror("ICMP6_FILTER");
        }
#endif
    }

    HRESULT hrc = com::Initialize();
#ifdef VBOX_WITH_XPCOM
    if (hrc == NS_ERROR_FILE_ACCESS_DENIED)
    {
        char szHome[RTPATH_MAX] = "";
        com::GetVBoxUserHomeDirectory(szHome, sizeof(szHome));
        return RTMsgErrorExit(RTEXITCODE_FAILURE,
               "Failed to initialize COM because the global settings directory '%s' is not accessible!", szHome);
    }
#endif
    if (FAILED(hrc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to initialize COM!");

    g_pLwipNat = new VBoxNetLwipNAT(icmpsock4, icmpsock6);

    Log2(("NAT: initialization\n"));
    int rc = g_pLwipNat->parseArgs(argc - 1, argv + 1);
    rc = (rc == 0) ? VINF_SUCCESS : VERR_GENERAL_FAILURE; /* XXX: FIXME */

    if (RT_SUCCESS(rc))
    {
        rc = g_pLwipNat->init();
    }

    if (RT_SUCCESS(rc))
    {
        g_pLwipNat->run();
    }

    delete g_pLwipNat;
    return 0;
}


#ifndef VBOX_WITH_HARDENING

int main(int argc, char **argv, char **envp)
{
    int rc = RTR3InitExe(argc, &argv, RTR3INIT_FLAGS_SUPLIB);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    return TrustedMain(argc, argv, envp);
}

# if defined(RT_OS_WINDOWS)

static LRESULT CALLBACK WindowProc(HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
    if(uMsg == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc (hwnd, uMsg, wParam, lParam);
}

static LPCWSTR g_WndClassName = L"VBoxNetNatLwipClass";

static DWORD WINAPI MsgThreadProc(__in  LPVOID lpParameter)
{
     HWND                 hwnd = 0;
     HINSTANCE hInstance = (HINSTANCE)GetModuleHandle (NULL);
     bool bExit = false;

     /* Register the Window Class. */
     WNDCLASS wc;
     wc.style         = 0;
     wc.lpfnWndProc   = WindowProc;
     wc.cbClsExtra    = 0;
     wc.cbWndExtra    = sizeof(void *);
     wc.hInstance     = hInstance;
     wc.hIcon         = NULL;
     wc.hCursor       = NULL;
     wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
     wc.lpszMenuName  = NULL;
     wc.lpszClassName = g_WndClassName;

     ATOM atomWindowClass = RegisterClass(&wc);

     if (atomWindowClass != 0)
     {
         /* Create the window. */
         hwnd = CreateWindowEx (WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
                 g_WndClassName, g_WndClassName,
                                                   WS_POPUPWINDOW,
                                                  -200, -200, 100, 100, NULL, NULL, hInstance, NULL);

         if (hwnd)
         {
             SetWindowPos(hwnd, HWND_TOPMOST, -200, -200, 0, 0,
                          SWP_NOACTIVATE | SWP_HIDEWINDOW | SWP_NOCOPYBITS | SWP_NOREDRAW | SWP_NOSIZE);

             MSG msg;
             while (GetMessage(&msg, NULL, 0, 0))
             {
                 TranslateMessage(&msg);
                 DispatchMessage(&msg);
             }

             DestroyWindow (hwnd);

             bExit = true;
         }

         UnregisterClass (g_WndClassName, hInstance);
     }

     if(bExit)
     {
         /* no need any accuracy here, in anyway the DHCP server usually gets terminated with TerminateProcess */
         exit(0);
     }

     return 0;
}



/** (We don't want a console usually.) */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
#if 0
    NOREF(hInstance); NOREF(hPrevInstance); NOREF(lpCmdLine); NOREF(nCmdShow);

    HANDLE hThread = CreateThread(
      NULL, /*__in_opt   LPSECURITY_ATTRIBUTES lpThreadAttributes, */
      0, /*__in       SIZE_T dwStackSize, */
      MsgThreadProc, /*__in       LPTHREAD_START_ROUTINE lpStartAddress,*/
      NULL, /*__in_opt   LPVOID lpParameter,*/
      0, /*__in       DWORD dwCreationFlags,*/
      NULL /*__out_opt  LPDWORD lpThreadId*/
    );

    if(hThread != NULL)
        CloseHandle(hThread);

#endif
    return main(__argc, __argv, environ);
}
# endif /* RT_OS_WINDOWS */

#endif /* !VBOX_WITH_HARDENING */
