/* $Id$ */

/**
 * XXX: license.
 */

#include <VBox/com/com.h>
#include <VBox/com/listeners.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/EventQueue.h>
#include <VBox/com/VirtualBox.h>

#include <iprt/asm.h>
#include <iprt/net.h>
#include <iprt/time.h>

#include <VBox/sup.h>
#include <VBox/intnet.h>
#include <VBox/intnetinline.h>
#include <VBox/vmm/vmm.h>
#include <VBox/version.h>

#include "../NetLib/VBoxNetLib.h"

#include <list>
#include <vector>
#include <map>
#include <string>

#include "Config.h"


const NullConfigEntity *g_NullConfig = new NullConfigEntity();
const RootConfigEntity *g_RootConfig = new RootConfigEntity(std::string("ROOT"));
const ClientMatchCriteria *g_AnyClient = new AnyClientMatchCriteria();

static SessionManager *g_SessionManager = SessionManager::getSessionManager();
static ConfigurationManager *g_ConfigurationManager = ConfigurationManager::getConfigurationManager();

static NetworkManager *g_NetworkManager = NetworkManager::getNetworkManager();

/* Client */

Client::Client(const RTMAC& mac, uint32_t xid)
{
    m_mac = mac;
    //    m_sessions.insert(Map2ClientSessionType(xid, Session(this, xid)));
}

/* Session */

bool Session::operator < (const Session& s) const
{
    return (m_u32Xid < s.m_u32Xid);
}

int Session::switchTo(CLIENTSESSIONSTATE enmState)
{
    m_state = enmState;
    return VINF_SUCCESS;
}

/* Configs
    NetworkConfigEntity(std::string name,
                        ConfigEntity* pCfg,
                        ClientMatchCriteria* criteria,
                        RTNETADDRIPV4& networkID,
                        RTNETADDRIPV4& networkMask)
*/
static const RTNETADDRIPV4 g_AnyIpv4 = {0};
static const RTNETADDRIPV4 g_AllIpv4 = {0xffffffff};
RootConfigEntity::RootConfigEntity(std::string name):
  NetworkConfigEntity(name, g_NullConfig, g_AnyClient, g_AnyIpv4, g_AllIpv4)
{
    m_MatchLevel = 2;
}

/* Session Manager */
SessionManager *SessionManager::getSessionManager()
{
    if (!g_SessionManager)
        g_SessionManager = new SessionManager();
    return g_SessionManager;
}

/*
 *  XXX: it sounds like a hack, we use packet descriptor to get the session,
 * instead use corresponding functions in NetworkManager to fetch client identification
 * (note: it isn't only mac driven) and XID for the session.
 */

/**
 * XXX: what about leases ... Lease is a committed Session....
 */
