/* $Id$ */
/** @file
 * DHCP server - server configuration
 */

/*
 * Copyright (C) 2017-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "Config.h"

#include <iprt/types.h>
#include <iprt/net.h>           /* NB: must come before getopt.h */
#include <iprt/getopt.h>
#include <iprt/path.h>
#include <iprt/message.h>
#include <iprt/string.h>
#include <iprt/uuid.h>

#include <VBox/com/com.h>

#include <iostream>

class ConfigFileError
  : public RTCError
{
public:
    ConfigFileError(const char *pszMessage)
      : RTCError(pszMessage) {}

    ConfigFileError(const RTCString &a_rstrMessage)
      : RTCError(a_rstrMessage) {}
};


Config::Config()
  : m_strHome(),
    m_strNetwork(),
    m_strBaseName(),
    m_strTrunk(),
    m_enmTrunkType(kIntNetTrunkType_Invalid),
    m_MacAddress(),
    m_IPv4Address(),
    m_IPv4Netmask(),
    m_IPv4PoolFirst(),
    m_IPv4PoolLast(),
    m_GlobalOptions(),
    m_VMMap()
{
    return;
}


int Config::init()
{
    int rc;

    rc = homeInit();
    if (RT_FAILURE(rc))
        return rc;

    return VINF_SUCCESS;
}


int Config::homeInit()
{
    int rc;

    /* pathname of ~/.VirtualBox or equivalent */
    char szHome[RTPATH_MAX];
    rc = com::GetVBoxUserHomeDirectory(szHome, sizeof(szHome), false);
    if (RT_FAILURE(rc))
    {
        LogDHCP(("unable to find VirtualBox home directory: %Rrs", rc));
        return rc;
    }

    m_strHome.assign(szHome);
    return VINF_SUCCESS;
}


void Config::setNetwork(const std::string &aStrNetwork)
{
    AssertReturnVoid(m_strNetwork.empty());

    m_strNetwork = aStrNetwork;
    sanitizeBaseName();
}


/*
 * Requires network name to be known as the log file name depends on
 * it.  Alternatively, consider passing the log file name via the
 * command line?
 */
int Config::logInit()
{
    int rc;
    size_t cch;

    if (m_strHome.empty() || m_strBaseName.empty())
        return VERR_GENERAL_FAILURE;

    /* default log file name */
    char szLogFile[RTPATH_MAX];
    cch = RTStrPrintf(szLogFile, sizeof(szLogFile),
                      "%s%c%s-Dhcpd.log",
                      m_strHome.c_str(), RTPATH_DELIMITER, m_strBaseName.c_str());
    if (cch >= sizeof(szLogFile))
        return VERR_BUFFER_OVERFLOW;


    /* get a writable copy of the base name */
    char szBaseName[RTPATH_MAX];
    rc = RTStrCopy(szBaseName, sizeof(szBaseName), m_strBaseName.c_str());
    if (RT_FAILURE(rc))
        return rc;

    /* sanitize base name some more to be usable in an environment variable name */
    for (char *p = szBaseName; *p != '\0'; ++p)
    {
        if (   *p != '_'
            && (*p < '0' || '9' < *p)
            && (*p < 'a' || 'z' < *p)
            && (*p < 'A' || 'Z' < *p))
        {
            *p = '_';
        }
    }


    /* name of the environment variable to control logging */
    char szEnvVarBase[128];
    cch = RTStrPrintf(szEnvVarBase, sizeof(szEnvVarBase),
                      "VBOXDHCP_%s_RELEASE_LOG", szBaseName);
    if (cch >= sizeof(szEnvVarBase))
        return VERR_BUFFER_OVERFLOW;


    rc = com::VBoxLogRelCreate("DHCP Server",
                               szLogFile,
                               RTLOGFLAGS_PREFIX_TIME_PROG,
                               "all all.restrict -default.restrict",
                               szEnvVarBase,
                               RTLOGDEST_FILE
#ifdef DEBUG
                               | RTLOGDEST_STDERR
#endif
                               ,
                               32768 /* cMaxEntriesPerGroup */,
                               0 /* cHistory */,
                               0 /* uHistoryFileTime */,
                               0 /* uHistoryFileSize */,
                               NULL /* pErrInfo */);

    return rc;
}


