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

#include <iprt/types.h>
#include <iprt/net.h>           /* NB: must come before getopt.h */
#include <iprt/getopt.h>
#include <iprt/path.h>
#include <iprt/string.h>

#include "Config.h"


bool Config::isSane()
{
    int rc;

    /* unicast MAC address */
    if (m_MacAddress.au8[0] & 0x01)
        return false;

    /* unicast IP address */
    if ((m_IPv4Address.au8[0] & 0xe0) == 0xe0)
        return false;

    /* valid netmask */
    int iPrefixLengh;
    rc = RTNetMaskToPrefixIPv4(&m_IPv4Netmask, &iPrefixLengh);
    if (RT_FAILURE(rc) || iPrefixLengh == 0)
        return false;

    /* first IP is from the same network */
    if ((m_IPv4PoolFirst.u & m_IPv4Netmask.u) != (m_IPv4Address.u & m_IPv4Netmask.u))
        return false;

    /* last IP is from the same network */
    if ((m_IPv4PoolLast.u & m_IPv4Netmask.u) != (m_IPv4Address.u & m_IPv4Netmask.u))
        return false;

    /* the pool is valid */
    if (RT_N2H_U32(m_IPv4PoolLast.u) < RT_N2H_U32(m_IPv4PoolFirst.u))
        return false;

    return true;
}


Config *Config::hardcoded()
{
    std::unique_ptr<Config> config(new Config());

    config->m_strNetwork.assign("HostInterfaceNetworking-vboxnet0");
    config->sanitizeBaseName(); /* nop, but be explicit */

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

    AssertReturn(config->isSane(), NULL);
    return config.release();
}


/* compatibility with old VBoxNetDHCP */
static RTGETOPTDEF g_aCompatOptions[] =
{
    { "--ip-address",     'i',   RTGETOPT_REQ_IPV4ADDR },
    { "--lower-ip",       'l',   RTGETOPT_REQ_IPV4ADDR },
    { "--mac-address",    'a',   RTGETOPT_REQ_MACADDR },
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
                      g_aCompatOptions, RT_ELEMENTS(g_aCompatOptions), 0,
                      RTGETOPTINIT_FLAGS_NO_STD_OPTS);

    std::unique_ptr<Config> config(new Config());
    for (;;)
    {
        RTGETOPTUNION Val;

        rc = RTGetOpt(&State, &Val);
        if (rc == 0)            /* done */
            break;

        switch (rc)
        {
            case 'a': /* --mac-address */
                config->m_MacAddress = Val.MacAddr;
                break;

            case 'i': /* --ip-address */
                config->m_IPv4Address = Val.IPv4Addr;
                break;

            case 'l': /* --lower-ip */
                config->m_IPv4PoolFirst = Val.IPv4Addr;
                break;

            case 'm': /* --netmask */
                config->m_IPv4Netmask = Val.IPv4Addr;
                break;

            case 'n': /* --network */
                config->m_strNetwork.assign(Val.psz);
                break;

            case 't': /* --trunk-name */
                config->m_strTrunk.assign(Val.psz);
                break;

            case 'T': /* --trunk-type */
                if (strcmp(Val.psz, "none") == 0)
                    config->m_enmTrunkType = kIntNetTrunkType_None;
                else if (strcmp(Val.psz, "whatever") == 0)
                    config->m_enmTrunkType = kIntNetTrunkType_WhateverNone;
                else if (strcmp(Val.psz, "netflt") == 0)
                    config->m_enmTrunkType = kIntNetTrunkType_NetFlt;
                else if (strcmp(Val.psz, "netadp") == 0)
                    config->m_enmTrunkType = kIntNetTrunkType_NetAdp;
                else
                    return NULL;
                break;

            case 'u': /* --upper-ip */
                config->m_IPv4PoolLast = Val.IPv4Addr;
                break;
        }
    }

    config->sanitizeBaseName();

    if (!config->isSane())
        return NULL;

    return config.release();
}


Config *Config::read(const char *pszFileName)
{
    RT_NOREF(pszFileName);
    return NULL;                /* not yet */
}


/*
 * Set m_strBaseName to sanitized version of m_strNetwork that can be
 * used in a path component.
 */
void Config::sanitizeBaseName()
{
    int rc;

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

    fillDefaultOptions(optmap, reqOpts);
    fillVendorOptions(optmap, reqOpts, vendor);
    fillHostOptions(optmap, reqOpts, id);

    return optmap;
}


void Config::fillDefaultOptions(optmap_t &optmap,
                                const OptParameterRequest &reqOpts) const
{
    RT_NOREF(reqOpts);

    optmap << new OptSubnetMask(m_IPv4Netmask);
}


void Config::fillHostOptions(optmap_t &optmap,
                             const OptParameterRequest &reqOpts,
                             const ClientId &id) const
{
    /* not yet */
    RT_NOREF(optmap, reqOpts, id);
}


void Config::fillVendorOptions(optmap_t &optmap,
                               const OptParameterRequest &reqOpts,
                               const OptVendorClassId &vendor) const
{
    if (!vendor.present())
        return;

    /* may be some day... */
    RT_NOREF(optmap, reqOpts);
}
