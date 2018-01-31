/* $Id$ */
/** @file
 * DHCP Message and its de/serialization.
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
#ifndef _DHCP_MESSAGE_H_
#define _DHCP_MESSAGE_H_

#include "Defs.h"
#include <iprt/net.h>
#include <string>
#include "ClientId.h"
#include "DhcpOptions.h"


/* move to <iptr/net.h>? */
#define DHCP_OPTION_OVERLOAD_MASK  0x3
#define DHCP_OPTION_OVERLOAD_FILE  0x1
#define DHCP_OPTION_OVERLOAD_SNAME 0x2


class DhcpMessage
{
protected:
    uint32_t m_xid;
    uint16_t m_flags;

    RTMAC m_mac;

    RTNETADDRIPV4   m_ciaddr;
    RTNETADDRIPV4   m_yiaddr;
    RTNETADDRIPV4   m_siaddr;
    RTNETADDRIPV4   m_giaddr;

    std::string m_sname;
    std::string m_file;

    OptMessageType m_optMessageType;

public:
    DhcpMessage();


    uint32_t xid() const { return m_xid; }

    uint16_t flags() const { return m_flags; }
    bool broadcast() const { return (m_flags & RTNET_DHCP_FLAG_BROADCAST) != 0; }

    const RTMAC &mac() const { return m_mac; }

    RTNETADDRIPV4 ciaddr() const { return m_ciaddr; }
    RTNETADDRIPV4 yiaddr() const { return m_yiaddr; }
    RTNETADDRIPV4 siaddr() const { return m_siaddr; }
    RTNETADDRIPV4 giaddr() const { return m_giaddr; }

    void setCiaddr(RTNETADDRIPV4 addr) { m_ciaddr = addr; }
    void setYiaddr(RTNETADDRIPV4 addr) { m_yiaddr = addr; }
    void setSiaddr(RTNETADDRIPV4 addr) { m_siaddr = addr; }
    void setGiaddr(RTNETADDRIPV4 addr) { m_giaddr = addr; }

    uint8_t messageType() const
    {
        Assert(m_optMessageType.present());
        return m_optMessageType.value();
    }
};


class DhcpClientMessage
  : public DhcpMessage
{
protected:
    rawopts_t m_rawopts;
    ClientId m_id;
    bool m_broadcasted;

public:
    static DhcpClientMessage *parse(bool broadcasted, const void *buf, size_t buflen);

    bool broadcasted() const { return m_broadcasted; }

    const rawopts_t &rawopts() const { return m_rawopts; }
    const ClientId &clientId() const { return m_id; }

    void dump() const;

protected:
    int parseOptions(const void *buf, size_t buflen);
};



class DhcpServerMessage
  : public DhcpMessage
{
protected:
    RTNETADDRIPV4 m_dst;

    OptServerId m_optServerId;

    optmap_t m_optmap;

public:
    DhcpServerMessage(const DhcpClientMessage &req,
                      uint8_t messageType, RTNETADDRIPV4 serverAddr);

    RTNETADDRIPV4 dst() const { return m_dst; }
    void setDst(RTNETADDRIPV4 aDst) { m_dst = aDst; }

    void maybeUnicast(const DhcpClientMessage &req);

    void addOption(DhcpOption *opt);
    void addOption(const DhcpOption &opt)
    {
        addOption(opt.clone());
    }

    void addOptions(const optmap_t &optmap);

    int encode(octets_t &data);
};

#endif /* _DHCP_MESSAGE_H_ */
