/* $Id$ */

/**
 * XXX: license.
 */

#include <iprt/asm.h>
#include <iprt/net.h>
#include <iprt/time.h>

#include <VBox/sup.h>
#include <VBox/intnet.h>
#include <VBox/intnetinline.h>
#include <VBox/vmm/vmm.h>
#include <VBox/version.h>

#include <VBox/com/string.h>

#include <iprt/cpp/xml.h>

#include "../NetLib/VBoxNetLib.h"
#include "../NetLib/shared_ptr.h"

#include <list>
#include <vector>
#include <map>
#include <string>

#include "Config.h"

/* types */
class ClientData
{
public:
    ClientData()
    {
        m_address.u = 0;
        m_network.u = 0;
        fHasLease = false;
        fHasClient = false;
        fBinding = true;
        u64TimestampBindingStarted = 0;
        u64TimestampLeasingStarted = 0;
        u32LeaseExpirationPeriod = 0;
        u32BindExpirationPeriod = 0;
        pCfg = NULL;

    }
    ~ClientData(){}
    
    /* client information */
    RTNETADDRIPV4 m_address;
    RTNETADDRIPV4 m_network;
    RTMAC m_mac;
    
    bool fHasClient;

    /* Lease part */
    bool fHasLease; 
    /** lease isn't commited */
    bool fBinding;

    /** Timestamp when lease commited. */
    uint64_t u64TimestampLeasingStarted;
    /** Period when lease is expired in secs. */
    uint32_t u32LeaseExpirationPeriod;

    /** timestamp when lease was bound */
    uint64_t u64TimestampBindingStarted;
    /* Period when binding is expired in secs. */
    uint32_t u32BindExpirationPeriod;

    MapOptionId2RawOption options;

    NetworkConfigEntity *pCfg;
};


bool operator== (const Lease& lhs, const Lease& rhs)
{
    return (lhs.m.get() == rhs.m.get());
}


bool operator!= (const Lease& lhs, const Lease& rhs)
{
    return !(lhs == rhs);
}


bool operator< (const Lease& lhs, const Lease& rhs)
{
    return (   (lhs.getAddress() < rhs.getAddress())
            || (lhs.issued() < rhs.issued()));
}
/* consts */

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
struct ConfigurationManager::Data
{
    Data():fFileExists(false){}

    MapLease2Ip4Address  m_allocations;
    Ipv4AddressContainer m_nameservers;
    Ipv4AddressContainer m_routers;

    std::string          m_domainName;
    VecClient            m_clients;
    std::string          m_leaseStorageFilename;
    bool                 fFileExists;
};
    
ConfigurationManager *ConfigurationManager::getConfigurationManager()
{
    if (!g_ConfigurationManager)


    {
        g_ConfigurationManager = new ConfigurationManager();
        g_ConfigurationManager->init();
    }

    return g_ConfigurationManager;
}


const std::string tagXMLLeases = "Leases";
const std::string tagXMLLeasesAttributeVersion = "version";
const std::string tagXMLLeasesVersion_1_0 = "1.0";
const std::string tagXMLLease = "Lease";
const std::string tagXMLLeaseAttributeMac = "mac";
const std::string tagXMLLeaseAttributeNetwork = "network";
const std::string tagXMLLeaseAddress = "Address";
const std::string tagXMLAddressAttributeValue = "value";
const std::string tagXMLLeaseTime = "Time";
const std::string tagXMLTimeAttributeIssued = "issued";
const std::string tagXMLTimeAttributeExpiration = "expiration";
const std::string tagXMLLeaseOptions = "Options";

/**
 * <Leases version="1.0">
 *   <Lease mac="" network=""/>
 *    <Address value=""/>
 *    <Time issued="" expiration=""/>
 *    <options>
 *      <option name="" type=""/>
 *      </option>
 *    </options>
 *   </Lease>
 * </Leases>
 */
