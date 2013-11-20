/* $Id$ */
/** @file
 * VBoxNetDHCP - DHCP Service for connecting to IntNet.
 */

/*
 * Copyright (C) 2009-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/** @page pg_net_dhcp       VBoxNetDHCP
 *
 * Write a few words...
 *
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/com/com.h>
#include <VBox/com/listeners.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/EventQueue.h>
#include <VBox/com/VirtualBox.h>

#include <iprt/alloca.h>
#include <iprt/buildconfig.h>
#include <iprt/err.h>
#include <iprt/net.h>                   /* must come before getopt */
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/time.h>
#include <iprt/string.h>

#include <VBox/sup.h>
#include <VBox/intnet.h>
#include <VBox/intnetinline.h>
#include <VBox/vmm/vmm.h>
#include <VBox/version.h>


#include "../NetLib/VBoxNetLib.h"
#include "../NetLib/shared_ptr.h"

#include <vector>
#include <list>
#include <string>
#include <map>

#include "../NetLib/VBoxNetBaseService.h"

#ifdef RT_OS_WINDOWS /* WinMain */
# include <Windows.h>
# include <stdlib.h>
# ifdef INET_ADDRSTRLEN
/* On Windows INET_ADDRSTRLEN defined as 22 Ws2ipdef.h, because it include port number */
#  undef INET_ADDRSTRLEN
# endif
# define INET_ADDRSTRLEN 16
#else
# include <netinet/in.h>
#endif


#include "Config.h"
/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * DHCP server instance.
 */
class VBoxNetDhcp: public VBoxNetBaseService
{
public:
    VBoxNetDhcp();
    virtual ~VBoxNetDhcp();

    int                 init();
    int                 run(void);
    void                usage(void) { /* XXX: document options */ };
    int                 parseOpt(int rc, const RTGETOPTUNION& getOptVal);

protected:
    bool                handleDhcpMsg(uint8_t uMsgType, PCRTNETBOOTP pDhcpMsg, size_t cb);
    bool                handleDhcpReqDiscover(PCRTNETBOOTP pDhcpMsg, size_t cb);
    bool                handleDhcpReqRequest(PCRTNETBOOTP pDhcpMsg, size_t cb);
    bool                handleDhcpReqDecline(PCRTNETBOOTP pDhcpMsg, size_t cb);
    bool                handleDhcpReqRelease(PCRTNETBOOTP pDhcpMsg, size_t cb);

    void                debugPrintV(int32_t iMinLevel, bool fMsg,  const char *pszFmt, va_list va) const;
    static const char  *debugDhcpName(uint8_t uMsgType);

private:
    int initNoMain();
    int initWithMain();

protected:
    /** @name The DHCP server specific configuration data members.
     * @{ */
    /*
     * XXX: what was the plan? SQL3 or plain text file?
     * How it will coexists with managment from VBoxManagement, who should manage db
     * in that case (VBoxManage, VBoxSVC ???)
     */
    std::string         m_LeaseDBName;

    /** @} */

    /* corresponding dhcp server description in Main */
    ComPtr<IDHCPServer> m_DhcpServer;

    ComPtr<INATNetwork> m_NATNetwork;

    /*
     * We will ignore cmd line parameters IFF there will be some DHCP specific arguments
     * otherwise all paramters will come from Main.
     */
    bool m_fIgnoreCmdLineParameters;

    /*
     * -b -n 10.0.1.2 -m 255.255.255.0 -> to the list processing in
     */
    typedef struct
    {
        char Key;
        std::string strValue;
    } CMDLNPRM;
    std::list<CMDLNPRM> CmdParameterll;
    typedef std::list<CMDLNPRM>::iterator CmdParameterIterator;

    /** @name Debug stuff
     * @{  */
    int32_t             m_cVerbosity;
    uint8_t             m_uCurMsgType;
    size_t              m_cbCurMsg;
    PCRTNETBOOTP        m_pCurMsg;
    VBOXNETUDPHDRS      m_CurHdrs;
    /** @} */
};

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Pointer to the DHCP server. */
static VBoxNetDhcp *g_pDhcp;

/* DHCP server specific options */
static RTGETOPTDEF g_aOptionDefs[] =
{
  { "--lease-db",       'D',   RTGETOPT_REQ_STRING },
  { "--begin-config",   'b',   RTGETOPT_REQ_NOTHING },
  { "--gateway",        'g',   RTGETOPT_REQ_IPV4ADDR },
  { "--lower-ip",       'l',   RTGETOPT_REQ_IPV4ADDR },
  { "--upper-ip",       'u',   RTGETOPT_REQ_IPV4ADDR },
};

