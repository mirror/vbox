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
RootConfigEntity *g_RootConfig = new RootConfigEntity(std::string("ROOT"), 1200 /* 20 min. */);
const ClientMatchCriteria *g_AnyClient = new AnyClientMatchCriteria();

static ConfigurationManager *g_ConfigurationManager = ConfigurationManager::getConfigurationManager();

static NetworkManager *g_NetworkManager = NetworkManager::getNetworkManager();

int BaseConfigEntity::match(Client& client, BaseConfigEntity **cfg)
{
        int iMatch = (m_criteria && m_criteria->check(client)? m_MatchLevel: 0);
        if (m_children.empty())
        {
            if (iMatch > 0)
            {
                *cfg = this;
                return iMatch;
            }
        }
        else
        {
            *cfg = this;
            /* XXX: hack */
            BaseConfigEntity *matching = this;
            int matchingLevel = m_MatchLevel;

            for (std::vector<BaseConfigEntity *>::iterator it = m_children.begin();
                 it != m_children.end();
                 ++it)
            {
                iMatch = (*it)->match(client, &matching);
                if (iMatch > matchingLevel)
                {
                    *cfg = matching;
                    matchingLevel = iMatch;
                }
            }
            return matchingLevel;
        }
        return iMatch;
}

/* Client */