int ConfigurationManager::loadFromFile(const std::string& leaseStorageFileName)
{
    m->m_leaseStorageFilename = leaseStorageFileName;
    
    xml::XmlFileParser parser;
    xml::Document doc;

    try {
        parser.read(m->m_leaseStorageFilename.c_str(), doc);
    }
    catch (...)
    {
        return VINF_SUCCESS;
    }

    /* XML parsing */
    xml::ElementNode *root = doc.getRootElement();

    if (!root || !root->nameEquals(tagXMLLeases.c_str()))
    {
        m->fFileExists = false;
        return VERR_NOT_FOUND;
    }

    com::Utf8Str version;
    if (root)
        root->getAttributeValue(tagXMLLeasesAttributeVersion.c_str(), version);

    /* XXX: version check */
    xml::NodesLoop leases(*root);

    bool valueExists;
    const xml::ElementNode *lease;
    while ((lease = leases.forAllNodes()))
    {
        if (!lease->nameEquals(tagXMLLease.c_str()))
            continue;
        
        ClientData *data = new ClientData();
        Lease l(data);
        if (l.fromXML(lease)) 
        {

            m->m_allocations.insert(MapLease2Ip4AddressPair(l, l.getAddress()));


            NetworkConfigEntity *pNetCfg = NULL;
            Client c(data);
            int rc = g_RootConfig->match(c, (BaseConfigEntity **)&pNetCfg);
            Assert(rc >= 0 && pNetCfg);

            l.setConfig(pNetCfg);

            m->m_clients.push_back(c);
        }
    }

    return VINF_SUCCESS;
}


int ConfigurationManager::saveToFile()
{
    if (m->m_leaseStorageFilename.empty())
        return VINF_SUCCESS;

    xml::Document doc;

    xml::ElementNode *root = doc.createRootElement(tagXMLLeases.c_str());
    if (!root)
        return VERR_INTERNAL_ERROR;
    
    root->setAttribute(tagXMLLeasesAttributeVersion.c_str(), tagXMLLeasesVersion_1_0.c_str());

    for(MapLease2Ip4AddressConstIterator it = m->m_allocations.begin();
        it != m->m_allocations.end(); ++it)
    {
        xml::ElementNode *lease = root->createChild(tagXMLLease.c_str());
        if (!it->first.toXML(lease))
        {
            /* XXX: todo logging + error handling */
        }
    }

    try {
        xml::XmlFileWriter writer(doc);
        writer.write(m->m_leaseStorageFilename.c_str(), true);
    } catch(...){}

    return VINF_SUCCESS;
}


int ConfigurationManager::extractRequestList(PCRTNETBOOTP pDhcpMsg, size_t cbDhcpMsg, RawOption& rawOpt)
{
    return ConfigurationManager::findOption(RTNET_DHCP_OPT_PARAM_REQ_LIST, pDhcpMsg, cbDhcpMsg, rawOpt);
}


Client ConfigurationManager::getClientByDhcpPacket(const RTNETBOOTP *pDhcpMsg, size_t cbDhcpMsg)
{

    VecClientIterator it;
    bool fDhcpValid = false;
    uint8_t uMsgType = 0;

    fDhcpValid = RTNetIPv4IsDHCPValid(NULL, pDhcpMsg, cbDhcpMsg, &uMsgType);
    AssertReturn(fDhcpValid, Client::NullClient);

    LogFlowFunc(("dhcp:mac:%RTmac\n", &pDhcpMsg->bp_chaddr.Mac));
    /* 1st. client IDs */
    for ( it = m->m_clients.begin();
         it != m->m_clients.end();
         ++it)
    {
        if ((*it) == pDhcpMsg->bp_chaddr.Mac)
        {
            LogFlowFunc(("client:mac:%RTmac\n",  it->getMacAddress()));
            /* check timestamp that request wasn't expired. */
            return (*it);
        }
    }

    if (it == m->m_clients.end())
    {
        /* We hasn't got any session for this client */
        Client c;
        c.initWithMac(pDhcpMsg->bp_chaddr.Mac);
        m->m_clients.push_back(c);
        return m->m_clients.back();
    }

    return Client::NullClient;
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
Lease ConfigurationManager::allocateLease4Client(const Client& client, PCRTNETBOOTP pDhcpMsg, size_t cbDhcpMsg)
{
    {
        /**
         * This mean that client has already bound or commited lease.
         * If we've it happens it means that we received DHCPDISCOVER twice.
         */
        const Lease l = client.lease();
        if (l != Lease::NullLease)
        {
            /* Here we should take lease from the m_allocation which was feed with leases 
             *  on start
             */
            if (l.isExpired())
            {
                expireLease4Client(const_cast<Client&>(client));
                if (!l.isExpired())
                    return l;
            }
            else
            {
                AssertReturn(l.getAddress().u != 0, Lease::NullLease);
                return l;
            }
        }
    }

    RTNETADDRIPV4 hintAddress;
    RawOption opt;
    NetworkConfigEntity *pNetCfg;

    Client cl(client);
    AssertReturn(g_RootConfig->match(cl, (BaseConfigEntity **)&pNetCfg) > 0, Lease::NullLease);

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
        && !isAddressTaken(hintAddress))
    {
        Lease l(cl);
        l.setConfig(pNetCfg);
        l.setAddress(hintAddress);
        m->m_allocations.insert(MapLease2Ip4AddressPair(l, hintAddress));
        return l;
    }

    uint32_t u32 = 0;
    for(u32 = RT_H2N_U32(pNetCfg->lowerIp().u);
        u32 <= RT_H2N_U32(pNetCfg->upperIp().u);
        ++u32)
    {
        RTNETADDRIPV4 address;
        address.u = RT_H2N_U32(u32);
        if (!isAddressTaken(address))
        {
            Lease l(cl);
            l.setConfig(pNetCfg);
            l.setAddress(address);
            m->m_allocations.insert(MapLease2Ip4AddressPair(l, address));
            return l;
        }
    }

    return Lease::NullLease;
}