/**
 * Construct a DHCP server with a default configuration.
 */
VBoxNetDhcp::VBoxNetDhcp()
{
    m_Name                  = "VBoxNetDhcp";
    m_Network               = "VBoxNetDhcp";
    m_TrunkName             = "";
    m_enmTrunkType          = kIntNetTrunkType_WhateverNone;
    m_MacAddress.au8[0]     = 0x08;
    m_MacAddress.au8[1]     = 0x00;
    m_MacAddress.au8[2]     = 0x27;
    m_MacAddress.au8[3]     = 0x40;
    m_MacAddress.au8[4]     = 0x41;
    m_MacAddress.au8[5]     = 0x42;
    m_Ipv4Address.u         = RT_H2N_U32_C(RT_BSWAP_U32_C(RT_MAKE_U32_FROM_U8( 10,  0,  2,  5)));

    m_pSession              = NIL_RTR0PTR;
    m_cbSendBuf             =  8192;
    m_cbRecvBuf             = 51200; /** @todo tune to 64 KB with help from SrvIntR0 */
    m_hIf                   = INTNET_HANDLE_INVALID;
    m_pIfBuf                = NULL;

    m_cVerbosity            = 0;
    m_uCurMsgType           = UINT8_MAX;
    m_cbCurMsg              = 0;
    m_pCurMsg               = NULL;
    memset(&m_CurHdrs, '\0', sizeof(m_CurHdrs));

    m_fIgnoreCmdLineParameters = true;

    for(unsigned int i = 0; i < RT_ELEMENTS(g_aOptionDefs); ++i)
        m_vecOptionDefs.push_back(&g_aOptionDefs[i]);

#if 0 /* enable to hack the code without a mile long argument list. */
    VBoxNetDhcpCfg *pDefCfg = new VBoxNetDhcpCfg();
    pDefCfg->m_LowerAddr.u    = RT_H2N_U32_C(RT_BSWAP_U32_C(RT_MAKE_U32_FROM_U8( 10,  0,  2,100)));
    pDefCfg->m_UpperAddr.u    = RT_H2N_U32_C(RT_BSWAP_U32_C(RT_MAKE_U32_FROM_U8( 10,  0,  2,250)));
    pDefCfg->m_SubnetMask.u   = RT_H2N_U32_C(RT_BSWAP_U32_C(RT_MAKE_U32_FROM_U8(255,255,255,  0)));
    RTNETADDRIPV4 Addr;
    Addr.u                    = RT_H2N_U32_C(RT_BSWAP_U32_C(RT_MAKE_U32_FROM_U8( 10,  0,  2,  1)));
    pDefCfg->m_Routers.push_back(Addr);
    Addr.u                    = RT_H2N_U32_C(RT_BSWAP_U32_C(RT_MAKE_U32_FROM_U8( 10,  0,  2,  2)));
    pDefCfg->m_DNSes.push_back(Addr);
    pDefCfg->m_DomainName     = "vboxnetdhcp.org";
# if 0
    pDefCfg->m_cSecLease      = 60*60; /* 1 hour */
# else
    pDefCfg->m_cSecLease      = 30; /* sec */
# endif
    pDefCfg->m_TftpServer     = "10.0.2.3"; //??
    this->addConfig(pDefCfg);
#endif
}


/**
 * Destruct a DHCP server.
 */
VBoxNetDhcp::~VBoxNetDhcp()
{
}




/**
 * Parse the DHCP specific arguments.
 *
 * This callback caled for each paramenter so
 * ....
 * we nee post analisys of the parameters, at least
 *    for -b, -g, -l, -u, -m
 */
int VBoxNetDhcp::parseOpt(int rc, const RTGETOPTUNION& Val)
{
    CMDLNPRM prm;

    /* Ok, we've entered here, thus we can't ignore cmd line parameters anymore */
    m_fIgnoreCmdLineParameters = false;

    prm.Key = rc;

    switch (rc)
    {
        case 'l':
        case 'u':
        case 'g': 
        {
            char buf[17];
            RTStrPrintf(buf, 17, "%RTnaipv4", Val.IPv4Addr.u);
            prm.strValue = buf;
            CmdParameterll.push_back(prm);
        }
        break;

        case 'b': // ignore
        case 'D': // ignore
            break;

        default:
            rc = RTGetOptPrintError(rc, &Val);
            RTPrintf("Use --help for more information.\n");
            return rc;
    }

    return VINF_SUCCESS;
}

