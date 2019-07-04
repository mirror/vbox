/* $Id$ */
/** @file
 * DHCP server - a pool of IPv4 addresses
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

#ifndef VBOX_INCLUDED_SRC_Dhcpd_IPv4Pool_h
#define VBOX_INCLUDED_SRC_Dhcpd_IPv4Pool_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/asm.h>
#include <iprt/stdint.h>
#include <iprt/net.h>
#include <set>


/** Host order IPv4 address. */
typedef uint32_t IPV4HADDR;


/**
 * A range of IPv4 addresses (in host order).
 */
struct IPv4Range
{
    IPV4HADDR FirstAddr;       /**< Lowest address. */
    IPV4HADDR LastAddr;        /**< Higest address (inclusive). */

    IPv4Range()
        : FirstAddr(0), LastAddr(0)
    {}

    explicit IPv4Range(IPV4HADDR aSingleAddr)
        : FirstAddr(aSingleAddr), LastAddr(aSingleAddr)
    {}

    IPv4Range(IPV4HADDR aFirstAddr, IPV4HADDR aLastAddr)
        : FirstAddr(aFirstAddr), LastAddr(aLastAddr)
    {}

    explicit IPv4Range(RTNETADDRIPV4 aSingleAddr)
        : FirstAddr(RT_N2H_U32(aSingleAddr.u)), LastAddr(RT_N2H_U32(aSingleAddr.u))
    {}

    IPv4Range(RTNETADDRIPV4 aFirstAddr, RTNETADDRIPV4 aLastAddr)
        : FirstAddr(RT_N2H_U32(aFirstAddr.u)), LastAddr(RT_N2H_U32(aLastAddr.u))
    {}

    bool isValid() const
    {
        return FirstAddr <= LastAddr;
    }

    bool contains(IPV4HADDR addr) const
    {
        return FirstAddr <= addr && addr <= LastAddr;
    }

    bool contains(RTNETADDRIPV4 addr) const
    {
        return contains(RT_N2H_U32(addr.u));
    }

    /** Checks if this range includes the @a a_rRange. */
    bool contains(const IPv4Range &a_rRange) const
    {
        return a_rRange.isValid()
            && FirstAddr <= a_rRange.FirstAddr
            && a_rRange.LastAddr <= LastAddr;
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


/**
 * IPv4 address pool.
 *
 * This manages a single range of IPv4 addresses (m_range).   Unallocated
 * addresses are tracked as a set of sub-ranges in the m_pool set.
 *
 */
class IPv4Pool
{
    typedef std::set<IPv4Range> set_t;
    typedef set_t::iterator it_t;

    /** The IPv4 range of this pool. */
    IPv4Range   m_range;
    /** Pool of available IPv4 ranges. */
    set_t       m_pool;

public:
    IPv4Pool()
    {}

    int init(const IPv4Range &aRange);
    int init(RTNETADDRIPV4 aFirstAddr, RTNETADDRIPV4 aLastAddr);

    /**
     * Checks if the pool range includes @a addr (allocation status not considered).
     */
    bool contains(RTNETADDRIPV4 addr) const
    {
        return m_range.contains(addr);
    }

    RTNETADDRIPV4 allocate();
    bool          allocate(RTNETADDRIPV4);

private:
    int insert(const IPv4Range &range);
#if 0
    int insert(IPV4HADDR single)                            { return insert(IPv4Range(single)); }
#endif
    int insert(IPV4HADDR first, IPV4HADDR last)             { return insert(IPv4Range(first, last)); }
    int insert(RTNETADDRIPV4 single)                        { return insert(IPv4Range(single)); }
    int insert(RTNETADDRIPV4 first, RTNETADDRIPV4 last)     { return insert(IPv4Range(first, last)); }
};

#endif /* !VBOX_INCLUDED_SRC_Dhcpd_IPv4Pool_h */
