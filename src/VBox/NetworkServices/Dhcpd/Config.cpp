/* $Id$ */
/** @file
 * DHCP server - server configuration
 */

/*
 * Copyright (C) 2017-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "DhcpdInternal.h"

#include <iprt/ctype.h>
#include <iprt/net.h>           /* NB: must come before getopt.h */
#include <iprt/getopt.h>
#include <iprt/path.h>
#include <iprt/message.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <iprt/cpp/path.h>

#include <VBox/com/com.h>       /* For log initialization. */

#include "Config.h"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/*static*/ bool         Config::g_fInitializedLog = false;
/*static*/ uint32_t     GroupConfig::s_uGroupNo   = 0;


/**
 * Configuration file exception.
 */
class ConfigFileError
    : public RTCError
{
public:
#if 0 /* This just confuses the compiler. */
    ConfigFileError(const char *a_pszMessage)
        : RTCError(a_pszMessage)
    {}
#endif

    explicit ConfigFileError(xml::Node const *pNode, const char *a_pszMsgFmt, ...)
        : RTCError((char *)NULL)
    {

        i_buildPath(pNode);
        m_strMsg.append(": ");

        va_list va;
        va_start(va, a_pszMsgFmt);
        m_strMsg.appendPrintf(a_pszMsgFmt, va);
        va_end(va);
    }


    ConfigFileError(const char *a_pszMsgFmt, ...)
        : RTCError((char *)NULL)
    {
        va_list va;
        va_start(va, a_pszMsgFmt);
        m_strMsg.printfV(a_pszMsgFmt, va);
        va_end(va);
    }

    ConfigFileError(const RTCString &a_rstrMessage)
        : RTCError(a_rstrMessage)
    {}

private:
    void i_buildPath(xml::Node const *pNode)
    {
        if (pNode)
        {
            i_buildPath(pNode->getParent());
            m_strMsg.append('/');
            m_strMsg.append(pNode->getName());
            if (pNode->isElement())
            {
                xml::ElementNode const *pElm = (xml::ElementNode const *)pNode;
                for (xml::Node const *pNodeChild = pElm->getFirstChild(); pNodeChild != NULL;
                    pNodeChild = pNodeChild->getNextSibiling())
                    if (pNodeChild->isAttribute())
                    {
                        m_strMsg.append("[@");
                        m_strMsg.append(pNodeChild->getName());
                        m_strMsg.append('=');
                        m_strMsg.append(pNodeChild->getValue());
                        m_strMsg.append(']');
                    }
            }
        }
    }

};


/**
 * Private default constructor, external users use factor methods.
 */
Config::Config()
    : m_strHome()
    , m_strNetwork()
    , m_strTrunk()
    , m_enmTrunkType(kIntNetTrunkType_Invalid)
    , m_MacAddress()
    , m_IPv4Address()
    , m_IPv4Netmask()
    , m_IPv4PoolFirst()
    , m_IPv4PoolLast()
    , m_GlobalConfig()
    , m_GroupConfigs()
    , m_HostConfigs()
{
}


/**
 * Initializes the object.
 *
 * @returns IPRT status code.
 */
int Config::i_init() RT_NOEXCEPT
{
    return i_homeInit();
}


/**
 * Initializes the m_strHome member with the path to ~/.VirtualBox or equivalent.
 *
 * @returns IPRT status code.
 * @todo Too many init functions?
 */
int Config::i_homeInit() RT_NOEXCEPT
{
    char szHome[RTPATH_MAX];
    int rc = com::GetVBoxUserHomeDirectory(szHome, sizeof(szHome), false);
    if (RT_SUCCESS(rc))
        rc = m_strHome.assignNoThrow(szHome);
    else
        DHCP_LOG_MSG_ERROR(("unable to locate the VirtualBox home directory: %Rrc\n", rc));
    return rc;
}


/**
 * Internal worker for the public factory methods that creates an instance and
 * calls i_init() on it.
 *
 * @returns Config instance on success, NULL on failure.
 */
/*static*/ Config *Config::i_createInstanceAndCallInit() RT_NOEXCEPT
{
    Config *pConfig;
    try
    {
        pConfig = new Config();
    }
    catch (std::bad_alloc &)
    {
        return NULL;
    }

    int rc = pConfig->i_init();
    if (RT_SUCCESS(rc))
        return pConfig;
    delete pConfig;
    return NULL;
}


/**
 * Worker for i_complete() that initializes the release log of the process.
 *
 * Requires network name to be known as the log file name depends on
 * it.  Alternatively, consider passing the log file name via the
 * command line?
 *
 * @note This is only used when no --log parameter was given.
 */
