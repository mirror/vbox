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

#include <iprt/err.h>
#include <iprt/stream.h>

#include "Db.h"


Db::~Db()
{
    /** @todo free bindings */
}


int Db::init(const Config *pConfig)
{
    Binding::registerFormat();

    m_pConfig = pConfig;

    m_pool.init(pConfig->getIPv4PoolFirst(),
                pConfig->getIPv4PoolLast());

    return VINF_SUCCESS;
}


bool Binding::g_fFormatRegistered = false;


void Binding::registerFormat()
{
    if (g_fFormatRegistered)
        return;

    int rc = RTStrFormatTypeRegister("binding", rtStrFormat, NULL);
    AssertRC(rc);

    g_fFormatRegistered = true;
}


DECLCALLBACK(size_t)
Binding::rtStrFormat(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                     const char *pszType, void const *pvValue,
                     int cchWidth, int cchPrecision, unsigned fFlags,
                     void *pvUser)
{
    const Binding *b = static_cast<const Binding *>(pvValue);
    size_t cb = 0;

    AssertReturn(strcmp(pszType, "binding") == 0, 0);
    RT_NOREF(pszType);

    RT_NOREF(cchWidth, cchPrecision, fFlags);
    RT_NOREF(pvUser);

    if (b == NULL)
    {
        return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                           "<NULL>");
    }

    cb += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                      "%RTnaipv4", b->m_addr.u);

    if (b->m_state == Binding::FREE)
    {
        cb += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                          " free");
    }
    else
    {
        cb += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                          " to %R[id], %s, valid from ",
                          &b->m_id, b->stateName());

        TimeStamp tsIssued = b->issued();
        cb += tsIssued.absStrFormat(pfnOutput, pvArgOutput);

        cb += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                          " for %ds until ",
                          b->leaseTime());

        TimeStamp tsValid = b->issued();
        tsValid.addSeconds(b->leaseTime());
        cb += tsValid.absStrFormat(pfnOutput, pvArgOutput);
    }

    return cb;
}

const char *Binding::stateName() const
{
    switch (m_state) {
    case FREE:
        return "free";
    case RELEASED:
        return "released";
    case EXPIRED:
        return "expired";
    case OFFERED:
        return "offered";
    case ACKED:
        return "acked";
    default:
        return "released";
    }
}


Binding &Binding::setState(const char *pszStateName)
{
    if (strcmp(pszStateName, "free") == 0)
        m_state = Binding::FREE;
    else if (strcmp(pszStateName, "released") == 0)
        m_state = Binding::RELEASED;
    else if (strcmp(pszStateName, "expired") == 0)
        m_state = Binding::EXPIRED;
    else if (strcmp(pszStateName, "offered") == 0)
        m_state = Binding::OFFERED;
    else if (strcmp(pszStateName, "acked") == 0)
        m_state = Binding::ACKED;
    else
        m_state = Binding::RELEASED;

    return *this;
}


bool Binding::expire(TimeStamp deadline)
{
    if (m_state <= Binding::EXPIRED)
        return false;

    TimeStamp t = m_issued;
    t.addSeconds(m_secLease);

    if (t < deadline)
    {
        if (m_state == Binding::OFFERED)
            setState(Binding::FREE);
        else
            setState(Binding::EXPIRED);
    }
    return true;
}


int Binding::toXML(xml::ElementNode *ndParent) const
{
    int rc;

    /*
     * Lease
     */
    xml::ElementNode *ndLease = ndParent->createChild("Lease");
    if (ndLease == NULL)
        return VERR_GENERAL_FAILURE;

    /* XXX: arrange for lease to get deleted if anything below fails */


    ndLease->setAttribute("mac", RTCStringFmt("%RTmac", &m_id.mac()));
    if (m_id.id().present())
    {
        /* I'd prefer RTSTRPRINTHEXBYTES_F_SEP_COLON but there's no decoder */
        size_t cbStrId = m_id.id().value().size() * 2 + 1;
        char *pszId = new char[cbStrId];
        rc = RTStrPrintHexBytes(pszId, cbStrId,
                                &m_id.id().value().front(), m_id.id().value().size(),
                                0);
        ndLease->setAttribute("id", pszId);
        delete[] pszId;
    }

    /* unused but we need it to keep the old code happy */
    ndLease->setAttribute("network", "0.0.0.0");

    ndLease->setAttribute("state", stateName());


    /*
     * Lease/Address
     */
    xml::ElementNode *ndAddr = ndLease->createChild("Address");
    ndAddr->setAttribute("value", RTCStringFmt("%RTnaipv4", m_addr.u));


    /*
     * Lease/Time
     */
    xml::ElementNode *ndTime = ndLease->createChild("Time");
    ndTime->setAttribute("issued", m_issued.getAbsSeconds());
    ndTime->setAttribute("expiration", m_secLease);

    return VINF_SUCCESS;
}