int ConfigurationManager::commitLease4Client(Client& client)
{
    Lease l = client.lease();
    AssertReturn(l != Lease::NullLease, VERR_INTERNAL_ERROR);

    l.bindingPhase(false);
    const NetworkConfigEntity *pCfg = l.getConfig();

    AssertPtr(pCfg);
    l.setExpiration(pCfg->expirationPeriod());
    l.phaseStart(RTTimeMilliTS());

    saveToFile();

    return VINF_SUCCESS;
}


int ConfigurationManager::expireLease4Client(Client& client)
{
    Lease l = client.lease();
    AssertReturn(l != Lease::NullLease, VERR_INTERNAL_ERROR);
    
    if (l.isInBindingPhase())
    {

        MapLease2Ip4AddressIterator it = m->m_allocations.find(l);
        AssertReturn(it != m->m_allocations.end(), VERR_NOT_FOUND);

        /*
         * XXX: perhaps it better to keep this allocation ????
         */
        m->m_allocations.erase(it);

        l.expire();
        return VINF_SUCCESS;
    }
    
    l = Lease(client); /* re-new */
    return VINF_SUCCESS;
}

bool ConfigurationManager::isAddressTaken(const RTNETADDRIPV4& addr, Lease& lease)
{
    MapLease2Ip4AddressIterator it;

    for (it = m->m_allocations.begin();
         it != m->m_allocations.end();
         ++it)
    {
        if (it->second.u == addr.u)
        {
            if (lease != Lease::NullLease)
                lease = it->first;

            return true;
        }
    }
    lease = Lease::NullLease;
    return false;
}


NetworkConfigEntity *ConfigurationManager::addNetwork(NetworkConfigEntity *,
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
            m->m_nameservers.push_back(address);
            break;
        case RTNET_DHCP_OPT_ROUTERS:
            m->m_routers.push_back(address);
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
            m->m_nameservers.clear();
            break;
        case RTNET_DHCP_OPT_ROUTERS:
            m->m_routers.clear();
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
            return m->m_nameservers;

        case RTNET_DHCP_OPT_ROUTERS:
            return m->m_routers;

    }
    /* XXX: Grrr !!! */
    return m_empty;
}


int ConfigurationManager::setString(uint8_t u8OptId, const std::string& str)
{
    switch (u8OptId)
    {
        case RTNET_DHCP_OPT_DOMAIN_NAME:
            m->m_domainName = str;
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
            if (m->m_domainName.length())
                return m->m_domainName;
            else
                return m_noString;
        default:
            break;
    }

    return m_noString;
}


void ConfigurationManager::init()
{
    m = new ConfigurationManager::Data();
}


ConfigurationManager::~ConfigurationManager() { if (m) delete m; }

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
int NetworkManager::offer4Client(const Client& client, uint32_t u32Xid,
                                 uint8_t *pu8ReqList, int cReqList)
{
    Lease l(client); /* XXX: oh, it looks badly, but now we have lease */
    prepareReplyPacket4Client(client, u32Xid);

    RTNETADDRIPV4 address = l.getAddress();
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

    std::vector<RawOption> extra(2);
    opt.u8OptId = RTNET_DHCP_OPT_MSG_TYPE;
    opt.au8RawOpt[0] = RTNET_DHCP_MT_OFFER;
    opt.cbRawOpt = 1;
    extra.push_back(opt);

    opt.u8OptId = RTNET_DHCP_OPT_LEASE_TIME;

    const NetworkConfigEntity *pCfg = l.getConfig();
    AssertPtr(pCfg);

    *(uint32_t *)opt.au8RawOpt = RT_H2N_U32(pCfg->expirationPeriod());
    opt.cbRawOpt = sizeof(RTNETADDRIPV4);

    extra.push_back(opt);

    processParameterReqList(client, pu8ReqList, cReqList);

    return doReply(client, extra);
}


