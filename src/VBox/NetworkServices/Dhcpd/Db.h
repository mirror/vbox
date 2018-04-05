/* $Id$ */
/** @file
 * DHCP server - address database
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

#ifndef _DHCPD_DB_H_
#define _DHCPD_DB_H_

#include <iprt/net.h>

#include <iprt/cpp/xml.h>

#include <list>

#include "Defs.h"
#include "TimeStamp.h"
#include "ClientId.h"
#include "IPv4Pool.h"
#include "Config.h"
#include "DhcpMessage.h"


class Binding
{
    friend class Db;

public:
    enum State { FREE, RELEASED, EXPIRED, OFFERED, ACKED };

private:
    const RTNETADDRIPV4 m_addr;
    State m_state;
    ClientId m_id;
    TimeStamp m_issued;
    uint32_t m_secLease;

public:
    Binding();
    Binding(const Binding &);

    explicit Binding(RTNETADDRIPV4 addr)
      : m_addr(addr), m_state(FREE),
        m_issued(), m_secLease() {}

    Binding(RTNETADDRIPV4 addr, const ClientId &id)
      : m_addr(addr), m_state(FREE), m_id(id),
        m_issued(), m_secLease() {}


    RTNETADDRIPV4 addr() const { return m_addr; }

    State state() const { return m_state; }
    const char *stateName() const;

    const ClientId &id() const { return m_id; }

    uint32_t leaseTime() const { return m_secLease; }
    TimeStamp issued() const { return m_issued; }

    Binding &setState(State state)
    {
        m_state = state;
        return *this;
    }

    Binding &setState(const char *pszStateName);

    Binding &setLeaseTime(uint32_t secLease)
    {
        m_issued = TimeStamp::now();
        m_secLease = secLease;
        return *this;
    }

    Binding &giveTo(const ClientId &id)
    {
        m_id = id;
        m_state = FREE;
        return *this;
    }

    void free()
    {
        m_id = ClientId();
        m_state = FREE;
    }

    bool expire(TimeStamp deadline);
    bool expire() { return expire(TimeStamp::now()); }

    static Binding *fromXML(const xml::ElementNode *ndLease);
    int toXML(xml::ElementNode *ndParent) const;

public:
    static void registerFormat(); /* %R[binding] */

private:
    static bool g_fFormatRegistered;
    static DECLCALLBACK(size_t) rtStrFormat(
        PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
        const char *pszType, void const *pvValue,
        int cchWidth, int cchPrecision, unsigned fFlags,
        void *pvUser);
};


class Db
{
private:
    typedef std::list<Binding *> bindings_t;

    const Config *m_pConfig;
    bindings_t m_bindings;
    IPv4Pool m_pool;

public:
    Db() {}
    ~Db();

    int init(const Config *pConfig);

    bool addressBelongs(RTNETADDRIPV4 addr) const { return m_pool.contains(addr); }

    Binding *allocateBinding(const DhcpClientMessage &req);
    bool releaseBinding(const DhcpClientMessage &req);

    void cancelOffer(const DhcpClientMessage &req);

    void expire();

public:
    int loadLeases(const std::string &strFileName);
    void loadLease(const xml::ElementNode *ndLease);

    int writeLeases(const std::string &strFileName) const;

private:
    Binding *createBinding(const ClientId &id = ClientId());
    Binding *createBinding(RTNETADDRIPV4 addr, const ClientId &id = ClientId());

    Binding *allocateAddress(const ClientId &id, RTNETADDRIPV4 addr);

    /* add binding e.g. from the leases file */
    int addBinding(Binding *b);
};

#endif  /* _DHCPD_DB_H_ */