int Config::complete()
{
    int rc;

    if (m_strNetwork.empty())
    {
        LogDHCP(("network name is not specified\n"));
        return false;
    }

    logInit();

    bool fMACGenerated = false;
    if (   m_MacAddress.au16[0] == 0
        && m_MacAddress.au16[1] == 0
        && m_MacAddress.au16[2] == 0)
    {
        RTUUID Uuid;
        RTUuidCreate(&Uuid);

        m_MacAddress.au8[0] = 0x08;
        m_MacAddress.au8[1] = 0x00;
        m_MacAddress.au8[2] = 0x27;
        m_MacAddress.au8[3] = Uuid.Gen.au8Node[3];
        m_MacAddress.au8[4] = Uuid.Gen.au8Node[4];
        m_MacAddress.au8[5] = Uuid.Gen.au8Node[5];

        LogDHCP(("MAC address is not specified: will use generated MAC %RTmac\n", &m_MacAddress));
        fMACGenerated = true;
    }

    /* unicast MAC address */
    if (m_MacAddress.au8[0] & 0x01)
    {
        LogDHCP(("MAC address is not unicast: %RTmac\n", &m_MacAddress));
        return VERR_GENERAL_FAILURE;
    }

    /* unicast IP address */
    if ((m_IPv4Address.au8[0] & 0xe0) == 0xe0)
    {
        LogDHCP(("IP address is not unicast: %RTnaipv4\n", m_IPv4Address.u));
        return VERR_GENERAL_FAILURE;
    }

    /* valid netmask */
    int iPrefixLengh;
    rc = RTNetMaskToPrefixIPv4(&m_IPv4Netmask, &iPrefixLengh);
    if (RT_FAILURE(rc) || iPrefixLengh == 0)
    {
        LogDHCP(("IP mask is not valid: %RTnaipv4\n", m_IPv4Netmask.u));
        return VERR_GENERAL_FAILURE;
    }

    /* first IP is from the same network */
    if ((m_IPv4PoolFirst.u & m_IPv4Netmask.u) != (m_IPv4Address.u & m_IPv4Netmask.u))
    {
        LogDHCP(("first pool address is outside the network %RTnaipv4/%d: %RTnaipv4\n",
                 (m_IPv4Address.u & m_IPv4Netmask.u), iPrefixLengh,
                 m_IPv4PoolFirst.u));
        return VERR_GENERAL_FAILURE;
    }

    /* last IP is from the same network */
    if ((m_IPv4PoolLast.u & m_IPv4Netmask.u) != (m_IPv4Address.u & m_IPv4Netmask.u))
    {
        LogDHCP(("last pool address is outside the network %RTnaipv4/%d: %RTnaipv4\n",
                 (m_IPv4Address.u & m_IPv4Netmask.u), iPrefixLengh,
                 m_IPv4PoolLast.u));
        return VERR_GENERAL_FAILURE;
    }

    /* the pool is valid */
    if (RT_N2H_U32(m_IPv4PoolLast.u) < RT_N2H_U32(m_IPv4PoolFirst.u))
    {
        LogDHCP(("pool range is invalid: %RTnaipv4 - %RTnaipv4\n",
                 m_IPv4PoolFirst.u, m_IPv4PoolLast.u));
        return VERR_GENERAL_FAILURE;
    }

    /* our own address is not inside the pool */
    if (   RT_N2H_U32(m_IPv4PoolFirst.u) <= RT_N2H_U32(m_IPv4Address.u)
        && RT_N2H_U32(m_IPv4Address.u)  <= RT_N2H_U32(m_IPv4PoolLast.u))
    {
        LogDHCP(("server address inside the pool range %RTnaipv4 - %RTnaipv4: %RTnaipv4\n",
                 m_IPv4PoolFirst.u, m_IPv4PoolLast.u, m_IPv4Address.u));
        return VERR_GENERAL_FAILURE;
    }

    if (!fMACGenerated)
        LogDHCP(("MAC address %RTmac\n", &m_MacAddress));
    LogDHCP(("IP address %RTnaipv4/%d\n", m_IPv4Address.u, iPrefixLengh));
    LogDHCP(("address pool %RTnaipv4 - %RTnaipv4\n", m_IPv4PoolFirst.u, m_IPv4PoolLast.u));

    return VINF_SUCCESS;
}


