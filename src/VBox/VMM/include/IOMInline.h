/* $Id$ */
/** @file
 * IOM - Inlined functions.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___IOMInline_h
#define ___IOMInline_h

/** @addtogroup grp_iom_int   Internals
 * @internal
 * @{
 */

/**
 * Gets the I/O port range for the specified I/O port in the current context.
 *
 * @returns Pointer to I/O port range.
 * @returns NULL if no port registered.
 *
 * @param   pVM     The VM handle.
 * @param   Port    The I/O port lookup.
 */
DECLINLINE(CTX_SUFF(PIOMIOPORTRANGE)) iomIOPortGetRange(PVM pVM, RTIOPORT Port)
{
    Assert(PDMCritSectIsOwner(&pVM->iom.s.EmtLock) || !PDMCritSectIsInitialized(&pVM->iom.s.EmtLock));
    return (CTX_SUFF(PIOMIOPORTRANGE))RTAvlroIOPortRangeGet(&pVM->iom.s.CTX_SUFF(pTrees)->CTX_SUFF(IOPortTree), Port);
}


/**
 * Gets the I/O port range for the specified I/O port in the HC.
 *
 * @returns Pointer to I/O port range.
 * @returns NULL if no port registered.
 *
 * @param   pVM     The VM handle.
 * @param   Port    The I/O port to lookup.
 */
DECLINLINE(PIOMIOPORTRANGER3) iomIOPortGetRangeR3(PVM pVM, RTIOPORT Port)
{
    Assert(PDMCritSectIsOwner(&pVM->iom.s.EmtLock) || !PDMCritSectIsInitialized(&pVM->iom.s.EmtLock));
    return (PIOMIOPORTRANGER3)RTAvlroIOPortRangeGet(&pVM->iom.s.CTX_SUFF(pTrees)->IOPortTreeR3, Port);
}


/**
 * Gets the MMIO range for the specified physical address in the current context.
 *
 * @returns Pointer to MMIO range.
 * @returns NULL if address not in a MMIO range.
 *
 * @param   pVM     The VM handle.
 * @param   GCPhys  Physical address to lookup.
 */
DECLINLINE(PIOMMMIORANGE) iomMMIOGetRange(PVM pVM, RTGCPHYS GCPhys)
{
    Assert(PDMCritSectIsOwner(&pVM->iom.s.EmtLock) || !PDMCritSectIsInitialized(&pVM->iom.s.EmtLock));
    PIOMMMIORANGE pRange = pVM->iom.s.CTX_SUFF(pMMIORangeLast);
    if (    !pRange
        ||  GCPhys - pRange->GCPhys >= pRange->cb)
        pVM->iom.s.CTX_SUFF(pMMIORangeLast) = pRange 
            = (PIOMMMIORANGE)RTAvlroGCPhysRangeGet(&pVM->iom.s.CTX_SUFF(pTrees)->MMIOTree, GCPhys);
    return pRange;
}


#ifdef VBOX_STRICT
/**
 * Gets the MMIO range for the specified physical address in the current context.
 *
 * @returns Pointer to MMIO range.
 * @returns NULL if address not in a MMIO range.
 *
 * @param   pVM     The VM handle.
 * @param   GCPhys  Physical address to lookup.
 */
DECLINLINE(PIOMMMIORANGE) iomMMIOGetRangeUnsafe(PVM pVM, RTGCPHYS GCPhys)
{
    PIOMMMIORANGE pRange = pVM->iom.s.CTX_SUFF(pMMIORangeLast);
    if (    !pRange
        ||  GCPhys - pRange->GCPhys >= pRange->cb)
        pVM->iom.s.CTX_SUFF(pMMIORangeLast) = pRange 
            = (PIOMMMIORANGE)RTAvlroGCPhysRangeGet(&pVM->iom.s.CTX_SUFF(pTrees)->MMIOTree, GCPhys);
    return pRange;
}
#endif /* VBOX_STRICT */


#ifdef VBOX_WITH_STATISTICS
/**
 * Gets the MMIO statistics record.
 *
 * In ring-3 this will lazily create missing records, while in GC/R0 the caller has to
 * return the appropriate status to defer the operation to ring-3.
 *
 * @returns Pointer to MMIO stats.
 * @returns NULL if not found (R0/GC), or out of memory (R3).
 *
 * @param   pVM         The VM handle.
 * @param   GCPhys      Physical address to lookup.
 * @param   pRange      The MMIO range.
 */
DECLINLINE(PIOMMMIOSTATS) iomMMIOGetStats(PVM pVM, RTGCPHYS GCPhys, PIOMMMIORANGE pRange)
{
    Assert(PDMCritSectIsOwner(&pVM->iom.s.EmtLock));

    /* For large ranges, we'll put everything on the first byte. */
    if (pRange->cb > PAGE_SIZE)
        GCPhys = pRange->GCPhys;

    PIOMMMIOSTATS pStats = pVM->iom.s.CTX_SUFF(pMMIOStatsLast);
    if (    !pStats
        ||  pStats->Core.Key != GCPhys)
    {
        pStats = (PIOMMMIOSTATS)RTAvloGCPhysGet(&pVM->iom.s.CTX_SUFF(pTrees)->MMIOStatTree, GCPhys);
# ifdef IN_RING3
        if (!pStats)
            pStats = iomR3MMIOStatsCreate(pVM, GCPhys, pRange->pszDesc);
# endif
    }
    return pStats;
}
#endif /* VBOX_WITH_STATISTICS */


/** @}  */

#endif