Session& SessionManager::getClientSessionByDhcpPacket(const RTNETBOOTP *pDhcpMsg, size_t cbDhcpMsg)
{

    VecClientIterator it;
    bool fDhcpValid = false;
    uint8_t uMsgType = 0;

    fDhcpValid = RTNetIPv4IsDHCPValid(NULL, pDhcpMsg, cbDhcpMsg, &uMsgType);
    Assert(fDhcpValid);

    /* 1st. client IDs */
    for ( it = m_clients.begin();
         it != m_clients.end();
         ++it)
    {
        /* OK. */
        if ((*it) == pDhcpMsg->bp_chaddr.Mac)
            break;
    }

    const uint32_t xid = pDhcpMsg->bp_xid;
    if (it == m_clients.end())
    {
        /* We hasn't got any session for this client */
        m_clients.push_back(Client(pDhcpMsg->bp_chaddr.Mac,
                                   pDhcpMsg->bp_xid));
        Client& client = m_clients.back();
        client.m_sessions.insert(Map2ClientSessionType(xid, Session(&client, xid)));
        Assert(client.m_sessions[xid].m_pClient);
        return client.m_sessions[xid];
    }

    Session& session = it->m_sessions[xid];
    session.m_pClient = &(*it);
    session.m_u32Xid = xid;

    RawOption opt;
    int rc;

    switch(uMsgType)
    {
        case RTNET_DHCP_MT_DISCOVER:
            session.switchTo(DHCPDISCOVERRECEIEVED);
            /* MAY */
            rc = ConfigurationManager::findOption(RTNET_DHCP_OPT_REQ_ADDR, pDhcpMsg, cbDhcpMsg, opt);
            if (RT_SUCCESS(rc))
            {
                /* hint for address allocation here */
                session.addressHint = *(PRTNETADDRIPV4)opt.au8RawOpt;
            }
            /* MAY: todo */
            rc = ConfigurationManager::findOption(RTNET_DHCP_OPT_LEASE_TIME, pDhcpMsg, cbDhcpMsg, opt);

            /* MAY: not now  */
            rc = ConfigurationManager::findOption(RTNET_DHCP_OPT_CLIENT_ID, pDhcpMsg, cbDhcpMsg, opt);
            /* XXX: MAY
            ConfigurationManager::findOption(RTNET_DHCP_OPT_VENDOR_CLASS_IDENTIFIER, pDhcpMsg, opt);
            */
            /* MAY: well */
            rc = ConfigurationManager::findOption(RTNET_DHCP_OPT_PARAM_REQ_LIST, pDhcpMsg, cbDhcpMsg, opt);
            if (RT_SUCCESS(rc))
                memcpy(&session.reqParamList, &opt, sizeof(RawOption));

            /* MAY: todo */
            rc = ConfigurationManager::findOption(RTNET_DHCP_OPT_MAX_DHCP_MSG_SIZE, pDhcpMsg, cbDhcpMsg, opt);

            break;

        case RTNET_DHCP_MT_REQUEST:
            session.switchTo(DHCPREQUESTRECEIVED);
            /* MUST in (SELECTING. INIT-REBOOT) MUST NOT BOUND. RENEW MAY  */
            rc = ConfigurationManager::findOption(RTNET_DHCP_OPT_REQ_ADDR, pDhcpMsg, cbDhcpMsg, opt);
            if (RT_SUCCESS(rc))
            {
                /* hint for address allocation here */
                session.addressHint = *(PRTNETADDRIPV4)opt.au8RawOpt;
            }

            /* MAY */
            ConfigurationManager::findOption(RTNET_DHCP_OPT_LEASE_TIME, pDhcpMsg, cbDhcpMsg, opt);
            /* MAY */
            ConfigurationManager::findOption(RTNET_DHCP_OPT_CLIENT_ID, pDhcpMsg, cbDhcpMsg, opt);
            /* XXX: MAY
            ConfigurationManager::findOption(RTNET_DHCP_OPT_VENDOR_CLASS_IDENTIFIER, pDhcpMsg, opt);
            */
            /* MAY */
            rc = ConfigurationManager::findOption(RTNET_DHCP_OPT_PARAM_REQ_LIST, pDhcpMsg, cbDhcpMsg, opt);
            if (RT_SUCCESS(rc))
                memcpy(&session.reqParamList, &opt, sizeof(RawOption));

            /* MAY */
            ConfigurationManager::findOption(RTNET_DHCP_OPT_MAX_DHCP_MSG_SIZE, pDhcpMsg, cbDhcpMsg, opt);

            break;

        case RTNET_DHCP_MT_DECLINE:
        case RTNET_DHCP_MT_OFFER:
        case RTNET_DHCP_MT_ACK:
        case RTNET_DHCP_MT_NAC:
        case RTNET_DHCP_MT_RELEASE:
        case RTNET_DHCP_MT_INFORM:
            AssertMsgFailed(("unimplemented"));
    }

    Assert(session.m_pClient);
    return session;
}

void SessionManager::releaseClientSession(Session& session)
{
}

void SessionManager::releaseClient(Client& client)
{
}


