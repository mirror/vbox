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
#include <iprt/net.h>
#include <iprt/initterm.h>
#include <iprt/alloca.h>
#include <iprt/err.h>
#include <iprt/time.h>
#include <iprt/stream.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/getopt.h>
#include <iprt/string.h>

#include <VBox/sup.h>
#include <VBox/intnet.h>
#include <VBox/vmm.h>
#include <VBox/version.h>

#include "../NetLib/VBoxNetLib.h"

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
    std::string                 m_HostName;
    /* * Option 13: Boot file size option. */
    /* * Option 14: Merit dump file. */
    /** Option 15: Domain name. */
    std::string                 m_DomainName;
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
    /** Address for the bp_siaddr field corresponding to m_TftpServer. */
    RTNETADDRIPV4               m_TftpServerAddr;
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

        if (RT_N2H_U32(m_UpperAddr.u) < RT_N2H_U32(m_LowerAddr.u))
        {
            RTStrmPrintf(g_pStdErr, "VBoxNetDHCP: The --upper-ip value is lower than the --lower-ip one!\n"
                                    "             %d.%d.%d.%d < %d.%d.%d.%d\n",
                         m_UpperAddr.au8[0], m_UpperAddr.au8[1], m_UpperAddr.au8[2], m_UpperAddr.au8[3],
                         m_LowerAddr.au8[0], m_LowerAddr.au8[1], m_LowerAddr.au8[2], m_LowerAddr.au8[3]);
            return 3;
        }

        /* the code goes insane if we have too many atm. lazy bird */
        uint32_t cIPs = RT_N2H_U32(m_UpperAddr.u) - RT_N2H_U32(m_LowerAddr.u);
        if (cIPs > 1024)
        {
            RTStrmPrintf(g_pStdErr, "VBoxNetDHCP: Too many IPs between --upper-ip and --lower-ip! %d (max 1024)\n"
                                    "             %d.%d.%d.%d < %d.%d.%d.%d\n",
                         cIPs,
                         m_UpperAddr.au8[0], m_UpperAddr.au8[1], m_UpperAddr.au8[2], m_UpperAddr.au8[3],
                         m_LowerAddr.au8[0], m_LowerAddr.au8[1], m_LowerAddr.au8[2], m_LowerAddr.au8[3]);
            return 3;
        }
        return 0;
    }

    /**
     * Is this config for one specific client?
     *
     * @return  true / false.
     */
    bool            isOneSpecificClient(void) const
    {
        return m_LowerAddr.u == m_UpperAddr.u
            && m_MacAddresses.size() > 0;
    }

    /**
     * Checks if this config matches the specified MAC address.
     *
     * @returns true / false.
     *
     * @param   pMac    The MAC address to match.
     */
    bool            matchesMacAddress(PCRTMAC pMac) const
    {
        size_t i = m_MacAddresses.size();
        if (RT_LIKELY(i < 1))
            return true; /* no entries == ALL wildcard match */

        while (i--)
        {
            PCRTMAC pCur = &m_MacAddresses[i];
            if (    pCur->au16[0] == pMac->au16[0]
                &&  pCur->au16[1] == pMac->au16[1]
                &&  pCur->au16[2] == pMac->au16[2])
                return true;
        }
        return false;
    }

};

/**
 * DHCP lease.
 */
class VBoxNetDhcpLease
{
public:
    typedef enum State
    {
        /** Invalid. */
        kState_Invalid = 0,
        /** The lease is free / released. */
        kState_Free,
        /** An offer has been made.
         * Expire time indicates when the offer expires. */
        kState_Offer,
        /** The lease is active.
         * Expire time indicates when the lease expires. */
        kState_Active
    } State;

    /** The client MAC address. */
    RTMAC           m_MacAddress;
    /** The IPv4 address. */
    RTNETADDRIPV4   m_IPv4Address;

    /** The current lease state. */
    State           m_enmState;
    /** The lease expiration time. */
    RTTIMESPEC      m_ExpireTime;
    /** Transaction ID. */
    uint32_t        m_xid;
    /** The configuration for this lease. */
    VBoxNetDhcpCfg *m_pCfg;

public:
    /** Constructor taking an IPv4 address and a Config. */
    VBoxNetDhcpLease(RTNETADDRIPV4 IPv4Addr, VBoxNetDhcpCfg *pCfg)
    {
        m_pCfg          = pCfg;
        m_IPv4Address   = IPv4Addr;

        m_MacAddress.au16[0] = m_MacAddress.au16[1] =  m_MacAddress.au16[2] = 0xff;
        m_enmState      = kState_Free;
        RTTimeSpecSetSeconds(&m_ExpireTime, 0);
        m_xid           = UINT32_MAX;
    }

    /** Destructor.  */
    ~VBoxNetDhcpLease()
    {
        m_IPv4Address.u = UINT32_MAX;
        m_pCfg          = NULL;
        m_MacAddress.au16[0] = m_MacAddress.au16[1] =  m_MacAddress.au16[2] = 0xff;
        m_enmState      = kState_Free;
        m_xid           = UINT32_MAX;
    }

    void            offer(uint32_t xid);
    void            activate(void);
    void            activate(uint32_t xid);
    void            release(void);
    bool            hasExpired(void) const;

