/* $Id$ */
/** @file
 * VBoxNetDHCP - DHCP Service for connecting to IntNet.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

/** @page pg_net_dhcp       VBoxNetDHCP
 *
 * Write a few words...
 *
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/initterm.h>
#include <iprt/net.h>
#include <iprt/err.h>
#include <iprt/time.h>
#include <iprt/stream.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/getopt.h>

#include <VBox/sup.h>
#include <VBox/intnet.h>
#include <VBox/vmm.h>
#include <VBox/version.h>

#include "../UDPLib/VBoxNetUDP.h"

#include <vector>
#include <string>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * DHCP configuration item.
 *
 * This is all public data because I'm too lazy to do it propertly right now.
 */
class VBoxNetDhcpCfg
{
public:
    /** The etheret addresses this matches config applies to.
     * An empty vector means 'ANY'. */
    std::vector<RTMAC>          m_MacAddresses;
    /** The upper address in the range. */
    RTNETADDRIPV4               m_UpperAddr;
    /** The lower address in the range. */
    RTNETADDRIPV4               m_LowerAddr;

    /** Option 1: The net mask. */
    RTNETADDRIPV4               m_SubnetMask;
    /* * Option 2: The time offset. */
    /** Option 3: Routers for the subnet. */
    std::vector<RTNETADDRIPV4>  m_Routers;
    /* * Option 4: Time server. */
    /* * Option 5: Name server. */
    /** Option 6: Domain Name Server (DNS) */
    std::vector<RTNETADDRIPV4>  m_DNSes;
    /* * Option 7: Log server. */
    /* * Option 8: Cookie server. */
    /* * Option 9: LPR server. */
    /* * Option 10: Impress server. */
    /* * Option 11: Resource location server. */
    /* * Option 12: Host name. */
    //std::string<char>           m_HostName;
    /* * Option 13: Boot file size option. */
    /* * Option 14: Merit dump file. */
    /** Option 15: Domain name. */
    std::string                m_DomainName;
    /* * Option 16: Swap server. */
    /* * Option 17: Root path. */
    /* * Option 18: Extension path. */
    /* * Option 19: IP forwarding enable/disable. */
    /* * Option 20: Non-local routing enable/disable. */
    /* * Option 21: Policy filter. */
    /* * Option 22: Maximum datagram reassembly size (MRS). */
    /* * Option 23: Default IP time-to-live. */
    /* * Option 24: Path MTU aging timeout. */
    /* * Option 25: Path MTU plateau table. */
    /* * Option 26: Interface MTU. */
    /* * Option 27: All subnets are local. */
    /* * Option 28: Broadcast address. */
    /* * Option 29: Perform maximum discovery. */
    /* * Option 30: Mask supplier. */
    /* * Option 31: Perform route discovery. */
    /* * Option 32: Router solicitation address. */
    /* * Option 33: Static route. */
    /* * Option 34: Trailer encapsulation. */
    /* * Option 35: ARP cache timeout. */
    /* * Option 36: Ethernet encapsulation. */
    /* * Option 37: TCP Default TTL. */
    /* * Option 38: TCP Keepalive Interval. */
    /* * Option 39: TCP Keepalive Garbage. */
    /* * Option 40: Network Information Service (NIS) Domain. */
    /* * Option 41: Network Information Servers. */
    /* * Option 42: Network Time Protocol Servers. */
    /* * Option 43: Vendor Specific Information. */
    /* * Option 44: NetBIOS over TCP/IP Name Server (NBNS). */
    /* * Option 45: NetBIOS over TCP/IP Datagram distribution Server (NBDD). */
    /* * Option 46: NetBIOS over TCP/IP Node Type. */
    /* * Option 47: NetBIOS over TCP/IP Scope. */
    /* * Option 48: X Window System Font Server. */
    /* * Option 49: X Window System Display Manager. */

    /** Option 51: IP Address Lease Time. */
    uint32_t                    m_cSecLease;