int Config::i_logInit() RT_NOEXCEPT
{
    if (g_fInitializedLog)
        return VINF_SUCCESS;

    if (m_strHome.isEmpty() || m_strNetwork.isEmpty())
        return VERR_PATH_ZERO_LENGTH;

    /* default log file name */
    char szLogFile[RTPATH_MAX];
    ssize_t cch = RTStrPrintf2(szLogFile, sizeof(szLogFile),
                               "%s%c%s-Dhcpd.log",
                               m_strHome.c_str(), RTPATH_DELIMITER, m_strNetwork.c_str());
    if (cch > 0)
    {
        RTPathPurgeFilename(RTPathFilename(szLogFile), RTPATH_STR_F_STYLE_HOST);
        return i_logInitWithFilename(szLogFile);
    }
    return VERR_BUFFER_OVERFLOW;
}


/**
 * Worker for i_logInit and for handling --log on the command line.
 *
 * @returns IPRT status code.
 * @param   pszFilename         The log filename.
 */
/*static*/ int Config::i_logInitWithFilename(const char *pszFilename) RT_NOEXCEPT
{
    AssertReturn(!g_fInitializedLog, VERR_WRONG_ORDER);

    int rc = com::VBoxLogRelCreate("DHCP Server",
                                   pszFilename,
                                   RTLOGFLAGS_PREFIX_TIME_PROG,
                                   "all net_dhcpd.e.l",
                                   "VBOXDHCP_RELEASE_LOG",
                                   RTLOGDEST_FILE
#ifdef DEBUG
                                   | RTLOGDEST_STDERR
#endif
                                   ,
                                   32768 /* cMaxEntriesPerGroup */,
                                   5 /* cHistory */,
                                   RT_SEC_1DAY /* uHistoryFileTime */,
                                   _32M /* uHistoryFileSize */,
                                   NULL /* pErrInfo */);
    if (RT_SUCCESS(rc))
        g_fInitializedLog = true;
    else
        RTMsgError("Log initialization failed: %Rrc, log file '%s'", rc, pszFilename);
    return rc;

}


/**
 * Post process and validate the configuration after it has been loaded.
 */
int Config::i_complete() RT_NOEXCEPT
{
    if (m_strNetwork.isEmpty())
    {
        LogRel(("network name is not specified\n"));
        return false;
    }

    i_logInit();

    bool fMACGenerated = false;
    if (   m_MacAddress.au16[0] == 0
        && m_MacAddress.au16[1] == 0
        && m_MacAddress.au16[2] == 0)
    {
        RTUUID Uuid;
        int rc = RTUuidCreate(&Uuid);
        AssertRCReturn(rc, rc);

        m_MacAddress.au8[0] = 0x08;
        m_MacAddress.au8[1] = 0x00;
        m_MacAddress.au8[2] = 0x27;
        m_MacAddress.au8[3] = Uuid.Gen.au8Node[3];
        m_MacAddress.au8[4] = Uuid.Gen.au8Node[4];
        m_MacAddress.au8[5] = Uuid.Gen.au8Node[5];

        LogRel(("MAC address is not specified: will use generated MAC %RTmac\n", &m_MacAddress));
        fMACGenerated = true;
    }

    /* unicast MAC address */
    if (m_MacAddress.au8[0] & 0x01)
    {
        LogRel(("MAC address is not unicast: %RTmac\n", &m_MacAddress));
        return VERR_GENERAL_FAILURE;
    }

    /* unicast IP address */
    if ((m_IPv4Address.au8[0] & 0xe0) == 0xe0)
    {
        LogRel(("IP address is not unicast: %RTnaipv4\n", m_IPv4Address.u));
        return VERR_GENERAL_FAILURE;
    }

    /* valid netmask */
    int cPrefixBits;
    int rc = RTNetMaskToPrefixIPv4(&m_IPv4Netmask, &cPrefixBits);
    if (RT_FAILURE(rc) || cPrefixBits == 0)
    {
        LogRel(("IP mask is not valid: %RTnaipv4\n", m_IPv4Netmask.u));
        return VERR_GENERAL_FAILURE;
    }

    /* first IP is from the same network */
    if ((m_IPv4PoolFirst.u & m_IPv4Netmask.u) != (m_IPv4Address.u & m_IPv4Netmask.u))
    {
        LogRel(("first pool address is outside the network %RTnaipv4/%d: %RTnaipv4\n",
                 (m_IPv4Address.u & m_IPv4Netmask.u), cPrefixBits, m_IPv4PoolFirst.u));
        return VERR_GENERAL_FAILURE;
    }

    /* last IP is from the same network */
    if ((m_IPv4PoolLast.u & m_IPv4Netmask.u) != (m_IPv4Address.u & m_IPv4Netmask.u))
    {
        LogRel(("last pool address is outside the network %RTnaipv4/%d: %RTnaipv4\n",
                 (m_IPv4Address.u & m_IPv4Netmask.u), cPrefixBits, m_IPv4PoolLast.u));
        return VERR_GENERAL_FAILURE;
    }

    /* the pool is valid */
    if (RT_N2H_U32(m_IPv4PoolLast.u) < RT_N2H_U32(m_IPv4PoolFirst.u))
    {
        LogRel(("pool range is invalid: %RTnaipv4 - %RTnaipv4\n",
                 m_IPv4PoolFirst.u, m_IPv4PoolLast.u));
        return VERR_GENERAL_FAILURE;
    }

    /* our own address is not inside the pool */
    if (   RT_N2H_U32(m_IPv4PoolFirst.u) <= RT_N2H_U32(m_IPv4Address.u)
        && RT_N2H_U32(m_IPv4Address.u)   <= RT_N2H_U32(m_IPv4PoolLast.u))
    {
        LogRel(("server address inside the pool range %RTnaipv4 - %RTnaipv4: %RTnaipv4\n",
                 m_IPv4PoolFirst.u, m_IPv4PoolLast.u, m_IPv4Address.u));
        return VERR_GENERAL_FAILURE;
    }

    if (!fMACGenerated)
        LogRel(("MAC address %RTmac\n", &m_MacAddress));
    LogRel(("IP address %RTnaipv4/%d\n", m_IPv4Address.u, cPrefixBits));
    LogRel(("address pool %RTnaipv4 - %RTnaipv4\n", m_IPv4PoolFirst.u, m_IPv4PoolLast.u));

    return VINF_SUCCESS;
}


