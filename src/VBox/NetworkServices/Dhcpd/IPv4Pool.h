/* $Id$ */
/** @file
 * DHCP server - a pool of IPv4 addresses
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

#ifndef _DHCPD_IPV4_POOL_H_
#define _DHCPD_IPV4_POOL_H_

#include <iprt/asm.h>
#include <iprt/stdint.h>
#include <iprt/net.h>
#include <set>

typedef uint32_t ip_haddr_t;    /* in host order */


/*
 * A range of IPv4 addresses (in host order).
 */
struct IPv4Range
{
    ip_haddr_t FirstAddr;
    ip_haddr_t LastAddr;        /* inclusive */

    IPv4Range()
      : FirstAddr(), LastAddr() {}

    explicit IPv4Range(ip_haddr_t aSingleAddr)
      : FirstAddr(aSingleAddr), LastAddr(aSingleAddr) {}

    IPv4Range(ip_haddr_t aFirstAddr, ip_haddr_t aLastAddr)
      : FirstAddr(aFirstAddr), LastAddr(aLastAddr) {}

    explicit IPv4Range(RTNETADDRIPV4 aSingleAddr)
      : FirstAddr(RT_N2H_U32(aSingleAddr.u)), LastAddr(RT_N2H_U32(aSingleAddr.u)) {}

    IPv4Range(RTNETADDRIPV4 aFirstAddr, RTNETADDRIPV4 aLastAddr)
      : FirstAddr(RT_N2H_U32(aFirstAddr.u)), LastAddr(RT_N2H_U32(aLastAddr.u)) {}

    bool isValid() const
    {
        return FirstAddr <= LastAddr;
    }

    bool contains(ip_haddr_t addr) const
    {
        return FirstAddr <= addr && addr <= LastAddr;
    }

    bool contains(RTNETADDRIPV4 addr) const
    {
        return contains(RT_N2H_U32(addr.u));
    }

    bool contains(const IPv4Range &range) const
    {
        return range.isValid() && FirstAddr <= range.FirstAddr && range.LastAddr <= LastAddr;
    }
};


inline bool operator==(const IPv4Range &l, const IPv4Range &r)
{
    return l.FirstAddr == r.FirstAddr && l.LastAddr == r.LastAddr;
}


inline bool operator<(const IPv4Range &l, const IPv4Range &r)
{
    return l.LastAddr < r.FirstAddr;
}


class IPv4Pool
{
    typedef std::set<IPv4Range> set_t;
    typedef set_t::iterator it_t;

    IPv4Range m_range;
    set_t m_pool;

public:
    IPv4Pool() {}

    int init(const IPv4Range &aRange);
    int init(RTNETADDRIPV4 aFirstAddr, RTNETADDRIPV4 aLastAddr);

    bool contains(RTNETADDRIPV4 addr) const
      { return m_range.contains(addr); }

    int insert(const IPv4Range &range);

#if 0
    int insert(ip_haddr_t single)
      { return insert(IPv4Range(single)); }
#endif

    int insert(ip_haddr_t first, ip_haddr_t last)
      { return insert(IPv4Range(first, last)); }

    int insert(RTNETADDRIPV4 single)
      { return insert(IPv4Range(single)); }

    int insert(RTNETADDRIPV4 first, RTNETADDRIPV4 last)
      { return insert(IPv4Range(first, last)); }

    RTNETADDRIPV4 allocate();
    bool allocate(RTNETADDRIPV4);
};

#endif  /* _DHCPD_IPV4_POOL_H_ */