    /* * Option 64: Network Information Service+ Domain. */
    /* * Option 65: Network Information Service+ Servers. */
    /** Option 66: TFTP server name. */
    std::string                 m_TftpServer;
    /** Option 67: Bootfile name. */
    std::string                 m_BootfileName;

    /* * Option 68: Mobile IP Home Agent. */
    /* * Option 69: Simple Mail Transport Protocol (SMPT) Server. */
    /* * Option 70: Post Office Protocol (POP3) Server. */
    /* * Option 71: Network News Transport Protocol (NNTP) Server. */
    /* * Option 72: Default World Wide Web (WWW) Server. */
    /* * Option 73: Default Finger Server. */
    /* * Option 74: Default Internet Relay Chat (IRC) Server. */
    /* * Option 75: StreetTalk Server. */

    /* * Option 119: Domain Search. */


    VBoxNetDhcpCfg()
    {
        m_UpperAddr.u = UINT32_MAX;
        m_LowerAddr.u = UINT32_MAX;
        m_SubnetMask.u = UINT32_MAX;
        m_cSecLease = 60*60; /* 1 hour */
    }

    /** Validates the configuration.
     * @returns 0 on success, exit code + error message to stderr on failure. */
    int validate(void)
    {
        if (    m_UpperAddr.u == UINT32_MAX
            ||  m_LowerAddr.u == UINT32_MAX
            ||  m_SubnetMask.u == UINT32_MAX)
        {
            RTStrmPrintf(g_pStdErr, "VBoxNetDHCP: Config is missing:");
            if (m_UpperAddr.u == UINT32_MAX)
                RTStrmPrintf(g_pStdErr, " --upper-ip");
            if (m_LowerAddr.u == UINT32_MAX)
                RTStrmPrintf(g_pStdErr, " --lower-ip");
            if (m_SubnetMask.u == UINT32_MAX)
                RTStrmPrintf(g_pStdErr, " --netmask");
            return 2;
        }
        return 0;
    }

};

/**
 * DHCP lease.
 */
class VBoxNetDhcpLease
{
public:
    /** The client MAC address. */
    RTMAC           m_MacAddress;
    /** The lease expiration time. */
    RTTIMESPEC      m_ExpireTime;
};

/**
 * DHCP server instance.
 */
class VBoxNetDhcp
{
public:
    VBoxNetDhcp();
    virtual ~VBoxNetDhcp();

    int                 parseArgs(int argc, char **argv);
    int                 tryGoOnline(void);
    int                 run(void);

protected:
    int                 addConfig(VBoxNetDhcpCfg *pCfg);
    bool                handleDhcpMsg(uint8_t uMsgType, PCRTNETBOOTP pDhcpMsg, size_t cb);
    bool                handleDhcpReqDiscover(PCRTNETBOOTP pDhcpMsg, size_t cb);
    bool                handleDhcpReqRequest(PCRTNETBOOTP pDhcpMsg, size_t cb);
    bool                handleDhcpReqDecline(PCRTNETBOOTP pDhcpMsg, size_t cb);
    bool                handleDhcpReqRelease(PCRTNETBOOTP pDhcpMsg, size_t cb);
    void                handleDhcpReply(uint8_t uMsgType, VBoxNetDhcpLease *pLease, PCRTNETBOOTP pDhcpMsg, size_t cb);

    inline void         debugPrint( int32_t iMinLevel, bool fMsg,  const char *pszFmt, ...) const;
    void                debugPrintV(int32_t iMinLevel, bool fMsg,  const char *pszFmt, va_list va) const;
    static const char  *debugDhcpName(uint8_t uMsgType);

protected:
    /** @name The server configuration data members.
     * @{ */
    std::string         m_Name;
    std::string         m_Network;
    RTMAC               m_MacAddress;
    RTNETADDRIPV4       m_IpAddress;
    /** @} */