/**
 * Parses the command line and loads the configuration.
 *
 * @returns The configuration, NULL if we ran into some fatal problem.
 * @param   argc    The argc from main().
 * @param   argv    The argv from main().
 */
Config *Config::create(int argc, char **argv) RT_NOEXCEPT
{
    /*
     * Parse the command line.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--comment",              '#', RTGETOPT_REQ_STRING },
        { "--config",               'c', RTGETOPT_REQ_STRING },
        { "--log",                  'l', RTGETOPT_REQ_STRING },
        { "--log-destinations",     'd', RTGETOPT_REQ_STRING },
        { "--log-flags",            'f', RTGETOPT_REQ_STRING },
        { "--log-group-settings",   'g', RTGETOPT_REQ_STRING },
        { "--relaxed",              'r', RTGETOPT_REQ_NOTHING },
        { "--strict",               's', RTGETOPT_REQ_NOTHING },
    };

    RTGETOPTSTATE State;
    int rc = RTGetOptInit(&State, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    AssertRCReturn(rc, NULL);

    const char *pszLogFile          = NULL;
    const char *pszLogGroupSettings = NULL;
    const char *pszLogDestinations  = NULL;
    const char *pszLogFlags         = NULL;
    const char *pszConfig           = NULL;
    const char *pszComment          = NULL;
    bool        fStrict             = true;

    for (;;)
    {
        RTGETOPTUNION ValueUnion;
        rc = RTGetOpt(&State, &ValueUnion);
        if (rc == 0)            /* done */
            break;

        switch (rc)
        {
            case 'c': /* --config */
                pszConfig = ValueUnion.psz;
                break;

            case 'l':
                pszLogFile = ValueUnion.psz;
                break;

            case 'd':
                pszLogDestinations = ValueUnion.psz;
                break;

            case 'f':
                pszLogFlags = ValueUnion.psz;
                break;

            case 'g':
                pszLogGroupSettings = ValueUnion.psz;
                break;

            case 'r':
                fStrict = false;
                break;

            case 's':
                fStrict = true;
                break;

            case '#': /* --comment */
                /* The sole purpose of this option is to allow identification of DHCP
                 * server instances in the process list. We ignore the required string
                 * argument of this option. */
                pszComment = ValueUnion.psz;
                break;

            default:
                RTGetOptPrintError(rc, &ValueUnion);
                return NULL;
        }
    }

    if (!pszConfig)
    {
        RTMsgError("No configuration file specified (--config file)!\n");
        return NULL;
    }

    /*
     * Init the log if a log file was specified.
     */
    if (pszLogFile)
    {
        rc = i_logInitWithFilename(pszLogFile);
        if (RT_FAILURE(rc))
            RTMsgError("Failed to initialize log file '%s': %Rrc", pszLogFile, rc);

        if (pszLogDestinations)
            RTLogDestinations(RTLogRelGetDefaultInstance(), pszLogDestinations);
        if (pszLogFlags)
            RTLogFlags(RTLogRelGetDefaultInstance(), pszLogFlags);
        if (pszLogGroupSettings)
            RTLogGroupSettings(RTLogRelGetDefaultInstance(), pszLogGroupSettings);

        LogRel(("--config:  %s\n", pszComment));
        if (pszComment)
            LogRel(("--comment: %s\n", pszComment));
    }

    /*
     * Read the log file.
     */
    RTMsgInfo("reading config from '%s'...\n", pszConfig);
    std::unique_ptr<Config> ptrConfig;
    ptrConfig.reset(Config::i_read(pszConfig, fStrict));
    if (ptrConfig.get() != NULL)
    {
        rc = ptrConfig->i_complete();
        if (RT_SUCCESS(rc))
            return ptrConfig.release();
    }
    return NULL;
}