Binding *Binding::fromXML(const xml::ElementNode *ndLease)
{
    int rc;

    /* Lease/@network seems to always have bogus value, ignore it. */

    /*
     * Lease/@mac
     */
    RTCString strMac;
    bool fHasMac = ndLease->getAttributeValue("mac", &strMac);
    if (!fHasMac)
        return NULL;

    RTMAC mac;
    rc = RTNetStrToMacAddr(strMac.c_str(), &mac);
    if (RT_FAILURE(rc))
        return NULL;

    OptClientId id;
    RTCString strId;
    bool fHasId = ndLease->getAttributeValue("id", &strId);
    if (fHasId)
    {
        /*
         * Decode from "de:ad:be:ef".
         * XXX: RTStrConvertHexBytes() doesn't grok colons
         */
        size_t cbBytes = strId.length() / 2;
        uint8_t *pBytes = new uint8_t[cbBytes];
        rc = RTStrConvertHexBytes(strId.c_str(), pBytes, cbBytes, 0);
        if (RT_SUCCESS(rc))
        {
            std::vector<uint8_t> rawopt(pBytes, pBytes + cbBytes);
            id = OptClientId(rawopt);
        }
        delete[] pBytes;
    }

    /*
     * Lease/@state - not present in old leases file.  We will try to
     * infer from lease time below.
     */
    RTCString strState;
    bool fHasState = ndLease->getAttributeValue("state", &strState);

    /*
     * Lease/Address
     */
    const xml::ElementNode *ndAddress = ndLease->findChildElement("Address");
    if (ndAddress == NULL)
        return NULL;

    /*
     * Lease/Address/@value
     */
    RTCString strAddress;
    bool fHasValue = ndAddress->getAttributeValue("value", &strAddress);
    if (!fHasValue)
        return NULL;

    RTNETADDRIPV4 addr;
    rc = RTNetStrToIPv4Addr(strAddress.c_str(), &addr);
    if (RT_FAILURE(rc))
        return NULL;

    /*
     * Lease/Time
     */
    const xml::ElementNode *ndTime = ndLease->findChildElement("Time");
    if (time == NULL)
        return NULL;

    /*
     * Lease/Time/@issued
     */
    int64_t issued;
    bool fHasIssued = ndTime->getAttributeValue("issued", &issued);
    if (!fHasIssued)
        return NULL;

    /*
     * Lease/Time/@expiration
     */
    uint32_t duration;
    bool fHasExpiration = ndTime->getAttributeValue("expiration", &duration);
    if (!fHasExpiration)
        return NULL;

    std::unique_ptr<Binding> b(new Binding(addr));
    b->m_id = ClientId(mac, id);

    if (fHasState)
    {
        b->m_issued = TimeStamp::absSeconds(issued);
        b->m_secLease = duration;
        b->setState(strState.c_str());
    }
    else
    {   /* XXX: old code wrote timestamps instead of absolute time. */
        /* pretend that lease has just ended */
        TimeStamp fakeIssued = TimeStamp::now();
        fakeIssued.subSeconds(duration);
        b->m_issued = fakeIssued;
        b->m_secLease = duration;
        b->m_state = Binding::EXPIRED;
    }

    return b.release();
}


void Db::expire()
{
    const TimeStamp now = TimeStamp::now();

    for (bindings_t::iterator it = m_bindings.begin();
         it != m_bindings.end(); ++it)
    {
        Binding *b = *it;
        b->expire(now);
    }
}


Binding *Db::createBinding(const ClientId &id)
{
    RTNETADDRIPV4 addr = m_pool.allocate();
    if (addr.u == 0)
        return NULL;

    Binding *b = new Binding(addr, id);
    m_bindings.push_front(b);
    return b;
}


Binding *Db::createBinding(RTNETADDRIPV4 addr, const ClientId &id)
{
    bool fAvailable = m_pool.allocate(addr);
    if (!fAvailable)
    {
        /*
         * XXX: this should not happen.  If the address is from the
         * pool, which we have verified before, then either it's in
         * the free pool or there's an binding (possibly free) for it.
         */
        return NULL;
    }

    Binding *b = new Binding(addr, id);
    m_bindings.push_front(b);
    return b;
}