int VBoxNetDhcp::init()
{
    int rc = this->VBoxNetBaseService::init();
    AssertRCReturn(rc, rc);

    NetworkManager *netManager = NetworkManager::getNetworkManager();

    netManager->setOurAddress(m_Ipv4Address);
    netManager->setOurNetmask(m_Ipv4Netmask);
    netManager->setOurMac(m_MacAddress);
    
    if (isMainNeeded())
        rc = initWithMain();
    else
        rc = initNoMain();

    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}

/**
 * Runs the DHCP server.
 *
 * @returns exit code + error message to stderr on failure, won't return on
 *          success (you must kill this process).
 */
int VBoxNetDhcp::run(void)
{

    /* XXX: shortcut should be hidden from network manager */
    NetworkManager *netManager = NetworkManager::getNetworkManager();
    netManager->setSession(m_pSession);
    netManager->setInterface(m_hIf);
    netManager->setRingBuffer(m_pIfBuf);

    /*
     * The loop.
     */
    PINTNETRINGBUF  pRingBuf = &m_pIfBuf->Recv;
    for (;;)
    {
        /*
         * Wait for a packet to become available.
         */
        INTNETIFWAITREQ WaitReq;
        WaitReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
        WaitReq.Hdr.cbReq = sizeof(WaitReq);
        WaitReq.pSession = m_pSession;
        WaitReq.hIf = m_hIf;
        WaitReq.cMillies = 2000; /* 2 secs - the sleep is for some reason uninterruptible... */  /** @todo fix interruptability in SrvIntNet! */
        int rc = SUPR3CallVMMR0Ex(NIL_RTR0PTR, NIL_VMCPUID, VMMR0_DO_INTNET_IF_WAIT, 0, &WaitReq.Hdr);
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_TIMEOUT || rc == VERR_INTERRUPTED)
                continue;
            RTStrmPrintf(g_pStdErr, "VBoxNetDHCP: VMMR0_DO_INTNET_IF_WAIT returned %Rrc\n", rc);
            return 1;
        }

        /*
         * Process the receive buffer.
         */
        while (IntNetRingHasMoreToRead(pRingBuf))
        {
            size_t  cb;
            void   *pv = VBoxNetUDPMatch(m_pIfBuf, RTNETIPV4_PORT_BOOTPS, &m_MacAddress,
                                         VBOXNETUDP_MATCH_UNICAST | VBOXNETUDP_MATCH_BROADCAST | VBOXNETUDP_MATCH_CHECKSUM
                                         | (m_cVerbosity > 2 ? VBOXNETUDP_MATCH_PRINT_STDERR : 0),
                                         &m_CurHdrs, &cb);
            if (pv && cb)
            {
                PCRTNETBOOTP pDhcpMsg = (PCRTNETBOOTP)pv;
                m_pCurMsg  = pDhcpMsg;
                m_cbCurMsg = cb;

                uint8_t uMsgType;
                if (RTNetIPv4IsDHCPValid(NULL /* why is this here? */, pDhcpMsg, cb, &uMsgType))
                {
                    m_uCurMsgType = uMsgType;
                    handleDhcpMsg(uMsgType, pDhcpMsg, cb);
                    m_uCurMsgType = UINT8_MAX;
                }
                else
                    debugPrint(1, true, "VBoxNetDHCP: Skipping invalid DHCP packet.\n"); /** @todo handle pure bootp clients too? */

                m_pCurMsg = NULL;
                m_cbCurMsg = 0;
            }
            else if (VBoxNetArpHandleIt(m_pSession, m_hIf, m_pIfBuf, &m_MacAddress, m_Ipv4Address))
            {
                /* nothing */
            }

            /* Advance to the next frame. */
            IntNetRingSkipFrame(pRingBuf);
        }
    }

    return 0;
}


/**
 * Handles a DHCP message.
 *
 * @returns true if handled, false if not.
 * @param   uMsgType        The message type.
 * @param   pDhcpMsg        The DHCP message.
 * @param   cb              The size of the DHCP message.
 */