Config *Config::hardcoded()
{
    int rc;

    std::unique_ptr<Config> config(new Config());
    rc = config->init();
    if (RT_FAILURE(rc))
        return NULL;

    config->setNetwork("HostInterfaceNetworking-vboxnet0");
    config->m_strTrunk.assign("vboxnet0");
    config->m_enmTrunkType = kIntNetTrunkType_NetFlt;

    config->m_MacAddress.au8[0] = 0x08;
    config->m_MacAddress.au8[1] = 0x00;
    config->m_MacAddress.au8[2] = 0x27;
    config->m_MacAddress.au8[3] = 0xa9;
    config->m_MacAddress.au8[4] = 0xcf;
    config->m_MacAddress.au8[5] = 0xef;


    config->m_IPv4Address.u = RT_H2N_U32_C(0xc0a838fe); /* 192.168.56.254 */
    config->m_IPv4Netmask.u = RT_H2N_U32_C(0xffffff00); /* 255.255.255.0 */

    /* flip to test naks */
#if 1
    config->m_IPv4PoolFirst.u = RT_H2N_U32_C(0xc0a8385a); /* 192.168.56.90 */
    config->m_IPv4PoolLast.u = RT_H2N_U32_C(0xc0a83863); /* 192.168.56.99 */
#else
    config->m_IPv4PoolFirst.u = RT_H2N_U32_C(0xc0a838c9); /* 192.168.56.201 */
    config->m_IPv4PoolLast.u = RT_H2N_U32_C(0xc0a838dc); /* 192.168.56.220 */
#endif

    rc = config->complete();
    AssertRCReturn(rc, NULL);

    return config.release();
}


/* compatibility with old VBoxNetDHCP */
static const RTGETOPTDEF g_aCompatOptions[] =
{
    { "--ip-address",     'i',   RTGETOPT_REQ_IPV4ADDR },
    { "--lower-ip",       'l',   RTGETOPT_REQ_IPV4ADDR },
    { "--mac-address",    'a',   RTGETOPT_REQ_MACADDR },
    { "--need-main",      'M',   RTGETOPT_REQ_BOOL },
    { "--netmask",        'm',   RTGETOPT_REQ_IPV4ADDR },
    { "--network",        'n',   RTGETOPT_REQ_STRING },
    { "--trunk-name",     't',   RTGETOPT_REQ_STRING },
    { "--trunk-type",     'T',   RTGETOPT_REQ_STRING },
    { "--upper-ip",       'u',   RTGETOPT_REQ_IPV4ADDR },
};


