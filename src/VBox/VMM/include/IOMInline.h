/* $Id$ */
/** @file
 * IOM - Inlined functions.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VMM_INCLUDED_SRC_include_IOMInline_h
#define VMM_INCLUDED_SRC_include_IOMInline_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/errcore.h>

/** @addtogroup grp_iom_int   Internals
 * @internal
 * @{
 */


/**
 * Gets the I/O port entry for the specified I/O port in the current context.
 *
 * @returns Pointer to I/O port entry.
 * @returns NULL if no port registered.
 *
 * @param   pVM             The cross context VM structure.
 * @param   uPort           The I/O port to lookup.
 * @param   poffPort        Where to return the port offset relative to the
 *                          start of the I/O port range.
 * @param   pidxLastHint    Pointer to IOMCPU::idxIoPortLastRead or
 *                          IOMCPU::idxIoPortLastWrite.
 *
 * @note    In ring-0 it is possible to get an uninitialized entry (pDevIns is
 *          NULL, cPorts is 0), in which case there should be ring-3 handlers
 *          for the entry.  Use IOMIOPORTENTRYR0::idxSelf to get the ring-3
 *          entry.
 *
 * @note    This code is almost identical to iomMmioGetEntry, so keep in sync.
 */
DECLINLINE(CTX_SUFF(PIOMIOPORTENTRY)) iomIoPortGetEntry(PVMCC pVM, RTIOPORT uPort, PRTIOPORT poffPort, uint16_t *pidxLastHint)
{
    Assert(IOM_IS_SHARED_LOCK_OWNER(pVM));

#ifdef IN_RING0
    uint32_t               iEnd      = RT_MIN(pVM->iom.s.cIoPortLookupEntries, pVM->iomr0.s.cIoPortAlloc);
    PCIOMIOPORTLOOKUPENTRY paLookup  = pVM->iomr0.s.paIoPortLookup;
#else
    uint32_t               iEnd      = pVM->iom.s.cIoPortLookupEntries;
    PCIOMIOPORTLOOKUPENTRY paLookup  = pVM->iom.s.paIoPortLookup;
#endif
    if (iEnd > 0)
    {
        uint32_t iFirst = 0;
        uint32_t i      = *pidxLastHint;
        if (i < iEnd)
        { /* likely */ }
        else
            i = iEnd / 2;
        for (;;)
        {
            PCIOMIOPORTLOOKUPENTRY pCur = &paLookup[i];
            if (pCur->uFirstPort > uPort)
            {
                if (i > iFirst)
                    iEnd = i;
                else
                    break;
            }
            else if (pCur->uLastPort < uPort)
            {
                i += 1;
                if (i < iEnd)
                    iFirst = i;
                else
                    break;
            }
            else
            {
                *pidxLastHint = (uint16_t)i;
                *poffPort     = uPort - pCur->uFirstPort;

                /*
                 * Translate the 'idx' member into a pointer.
                 */
                size_t const idx = pCur->idx;
#ifdef IN_RING0
                AssertMsg(idx < pVM->iom.s.cIoPortRegs && idx < pVM->iomr0.s.cIoPortAlloc,
                          ("%#zx vs %#x/%x (port %#x)\n", idx, pVM->iom.s.cIoPortRegs, pVM->iomr0.s.cIoPortMax, uPort));
                if (idx < pVM->iomr0.s.cIoPortAlloc)
                    return &pVM->iomr0.s.paIoPortRegs[idx];
#else
                if (idx < pVM->iom.s.cIoPortRegs)
                    return &pVM->iom.s.paIoPortRegs[idx];
                AssertMsgFailed(("%#zx vs %#x (port %#x)\n", idx, pVM->iom.s.cIoPortRegs, uPort));
#endif
                break;
            }

            i = iFirst + (iEnd - iFirst) / 2;
        }
    }
    *poffPort = 0;
    return NULL;
}


#ifdef VBOX_WITH_STATISTICS
/**
 * Gets the statistics entry for an I/O port.
 *
 * @returns Pointer to stats.  Instead of NULL, a pointer to IoPortDummyStats is
 *          returned, so the caller does not need to check for NULL.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pRegEntry   The I/O port entry to get stats for.
 * @param   offPort     The offset of the  port relative to the start of the
 *                      registration entry.
 */
DECLINLINE(PIOMIOPORTSTATSENTRY) iomIoPortGetStats(PVMCC pVM, CTX_SUFF(PIOMIOPORTENTRY) pRegEntry, uint16_t offPort)
{
    size_t idxStats = pRegEntry->idxStats;
    idxStats += offPort;
# ifdef IN_RING0
    if (idxStats < pVM->iomr0.s.cIoPortStatsAllocation)
        return &pVM->iomr0.s.paIoPortStats[idxStats];
# else
    if (idxStats < pVM->iom.s.cIoPortStats)
        return &pVM->iom.s.paIoPortStats[idxStats];
# endif
    return &pVM->iom.s.IoPortDummyStats;
}
#endif