    /** The current configs. */
    std::vector<VBoxNetDhcpCfg> m_Cfgs;

    /** The current leases. */
    std::vector<VBoxNetDhcpLease> m_Leases;

    /** @name The network interface
     * @{ */
    PSUPDRVSESSION      m_pSession;
    uint32_t            m_cbSendBuf;
    uint32_t            m_cbRecvBuf;
    INTNETIFHANDLE      m_hIf;          /**< The handle to the network interface. */
    PINTNETBUF          m_pIfBuf;       /**< Interface buffer. */
    /** @} */

    /** @name Debug stuff
     * @{  */
    int32_t             m_cVerbosity;
    uint8_t             m_uCurMsgType;
    uint16_t            m_cbCurMsg;
    PCRTNETBOOTP        m_pCurMsg;
    VBOXNETUDPHDRS      m_CurHdrs;
    /** @} */
};


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Pointer to the DHCP server. */
static VBoxNetDhcp *g_pDhcp;



/**
 * Construct a DHCP server with a default configuration.
 */
VBoxNetDhcp::VBoxNetDhcp()
{
    m_Name                  = "VBoxNetDhcp";
    m_Network               = "VBoxNetDhcp";
    m_MacAddress.au8[0]     = 0x08;
    m_MacAddress.au8[1]     = 0x00;
    m_MacAddress.au8[2]     = 0x27;
    m_MacAddress.au8[3]     = 0x40;
    m_MacAddress.au8[4]     = 0x41;
    m_MacAddress.au8[5]     = 0x42;
    m_IpAddress.u           = RT_H2N_U32_C(RT_BSWAP_U32_C(RT_MAKE_U32_FROM_U8( 10,  0,  2,  5)));

    m_pSession              = NULL;
    m_cbSendBuf             =  8192;
    m_cbRecvBuf             = 51200; /** @todo tune to 64 KB with help from SrvIntR0 */
    m_hIf                   = INTNET_HANDLE_INVALID;
    m_pIfBuf                = NULL;

    m_cVerbosity            = 0;
    m_uCurMsgType           = UINT8_MAX;
    m_cbCurMsg              = 0;
    m_pCurMsg               = NULL;
    memset(&m_CurHdrs, '\0', sizeof(m_CurHdrs));

#if 1 /* while hacking. */
    VBoxNetDhcpCfg DefCfg;
    DefCfg.m_LowerAddr.u    = RT_H2N_U32_C(RT_BSWAP_U32_C(RT_MAKE_U32_FROM_U8( 10,  0,  2,100)));
    DefCfg.m_UpperAddr.u    = RT_H2N_U32_C(RT_BSWAP_U32_C(RT_MAKE_U32_FROM_U8( 10,  0,  2,250)));
    DefCfg.m_SubnetMask.u   = RT_H2N_U32_C(RT_BSWAP_U32_C(RT_MAKE_U32_FROM_U8(255,255,255,  0)));
    RTNETADDRIPV4 Addr;
    Addr.u                  = RT_H2N_U32_C(RT_BSWAP_U32_C(RT_MAKE_U32_FROM_U8( 10,  0,  2,  1)));
    DefCfg.m_Routers.push_back(Addr);
    Addr.u                  = RT_H2N_U32_C(RT_BSWAP_U32_C(RT_MAKE_U32_FROM_U8( 10,  0,  2,  2)));
    DefCfg.m_DNSes.push_back(Addr);
    DefCfg.m_DomainName     = "vboxnetdhcp.org";
    DefCfg.m_cSecLease      = 60*60; /* 1 hour */
    DefCfg.m_TftpServer     = "10.0.2.3"; //??
#endif
}


/**
 * Destruct a DHCP server.
 */