/**
 *
 * @note The release log has is not operational when this method is called.
 */
Config *Config::i_read(const char *pszFileName, bool fStrict) RT_NOEXCEPT
{
    if (pszFileName == NULL || pszFileName[0] == '\0')
    {
        DHCP_LOG_MSG_ERROR(("Config::i_read: Empty configuration filename\n"));
        return NULL;
    }

    xml::Document doc;
    try
    {
        xml::XmlFileParser parser;
        parser.read(pszFileName, doc);
    }
    catch (const xml::EIPRTFailure &e)
    {
        DHCP_LOG_MSG_ERROR(("Config::i_read: %s\n", e.what()));
        return NULL;
    }
    catch (const RTCError &e)
    {
        DHCP_LOG_MSG_ERROR(("Config::i_read: %s\n", e.what()));
        return NULL;
    }
    catch (...)
    {
        DHCP_LOG_MSG_ERROR(("Config::i_read: Unknown exception while reading and parsing '%s'\n", pszFileName));
        return NULL;
    }

    std::unique_ptr<Config> config(i_createInstanceAndCallInit());
    AssertReturn(config.get() != NULL, NULL);

    try
    {
        config->i_parseConfig(doc.getRootElement(), fStrict);
    }
    catch (const RTCError &e)
    {
        DHCP_LOG_MSG_ERROR(("Config::i_read: %s\n", e.what()));
        return NULL;
    }
    catch (std::bad_alloc &)
    {
        DHCP_LOG_MSG_ERROR(("Config::i_read: std::bad_alloc\n"));
        return NULL;
    }
    catch (...)
    {
        DHCP_LOG_MSG_ERROR(("Config::i_read: Unexpected exception\n"));
        return NULL;
    }

    return config.release();
}


/**
 * Helper for retrieving a IPv4 attribute.
 *
 * @param   pElm            The element to get the attribute from.
 * @param   pszAttrName     The name of the attribute
 * @param   pAddr           Where to return the address.
 * @throws  ConfigFileError
 */
static void getIPv4AddrAttribute(const xml::ElementNode *pElm, const char *pszAttrName, PRTNETADDRIPV4 pAddr)
{
    const char *pszAttrValue;
    if (pElm->getAttributeValue(pszAttrName, &pszAttrValue))
    {
        int rc = RTNetStrToIPv4Addr(pszAttrValue, pAddr);
        if (RT_SUCCESS(rc))
            return;
        throw ConfigFileError(pElm, "Attribute %s is not a valid IPv4 address: '%s' -> %Rrc", pszAttrName, pszAttrValue, rc);
    }
    throw ConfigFileError(pElm, "Required %s attribute missing", pszAttrName);
}


/**
 * Helper for retrieving a MAC address attribute.
 *
 * @param   pElm            The element to get the attribute from.
 * @param   pszAttrName     The name of the attribute
 * @param   pMacAddr        Where to return the MAC address.
 * @throws  ConfigFileError
 */
static void getMacAddressAttribute(const xml::ElementNode *pElm, const char *pszAttrName, PRTMAC pMacAddr)
{
    const char *pszAttrValue;
    if (pElm->getAttributeValue(pszAttrName, &pszAttrValue))
    {
        int rc = RTNetStrToMacAddr(pszAttrValue, pMacAddr);
        if (RT_SUCCESS(rc) && rc != VWRN_TRAILING_CHARS)
            return;
        throw ConfigFileError(pElm, "attribute %s is not a valid MAC address: '%s' -> %Rrc", pszAttrName, pszAttrValue, rc);
    }
    throw ConfigFileError(pElm, "Required %s attribute missing", pszAttrName);
}


/**
 * Internal worker for i_read() that parses the root element and everything
 * below it.
 *
 * @param   pElmRoot            The root element.
 * @param   fStrict             Set if we're in strict mode, clear if we just
 *                              want to get on with it if we can.
 * @throws  std::bad_alloc, ConfigFileError
 */