bool VBoxNetDhcp::handleDhcpMsg(uint8_t uMsgType, PCRTNETBOOTP pDhcpMsg, size_t cb)
{
    if (pDhcpMsg->bp_op == RTNETBOOTP_OP_REQUEST)
    {
        switch (uMsgType)
        {
            case RTNET_DHCP_MT_DISCOVER:
                return handleDhcpReqDiscover(pDhcpMsg, cb);

            case RTNET_DHCP_MT_REQUEST:
                return handleDhcpReqRequest(pDhcpMsg, cb);

            case RTNET_DHCP_MT_DECLINE:
                return handleDhcpReqDecline(pDhcpMsg, cb);

            case RTNET_DHCP_MT_RELEASE:
                return handleDhcpReqRelease(pDhcpMsg, cb);

            case RTNET_DHCP_MT_INFORM:
                debugPrint(0, true, "Should we handle this?");
                break;

            default:
                debugPrint(0, true, "Unexpected.");
                break;
        }
    }
    return false;
}


/**
 * The client is requesting an offer.
 *
 * @returns true.
 *
 * @param   pDhcpMsg    The message.
 * @param   cb          The message size.
 */
bool VBoxNetDhcp::handleDhcpReqDiscover(PCRTNETBOOTP pDhcpMsg, size_t cb)
{
    RawOption opt;
    memset(&opt, 0, sizeof(RawOption));
    /* 1. Find client */
    ConfigurationManager *confManager = ConfigurationManager::getConfigurationManager();
    Client client = confManager->getClientByDhcpPacket(pDhcpMsg, cb);

    /* 2. Find/Bind lease for client */
    Lease lease = confManager->allocateLease4Client(client, pDhcpMsg, cb);
    AssertReturn(lease != Lease::NullLease, VINF_SUCCESS);

    int rc = ConfigurationManager::extractRequestList(pDhcpMsg, cb, opt);

    /* 3. Send of offer */
    NetworkManager *networkManager = NetworkManager::getNetworkManager();

    lease.bindingPhase(true);
    lease.phaseStart(RTTimeMilliTS());
    lease.setExpiration(300); /* 3 min. */
    networkManager->offer4Client(client, pDhcpMsg->bp_xid, opt.au8RawOpt, opt.cbRawOpt);

    return VINF_SUCCESS;
}


/**
 * The client is requesting an offer.
 *
 * @returns true.
 *
 * @param   pDhcpMsg    The message.
 * @param   cb          The message size.
 */
bool VBoxNetDhcp::handleDhcpReqRequest(PCRTNETBOOTP pDhcpMsg, size_t cb)
{
    ConfigurationManager *confManager = ConfigurationManager::getConfigurationManager();
    NetworkManager *networkManager = NetworkManager::getNetworkManager();

    /* 1. find client */
    Client client = confManager->getClientByDhcpPacket(pDhcpMsg, cb);

    /* 2. find bound lease */
    Lease l = client.lease();
    if (l != Lease::NullLease)
    {

        if (l.isExpired())
        {
            /* send client to INIT state */
            Client c(client);
            networkManager->nak(client, pDhcpMsg->bp_xid);
            confManager->expireLease4Client(c);
            return true;
        }
        else {
            /* XXX: Validate request */
            RawOption opt;
            RT_ZERO(opt);

            Client c(client);
            int rc = confManager->commitLease4Client(c);
            AssertRCReturn(rc, false);

            rc = ConfigurationManager::extractRequestList(pDhcpMsg, cb, opt);
            AssertRCReturn(rc, false);

            networkManager->ack(client, pDhcpMsg->bp_xid, opt.au8RawOpt, opt.cbRawOpt);
        }
    }
    else
    {
        networkManager->nak(client, pDhcpMsg->bp_xid);
    }
    return true;
}


/**
 * The client is declining an offer we've made.
 *
 * @returns true.
 *
 * @param   pDhcpMsg    The message.
 * @param   cb          The message size.
 */
bool VBoxNetDhcp::handleDhcpReqDecline(PCRTNETBOOTP, size_t)
{
    /** @todo Probably need to match the server IP here to work correctly with
     *        other servers. */

    /*
     * The client is supposed to pass us option 50, requested address,
     * from the offer. We also match the lease state. Apparently the
     * MAC address is not supposed to be checked here.
     */

    /** @todo this is not required in the initial implementation, do it later. */
    debugPrint(1, true, "DECLINE is not implemented");
    return true;
}


