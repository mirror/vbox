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

#include <VBox/com/com.h>       /* For log initialization. */

#include "Config.h"


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
};


/**
 * Private default constructor, external users use factor methods.
 */
Config::Config()
    : m_strHome()
    , m_strNetwork()
    , m_strBaseName()
    , m_strTrunk()
    , m_enmTrunkType(kIntNetTrunkType_Invalid)
    , m_MacAddress()
    , m_IPv4Address()
    , m_IPv4Netmask()
    , m_IPv4PoolFirst()
    , m_IPv4PoolLast()
    , m_GlobalOptions()
    , m_VMMap()
{
}


/**
 * Initializes the object.
 *
 * @returns IPRT status code.
 */
int Config::i_init()
{
    return i_homeInit();
}


/**
 * Initializes the m_strHome member with the path to ~/.VirtualBox or equivalent.
 *
 * @returns IPRT status code.
 * @todo Too many init functions?
 */
int Config::i_homeInit()
{
    char szHome[RTPATH_MAX];
    int rc = com::GetVBoxUserHomeDirectory(szHome, sizeof(szHome), false);
    if (RT_SUCCESS(rc))
        rc = m_strHome.assignNoThrow(szHome);
    else
    {
        LogFunc(("unable to locate the VirtualBox home directory: %Rrc", rc)); /* no release log at this point. */
        RTMsgError("unable to locate the VirtualBox home directory: %Rrs", rc);
    }
    return rc;
}


/**
 * Internal worker for the public factory methods that creates an instance and
 * calls i_init() on it.
 *
 * @returns Config instance on success, NULL on failure.
 */
/*static*/ Config *Config::i_createInstanceAndCallInit()
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
 * Internal network name (m_strNetwork) setter.
 *
 * Can only be called once.  Will also invoke i_sanitizeBaseName to update
 * m_strBaseName.
 *
 * @throws std::bad_alloc
 */
void Config::i_setNetwork(const RTCString &aStrNetwork)
{
    AssertReturnVoid(m_strNetwork.isEmpty());

    m_strNetwork = aStrNetwork;
    i_sanitizeBaseName();
}


/**
 * Interal worker for i_setNetwork() that sets m_strBaseName to sanitized the
 * version of m_strNetwork suitable for use as a path component.
 */
void Config::i_sanitizeBaseName()
{
    if (m_strNetwork.isNotEmpty())
    {
        m_strBaseName = m_strNetwork;

/** @todo make IPRT function for this.   */
        char ch;
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
        static const char s_szIllegals[] = "/\\\"*:<>?|\t\v\n\r\f\a\b"; /** @todo all control chars... */
        for (char *psz = m_strBaseName.mutableRaw(); (ch = *psz) != '\0'; ++psz)
            if (strchr(s_szIllegals, ch))
                *psz = '_';
#else
        for (char *psz = m_strBaseName.mutableRaw(); (ch = *psz) != '\0'; ++psz)
            if (RTPATH_IS_SEP(ch))
                *psz = '_';
#endif
        m_strBaseName.jolt(); /* Not really necessary, but it's protocol. */
    }
    else
        m_strBaseName.setNull();
}


/**
 * Worker for i_complete() that initializes the release log of the process.
 *
 * Requires network name to be known as the log file name depends on
 * it.  Alternatively, consider passing the log file name via the
 * command line?
 *
 * @todo make the log file directly configurable?
 */