void Config::i_parseConfig(const xml::ElementNode *pElmRoot, bool fStrict)
{
    /*
     * Check the root element and call i_parseServer to do real work.
     */
    if (pElmRoot == NULL)
        throw ConfigFileError("Empty config file");

    /** @todo XXX: NAMESPACE API IS COMPLETELY BROKEN, SO IGNORE IT FOR NOW */

    if (!pElmRoot->nameEquals("DHCPServer"))
        throw ConfigFileError("Unexpected root element '%s'", pElmRoot->getName());

    i_parseServer(pElmRoot, fStrict);

#if 0 /** @todo convert to LogRel2 stuff */
    // XXX: debug
    for (optmap_t::const_iterator it = m_GlobalOptions.begin(); it != m_GlobalOptions.end(); ++it) {
        std::shared_ptr<DhcpOption> opt(it->second);

        octets_t data;
        opt->encode(data);

        bool space = false;
        for (octets_t::const_iterator itData = data.begin(); itData != data.end(); ++itData) {
            uint8_t c = *itData;
            if (space)
                std::cout << " ";
            else
                space = true;
            std::cout << (int)c;
        }
        std::cout << std::endl;
    }
#endif
}


/**
 * Internal worker for parsing the elements under /DHCPServer/.
 *
 * @param   pElmServer          The DHCPServer element.
 * @param   fStrict             Set if we're in strict mode, clear if we just
 *                              want to get on with it if we can.
 * @throws  std::bad_alloc, ConfigFileError
 */
void Config::i_parseServer(const xml::ElementNode *pElmServer, bool fStrict)
{
    /*
     * <DHCPServer> attributes
     */
    if (!pElmServer->getAttributeValue("networkName", m_strNetwork))
        throw ConfigFileError("DHCPServer/@networkName missing");
    if (m_strNetwork.isEmpty())
        throw ConfigFileError("DHCPServer/@networkName is empty");

    const char *pszTrunkType;
    if (!pElmServer->getAttributeValue("trunkType", &pszTrunkType))
        throw ConfigFileError("DHCPServer/@trunkType missing");
    if (strcmp(pszTrunkType, "none") == 0)
        m_enmTrunkType = kIntNetTrunkType_None;
    else if (strcmp(pszTrunkType, "whatever") == 0)
        m_enmTrunkType = kIntNetTrunkType_WhateverNone;
    else if (strcmp(pszTrunkType, "netflt") == 0)
        m_enmTrunkType = kIntNetTrunkType_NetFlt;
    else if (strcmp(pszTrunkType, "netadp") == 0)
        m_enmTrunkType = kIntNetTrunkType_NetAdp;
    else
        throw ConfigFileError("Invalid DHCPServer/@trunkType value: %s", pszTrunkType);

    if (   m_enmTrunkType == kIntNetTrunkType_NetFlt
        || m_enmTrunkType == kIntNetTrunkType_NetAdp)
    {
        if (!pElmServer->getAttributeValue("trunkName", &m_strTrunk))
            throw ConfigFileError("DHCPServer/@trunkName missing");
    }
    else
        m_strTrunk = "";

    m_strLeasesFilename = pElmServer->findAttributeValue("leasesFilename"); /* optional */
    if (m_strLeasesFilename.isEmpty())
    {
        int rc = m_strLeasesFilename.assignNoThrow(getHome());
        if (RT_SUCCESS(rc))
            rc = RTPathAppendCxx(m_strLeasesFilename, m_strNetwork);
        if (RT_SUCCESS(rc))
            rc = m_strLeasesFilename.appendNoThrow("-Dhcpd.leases");
        if (RT_FAILURE(rc))
            throw ConfigFileError("Unexpected error constructing default m_strLeasesFilename value: %Rrc", rc);
        RTPathPurgeFilename(RTPathFilename(m_strLeasesFilename.mutableRaw()), RTPATH_STR_F_STYLE_HOST);
        m_strLeasesFilename.jolt();
    }

    ::getIPv4AddrAttribute(pElmServer, "IPAddress", &m_IPv4Address);
    ::getIPv4AddrAttribute(pElmServer, "networkMask", &m_IPv4Netmask);
    ::getIPv4AddrAttribute(pElmServer, "lowerIP", &m_IPv4PoolFirst);
    ::getIPv4AddrAttribute(pElmServer, "upperIP", &m_IPv4PoolLast);

    /*
     * <DHCPServer> children
     */
    xml::NodesLoop it(*pElmServer);
    const xml::ElementNode *pElmChild;
    while ((pElmChild = it.forAllNodes()) != NULL)
    {
        /* Global options: */
        if (pElmChild->nameEquals("Options"))
            m_GlobalConfig.initFromXml(pElmChild, fStrict);
        /* Group w/ options: */
        else if (pElmChild->nameEquals("Group"))
        {
            std::unique_ptr<GroupConfig> ptrGroup(new GroupConfig());
            ptrGroup->initFromXml(pElmChild, fStrict);
            if (m_GroupConfigs.find(ptrGroup->getGroupName()) == m_GroupConfigs.end())
            {
                m_GroupConfigs[ptrGroup->getGroupName()] = ptrGroup.get();
                ptrGroup.release();
            }
            else if (!fStrict)
                LogRelFunc(("Ignoring duplicate group name: %s", ptrGroup->getGroupName().c_str()));
            else
                throw ConfigFileError("Duplicate group name: %s", ptrGroup->getGroupName().c_str());
        }
        /*
         * MAC address and per VM NIC configurations:
         */
        else if (pElmChild->nameEquals("Config"))
        {
            std::unique_ptr<HostConfig> ptrHost(new HostConfig());
            ptrHost->initFromXml(pElmChild, fStrict);
            if (m_HostConfigs.find(ptrHost->getMACAddress()) == m_HostConfigs.end())
            {
                m_HostConfigs[ptrHost->getMACAddress()] = ptrHost.get();
                ptrHost.release();
            }
            else if (!fStrict)
                LogRelFunc(("Ignorining duplicate MAC address (Config): %RTmac", &ptrHost->getMACAddress()));
            else
                throw ConfigFileError("Duplicate MAC address (Config): %RTmac", &ptrHost->getMACAddress());
        }
        else if (!fStrict)
            LogRel(("Ignoring unexpected DHCPServer child: %s\n", pElmChild->getName()));
        else
            throw ConfigFileError("Unexpected DHCPServer child <%s>'", pElmChild->getName());
    }
}