/**
 * The client is releasing its lease - good boy.
 *
 * @returns true.
 *
 * @param   pDhcpMsg    The message.
 * @param   cb          The message size.
 */
bool VBoxNetDhcp::handleDhcpReqRelease(PCRTNETBOOTP, size_t)
{
    /** @todo Probably need to match the server IP here to work correctly with
     *        other servers. */

    /*
     * The client may pass us option 61, client identifier, which we should
     * use to find the lease by.
     *
     * We're matching MAC address and lease state as well.
     */

    /*
     * If no client identifier or if we couldn't find a lease by using it,
     * we will try look it up by the client IP address.
     */


    /*
     * If found, release it.
     */


    /** @todo this is not required in the initial implementation, do it later. */
    debugPrint(1, true, "RELEASE is not implemented");
    return true;
}


/**
 * Print debug message depending on the m_cVerbosity level.
 *
 * @param   iMinLevel       The minimum m_cVerbosity level for this message.
 * @param   fMsg            Whether to dump parts for the current DHCP message.
 * @param   pszFmt          The message format string.
 * @param   va              Optional arguments.
 */
void VBoxNetDhcp::debugPrintV(int iMinLevel, bool fMsg, const char *pszFmt, va_list va) const
{
    if (iMinLevel <= m_cVerbosity)
    {
        va_list vaCopy;                 /* This dude is *very* special, thus the copy. */
        va_copy(vaCopy, va);
        RTStrmPrintf(g_pStdErr, "VBoxNetDHCP: %s: %N\n", iMinLevel >= 2 ? "debug" : "info", pszFmt, &vaCopy);
        va_end(vaCopy);

        if (    fMsg
            &&  m_cVerbosity >= 2
            &&  m_pCurMsg)
        {
            /* XXX: export this to debugPrinfDhcpMsg or variant and other method export
             *  to base class
             */
            const char *pszMsg = m_uCurMsgType != UINT8_MAX ? debugDhcpName(m_uCurMsgType) : "";
            RTStrmPrintf(g_pStdErr, "VBoxNetDHCP: debug: %8s chaddr=%.6Rhxs ciaddr=%d.%d.%d.%d yiaddr=%d.%d.%d.%d siaddr=%d.%d.%d.%d xid=%#x\n",
                         pszMsg,
                         &m_pCurMsg->bp_chaddr,
                         m_pCurMsg->bp_ciaddr.au8[0], m_pCurMsg->bp_ciaddr.au8[1], m_pCurMsg->bp_ciaddr.au8[2], m_pCurMsg->bp_ciaddr.au8[3],
                         m_pCurMsg->bp_yiaddr.au8[0], m_pCurMsg->bp_yiaddr.au8[1], m_pCurMsg->bp_yiaddr.au8[2], m_pCurMsg->bp_yiaddr.au8[3],
                         m_pCurMsg->bp_siaddr.au8[0], m_pCurMsg->bp_siaddr.au8[1], m_pCurMsg->bp_siaddr.au8[2], m_pCurMsg->bp_siaddr.au8[3],
                         m_pCurMsg->bp_xid);
        }
    }
}


/**
 * Gets the name of given DHCP message type.
 *
 * @returns Readonly name.
 * @param   uMsgType        The message number.
 */
/* static */ const char *VBoxNetDhcp::debugDhcpName(uint8_t uMsgType)
{
    switch (uMsgType)
    {
        case 0:                         return "MT_00";
        case RTNET_DHCP_MT_DISCOVER:    return "DISCOVER";
        case RTNET_DHCP_MT_OFFER:       return "OFFER";
        case RTNET_DHCP_MT_REQUEST:     return "REQUEST";
        case RTNET_DHCP_MT_DECLINE:     return "DECLINE";
        case RTNET_DHCP_MT_ACK:         return "ACK";
        case RTNET_DHCP_MT_NAC:         return "NAC";
        case RTNET_DHCP_MT_RELEASE:     return "RELEASE";
        case RTNET_DHCP_MT_INFORM:      return "INFORM";
        case 9:                         return "MT_09";
        case 10:                        return "MT_0a";
        case 11:                        return "MT_0b";
        case 12:                        return "MT_0c";
        case 13:                        return "MT_0d";
        case 14:                        return "MT_0e";
        case 15:                        return "MT_0f";
        case 16:                        return "MT_10";
        case 17:                        return "MT_11";
        case 18:                        return "MT_12";
        case 19:                        return "MT_13";
        case UINT8_MAX:                 return "MT_ff";
        default:                        return "UNKNOWN";
    }
}


