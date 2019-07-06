/* $Id$ */
/** @file
 * DHCP server - address database
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
#include <iprt/errcore.h>

#include "Db.h"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Indicates whether has been called successfully yet. */
bool Binding::g_fFormatRegistered = false;


/**
 * Registers the ClientId format type callback ("%R[binding]").
 */
void Binding::registerFormat() RT_NOEXCEPT
{
    if (!g_fFormatRegistered)
    {
        int rc = RTStrFormatTypeRegister("binding", rtStrFormat, NULL);
        AssertRC(rc);
        g_fFormatRegistered = true;
    }
}


/**
 * @callback_method_impl{FNRTSTRFORMATTYPE, Formats ClientId via "%R[binding]".}
 */
DECLCALLBACK(size_t)
Binding::rtStrFormat(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                     const char *pszType, void const *pvValue,
                     int cchWidth, int cchPrecision, unsigned fFlags,
                     void *pvUser)
{

    AssertReturn(strcmp(pszType, "binding") == 0, 0);
    RT_NOREF(pszType);

    RT_NOREF(cchWidth, cchPrecision, fFlags);
    RT_NOREF(pvUser);

    const Binding *b = static_cast<const Binding *>(pvValue);
    if (b == NULL)
        return pfnOutput(pvArgOutput, RT_STR_TUPLE("<NULL>"));

    size_t cb = RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%RTnaipv4", b->m_addr.u);
    if (b->m_state == Binding::FREE)
        cb += pfnOutput(pvArgOutput, RT_STR_TUPLE(" free"));
    else
    {
        cb += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, " to %R[id], %s, valid from ", &b->m_id, b->stateName());

        Timestamp tsIssued = b->issued();
        cb += tsIssued.strFormatHelper(pfnOutput, pvArgOutput);

        cb += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, " for %ds until ", b->leaseTime());

        Timestamp tsValid = b->issued();
        tsValid.addSeconds(b->leaseTime());
        cb += tsValid.strFormatHelper(pfnOutput, pvArgOutput);
    }

    return cb;
}

const char *Binding::stateName() const RT_NOEXCEPT
{
    switch (m_state)
    {
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
            AssertMsgFailed(("%d\n", m_state));
            return "released";
    }
}


Binding &Binding::setState(const char *pszStateName) RT_NOEXCEPT
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
    {
        AssertMsgFailed(("%d\n", m_state));
        m_state = Binding::RELEASED;
    }

    return *this;
}


/**
 * Expires the binding if it's past the specified deadline.
 *
 * @returns False if already expired, released or freed, otherwise true (i.e.
 *          does not indicate whether action was taken or not).
 * @param   tsDeadline          The expiry deadline to use.
 */
bool Binding::expire(Timestamp tsDeadline) RT_NOEXCEPT
{
    if (m_state <= Binding::EXPIRED)
        return false;

    Timestamp tsExpire = m_issued;
    tsExpire.addSeconds(m_secLease);

    if (tsExpire < tsDeadline)
    {
        if (m_state == Binding::OFFERED)
            setState(Binding::FREE);
        else
            setState(Binding::EXPIRED);
    }
    return true;
}


/**
 * Serializes the binding to XML for the lease database.
 *
 * @throw  std::bad_alloc
 */
void Binding::toXML(xml::ElementNode *pElmParent) const
{
    /*
     * Lease
     */
    xml::ElementNode *pElmLease = pElmParent->createChild("Lease");

    pElmLease->setAttribute("mac", RTCStringFmt("%RTmac", &m_id.mac()));
    if (m_id.id().present())
    {
        /* I'd prefer RTSTRPRINTHEXBYTES_F_SEP_COLON but there's no decoder */
        size_t cbStrId = m_id.id().value().size() * 2 + 1;
        char *pszId = new char[cbStrId];
        int rc = RTStrPrintHexBytes(pszId, cbStrId,
                                    &m_id.id().value().front(), m_id.id().value().size(),
                                    0);
        AssertRC(rc);
        pElmLease->setAttribute("id", pszId);
        delete[] pszId;
    }

    /* unused but we need it to keep the old code happy */
    pElmLease->setAttribute("network", "0.0.0.0");
    pElmLease->setAttribute("state", stateName());

    /*
     * Lease/Address
     */
    xml::ElementNode *pElmAddr = pElmLease->createChild("Address");
    pElmAddr->setAttribute("value", RTCStringFmt("%RTnaipv4", m_addr.u));

    /*
     * Lease/Time
     */
    xml::ElementNode *pElmTime = pElmLease->createChild("Time");
    pElmTime->setAttribute("issued", m_issued.getAbsSeconds());
    pElmTime->setAttribute("expiration", m_secLease);
}


