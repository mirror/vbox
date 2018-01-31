/* $Id$ */
/** @file
 * DHCP Message and its de/serialization.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "DhcpMessage.h"
#include "DhcpOptions.h"

#include <iprt/string.h>
#include <iprt/stream.h>



DhcpMessage::DhcpMessage()
  : m_xid(0), m_flags(0),
    m_ciaddr(), m_yiaddr(), m_siaddr(), m_giaddr(),
    m_sname(), m_file(),
    m_optMessageType()
{
}


/* static */
DhcpClientMessage *DhcpClientMessage::parse(bool broadcasted, const void *buf, size_t buflen)
{
    if (buflen < RT_OFFSETOF(RTNETBOOTP, bp_vend.Dhcp.dhcp_opts))
    {
        RTPrintf("%s: %zu bytes datagram is too short\n", __func__, buflen);
        return NULL;
    }

    PCRTNETBOOTP bp = (PCRTNETBOOTP)buf;

    if (bp->bp_op != RTNETBOOTP_OP_REQUEST)
    {
        RTPrintf("%s: bad opcode: %d\n", __func__, bp->bp_op);
        return NULL;
    }

    if (bp->bp_htype != RTNET_ARP_ETHER)
    {
        RTPrintf("%s: unsupported htype %d\n", __func__, bp->bp_htype);
        return NULL;
    }

    if (bp->bp_hlen != sizeof(RTMAC))
    {
        RTPrintf("%s: unexpected hlen %d\n", __func__, bp->bp_hlen);
        return NULL;
    }

    if (   (bp->bp_chaddr.Mac.au8[0] & 0x01) != 0
        && (bp->bp_flags & RTNET_DHCP_FLAG_BROADCAST) == 0)
    {
        RTPrintf("%s: multicast chaddr %RTmac without broadcast flag\n",
                 __func__, &bp->bp_chaddr.Mac);
    }

    /* we don't want to deal with forwarding */
    if (bp->bp_giaddr.u != 0)
    {
        RTPrintf("%s: giaddr %RTnaipv4\n", __func__, bp->bp_giaddr.u);
        return NULL;
    }

    if (bp->bp_hops != 0)
    {
        RTPrintf("%s: non-zero hops %d\n", __func__, bp->bp_hops);
        return NULL;
    }

    std::unique_ptr<DhcpClientMessage> msg(new DhcpClientMessage());

    msg->m_broadcasted = broadcasted;

    msg->m_xid = bp->bp_xid;
    msg->m_flags = bp->bp_flags;

    msg->m_mac = bp->bp_chaddr.Mac;

    msg->m_ciaddr = bp->bp_ciaddr;
    msg->m_yiaddr = bp->bp_yiaddr;
    msg->m_siaddr = bp->bp_siaddr;
    msg->m_giaddr = bp->bp_giaddr;

    if (bp->bp_vend.Dhcp.dhcp_cookie != RT_H2N_U32_C(RTNET_DHCP_COOKIE))
    {
        RTPrintf("bad cookie\n");
        return NULL;
    }

    int overload;
    overload = msg->parseOptions(&bp->bp_vend.Dhcp.dhcp_opts,
                                 buflen - RT_OFFSETOF(RTNETBOOTP, bp_vend.Dhcp.dhcp_opts));
    if (overload < 0)
        return NULL;

    /* "The 'file' field MUST be interpreted next ..." */
    if (overload & DHCP_OPTION_OVERLOAD_FILE) {
        int status = msg->parseOptions(bp->bp_file, sizeof(bp->bp_file));
        if (status != 0)
            return NULL;
    }
    else if (bp->bp_file[0] != '\0')
    {
        /* must be zero terminated, ignore if not */
        const char *pszFile = (const char *)bp->bp_file;
        size_t len = RTStrNLen(pszFile, sizeof(bp->bp_file));
        if (len < sizeof(bp->bp_file))
            msg->m_file.assign(pszFile, len);
    }

    /* "... followed by the 'sname' field." */
    if (overload & DHCP_OPTION_OVERLOAD_SNAME) {
        int status = msg->parseOptions(bp->bp_sname, sizeof(bp->bp_sname));
        if (status != 0) /* NB: this includes "nested" Option Overload */
            return NULL;
    }
    else if (bp->bp_sname[0] != '\0')
    {
        /* must be zero terminated, ignore if not */
        const char *pszSName = (const char *)bp->bp_sname;
        size_t len = RTStrNLen(pszSName, sizeof(bp->bp_sname));
        if (len < sizeof(bp->bp_sname))
            msg->m_sname.assign(pszSName, len);
    }

    msg->m_optMessageType = OptMessageType(*msg);
    if (!msg->m_optMessageType.present())
        return NULL;

    msg->m_id = ClientId(msg->m_mac, OptClientId(*msg));

    return msg.release();
}