/**
 * Internal worker for parsing \<Option\> elements found under
 * /DHCPServer/Options/, /DHCPServer/Group/ and /DHCPServer/Config/.
 *
 * @param   pElmOption          An \<Option\> element.
 * @throws  std::bad_alloc, ConfigFileError
 */
void ConfigLevelBase::i_parseOption(const xml::ElementNode *pElmOption)
{
    /* The 'name' attribute: */
    const char *pszName;
    if (!pElmOption->getAttributeValue("name", &pszName))
        throw ConfigFileError(pElmOption, "missing option name");

    uint8_t u8Opt;
    int rc = RTStrToUInt8Full(pszName, 10, &u8Opt);
    if (rc != VINF_SUCCESS) /* no warnings either */
        throw ConfigFileError(pElmOption, "Bad option name '%s': %Rrc", pszName, rc);

    /* The opional 'encoding' attribute: */
    uint32_t u32Enc = 0;            /* XXX: DHCPOptionEncoding_Normal */
    const char *pszEncoding;
    if (pElmOption->getAttributeValue("encoding", &pszEncoding))
    {
        rc = RTStrToUInt32Full(pszEncoding, 10, &u32Enc);
        if (rc != VINF_SUCCESS) /* no warnings either */
            throw ConfigFileError(pElmOption, "Bad option encoding '%s': %Rrc", pszEncoding, rc);

        switch (u32Enc)
        {
            case 0:                 /* XXX: DHCPOptionEncoding_Normal */
            case 1:                 /* XXX: DHCPOptionEncoding_Hex */
                break;
            default:
                throw ConfigFileError(pElmOption, "Unknown encoding '%s'", pszEncoding);
        }
    }

    /* The 'value' attribute. May be omitted for OptNoValue options like rapid commit. */
    const char *pszValue;
    if (!pElmOption->getAttributeValue("value", &pszValue))
        pszValue = "";

    /** @todo XXX: TODO: encoding, handle hex */
    DhcpOption *opt = DhcpOption::parse(u8Opt, u32Enc, pszValue);
    if (opt == NULL)
        throw ConfigFileError(pElmOption, "Bad option '%s' (encoding %u): '%s' ", pszName, u32Enc, pszValue ? pszValue : "");

    /* Add it to the map: */
    m_Options << opt;
}


/**
 * Final children parser, handling only \<Option\> and barfing at anything else.
 *
 * @param   pElmChild           The child element to handle.
 * @param   fStrict             Set if we're in strict mode, clear if we just
 *                              want to get on with it if we can.
 * @throws  std::bad_alloc, ConfigFileError
 */
void ConfigLevelBase::i_parseChild(const xml::ElementNode *pElmChild, bool fStrict)
{
    if (pElmChild->nameEquals("Option"))
    {
        try
        {
            i_parseOption(pElmChild);
        }
        catch (ConfigFileError &rXcpt)
        {
            if (fStrict)
                throw rXcpt;
            LogRelFunc(("Ignoring option: %s\n", rXcpt.what()));
        }
    }
    else if (!fStrict)
    {
        ConfigFileError Dummy(pElmChild->getParent(), "Unexpected child '%s'", pElmChild->getName());
        LogRelFunc(("%s\n", Dummy.what()));
    }
    else
        throw ConfigFileError(pElmChild->getParent(), "Unexpected child '%s'", pElmChild->getName());
}