/**
 * Deserializes the binding from the XML lease database.
 *
 * @param   pElmLease   The "Lease" element.
 * @return  Pointer to the resulting binding, NULL on failure.
 * @throw   std::bad_alloc
 */
Binding *Binding::fromXML(const xml::ElementNode *pElmLease)
{
    /* Lease/@network seems to always have bogus value, ignore it. */

    /*
     * Lease/@mac
     */
    RTCString strMac;
    bool fHasMac = pElmLease->getAttributeValue("mac", &strMac);
    if (!fHasMac)
        return NULL;

    RTMAC mac;
    int rc = RTNetStrToMacAddr(strMac.c_str(), &mac);
    if (RT_FAILURE(rc))
        return NULL;

    OptClientId id;
    RTCString strId;
    bool fHasId = pElmLease->getAttributeValue("id", &strId);
    if (fHasId)
    {
        /*
         * Decode from "de:ad:be:ef".
         */
        /** @todo RTStrConvertHexBytes() doesn't grok colons */
        size_t   cbBytes = strId.length() / 2;
        uint8_t *pbBytes = new uint8_t[cbBytes];
        rc = RTStrConvertHexBytes(strId.c_str(), pbBytes, cbBytes, 0);
        if (RT_SUCCESS(rc))
        {
            try
            {
                std::vector<uint8_t> rawopt(pbBytes, pbBytes + cbBytes);
                id = OptClientId(rawopt);
            }
            catch (std::bad_alloc &)
            {
                delete[] pbBytes;
                throw;
            }
        }
        delete[] pbBytes;
    }

    /*
     * Lease/@state - not present in old leases file.  We will try to
     * infer from lease time below.
     */
    RTCString strState;
    bool fHasState = pElmLease->getAttributeValue("state", &strState);

    /*
     * Lease/Address
     */
    const xml::ElementNode *ndAddress = pElmLease->findChildElement("Address");
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
    const xml::ElementNode *ndTime = pElmLease->findChildElement("Time");
    if (ndTime == NULL)
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
        b->m_issued = Timestamp::absSeconds(issued);
        b->m_secLease = duration;
        b->setState(strState.c_str());
    }
    else
    {   /** @todo XXX: old code wrote timestamps instead of absolute time. */
        /* pretend that lease has just ended */
        Timestamp fakeIssued = Timestamp::now();
        fakeIssued.subSeconds(duration);
        b->m_issued = fakeIssued;
        b->m_secLease = duration;
        b->m_state = Binding::EXPIRED;
    }

    return b.release();
}



/*********************************************************************************************************************************
*   Class Db Implementation                                                                                                      *
*********************************************************************************************************************************/

Db::Db()
    : m_pConfig(NULL)
{
}


Db::~Db()
{
    /** @todo free bindings */
}


int Db::init(const Config *pConfig)
{
    Binding::registerFormat();

    m_pConfig = pConfig;

    return m_pool.init(pConfig->getIPv4PoolFirst(), pConfig->getIPv4PoolLast());
}


/**
 * Expire old binding (leases).
 */
void Db::expire() RT_NOEXCEPT
{
    const Timestamp now = Timestamp::now();
    for (bindings_t::iterator it = m_bindings.begin(); it != m_bindings.end(); ++it)
    {
        Binding *b = *it;
        b->expire(now);
    }
}


/**
 * Internal worker that creates a binding for the given client, allocating new
 * IPv4 address for it.
 *
 * @returns Pointer to the binding.
 * @param   id          The client ID.
 */