int Config::i_logInit()
{
    if (m_strHome.isEmpty() || m_strBaseName.isEmpty())
        return VERR_PATH_ZERO_LENGTH;

    /* default log file name */
    char szLogFile[RTPATH_MAX];
    ssize_t cch = RTStrPrintf2(szLogFile, sizeof(szLogFile),
                               "%s%c%s-Dhcpd.log",
                               m_strHome.c_str(), RTPATH_DELIMITER, m_strBaseName.c_str());
    if (cch <= 0)
        return VERR_BUFFER_OVERFLOW;

    /* Sanitize base name some more to be usable in an environment variable name: */
    char szEnvVarBase[128];
    cch = RTStrPrintf(szEnvVarBase, sizeof(szEnvVarBase), "VBOXDHCP_%s_RELEASE_LOG", m_strBaseName.c_str());
    if (cch <= 0)
        return VERR_BUFFER_OVERFLOW;
    for (char *p = szEnvVarBase; *p != '\0'; ++p)
        if (*p != '_' && !RT_C_IS_ALNUM(*p))
            *p = '_';

    int rc = com::VBoxLogRelCreate("DHCP Server",
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


/**
 * Post process and validate the configuration after it has been loaded.
 */
int Config::i_complete()
{
    if (m_strNetwork.isEmpty())
    {
        LogDHCP(("network name is not specified\n"));
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
        AssertReturn(rc, rc);

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
    int cPrefixBits;
    int rc = RTNetMaskToPrefixIPv4(&m_IPv4Netmask, &cPrefixBits);
    if (RT_FAILURE(rc) || cPrefixBits == 0)
    {
        LogDHCP(("IP mask is not valid: %RTnaipv4\n", m_IPv4Netmask.u));
        return VERR_GENERAL_FAILURE;
    }

    /* first IP is from the same network */
    if ((m_IPv4PoolFirst.u & m_IPv4Netmask.u) != (m_IPv4Address.u & m_IPv4Netmask.u))
    {
        LogDHCP(("first pool address is outside the network %RTnaipv4/%d: %RTnaipv4\n",
                 (m_IPv4Address.u & m_IPv4Netmask.u), cPrefixBits, m_IPv4PoolFirst.u));
        return VERR_GENERAL_FAILURE;
    }

    /* last IP is from the same network */
    if ((m_IPv4PoolLast.u & m_IPv4Netmask.u) != (m_IPv4Address.u & m_IPv4Netmask.u))
    {
        LogDHCP(("last pool address is outside the network %RTnaipv4/%d: %RTnaipv4\n",
                 (m_IPv4Address.u & m_IPv4Netmask.u), cPrefixBits, m_IPv4PoolLast.u));
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
        && RT_N2H_U32(m_IPv4Address.u)   <= RT_N2H_U32(m_IPv4PoolLast.u))
    {
        LogDHCP(("server address inside the pool range %RTnaipv4 - %RTnaipv4: %RTnaipv4\n",
                 m_IPv4PoolFirst.u, m_IPv4PoolLast.u, m_IPv4Address.u));
        return VERR_GENERAL_FAILURE;
    }

    if (!fMACGenerated)
        LogDHCP(("MAC address %RTmac\n", &m_MacAddress));
    LogDHCP(("IP address %RTnaipv4/%d\n", m_IPv4Address.u, cPrefixBits));
    LogDHCP(("address pool %RTnaipv4 - %RTnaipv4\n", m_IPv4PoolFirst.u, m_IPv4PoolLast.u));

    return VINF_SUCCESS;
}


/*static*/ Config *Config::hardcoded()
{
    std::unique_ptr<Config> config(i_createInstanceAndCallInit());
    AssertReturn(config.get() != NULL, NULL);
    try
    {
        config->i_setNetwork("HostInterfaceNetworking-vboxnet0");
        config->m_strTrunk.assign("vboxnet0");
    }
    catch (std::bad_alloc &)
    {
        return NULL;
    }
    config->m_enmTrunkType = kIntNetTrunkType_NetFlt;

    config->m_MacAddress.au8[0] = 0x08;
    config->m_MacAddress.au8[1] = 0x00;
    config->m_MacAddress.au8[2] = 0x27;
    config->m_MacAddress.au8[3] = 0xa9;
    config->m_MacAddress.au8[4] = 0xcf;
    config->m_MacAddress.au8[5] = 0xef;


    config->m_IPv4Address.u     = RT_H2N_U32_C(0xc0a838fe); /* 192.168.56.254 */
    config->m_IPv4Netmask.u     = RT_H2N_U32_C(0xffffff00); /* 255.255.255.0 */

    /* flip to test naks */
#if 1
    config->m_IPv4PoolFirst.u   = RT_H2N_U32_C(0xc0a8385a); /* 192.168.56.90 */
    config->m_IPv4PoolLast.u    = RT_H2N_U32_C(0xc0a83863); /* 192.168.56.99 */
#else
    config->m_IPv4PoolFirst.u   = RT_H2N_U32_C(0xc0a838c9); /* 192.168.56.201 */
    config->m_IPv4PoolLast.u    = RT_H2N_U32_C(0xc0a838dc); /* 192.168.56.220 */
#endif

    int rc = config->i_complete();
    AssertRCReturn(rc, NULL);

    return config.release();
}



/*static*/ Config *Config::compat(int argc, char **argv)
{
    /* compatibility with old VBoxNetDHCP */
    static const RTGETOPTDEF s_aCompatOptions[] =
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

    RTGETOPTSTATE State;
    int rc = RTGetOptInit(&State, argc, argv, s_aCompatOptions, RT_ELEMENTS(s_aCompatOptions), 1, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    AssertRCReturn(rc, NULL);

    std::unique_ptr<Config> config(i_createInstanceAndCallInit());
    AssertReturn(config.get() != NULL, NULL);

    for (;;)
    {
        RTGETOPTUNION ValueUnion;
        rc = RTGetOpt(&State, &ValueUnion);
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
                config->m_MacAddress = ValueUnion.MacAddr;
                break;

            case 'i': /* --ip-address */
                if (config->m_IPv4Address.u != 0)
                {
                    RTMsgError("Duplicate --ip-address option");
                    return NULL;
                }
                config->m_IPv4Address = ValueUnion.IPv4Addr;
                break;

            case 'l': /* --lower-ip */
                if (config->m_IPv4PoolFirst.u != 0)
                {
                    RTMsgError("Duplicate --lower-ip option");
                    return NULL;
                }
                config->m_IPv4PoolFirst = ValueUnion.IPv4Addr;
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
                config->m_IPv4Netmask = ValueUnion.IPv4Addr;
                break;

            case 'n': /* --network */
                if (!config->m_strNetwork.isEmpty())
                {
                    RTMsgError("Duplicate --network option");
                    return NULL;
                }
                config->i_setNetwork(ValueUnion.psz);
                break;

            case 't': /* --trunk-name */
                if (!config->m_strTrunk.isEmpty())
                {
                    RTMsgError("Duplicate --trunk-name option");
                    return NULL;
                }
                config->m_strTrunk.assign(ValueUnion.psz);
                break;

            case 'T': /* --trunk-type */
                if (config->m_enmTrunkType != kIntNetTrunkType_Invalid)
                {
                    RTMsgError("Duplicate --trunk-type option");
                    return NULL;
                }
                else if (strcmp(ValueUnion.psz, "none") == 0)
                    config->m_enmTrunkType = kIntNetTrunkType_None;
                else if (strcmp(ValueUnion.psz, "whatever") == 0)
                    config->m_enmTrunkType = kIntNetTrunkType_WhateverNone;
                else if (strcmp(ValueUnion.psz, "netflt") == 0)
                    config->m_enmTrunkType = kIntNetTrunkType_NetFlt;
                else if (strcmp(ValueUnion.psz, "netadp") == 0)
                    config->m_enmTrunkType = kIntNetTrunkType_NetAdp;
                else
                {
                    RTMsgError("Unknown trunk type '%s'", ValueUnion.psz);
                    return NULL;
                }
                break;

            case 'u': /* --upper-ip */
                if (config->m_IPv4PoolLast.u != 0)
                {
                    RTMsgError("Duplicate --upper-ip option");
                    return NULL;
                }
                config->m_IPv4PoolLast = ValueUnion.IPv4Addr;
                break;

            case VINF_GETOPT_NOT_OPTION:
                RTMsgError("%s: Unexpected command line argument", ValueUnion.psz);
                return NULL;

            default:
                RTGetOptPrintError(rc, &ValueUnion);
                return NULL;
        }
    }

    rc = config->i_complete();
    if (RT_FAILURE(rc))
        return NULL;

    return config.release();
}


Config *Config::create(int argc, char **argv)
{
#define DHCPD_GETOPT_COMMENT 256 /* No short option for --comment */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--config",       'c',                  RTGETOPT_REQ_STRING },
        { "--comment",      DHCPD_GETOPT_COMMENT, RTGETOPT_REQ_STRING }
    };

    RTGETOPTSTATE State;
    int rc = RTGetOptInit(&State, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    AssertRCReturn(rc, NULL);

    std::unique_ptr<Config> config;

    for (;;)
    {
        RTGETOPTUNION ValueUnion;
        rc = RTGetOpt(&State, &ValueUnion);
        if (rc == 0)            /* done */
            break;

        switch (rc)
        {
            case 'c': /* --config */
                if (config.get() != NULL)
                {
                    RTMsgError("Duplicate option: --config '%s'\n", ValueUnion.psz);
                    return NULL;
                }

                RTMsgInfo("reading config from '%s'...\n", ValueUnion.psz);
                config.reset(Config::i_read(ValueUnion.psz));
                if (config.get() == NULL)
                    return NULL;
                break;

            case DHCPD_GETOPT_COMMENT: /* --comment */
                /* The sole purpose of this option is to allow identification of DHCP
                 * server instances in the process list. We ignore the required string
                 * argument of this option.
                 */
                continue;

            case VINF_GETOPT_NOT_OPTION:
                RTMsgError("Unexpected command line argument: '%s'", ValueUnion.psz);
                return NULL;

            default:
                RTGetOptPrintError(rc, &ValueUnion);
                return NULL;
        }
    }

    if (config.get() != NULL)
    {
        rc = config->i_complete();
        if (RT_SUCCESS(rc))
            return config.release();
    }
    else
        RTMsgError("No configuration file specified (--config file)!\n");
    return NULL;
}


/**
 *
 * @note The release log has is not operational when this method is called.
 */
Config *Config::i_read(const char *pszFileName)
{
    if (pszFileName == NULL || pszFileName[0] == '\0')
    {
        RTMsgError("Empty configuration filename");
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
        LogFunc(("%s\n", e.what()));
        RTMsgError("%s\n", e.what());
        return NULL;
    }
    catch (const RTCError &e)
    {
        LogFunc(("%s\n", e.what()));
        RTMsgError("%s\n", e.what());
        return NULL;
    }
    catch (...)
    {
        LogFunc(("Unknown exception while reading and parsing '%s'\n", pszFileName));
        RTMsgError("Unknown exception while reading and parsing '%s'\n", pszFileName);
        return NULL;
    }

    std::unique_ptr<Config> config(i_createInstanceAndCallInit());
    AssertReturn(config.get() != NULL, NULL);

    try
    {
        config->i_parseConfig(doc.getRootElement());
    }
    catch (const RTCError &e)
    {
        LogFunc(("%s\n", e.what()));
        RTMsgError("%s\n", e.what());
        return NULL;
    }
    catch (...)
    {
        LogFunc(("Unexpected exception\n"));
        RTMsgError("Unexpected exception\n");
        return NULL;
    }

    return config.release();
}


/**
 * Internal worker for i_read() that parses the root element and everything
 * below it.
 *
 * @throws stuff.
 */
void Config::i_parseConfig(const xml::ElementNode *pElmRoot)
{
    /*
     * Check the root element and call i_parseServer to do real work.
     */
    if (pElmRoot == NULL)
        throw ConfigFileError("Empty config file");

    /** @todo XXX: NAMESPACE API IS COMPLETELY BROKEN, SO IGNORE IT FOR NOW */

    if (!pElmRoot->nameEquals("DHCPServer"))
        throw ConfigFileError("Unexpected root element '%s'", pElmRoot->getName());

    i_parseServer(pElmRoot);

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
 * @throws  ConfigFileError
 */
void Config::i_parseServer(const xml::ElementNode *pElmServer)
{
    /*
     * <DHCPServer> attributes
     */
    const char *pszNetworkName;
    if (pElmServer->getAttributeValue("networkName", &pszNetworkName))
        i_setNetwork(pszNetworkName);
    else
        throw ConfigFileError("DHCPServer/@networkName missing");
    /** @todo log file override.  */
    /** @todo log level (dhcpd group flags).  */
    /** @todo log flags.  */
    /** @todo control logging to stderr/out.  */
    /** @todo if we like we could open the release log now.   */

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

    i_getIPv4AddrAttribute(pElmServer, "IPAddress", &m_IPv4Address);
    i_getIPv4AddrAttribute(pElmServer, "networkMask", &m_IPv4Netmask);
    i_getIPv4AddrAttribute(pElmServer, "lowerIP", &m_IPv4PoolFirst);
    i_getIPv4AddrAttribute(pElmServer, "upperIP", &m_IPv4PoolLast);

    /*
     * <DHCPServer> children
     */
    xml::NodesLoop it(*pElmServer);
    const xml::ElementNode *pElmChild;
    while ((pElmChild = it.forAllNodes()) != NULL)
    {
        /*
         * Global options
         */
        if (pElmChild->nameEquals("Options"))
            i_parseGlobalOptions(pElmChild);
        /*
         * Per-VM configuration
         */
        else if (pElmChild->nameEquals("Config"))
            i_parseVMConfig(pElmChild);
        else
            LogDHCP(("Ignoring unexpected DHCPServer child: %s\n", pElmChild->getName()));
    }
}


/**
 * Internal worker for parsing the elements under /DHCPServer/Options/.
 *
 * @param   pElmServer          The <Options> element.
 * @throws  ConfigFileError
 */
void Config::i_parseGlobalOptions(const xml::ElementNode *options)
{
    xml::NodesLoop it(*options);
    const xml::ElementNode *pElmChild;
    while ((pElmChild = it.forAllNodes()) != NULL)
    {
        if (pElmChild->nameEquals("Option"))
            i_parseOption(pElmChild, m_GlobalOptions);
        else
            throw ConfigFileError("Unexpected element <%s>", pElmChild->getName());
    }
}


/**
 * Internal worker for parsing the elements under /DHCPServer/Config/.
 *
 * VM Config entries are generated automatically from VirtualBox.xml
 * with the MAC fetched from the VM config.  The client id is nowhere
 * in the picture there, so VM config is indexed with plain RTMAC, not
 * ClientId (also see getOptions below).
 *
 * @param   pElmServer          The <Config> element.
 * @throws  ConfigFileError
 */
void Config::i_parseVMConfig(const xml::ElementNode *pElmConfig)
{
    /*
     * Attributes:
     */
    /* The MAC address: */
    RTMAC MacAddr;
    i_getMacAddressAttribute(pElmConfig, "MACAddress", &MacAddr);

    vmmap_t::iterator vmit( m_VMMap.find(MacAddr) );
    if (vmit != m_VMMap.end())
        throw ConfigFileError("Duplicate Config for MACAddress %RTmac", &MacAddr);

    optmap_t &vmopts = m_VMMap[MacAddr];

    /* Name - optional: */
    const char *pszName = NULL;
    if (pElmConfig->getAttributeValue("name", &pszName))
    {
        /** @todo */
    }

    /* Fixed IP address assignment - optional: */
    if (pElmConfig->findAttribute("FixedIPAddress") != NULL)
    {
        /** @todo */
    }

    /*
     * Process the children.
     */
    xml::NodesLoop it(*pElmConfig);
    const xml::ElementNode *pElmChild;
    while ((pElmChild = it.forAllNodes()) != NULL)
        if (pElmChild->nameEquals("Option"))
            i_parseOption(pElmChild, vmopts);
        else
            throw ConfigFileError("Unexpected element '%s' under '%s'", pElmChild->getName(), pElmConfig->getName());
}


/**
 * Internal worker for parsing <Option> elements found under
 * /DHCPServer/Options/ and /DHCPServer/Config/
 *
 * @param   pElmServer          The <Option> element.
 * @param   optmap              The option map to add the option to.
 * @throws  ConfigFileError
 */
void Config::i_parseOption(const xml::ElementNode *pElmOption, optmap_t &optmap)
{
    /* The 'name' attribute: */
    const char *pszName;
    if (!pElmOption->getAttributeValue("name", &pszName))
        throw ConfigFileError("missing option name");

    uint8_t u8Opt;
    int rc = RTStrToUInt8Full(pszName, 10, &u8Opt);
    if (rc != VINF_SUCCESS) /* no warnings either */
        throw ConfigFileError("Bad option name '%s'", pszName);

    /* The opional 'encoding' attribute: */
    uint32_t u32Enc = 0;        /* XXX: DhcpOptEncoding_Legacy */
    const char *pszEncoding;
    if (pElmOption->getAttributeValue("encoding", &pszEncoding))
    {
        rc = RTStrToUInt32Full(pszEncoding, 10, &u32Enc);
        if (rc != VINF_SUCCESS) /* no warnings either */
            throw ConfigFileError("Bad option encoding '%s'", pszEncoding);

        switch (u32Enc)
        {
            case 0:                 /* XXX: DhcpOptEncoding_Legacy */
            case 1:                 /* XXX: DhcpOptEncoding_Hex */
                break;
            default:
                throw ConfigFileError("Unknown encoding '%s'", pszEncoding);
        }
    }

    /* The 'value' attribute. May be omitted for OptNoValue options like rapid commit. */
    const char *pszValue;
    if (!pElmOption->getAttributeValue("value", &pszValue))
        pszValue = "";

    /** @todo XXX: TODO: encoding, handle hex */
    DhcpOption *opt = DhcpOption::parse(u8Opt, u32Enc, pszValue);
    if (opt == NULL)
        throw ConfigFileError("Bad option '%s' (encoding %u): '%s' ", pszName, u32Enc, pszValue ? pszValue : "");

    /* Add it to the map: */
    optmap << opt;
}


/**
 * Helper for retrieving a IPv4 attribute.
 *
 * @param   pElm            The element to get the attribute from.
 * @param   pszAttrName     The name of the attribute
 * @param   pAddr           Where to return the address.
 * @throws  ConfigFileError
 */
/*static*/ void Config::i_getIPv4AddrAttribute(const xml::ElementNode *pElm, const char *pszAttrName, PRTNETADDRIPV4 pAddr)
{
    const char *pszAttrValue;
    if (pElm->getAttributeValue(pszAttrName, &pszAttrValue))
    {
        int rc = RTNetStrToIPv4Addr(pszAttrValue, pAddr);
        if (RT_SUCCESS(rc))
            return;
        throw ConfigFileError("%s attribute %s is not a valid IPv4 address: '%s' -> %Rrc",
                              pElm->getName(), pszAttrName, pszAttrValue, rc);
    }
    else
        throw ConfigFileError("Required %s attribute missing on element %s", pszAttrName, pElm->getName());
}


/**
 * Helper for retrieving a MAC address attribute.
 *
 * @param   pElm            The element to get the attribute from.
 * @param   pszAttrName     The name of the attribute
 * @param   pMacAddr        Where to return the MAC address.
 * @throws  ConfigFileError
 */
/*static*/ void Config::i_getMacAddressAttribute(const xml::ElementNode *pElm, const char *pszAttrName, PRTMAC pMacAddr)
{
    const char *pszAttrValue;
    if (pElm->getAttributeValue(pszAttrName, &pszAttrValue))
    {
        int rc = RTNetStrToMacAddr(pszAttrValue, pMacAddr);
        if (RT_SUCCESS(rc) && rc != VWRN_TRAILING_CHARS)
            return;
        throw ConfigFileError("%s attribute %s is not a valid MAC address: '%s' -> %Rrc",
                              pElm->getName(), pszAttrName, pszAttrValue, rc);
    }
    else
        throw ConfigFileError("Required %s attribute missing on element %s", pszAttrName, pElm->getName());
}


/**
 * Method used by DHCPD to assemble a list of options for the client.
 *
 * @returns a_rRetOpts for convenience
 * @param   a_rRetOpts      Where to put the requested options.
 * @param   reqOpts         The requested options.
 * @param   id              The client ID.
 * @param   idVendorClass   The vendor class ID.
 * @param   idUserClass     The user class ID.
 */
optmap_t &Config::getOptions(optmap_t &a_rRetOpts, const OptParameterRequest &reqOpts, const ClientId &id,
                             const OptVendorClassId &idVendorClass /*= OptVendorClassId()*/,
                             const OptUserClassId &idUserClass /*= OptUserClassId()*/) const
{
    const optmap_t *vmopts = NULL;
    vmmap_t::const_iterator vmit( m_VMMap.find(id.mac()) );
    if (vmit != m_VMMap.end())
        vmopts = &vmit->second;

    RT_NOREF(idVendorClass, idUserClass); /* not yet */

    a_rRetOpts << new OptSubnetMask(m_IPv4Netmask);

    const OptParameterRequest::value_t& reqValue = reqOpts.value();
    for (octets_t::const_iterator itOptReq = reqValue.begin(); itOptReq != reqValue.end(); ++itOptReq)
    {
        uint8_t optreq = *itOptReq;
        LogRel2((">>> requested option %d (%#x)\n", optreq, optreq));

        if (optreq == OptSubnetMask::optcode)
        {
            LogRel2(("... always supplied\n"));
            continue;
        }

        if (vmopts != NULL)
        {
            optmap_t::const_iterator it( vmopts->find(optreq) );
            if (it != vmopts->end())
            {
                a_rRetOpts << it->second;
                LogRel2(("... found in VM options\n"));
                continue;
            }
        }

        optmap_t::const_iterator it( m_GlobalOptions.find(optreq) );
        if (it != m_GlobalOptions.end())
        {
            a_rRetOpts << it->second;
            LogRel2(("... found in global options\n"));
            continue;
        }

        LogRel3(("... not found\n"));
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
