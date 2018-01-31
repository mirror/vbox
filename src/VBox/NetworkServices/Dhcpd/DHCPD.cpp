/* $Id$ */
/** @file
 * DHCP server - protocol logic
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

#include "DHCPD.h"
#include "DhcpOptions.h"

#include <iprt/path.h>


DHCPD::DHCPD()
  : m_pConfig(NULL), m_db()
{
}


int DHCPD::init(const Config *pConfig)
{
    int rc;

    if (m_pConfig != NULL)
        return VERR_INVALID_STATE;

    /* leases file name */
    m_strLeasesFileName = pConfig->getHome();
    m_strLeasesFileName += RTPATH_DELIMITER;
    m_strLeasesFileName += pConfig->getBaseName();
    m_strLeasesFileName += "-Dhcpd.leases";

    rc = m_db.init(pConfig);
    if (RT_FAILURE(rc))
        return rc;

    loadLeases();

    m_pConfig = pConfig;
    return VINF_SUCCESS;
}


void DHCPD::loadLeases()
{
    m_db.loadLeases(m_strLeasesFileName);
}


void DHCPD::saveLeases()
{
    m_db.expire();
    m_db.writeLeases(m_strLeasesFileName);
}


DhcpServerMessage *DHCPD::process(DhcpClientMessage &req)
{
    DhcpServerMessage *reply = NULL;

    req.dump();

    OptServerId sid(req);
    if (sid.present() && sid.value().u != m_pConfig->getIPv4Address().u)
    {
        if (req.broadcasted() && req.messageType() == RTNET_DHCP_MT_REQUEST)
            m_db.cancelOffer(req);

        return NULL;
    }

    switch (req.messageType())
    {
        /*
         * Requests that require server's reply.
         */
        case RTNET_DHCP_MT_DISCOVER:
            reply = doDiscover(req);
            break;

        case RTNET_DHCP_MT_REQUEST:
            reply = doRequest(req);
            break;

        case RTNET_DHCP_MT_INFORM:
            reply = doInform(req);
            break;

        /*
         * Requests that don't have a reply.
         */
        case RTNET_DHCP_MT_DECLINE:
            doDecline(req);
            break;

        case RTNET_DHCP_MT_RELEASE:
            doRelease(req);
            break;

        /*
         * Unexpected or unknown message types.
         */
        case RTNET_DHCP_MT_OFFER: /* FALLTHROUGH */
        case RTNET_DHCP_MT_ACK:   /* FALLTHROUGH */
        case RTNET_DHCP_MT_NAC:   /* FALLTHROUGH */
        default:
            break;
    }

    return reply;
}


DhcpServerMessage *DHCPD::createMessage(int type, DhcpClientMessage &req)
{
    return new DhcpServerMessage(req, type, m_pConfig->getIPv4Address());
}


DhcpServerMessage *DHCPD::doDiscover(DhcpClientMessage &req)
{
    /*
     * XXX: TODO: Windows iSCSI initiator sends DHCPDISCOVER first and
     * it has ciaddr filled.  Shouldn't let it screw up the normal
     * lease we already have for that client, but we should probably
     * reply with a pro-forma offer.
     */
    if (req.ciaddr().u != 0)
        return NULL;

    Binding *b = m_db.allocateBinding(req);
    if (b == NULL)
        return NULL;


    std::unique_ptr<DhcpServerMessage> reply;

    bool fRapidCommit = OptRapidCommit(req).present();
    if (!fRapidCommit)
    {
        reply.reset(createMessage(RTNET_DHCP_MT_OFFER, req));

        if (b->state() < Binding::OFFERED)
            b->setState(Binding::OFFERED);

        /* use small lease time internally to quickly free unclaimed offers? */
    }
    else
    {
        reply.reset(createMessage(RTNET_DHCP_MT_ACK, req));
        reply->addOption(OptRapidCommit(true));

        b->setState(Binding::ACKED);
        saveLeases();
    }

    reply->setYiaddr(b->addr());
    reply->addOption(OptLeaseTime(b->leaseTime()));


    OptParameterRequest optlist(req);
    reply->addOptions(m_pConfig->getOptions(optlist, req.clientId()));

    // reply->maybeUnicast(req); /* XXX: we reject ciaddr != 0 above */
    return reply.release();
}


DhcpServerMessage *DHCPD::doRequest(DhcpClientMessage &req)
{
    OptRequestedAddress reqAddr(req);
    if (req.ciaddr().u != 0 && reqAddr.present() && reqAddr.value().u != req.ciaddr().u)
    {
        std::unique_ptr<DhcpServerMessage> nak (
            createMessage(RTNET_DHCP_MT_NAC, req)
        );
        nak->addOption(OptMessage("Requested address does not match ciaddr"));
        return nak.release();
    }


    Binding *b = m_db.allocateBinding(req);
    if (b == NULL)
    {
        return createMessage(RTNET_DHCP_MT_NAC, req);
    }


    std::unique_ptr<DhcpServerMessage> ack (
        createMessage(RTNET_DHCP_MT_ACK, req)
    );

    b->setState(Binding::ACKED);
    saveLeases();

    ack->setYiaddr(b->addr());
    ack->addOption(OptLeaseTime(b->leaseTime()));

    OptParameterRequest optlist(req);
    ack->addOptions(m_pConfig->getOptions(optlist, req.clientId()));

    ack->addOption(OptMessage("Ok, ok, here it is"));

    ack->maybeUnicast(req);
    return ack.release();
}


/*
 * 4.3.5 DHCPINFORM message
 *
 *    The server responds to a DHCPINFORM message by sending a DHCPACK
 *    message directly to the address given in the 'ciaddr' field of the
 *    DHCPINFORM message.  The server MUST NOT send a lease expiration time
 *    to the client and SHOULD NOT fill in 'yiaddr'.  The server includes
 *    other parameters in the DHCPACK message as defined in section 4.3.1.
 */
DhcpServerMessage *DHCPD::doInform(DhcpClientMessage &req)
{
    if (req.ciaddr().u == 0)
        return NULL;

    const OptParameterRequest params(req);
    if (!params.present())
        return NULL;

    optmap_t info(m_pConfig->getOptions(params, req.clientId()));
    if (info.empty())
        return NULL;

    std::unique_ptr<DhcpServerMessage> ack (
        createMessage(RTNET_DHCP_MT_ACK, req)
    );

    ack->addOptions(info);

    ack->maybeUnicast(req);
    return ack.release();
}


/*
 * 4.3.3 DHCPDECLINE message
 *
 *    If the server receives a DHCPDECLINE message, the client has
 *    discovered through some other means that the suggested network
 *    address is already in use.  The server MUST mark the network address
 *    as not available and SHOULD notify the local system administrator of
 *    a possible configuration problem.
 */
DhcpServerMessage *DHCPD::doDecline(DhcpClientMessage &req)
{
    RT_NOREF(req);
    return NULL;
}


/*
 * 4.3.4 DHCPRELEASE message
 *
 *    Upon receipt of a DHCPRELEASE message, the server marks the network
 *    address as not allocated.  The server SHOULD retain a record of the
 *    client's initialization parameters for possible reuse in response to
 *    subsequent requests from the client.
 */
DhcpServerMessage *DHCPD::doRelease(DhcpClientMessage &req)
{
    if (req.ciaddr().u == 0)
        return NULL;

    bool released = m_db.releaseBinding(req);
    if (released)
        saveLeases();

    return NULL;
}
