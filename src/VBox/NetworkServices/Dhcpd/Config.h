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

#ifndef VBOX_INCLUDED_SRC_Dhcpd_Config_h
#define VBOX_INCLUDED_SRC_Dhcpd_Config_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "DhcpdInternal.h"
#include <iprt/types.h>
#include <iprt/net.h>
#include <iprt/cpp/xml.h>
#include <iprt/cpp/ministring.h>

#include <VBox/intnet.h>

#include "DhcpOptions.h"
#include "ClientId.h"


/**
 * DHCP server configuration.
 */
class Config
{
    /** @todo XXX: also store fixed address assignments, etc? */
    typedef std::map<RTMAC, optmap_t> vmmap_t;

    RTCString       m_strHome;          /**< path of ~/.VirtualBox or equivalent, */

    RTCString       m_strNetwork;       /**< The name of the internal network the DHCP server is connected to. */
    RTCString       m_strBaseName;      /**< m_strNetwork sanitized to be usable in a path component. */
    RTCString       m_strLeaseFilename; /**< The lease filename if one given.  Dhcpd will pick a default if empty. */

    RTCString       m_strTrunk;         /**< The trunk name of the internal network. */
    INTNETTRUNKTYPE m_enmTrunkType;     /**< The trunk type of the internal network. */

    RTMAC           m_MacAddress;       /**< The MAC address for the DHCP server. */

    RTNETADDRIPV4   m_IPv4Address;      /**< The IPv4 address of the DHCP server. */
    RTNETADDRIPV4   m_IPv4Netmask;      /**< The IPv4 netmask for the DHCP server. */

    RTNETADDRIPV4   m_IPv4PoolFirst;    /**< The first IPv4 address in the pool. */
    RTNETADDRIPV4   m_IPv4PoolLast;     /**< The last IPV4 address in the pool (inclusive like all other 'last' variables). */


    optmap_t        m_GlobalOptions;    /**< Global DHCP option. */
    vmmap_t         m_VMMap;            /**< Per MAC address (VM) DHCP options. */
    /** @todo r=bird: optmap_t is too narrow for adding configuration options such
     *        as max-lease-time, min-lease-time, default-lease-time and such like
     *        that does not translate directly to any specific DHCP option. */
    /** @todo r=bird: Additionally, I'd like to have a more generic option groups
     *        that fits inbetween m_VMMap (mac based) and m_GlobalOptions.
     *        Pattern/wildcard matching on MAC address, possibly also client ID,
     *        vendor class and user class, including simple lists of these. */

private:
    Config();

    int                 i_init() RT_NOEXCEPT;
    int                 i_homeInit() RT_NOEXCEPT;
    static Config      *i_createInstanceAndCallInit() RT_NOEXCEPT;
    int                 i_logInit() RT_NOEXCEPT;
    int                 i_complete() RT_NOEXCEPT;

public:
    /** @name Factory methods
     * @{ */
    static Config      *hardcoded() RT_NOEXCEPT;                    /**< For testing. */
    static Config      *create(int argc, char **argv) RT_NOEXCEPT;  /**< --config */
    static Config      *compat(int argc, char **argv);
    /** @} */

    /** @name Accessors
     * @{ */
    const RTCString    &getHome() const RT_NOEXCEPT             { return m_strHome; }

    const RTCString    &getNetwork() const RT_NOEXCEPT          { return m_strNetwork; }
    const RTCString    &getBaseName() const RT_NOEXCEPT         { return m_strBaseName; }
    const RTCString    &getLeaseFilename() const RT_NOEXCEPT    { return m_strLeaseFilename; }

    const RTCString    &getTrunk() const RT_NOEXCEPT            { return m_strTrunk; }
    INTNETTRUNKTYPE     getTrunkType() const RT_NOEXCEPT        { return m_enmTrunkType; }

    const RTMAC        &getMacAddress() const RT_NOEXCEPT       { return m_MacAddress; }

    RTNETADDRIPV4       getIPv4Address() const RT_NOEXCEPT      { return m_IPv4Address; }
    RTNETADDRIPV4       getIPv4Netmask() const RT_NOEXCEPT      { return m_IPv4Netmask; }

    RTNETADDRIPV4       getIPv4PoolFirst() const RT_NOEXCEPT    { return m_IPv4PoolFirst; }
    RTNETADDRIPV4       getIPv4PoolLast() const RT_NOEXCEPT     { return m_IPv4PoolLast; }
    /** @} */

    optmap_t           &getOptions(optmap_t &a_rRetOpts, const OptParameterRequest &reqOpts, const ClientId &id,
                                   const OptVendorClassId &idVendorClass = OptVendorClassId(),
                                   const OptUserClassId &idUserClass = OptUserClassId()) const;

private:
    /** @name Configuration file reading and parsing
     * @{ */
    static Config      *i_read(const char *pszFileName) RT_NOEXCEPT;
    void                i_parseConfig(const xml::ElementNode *root);
    void                i_parseServer(const xml::ElementNode *server);
    void                i_parseGlobalOptions(const xml::ElementNode *options);
    void                i_parseVMConfig(const xml::ElementNode *config);
    void                i_parseOption(const xml::ElementNode *option, optmap_t &optmap);

    static void         i_getIPv4AddrAttribute(const xml::ElementNode *pElm, const char *pcszAttrName, PRTNETADDRIPV4 pAddr);
    static void         i_getMacAddressAttribute(const xml::ElementNode *pElm, const char *pszAttrName, PRTMAC pMacAddr);
    /** @} */

    void                i_setNetwork(const RTCString &aStrNetwork);
    void                i_sanitizeBaseName();
};

#endif /* !VBOX_INCLUDED_SRC_Dhcpd_Config_h */
