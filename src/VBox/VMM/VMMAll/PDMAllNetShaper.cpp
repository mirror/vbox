/* $Id$ */
/** @file
 * PDM Network Shaper - Limit network traffic according to bandwidth group settings.
 */

/*
 * Copyright (C) 2011-2022 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_NET_SHAPER
#include <VBox/vmm/pdmnetshaper.h>
#include "PDMInternal.h"
#include <VBox/vmm/vmcc.h>

#include <VBox/log.h>
#include <iprt/time.h>


/**
 * Obtain bandwidth in a bandwidth group.
 *
 * @returns True if bandwidth was allocated, false if not.
 * @param   pVM             The cross context VM structure.
 * @param   pFilter         Pointer to the filter that allocates bandwidth.
 * @param   cbTransfer      Number of bytes to allocate.
 */
VMM_INT_DECL(bool) PDMNetShaperAllocateBandwidth(PVMCC pVM, PPDMNSFILTER pFilter, size_t cbTransfer)
{
    AssertPtrReturn(pFilter, true);

    /*
     * If we haven't got a valid bandwidth group, we always allow the traffic.
     */
    bool fAllowed = true;
    uint32_t iGroup = ASMAtomicUoReadU32(&pFilter->iGroup);
    if (iGroup != 0)
    {
        if (iGroup <= RT_MIN(pVM->pdm.s.cNsGroups, RT_ELEMENTS(pVM->pdm.s.aNsGroups)))
        {
            PPDMNSBWGROUP pGroup = &pVM->pdm.s.aNsGroups[iGroup - 1];
            int rc = PDMCritSectEnter(pVM, &pGroup->Lock, VINF_TRY_AGAIN);
            if (rc == VINF_SUCCESS)
            {
                uint64_t const cbPerSecMax = pGroup->cbPerSecMax;
                if (cbPerSecMax > 0)
                {
                    /*
                     * Re-fill the bucket first
                     */
                    uint64_t const tsNow    = RTTimeSystemNanoTS();
                    uint64_t const cNsDelta = tsNow - pGroup->tsUpdatedLast;
                    /** @todo r=bird: there might be an overflow issue here if the gap
                     *                between two transfers is too large. */
                    uint32_t cTokensAdded   = cNsDelta * cbPerSecMax / RT_NS_1SEC;

                    uint32_t const cbBucket     = pGroup->cbBucket;
                    uint32_t const cbTokensLast = pGroup->cbTokensLast;
                    uint32_t const cTokens      = RT_MIN(cbBucket, cTokensAdded + cbTokensLast);

                    /*
                     * Allowed?
                     */
                    if (cbTransfer <= cTokens)
                    {
                        pGroup->cbTokensLast  = cTokens - (uint32_t)cbTransfer;
                        pGroup->tsUpdatedLast = tsNow;
                        Log2(("pdmNsAllocateBandwidth/%s: allowed - cbTransfer=%#zx cTokens=%#x cTokensAdded=%#x\n",
                              pGroup->szName, cbTransfer, cTokens, cTokensAdded));
                    }
                    else
                    {
                        ASMAtomicWriteBool(&pFilter->fChoked, true);
                        Log2(("pdmNsAllocateBandwidth/%s: refused - cbTransfer=%#zx cTokens=%#x cTokensAdded=%#x\n",
                              pGroup->szName, cbTransfer, cTokens, cTokensAdded));
                        fAllowed = false;
                    }
                }
                else
                    Log2(("pdmNsAllocateBandwidth/%s: disabled\n", pGroup->szName));

                rc = PDMCritSectLeave(pVM, &pGroup->Lock);
                AssertRCSuccess(rc);
            }
            else if (rc == VINF_TRY_AGAIN) /* (accounted for by the critsect stats) */
                Log2(("pdmNsAllocateBandwidth/%s: allowed - lock contention\n", pGroup->szName));
            else
                PDM_CRITSECT_RELEASE_ASSERT_RC(pVM, &pGroup->Lock, rc);
        }
        else
            AssertMsgFailed(("Invalid iGroup=%d\n", iGroup));
    }
    return fAllowed;
}
