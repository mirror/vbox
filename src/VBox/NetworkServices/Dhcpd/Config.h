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

#ifndef _DHCPD_CONFIG_H_
#define _DHCPD_CONFIG_H_

#include <iprt/types.h>
#include <iprt/net.h>

#include <VBox/intnet.h>

#include <string>

#include "Defs.h"
#include "DhcpOptions.h"
#include "ClientId.h"


class Config
{
    std::string m_strHome;

    std::string m_strNetwork;
    std::string m_strBaseName; /* m_strNetwork sanitized to be usable in a path component */

    std::string m_strTrunk;
    INTNETTRUNKTYPE m_enmTrunkType;

    RTMAC m_MacAddress;

    RTNETADDRIPV4 m_IPv4Address;
    RTNETADDRIPV4 m_IPv4Netmask;

    RTNETADDRIPV4 m_IPv4PoolFirst;
    RTNETADDRIPV4 m_IPv4PoolLast;

private:
    bool isSane();

public: /* factory methods */
    static Config *hardcoded();                   /* for testing */
    static Config *create(int argc, char **argv); /* --config */
    static Config *compat(int argc, char **argv); /* old VBoxNetDHCP flags */

public: /* accessors */
    void setHome(const std::string &strHome) { m_strHome = strHome; }

    const std::string &getHome() const { return m_strHome; }
    const std::string &getNetwork() const { return m_strNetwork; }
    const std::string &getBaseName() const { return m_strBaseName; }
    const std::string &getTrunk() const { return m_strTrunk; }
    INTNETTRUNKTYPE getTrunkType() const { return m_enmTrunkType; }

    const RTMAC &getMacAddress() const { return m_MacAddress; }

    RTNETADDRIPV4 getIPv4Address() const { return m_IPv4Address; }
    RTNETADDRIPV4 getIPv4Netmask() const { return m_IPv4Netmask; }

    RTNETADDRIPV4 getIPv4PoolFirst() const { return m_IPv4PoolFirst; }
    RTNETADDRIPV4 getIPv4PoolLast() const { return m_IPv4PoolLast; }

public:
    optmap_t getOptions(const OptParameterRequest &reqOpts, const ClientId &id,
                        const OptVendorClassId &vendor = OptVendorClassId()) const;

private:
    static Config *read(const char *pszFileName);

    void sanitizeBaseName();

    void fillDefaultOptions(optmap_t &optmap,
                            const OptParameterRequest &reqOpts) const;
    void fillVendorOptions(optmap_t &optmap,
                           const OptParameterRequest &reqOpts,
                           const OptVendorClassId &vendor) const;
    void fillHostOptions(optmap_t &optmap,
                         const OptParameterRequest &reqOpts,
                         const ClientId &id) const;
};

#endif  /* _DHCPD_CONFIG_H_ */