Binding *Db::i_createBinding(const ClientId &id)
{
    Binding      *pBinding = NULL;
    RTNETADDRIPV4 addr = m_pool.allocate();
    if (addr.u != 0)
    {
        try
        {
            pBinding = new Binding(addr, id);
            m_bindings.push_front(pBinding);
        }
        catch (std::bad_alloc &)
        {
            if (pBinding)
                delete pBinding;
            /** @todo free address (no pool method for that)  */
        }
    }
    return pBinding;
}


/**
 * Internal worker that creates a binding to the specified IPv4 address for the
 * given client.
 *
 * @returns Pointer to the binding.
 *          NULL if the address is in use or we ran out of memory.
 * @param   addr        The IPv4 address.
 * @param   id          The client.
 */
Binding *Db::i_createBinding(RTNETADDRIPV4 addr, const ClientId &id)
{
    bool fAvailable = m_pool.allocate(addr);
    if (!fAvailable)
    {
        /** @todo
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


/**
 * Internal worker that allocates an IPv4 address for the given client, taking
 * the preferred address (@a addr) into account when possible and if non-zero.
 */
Binding *Db::i_allocateAddress(const ClientId &id, RTNETADDRIPV4 addr)
{
    Assert(addr.u == 0 || addressBelongs(addr));

    if (addr.u != 0)
        LogDHCP(("> allocateAddress %RTnaipv4 to client %R[id]\n", addr.u, &id));
    else
        LogDHCP(("> allocateAddress to client %R[id]\n", &id));

    /*
     * Allocate existing address if client has one.  Ignore requested
     * address in that case.  While here, look for free addresses and
     * addresses that can be reused.
     */
    Binding        *addrBinding  = NULL;
    Binding        *freeBinding  = NULL;
    Binding        *reuseBinding = NULL;
    const Timestamp now          = Timestamp::now();
    for (bindings_t::iterator it = m_bindings.begin(); it != m_bindings.end(); ++it)
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
            addrBinding = i_createBinding(addr, id);
            Assert(addrBinding != NULL);
            LogDHCP(("> .... creating new binding for this address %R[binding]\n", addrBinding));
            return addrBinding;
        }

        if (addrBinding->m_state <= Binding::EXPIRED) /* not in use */
        {
            LogDHCP(("> .... reusing %s binding for this address\n", addrBinding->stateName()));
            addrBinding->giveTo(id);
            return addrBinding;
        }
        LogDHCP(("> .... cannot reuse %s binding for this address\n", addrBinding->stateName()));
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
        idBinding = i_createBinding();
        if (idBinding != NULL)
            LogDHCP(("> .... creating new binding\n"));
        else
        {
            idBinding = reuseBinding;
            if (idBinding != NULL)
                LogDHCP(("> .... reusing %s binding %R[binding]\n", reuseBinding->stateName(), reuseBinding));
            else
            {
                LogDHCP(("> .... failed to allocate binding\n"));
                return NULL;
            }
        }
    }

    idBinding->giveTo(id);
    LogDHCP(("> .... allocated %R[binding]\n", idBinding));

    return idBinding;
}



/**
 * Called by DHCPD to allocate a binding for the specified request.
 *
 * @returns Pointer to the binding, NULL on failure.
 * @param   req                 The DHCP request being served.
 */
Binding *Db::allocateBinding(const DhcpClientMessage &req)
{
    /** @todo XXX: handle fixed address assignments */

    /*
     * Get and validate the requested address (if present).
     */
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

    /*
     * Allocate the address.
     */
    const ClientId &id(req.clientId());

    Binding *b = i_allocateAddress(id, reqAddr.value());
    if (b != NULL)
    {
        Assert(b->id() == id);

        /** @todo
         * XXX: handle requests for specific lease time!
         * XXX: old lease might not have expired yet?
         * Make lease time configurable.
         */
        // OptLeaseTime reqLeaseTime(req);
        b->setLeaseTime(1200);
    }
    return b;
}


/**
 * Internal worker used by loadLease().
 *
 * @returns IPRT status code.
 * @param   pNewBinding     The new binding to add.
 */
