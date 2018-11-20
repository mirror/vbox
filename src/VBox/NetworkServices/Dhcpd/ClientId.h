/* $Id$ */
/** @file
 * DHCP server - client identifier
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

#ifndef _DHCPD_CLIENT_ID_H_
#define _DHCPD_CLIENT_ID_H_

#include "Defs.h"
#include <iprt/net.h>
#include "DhcpOptions.h"

/*
 * Client is identified by either the Client ID option it sends or its
 * chaddr, i.e. MAC address.
 */
class ClientId
{
    RTMAC m_mac;
    OptClientId m_id;

public:
    ClientId()
      : m_mac(), m_id() {}
    ClientId(const RTMAC &mac, const OptClientId &id)
      : m_mac(mac), m_id(id) {}

    const RTMAC &mac() const { return m_mac; }
    const OptClientId &id() const { return m_id; }

public:
    static void registerFormat(); /* %R[id] */

private:
    static bool g_fFormatRegistered;
    static DECLCALLBACK(size_t) rtStrFormat(
        PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
        const char *pszType, void const *pvValue,
        int cchWidth, int cchPrecision, unsigned fFlags,
        void *pvUser);

private:
    friend bool operator==(const ClientId &l, const ClientId &r);
    friend bool operator<(const ClientId &l, const ClientId &r);
};

bool operator==(const ClientId &l, const ClientId &r);
bool operator<(const ClientId &l, const ClientId &r);

inline bool operator!=(const ClientId &l, const ClientId &r)
{
    return !(l == r);
}

#endif /* _DHCPD_CLIENT_ID_H_ */
