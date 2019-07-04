/* $Id$ */
/** @file
 * DHCP server - A pool of IPv4 addresses.
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

#include "IPv4Pool.h"


int IPv4Pool::init(const IPv4Range &aRange)
{
    AssertReturn(aRange.isValid(), VERR_INVALID_PARAMETER);

    m_range = aRange;
    m_pool.insert(m_range);
    return VINF_SUCCESS;
}


int IPv4Pool::init(RTNETADDRIPV4 aFirstAddr, RTNETADDRIPV4 aLastAddr)
{
    return init(IPv4Range(aFirstAddr, aLastAddr));
}


/**
 * Internal worker for inserting a range into the pool of available addresses.
 *
 * @returns IPRT status code (asserted).
 * @param   a_Range         The range to insert.
 */
int IPv4Pool::insert(const IPv4Range &a_Range)
{
    /*
     * Check preconditions. Asserting because nobody checks the return code.
     */
    AssertReturn(m_range.isValid(), VERR_INVALID_STATE);
    AssertReturn(a_Range.isValid(), VERR_INVALID_PARAMETER);
    AssertReturn(m_range.contains(a_Range), VERR_INVALID_PARAMETER);

    /*
     * Check that the incoming range doesn't overlap with existing ranges in the pool.
     */
    it_t itHint = m_pool.upper_bound(IPv4Range(a_Range.LastAddr)); /* successor, insertion hint */
#if 0 /** @todo r=bird: This code is wrong.  It has no end() check for starters.  Since the method is
       *                only for internal consumption, I've replaced it with a strict build assertion. */
    if (itHint != m_pool.begin())
    {
        it_t prev(itHint);
        --prev;
        if (a_Range.FirstAddr <= prev->LastAddr)
        {
            LogDHCP(("%08x-%08x conflicts with %08x-%08x\n",
                     a_Range.FirstAddr, a_Range.LastAddr,
                     prev->FirstAddr, prev->LastAddr));
            return VERR_INVALID_PARAMETER;
        }
    }
#endif
#ifdef VBOX_STRICT
    for (it_t it2 = m_pool.begin(); it2 != m_pool.end(); ++it2)
        AssertMsg(it2->LastAddr < a_Range.FirstAddr || it2->FirstAddr > a_Range.LastAddr,
                  ("%08RX32-%08RX32 conflicts with %08RX32-%08RX32\n",
                   a_Range.FirstAddr, a_Range.LastAddr, it2->FirstAddr, it2->LastAddr));
#endif

    /*
     * No overlaps, insert it.
     */
    m_pool.insert(itHint, a_Range);
    return VINF_SUCCESS;
}


/**
 * Allocates an available IPv4 address from the pool.
 *
 * @returns Non-zero network order IPv4 address on success, zero address
 *          (0.0.0.0) on failure.
 */
RTNETADDRIPV4 IPv4Pool::allocate()
{
    RTNETADDRIPV4 RetAddr;
    if (!m_pool.empty())
    {
        /* Grab the first address in the pool: */
        it_t itBeg = m_pool.begin();
        RetAddr.u = RT_H2N_U32(itBeg->FirstAddr);

        if (itBeg->FirstAddr == itBeg->LastAddr)
            m_pool.erase(itBeg);
        else
        {
            /* Trim the entry (re-inserting it): */
            IPv4Range trimmed = *itBeg;
            trimmed.FirstAddr += 1;
            m_pool.erase(itBeg);
            m_pool.insert(trimmed);
        }
    }
    else
        RetAddr.u = 0;
    return RetAddr;
}


/**
 * Allocate the given address.
 *
 * @returns Success indicator.
 * @param   a_Addr      The IP address to allocate (network order).
 */
bool IPv4Pool::allocate(RTNETADDRIPV4 a_Addr)
{
    /*
     * Find the range containing a_Addr.
     */
    it_t it = m_pool.lower_bound(IPv4Range(a_Addr)); /* candidate range */
    if (it != m_pool.end())
    {
        Assert(RT_N2H_U32(a_Addr.u) <= it->LastAddr); /* by definition of < and lower_bound */

        if (it->contains(a_Addr))
        {
            /*
             * Remove a_Addr from the range by way of re-insertion.
             */
            const IPV4HADDR haddr = RT_N2H_U32(a_Addr.u);
            IPV4HADDR       first = it->FirstAddr;
            IPV4HADDR       last  = it->LastAddr;

            m_pool.erase(it);
            if (first != last)
            {
                if (haddr == first)
                    insert(++first, last);
                else if (haddr == last)
                    insert(first, --last);
                else
                {
                    insert(first, haddr - 1);
                    insert(haddr + 1, last);
                }
            }

            return true;
        }
    }
    return false;
}