/* Configuration Manager */
ConfigurationManager *ConfigurationManager::getConfigurationManager()
{
    if (!g_ConfigurationManager)
        g_ConfigurationManager = new ConfigurationManager();

    return g_ConfigurationManager;
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
 *                          that this is adjusted if the option length is larger
 *                          than the message buffer.
 */
int
ConfigurationManager::findOption(uint8_t uOption, PCRTNETBOOTP pDhcpMsg, size_t cbDhcpMsg, RawOption& opt)
{
    Assert(uOption != RTNET_DHCP_OPT_PAD);

    /*
     * Validate the DHCP bits and figure the max size of the options in the vendor field.
     */
    if (cbDhcpMsg <= RT_UOFFSETOF(RTNETBOOTP, bp_vend.Dhcp.dhcp_opts))
        return VERR_INVALID_PARAMETER;

    if (pDhcpMsg->bp_vend.Dhcp.dhcp_cookie != RT_H2N_U32_C(RTNET_DHCP_COOKIE))
        return VERR_INVALID_PARAMETER;

    size_t cbLeft = cbDhcpMsg - RT_UOFFSETOF(RTNETBOOTP, bp_vend.Dhcp.dhcp_opts);
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
                opt.u8OptId = uCur;
                memcpy(opt.au8RawOpt, pb+2, cbCur);
                opt.cbRawOpt = cbCur;
                return VINF_SUCCESS;
            }
            pb     += cbCur + 2;
            cbLeft -= cbCur - 2;
        }
    }

    /** @todo search extended dhcp option field(s) when present */

    return VERR_NOT_FOUND;
}


/**
 * We've find the config for session ...
 * XXX: using Session's private members
 */
int ConfigurationManager::findConfiguration4Session(Session& session)
{
    /* XXX: state switching broken?
     * XXX: DHCPDECLINE and DHCPINFO should we support them.
     */
    AssertReturn(   session.m_state == DHCPDISCOVERRECEIEVED
                 || session.m_state == DHCPREQUESTRECEIVED, VERR_INTERNAL_ERROR);

    if (session.m_pCfg)
        return VINF_SUCCESS;

    Assert(session.m_pClient);
    if (g_RootConfig->match(*session.m_pClient, &session.m_pCfg) > 0)
        return VINF_SUCCESS;
    else
        return VERR_INTERNAL_ERROR; /* XXX: is it really *internal* error? Perhaps some misconfiguration */

}

/**
 * What we are archieveing here is non commited lease ()
 */
int ConfigurationManager::allocateConfiguration4Session(Session& session)
{
    /**
     * Well, session hasn't get the config.
     */
    AssertPtrReturn(session.m_pCfg, VERR_INTERNAL_ERROR);

    bool fWithAddressHint = (session.addressHint.u != 0);

    if (fWithAddressHint)
    {
        if (m_allocations[session].u == session.addressHint.u)
        {
            /* Good, our hint matches allocation  */
            return VINF_SUCCESS;
        }
        /**
         * This definetly depends on client state ...
         * AssertMsgFailed(("Debug Me"));
         * Workaround #1
         * clear and ignore.
         */
        fWithAddressHint = false;
        session.addressHint.u = 0;
    }

    /*
     * We've received DHCPDISCOVER
     */
    AssertReturn(session.m_state == DHCPDISCOVERRECEIEVED, VERR_INTERNAL_ERROR);

    /**
     * XXX: this is wrong allocation check...
     * session initilized by client shouldn't be equal to lease in STL terms.
     */
    MapSession2Ip4AddressIterator it = m_allocations.find(session);

    if (it == m_allocations.end())
    {
        /* XXX: not optimal allocation */
        const NetworkConfigEntity *pNetCfg = dynamic_cast<const NetworkConfigEntity *>(session.m_pCfg);

        /**
         * Check config class.
         */
        AssertPtrReturn(pNetCfg, VERR_INTERNAL_ERROR);

        uint32_t u32Address = RT_N2H_U32(pNetCfg->lowerIp().u);
        while (u32Address < RT_N2H_U32(pNetCfg->upperIp().u))
        {

            /* u32Address in host format */
            MapSession2Ip4AddressIterator addressIterator;
            bool fFound = false;

            for (addressIterator = m_allocations.begin();
                 addressIterator != m_allocations.end();
                 ++addressIterator)
            {
                if (RT_N2H_U32(addressIterator->second.u) == u32Address)
                {
                    /*
                     * This address is taken
                     * XXX: check if session isn't expired... if expired we can
                     * reuse it for this request
                     */
                    /* XXX: fTakeAddress = true; owning session is expired */
                    fFound = true;
                    break;
                }
            } /* end of for over allocations */

            if (!fFound)
            {
                RTNETADDRIPV4 address = { RT_H2N_U32_C(u32Address)};
                m_allocations.insert(MapSession2Ip4AddressPair(session, address));
                session.switchTo(DHCPOFFERPREPARED);
                return VINF_SUCCESS;
            }

            u32Address ++;
        } /* end of while over candidates */

    }

    /* XXX: really??? */
    session.switchTo(DHCPOFFERPREPARED);
    return VINF_SUCCESS;
}


