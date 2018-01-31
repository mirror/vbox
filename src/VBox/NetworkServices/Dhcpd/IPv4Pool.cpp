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

#include <iprt/err.h>
#include <iprt/stream.h>

#include "IPv4Pool.h"


int IPv4Pool::init(const IPv4Range &aRange)
{
    if (!aRange.isValid())
        return VERR_INVALID_PARAMETER;

    m_range = aRange;
    m_pool.insert(m_range);
    return VINF_SUCCESS;
}


int IPv4Pool::init(RTNETADDRIPV4 aFirstAddr, RTNETADDRIPV4 aLastAddr)
{
    IPv4Range range(aFirstAddr, aLastAddr);

    if (!range.isValid())
        return VERR_INVALID_PARAMETER;

    m_range = range;
    m_pool.insert(m_range);
    return VINF_SUCCESS;
}


int IPv4Pool::insert(const IPv4Range &range)
{
    if (!m_range.isValid())
        return VERR_INVALID_PARAMETER;

    if (!m_range.contains(range))
        return VERR_INVALID_PARAMETER;

    it_t it = m_pool.upper_bound(IPv4Range(range.LastAddr)); /* successor */
    if (it != m_pool.begin())
    {
        it_t prev(it);
        --prev;
        if (range.FirstAddr <= prev->LastAddr) {
#if 1 /* XXX */
            RTPrintf("%08x-%08x conflicts with %08x-%08x\n",
                     range.FirstAddr, range.LastAddr,
                     prev->FirstAddr, prev->LastAddr);
#endif
            return VERR_INVALID_PARAMETER;
        }
    }

    m_pool.insert(it, range);
    return VINF_SUCCESS;
}


RTNETADDRIPV4 IPv4Pool::allocate()
{
    if (m_pool.empty())
    {
        RTNETADDRIPV4 res = { 0 };
        return res;
    }

    it_t beg = m_pool.begin();
    ip_haddr_t addr = beg->FirstAddr;

    if (beg->FirstAddr == beg->LastAddr)
    {
        m_pool.erase(beg);
    }
    else
    {
        IPv4Range trimmed = *beg;
        ++trimmed.FirstAddr;
        m_pool.erase(beg);
        m_pool.insert(trimmed);
    }

    RTNETADDRIPV4 res = { RT_H2N_U32(addr) };
    return res;
}


bool IPv4Pool::allocate(RTNETADDRIPV4 addr)
{
    it_t it = m_pool.lower_bound(IPv4Range(addr)); /* candidate range */
    if (it == m_pool.end())
        return false;

    Assert(RT_N2H_U32(addr.u) <= it->LastAddr); /* by definition of < and lower_bound */

    if (!it->contains(addr))
        return false;

    const ip_haddr_t haddr = RT_N2H_U32(addr.u);
    ip_haddr_t first = it->FirstAddr;
    ip_haddr_t last = it->LastAddr;

    m_pool.erase(it);
    if (first != last)
    {
        if (haddr == first)
        {
            insert(++first, last);
        }
        else if (haddr == last)
        {
            insert(first, --last);
        }
        else
        {
            insert(first, haddr - 1);
            insert(haddr + 1, last);
        }
    }

    return true;
}