int VBoxNetDhcp::initNoMain()
{
    CmdParameterIterator it;

    RTNETADDRIPV4 networkId;
    networkId.u = m_Ipv4Address.u & m_Ipv4Netmask.u;
    RTNETADDRIPV4 netmask = m_Ipv4Netmask;

    RTNETADDRIPV4 UpperAddress;
    RTNETADDRIPV4 LowerAddress = networkId;
    UpperAddress.u = RT_H2N_U32(RT_N2H_U32(LowerAddress.u) | RT_N2H_U32(netmask.u));

    for (it = CmdParameterll.begin(); it != CmdParameterll.end(); ++it)
    {
        switch(it->Key)
        {
            case 'l':
                RTNetStrToIPv4Addr(it->strValue.c_str(), &LowerAddress);
                break;

            case 'u':
                RTNetStrToIPv4Addr(it->strValue.c_str(), &UpperAddress);
                break;
            case 'b':
                break;
                    
        }
    }
        
    ConfigurationManager *confManager = ConfigurationManager::getConfigurationManager();
    AssertPtrReturn(confManager, VERR_INTERNAL_ERROR);
    confManager->addNetwork(unconst(g_RootConfig),
                            networkId,
                            m_Ipv4Netmask,
                            LowerAddress,
                            UpperAddress);

    return VINF_SUCCESS;
}