/**
 * Network manager creates DHCPACK
 */
int NetworkManager::ack(const Client& client, uint32_t u32Xid,
                        uint8_t *pu8ReqList, int cReqList)
{
    RTNETADDRIPV4 address;

    prepareReplyPacket4Client(client, u32Xid);
    
    Lease l = client.lease();
    address = l.getAddress();
    BootPReplyMsg.BootPHeader.bp_ciaddr =  address;


    /* rfc2131 4.3.1 is about DHCPDISCOVER and this value is equal to ciaddr from
     * DHCPREQUEST or 0 ...
     * XXX: Using addressHint is not correct way to initialize [cy]iaddress...
     */
    BootPReplyMsg.BootPHeader.bp_ciaddr = address;
    BootPReplyMsg.BootPHeader.bp_yiaddr = address;

    Assert(BootPReplyMsg.BootPHeader.bp_yiaddr.u);

    /* options:
     * - IP address lease time (if DHCPREQUEST)
     * - message type
     * - server identifier
     */
    RawOption opt;
    RT_ZERO(opt);

    std::vector<RawOption> extra(2);
    opt.u8OptId = RTNET_DHCP_OPT_MSG_TYPE;
    opt.au8RawOpt[0] = RTNET_DHCP_MT_ACK;
    opt.cbRawOpt = 1;
    extra.push_back(opt);

    /*
     * XXX: lease time should be conditional. If on dhcprequest then tim should be provided,
     * else on dhcpinform it mustn't.
     */
    opt.u8OptId = RTNET_DHCP_OPT_LEASE_TIME;
    *(uint32_t *)opt.au8RawOpt = RT_H2N_U32(l.getExpiration());
    opt.cbRawOpt = sizeof(RTNETADDRIPV4);
    extra.push_back(opt);

    processParameterReqList(client, pu8ReqList, cReqList);

    return doReply(client, extra);
}


/**
 * Network manager creates DHCPNAK
 */
int NetworkManager::nak(const Client& client, uint32_t u32Xid)
{

    Lease l = client.lease();
    if (l == Lease::NullLease)
        return VERR_INTERNAL_ERROR;

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
    std::vector<RawOption> extra;

    opt.u8OptId = RTNET_DHCP_OPT_MSG_TYPE;
    opt.au8RawOpt[0] = RTNET_DHCP_MT_NAC;
    opt.cbRawOpt = 1;
    extra.push_back(opt);

    return doReply(client, extra);
}


/**
 *
 */
int NetworkManager::prepareReplyPacket4Client(const Client& client, uint32_t u32Xid)
{
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

    BootPReplyMsg.BootPHeader.bp_chaddr.Mac = client.getMacAddress();

    const Lease l = client.lease();
    BootPReplyMsg.BootPHeader.bp_yiaddr = l.getAddress();
    BootPReplyMsg.BootPHeader.bp_siaddr.u = 0;


    BootPReplyMsg.BootPHeader.bp_vend.Dhcp.dhcp_cookie = RT_H2N_U32_C(RTNET_DHCP_COOKIE);

    memset(&BootPReplyMsg.BootPHeader.bp_vend.Dhcp.dhcp_opts[0],
           '\0',
           RTNET_DHCP_OPT_SIZE);

    return VINF_SUCCESS;
}