    /**
     * Checks if the lease is in use or not.
     *
     * @returns true if active, false if free or expired.
     *
     * @param   pNow        The current time to use. Optional.
     */
    bool            isInUse(PCRTTIMESPEC pNow = NULL) const
    {
        if  (   m_enmState == kState_Offer
             || m_enmState == kState_Active)
        {
            RTTIMESPEC Now;
            if (!pNow)
                pNow = RTTimeNow(&Now);
            return RTTimeSpecGetSeconds(&m_ExpireTime) > RTTimeSpecGetSeconds(pNow);
        }
        return false;
    }

    /**
     * Is this lease for one specific client?
     *
     * @return  true/false.
     */
    bool            isOneSpecificClient(void) const
    {
        return m_pCfg
            && m_pCfg->isOneSpecificClient();
    }

    /**
     * Is this lease currently being offered to a client.
     *
     * @returns true / false.
     */
    bool            isBeingOffered(void) const
    {
        return m_enmState == kState_Offer
            && isInUse();
    }

    /**
     * Is the lease in the current config or not.
     *
     * When updating the config we might leave active leases behind which aren't
     * included in the new config. These will have m_pCfg set to NULL and should be
     * freed up when they expired.
     *
     * @returns true / false.
     */
    bool            isInCurrentConfig(void) const
    {
        return m_pCfg != NULL;
    }
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
    void                explodeConfig(void);

    bool                handleDhcpMsg(uint8_t uMsgType, PCRTNETBOOTP pDhcpMsg, size_t cb);
    bool                handleDhcpReqDiscover(PCRTNETBOOTP pDhcpMsg, size_t cb);
    bool                handleDhcpReqRequest(PCRTNETBOOTP pDhcpMsg, size_t cb);
    bool                handleDhcpReqDecline(PCRTNETBOOTP pDhcpMsg, size_t cb);
    bool                handleDhcpReqRelease(PCRTNETBOOTP pDhcpMsg, size_t cb);
    void                makeDhcpReply(uint8_t uMsgType, VBoxNetDhcpLease *pLease, PCRTNETBOOTP pDhcpMsg, size_t cb);

    VBoxNetDhcpLease   *findLeaseByMacAddress(PCRTMAC pMacAddress, bool fAnyState);
    VBoxNetDhcpLease   *findLeaseByIpv4AndMacAddresses(RTNETADDRIPV4 IPv4Addr, PCRTMAC pMacAddress, bool fAnyState);
    VBoxNetDhcpLease   *newLease(PCRTNETBOOTP pDhcpMsg, size_t cb);

    static uint8_t const *findOption(uint8_t uOption, PCRTNETBOOTP pDhcpMsg, size_t cb, size_t *pcbMaxOpt);
    static bool         findOptionIPv4Addr(uint8_t uOption, PCRTNETBOOTP pDhcpMsg, size_t cb, PRTNETADDRIPV4 pIPv4Addr);

    inline void         debugPrint( int32_t iMinLevel, bool fMsg,  const char *pszFmt, ...) const;
    void                debugPrintV(int32_t iMinLevel, bool fMsg,  const char *pszFmt, va_list va) const;
    static const char  *debugDhcpName(uint8_t uMsgType);

protected:
    /** @name The server configuration data members.
     * @{ */
    std::string         m_Name;
    std::string         m_Network;
    std::string         m_TrunkName;
    INTNETTRUNKTYPE     m_enmTrunkType;
    RTMAC               m_MacAddress;
    RTNETADDRIPV4       m_Ipv4Address;
    std::string         m_LeaseDBName;
    /** @} */

    /** The current configs. */
    std::vector<VBoxNetDhcpCfg *> m_Cfgs;

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
 * Offer this lease to a client.
 *
 * @param   xid             The transaction ID.
 */
void VBoxNetDhcpLease::offer(uint32_t xid)
{
    m_enmState = kState_Offer;
    m_xid = xid;
    RTTimeNow(&m_ExpireTime);
    RTTimeSpecAddSeconds(&m_ExpireTime, 60);
}


/**
 * Activate this lease (i.e. a client is now using it).
 */
void VBoxNetDhcpLease::activate(void)
{
    m_enmState = kState_Active;
    RTTimeNow(&m_ExpireTime);
    RTTimeSpecAddSeconds(&m_ExpireTime, m_pCfg ? m_pCfg->m_cSecLease : 60); /* m_pCfg can be NULL right now... */
}


/**
 * Activate this lease with a new transaction ID.
 *
 * @param   xid     The transaction ID.
 * @todo    check if this is really necessary.
 */
void VBoxNetDhcpLease::activate(uint32_t xid)
{
    activate();
    m_xid = xid;
}


/**
 * Release a lease either upon client request or because it didn't quite match a
 * DHCP_REQUEST.
 */
void VBoxNetDhcpLease::release(void)
{
    m_enmState = kState_Free;
    RTTimeNow(&m_ExpireTime);
    RTTimeSpecAddSeconds(&m_ExpireTime, 5);
}


/**
 * Checks if the lease has expired or not.
 *
 * This just checks the expiration time not the state. This is so that this
 * method will work for reusing RELEASEd leases when the client comes back after
 * a reboot or ipconfig /renew. Callers not interested in info on released
 * leases should check the state first.
 *
 * @returns true if expired, false if not.
 */
bool VBoxNetDhcpLease::hasExpired() const
{
    RTTIMESPEC Now;
    return RTTimeSpecGetSeconds(&m_ExpireTime) > RTTimeSpecGetSeconds(RTTimeNow(&Now));
}




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
#if 0
    pDefCfg->m_cSecLease      = 60*60; /* 1 hour */
#else
    pDefCfg->m_cSecLease      = 30; /* sec */
#endif
    pDefCfg->m_TftpServer     = "10.0.2.3"; //??
    this->addConfig(pDefCfg);
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
        m_pSession = NIL_RTR0PTR;
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
            m_Cfgs.push_back(pCfg);
        else
            delete pCfg;
    }
    return rc;
}