int ConfigurationManager::commitConfiguration4ClientSession(Session& session)
{
    /**/
    session.switchTo(DHCPACKNAKPREPARED);

    /* XXX: clean up the rest of the session, now this session LEASE!!! */
    return VINF_SUCCESS;
}

RTNETADDRIPV4 ConfigurationManager::getSessionAddress(const Session& session)
{
    return m_allocations[session];
}


NetworkConfigEntity *ConfigurationManager::addNetwork(NetworkConfigEntity *pCfg,
                                    const RTNETADDRIPV4& networkId,
                                    const RTNETADDRIPV4& netmask,
                                    RTNETADDRIPV4& LowerAddress,
                                    RTNETADDRIPV4& UpperAddress)
{
        static int id;
        char name[64];

        RTStrPrintf(name, RT_ELEMENTS(name), "network-%d", id);
        std::string strname(name);
        id++;


        if (!LowerAddress.u)
            LowerAddress = networkId;

        if (!UpperAddress.u)
            UpperAddress.u = networkId.u | (~netmask.u);

        return new NetworkConfigEntity(strname,
                            g_RootConfig,
                            g_AnyClient,
                            5,
                            networkId,
                            netmask,
                            LowerAddress,
                            UpperAddress);
}

HostConfigEntity *ConfigurationManager::addHost(NetworkConfigEntity* pCfg,
                                                const RTNETADDRIPV4& address,
                                                ClientMatchCriteria *criteria)
{
    static int id;
    char name[64];

    RTStrPrintf(name, RT_ELEMENTS(name), "host-%d", id);
    std::string strname(name);
    id++;

    return new HostConfigEntity(address, strname, pCfg, criteria);
}

/**
 * Network manager
 */
NetworkManager *NetworkManager::getNetworkManager()
{
    if (!g_NetworkManager)
        g_NetworkManager = new NetworkManager();

    return g_NetworkManager;
}


/**
 * Network manager creates DHCPOFFER datagramm
 */
int NetworkManager::offer4Session(Session& session)
{
    AssertReturn(session.m_state == DHCPOFFERPREPARED, VERR_INTERNAL_ERROR);

    prepareReplyPacket4Session(session);

    RTNETADDRIPV4 address = ConfigurationManager::getConfigurationManager()->getSessionAddress(session);
    BootPReplyMsg.BootPHeader.bp_yiaddr =  address;

    /* Ubuntu ???*/
    BootPReplyMsg.BootPHeader.bp_ciaddr =  address;

    /* options:
     * - IP lease time
     * - message type
     * - server identifier
     */
    RawOption opt;
    RT_ZERO(opt);

    /* XXX: can't store options per session */
    Client *client = unconst(session.m_pClient);
    AssertPtr(client);

    opt.u8OptId = RTNET_DHCP_OPT_MSG_TYPE;
    opt.au8RawOpt[0] = RTNET_DHCP_MT_OFFER;
    opt.cbRawOpt = 1;
    client->rawOptions.push_back(opt);

    opt.u8OptId = RTNET_DHCP_OPT_LEASE_TIME;
    *(uint32_t *)opt.au8RawOpt = RT_H2N_U32(ConfigurationManager::getConfigurationManager()->getLeaseTime());
    opt.cbRawOpt = sizeof(RTNETADDRIPV4);
    client->rawOptions.push_back(opt);

    processParameterReqList(session);

    return doReply(session);
}


/**
 * Network manager creates DHCPACK
 */