Config *Config::compat(int argc, char **argv)
{
    RTGETOPTSTATE State;
    int rc;

    rc = RTGetOptInit(&State, argc, argv,
                      g_aCompatOptions, RT_ELEMENTS(g_aCompatOptions), 1,
                      RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    AssertRCReturn(rc, NULL);

    std::unique_ptr<Config> config(new Config());
    rc = config->init();
    if (RT_FAILURE(rc))
        return NULL;

    for (;;)
    {
        RTGETOPTUNION Val;

        rc = RTGetOpt(&State, &Val);
        if (rc == 0)            /* done */
            break;

        switch (rc)
        {
            case 'a': /* --mac-address */
                if (   config->m_MacAddress.au16[0] != 0
                    || config->m_MacAddress.au16[1] != 0
                    || config->m_MacAddress.au16[2] != 0)
                {
                    RTMsgError("Duplicate --mac-address option");
                    return NULL;
                }
                config->m_MacAddress = Val.MacAddr;
                break;

            case 'i': /* --ip-address */
                if (config->m_IPv4Address.u != 0)
                {
                    RTMsgError("Duplicate --ip-address option");
                    return NULL;
                }
                config->m_IPv4Address = Val.IPv4Addr;
                break;

            case 'l': /* --lower-ip */
                if (config->m_IPv4PoolFirst.u != 0)
                {
                    RTMsgError("Duplicate --lower-ip option");
                    return NULL;
                }
                config->m_IPv4PoolFirst = Val.IPv4Addr;
                break;

            case 'M': /* --need-main */
                /* for backward compatibility, ignored */
                break;

            case 'm': /* --netmask */
                if (config->m_IPv4Netmask.u != 0)
                {
                    RTMsgError("Duplicate --netmask option");
                    return NULL;
                }
                config->m_IPv4Netmask = Val.IPv4Addr;
                break;

            case 'n': /* --network */
                if (!config->m_strNetwork.empty())
                {
                    RTMsgError("Duplicate --network option");
                    return NULL;
                }
                config->setNetwork(Val.psz);
                break;

            case 't': /* --trunk-name */
                if (!config->m_strTrunk.empty())
                {
                    RTMsgError("Duplicate --trunk-name option");
                    return NULL;
                }
                config->m_strTrunk.assign(Val.psz);
                break;

            case 'T': /* --trunk-type */
                if (config->m_enmTrunkType != kIntNetTrunkType_Invalid)
                {
                    RTMsgError("Duplicate --trunk-type option");
                    return NULL;
                }
                else if (strcmp(Val.psz, "none") == 0)
                    config->m_enmTrunkType = kIntNetTrunkType_None;
                else if (strcmp(Val.psz, "whatever") == 0)
                    config->m_enmTrunkType = kIntNetTrunkType_WhateverNone;
                else if (strcmp(Val.psz, "netflt") == 0)
                    config->m_enmTrunkType = kIntNetTrunkType_NetFlt;
                else if (strcmp(Val.psz, "netadp") == 0)
                    config->m_enmTrunkType = kIntNetTrunkType_NetAdp;
                else
                {
                    RTMsgError("Unknown trunk type '%s'", Val.psz);
                    return NULL;
                }
                break;

            case 'u': /* --upper-ip */
                if (config->m_IPv4PoolLast.u != 0)
                {
                    RTMsgError("Duplicate --upper-ip option");
                    return NULL;
                }
                config->m_IPv4PoolLast = Val.IPv4Addr;
                break;

            case VINF_GETOPT_NOT_OPTION:
                RTMsgError("%s: Unexpected command line argument", Val.psz);
                return NULL;

            default:
                RTGetOptPrintError(rc, &Val);
                return NULL;
        }
    }

    rc = config->complete();
    if (RT_FAILURE(rc))
        return NULL;

    return config.release();
}


static const RTGETOPTDEF g_aOptions[] =
{
    { "--config",       'c',    RTGETOPT_REQ_STRING },
};


Config *Config::create(int argc, char **argv)
{
    RTGETOPTSTATE State;
    int rc;

    rc = RTGetOptInit(&State, argc, argv,
                      g_aOptions, RT_ELEMENTS(g_aOptions), 1,
                      RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    AssertRCReturn(rc, NULL);

    std::unique_ptr<Config> config;
    for (;;)
    {
        RTGETOPTUNION Val;

        rc = RTGetOpt(&State, &Val);
        if (rc == 0)            /* done */
            break;

        switch (rc)
        {
            case 'c': /* --config */
                if (config.get() != NULL)
                {
                    printf("Duplicate option: --config '%s'\n", Val.psz);
                    return NULL;
                }

                printf("reading config from %s\n", Val.psz);
                config.reset(Config::read(Val.psz));
                if (config.get() == NULL)
                    return NULL;

                break;

            case VINF_GETOPT_NOT_OPTION:
                RTMsgError("Unexpected command line argument: '%s'", Val.psz);
                return NULL;

            default:
                RTGetOptPrintError(rc, &Val);
                return NULL;
        }
    }

    if (config.get() == NULL)
        return NULL;

    rc = config->complete();
    if (RT_FAILURE(rc))
        return NULL;

    return config.release();
}


Config *Config::read(const char *pszFileName)
{
    int rc;

    if (pszFileName == NULL || pszFileName[0] == '\0')
        return NULL;

    xml::Document doc;
    try
    {
        xml::XmlFileParser parser;
        parser.read(pszFileName, doc);
    }
    catch (const xml::EIPRTFailure &e)
    {
        LogDHCP(("%s\n", e.what()));
        return NULL;
    }
    catch (const RTCError &e)
    {
        LogDHCP(("%s\n", e.what()));
        return NULL;
    }
    catch (...)
    {
        LogDHCP(("Unknown exception while reading and parsing '%s'\n",
                 pszFileName));
        return NULL;
    }

    std::unique_ptr<Config> config(new Config());
    rc = config->init();
    if (RT_FAILURE(rc))
        return NULL;

    try
    {
        config->parseConfig(doc.getRootElement());
    }
    catch (const RTCError &e)
    {
        LogDHCP(("%s\n", e.what()));
        return NULL;
    }
    catch (...)
    {
        LogDHCP(("Unexpected exception\n"));
        return NULL;
    }

    return config.release();
}


void Config::parseConfig(const xml::ElementNode *root)
{
    if (root == NULL)
        throw ConfigFileError("Empty config file");

    /*
     * XXX: NAMESPACE API IS COMPLETELY BROKEN, SO IGNORE IT FOR NOW
     */
    if (!root->nameEquals("DHCPServer"))
    {
        const char *name = root->getName();
        throw ConfigFileError(RTCStringFmt("Unexpected root element \"%s\"",
                                           name ? name : "(null)"));
    }

    parseServer(root);

    // XXX: debug
    for (auto it: m_GlobalOptions) {
        std::shared_ptr<DhcpOption> opt(it.second);

        octets_t data;
        opt->encode(data);

        bool space = false;
        for (auto c: data) {
            if (space)
                std::cout << " ";
            else
                space = true;
            std::cout << (int)c;
        }
        std::cout << std::endl;
    }
}


static void getIPv4AddrAttribute(const xml::ElementNode *pNode, const char *pcszAttrName,
                                 RTNETADDRIPV4 *pAddr)
{
    RTCString strAddr;
    bool fHasAttr = pNode->getAttributeValue(pcszAttrName, &strAddr);
    if (!fHasAttr)
        throw ConfigFileError(RTCStringFmt("%s attribute missing",
                                           pcszAttrName));

    int rc = RTNetStrToIPv4Addr(strAddr.c_str(), pAddr);
    if (RT_FAILURE(rc))
        throw ConfigFileError(RTCStringFmt("%s attribute invalid",
                                           pcszAttrName));
}


void Config::parseServer(const xml::ElementNode *server)
{
    /*
     * DHCPServer attributes
     */
    RTCString strNetworkName;
    bool fHasNetworkName = server->getAttributeValue("networkName", &strNetworkName);
    if (!fHasNetworkName)
        throw ConfigFileError("DHCPServer/@networkName missing");

    setNetwork(strNetworkName.c_str());

    getIPv4AddrAttribute(server, "IPAddress", &m_IPv4Address);
    getIPv4AddrAttribute(server, "networkMask", &m_IPv4Netmask);
    getIPv4AddrAttribute(server, "lowerIP", &m_IPv4PoolFirst);
    getIPv4AddrAttribute(server, "upperIP", &m_IPv4PoolLast);

    /*
     * DHCPServer children
     */
    xml::NodesLoop it(*server);
    const xml::ElementNode *node;
    while ((node = it.forAllNodes()) != NULL)
    {
        /*
         * Global options
         */
        if (node->nameEquals("Options"))
        {
            parseGlobalOptions(node);
        }

        /*
         * Per-VM configuration
         */
        else if (node->nameEquals("Config"))
        {
            parseVMConfig(node);
        }
    }
}


void Config::parseGlobalOptions(const xml::ElementNode *options)
{
    xml::NodesLoop it(*options);
    const xml::ElementNode *node;
    while ((node = it.forAllNodes()) != NULL)
    {
        if (node->nameEquals("Option"))
        {
            parseOption(node, m_GlobalOptions);
        }
        else
        {
            throw ConfigFileError(RTCStringFmt("Unexpected element \"%s\"",
                                               node->getName()));
        }
    }
}


/*
 * VM Config entries are generated automatically from VirtualBox.xml
 * with the MAC fetched from the VM config.  The client id is nowhere
 * in the picture there, so VM config is indexed with plain RTMAC, not
 * ClientId (also see getOptions below).
 */
void Config::parseVMConfig(const xml::ElementNode *config)
{
    RTMAC mac;
    int rc;

    RTCString strMac;
    bool fHasMac = config->getAttributeValue("MACAddress", &strMac);
    if (!fHasMac)
        throw ConfigFileError(RTCStringFmt("Config missing MACAddress attribute"));

    rc = parseMACAddress(mac, strMac);
    if (RT_FAILURE(rc))
    {
        throw ConfigFileError(RTCStringFmt("Malformed MACAddress attribute \"%s\"",
                                           strMac.c_str()));
    }

    vmmap_t::iterator vmit( m_VMMap.find(mac) );
    if (vmit != m_VMMap.end())
    {
        throw ConfigFileError(RTCStringFmt("Duplicate Config for MACAddress \"%s\"",
                                           strMac.c_str()));
    }

    optmap_t &vmopts = m_VMMap[mac];

    xml::NodesLoop it(*config);
    const xml::ElementNode *node;
    while ((node = it.forAllNodes()) != NULL)
    {
        if (node->nameEquals("Option"))
        {
            parseOption(node, vmopts);
        }
        else
        {
            throw ConfigFileError(RTCStringFmt("Unexpected element \"%s\"",
                                               node->getName()));
        }
    }    
}


int Config::parseMACAddress(RTMAC &aMac, const RTCString &aStr)
{
    RTMAC mac;
    int rc;

    rc = RTNetStrToMacAddr(aStr.c_str(), &mac);
    if (RT_FAILURE(rc))
        return rc;
    if (rc == VWRN_TRAILING_CHARS)
        return VERR_INVALID_PARAMETER;

    aMac = mac;
    return VINF_SUCCESS;
}


int Config::parseClientId(OptClientId &aId, const RTCString &aStr)
{
    RT_NOREF(aId, aStr);
    return VERR_GENERAL_FAILURE;
}


/*
 * Parse <Option/> element and add the option to the specified optmap.
 */
void Config::parseOption(const xml::ElementNode *option, optmap_t &optmap)
{
    int rc;

    uint8_t u8Opt;
    RTCString strName;
    bool fHasName = option->getAttributeValue("name", &strName);
    if (fHasName)
    {
        const char *pcszName = strName.c_str();

        rc = RTStrToUInt8Full(pcszName, 10, &u8Opt);
        if (rc != VINF_SUCCESS) /* no warnings either */
            throw ConfigFileError(RTCStringFmt("Bad option \"%s\"", pcszName));

    }
    else
        throw ConfigFileError("missing option name");


    uint32_t u32Enc = 0;        /* XXX: DhcpOptEncoding_Legacy */
    RTCString strEncoding;
    bool fHasEncoding = option->getAttributeValue("encoding", &strEncoding);
    if (fHasEncoding)
    {
        const char *pcszEnc = strEncoding.c_str();

        rc = RTStrToUInt32Full(pcszEnc, 10, &u32Enc);
        if (rc != VINF_SUCCESS) /* no warnings either */
            throw ConfigFileError(RTCStringFmt("Bad encoding \"%s\"", pcszEnc));

        switch (u32Enc)
        {
        case 0:                 /* XXX: DhcpOptEncoding_Legacy */
        case 1:                 /* XXX: DhcpOptEncoding_Hex */
            break;
        default:
            throw ConfigFileError(RTCStringFmt("Unknown encoding \"%s\"", pcszEnc));
        }
    }


    /* value may be omitted for OptNoValue options like rapid commit */
    RTCString strValue;
    option->getAttributeValue("value", &strValue);

    /* XXX: TODO: encoding, handle hex */
    DhcpOption *opt = DhcpOption::parse(u8Opt, u32Enc, strValue.c_str());
    if (opt == NULL)
        throw ConfigFileError(RTCStringFmt("Bad option \"%s\"", strName.c_str()));

    optmap << opt;
}


/*
 * Set m_strBaseName to sanitized version of m_strNetwork that can be
 * used in a path component.
 */
void Config::sanitizeBaseName()
{
    int rc;

    if (m_strNetwork.empty())
        return;

    char szBaseName[RTPATH_MAX];
    rc = RTStrCopy(szBaseName, sizeof(szBaseName), m_strNetwork.c_str());
    if (RT_FAILURE(rc))
        return;

    for (char *p = szBaseName; *p != '\0'; ++p)
    {
        if (RTPATH_IS_SEP(*p))
        {
            *p = '_';
        }
    }

    m_strBaseName.assign(szBaseName);
}


optmap_t Config::getOptions(const OptParameterRequest &reqOpts,
                            const ClientId &id,
                            const OptVendorClassId &vendor) const
{
    optmap_t optmap;

    const optmap_t *vmopts = NULL;
    vmmap_t::const_iterator vmit( m_VMMap.find(id.mac()) );
    if (vmit != m_VMMap.end())
        vmopts = &vmit->second;

    RT_NOREF(vendor); /* not yet */


    optmap << new OptSubnetMask(m_IPv4Netmask);

    for (auto optreq: reqOpts.value())
    {
        std::cout << ">>> requested option " << (int)optreq << std::endl;

        if (optreq == OptSubnetMask::optcode)
        {
            std::cout << "... always supplied" << std::endl;
            continue;
        }

        if (vmopts != NULL)
        {
            optmap_t::const_iterator it( vmopts->find(optreq) );
            if (it != vmopts->end())
            {
                optmap << it->second;
                std::cout << "... found in VM options" << std::endl;
                continue;
            }
        }

        optmap_t::const_iterator it( m_GlobalOptions.find(optreq) );
        if (it != m_GlobalOptions.end())
        {
            optmap << it->second;
            std::cout << "... found in global options" << std::endl;
            continue;
        }

        // std::cout << "... not found" << std::endl;
    }


    /* XXX: testing ... */
    if (vmopts != NULL)
    {
        for (auto it: *vmopts) {
            std::shared_ptr<DhcpOption> opt(it.second);
            if (optmap.count(opt->optcode()) == 0 && opt->optcode() > 127)
            {
                optmap << opt;
                std::cout << "... forcing VM option " << (int)opt->optcode() << std::endl;
            }
        }
    }

    for (auto it: m_GlobalOptions) {
        std::shared_ptr<DhcpOption> opt(it.second);
        if (optmap.count(opt->optcode()) == 0 && opt->optcode() > 127)
        {
            optmap << opt;
            std::cout << "... forcing global option " << (int)opt->optcode() << std::endl;
        }
    }

    return optmap;
}