int NetworkManager::doReply(const Client& client, const std::vector<RawOption>& extra)
{
    int rc;

    /*
      Options....
     */
    VBoxNetDhcpWriteCursor Cursor(&BootPReplyMsg.BootPHeader, RTNET_DHCP_NORMAL_SIZE);

    /* The basics */

    Cursor.optIPv4Addr(RTNET_DHCP_OPT_SERVER_ID, m_OurAddress);

    const Lease l = client.lease(); 
    const std::map<uint8_t, RawOption>& options = l.options();

    for(std::vector<RawOption>::const_iterator it = extra.begin(); 
        it != extra.end(); ++it)
    {
        if (!Cursor.begin(it->u8OptId, it->cbRawOpt))
            break;
        Cursor.put(it->au8RawOpt, it->cbRawOpt);

    }

    for(std::map<uint8_t, RawOption>::const_iterator it = options.begin(); 
        it != options.end(); ++it)
    {
        if (!Cursor.begin(it->second.u8OptId, it->second.cbRawOpt))
            break;
        Cursor.put(it->second.au8RawOpt, it->second.cbRawOpt);

    }

    Cursor.optEnd();

    /*
     */
#if 0
    /** @todo need to see someone set this flag to check that it's correct. */
    if (!(pDhcpMsg->bp_flags & RTNET_DHCP_FLAGS_NO_BROADCAST))  
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


int NetworkManager::processParameterReqList(const Client& client, uint8_t *pu8ReqList, int cReqList)
{
    /* request parameter list */
    RawOption opt;
    int idxParam = 0;

    uint8_t *pReqList = pu8ReqList;
    
    const Lease const_l = client.lease();
    Lease l = Lease(const_l);
    
    const NetworkConfigEntity *pNetCfg = l.getConfig();

    for (idxParam = 0; idxParam < cReqList; ++idxParam)
    {

        bool fIgnore = false;
        RT_ZERO(opt);
        opt.u8OptId = pReqList[idxParam];

        switch(pReqList[idxParam])
        {
            case RTNET_DHCP_OPT_SUBNET_MASK:
                ((PRTNETADDRIPV4)opt.au8RawOpt)->u = pNetCfg->netmask().u;
                opt.cbRawOpt = sizeof(RTNETADDRIPV4);

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

                    if (lst.empty())
                        fIgnore = true;
                }
                break;
            case RTNET_DHCP_OPT_DOMAIN_NAME:
                {
                    std::string domainName = g_ConfigurationManager->getString(pReqList[idxParam]);
                    if (domainName == g_ConfigurationManager->m_noString)
                    {
                        fIgnore = true;
                        break;
                    }

                    char *pszDomainName = (char *)&opt.au8RawOpt[0];

                    strcpy(pszDomainName, domainName.c_str());
                    opt.cbRawOpt = domainName.length();
                }
                break;
            default:
                Log(("opt: %d is ignored\n", pReqList[idxParam]));
                fIgnore = true;
                break;
        }

        if (!fIgnore)
            l.options().insert(std::map<uint8_t, RawOption>::value_type(opt.u8OptId, opt));

    }

    return VINF_SUCCESS;
}

/* Utility */
bool operator== (const RTMAC& lhs, const RTMAC& rhs)
{
    return (   lhs.au16[0] == rhs.au16[0]
            && lhs.au16[1] == rhs.au16[1]
            && lhs.au16[2] == rhs.au16[2]);
}


/* Client */
Client::Client()
{
    m = SharedPtr<ClientData>();
}


void Client::initWithMac(const RTMAC& mac)
{
    m = SharedPtr<ClientData>(new ClientData());
    m->m_mac = mac;
}


bool Client::operator== (const RTMAC& mac) const
{
    return (m.get() && m->m_mac == mac);
}


const RTMAC& Client::getMacAddress() const
{
    return m->m_mac;
}


Lease Client::lease()
{
    if (!m.get()) return Lease::NullLease;

    if (m->fHasLease)
        return Lease(*this);
    else
        return Lease::NullLease;
}


const Lease Client::lease() const
{
    return const_cast<Client *>(this)->lease();
}


Client::Client(ClientData *data):m(SharedPtr<ClientData>(data)){}

/* Lease */
Lease::Lease()
{
    m = SharedPtr<ClientData>();
}


Lease::Lease (const Client& c)
{
    m = SharedPtr<ClientData>(c.m);
    if (   !m->fHasLease
        || (   isExpired()
            && !isInBindingPhase()))
    {
        m->fHasLease = true;
        m->fBinding = true;
        phaseStart(RTTimeMilliTS());
    }
}


bool Lease::isExpired() const
{
    AssertPtrReturn(m.get(), false);

    if (!m->fBinding)
        return (ASMDivU64ByU32RetU32(RTTimeMilliTS() - m->u64TimestampLeasingStarted, 1000)
                > m->u32LeaseExpirationPeriod);
    else
        return (ASMDivU64ByU32RetU32(RTTimeMilliTS() - m->u64TimestampBindingStarted, 1000)
                > m->u32BindExpirationPeriod);
}


void Lease::expire()
{
    /* XXX: TODO */
}