/**
 * Gets the MMIO region entry for the specified address in the current context.
 *
 * @returns Pointer to MMIO region entry.
 * @returns NULL if no MMIO region registered for the given address.
 *
 * @param   pVM             The cross context VM structure.
 * @param   GCPhys          The address to lookup.
 * @param   poffRegion      Where to return the byte offset into the MMIO
 *                          region that corresponds to @a GCPhys.
 * @param   pidxLastHint    Pointer to IOMCPU::idxMmioLastRead,
 *                          IOMCPU::idxMmioLastWrite, or similar.
 *
 * @note    In ring-0 it is possible to get an uninitialized entry (pDevIns is
 *          NULL, cbRegion is 0), in which case there should be ring-3 handlers
 *          for the entry.  Use IOMMMIOENTRYR0::idxSelf to get the ring-3 entry.
 *
 * @note    This code is almost identical to iomIoPortGetEntry, so keep in sync.
 */
DECLINLINE(CTX_SUFF(PIOMMMIOENTRY)) iomMmioGetEntry(PVMCC pVM, RTGCPHYS GCPhys, PRTGCPHYS poffRegion, uint16_t *pidxLastHint)
{
    Assert(IOM_IS_SHARED_LOCK_OWNER(pVM));

#ifdef IN_RING0
    uint32_t               iEnd      = RT_MIN(pVM->iom.s.cMmioLookupEntries, pVM->iomr0.s.cMmioAlloc);
    PCIOMMMIOLOOKUPENTRY   paLookup  = pVM->iomr0.s.paMmioLookup;
#else
    uint32_t               iEnd      = pVM->iom.s.cMmioLookupEntries;
    PCIOMMMIOLOOKUPENTRY   paLookup  = pVM->iom.s.paMmioLookup;
#endif
    if (iEnd > 0)
    {
        uint32_t iFirst = 0;
        uint32_t i      = *pidxLastHint;
        if (i < iEnd)
        { /* likely */ }
        else
            i = iEnd / 2;
        for (;;)
        {
            PCIOMMMIOLOOKUPENTRY pCur = &paLookup[i];
            if (pCur->GCPhysFirst > GCPhys)
            {
                if (i > iFirst)
                    iEnd = i;
                else
                    break;
            }
            else if (pCur->GCPhysLast < GCPhys)
            {
                i += 1;
                if (i < iEnd)
                    iFirst = i;
                else
                    break;
            }
            else
            {
                *pidxLastHint = (uint16_t)i;
                *poffRegion   = GCPhys - pCur->GCPhysFirst;

                /*
                 * Translate the 'idx' member into a pointer.
                 */
                size_t const idx = pCur->idx;
#ifdef IN_RING0
                AssertMsg(idx < pVM->iom.s.cMmioRegs && idx < pVM->iomr0.s.cMmioAlloc,
                          ("%#zx vs %#x/%x (GCPhys=%RGp)\n", idx, pVM->iom.s.cMmioRegs, pVM->iomr0.s.cMmioMax, GCPhys));
                if (idx < pVM->iomr0.s.cMmioAlloc)
                    return &pVM->iomr0.s.paMmioRegs[idx];
#else
                if (idx < pVM->iom.s.cMmioRegs)
                    return &pVM->iom.s.paMmioRegs[idx];
                AssertMsgFailed(("%#zx vs %#x (GCPhys=%RGp)\n", idx, pVM->iom.s.cMmioRegs, GCPhys));
#endif
                break;
            }

            i = iFirst + (iEnd - iFirst) / 2;
        }
    }
    *poffRegion = 0;
    return NULL;
}


#ifdef VBOX_WITH_STATISTICS
/**
 * Gets the statistics entry for an MMIO region.
 *
 * @returns Pointer to stats.  Instead of NULL, a pointer to MmioDummyStats is
 *          returned, so the caller does not need to check for NULL.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pRegEntry   The I/O port entry to get stats for.
 */
DECLINLINE(PIOMMMIOSTATSENTRY) iomMmioGetStats(PVMCC pVM, CTX_SUFF(PIOMMMIOENTRY) pRegEntry)
{
    size_t idxStats = pRegEntry->idxStats;
# ifdef IN_RING0
    if (idxStats < pVM->iomr0.s.cMmioStatsAllocation)
        return &pVM->iomr0.s.paMmioStats[idxStats];
# else
    if (idxStats < pVM->iom.s.cMmioStats)
        return &pVM->iom.s.paMmioStats[idxStats];
# endif
    return &pVM->iom.s.MmioDummyStats;
}
#endif


/**
 * Gets the MMIO range for the specified physical address in the current context.
 *
 * @returns Pointer to MMIO range.
 * @returns NULL if address not in a MMIO range.
 *
 * @param   pVM     The cross context VM structure.
 * @param   pVCpu   The cross context virtual CPU structure of the calling EMT.
 * @param   GCPhys  Physical address to lookup.
 */