VBoxNetDhcp::~VBoxNetDhcp()
{
    /*
     * Close the interface connection.
     */
    if (m_hIf != INTNET_HANDLE_INVALID)
    {
        INTNETIFCLOSEREQ CloseReq;
        CloseReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
        CloseReq.Hdr.cbReq = sizeof(CloseReq);
        CloseReq.pSession = m_pSession;
        CloseReq.hIf = m_hIf;
        m_hIf = INTNET_HANDLE_INVALID;
        int rc = SUPCallVMMR0Ex(NIL_RTR0PTR, VMMR0_DO_INTNET_IF_CLOSE, 0, &CloseReq.Hdr);
        AssertRC(rc);
    }

    if (m_pSession)
    {
        SUPTerm(false /* not forced */);
        m_pSession = NULL;
    }
}


/**
 * Adds a config to the tail.
 *
 * @returns See VBoxNetDHCP::validate().
 * @param   pCfg        The config too add.
 *                      This object will be consumed by this call!
 */
int VBoxNetDhcp::addConfig(VBoxNetDhcpCfg *pCfg)
{
    int rc = 0;
    if (pCfg)
    {
        rc = pCfg->validate();
        if (!rc)
            m_Cfgs.push_back(*pCfg);
        delete pCfg;
    }
    return rc;
}


/**
 * Parse the arguments.
 *
 * @returns 0 on success, fully bitched exit code on failure.
 *
 * @param   argc    Argument count.
 * @param   argv    Argument vector.
 */
int VBoxNetDhcp::parseArgs(int argc, char **argv)
{
    static const RTGETOPTDEF s_aOptionDefs[] =
    {
        { "--name",           'N',   RTGETOPT_REQ_STRING },
        { "--network",        'n',   RTGETOPT_REQ_STRING },
        { "--mac-address",    'a',   RTGETOPT_REQ_MACADDR },
        { "--ip-address",     'i',   RTGETOPT_REQ_IPV4ADDR },
        { "--verbose",        'v',   RTGETOPT_REQ_NOTHING },

        { "--begin-config",   'b',   RTGETOPT_REQ_NOTHING },
        { "--gateway",        'g',   RTGETOPT_REQ_IPV4ADDR },
        { "--lower-ip",       'l',   RTGETOPT_REQ_IPV4ADDR },
        { "--upper-ip",       'u',   RTGETOPT_REQ_IPV4ADDR },
        { "--netmask",        'm',   RTGETOPT_REQ_IPV4ADDR },

        { "--help",           'h',   RTGETOPT_REQ_NOTHING },
        { "--version ",       'V',   RTGETOPT_REQ_NOTHING },
    };

    RTGETOPTSTATE State;
    int rc = RTGetOptInit(&State, argc, argv, &s_aOptionDefs[0], RT_ELEMENTS(s_aOptionDefs), 0, 0);
    AssertRCReturn(rc, 49);

    VBoxNetDhcpCfg *pCurCfg = NULL;
    for (;;)
    {
        RTGETOPTUNION Val;
        rc = RTGetOpt(&State, &Val);
        if (!rc)
            break;
        switch (rc)
        {
            case 'N':
                m_Name = Val.psz;
                break;
            case 'n':
                m_Network = Val.psz;
                break;
            case 'a':
                m_MacAddress = Val.MacAddr;
                break;
            case 'i':
                m_IpAddress = Val.IPv4Addr;
                break;

            case 'v':
                m_cVerbosity++;
                break;

            /* Begin config. */
            case 'b':
                rc = addConfig(pCurCfg);
                if (rc)
                    break;
                pCurCfg = NULL;
                /* fall thru */

            /* config specific ones. */
            case 'g':
            case 'l':
            case 'u':
            case 'm':
                if (!pCurCfg)
                {
                    pCurCfg = new VBoxNetDhcpCfg();
                    if (!pCurCfg)
                    {
                        RTStrmPrintf(g_pStdErr, "VBoxNetDHCP: new VBoxDhcpCfg failed\n");
                        rc = 1;
                        break;
                    }
                }

                switch (rc)
                {
                    case 'g':
                        pCurCfg->m_Routers.push_back(Val.IPv4Addr);
                        break;

                    case 'l':
                        pCurCfg->m_LowerAddr = Val.IPv4Addr;
                        break;

                    case 'u':
                        pCurCfg->m_UpperAddr = Val.IPv4Addr;
                        break;

                    case 'm':
                        pCurCfg->m_SubnetMask = Val.IPv4Addr;
                        break;

                    case 0: /* ignore */ break;
                    default:
                        AssertMsgFailed(("%d", rc));
                        rc = 1;
                        break;
                }
                break;

            case 'V':
                RTPrintf("%sr%d\n", VBOX_VERSION_STRING, VBOX_SVN_REV);
                rc = 0;
                break;

            case 'h':
                RTPrintf("VBoxNetDHCP Version %s\n"
                         "(C) 2009 Sun Microsystems, Inc.\n"
                         "All rights reserved\n"
                         "\n"
                         "Usage:\n"
                         "  TODO\n",
                         VBOX_VERSION_STRING);
                rc = 1;
                break;

            default:
                break;
        }
    }

    return rc;
}