int Db::i_addBinding(Binding *pNewBinding) RT_NOEXCEPT
{
    /*
     * Validate the binding against the range and existing bindings.
     */
    if (!addressBelongs(pNewBinding->m_addr))
    {
        LogDHCP(("Binding for out of range address %RTnaipv4 ignored\n", pNewBinding->m_addr.u));
        return VERR_OUT_OF_RANGE;
    }

    for (bindings_t::iterator it = m_bindings.begin(); it != m_bindings.end(); ++it)
    {
        Binding *b = *it;

        if (pNewBinding->m_addr.u == b->m_addr.u)
        {
            LogDHCP(("> ADD: %R[binding]\n", pNewBinding));
            LogDHCP(("> .... duplicate ip: %R[binding]\n", b));
            return VERR_DUPLICATE;
        }

        if (pNewBinding->m_id == b->m_id)
        {
            LogDHCP(("> ADD: %R[binding]\n", pNewBinding));
            LogDHCP(("> .... duplicate id: %R[binding]\n", b));
            return VERR_DUPLICATE;
        }
    }

    /*
     * Allocate the address and add the binding to the list.
     */
    AssertLogRelMsgReturn(m_pool.allocate(pNewBinding->m_addr),
                          ("> ADD: failed to claim IP %R[binding]\n", pNewBinding),
                          VERR_INTERNAL_ERROR);
    try
    {
        m_bindings.push_back(pNewBinding);
    }
    catch (std::bad_alloc &)
    {
        return VERR_NO_MEMORY;
    }
    return VINF_SUCCESS;
}


/**
 * Called by DHCP to cancel an offset.
 *
 * @param   req                 The DHCP request.
 */
void Db::cancelOffer(const DhcpClientMessage &req) RT_NOEXCEPT
{
    const OptRequestedAddress reqAddr(req);
    if (!reqAddr.present())
        return;

    const RTNETADDRIPV4 addr = reqAddr.value();
    const ClientId     &id(req.clientId());

    for (bindings_t::iterator it = m_bindings.begin(); it != m_bindings.end(); ++it)
    {
        Binding *b = *it;

        if (b->addr().u == addr.u && b->id() == id)
        {
            if (b->state() == Binding::OFFERED)
            {
                LogRel2(("Db::cancelOffer: cancelling %R[binding]\n", b));
                b->setLeaseTime(0);
                b->setState(Binding::RELEASED);
            }
            else
                LogRel2(("Db::cancelOffer: not offered state: %R[binding]\n", b));
            return;
        }
    }
    LogRel2(("Db::cancelOffer: not found (%RTnaipv4, %R[id])\n", addr.u, &id));
}


/**
 * Called by DHCP to cancel an offset.
 *
 * @param   req                 The DHCP request.
 * @returns true if found and released, otherwise false.
 * @throws  nothing
 */
bool Db::releaseBinding(const DhcpClientMessage &req) RT_NOEXCEPT
{
    const RTNETADDRIPV4 addr = req.ciaddr();
    const ClientId     &id(req.clientId());

    for (bindings_t::iterator it = m_bindings.begin(); it != m_bindings.end(); ++it)
    {
        Binding *b = *it;

        if (b->addr().u == addr.u && b->id() == id)
        {
            LogRel2(("Db::releaseBinding: releasing %R[binding]\n", b));
            b->setState(Binding::RELEASED);
            return true;
        }
    }

    LogRel2(("Db::releaseBinding: not found (%RTnaipv4, %R[id])\n", addr.u, &id));
    return false;
}


/**
 * Called by DHCPD to write out the lease database to @a strFilename.
 *
 * @returns IPRT status code.
 * @param   strFilename         The file to write it to.
 */