int VBoxNetDhcp::initWithMain()
{
    /* ok, here we should initiate instance of dhcp server
     * and listener for Dhcp configuration events
     */
    AssertRCReturn(virtualbox.isNull(), VERR_INTERNAL_ERROR);

    HRESULT hrc = virtualbox->FindDHCPServerByNetworkName(com::Bstr(m_Network.c_str()).raw(),
                                                  m_DhcpServer.asOutParam());
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);

    hrc = virtualbox->FindNATNetworkByName(com::Bstr(m_Network.c_str()).raw(),
                                           m_NATNetwork.asOutParam());

    BOOL fNeedDhcpServer = false;
    if (FAILED(m_NATNetwork->COMGETTER(NeedDhcpServer)(&fNeedDhcpServer)))
        return VERR_INTERNAL_ERROR;

    if (!fNeedDhcpServer)
        return VERR_CANCELLED;

    RTNETADDRIPV4 gateway;
    com::Bstr strGateway;

    hrc = m_NATNetwork->COMGETTER(Gateway)(strGateway.asOutParam());
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);
    RTNetStrToIPv4Addr(com::Utf8Str(strGateway).c_str(), &gateway);

    ConfigurationManager *confManager = ConfigurationManager::getConfigurationManager();
    AssertPtrReturn(confManager, VERR_INTERNAL_ERROR);
    confManager->addToAddressList(RTNET_DHCP_OPT_ROUTERS, gateway);

    unsigned int i;
    unsigned int count_strs;
    com::SafeArray<BSTR> strs;
    std::map<RTNETADDRIPV4, uint32_t> MapIp4Addr2Off;

    hrc = m_NATNetwork->COMGETTER(LocalMappings)(ComSafeArrayAsOutParam(strs));
    if (   SUCCEEDED(hrc)
           && (count_strs = strs.size()))
    {
        for (i = 0; i < count_strs; ++i)
        {
            char szAddr[17];
            RTNETADDRIPV4 ip4addr;
            char *pszTerm;
            uint32_t u32Off;
            com::Utf8Str strLo2Off(strs[i]);
            const char *pszLo2Off = strLo2Off.c_str();

            RT_ZERO(szAddr);

            pszTerm = RTStrStr(pszLo2Off, "=");

            if (   pszTerm
                   && (pszTerm - pszLo2Off) <= INET_ADDRSTRLEN)
            {
                memcpy(szAddr, pszLo2Off, (pszTerm - pszLo2Off));
                int rc = RTNetStrToIPv4Addr(szAddr, &ip4addr);
                if (RT_SUCCESS(rc))
                {
                    u32Off = RTStrToUInt32(pszTerm + 1);
                    if (u32Off != 0)
                        MapIp4Addr2Off.insert(
                          std::map<RTNETADDRIPV4,uint32_t>::value_type(ip4addr, u32Off));
                }
            }
        }
    }

    strs.setNull();
    ComPtr<IHost> host;
    if (SUCCEEDED(virtualbox->COMGETTER(Host)(host.asOutParam())))
    {
        if (SUCCEEDED(host->COMGETTER(NameServers)(ComSafeArrayAsOutParam(strs))))
        {
            RTNETADDRIPV4 addr;

            confManager->flushAddressList(RTNET_DHCP_OPT_DNS);
            int rc;
            for (i = 0; i < strs.size(); ++i)
            {
                rc = RTNetStrToIPv4Addr(com::Utf8Str(strs[i]).c_str(), &addr);
                if (RT_SUCCESS(rc))
                {
                    if (addr.au8[0] == 127)
                    {
                        if (MapIp4Addr2Off[addr] != 0)
                        {
                            addr.u = RT_H2N_U32(RT_N2H_U32(m_Ipv4Address.u & m_Ipv4Netmask.u)
                                                + MapIp4Addr2Off[addr]);
                        }
                        else
                            continue;
                    }

                    confManager->addToAddressList(RTNET_DHCP_OPT_DNS, addr);
                }
            }
        }

        strs.setNull();
#if 0
        if (SUCCEEDED(host->COMGETTER(SearchStrings)(ComSafeArrayAsOutParam(strs)))) 
        {
            /* XXX: todo. */;
        }
        strs.setNull();
#endif
        com::Bstr domain;
        if (SUCCEEDED(host->COMGETTER(DomainName)(domain.asOutParam())))
            confManager->setString(RTNET_DHCP_OPT_DOMAIN_NAME, std::string(com::Utf8Str(domain).c_str()));
    }

    com::Bstr strUpperIp, strLowerIp;
    
    RTNETADDRIPV4 LowerAddress;
    RTNETADDRIPV4 UpperAddress;

    hrc = m_DhcpServer->COMGETTER(UpperIP)(strUpperIp.asOutParam());
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);
    RTNetStrToIPv4Addr(com::Utf8Str(strUpperIp).c_str(), &UpperAddress);


    hrc = m_DhcpServer->COMGETTER(LowerIP)(strLowerIp.asOutParam());
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);
    RTNetStrToIPv4Addr(com::Utf8Str(strLowerIp).c_str(), &LowerAddress);

    RTNETADDRIPV4 networkId;
    networkId.u = m_Ipv4Address.u & m_Ipv4Netmask.u;
    std::string name = std::string("default");

    confManager->addNetwork(unconst(g_RootConfig),
                            networkId,
                            m_Ipv4Netmask,
                            LowerAddress,
                            UpperAddress);

    com::Bstr bstr;
    hrc = virtualbox->COMGETTER(HomeFolder)(bstr.asOutParam());
    std::string strXmlLeaseFile(com::Utf8StrFmt("%ls%c%s.leases",
                                                bstr.raw(), RTPATH_DELIMITER, m_Network.c_str()).c_str());
    confManager->loadFromFile(strXmlLeaseFile);

    return VINF_SUCCESS;
}

/**
 *  Entry point.
 */
extern "C" DECLEXPORT(int) TrustedMain(int argc, char **argv)
{
    /*
     * Instantiate the DHCP server and hand it the options.
     */

    VBoxNetDhcp *pDhcp = new VBoxNetDhcp();
    if (!pDhcp)
    {
        RTStrmPrintf(g_pStdErr, "VBoxNetDHCP: new VBoxNetDhcp failed!\n");
        return 1;
    }
    int rc = pDhcp->parseArgs(argc - 1, argv + 1);
    if (rc)
        return rc;

    pDhcp->init();

    /*
     * Try connect the server to the network.
     */
    rc = pDhcp->tryGoOnline();
    if (RT_FAILURE(rc))
    {
        delete pDhcp;
        return 1;
    }

    /*
     * Process requests.
     */
    g_pDhcp = pDhcp;
    rc = pDhcp->run();
    g_pDhcp = NULL;
    delete pDhcp;

    return 0;
}


#ifndef VBOX_WITH_HARDENING

int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, RTR3INIT_FLAGS_SUPLIB);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    return TrustedMain(argc, argv);
}

# ifdef RT_OS_WINDOWS

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

static LPCWSTR g_WndClassName = L"VBoxNetDHCPClass";

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

    return main(__argc, __argv);
}
# endif /* RT_OS_WINDOWS */

#endif /* !VBOX_WITH_HARDENING */