/**
 * Base class initialization taking a /DHCPServer/Options, /DHCPServer/Group or
 * /DHCPServer/Config element as input and handling common attributes as well as
 * any \<Option\> children.
 *
 * @param   pElmConfig          The configuration element to parse.
 * @param   fStrict             Set if we're in strict mode, clear if we just
 *                              want to get on with it if we can.
 * @throws  std::bad_alloc, ConfigFileError
 */
void ConfigLevelBase::initFromXml(const xml::ElementNode *pElmConfig, bool fStrict)
{
    /*
     * Common attributes:
     */
    if (!pElmConfig->getAttributeValue("secMinLeaseTime", &m_secMinLeaseTime))
        m_secMinLeaseTime = 0;
    if (!pElmConfig->getAttributeValue("secDefaultLeaseTime", &m_secDefaultLeaseTime))
        m_secDefaultLeaseTime = 0;
    if (!pElmConfig->getAttributeValue("secMaxLeaseTime", &m_secMaxLeaseTime))
        m_secMaxLeaseTime = 0;

    /*
     * Parse children.
     */
    xml::NodesLoop it(*pElmConfig);
    const xml::ElementNode *pElmChild;
    while ((pElmChild = it.forAllNodes()) != NULL)
        i_parseChild(pElmChild, fStrict);
}


/**
 * Internal worker for parsing the elements under /DHCPServer/Options/.
 *
 * @param   pElmOptions         The <Options> element.
 * @param   fStrict             Set if we're in strict mode, clear if we just
 *                              want to get on with it if we can.
 * @throws  std::bad_alloc, ConfigFileError
 */
void GlobalConfig::initFromXml(const xml::ElementNode *pElmOptions, bool fStrict)
{
    ConfigLevelBase::initFromXml(pElmOptions, fStrict);
}


/**
 * Overrides base class to handle the condition elements under \<Group\>.
 *
 * @param   pElmChild           The child element.
 * @param   fStrict             Set if we're in strict mode, clear if we just
 *                              want to get on with it if we can.
 * @throws  std::bad_alloc, ConfigFileError
 */
void GroupConfig::i_parseChild(const xml::ElementNode *pElmChild, bool fStrict)
{
    if (pElmChild->nameEquals("ConditionMAC"))
    { }
    else if (pElmChild->nameEquals("ConditionMACWildcard"))
    { }
    else if (pElmChild->nameEquals("ConditionVendorClassID"))
    { }
    else if (pElmChild->nameEquals("ConditionVendorClassIDWildcard"))
    { }
    else if (pElmChild->nameEquals("ConditionUserClassID"))
    { }
    else if (pElmChild->nameEquals("ConditionUserClassIDWildcard"))
    { }
    else
    {
        ConfigLevelBase::i_parseChild(pElmChild, fStrict);
        return;
    }
}


/**
 * Internal worker for parsing the elements under /DHCPServer/Group/.
 *
 * @param   pElmGroup           The \<Group\> element.
 * @param   fStrict             Set if we're in strict mode, clear if we just
 *                              want to get on with it if we can.
 * @throws  std::bad_alloc, ConfigFileError
 */
void GroupConfig::initFromXml(const xml::ElementNode *pElmGroup, bool fStrict)
{
    /*
     * Attributes:
     */
    if (!pElmGroup->getAttributeValue("name", m_strName) || m_strName.isEmpty())
    {
        if (fStrict)
            throw ConfigFileError(pElmGroup, "Group as no name or the name is empty");
        m_strName.printf("Group#%u", s_uGroupNo++);
    }

    /*
     * Do common initialization (including children).
     */
    ConfigLevelBase::initFromXml(pElmGroup, fStrict);
}


/**
 * Internal worker for parsing the elements under /DHCPServer/Config/.
 *
 * VM Config entries are generated automatically from VirtualBox.xml
 * with the MAC fetched from the VM config.  The client id is nowhere
 * in the picture there, so VM config is indexed with plain RTMAC, not
 * ClientId (also see getOptions below).
 *
 * @param   pElmConfig          The \<Config\> element.
 * @param   fStrict             Set if we're in strict mode, clear if we just
 *                              want to get on with it if we can.
 * @throws  std::bad_alloc, ConfigFileError
 */