Client::Client(const RTMAC& mac)
{
    m_mac = mac;
    m_lease = NULL;
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
RootConfigEntity::RootConfigEntity(std::string name, uint32_t expPeriod):
  NetworkConfigEntity(name, g_NullConfig, g_AnyClient, g_AnyIpv4, g_AllIpv4)
{
    m_MatchLevel = 2;
    m_u32ExpirationPeriod = expPeriod;
}


/* Configuration Manager */
ConfigurationManager *ConfigurationManager::getConfigurationManager()
{
    if (!g_ConfigurationManager)
        g_ConfigurationManager = new ConfigurationManager();

    return g_ConfigurationManager;
}


int ConfigurationManager::extractRequestList(PCRTNETBOOTP pDhcpMsg, size_t cbDhcpMsg, RawOption& rawOpt)
{
    return ConfigurationManager::findOption(RTNET_DHCP_OPT_PARAM_REQ_LIST, pDhcpMsg, cbDhcpMsg, rawOpt);
}


Client *ConfigurationManager::getClientByDhcpPacket(const RTNETBOOTP *pDhcpMsg, size_t cbDhcpMsg)
{

    VecClientIterator it;
    bool fDhcpValid = false;
    uint8_t uMsgType = 0;
    
    fDhcpValid = RTNetIPv4IsDHCPValid(NULL, pDhcpMsg, cbDhcpMsg, &uMsgType);
    AssertReturn(fDhcpValid, NULL);

    LogFlowFunc(("dhcp:mac:%RTmac\n", &pDhcpMsg->bp_chaddr.Mac));
    /* 1st. client IDs */
    for ( it = m_clients.begin();
         it != m_clients.end();
         ++it)
    {
        if (*(*it) == pDhcpMsg->bp_chaddr.Mac)
        {
            LogFlowFunc(("client:mac:%RTmac\n",  &(*it)->m_mac));
            /* check timestamp that request wasn't expired. */
            return (*it);
        }
    }

    if (it == m_clients.end())
    {
        /* We hasn't got any session for this client */
        m_clients.push_back(new Client(pDhcpMsg->bp_chaddr.Mac));
        return m_clients.back();
    }

    return NULL;
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
            size_t cbCur = pb[1];
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
 * We bind lease for client till it continue with it on DHCPREQUEST.
 */
Lease *ConfigurationManager::allocateLease4Client(Client *client, PCRTNETBOOTP pDhcpMsg, size_t cbDhcpMsg)
{
    /**
     * Well, session hasn't get the config.
     */
    AssertPtrReturn(client, NULL);
    
    /**
     * This mean that client has already bound or commited lease.
     * If we've it happens it means that we received DHCPDISCOVER twice. 
     */
    if (client->m_lease)
    {
        if (client->m_lease->isExpired())
            expireLease4Client(client);
        else
        {
            AssertReturn(client->m_lease->m_address.u != 0,NULL);
            return client->m_lease;
        }
    }

    RTNETADDRIPV4 hintAddress;
    RawOption opt;
    Lease *please = NULL;

    NetworkConfigEntity *pNetCfg;

    AssertReturn(g_RootConfig->match(*client, (BaseConfigEntity **)&pNetCfg) > 0, NULL);

    /* DHCPDISCOVER MAY contain request address */
    hintAddress.u = 0;
    int rc = findOption(RTNET_DHCP_OPT_REQ_ADDR, pDhcpMsg, cbDhcpMsg, opt);
    if (RT_SUCCESS(rc))
    {
        hintAddress.u = *(uint32_t *)opt.au8RawOpt;
        if (   RT_H2N_U32(hintAddress.u) < RT_H2N_U32(pNetCfg->lowerIp().u)
            || RT_H2N_U32(hintAddress.u) > RT_H2N_U32(pNetCfg->upperIp().u))
            hintAddress.u = 0; /* clear hint */
    }

    if (   hintAddress.u 
        && !isAddressTaken(hintAddress, NULL))
    {
        please = new Lease();
        please->pCfg = pNetCfg;
        please->m_client = client;
        client->m_lease = please;
        client->m_lease->m_address = hintAddress;
        m_allocations[please] = hintAddress;
        return please;
    }

    uint32_t u32 = 0;
    for(u32 = RT_H2N_U32(pNetCfg->lowerIp().u);
        u32 <= RT_H2N_U32(pNetCfg->upperIp().u);
        ++u32)
    {
        RTNETADDRIPV4 address;
        address.u = RT_H2N_U32(u32);
        if (!isAddressTaken(address, NULL))
        {
            please = new Lease();
            please->pCfg = pNetCfg;
            please->m_client = client;
            client->m_lease = please;
            client->m_lease->m_address = address;
            m_allocations[please] = client->m_lease->m_address;
            return please;
        }
    }
   
    return NULL;
}


int ConfigurationManager::commitLease4Client(Client *client)
{
    client->m_lease->u64TimestampBindingStarted = 0;
    client->m_lease->u32LeaseExpirationPeriod = client->m_lease->pCfg->expirationPeriod();
    client->m_lease->u64TimestampLeasingStarted = RTTimeMilliTS();
    client->m_lease->fBinding = false;
    return VINF_SUCCESS;
}

int ConfigurationManager::expireLease4Client(Client *client)
{
    MapLease2Ip4AddressIterator it = m_allocations.find(client->m_lease);
    AssertReturn(it != m_allocations.end(), VERR_NOT_FOUND);

    m_allocations.erase(it);

    delete client->m_lease;
    client->m_lease = NULL;
    
    return VINF_SUCCESS;
}

bool ConfigurationManager::isAddressTaken(const RTNETADDRIPV4& addr, Lease** ppLease)
{
    MapLease2Ip4AddressIterator it;
    
    for (it = m_allocations.begin();
         it != m_allocations.end();
         ++it)
    {
        if (it->second.u == addr.u)
        {
            if (ppLease)
                *ppLease = it->first;
            
            return true;
        }
    }
    return false;
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

int ConfigurationManager::addToAddressList(uint8_t u8OptId, RTNETADDRIPV4& address)
{
    switch(u8OptId)
    {
        case RTNET_DHCP_OPT_DNS:
            m_nameservers.push_back(address);
            break;
        case RTNET_DHCP_OPT_ROUTERS:
            m_routers.push_back(address);
            break;
        default:
            Log(("dhcp-opt: list (%d) unsupported\n", u8OptId));
    }
    return VINF_SUCCESS;
}

int ConfigurationManager::flushAddressList(uint8_t u8OptId)
{
    switch(u8OptId)
    {
        case RTNET_DHCP_OPT_DNS:
            m_nameservers.clear();
            break;
        case RTNET_DHCP_OPT_ROUTERS:
            m_routers.clear();
            break;
        default:
            Log(("dhcp-opt: list (%d) unsupported\n", u8OptId));
    }
    return VINF_SUCCESS;
}

const Ipv4AddressContainer& ConfigurationManager::getAddressList(uint8_t u8OptId)
{
    switch(u8OptId)
    {
        case RTNET_DHCP_OPT_DNS:
            return m_nameservers;

        case RTNET_DHCP_OPT_ROUTERS:
            return m_routers;

    }
    /* XXX: Grrr !!! */
    return m_empty;
}

int ConfigurationManager::setString(uint8_t u8OptId, const std::string& str)
{
    switch (u8OptId)
    {
        case RTNET_DHCP_OPT_DOMAIN_NAME:
            m_domainName = str;
            break;
        default:
            break;
    }

    return VINF_SUCCESS;
}

const std::string& ConfigurationManager::getString(uint8_t u8OptId)
{
    switch (u8OptId)
    {
        case RTNET_DHCP_OPT_DOMAIN_NAME:
            if (m_domainName.length())
                return m_domainName;
            else
                return m_noString;
        default:
            break;
    }

    return m_noString;
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
int NetworkManager::offer4Client(Client *client, uint32_t u32Xid, 
                                 uint8_t *pu8ReqList, int cReqList)
{
    AssertPtrReturn(client, VERR_INTERNAL_ERROR);
    AssertPtrReturn(client->m_lease, VERR_INTERNAL_ERROR);

    prepareReplyPacket4Client(client, u32Xid);

    
    RTNETADDRIPV4 address = client->m_lease->m_address;
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
    AssertPtr(client);

    opt.u8OptId = RTNET_DHCP_OPT_MSG_TYPE;
    opt.au8RawOpt[0] = RTNET_DHCP_MT_OFFER;
    opt.cbRawOpt = 1;
    client->rawOptions.push_back(opt);

    opt.u8OptId = RTNET_DHCP_OPT_LEASE_TIME;
    *(uint32_t *)opt.au8RawOpt = RT_H2N_U32(client->m_lease->pCfg->expirationPeriod());
    opt.cbRawOpt = sizeof(RTNETADDRIPV4);
    client->rawOptions.push_back(opt);

    processParameterReqList(client, pu8ReqList, cReqList);

    return doReply(client);
}


/**
 * Network manager creates DHCPACK
 */
int NetworkManager::ack(Client *client, uint32_t u32Xid,
                        uint8_t *pu8ReqList, int cReqList)
{
    AssertPtrReturn(client, VERR_INTERNAL_ERROR);
    AssertPtrReturn(client->m_lease, VERR_INTERNAL_ERROR);

    RTNETADDRIPV4 address;

    prepareReplyPacket4Client(client, u32Xid);

    address = client->m_lease->m_address;
    BootPReplyMsg.BootPHeader.bp_ciaddr =  address;


    /* rfc2131 4.3.1 is about DHCPDISCOVER and this value is equal to ciaddr from
     * DHCPREQUEST or 0 ...
     * XXX: Using addressHint is not correct way to initialize [cy]iaddress...
     */
    BootPReplyMsg.BootPHeader.bp_ciaddr = client->m_lease->m_address;
    BootPReplyMsg.BootPHeader.bp_yiaddr = client->m_lease->m_address;

    Assert(BootPReplyMsg.BootPHeader.bp_yiaddr.u);

    /* options:
     * - IP address lease time (if DHCPREQUEST)
     * - message type
     * - server identifier
     */
    RawOption opt;
    RT_ZERO(opt);

    opt.u8OptId = RTNET_DHCP_OPT_MSG_TYPE;
    opt.au8RawOpt[0] = RTNET_DHCP_MT_ACK;
    opt.cbRawOpt = 1;
    client->rawOptions.push_back(opt);

    /*
     * XXX: lease time should be conditional. If on dhcprequest then tim should be provided,
     * else on dhcpinform it mustn't.
     */
    opt.u8OptId = RTNET_DHCP_OPT_LEASE_TIME;
    *(uint32_t *)opt.au8RawOpt = RT_H2N_U32(client->m_lease->u32LeaseExpirationPeriod);
    opt.cbRawOpt = sizeof(RTNETADDRIPV4);
    client->rawOptions.push_back(opt);

    processParameterReqList(client, pu8ReqList, cReqList);

    return doReply(client);
}


/**
 * Network manager creates DHCPNAK
 */
int NetworkManager::nak(Client* client, uint32_t u32Xid)
{
    AssertPtrReturn(client, VERR_INTERNAL_ERROR);
    AssertPtrReturn(client->m_lease, VERR_INTERNAL_ERROR);

    prepareReplyPacket4Client(client, u32Xid);

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

    opt.u8OptId = RTNET_DHCP_OPT_MSG_TYPE;
    opt.au8RawOpt[0] = RTNET_DHCP_MT_NAC;
    opt.cbRawOpt = 1;
    client->rawOptions.push_back(opt);

    return doReply(client);
}


/**
 *
 */
int NetworkManager::prepareReplyPacket4Client(Client *client, uint32_t u32Xid)
{
    AssertPtrReturn(client, VERR_INTERNAL_ERROR);
    AssertPtrReturn(client->m_lease, VERR_INTERNAL_ERROR);

    memset(&BootPReplyMsg, 0, sizeof(BootPReplyMsg));

    BootPReplyMsg.BootPHeader.bp_op     = RTNETBOOTP_OP_REPLY;
    BootPReplyMsg.BootPHeader.bp_htype  = RTNET_ARP_ETHER;
    BootPReplyMsg.BootPHeader.bp_hlen   = sizeof(RTMAC);
    BootPReplyMsg.BootPHeader.bp_hops   = 0;
    BootPReplyMsg.BootPHeader.bp_xid    = u32Xid;
    BootPReplyMsg.BootPHeader.bp_secs   = 0;
    /* XXX: bp_flags should be processed specially */
    BootPReplyMsg.BootPHeader.bp_flags  = 0;
    BootPReplyMsg.BootPHeader.bp_ciaddr.u = 0;
    BootPReplyMsg.BootPHeader.bp_giaddr.u = 0;

    BootPReplyMsg.BootPHeader.bp_chaddr.Mac = client->m_mac;

    BootPReplyMsg.BootPHeader.bp_yiaddr = client->m_lease->m_address;
    BootPReplyMsg.BootPHeader.bp_siaddr.u = 0;


    BootPReplyMsg.BootPHeader.bp_vend.Dhcp.dhcp_cookie = RT_H2N_U32_C(RTNET_DHCP_COOKIE);

    memset(&BootPReplyMsg.BootPHeader.bp_vend.Dhcp.dhcp_opts[0],
           '\0',
           RTNET_DHCP_OPT_SIZE);

    return VINF_SUCCESS;
}


int NetworkManager::doReply(Client *client)
{
    int rc;

    AssertPtrReturn(client, VERR_INTERNAL_ERROR);
    AssertPtrReturn(client->m_lease, VERR_INTERNAL_ERROR);

    /*
      Options....
     */
    VBoxNetDhcpWriteCursor Cursor(&BootPReplyMsg.BootPHeader, RTNET_DHCP_NORMAL_SIZE);

    /* The basics */

    Cursor.optIPv4Addr(RTNET_DHCP_OPT_SERVER_ID, m_OurAddress);

    while(!client->rawOptions.empty())
    {
        RawOption opt = client->rawOptions.back();
        if (!Cursor.begin(opt.u8OptId, opt.cbRawOpt))
            break;
        Cursor.put(opt.au8RawOpt, opt.cbRawOpt);

        client->rawOptions.pop_back();
    }


    if (!client->rawOptions.empty())
    {
        Log(("Wasn't able to put all options\n"));
        /* force clean up */
        client->rawOptions.clear();
    }

    Cursor.optEnd();

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


int NetworkManager::processParameterReqList(Client* client, uint8_t *pu8ReqList, int cReqList)
{
    /* request parameter list */
    RawOption opt;
    int idxParam = 0;

    AssertPtrReturn(client, VERR_INTERNAL_ERROR);

    uint8_t *pReqList = pu8ReqList;

    const NetworkConfigEntity *pNetCfg = client->m_lease->pCfg;

    for (idxParam = 0; idxParam < cReqList; ++idxParam)
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
            case RTNET_DHCP_OPT_DNS:
                {
                    const Ipv4AddressContainer lst = 
                      g_ConfigurationManager->getAddressList(pReqList[idxParam]);
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
                {
                    std::string domainName = g_ConfigurationManager->getString(pReqList[idxParam]);
                    if (domainName == g_ConfigurationManager->m_noString)
                        break;

                    char *pszDomainName = (char *)&opt.au8RawOpt[0];

                    strcpy(pszDomainName, domainName.c_str());
                    opt.cbRawOpt = domainName.length();
                    client->rawOptions.push_back(opt);
                }
                break;
            default:
                Log(("opt: %d is ignored\n", pReqList[idxParam]));
                break;
        }
    }

    return VINF_SUCCESS;
}
