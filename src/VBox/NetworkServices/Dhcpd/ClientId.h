/* $Id$ */
/** @file
 * DHCP server - client identifier
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

#ifndef VBOX_INCLUDED_SRC_Dhcpd_ClientId_h
#define VBOX_INCLUDED_SRC_Dhcpd_ClientId_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "DhcpdInternal.h"
#include <iprt/net.h>
#include "DhcpOptions.h"

/**
 * A client is identified by either the Client ID option it sends or its chaddr,
 * i.e. MAC address.
 */
class ClientId
{
    /** The mac address of the client. */
    RTMAC       m_mac;
    /** The client ID. */
    OptClientId m_id;

public:
    ClientId()
        : m_mac(), m_id()
    {}
    ClientId(const RTMAC &a_mac, const OptClientId &a_id)
        : m_mac(a_mac), m_id(a_id)
    {}
    ClientId(const ClientId &a_rThat)
        : m_mac(a_rThat.m_mac), m_id(a_rThat.m_id)
    {}
    ClientId &operator=(const ClientId &a_rThat)
    {
        m_mac = a_rThat.m_mac;
        m_id  = a_rThat.m_id;
        return *this;
    }

    const RTMAC       &mac() const  { return m_mac; }
    const OptClientId &id() const   { return m_id; }

    /** @name String formatting of %R[id].
     * @{ */
    static void registerFormat();
private:
    static DECLCALLBACK(size_t) rtStrFormat(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput, const char *pszType,
                                            void const *pvValue, int cchWidth, int cchPrecision, unsigned fFlags, void *pvUser);
    static bool g_fFormatRegistered;
    /** @} */

    friend bool operator==(const ClientId &l, const ClientId &r);
    friend bool operator<(const ClientId &l, const ClientId &r);
};

bool operator==(const ClientId &l, const ClientId &r);
bool operator<(const ClientId &l, const ClientId &r);

inline bool operator!=(const ClientId &l, const ClientId &r)
{
    return !(l == r);
}

#endif /* !VBOX_INCLUDED_SRC_Dhcpd_ClientId_h */