int DhcpClientMessage::parseOptions(const void *buf, size_t buflen)
{
    uint8_t opt, optlen;
    const uint8_t *data;
    int overload;

    overload = 0;

    data = static_cast<const uint8_t *>(buf);
    while (buflen > 0) {
        opt = *data++;
        --buflen;

        if (opt == RTNET_DHCP_OPT_PAD) {
            continue;
        }

        if (opt == RTNET_DHCP_OPT_END) {
            break;
        }

        if (buflen == 0) {
            RTPrintf("option %d has no length field\n", opt);
            return -1;
        }

        optlen = *data++;
        --buflen;

        if (optlen > buflen) {
            RTPrintf("option %d truncated (length %d, but only %lu bytes left)\n",
                   opt, optlen, (unsigned long)buflen);
            return -1;
        }

#if 0
        rawopts_t::const_iterator it(m_optmap.find(opt));
        if (it != m_optmap.cend())
            return -1;
#endif
        if (opt == RTNET_DHCP_OPT_OPTION_OVERLOAD) {
            if (optlen != 1) {
                RTPrintf("Overload Option (option %d) has invalid length %d\n",
                       opt, optlen);
                return -1;
            }

            overload = *data;

            if ((overload & ~DHCP_OPTION_OVERLOAD_MASK) != 0) {
                RTPrintf("Overload Option (option %d) has invalid value 0x%x\n",
                       opt, overload);
                return -1;
            }
        }
        else
        {
            m_rawopts.insert(std::make_pair(opt, octets_t(data, data + optlen)));
        }

        data += optlen;
        buflen -= optlen;
    }

    return overload;
}


void DhcpClientMessage::dump() const
{
    switch (m_optMessageType.value())
    {
        case RTNET_DHCP_MT_DISCOVER:
            LogDHCP(("DISCOVER"));
            break;

        case RTNET_DHCP_MT_REQUEST:
            LogDHCP(("REQUEST"));
            break;

        case RTNET_DHCP_MT_INFORM:
            LogDHCP(("INFORM"));
            break;

        case RTNET_DHCP_MT_DECLINE:
            LogDHCP(("DECLINE"));
            break;

        case RTNET_DHCP_MT_RELEASE:
            LogDHCP(("RELEASE"));
            break;

        default:
            LogDHCP(("<Unknown Mesage Type %d>", m_optMessageType.value()));
            break;
    }

    if (OptRapidCommit(*this).present())
        LogDHCP((" (rapid commit)"));


    const OptServerId sid(*this);
    if (sid.present())
        LogDHCP((" for server %RTnaipv4", sid.value().u));

    LogDHCP((" xid 0x%08x", m_xid));
    LogDHCP((" chaddr %RTmac\n", &m_mac));

    const OptClientId cid(*this);
    if (cid.present()) {
        if (cid.value().size() > 0)
            LogDHCP((" client id: %.*Rhxs\n", cid.value().size(), &cid.value().front()));
        else
            LogDHCP((" client id: <empty>\n"));
    }

    LogDHCP((" ciaddr %RTnaipv4", m_ciaddr.u));
    if (m_yiaddr.u != 0)
        LogDHCP((" yiaddr %RTnaipv4", m_yiaddr.u));
    if (m_siaddr.u != 0)
        LogDHCP((" siaddr %RTnaipv4", m_siaddr.u));
    if (m_giaddr.u != 0)
        LogDHCP((" giaddr %RTnaipv4", m_giaddr.u));
    LogDHCP(("%s\n", broadcast() ? "broadcast" : ""));


    const OptRequestedAddress reqAddr(*this);
    if (reqAddr.present())
        LogDHCP((" requested address %RTnaipv4", reqAddr.value().u));
    const OptLeaseTime reqLeaseTime(*this);
    if (reqLeaseTime.present())
        LogDHCP((" requested lease time %d", reqAddr.value()));
    if (reqAddr.present() || reqLeaseTime.present())
        LogDHCP(("\n"));

    const OptParameterRequest params(*this);
    if (params.present())
    {
        LogDHCP((" params {"));
        typedef OptParameterRequest::value_t::const_iterator it_t;
        for (it_t it = params.value().begin(); it != params.value().end(); ++it)
            LogDHCP((" %d", *it));
        LogDHCP((" }\n"));
    }

    bool fHeader = true;
    for (rawopts_t::const_iterator it = m_rawopts.begin();
         it != m_rawopts.end(); ++it)
    {
        const uint8_t optcode = (*it).first;
        switch (optcode) {
            case OptMessageType::optcode:      /* FALLTHROUGH */
            case OptClientId::optcode:         /* FALLTHROUGH */
            case OptRequestedAddress::optcode: /* FALLTHROUGH */
            case OptLeaseTime::optcode:        /* FALLTHROUGH */
            case OptParameterRequest::optcode: /* FALLTHROUGH */
            case OptRapidCommit::optcode:
                break;

            default:
                if (fHeader)
                {
                    LogDHCP((" other options:"));
                    fHeader = false;
                }
                LogDHCP((" %d", optcode));
                break;
        }
    }
    if (!fHeader)
        LogDHCP(("\n"));
}