/**
 * Tries to connect to the internal network.
 *
 * @returns 0 on success, exit code + error message to stderr on failure.
 */
int VBoxNetDhcp::tryGoOnline(void)
{
    /*
     * Open the session, load ring-0 and issue the request.
     */
    int rc = SUPR3Init(&m_pSession);
    if (RT_FAILURE(rc))
    {
        m_pSession = NULL;
        RTStrmPrintf(g_pStdErr, "VBoxNetDHCP: SUPR3Init -> %Rrc", rc);
        return 1;
    }

    char szPath[RTPATH_MAX];
    rc = RTPathProgram(szPath, sizeof(szPath) - sizeof("/VMMR0.r0"));
    if (RT_FAILURE(rc))
    {
        RTStrmPrintf(g_pStdErr, "VBoxNetDHCP: RTPathProgram -> %Rrc", rc);
        return 1;
    }

    rc = SUPLoadVMM(strcat(szPath, "/VMMR0.r0"));
    if (RT_FAILURE(rc))
    {
        RTStrmPrintf(g_pStdErr, "VBoxNetDHCP: SUPLoadVMM(\"%s\") -> %Rrc", szPath, rc);
        return 1;
    }

    /*
     * Create the open request.
     */
    INTNETOPENREQ OpenReq;
    OpenReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    OpenReq.Hdr.cbReq = sizeof(OpenReq);
    OpenReq.pSession = m_pSession;
    strncpy(OpenReq.szNetwork, m_Network.c_str(), sizeof(OpenReq.szNetwork));
    OpenReq.szTrunk[0] = '\0';
    OpenReq.enmTrunkType = kIntNetTrunkType_WhateverNone;
    OpenReq.fFlags = 0; /** @todo check this */
    OpenReq.cbSend = m_cbSendBuf;
    OpenReq.cbRecv = m_cbRecvBuf;
    OpenReq.hIf = INTNET_HANDLE_INVALID;

    /*
     * Issue the request.
     */
    debugPrint(2, false, "attempting to open/create network \"%s\"...", OpenReq.szNetwork);
    rc = SUPCallVMMR0Ex(NIL_RTR0PTR, VMMR0_DO_INTNET_OPEN, 0, &OpenReq.Hdr);
    if (RT_SUCCESS(rc))
    {
        m_hIf = OpenReq.hIf;
        debugPrint(1, false, "successfully opened/created \"%s\" - hIf=%#x", OpenReq.szNetwork, m_hIf);

        /*
         * Get the ring-3 address of the shared interface buffer.
         */
        INTNETIFGETRING3BUFFERREQ GetRing3BufferReq;
        GetRing3BufferReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
        GetRing3BufferReq.Hdr.cbReq = sizeof(GetRing3BufferReq);
        GetRing3BufferReq.pSession = m_pSession;
        GetRing3BufferReq.hIf = m_hIf;
        GetRing3BufferReq.pRing3Buf = NULL;
        rc = SUPCallVMMR0Ex(NIL_RTR0PTR, VMMR0_DO_INTNET_IF_GET_RING3_BUFFER, 0, &GetRing3BufferReq.Hdr);
        if (RT_SUCCESS(rc))
        {
            PINTNETBUF pBuf = GetRing3BufferReq.pRing3Buf;
            debugPrint(1, false, "pBuf=%p cbBuf=%d cbSend=%d cbRecv=%d",
                       pBuf, pBuf->cbBuf, pBuf->cbSend, pBuf->cbRecv);
            m_pIfBuf = pBuf;

            /*
             * Activate the interface.
             */
            INTNETIFSETACTIVEREQ ActiveReq;
            ActiveReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
            ActiveReq.Hdr.cbReq = sizeof(ActiveReq);
            ActiveReq.pSession = m_pSession;
            ActiveReq.hIf = m_hIf;
            ActiveReq.fActive = true;
            rc = SUPCallVMMR0Ex(NIL_RTR0PTR, VMMR0_DO_INTNET_IF_SET_ACTIVE, 0, &ActiveReq.Hdr);
            if (RT_SUCCESS(rc))
                return 0;

            /* bail out */
            RTStrmPrintf(g_pStdErr, "VBoxNetDHCP: SUPCallVMMR0Ex(,VMMR0_DO_INTNET_IF_SET_PROMISCUOUS_MODE,) failed, rc=%Rrc\n", rc);
        }
        else
            RTStrmPrintf(g_pStdErr, "VBoxNetDHCP: SUPCallVMMR0Ex(,VMMR0_DO_INTNET_IF_GET_RING3_BUFFER,) failed, rc=%Rrc\n", rc);
    }
    else
        RTStrmPrintf(g_pStdErr, "VBoxNetDHCP: SUPCallVMMR0Ex(,VMMR0_DO_INTNET_OPEN,) failed, rc=%Rrc\n", rc);

    return RT_SUCCESS(rc) ? 0 : 1;
}