DECLINLINE(PIOMMMIORANGE) iomMmioGetRange(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys)
{
    Assert(IOM_IS_SHARED_LOCK_OWNER(pVM));
    PIOMMMIORANGE pRange = pVCpu->iom.s.CTX_SUFF(pMMIORangeLast);
    if (    !pRange
        ||  GCPhys - pRange->GCPhys >= pRange->cb)
        pVCpu->iom.s.CTX_SUFF(pMMIORangeLast) = pRange
            = (PIOMMMIORANGE)RTAvlroGCPhysRangeGet(&pVM->iom.s.CTX_SUFF(pTrees)->MMIOTree, GCPhys);
    return pRange;
}

/**
 * Retain a MMIO range.
 *
 * @param   pRange  The range to release.
 */
DECLINLINE(void) iomMmioRetainRange(PIOMMMIORANGE pRange)
{
    uint32_t cRefs = ASMAtomicIncU32(&pRange->cRefs);
    Assert(cRefs > 1);
    Assert(cRefs < _1M);
    NOREF(cRefs);
}


/**
 * Gets the referenced MMIO range for the specified physical address in the
 * current context.
 *
 * @returns Pointer to MMIO range.
 * @returns NULL if address not in a MMIO range.
 *
 * @param   pVM     The cross context VM structure.
 * @param   pVCpu   The cross context virtual CPU structure of the calling EMT.
 * @param   GCPhys  Physical address to lookup.
 */
DECLINLINE(PIOMMMIORANGE) iomMmioGetRangeWithRef(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys)
{
    int rc = IOM_LOCK_SHARED_EX(pVM, VINF_SUCCESS);
    AssertRCReturn(rc, NULL);

    PIOMMMIORANGE pRange = pVCpu->iom.s.CTX_SUFF(pMMIORangeLast);
    if (   !pRange
        || GCPhys - pRange->GCPhys >= pRange->cb)
        pVCpu->iom.s.CTX_SUFF(pMMIORangeLast) = pRange
            = (PIOMMMIORANGE)RTAvlroGCPhysRangeGet(&pVM->iom.s.CTX_SUFF(pTrees)->MMIOTree, GCPhys);
    if (pRange)
        iomMmioRetainRange(pRange);

    IOM_UNLOCK_SHARED(pVM);
    return pRange;
}


/**
 * Releases a MMIO range.
 *
 * @param   pVM     The cross context VM structure.
 * @param   pRange  The range to release.
 */
DECLINLINE(void) iomMmioReleaseRange(PVMCC pVM, PIOMMMIORANGE pRange)
{
    uint32_t cRefs = ASMAtomicDecU32(&pRange->cRefs);
    if (!cRefs)
        iomMmioFreeRange(pVM, pRange);
}


#ifdef VBOX_STRICT
/**
 * Gets the MMIO range for the specified physical address in the current context.
 *
 * @returns Pointer to MMIO range.
 * @returns NULL if address not in a MMIO range.
 *
 * @param   pVM     The cross context VM structure.
 * @param   pVCpu   The cross context virtual CPU structure of the calling EMT.
 * @param   GCPhys  Physical address to lookup.
 */
DECLINLINE(PIOMMMIORANGE) iomMMIOGetRangeUnsafe(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys)
{
    PIOMMMIORANGE pRange = pVCpu->iom.s.CTX_SUFF(pMMIORangeLast);
    if (    !pRange
        ||  GCPhys - pRange->GCPhys >= pRange->cb)
        pVCpu->iom.s.CTX_SUFF(pMMIORangeLast) = pRange
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
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   GCPhys      Physical address to lookup.
 * @param   pRange      The MMIO range.
 *
 * @remarks The caller holds the IOM critical section with shared access prior
 *          to calling this method.  Upon return, the lock has been released!
 *          This is ugly, but it's a necessary evil since we cannot upgrade read
 *          locks to write locks and the whole purpose here is calling
 *          iomR3MMIOStatsCreate.
 */
DECLINLINE(PIOMMMIOSTATS) iomMmioGetStats(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys, PIOMMMIORANGE pRange)
{
    Assert(IOM_IS_SHARED_LOCK_OWNER(pVM));

    /* For large ranges, we'll put everything on the first byte. */
    if (pRange->cb > PAGE_SIZE)
        GCPhys = pRange->GCPhys;

    PIOMMMIOSTATS pStats = pVCpu->iom.s.CTX_SUFF(pMMIOStatsLast);
    if (    !pStats
        ||  pStats->Core.Key != GCPhys)
    {
        pStats = (PIOMMMIOSTATS)RTAvloGCPhysGet(&pVM->iom.s.CTX_SUFF(pTrees)->MmioStatTree, GCPhys);
# ifdef IN_RING3
        if (!pStats)
        {
            IOM_UNLOCK_SHARED(pVM);
            return iomR3MMIOStatsCreate(pVM, GCPhys, pRange->pszDesc);
        }
# endif
    }

    IOM_UNLOCK_SHARED(pVM);
    return pStats;
}
#endif /* VBOX_WITH_STATISTICS */


/** @}  */

#endif /* !VMM_INCLUDED_SRC_include_IOMInline_h */