void Lease::phaseStart(uint64_t u64Start)
{
    if (m->fBinding)
        m->u64TimestampBindingStarted = u64Start;
    else
        m->u64TimestampLeasingStarted = u64Start;
}


void Lease::bindingPhase(bool fOnOff)
{
    m->fBinding = fOnOff;
}


bool Lease::isInBindingPhase() const
{
    return m->fBinding;
}


uint64_t Lease::issued() const
{
    return m->u64TimestampLeasingStarted;
}


void Lease::setExpiration(uint32_t exp)
{
    if (m->fBinding)
        m->u32BindExpirationPeriod = exp;
    else
        m->u32LeaseExpirationPeriod = exp;
}


uint32_t Lease::getExpiration() const
{
    if (m->fBinding)
        return m->u32BindExpirationPeriod;
    else
        return m->u32LeaseExpirationPeriod;
}


RTNETADDRIPV4 Lease::getAddress() const
{
    return m->m_address;
}


void Lease::setAddress(RTNETADDRIPV4 address)
{
    m->m_address = address;
}


const NetworkConfigEntity *Lease::getConfig() const
{
    return m->pCfg;
}


void Lease::setConfig(NetworkConfigEntity *pCfg)
{
    m->pCfg = pCfg;
}


const MapOptionId2RawOption& Lease::options() const
{
    return m->options;
}


MapOptionId2RawOption& Lease::options()
{
    return m->options;
}


Lease::Lease(ClientData *pd):m(SharedPtr<ClientData>(pd)){}


bool Lease::toXML(xml::ElementNode *node) const
{
    bool valueAddition = node->setAttribute(tagXMLLeaseAttributeMac.c_str(), com::Utf8StrFmt("%RTmac", &m->m_mac));
    if (!valueAddition) return false;

    valueAddition = node->setAttribute(tagXMLLeaseAttributeNetwork.c_str(), com::Utf8StrFmt("%RTnaipv4", m->m_network));
    if (!valueAddition) return false;

    xml::ElementNode *address = node->createChild(tagXMLLeaseAddress.c_str()); 
    if (!address) return false;

    valueAddition = address->setAttribute(tagXMLAddressAttributeValue.c_str(), com::Utf8StrFmt("%RTnaipv4", m->m_address));
    if (!valueAddition) return false;

    xml::ElementNode *time = node->createChild(tagXMLLeaseTime.c_str()); 
    if (!time) return false;

    valueAddition = time->setAttribute(tagXMLTimeAttributeIssued.c_str(), 
                                       m->u64TimestampLeasingStarted);
    if (!valueAddition) return false;

    valueAddition = time->setAttribute(tagXMLTimeAttributeExpiration.c_str(), 
                                       m->u32LeaseExpirationPeriod);
    if (!valueAddition) return false;

    return true;
}


bool Lease::fromXML(const xml::ElementNode *node)
{
    com::Utf8Str mac;
    bool valueExists = node->getAttributeValue(tagXMLLeaseAttributeMac.c_str(), mac);
    if (!valueExists) return false;
    int rc = RTNetStrToMacAddr(mac.c_str(), &m->m_mac);
    if (RT_FAILURE(rc)) return false;

    com::Utf8Str network;
    valueExists = node->getAttributeValue(tagXMLLeaseAttributeNetwork.c_str(), network);
    if (!valueExists) return false;
    rc = RTNetStrToIPv4Addr(network.c_str(), &m->m_network);
    if (RT_FAILURE(rc)) return false;

    /* Address */
    const xml::ElementNode *address = node->findChildElement(tagXMLLeaseAddress.c_str());
    if (!address) return false;
    com::Utf8Str addressValue;
    valueExists = address->getAttributeValue(tagXMLAddressAttributeValue.c_str(), addressValue);
    if (!valueExists) return false;
    rc = RTNetStrToIPv4Addr(addressValue.c_str(), &m->m_address);
    
    /* Time */
    const xml::ElementNode *time = node->findChildElement(tagXMLLeaseTime.c_str());
    if (!time) return false;

    valueExists = time->getAttributeValue(tagXMLTimeAttributeIssued.c_str(), 
                                          &m->u64TimestampLeasingStarted);
    if (!valueExists) return false;
    m->fBinding = false;
    
    valueExists = time->getAttributeValue(tagXMLTimeAttributeExpiration.c_str(), 
                                          &m->u32LeaseExpirationPeriod);
    if (!valueExists) return false;

    m->fHasLease = true;
    return true;
}


const Lease Lease::NullLease;


const Client Client::NullClient;
