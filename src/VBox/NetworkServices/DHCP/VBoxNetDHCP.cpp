

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/initterm.h>
#include <iprt/net.h>
#include <iprt/err.h>
#include <iprt/time.h>
#include <iprt/stream.h>
#include <iprt/getopt.h>

#ifdef VBOX_WITH_HARDENING
# include <VBox/sup.h>
#endif
#include <VBox/version.h>

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

    int parseArgs(int argc, char **argv);
    int tryGoOnline(void);
    int run(void);

protected:
    int addConfig(VBoxNetDhcpCfg *pCfg);

protected:
    /** @name The server configuration data members.
     * @{ */
    std::string         m_Name;
    std::string         m_Network;
    RTMAC               m_MacAddress;
    RTNETADDRIPV4       m_IpAddress;
    int32_t             m_cVerbosity;
    /** @} */

    /** The current configs. */
    std::vector<VBoxNetDhcpCfg> m_Cfgs;

    /** The current leases. */
    std::vector<VBoxNetDhcpLease> m_Leases;
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
/// @todo        { "--mac-address",    'a',   RTGETOPT_REQ_MACADDR },
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
    AssertReturn(rc, 49);

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
AssertFailed();
//                m_MacAddress = Val.Mac;
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
    return 0;
}


/**
 * Runs the DHCP server.
 *
 * @returns 0 on success, exit code + error message to stderr on failure.
 */
int VBoxNetDhcp::run(void)
{
    return 0;
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