void HostConfig::initFromXml(const xml::ElementNode *pElmConfig, bool fStrict)
{
    /*
     * Attributes:
     */
    /* The MAC address: */
    ::getMacAddressAttribute(pElmConfig, "MACAddress", &m_MACAddress);

    /* Name - optional: */
    if (!pElmConfig->getAttributeValue("name", m_strName))
        m_strName.printf("MAC:%RTmac", m_MACAddress);

    /* Fixed IP address assignment - optional: */
    const char *pszFixedAddress = pElmConfig->findAttributeValue("FixedIPAddress");
    if (!pszFixedAddress || *RTStrStripL(pszFixedAddress) == '\0')
        m_fHaveFixedAddress = false;
    else
    {
        m_fHaveFixedAddress = false;
        ::getIPv4AddrAttribute(pElmConfig, "FixedIPAddress", &m_FixedAddress);
    }

    /*
     * Do common initialization.
     */
    ConfigLevelBase::initFromXml(pElmConfig, fStrict);
}


/**
 * Assembles a priorities vector of configurations for the client.
 *
 * @returns a_rRetConfigs for convenience.
 * @param   a_rRetConfigs       Where to return the configurations.
 * @param   a_ridClient         The client ID.
 * @param   a_ridVendorClass    The vendor class ID if present.
 * @param   a_ridUserClass      The user class ID if present
 */
Config::ConfigVec &Config::getConfigsForClient(Config::ConfigVec &a_rRetConfigs, const ClientId &a_ridClient,
                                               const OptVendorClassId &a_ridVendorClass,
                                               const OptUserClassId &a_ridUserClass) const
{
    /* Host specific config first: */
    HostConfigMap::const_iterator itHost = m_HostConfigs.find(a_ridClient.mac());
    if (itHost != m_HostConfigs.end())
        a_rRetConfigs.push_back(itHost->second);

    /* Groups: */
    RT_NOREF(a_ridVendorClass, a_ridUserClass); /* not yet */

    /* Global: */
    a_rRetConfigs.push_back(&m_GlobalConfig);

    return a_rRetConfigs;
}


/**
 * Method used by DHCPD to assemble a list of options for the client.
 *
 * @returns a_rRetOpts for convenience
 * @param   a_rRetOpts      Where to put the requested options.
 * @param   a_rReqOpts      The requested options.
 * @param   a_rConfigs      Relevant configurations returned by
 *                          Config::getConfigsForClient().
 *
 * @throws  std::bad_alloc
 */
optmap_t &Config::getOptionsForClient(optmap_t &a_rRetOpts, const OptParameterRequest &a_rReqOpts, ConfigVec &a_rConfigs) const
{
    /*
     * Always supply the subnet:
     */
    a_rRetOpts << new OptSubnetMask(m_IPv4Netmask);

    /*
     * Try provide the requested options:
     */
    const OptParameterRequest::value_t &reqValue = a_rReqOpts.value();
    for (octets_t::const_iterator itOptReq = reqValue.begin(); itOptReq != reqValue.end(); ++itOptReq)
    {
        uint8_t bOptReq = *itOptReq;
        LogRel2((">>> requested option %d (%#x)\n", bOptReq, bOptReq));

        if (bOptReq != OptSubnetMask::optcode)
        {
            bool fFound = false;
            for (size_t i = 0; i < a_rConfigs.size(); i++)
            {
                optmap_t::const_iterator itFound;
                if (a_rConfigs[i]->findOption(bOptReq, itFound)) /* crap interface */
                {
                    LogRel2(("... found in %s (type %s)\n", a_rConfigs[i]->getName(), a_rConfigs[i]->getType()));
                    a_rRetOpts << itFound->second;
                    fFound = true;
                    break;
                }
            }
            if (!fFound)
                LogRel3(("... not found\n"));
        }
        else
            LogRel2(("... always supplied\n"));
    }


#if 0 /* bird disabled this as it looks dubious and testing only. */
    /** @todo XXX: testing ... */
    if (vmopts != NULL)
    {
        for (optmap_t::const_iterator it = vmopts->begin(); it != vmopts->end(); ++it)
        {
            std::shared_ptr<DhcpOption> opt(it->second);
            if (a_rRetOpts.count(opt->optcode()) == 0 && opt->optcode() > 127)
            {
                a_rRetOpts << opt;
                LogRel2(("... forcing VM option %d (%#x)\n", opt->optcode(), opt->optcode()));
            }
        }
    }

    for (optmap_t::const_iterator it = m_GlobalOptions.begin(); it != m_GlobalOptions.end(); ++it)
    {
        std::shared_ptr<DhcpOption> opt(it->second);
        if (a_rRetOpts.count(opt->optcode()) == 0 && opt->optcode() > 127)
        {
            a_rRetOpts << opt;
            LogRel2(("... forcing global option %d (%#x)", opt->optcode(), opt->optcode()));
        }
    }
#endif

    return a_rRetOpts;
}