Binding *Db::allocateAddress(const ClientId &id, RTNETADDRIPV4 addr)
{
    Assert(addr.u == 0 || addressBelongs(addr));

    Binding *addrBinding = NULL;
    Binding *freeBinding = NULL;
    Binding *reuseBinding = NULL;

    if (addr.u != 0)
        LogDHCP(("> allocateAddress %RTnaipv4 to client %R[id]\n", addr.u, &id));
    else
        LogDHCP(("> allocateAddress to client %R[id]\n", &id));

    /*
     * Allocate existing address if client has one.  Ignore requested
     * address in that case.  While here, look for free addresses and
     * addresses that can be reused.
     */
    const TimeStamp now = TimeStamp::now();
    for (bindings_t::iterator it = m_bindings.begin();
         it != m_bindings.end(); ++it)
    {
        Binding *b = *it;
        b->expire(now);

        /*
         * We've already seen this client, give it its old binding.
         */
        if (b->m_id == id)
        {
            LogDHCP(("> ... found existing binding %R[binding]\n", b));
            return b;
        }

        if (addr.u != 0 && b->m_addr.u == addr.u)
        {
            Assert(addrBinding == NULL);
            addrBinding = b;
            LogDHCP(("> .... noted existing binding %R[binding]\n", addrBinding));
        }

        /* if we haven't found a free binding yet, keep looking */
        if (freeBinding == NULL)
        {
            if (b->m_state == Binding::FREE)
            {
                freeBinding = b;
                LogDHCP(("> .... noted free binding %R[binding]\n", freeBinding));
                continue;
            }

            /* still no free binding, can this one be reused? */
            if (b->m_state == Binding::RELEASED)
            {
                if (   reuseBinding == NULL
                    /* released binding is better than an expired one */
                    || reuseBinding->m_state == Binding::EXPIRED)
                {
                    reuseBinding = b;
                    LogDHCP(("> .... noted released binding %R[binding]\n", reuseBinding));
                }
            }
            else if (b->m_state == Binding::EXPIRED)
            {
                if (   reuseBinding == NULL
                    /* long expired binding is bettern than a recent one */
                    /* || (reuseBinding->m_state == Binding::EXPIRED && b->olderThan(reuseBinding)) */)
                {
                    reuseBinding = b;
                    LogDHCP(("> .... noted expired binding %R[binding]\n", reuseBinding));
                }
            }
        }
    }

    /*
     * Allocate requested address if we can.
     */
    if (addr.u != 0)
    {
        if (addrBinding == NULL)
        {
            addrBinding = createBinding(addr, id);
            Assert(addrBinding != NULL);
            LogDHCP(("> .... creating new binding for this address %R[binding]\n",
                     addrBinding));
            return addrBinding;
        }

        if (addrBinding->m_state <= Binding::EXPIRED) /* not in use */
        {
            LogDHCP(("> .... reusing %s binding for this address\n",
                     addrBinding->stateName()));
            addrBinding->giveTo(id);
            return addrBinding;
        }
        else
        {
            LogDHCP(("> .... cannot reuse %s binding for this address\n",
                     addrBinding->stateName()));
        }
    }

    /*
     * Allocate new (or reuse).
     */
    Binding *idBinding = NULL;
    if (freeBinding != NULL)
    {
        idBinding = freeBinding;
        LogDHCP(("> .... reusing free binding\n"));
    }
    else
    {
        idBinding = createBinding();
        if (idBinding != NULL)
        {
            LogDHCP(("> .... creating new binding\n"));
        }
        else
        {
            idBinding = reuseBinding;
            LogDHCP(("> .... reusing %s binding %R[binding]\n",
                     reuseBinding->stateName()));
        }
    }

    if (idBinding == NULL)
    {
        LogDHCP(("> .... failed to allocate binding\n"));
        return NULL;
    }

    idBinding->giveTo(id);
    LogDHCP(("> .... allocated %R[binding]\n", idBinding));

    return idBinding;
}



Binding *Db::allocateBinding(const DhcpClientMessage &req)
{
    /** @todo XXX: handle fixed address assignments */
    OptRequestedAddress reqAddr(req);
    if (reqAddr.present() && !addressBelongs(reqAddr.value()))
    {
        if (req.messageType() == RTNET_DHCP_MT_DISCOVER)
        {
            LogDHCP(("DISCOVER: ignoring invalid requested address\n"));
            reqAddr = OptRequestedAddress();
        }
        else
        {
            LogDHCP(("rejecting invalid requested address\n"));
            return NULL;
        }
    }

    const ClientId &id(req.clientId());

    Binding *b = allocateAddress(id, reqAddr.value());
    if (b == NULL)
        return NULL;

    Assert(b->id() == id);

    /*
     * XXX: handle requests for specific lease time!
     * XXX: old lease might not have expired yet?
     */
    // OptLeaseTime reqLeaseTime(req);
    b->setLeaseTime(1200);
    return b;
}