int Db::writeLeases(const RTCString &strFilename) const RT_NOEXCEPT
{
    LogDHCP(("writing leases to %s\n", strFilename.c_str()));

    /** @todo This could easily be written directly to the file w/o going thru
     *        a xml::Document, xml::XmlFileWriter, hammering the heap and being
     *        required to catch a lot of different exceptions at various points.
     *        (RTStrmOpen, bunch of RTStrmPrintf using \%RMas and \%RMes.,
     *        RTStrmClose closely followed by a couple of renames.)
     */

    /*
     * Create the document and root element.
     */
    xml::Document doc;
    try
    {
        xml::ElementNode *pElmRoot = doc.createRootElement("Leases");
        pElmRoot->setAttribute("version", "1.0");

        /*
         * Add the leases.
         */
        for (bindings_t::const_iterator it = m_bindings.begin(); it != m_bindings.end(); ++it)
        {
            const Binding *b = *it;
            b->toXML(pElmRoot);
        }
    }
    catch (std::bad_alloc &)
    {
        return VERR_NO_MEMORY;
    }

    /*
     * Write the document to the specified file in a safe manner (written to temporary
     * file, renamed to destination on success)
     */
    try
    {
        xml::XmlFileWriter writer(doc);
        writer.write(strFilename.c_str(), true /*fSafe*/);
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
        LogDHCP(("Unknown exception while writing '%s'\n", strFilename.c_str()));
        return VERR_UNEXPECTED_EXCEPTION;
    }

    return VINF_SUCCESS;
}


/**
 * Called by DHCPD to load the lease database to @a strFilename.
 *
 * @note Does not clear the database state before doing the load.
 *
 * @returns IPRT status code.
 * @param   strFilename         The file to load it from.
 * @throws  nothing
 */
int Db::loadLeases(const RTCString &strFilename) RT_NOEXCEPT
{
    LogDHCP(("loading leases from %s\n", strFilename.c_str()));

    /*
     * Load the file into an XML document.
     */
    xml::Document doc;
    try
    {
        xml::XmlFileParser parser;
        parser.read(strFilename.c_str(), doc);
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
        LogDHCP(("Unknown exception while reading and parsing '%s'\n", strFilename.c_str()));
        return VERR_UNEXPECTED_EXCEPTION;
    }

    /*
     * Check that the root element is "Leases" and process its children.
     */
    xml::ElementNode *pElmRoot = doc.getRootElement();
    if (!pElmRoot)
    {
        LogDHCP(("No root element in '%s'\n", strFilename.c_str()));
        return VERR_NOT_FOUND;
    }
    if (!pElmRoot->nameEquals("Leases"))
    {
        LogDHCP(("No root element is not 'Leases' in '%s', but '%s'\n", strFilename.c_str(), pElmRoot->getName()));
        return VERR_NOT_FOUND;
    }

    int                     rc = VINF_SUCCESS;
    xml::NodesLoop          it(*pElmRoot);
    const xml::ElementNode *pElmLease;
    while ((pElmLease = it.forAllNodes()) != NULL)
    {
        if (pElmLease->nameEquals("Lease"))
        {
            int rc2 = i_loadLease(pElmLease);
            if (RT_SUCCESS(rc2))
            { /* likely */ }
            else if (rc2 == VERR_NO_MEMORY)
                return rc2;
            else
                rc = -rc2;
        }
        else
            LogDHCP(("Ignoring unexpected element '%s' under 'Leases'...\n", pElmLease->getName()));
    }

    return rc;
}


/**
 * Internal worker for loadLeases() that handles one 'Lease' element.
 *
 * @param   pElmLease           The 'Lease' element to handle.
 * @return  IPRT status code.
 */
int Db::i_loadLease(const xml::ElementNode *pElmLease) RT_NOEXCEPT
{
    Binding *pBinding = NULL;
    try
    {
        pBinding = Binding::fromXML(pElmLease);
    }
    catch (std::bad_alloc &)
    {
        return VERR_NO_MEMORY;
    }
    if (pBinding)
    {
        bool fExpired = pBinding->expire();
        if (!fExpired)
            LogDHCP(("> LOAD:         lease %R[binding]\n", pBinding));
        else
            LogDHCP(("> LOAD: EXPIRED lease %R[binding]\n", pBinding));

        int rc = i_addBinding(pBinding);
        if (RT_FAILURE(rc))
            delete pBinding;
        return rc;
    }
    LogDHCP(("> LOAD: failed to load lease!\n"));
    return VERR_PARSE_ERROR;
}