/**
 * Explodes the config into leases.
 *
 * @remarks     This code is brute force and not very fast nor memory efficient.
 *              We will have to revisit this later.
 *
 * @remarks     If an IP has been reconfigured for a fixed mac address and it's
 *              already leased to a client, we it won't be available until the
 *              client releases its lease or it expires.
 */
void VBoxNetDhcp::explodeConfig(void)
{
    RTTIMESPEC  Now;
    RTTimeNow(&Now);

    /*
     * Remove all non-active leases from the vector and zapping the
     * config pointers of the once left behind.
     */
    std::vector<VBoxNetDhcpLease>::iterator Itr = m_Leases.begin();
    while (Itr != m_Leases.end())
    {
        if (!Itr->isInUse(&Now))
            Itr = m_Leases.erase(Itr);
        else
        {
            Itr->m_pCfg = NULL;
            Itr++;
        }
    }

    /*
     * Loop thru the configurations in reverse order, giving the last
     * configs priority of the newer ones.
     */
    size_t iCfg = m_Cfgs.size();
    while (iCfg-- > 0)
    {
        VBoxNetDhcpCfg *pCfg = m_Cfgs[iCfg];

        /* Expand the IP lease range. */
        uint32_t const uEnd = RT_N2H_U32(pCfg->m_UpperAddr.u);
        for (uint32_t i = RT_N2H_U32(pCfg->m_LowerAddr.u); i < uEnd; i++)
        {
            RTNETADDRIPV4 IPv4Addr;
            IPv4Addr.u = RT_H2N_U32(i);

            /* Check if it exists and is configured. */
            VBoxNetDhcpLease *pLease = NULL;
            for (size_t i = 0; i < m_Leases.size(); i++)
                if (m_Leases[i].m_IPv4Address.u == IPv4Addr.u)
                {
                    pLease = &m_Leases[i];
                    break;
                }
            if (pLease)
            {
                if (!pLease->m_pCfg)
                    pLease->m_pCfg = pCfg;
            }
            else
            {
                /* add it. */
                VBoxNetDhcpLease NewLease(IPv4Addr, pCfg);
                m_Leases.push_back(NewLease);
                debugPrint(10, false, "exploseConfig: new lease %d.%d.%d.%d",
                           IPv4Addr.au8[0], IPv4Addr.au8[1], IPv4Addr.au8[2], IPv4Addr.au8[3]);
            }
        }
    }
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
        { "--trunk-name",     't',   RTGETOPT_REQ_STRING },
        { "--trunk-type",     'T',   RTGETOPT_REQ_STRING },
        { "--mac-address",    'a',   RTGETOPT_REQ_MACADDR },
        { "--ip-address",     'i',   RTGETOPT_REQ_IPV4ADDR },
        { "--lease-db",       'D',   RTGETOPT_REQ_STRING },
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
            case 't':
                m_TrunkName = Val.psz;
                break;
            case 'T':
                if (!strcmp(Val.psz, "whatever"))
                    m_enmTrunkType = kIntNetTrunkType_WhateverNone;
                else if (!strcmp(Val.psz, "netflt"))
                    m_enmTrunkType = kIntNetTrunkType_NetFlt;
                else if (!strcmp(Val.psz, "netadp"))
                    m_enmTrunkType = kIntNetTrunkType_NetAdp;
                else if (!strcmp(Val.psz, "srvnat"))
                    m_enmTrunkType = kIntNetTrunkType_SrvNat;
                else
                {
                    RTStrmPrintf(g_pStdErr, "Invalid trunk type '%s'\n", Val.psz);
                    return 1;
                }
                break;
            case 'a':
                m_MacAddress = Val.MacAddr;
                break;
            case 'i':
                m_Ipv4Address = Val.IPv4Addr;
                break;
            case 'd':
                m_LeaseDBName = Val.psz;
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
                        return 1;
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
                        return 1;
                }
                break;

            case 'V':
                RTPrintf("%sr%d\n", VBOX_VERSION_STRING, VBOX_SVN_REV);
                return 0;

            case 'h':
                RTPrintf("VBoxNetDHCP Version %s\n"
                         "(C) 2009 Sun Microsystems, Inc.\n"
                         "All rights reserved\n"
                         "\n"
                         "Usage: VBoxNetDHCP <options>\n"
                         "\n"
                         "Options:\n"
                         VBOX_VERSION_STRING);
                for (size_t i = 0; i < RT_ELEMENTS(s_aOptionDefs); i++)
                    RTPrintf("    -%c, %s\n", s_aOptionDefs[i].iShort, s_aOptionDefs[i].pszLong);
                return 1;

            default:
                break;
        }
    }

    /*
     * Do the reconfig. (move this later)
     */
    if (!rc)
        explodeConfig();

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
        m_pSession = NIL_RTR0PTR;
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
    OpenReq.szNetwork[sizeof(OpenReq.szNetwork) - 1] = '\0';
    strncpy(OpenReq.szTrunk, m_TrunkName.c_str(), sizeof(OpenReq.szTrunk));
    OpenReq.szTrunk[sizeof(OpenReq.szTrunk) - 1] = '\0';
    OpenReq.enmTrunkType = m_enmTrunkType;
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
                /* nothing */;

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
     * The newLease() method contains logic for finding current leases
     * and reusing them in case the client is forgetful.
     */
    VBoxNetDhcpLease *pLease = newLease(pDhcpMsg, cb);
    if (!pLease)
        return false;
    debugPrint(1, true, "Offering %d.%d.%d.%d to %.6Rhxs xid=%#x",
               pLease->m_IPv4Address.au8[0],
               pLease->m_IPv4Address.au8[1],
               pLease->m_IPv4Address.au8[2],
               pLease->m_IPv4Address.au8[3],
               &pDhcpMsg->bp_chaddr.Mac,
               pDhcpMsg->bp_xid);
    pLease->offer(pDhcpMsg->bp_xid);

    makeDhcpReply(RTNET_DHCP_MT_OFFER, pLease, pDhcpMsg, cb);
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
    /** @todo Probably need to match the server IP here to work correctly with
     *        other servers. */
    /** @todo This code isn't entirely correct and quite a bit of a hack, but it
     *        will have to do for now as the right thing (tm) is very complex.
     *        Part of the fun is verifying that the request is something we can
     *        and should handle. */

    /*
     * Try find the lease by the requested address + client MAC address.
     */
    VBoxNetDhcpLease   *pLease = NULL;
    RTNETADDRIPV4       IPv4Addr;
    bool                fReqAddr = findOptionIPv4Addr(RTNET_DHCP_OPT_REQ_ADDR, pDhcpMsg, cb, &IPv4Addr);
    if (fReqAddr)
    {
        fReqAddr = true;
        pLease = findLeaseByIpv4AndMacAddresses(IPv4Addr, &pDhcpMsg->bp_chaddr.Mac, true /* fAnyState */);
    }

    /*
     * Try find the lease by the client IP address + client MAC address.
     */
    if (    !pLease
        &&  pDhcpMsg->bp_ciaddr.u)
        pLease = findLeaseByIpv4AndMacAddresses(pDhcpMsg->bp_ciaddr, &pDhcpMsg->bp_chaddr.Mac, true /* fAnyState */);