int Db::addBinding(Binding *newb)
{
    if (!addressBelongs(newb->m_addr))
    {
        LogDHCP(("Binding for out of range address %RTnaipv4 ignored\n",
                 newb->m_addr.u));
        return VERR_INVALID_PARAMETER;
    }

    for (bindings_t::iterator it = m_bindings.begin();
         it != m_bindings.end(); ++it)
    {
        Binding *b = *it;

        if (newb->m_addr.u == b->m_addr.u)
        {
            LogDHCP(("> ADD: %R[binding]\n", newb));
            LogDHCP(("> .... duplicate ip: %R[binding]\n", b));
            return VERR_INVALID_PARAMETER;
        }

        if (newb->m_id == b->m_id)
        {
            LogDHCP(("> ADD: %R[binding]\n", newb));
            LogDHCP(("> .... duplicate id: %R[binding]\n", b));
            return VERR_INVALID_PARAMETER;
        }
    }

    bool ok = m_pool.allocate(newb->m_addr);
    if (!ok)
    {
        LogDHCP(("> ADD: failed to claim IP %R[binding]\n", newb));
        return VERR_INVALID_PARAMETER;
    }

    m_bindings.push_back(newb);
    return VINF_SUCCESS;
}


void Db::cancelOffer(const DhcpClientMessage &req)
{
    const OptRequestedAddress reqAddr(req);
    if (!reqAddr.present())
        return;

    const RTNETADDRIPV4 addr = reqAddr.value();
    const ClientId &id(req.clientId());

    for (bindings_t::iterator it = m_bindings.begin();
         it != m_bindings.end(); ++it)
    {
        Binding *b = *it;

        if (b->addr().u == addr.u && b->id() == id)
        {
            if (b->state() == Binding::OFFERED)
            {
                b->setLeaseTime(0);
                b->setState(Binding::RELEASED);
            }
            return;
        }
    }
}


bool Db::releaseBinding(const DhcpClientMessage &req)
{
    const RTNETADDRIPV4 addr = req.ciaddr();
    const ClientId &id(req.clientId());

    for (bindings_t::iterator it = m_bindings.begin();
         it != m_bindings.end(); ++it)
    {
        Binding *b = *it;

        if (b->addr().u == addr.u && b->id() == id)
        {
            b->setState(Binding::RELEASED);
            return true;
        }
    }

    return false;
}


int Db::writeLeases(const std::string &strFileName) const
{
    LogDHCP(("writing leases to %s\n", strFileName.c_str()));

    xml::Document doc;

    xml::ElementNode *root = doc.createRootElement("Leases");
    if (root == NULL)
        return VERR_INTERNAL_ERROR;

    root->setAttribute("version", "1.0");

    for (bindings_t::const_iterator it = m_bindings.begin();
         it != m_bindings.end(); ++it)
    {
        const Binding *b = *it;
        b->toXML(root);
    }

    try {
        xml::XmlFileWriter writer(doc);
        writer.write(strFileName.c_str(), true);
    }
    catch (const xml::EIPRTFailure &e)
    {
        LogDHCP(("%s\n", e.what()));
        return e.rc();
    }
    catch (const RTCError &e)
    {
        LogDHCP(("%s\n", e.what()));
        return VERR_GENERAL_FAILURE;
    }
    catch (...)
    {
        LogDHCP(("Unknown exception while writing '%s'\n",
                 strFileName.c_str()));
        return VERR_GENERAL_FAILURE;
    }

    return VINF_SUCCESS;
}


int Db::loadLeases(const std::string &strFileName)
{
    LogDHCP(("loading leases from %s\n", strFileName.c_str()));

    xml::Document doc;
    try
    {
        xml::XmlFileParser parser;
        parser.read(strFileName.c_str(), doc);
    }
    catch (const xml::EIPRTFailure &e)
    {
        LogDHCP(("%s\n", e.what()));
        return e.rc();
    }
    catch (const RTCError &e)
    {
        LogDHCP(("%s\n", e.what()));
        return VERR_GENERAL_FAILURE;
    }
    catch (...)
    {
        LogDHCP(("Unknown exception while reading and parsing '%s'\n",
                 strFileName.c_str()));
        return VERR_GENERAL_FAILURE;
    }

    xml::ElementNode *ndRoot = doc.getRootElement();
    if (ndRoot == NULL || !ndRoot->nameEquals("Leases"))
    {
        return VERR_NOT_FOUND;
    }

    xml::NodesLoop it(*ndRoot);
    const xml::ElementNode *node;
    while ((node = it.forAllNodes()) != NULL)
    {
        if (!node->nameEquals("Lease"))
            continue;

        loadLease(node);
    }

    return VINF_SUCCESS;
}


void Db::loadLease(const xml::ElementNode *ndLease)
{
    Binding *b = Binding::fromXML(ndLease);
    bool expired = b->expire();

    if (!expired)
        LogDHCP(("> LOAD: lease %R[binding]\n", b));
    else
        LogDHCP(("> LOAD: EXPIRED lease %R[binding]\n", b));

    addBinding(b);
}