int NetworkManager::ack(Session& session)
{
    RTNETADDRIPV4 address;

    AssertReturn(session.m_state == DHCPACKNAKPREPARED, VERR_INTERNAL_ERROR);
    prepareReplyPacket4Session(session);

    address = ConfigurationManager::getConfigurationManager()->getSessionAddress(session);
    BootPReplyMsg.BootPHeader.bp_ciaddr =  address;


    /* rfc2131 4.3.1 is about DHCPDISCOVER and this value is equal to ciaddr from
     * DHCPREQUEST or 0 ...
     * XXX: Using addressHint is not correct way to initialize [cy]iaddress...
     */
    BootPReplyMsg.BootPHeader.bp_ciaddr = session.addressHint;
    BootPReplyMsg.BootPHeader.bp_yiaddr = session.addressHint;

    Assert(BootPReplyMsg.BootPHeader.bp_yiaddr.u);

    /* options:
     * - IP address lease time (if DHCPREQUEST)
     * - message type
     * - server identifier
     */
    RawOption opt;
    RT_ZERO(opt);

    /* XXX: can't store options per session */
    Client *client = unconst(session.m_pClient);
    AssertPtr(client);

    opt.u8OptId = RTNET_DHCP_OPT_MSG_TYPE;
    opt.au8RawOpt[0] = RTNET_DHCP_MT_ACK;
    opt.cbRawOpt = 1;
    client->rawOptions.push_back(opt);

    /*
     * XXX: lease time should be conditional. If on dhcprequest then tim should be provided,
     * else on dhcpinform it mustn't.
     */
    opt.u8OptId = RTNET_DHCP_OPT_LEASE_TIME;
    *(uint32_t *)opt.au8RawOpt = 
      RT_H2N_U32(ConfigurationManager::getConfigurationManager()->getLeaseTime());
    opt.cbRawOpt = sizeof(RTNETADDRIPV4);
    client->rawOptions.push_back(opt);

    processParameterReqList(session);

    return doReply(session);

}


/**
 * Network manager creates DHCPNAK
 */
int NetworkManager::nak(Session& session)
{
    AssertReturn(session.m_state == DHCPACKNAKPREPARED, VERR_INTERNAL_ERROR);

    prepareReplyPacket4Session(session);

    /* this field filed in prepareReplyPacket4Session, and
     * RFC 2131 require to have it zero fo NAK.
     */
    BootPReplyMsg.BootPHeader.bp_yiaddr.u = 0;

    /* options:
     * - message type (if DHCPREQUEST)
     * - server identifier
     */
    RawOption opt;
    RT_ZERO(opt);

    /* XXX: can't store options per session */
    Client *client = unconst(session.m_pClient);
    AssertPtr(client);

    opt.u8OptId = RTNET_DHCP_OPT_MSG_TYPE;
    opt.au8RawOpt[0] = RTNET_DHCP_MT_NAC;
    opt.cbRawOpt = 1;
    client->rawOptions.push_back(opt);

    return doReply(session);
}


/**
 *
 */
int NetworkManager::prepareReplyPacket4Session(const Session& session)
{
    memset(&BootPReplyMsg, 0, sizeof(BootPReplyMsg));

    BootPReplyMsg.BootPHeader.bp_op     = RTNETBOOTP_OP_REPLY;
    BootPReplyMsg.BootPHeader.bp_htype  = RTNET_ARP_ETHER;
    BootPReplyMsg.BootPHeader.bp_hlen   = sizeof(RTMAC);
    BootPReplyMsg.BootPHeader.bp_hops   = 0;
    BootPReplyMsg.BootPHeader.bp_xid    = session.m_u32Xid;
    BootPReplyMsg.BootPHeader.bp_secs   = 0;
    /* XXX: bp_flags should be processed specially */
    BootPReplyMsg.BootPHeader.bp_flags  = 0;
    BootPReplyMsg.BootPHeader.bp_ciaddr.u = 0;
    BootPReplyMsg.BootPHeader.bp_giaddr.u = 0;

    Assert(session.m_pClient);
    BootPReplyMsg.BootPHeader.bp_chaddr.Mac = session.m_pClient->m_mac;

    BootPReplyMsg.BootPHeader.bp_yiaddr =
      ConfigurationManager::getConfigurationManager()->getSessionAddress(session);

    BootPReplyMsg.BootPHeader.bp_siaddr.u = 0;


    BootPReplyMsg.BootPHeader.bp_vend.Dhcp.dhcp_cookie = RT_H2N_U32_C(RTNET_DHCP_COOKIE);

    memset(&BootPReplyMsg.BootPHeader.bp_vend.Dhcp.dhcp_opts[0],
           '\0',
           RTNET_DHCP_OPT_SIZE);

    return VINF_SUCCESS;
}