#if 0 /** @todo client id stuff - it doesn't make sense here imho, we need IP + MAC. What would make sense
                though is to compare the client id with what we've got in the lease and use it to root out
                bad requests. */
    /*
     * Try find the lease by using the client id.
     */
    if (!pLease)
    {
        size_t          cbClientID = 0;
        uint8_t const  *pbClientID  = findOption(RTNET_DHCP_OPT_CLIENT_ID, pDhcpMsg, cb, &cbClientID);
        if (    pbClientID
            &&  cbClientID == sizeof(RTMAC) + 1
            &&  pbClientID[0] == RTNET_ARP_ETHER
            &&
                )
        {
            pLease = findLeaseByIpv4AndMacAddresses(pDhcpMsg->bp_ciaddr, &pDhcpMsg->bp_chaddr.Mac, true /* fAnyState */);
        }
    }
#endif

    /*
     * Validate the lease that's requested.
     * We've already check the MAC and IP addresses.
     */
    bool fAckIt = false;
    if (pLease)
    {
        if (pLease->isBeingOffered())
        {
            if (pLease->m_xid == pDhcpMsg->bp_xid)
            {
                fAckIt = true;
                debugPrint(2, true, "REQUEST for offered lease.");
                pLease->activate();
            }
            else
                debugPrint(2, true, "REQUEST for offered lease, xid mismatch. Expected %#x, got %#x.",
                           pLease->m_xid, pDhcpMsg->bp_xid);
        }
        else if (!pLease->isInCurrentConfig())
            debugPrint(1, true, "REQUEST for obsolete lease -> NAK");
        else if (fReqAddr != (pDhcpMsg->bp_ciaddr.u != 0)) // ???
        {
            /** @todo this ain't safe. */
            debugPrint(1, true, "REQUEST for lease not on offer, assuming renewal. lease_xid=%#x bp_xid=%#x",
                       pLease->m_xid, pDhcpMsg->bp_xid);
            fAckIt = true;
            pLease->activate(pDhcpMsg->bp_xid);
        }
        else
            debugPrint(1, true, "REQUEST for lease not on offer, NAK it.");
    }

    /*
     * NAK if if no lease was found.
     */
    if (fAckIt)
    {
        debugPrint(1, false, "ACK'ing DHCP_REQUEST");
        makeDhcpReply(RTNET_DHCP_MT_ACK, pLease, pDhcpMsg, cb);
    }
    else
    {
        debugPrint(1, false, "NAK'ing DHCP_REQUEST");
        makeDhcpReply(RTNET_DHCP_MT_NAC, NULL, pDhcpMsg, cb);
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
bool VBoxNetDhcp::handleDhcpReqDecline(PCRTNETBOOTP pDhcpMsg, size_t cb)
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
bool VBoxNetDhcp::handleDhcpReqRelease(PCRTNETBOOTP pDhcpMsg, size_t cb)
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
 * Helper class for stuffing DHCP options into a reply packet.
 */
class VBoxNetDhcpWriteCursor
{
private:
    uint8_t        *m_pbCur;       /**< The current cursor position. */
    uint8_t        *m_pbEnd;       /**< The end the current option space. */
    uint8_t        *m_pfOverload;  /**< Pointer to the flags of the overload option. */
    uint8_t         m_fUsed;       /**< Overload fields that have been used. */
    PRTNETDHCPOPT   m_pOpt;        /**< The current option. */
    PRTNETBOOTP     m_pDhcp;       /**< The DHCP packet. */
    bool            m_fOverflowed; /**< Set if we've overflowed, otherwise false. */

public:
    /** Instantiate an option cursor for the specified DHCP message. */
    VBoxNetDhcpWriteCursor(PRTNETBOOTP pDhcp, size_t cbDhcp) :
        m_pbCur(&pDhcp->bp_vend.Dhcp.dhcp_opts[0]),
        m_pbEnd((uint8_t *)pDhcp + cbDhcp),
        m_pfOverload(NULL),
        m_fUsed(0),
        m_pOpt(NULL),
        m_pDhcp(pDhcp),
        m_fOverflowed(false)
    {
        AssertPtr(pDhcp);
        Assert(cbDhcp > RT_UOFFSETOF(RTNETBOOTP, bp_vend.Dhcp.dhcp_opts[10]));
    }

    /** Destructor.  */
    ~VBoxNetDhcpWriteCursor()
    {
        m_pbCur = m_pbEnd = m_pfOverload = NULL;
        m_pOpt = NULL;
        m_pDhcp = NULL;
    }

    /**
     * Try use the bp_file field.
     * @returns true if not overloaded, false otherwise.
     */
    bool useBpFile(void)
    {
        if (    m_pfOverload
            &&  (*m_pfOverload & 1))
            return false;
        m_fUsed |= 1 /* bp_file flag*/;
        return true;
    }


    /**
     * Try overload more BOOTP fields
     */
    bool overloadMore(void)
    {
        /* switch option area. */
        uint8_t    *pbNew;
        uint8_t    *pbNewEnd;
        uint8_t     fField;
        if (!(m_fUsed & 1))
        {
            fField     = 1;
            pbNew      = &m_pDhcp->bp_file[0];
            pbNewEnd   = &m_pDhcp->bp_file[sizeof(m_pDhcp->bp_file)];
        }
        else if (!(m_fUsed & 2))
        {
            fField     = 2;
            pbNew      = &m_pDhcp->bp_sname[0];
            pbNewEnd   = &m_pDhcp->bp_sname[sizeof(m_pDhcp->bp_sname)];
        }
        else
            return false;

        if (!m_pfOverload)
        {
            /* Add an overload option. */
            *m_pbCur++ = RTNET_DHCP_OPT_OPTION_OVERLOAD;
            *m_pbCur++ = fField;
            m_pfOverload = m_pbCur;
            *m_pbCur++ = 1;     /* bp_file flag */
        }
        else
            *m_pfOverload |= fField;

        /* pad current option field */
        while (m_pbCur != m_pbEnd)
            *m_pbCur++ = RTNET_DHCP_OPT_PAD; /** @todo not sure if this stuff is at all correct... */

        /* switch */
        m_pbCur = pbNew;
        m_pbEnd = pbNewEnd;
        return true;
    }

    /**
     * Begin an option.
     *
     * @returns true on succes, false if we're out of space.
     *
     * @param   uOption     The option number.
     * @param   cb          The amount of data.
     */
    bool begin(uint8_t uOption, size_t cb)
    {
        /* Check that the data of the previous option has all been written. */
        Assert(   !m_pOpt
               || (m_pbCur - m_pOpt->dhcp_len == (uint8_t *)(m_pOpt + 1)));
        AssertMsg(cb <= 255, ("%#x\n", cb));

        /* Check if we need to overload more stuff. */
        if ((uintptr_t)(m_pbEnd - m_pbCur) < cb + 2 + (m_pfOverload ? 1 : 3))
        {
            m_pOpt = NULL;
            if (!overloadMore())
            {
                m_fOverflowed = true;
                AssertMsgFailedReturn(("%u %#x\n", uOption, cb), false);
            }
            if ((uintptr_t)(m_pbEnd - m_pbCur) < cb + 2 + 1)
            {
                m_fOverflowed = true;
                AssertMsgFailedReturn(("%u %#x\n", uOption, cb), false);
            }
        }

        /* Emit the option header. */
        m_pOpt = (PRTNETDHCPOPT)m_pbCur;
        m_pOpt->dhcp_opt = uOption;
        m_pOpt->dhcp_len = cb;
        m_pbCur += 2;
        return true;
    }

    /**
     * Puts option data.
     *
     * @param   pvData      The data.
     * @param   cb          The amount to put.
     */
    void put(void const *pvData, size_t cb)
    {
        Assert(m_pOpt || m_fOverflowed);
        if (RT_LIKELY(m_pOpt))
        {
            Assert((uintptr_t)m_pbCur - (uintptr_t)(m_pOpt + 1) + cb  <= (size_t)m_pOpt->dhcp_len);
            memcpy(m_pbCur, pvData, cb);
            m_pbCur += cb;
        }
    }

    /**
     * Puts an IPv4 Address.
     *
     * @param   IPv4Addr    The address.
     */
    void putIPv4Addr(RTNETADDRIPV4 IPv4Addr)
    {
        put(&IPv4Addr, 4);
    }

    /**
     * Adds an IPv4 address option.
     *
     * @returns true/false just like begin().
     *
     * @param   uOption     The option number.
     * @param   IPv4Addr    The address.
     */
    bool optIPv4Addr(uint8_t uOption, RTNETADDRIPV4 IPv4Addr)
    {
        if (!begin(uOption, 4))
            return false;
        putIPv4Addr(IPv4Addr);
        return true;
    }

    /**
     * Adds an option taking 1 or more IPv4 address.
     *
     * If the vector contains no addresses, the option will not be added.
     *
     * @returns true/false just like begin().
     *
     * @param   uOption     The option number.
     * @param   rIPv4Addrs  Reference to the address vector.
     */
    bool optIPv4Addrs(uint8_t uOption, std::vector<RTNETADDRIPV4> const &rIPv4Addrs)
    {
        size_t const c = rIPv4Addrs.size();
        if (!c)
            return true;

        if (!begin(uOption, 4*c))
            return false;
        for (size_t i = 0; i < c; i++)
            putIPv4Addr(rIPv4Addrs[i]);
        return true;
    }

    /**
     * Puts an 8-bit integer.
     *
     * @param   u8          The integer.
     */
    void putU8(uint8_t u8)
    {
        put(&u8, 1);
    }

    /**
     * Adds an 8-bit integer option.
     *
     * @returns true/false just like begin().
     *
     * @param   uOption     The option number.
     * @param   u8          The integer
     */
    bool optU8(uint8_t uOption, uint8_t u8)
    {
        if (!begin(uOption, 1))
            return false;
        putU8(u8);
        return true;
    }

    /**
     * Puts an 32-bit integer (network endian).
     *
     * @param   u32Network  The integer.
     */
    void putU32(uint32_t u32)
    {
        put(&u32, 4);
    }

    /**
     * Adds an 32-bit integer (network endian) option.
     *
     * @returns true/false just like begin().
     *
     * @param   uOption     The option number.
     * @param   u32Network  The integer.
     */
    bool optU32(uint8_t uOption, uint32_t u32)
    {
        if (!begin(uOption, 4))
            return false;
        putU32(u32);
        return true;
    }

    /**
     * Puts a std::string.
     *
     * @param   rStr        Reference to the string.
     */
    void putStr(std::string const &rStr)
    {
        put(rStr.c_str(), rStr.size());
    }

    /**
     * Adds an std::string option if the string isn't empty.
     *
     * @returns true/false just like begin().
     *
     * @param   uOption     The option number.
     * @param   rStr        Reference to the string.
     */
    bool optStr(uint8_t uOption, std::string const &rStr)
    {
        const size_t cch = rStr.size();
        if (!cch)
            return true;

        if (!begin(uOption, cch))
            return false;
        put(rStr.c_str(), cch);
        return true;
    }

    /**
     * Whether we've overflowed.
     *
     * @returns true on overflow, false otherwise.
     */
    bool hasOverflowed(void) const
    {
        return m_fOverflowed;
    }

    /**
     * Adds the terminating END option.
     *
     * The END will always be added as we're reserving room for it, however, we
     * might've dropped previous options due to overflows and that is what the
     * return status indicates.
     *
     * @returns true on success, false on a (previous) overflow.
     */
    bool optEnd(void)
    {
        Assert((uintptr_t)(m_pbEnd - m_pbCur) < 4096);
        *m_pbCur++ = RTNET_DHCP_OPT_END;
        return !hasOverflowed();
    }
};


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
void VBoxNetDhcp::makeDhcpReply(uint8_t uMsgType, VBoxNetDhcpLease *pLease, PCRTNETBOOTP pDhcpMsg, size_t cb)
{
    size_t      cbReply = RTNET_DHCP_NORMAL_SIZE; /** @todo respect the RTNET_DHCP_OPT_MAX_DHCP_MSG_SIZE option */
    PRTNETBOOTP pReply = (PRTNETBOOTP)alloca(cbReply);

    /*
     * The fixed bits stuff.
     */
    pReply->bp_op     = RTNETBOOTP_OP_REPLY;
    pReply->bp_htype  = RTNET_ARP_ETHER;
    pReply->bp_hlen   = sizeof(RTMAC);
    pReply->bp_hops   = 0;
    pReply->bp_xid    = pDhcpMsg->bp_xid;
    pReply->bp_secs   = 0;
    pReply->bp_flags  = 0; // (pDhcpMsg->bp_flags & RTNET_DHCP_FLAGS_NO_BROADCAST); ??
    pReply->bp_ciaddr.u = 0;
    pReply->bp_yiaddr.u = pLease ? pLease->m_IPv4Address.u : 0xffffffff;
    pReply->bp_siaddr.u = pLease && pLease->m_pCfg ? pLease->m_pCfg->m_TftpServerAddr.u : 0; /* (next server == TFTP)*/
    pReply->bp_giaddr.u = 0;
    memset(&pReply->bp_chaddr, '\0', sizeof(pReply->bp_chaddr));
    pReply->bp_chaddr.Mac = pDhcpMsg->bp_chaddr.Mac;
    memset(&pReply->bp_sname[0], '\0', sizeof(pReply->bp_sname));
    memset(&pReply->bp_file[0],  '\0', sizeof(pReply->bp_file));
    pReply->bp_vend.Dhcp.dhcp_cookie = RT_H2N_U32_C(RTNET_DHCP_COOKIE);
    memset(&pReply->bp_vend.Dhcp.dhcp_opts[0], '\0', RTNET_DHCP_OPT_SIZE);

    /*
     * The options - use a cursor class for dealing with the ugly stuff.
     */
    VBoxNetDhcpWriteCursor Cursor(pReply, cbReply);

    /* The basics */
    Cursor.optU8(RTNET_DHCP_OPT_MSG_TYPE, uMsgType);
    Cursor.optIPv4Addr(RTNET_DHCP_OPT_SERVER_ID, m_Ipv4Address);

    if (uMsgType != RTNET_DHCP_MT_NAC)
    {
        AssertReturnVoid(pLease && pLease->m_pCfg);
        const VBoxNetDhcpCfg *pCfg = pLease->m_pCfg; /* no need to retain it. */

        /* The IP config. */
        Cursor.optU32(RTNET_DHCP_OPT_LEASE_TIME, RT_H2N_U32(pCfg->m_cSecLease));
        Cursor.optIPv4Addr(RTNET_DHCP_OPT_SUBNET_MASK, pCfg->m_SubnetMask);
        Cursor.optIPv4Addrs(RTNET_DHCP_OPT_ROUTERS, pCfg->m_Routers);
        Cursor.optIPv4Addrs(RTNET_DHCP_OPT_ROUTERS, pCfg->m_DNSes);
        Cursor.optStr(RTNET_DHCP_OPT_HOST_NAME, pCfg->m_HostName);
        Cursor.optStr(RTNET_DHCP_OPT_DOMAIN_NAME, pCfg->m_DomainName);

        /* The PXE config. */
        if (pCfg->m_BootfileName.size())
        {
            if (Cursor.useBpFile())
                RTStrPrintf((char *)&pReply->bp_file[0], sizeof(pReply->bp_file), "%s", pCfg->m_BootfileName.c_str());
            else
                Cursor.optStr(RTNET_DHCP_OPT_BOOTFILE_NAME, pCfg->m_BootfileName);
        }
    }

    /* Terminate the options. */
    if (!Cursor.optEnd())
        debugPrint(0, true, "option overflow\n");

    /*
     * Send it.
     */
    int rc;
#if 0
    if (!(pDhcpMsg->bp_flags & RTNET_DHCP_FLAGS_NO_BROADCAST)) /** @todo need to see someone set this flag to check that it's correct. */
    {
        RTNETADDRIPV4 IPv4AddrBrdCast;
        IPv4AddrBrdCast.u = UINT32_C(0xffffffff); /* broadcast IP */
        rc = VBoxNetUDPUnicast(m_pSession, m_hIf, m_pIfBuf,
                               m_Ipv4Address, &m_MacAddress, RTNETIPV4_PORT_BOOTPS,                 /* sender */
                               IPv4AddrBrdCast, &pDhcpMsg->bp_chaddr.Mac, RTNETIPV4_PORT_BOOTPC,    /* receiver */
                               pReply, cbReply);
    }
    else
#endif
        rc = VBoxNetUDPBroadcast(m_pSession, m_hIf, m_pIfBuf,
                                 m_Ipv4Address, &m_MacAddress, RTNETIPV4_PORT_BOOTPS,               /* sender */
                                 RTNETIPV4_PORT_BOOTPC,                                             /* receiver port */
                                 pReply, cbReply);
    if (RT_FAILURE(rc))
        debugPrint(0, true, "error %Rrc when sending the reply", rc);
}


/**
 * Look up a lease by MAC address.
 *
 * @returns Pointer to the lease if found, NULL if not found.
 * @param   pMacAddress             The mac address.
 * @param   fAnyState       Any state.
 */
VBoxNetDhcpLease *VBoxNetDhcp::findLeaseByMacAddress(PCRTMAC pMacAddress, bool fAnyState)
{
    size_t iLease = m_Leases.size();
    while (iLease-- > 0)
    {
        VBoxNetDhcpLease *pLease = &m_Leases[iLease];
        if (    pLease
            &&  pLease->m_MacAddress.au16[0] == pMacAddress->au16[0]
            &&  pLease->m_MacAddress.au16[1] == pMacAddress->au16[1]
            &&  pLease->m_MacAddress.au16[2] == pMacAddress->au16[2]
            &&  (   fAnyState
                 || (pLease->m_enmState != VBoxNetDhcpLease::kState_Free)) )
            return pLease;
    }

    return NULL;
}


/**
 * Look up a lease by IPv4 and MAC addresses.
 *
 * @returns Pointer to the lease if found, NULL if not found.
 * @param   IPv4Addr        The IPv4 address.
 * @param   pMacAddress     The mac address.
 * @param   fAnyState       Any state.
 */
VBoxNetDhcpLease *VBoxNetDhcp::findLeaseByIpv4AndMacAddresses(RTNETADDRIPV4 IPv4Addr, PCRTMAC pMacAddress, bool fAnyState)
{
    size_t iLease = m_Leases.size();
    while (iLease-- > 0)
    {
        VBoxNetDhcpLease *pLease = &m_Leases[iLease];
        if (    pLease
            &&  pLease->m_IPv4Address.u      == IPv4Addr.u
            &&  pLease->m_MacAddress.au16[0] == pMacAddress->au16[0]
            &&  pLease->m_MacAddress.au16[1] == pMacAddress->au16[1]
            &&  pLease->m_MacAddress.au16[2] == pMacAddress->au16[2]
            &&  (   fAnyState
                 || (pLease->m_enmState != VBoxNetDhcpLease::kState_Free)) )
            return pLease;
    }

    return NULL;
}


/**
 * Creates a new lease for the client specified in the DHCP message.
 *
 * The caller has already made sure it doesn't already have a lease.
 *
 * @returns Pointer to the lease if found, NULL+log if not found.
 * @param   IPv4Addr        The IPv4 address.
 * @param   pMacAddress     The MAC address.
 */
VBoxNetDhcpLease *VBoxNetDhcp::newLease(PCRTNETBOOTP pDhcpMsg, size_t cb)
{
    RTMAC const MacAddr = pDhcpMsg->bp_chaddr.Mac;
    RTTIMESPEC  Now;
    RTTimeNow(&Now);

    /*
     * Search the possible leases.
     *
     * We'll try do all the searches in one pass, that is to say, perfect
     * match, old lease, and next free/expired lease.
     */
    VBoxNetDhcpLease *pBest = NULL;
    VBoxNetDhcpLease *pOld  = NULL;
    VBoxNetDhcpLease *pFree = NULL;

    size_t cLeases = m_Leases.size();
    for (size_t i = 0; i < cLeases; i++)
    {
        VBoxNetDhcpLease *pCur = &m_Leases[i];

        /* Skip it if no configuration, that means its not in the current config. */
        if (!pCur->m_pCfg)
            continue;

        /* best */
        if (    pCur->isOneSpecificClient()
            &&  pCur->m_pCfg->matchesMacAddress(&MacAddr))
        {
            if (    !pBest
                ||  pBest->m_pCfg->m_MacAddresses.size() < pCur->m_pCfg->m_MacAddresses.size())
                pBest = pCur;
        }

        /* old lease */
        if (    pCur->m_MacAddress.au16[0] == MacAddr.au16[0]
            &&  pCur->m_MacAddress.au16[1] == MacAddr.au16[1]
            &&  pCur->m_MacAddress.au16[2] == MacAddr.au16[2])
        {
            if (    !pOld
                ||  RTTimeSpecGetSeconds(&pCur->m_ExpireTime) > RTTimeSpecGetSeconds(&pFree->m_ExpireTime))
                pOld = pCur;
        }

        /* expired lease */
        if (!pCur->isInUse(&Now))
        {
            if (    !pFree
                ||  RTTimeSpecGetSeconds(&pCur->m_ExpireTime) < RTTimeSpecGetSeconds(&pFree->m_ExpireTime))
                pFree = pCur;
        }
    }

    VBoxNetDhcpLease *pNew = pBest;
    if (!pNew)
        pNew = pOld;
    if (!pNew)
        pNew = pFree;
    if (!pNew)
    {
        debugPrint(0, true, "No more leases.");
        return NULL;
    }

    /*
     * Init the lease.
     */
    pNew->m_MacAddress = MacAddr;
    pNew->m_xid        = pDhcpMsg->bp_xid;
    /** @todo extract the client id. */

    return pNew;
}


/**
 * Finds an option.
 *
 * @returns On success, a pointer to the first byte in the option data (no none
 *          then it'll be the byte following the 0 size field) and *pcbOpt set
 *          to the option length.
 *          On failure, NULL is returned and *pcbOpt unchanged.
 *
 * @param   uOption         The option to search for.
 * @param   pDhcpMsg        The DHCP message.
 * @param   cb              The size of the message.
 * @param   pcbOpt          Where to store the option size size. Optional. Note
 *                          that this is adjusted if the option length is larger
 *                          than the message buffer.
 */
/* static */ const uint8_t *
VBoxNetDhcp::findOption(uint8_t uOption, PCRTNETBOOTP pDhcpMsg, size_t cb, size_t *pcbOpt)
{
    Assert(uOption != RTNET_DHCP_OPT_PAD);

    /*
     * Validate the DHCP bits and figure the max size of the options in the vendor field.
     */
    if (cb <= RT_UOFFSETOF(RTNETBOOTP, bp_vend.Dhcp.dhcp_opts))
        return NULL;
    if (pDhcpMsg->bp_vend.Dhcp.dhcp_cookie != RT_H2N_U32_C(RTNET_DHCP_COOKIE))
        return NULL;
    size_t cbLeft = cb - RT_UOFFSETOF(RTNETBOOTP, bp_vend.Dhcp.dhcp_opts);
    if (cbLeft > RTNET_DHCP_OPT_SIZE)
        cbLeft = RTNET_DHCP_OPT_SIZE;

    /*
     * Search the vendor field.
     */
    bool            fExtended = false;
    uint8_t const  *pb = &pDhcpMsg->bp_vend.Dhcp.dhcp_opts[0];
    while (pb && cbLeft > 0)
    {
        uint8_t uCur  = *pb;
        if (uCur == RTNET_DHCP_OPT_PAD)
        {
            cbLeft--;
            pb++;
        }
        else if (cbLeft <= 1)
            break;
        else
        {
            size_t  cbCur = pb[1];
            if (cbCur > cbLeft - 2)
                cbCur = cbLeft - 2;
            if (uCur == uOption)
            {
                if (pcbOpt)
                    *pcbOpt = cbCur;
                return pb+2;
            }
            pb     += cbCur + 2;
            cbLeft -= cbCur - 2;
        }
    }

    /** @todo search extended dhcp option field(s) when present */

    return NULL;
}


/**
 * Locates an option with an IPv4 address in the DHCP message.
 *
 * @returns true and *pIpv4Addr if found, false if not.
 *
 * @param   uOption         The option to find.
 * @param   pDhcpMsg        The DHCP message.
 * @param   cb              The size of the message.
 * @param   pIPv4Addr       Where to put the address.
 */
/* static */ bool
VBoxNetDhcp::findOptionIPv4Addr(uint8_t uOption, PCRTNETBOOTP pDhcpMsg, size_t cb, PRTNETADDRIPV4 pIPv4Addr)
{
    size_t          cbOpt;
    uint8_t const  *pbOpt = findOption(uOption, pDhcpMsg, cb, &cbOpt);
    if (pbOpt)
    {
        if (cbOpt >= sizeof(RTNETADDRIPV4))
        {
            *pIPv4Addr = *(PCRTNETADDRIPV4)pbOpt;
            return true;
        }
    }
    return false;
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