DhcpServerMessage::DhcpServerMessage(const DhcpClientMessage &req,
                                     uint8_t messageType, RTNETADDRIPV4 serverAddr)
  : DhcpMessage(),
    m_optServerId(serverAddr)
{
    m_dst.u = 0xffffffff;       /* broadcast */

    m_optMessageType = OptMessageType(messageType);

    /* copy values from the request (cf. RFC2131 Table 3) */
    m_xid = req.xid();
    m_flags = req.flags();
    m_giaddr = req.giaddr();
    m_mac = req.mac();

    if (req.messageType() == RTNET_DHCP_MT_REQUEST)
        m_ciaddr = req.ciaddr();
}


void DhcpServerMessage::maybeUnicast(const DhcpClientMessage &req)
{
    if (!req.broadcast() && req.ciaddr().u != 0)
        setDst(req.ciaddr());
}


void DhcpServerMessage::addOption(DhcpOption *opt)
{
    m_optmap << opt;
}


void DhcpServerMessage::addOptions(const optmap_t &optmap)
{
    for (optmap_t::const_iterator it ( optmap.begin() );
         it != optmap.end(); ++it)
    {
        m_optmap << it->second;
    }
}


int DhcpServerMessage::encode(octets_t &data)
{
    /*
     * Header, including DHCP cookie.
     */
    RTNETBOOTP bp;
    RT_ZERO(bp);

    bp.bp_op = RTNETBOOTP_OP_REPLY;
    bp.bp_htype = RTNET_ARP_ETHER;
    bp.bp_hlen = sizeof(RTMAC);

    bp.bp_xid = m_xid;

    bp.bp_ciaddr = m_ciaddr;
    bp.bp_yiaddr = m_yiaddr;
    bp.bp_siaddr = m_siaddr;
    bp.bp_giaddr = m_giaddr;

    bp.bp_chaddr.Mac = m_mac;

    bp.bp_vend.Dhcp.dhcp_cookie = RT_H2N_U32_C(RTNET_DHCP_COOKIE);

    data.insert(data.end(), (uint8_t *)&bp, (uint8_t *)&bp.bp_vend.Dhcp.dhcp_opts);

    /*
     * Options
     */
    data << m_optServerId
         << m_optMessageType;

    for (optmap_t::const_iterator it ( m_optmap.begin() );
         it != m_optmap.end(); ++it)
    {
        RTPrintf("encoding option %d\n", it->first);
        DhcpOption &opt = *it->second;
        data << opt;
    }

    data << OptEnd();

    if (data.size() < 548)      /* XXX */
        data.resize(548);

    return VINF_SUCCESS;
}