int NetworkManager::doReply(const Session& session)
{
    int rc;

    /*
      Options....
     */
    VBoxNetDhcpWriteCursor Cursor(&BootPReplyMsg.BootPHeader, RTNET_DHCP_NORMAL_SIZE);

    /* XXX: unconst */
    Client *cl = unconst(session.m_pClient);
    AssertPtrReturn(cl, VERR_INTERNAL_ERROR);

    /* The basics */

    Cursor.optIPv4Addr(RTNET_DHCP_OPT_SERVER_ID, m_OurAddress);

    while(!cl->rawOptions.empty())
    {
        RawOption opt = cl->rawOptions.back();
        if (!Cursor.begin(opt.u8OptId, opt.cbRawOpt))
            break;
        Cursor.put(opt.au8RawOpt, opt.cbRawOpt);

        cl->rawOptions.pop_back();
    }


    if (!cl->rawOptions.empty())
    {
        Log(("Wasn't able to put all options\n"));
        /* force clean up */
        cl->rawOptions.clear();
    }

    Cursor.optEnd();

    switch (session.m_state)
    {
        case DHCPOFFERPREPARED:
            break;
        case DHCPACKNAKPREPARED:
            break;
        default:
            AssertMsgFailedReturn(("Unsupported state(%d)\n", session.m_state), VERR_INTERNAL_ERROR);
    }

    /*
     */
#if 0
    if (!(pDhcpMsg->bp_flags & RTNET_DHCP_FLAGS_NO_BROADCAST)) /** @todo need to see someone set this flag to check that it's correct. */
    {
        rc = VBoxNetUDPUnicast(m_pSession,
                               m_hIf,
                               m_pIfBuf,
                               m_OurAddress,
                               &m_OurMac,
                               RTNETIPV4_PORT_BOOTPS,                 /* sender */
                               IPv4AddrBrdCast,
                               &BootPReplyMsg.BootPHeader->bp_chaddr.Mac,
                               RTNETIPV4_PORT_BOOTPC,    /* receiver */
                               &BootPReplyMsg, cbBooPReplyMsg);
    }
    else
#endif
        rc = VBoxNetUDPBroadcast(m_pSession,
                                 m_hIf,
                                 m_pIfBuf,
                                 m_OurAddress,
                                 &m_OurMac,
                                 RTNETIPV4_PORT_BOOTPS,               /* sender */
                                 RTNETIPV4_PORT_BOOTPC,
                                 &BootPReplyMsg, RTNET_DHCP_NORMAL_SIZE);

    AssertRCReturn(rc,rc);

    return VINF_SUCCESS;
}


int NetworkManager::processParameterReqList(Session& session)
{
    /* request parameter list */
    RawOption opt;
    int idxParam = 0;

    uint8_t *pReqList = session.reqParamList.au8RawOpt;

    const NetworkConfigEntity *pNetCfg = dynamic_cast<const NetworkConfigEntity *>(session.m_pCfg);

    /* XXX: can't store options per session */
    Client *client = unconst(session.m_pClient);
    AssertPtr(client);


    for (idxParam = 0; idxParam < session.reqParamList.cbRawOpt; ++idxParam)
    {

        RT_ZERO(opt);
        opt.u8OptId = pReqList[idxParam];
        switch(pReqList[idxParam])
        {
            case RTNET_DHCP_OPT_SUBNET_MASK:
                ((PRTNETADDRIPV4)opt.au8RawOpt)->u = pNetCfg->netmask().u;
                opt.cbRawOpt = sizeof(RTNETADDRIPV4);
                client->rawOptions.push_back(opt);
                break;

            case RTNET_DHCP_OPT_ROUTERS:
                {
                    const Ipv4AddressContainer lst = g_ConfigurationManager->getAddressList(RTNET_DHCP_OPT_ROUTERS);
                    PRTNETADDRIPV4 pAddresses = (PRTNETADDRIPV4)&opt.au8RawOpt[0];

                    for (Ipv4AddressConstIterator it = lst.begin();
                         it != lst.end();
                         ++it)
                    {
                        *pAddresses = (*it);
                        pAddresses++;
                        opt.cbRawOpt += sizeof(RTNETADDRIPV4);
                    }

                    if (!lst.empty())
                        client->rawOptions.push_back(opt);
                }
                break;
            case RTNET_DHCP_OPT_DOMAIN_NAME:
                break;
            case RTNET_DHCP_OPT_DNS:
                break;
            default:
                Log(("opt: %d is ignored\n", pReqList[idxParam]));
                break;
        }
    }

    return VINF_SUCCESS;
}
