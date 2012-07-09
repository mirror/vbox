/* $Id$ */
/** @file
 * PDM Network Shaper - Internal data structures and functions common for both
 * R0 and R3 parts.
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/**
 * Bandwidth group instance data
 */
typedef struct PDMNSBWGROUP
{
    /** Pointer to the next group in the list. */
    struct PDMNSBWGROUP                        *pNext;
    /** Pointer to the shared UVM structure. */
    struct PDMNETSHAPER                        *pShaper;
    /** Critical section protecting all members below. */
    PDMCRITSECT               cs;
    /** Pointer to the first filter attached to this group. */
    struct PDMNSFILTER                         *pFiltersHead;
    /** Bandwidth group name. */
    char                                       *pszName;
    /** Maximum number of bytes filters are allowed to transfer. */
    volatile uint64_t                           cbTransferPerSecMax;
    /** Number of bytes we are allowed to transfer in one burst. */
    volatile uint32_t                           cbBucketSize;
    /** Number of bytes we were allowed to transfer at the last update. */
    volatile uint32_t                           cbTokensLast;
    /** Timestamp of the last update */
    volatile uint64_t                           tsUpdatedLast;
    /** Reference counter - How many filters are associated with this group. */
    volatile uint32_t                           cRefs;
} PDMNSBWGROUP;
/** Pointer to a bandwidth group. */
typedef PDMNSBWGROUP *PPDMNSBWGROUP;

DECLINLINE(bool) pdmNsAllocateBandwidth(PPDMNSFILTER pFilter, uint32_t cbTransfer)
{
    AssertPtrReturn(pFilter, true);
    if (!VALID_PTR(pFilter->CTX_SUFF(pBwGroup)))
        return true;

    PPDMNSBWGROUP pBwGroup = ASMAtomicReadPtrT(&pFilter->CTX_SUFF(pBwGroup), PPDMNSBWGROUP);
    int rc = PDMCritSectEnter(&pBwGroup->cs, VERR_SEM_BUSY); AssertRC(rc);
    if (RT_UNLIKELY(rc == VERR_SEM_BUSY))
        return true;
    bool fAllowed = true;
    if (pBwGroup->cbTransferPerSecMax)
    {
        /* Re-fill the bucket first */
        uint64_t tsNow = RTTimeSystemNanoTS();
        uint32_t uTokensAdded = (tsNow - pBwGroup->tsUpdatedLast)*pBwGroup->cbTransferPerSecMax/(1000*1000*1000);
        uint32_t uTokens = RT_MIN(pBwGroup->cbBucketSize, uTokensAdded + pBwGroup->cbTokensLast);

        if (cbTransfer > uTokens)
        {
            fAllowed = false;
            ASMAtomicWriteBool(&pFilter->fChoked, true);
        }
        else
        {
            pBwGroup->tsUpdatedLast = tsNow;
            pBwGroup->cbTokensLast = uTokens - cbTransfer;
        }
        Log2((LOG_FN_FMT "BwGroup=%#p{%s} cbTransfer=%u uTokens=%u uTokensAdded=%u fAllowed=%RTbool\n",
              __PRETTY_FUNCTION__, pBwGroup, pBwGroup->pszName, cbTransfer, uTokens, uTokensAdded, fAllowed));
    }
    else
        Log2((LOG_FN_FMT "BwGroup=%#p{%s} disabled fAllowed=%RTbool\n",
              __PRETTY_FUNCTION__, pBwGroup, pBwGroup->pszName, fAllowed));

    rc = PDMCritSectLeave(&pBwGroup->cs); AssertRC(rc);
    return fAllowed;
}