/**
 * Runs the DHCP server.
 *
 * @returns exit code + error message to stderr on failure, won't return on
 *          success (you must kill this process).
 */
int VBoxNetDhcp::run(void)
{
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
        int rc = SUPCallVMMR0Ex(NIL_RTR0PTR, VMMR0_DO_INTNET_IF_WAIT, 0, &WaitReq.Hdr);
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_TIMEOUT)
                continue;
            RTStrmPrintf(g_pStdErr, "VBoxNetDHCP: VMMR0_DO_INTNET_IF_WAIT returned %Rrc\n", rc);
            return 1;
        }

        /*
         * Process the receive buffer.
         */
        while (INTNETRingGetReadable(pRingBuf) > 0)
        {
            size_t  cb;
            void   *pv = VBoxNetUDPMatch(m_pIfBuf, 67 /* bootps */, &m_MacAddress,
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

            /* Advance to the next frame. */
            INTNETRingSkipFrame(m_pIfBuf, pRingBuf);
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
    /*
     * First, see if there is already a lease for this client. It may have rebooted,
     * crashed or whatever that have caused it to forget its existing lease.
     * If none was found, create a new lease for it and then construct a reply.
     */

    /** @todo this code IS required. */
    return true;
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
    /*
     * Windows will reissue these requests when rejoining a network if it thinks it
     * already has an address on the network. If we cannot find a valid lease,
     * make a new one.
     */
    /** @todo check how windows treats bp_xid here, it should match I think. If
     *        it doesn't we've no way of filtering out broadcast replies to other
     *        DHCP servers. */

    /** @todo this code IS required. */
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
bool VBoxNetDhcp::handleDhcpReqDecline(PCRTNETBOOTP pDhcpMsg, size_t cb)
{
    /*
     * The client is supposed to pass us option 50, requested address,
     * from the offer. We also match the lease state. Apparently the
     * MAC address is not supposed to be checked here.
     */

    /** @todo this is not required in the initial implementation, do it later. */
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
bool VBoxNetDhcp::handleDhcpReqRelease(PCRTNETBOOTP pDhcpMsg, size_t cb)
{
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
    return true;
}


/**
 * Constructs and sends a reply to a client.
 *
 * @returns
 * @param   uMsgType        The DHCP message type.
 * @param   pLease          The lease. This can be NULL for some replies.
 * @param   pDhcpMsg        The client message. We will dig out the MAC address,
 *                          transaction ID, and requested options from this.
 * @param   cb              The size of the client message.
 */
void VBoxNetDhcp::handleDhcpReply(uint8_t uMsgType, VBoxNetDhcpLease *pLease, PCRTNETBOOTP pDhcpMsg, size_t cb)
{
    /** @todo this is required. :-) */
}



/**
 * Print debug message depending on the m_cVerbosity level.
 *
 * @param   iMinLevel       The minimum m_cVerbosity level for this message.
 * @param   fMsg            Whether to dump parts for the current DHCP message.
 * @param   pszFmt          The message format string.
 * @param   ...             Optional arguments.
 */
inline void VBoxNetDhcp::debugPrint(int32_t iMinLevel, bool fMsg, const char *pszFmt, ...) const
{
    if (iMinLevel <= m_cVerbosity)
    {
        va_list va;
        va_start(va, pszFmt);
        debugPrintV(iMinLevel, fMsg, pszFmt, va);
        va_end(va);
    }
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
            const char *pszMsg = m_uCurMsgType != UINT8_MAX ? debugDhcpName(m_uCurMsgType) : "";
            RTStrmPrintf(g_pStdErr, "VBoxNetDHCP: debug: %8s chaddr=%.6Rhxs ciaddr=%d.%d.%d.%d yiaddr=%d.%d.%d.%d siaddr=%d.%d.%d.%d\n",
                         pszMsg,
                         &m_pCurMsg->bp_chaddr,
                         m_pCurMsg->bp_ciaddr.au8[0], m_pCurMsg->bp_ciaddr.au8[1], m_pCurMsg->bp_ciaddr.au8[2], m_pCurMsg->bp_ciaddr.au8[3],
                         m_pCurMsg->bp_yiaddr.au8[0], m_pCurMsg->bp_yiaddr.au8[1], m_pCurMsg->bp_yiaddr.au8[2], m_pCurMsg->bp_yiaddr.au8[3],
                         m_pCurMsg->bp_siaddr.au8[0], m_pCurMsg->bp_siaddr.au8[1], m_pCurMsg->bp_siaddr.au8[2], m_pCurMsg->bp_siaddr.au8[3]);
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



/**
 *  Entry point.
 */
extern "C" DECLEXPORT(int) TrustedMain(int argc, char **argv, char **envp)
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

    /*
     * Try connect the server to the network.
     */
    rc = pDhcp->tryGoOnline();
    if (rc)
    {
        delete pDhcp;
        return rc;
    }

    /*
     * Process requests.
     */
    g_pDhcp = pDhcp;
    rc = pDhcp->run();
    g_pDhcp = NULL;
    delete pDhcp;

    return rc;
}



#ifndef VBOX_WITH_HARDENING

int main(int argc, char **argv, char **envp)
{
    int rc = RTR3InitAndSUPLib();
    if (RT_FAILURE(rc))
    {
        RTStrmPrintf(g_pStdErr, "VBoxNetDHCP: RTR3InitAndSupLib failed, rc=%Rrc\n", rc);
        return 1;
    }

    return TrustedMain(argc, argv, envp);
}

#endif /* !VBOX_WITH_HARDENING */

