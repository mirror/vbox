/* $Id$ */
/** @file
 * MM - Memory Monitor(/Manager) - Any Context.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_MM_HYPER
#include <VBox/mm.h>
#include "MMInternal.h"
#include <VBox/vm.h>
#include <VBox/log.h>
#include <iprt/assert.h>



/**
 * Lookup a host context ring-3 address.
 *
 * @returns Pointer to the corresponding lookup record.
 * @returns NULL on failure.
 * @param   pVM     The VM handle.
 * @param   R3Ptr   The host context ring-3 address to lookup.
 * @param   poff    Where to store the offset into the HMA memory chunk.
 */
DECLINLINE(PMMLOOKUPHYPER) mmHyperLookupR3(PVM pVM, RTR3PTR R3Ptr, uint32_t *poff)
{
    /** @todo cache last lookup this stuff ain't cheap! */
    PMMLOOKUPHYPER  pLookup = (PMMLOOKUPHYPER)((char*)CTXSUFF(pVM->mm.s.pHyperHeap) + pVM->mm.s.offLookupHyper);
    for (;;)
    {
        switch (pLookup->enmType)
        {
            case MMLOOKUPHYPERTYPE_LOCKED:
            {
                const uint32_t off = (RTR3UINTPTR)R3Ptr - (RTR3UINTPTR)pLookup->u.Locked.pvHC;
                if (off < pLookup->cb)
                {
                    *poff = off;
                    return pLookup;
                }
                break;
            }

            case MMLOOKUPHYPERTYPE_HCPHYS:
            {
                const uint32_t off = (RTR3UINTPTR)R3Ptr - (RTR3UINTPTR)pLookup->u.HCPhys.pvHC;
                if (off < pLookup->cb)
                {
                    *poff = off;
                    return pLookup;
                }
                break;
            }

            case MMLOOKUPHYPERTYPE_GCPHYS:  /* (for now we'll not allow these kind of conversions) */
            case MMLOOKUPHYPERTYPE_MMIO2:
            case MMLOOKUPHYPERTYPE_DYNAMIC:
                break;

            default:
                AssertMsgFailed(("enmType=%d\n", pLookup->enmType));
                break;
        }

        /* next */
        if (pLookup->offNext ==  (int32_t)NIL_OFFSET)
            break;
        pLookup = (PMMLOOKUPHYPER)((char *)pLookup + pLookup->offNext);
    }

    AssertMsgFailed(("R3Ptr=%p is not inside the hypervisor memory area!\n", R3Ptr));
    return NULL;
}


/**
 * Lookup a host context ring-0 address.
 *
 * @returns Pointer to the corresponding lookup record.
 * @returns NULL on failure.
 * @param   pVM     The VM handle.
 * @param   R0Ptr   The host context ring-0 address to lookup.
 * @param   poff    Where to store the offset into the HMA memory chunk.
 */
DECLINLINE(PMMLOOKUPHYPER) mmHyperLookupR0(PVM pVM, RTR0PTR R0Ptr, uint32_t *poff)
{
    AssertCompile(sizeof(RTR0PTR) == sizeof(RTR3PTR));

    /*
     * Translate Ring-0 VM addresses into Ring-3 VM addresses before feeding it to mmHyperLookupR3.
     */
    /** @todo fix this properly; the ring 0 pVM address differs from the R3 one. (#1865) */
    RTR0UINTPTR offVM = (RTR0UINTPTR)R0Ptr - (RTR0UINTPTR)pVM->pVMR0;
    RTR3PTR R3Ptr = offVM < sizeof(*pVM)
                  ? (RTR3PTR)((RTR3UINTPTR)pVM->pVMR3 + offVM)
                  : (RTR3PTR)R0Ptr;

    return mmHyperLookupR3(pVM, R3Ptr, poff);
}


/**
 * Lookup a guest context address.
 *
 * @returns Pointer to the corresponding lookup record.
 * @returns NULL on failure.
 * @param   pVM     The VM handle.
 * @param   GCPtr   The guest context address to lookup.
 * @param   poff    Where to store the offset into the HMA memory chunk.
 */
DECLINLINE(PMMLOOKUPHYPER) mmHyperLookupGC(PVM pVM, RTGCPTR GCPtr, uint32_t *poff)
{
    /** @todo cache last lookup this stuff ain't cheap! */
    unsigned        offGC = (RTGCUINTPTR)GCPtr - (RTGCUINTPTR)pVM->mm.s.pvHyperAreaGC;
    PMMLOOKUPHYPER  pLookup = (PMMLOOKUPHYPER)((char*)CTXSUFF(pVM->mm.s.pHyperHeap) + pVM->mm.s.offLookupHyper);
    for (;;)
    {
        const uint32_t off = offGC - pLookup->off;
        if (off < pLookup->cb)
        {
            switch (pLookup->enmType)
            {
                case MMLOOKUPHYPERTYPE_LOCKED:
                case MMLOOKUPHYPERTYPE_HCPHYS:
                    *poff = off;
                    return pLookup;
                default:
                    break;
            }
            AssertMsgFailed(("enmType=%d\n", pLookup->enmType));
            return NULL;
        }

        /* next */
        if (pLookup->offNext == (int32_t)NIL_OFFSET)
            break;
        pLookup = (PMMLOOKUPHYPER)((char *)pLookup + pLookup->offNext);
    }

    AssertMsgFailed(("GCPtr=%p is not inside the hypervisor memory area!\n", GCPtr));
    return NULL;
}


/**
 * Lookup a current context address.
 *
 * @returns Pointer to the corresponding lookup record.
 * @returns NULL on failure.
 * @param   pVM     The VM handle.
 * @param   pv      The current context address to lookup.
 * @param   poff    Where to store the offset into the HMA memory chunk.
 */
DECLINLINE(PMMLOOKUPHYPER) mmHyperLookupCC(PVM pVM, void *pv, uint32_t *poff)
{
#ifdef IN_GC
    return mmHyperLookupGC(pVM, pv, poff);
#elif defined(IN_RING0)
    return mmHyperLookupR0(pVM, pv, poff);
#else
    return mmHyperLookupR3(pVM, pv, poff);
#endif
}


/**
 * Calculate the host context ring-3 address of an offset into the HMA memory chunk.
 *
 * @returns the host context ring-3 address.
 * @param   pLookup     The HMA lookup record.
 * @param   off         The offset into the HMA memory chunk.
 */
DECLINLINE(RTR3PTR) mmHyperLookupCalcR3(PMMLOOKUPHYPER pLookup, uint32_t off)
{
    switch (pLookup->enmType)
    {
        case MMLOOKUPHYPERTYPE_LOCKED:
            return (RTR3PTR)((RTR3UINTPTR)pLookup->u.Locked.pvHC + off);
        case MMLOOKUPHYPERTYPE_HCPHYS:
            return (RTR3PTR)((RTR3UINTPTR)pLookup->u.HCPhys.pvHC + off);
        default:
            AssertMsgFailed(("enmType=%d\n", pLookup->enmType));
            return NIL_RTR3PTR;
    }
}


/**
 * Calculate the host context ring-0 address of an offset into the HMA memory chunk.
 *
 * @returns the host context ring-0 address.
 * @param   pLookup     The HMA lookup record.
 * @param   off         The offset into the HMA memory chunk.
 */
DECLINLINE(RTR0PTR) mmHyperLookupCalcR0(PMMLOOKUPHYPER pLookup, uint32_t off)
{
    switch (pLookup->enmType)
    {
        case MMLOOKUPHYPERTYPE_LOCKED:
            if (pLookup->u.Locked.pvR0)
                return (RTR0PTR)((RTR0UINTPTR)pLookup->u.Locked.pvR0 + off);
            return (RTR0PTR)((RTR3UINTPTR)pLookup->u.Locked.pvHC + off);
        case MMLOOKUPHYPERTYPE_HCPHYS:
            return (RTR0PTR)((RTR3UINTPTR)pLookup->u.HCPhys.pvHC + off);
        default:
            AssertMsgFailed(("enmType=%d\n", pLookup->enmType));
            return NIL_RTR0PTR;
    }
}


/**
 * Calculate the guest context address of an offset into the HMA memory chunk.
 *
 * @returns the guest context base address.
 * @param   pVM         The the VM handle.
 * @param   pLookup     The HMA lookup record.
 * @param   off         The offset into the HMA memory chunk.
 */
DECLINLINE(RTGCPTR) mmHyperLookupCalcGC(PVM pVM, PMMLOOKUPHYPER pLookup, uint32_t off)
{
    return (RTGCPTR)((RTGCUINTPTR)pVM->mm.s.pvHyperAreaGC + pLookup->off + off);
}


/**
 * Calculate the guest context address of an offset into the HMA memory chunk.
 *
 * @returns the guest context base address.
 * @param   pVM         The the VM handle.
 * @param   pLookup     The HMA lookup record.
 * @param   off         The offset into the HMA memory chunk.
 */
DECLINLINE(void *) mmHyperLookupCalcCC(PVM pVM, PMMLOOKUPHYPER pLookup, uint32_t off)
{
#ifdef IN_GC
    return mmHyperLookupCalcGC(pVM, pLookup, off);
#elif defined(IN_RING0)
    return mmHyperLookupCalcR0(pLookup, off);
#else
    return mmHyperLookupCalcR3(pLookup, off);
#endif
}


/**
 * Converts a ring-0 host context address in the Hypervisor memory region to a ring-3 host context address.
 *
 * @returns ring-3 host context address.
 * @param   pVM         The VM to operate on.
 * @param   R0Ptr       The ring-0 host context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
MMDECL(RTR3PTR) MMHyperR0ToR3(PVM pVM, RTR0PTR R0Ptr)
{
    uint32_t off;
    PMMLOOKUPHYPER pLookup = mmHyperLookupR0(pVM, R0Ptr, &off);
    if (pLookup)
        return mmHyperLookupCalcR3(pLookup, off);
    return NIL_RTR3PTR;
}


/**
 * Converts a ring-0 host context address in the Hypervisor memory region to a guest context address.
 *
 * @returns guest context address.
 * @param   pVM         The VM to operate on.
 * @param   R0Ptr       The ring-0 host context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
MMDECL(RTGCPTR) MMHyperR0ToGC(PVM pVM, RTR0PTR R0Ptr)
{
    uint32_t off;
    PMMLOOKUPHYPER pLookup = mmHyperLookupR0(pVM, R0Ptr, &off);
    if (pLookup)
        return mmHyperLookupCalcGC(pVM, pLookup, off);
    return NIL_RTGCPTR;
}


#ifndef IN_RING0
/**
 * Converts a ring-0 host context address in the Hypervisor memory region to a current context address.
 *
 * @returns current context address.
 * @param   pVM         The VM to operate on.
 * @param   R0Ptr       The ring-0 host context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
MMDECL(void *) MMHyperR0ToCC(PVM pVM, RTR0PTR R0Ptr)
{
    uint32_t off;
    PMMLOOKUPHYPER pLookup = mmHyperLookupR0(pVM, R0Ptr, &off);
    if (pLookup)
        return mmHyperLookupCalcCC(pVM, pLookup, off);
    return NULL;
}
#endif


/**
 * Converts a ring-3 host context address in the Hypervisor memory region to a ring-0 host context address.
 *
 * @returns ring-0 host context address.
 * @param   pVM         The VM to operate on.
 * @param   R3Ptr       The ring-3 host context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
MMDECL(RTR0PTR) MMHyperR3ToR0(PVM pVM, RTR3PTR R3Ptr)
{
    uint32_t off;
    PMMLOOKUPHYPER pLookup = mmHyperLookupR3(pVM, R3Ptr, &off);
    if (pLookup)
        return mmHyperLookupCalcR0(pLookup, off);
    AssertMsgFailed(("R3Ptr=%p is not inside the hypervisor memory area!\n", R3Ptr));
    return NIL_RTR0PTR;
}


/**
 * Converts a ring-3 host context address in the Hypervisor memory region to a guest context address.
 *
 * @returns guest context address.
 * @param   pVM         The VM to operate on.
 * @param   R3Ptr       The ring-3 host context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
MMDECL(RTGCPTR) MMHyperR3ToGC(PVM pVM, RTR3PTR R3Ptr)
{
    uint32_t off;
    PMMLOOKUPHYPER pLookup = mmHyperLookupR3(pVM, R3Ptr, &off);
    if (pLookup)
        return mmHyperLookupCalcGC(pVM, pLookup, off);
    AssertMsgFailed(("R3Ptr=%p is not inside the hypervisor memory area!\n", R3Ptr));
    return NIL_RTGCPTR;
}


/**
 * Converts a ring-3 host context address in the Hypervisor memory region to a current context address.
 *
 * @returns current context address.
 * @param   pVM         The VM to operate on.
 * @param   R3Ptr       The ring-3 host context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
#ifndef IN_RING3
MMDECL(void *) MMHyperR3ToCC(PVM pVM, RTR3PTR R3Ptr)
{
    uint32_t off;
    PMMLOOKUPHYPER pLookup = mmHyperLookupR3(pVM, R3Ptr, &off);
    if (pLookup)
        return mmHyperLookupCalcCC(pVM, pLookup, off);
    return NULL;
}
#endif


/**
 * Converts a guest context address in the Hypervisor memory region to a ring-3 context address.
 *
 * @returns ring-3 host context address.
 * @param   pVM         The VM to operate on.
 * @param   GCPtr       The guest context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
MMDECL(RTR3PTR) MMHyperGCToR3(PVM pVM, RTGCPTR GCPtr)
{
    uint32_t off;
    PMMLOOKUPHYPER pLookup = mmHyperLookupGC(pVM, GCPtr, &off);
    if (pLookup)
        return mmHyperLookupCalcR3(pLookup, off);
    return NIL_RTR3PTR;
}


/**
 * Converts a guest context address in the Hypervisor memory region to a ring-0 host context address.
 *
 * @returns ring-0 host context address.
 * @param   pVM         The VM to operate on.
 * @param   GCPtr       The guest context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
MMDECL(RTR0PTR) MMHyperGCToR0(PVM pVM, RTGCPTR GCPtr)
{
    uint32_t off;
    PMMLOOKUPHYPER pLookup = mmHyperLookupGC(pVM, GCPtr, &off);
    if (pLookup)
        return mmHyperLookupCalcR0(pLookup, off);
    return NIL_RTR0PTR;
}


/**
 * Converts a guest context address in the Hypervisor memory region to a current context address.
 *
 * @returns current context address.
 * @param   pVM         The VM to operate on.
 * @param   GCPtr       The guest host context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
#ifndef IN_GC
MMDECL(void *) MMHyperGCToCC(PVM pVM, RTGCPTR GCPtr)
{
    uint32_t off;
    PMMLOOKUPHYPER pLookup = mmHyperLookupGC(pVM, GCPtr, &off);
    if (pLookup)
        return mmHyperLookupCalcCC(pVM, pLookup, off);
    return NULL;
}
#endif



/**
 * Converts a current context address in the Hypervisor memory region to a ring-3 host context address.
 *
 * @returns ring-3 host context address.
 * @param   pVM         The VM to operate on.
 * @param   pv          The current context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
#ifndef IN_RING3
MMDECL(RTR3PTR) MMHyperCCToR3(PVM pVM, void *pv)
{
    uint32_t off;
    PMMLOOKUPHYPER pLookup = mmHyperLookupCC(pVM, pv, &off);
    if (pLookup)
        return mmHyperLookupCalcR3(pLookup, off);
    return NIL_RTR3PTR;
}
#endif

/**
 * Converts a current context address in the Hypervisor memory region to a ring-0 host context address.
 *
 * @returns ring-0 host context address.
 * @param   pVM         The VM to operate on.
 * @param   pv          The current context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
#ifndef IN_RING0
MMDECL(RTR0PTR) MMHyperCCToR0(PVM pVM, void *pv)
{
    uint32_t off;
    PMMLOOKUPHYPER pLookup = mmHyperLookupCC(pVM, pv, &off);
    if (pLookup)
        return mmHyperLookupCalcR0(pLookup, off);
    return NIL_RTR0PTR;
}
#endif


/**
 * Converts a current context address in the Hypervisor memory region to a guest context address.
 *
 * @returns guest context address.
 * @param   pVM         The VM to operate on.
 * @param   pv          The current context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
#ifndef IN_GC
MMDECL(RTGCPTR) MMHyperCCToGC(PVM pVM, void *pv)
{
    uint32_t off;
    PMMLOOKUPHYPER pLookup = mmHyperLookupCC(pVM, pv, &off);
    if (pLookup)
        return mmHyperLookupCalcGC(pVM, pLookup, off);
    return NIL_RTGCPTR;
}
#endif



/**
 * Converts a HC address in the Hypervisor memory region to a GC address.
 * The memory must have been allocated with MMGCHyperAlloc() or MMR3HyperAlloc().
 *
 * @returns GC address.
 * @param   pVM         The VM to operate on.
 * @param   HCPtr       The host context address.
 *                      You'll be damed if this is not in the hypervisor region! :-)
 * @deprecated
 */
MMDECL(RTGCPTR) MMHyperHC2GC(PVM pVM, RTHCPTR HCPtr)
{
    PMMLOOKUPHYPER  pLookup = (PMMLOOKUPHYPER)((char*)CTXSUFF(pVM->mm.s.pHyperHeap) + pVM->mm.s.offLookupHyper);
    for (;;)
    {
        switch (pLookup->enmType)
        {
            case MMLOOKUPHYPERTYPE_LOCKED:
            {
                unsigned    off = (RTHCUINTPTR)HCPtr - (RTHCUINTPTR)pLookup->u.Locked.pvHC;
                if (off < pLookup->cb)
                    return (RTGCPTR)((RTGCUINTPTR)pVM->mm.s.pvHyperAreaGC + pLookup->off + off);
                break;
            }

            case MMLOOKUPHYPERTYPE_HCPHYS:
            {
                unsigned    off = (RTHCUINTPTR)HCPtr - (RTHCUINTPTR)pLookup->u.HCPhys.pvHC;
                if (off < pLookup->cb)
                    return (RTGCPTR)((RTGCUINTPTR)pVM->mm.s.pvHyperAreaGC + pLookup->off + off);
                break;
            }

            case MMLOOKUPHYPERTYPE_GCPHYS:  /* (for now we'll not allow these kind of conversions) */
            case MMLOOKUPHYPERTYPE_MMIO2:
            case MMLOOKUPHYPERTYPE_DYNAMIC:
                break;

            default:
                AssertMsgFailed(("enmType=%d\n", pLookup->enmType));
                break;
        }

        /* next */
        if ((unsigned)pLookup->offNext == NIL_OFFSET)
            break;
        pLookup = (PMMLOOKUPHYPER)((char *)pLookup + pLookup->offNext);
    }

    AssertMsgFailed(("HCPtr=%p is not inside the hypervisor memory area!\n", HCPtr));
    return (RTGCPTR)0;
}


/**
 * Converts a GC address in the Hypervisor memory region to a HC address.
 * The memory must have been allocated with MMHyperAlloc().
 *
 * @returns HC address.
 * @param   pVM         The VM to operate on.
 * @param   GCPtr       The guest context address.
 *                      You'll be damed if this is not in the hypervisor region! :-)
 * @deprecated
 */
MMDECL(RTHCPTR) MMHyperGC2HC(PVM pVM, RTGCPTR GCPtr)
{
    unsigned        offGC = (RTGCUINTPTR)GCPtr - (RTGCUINTPTR)pVM->mm.s.pvHyperAreaGC;
    PMMLOOKUPHYPER  pLookup = (PMMLOOKUPHYPER)((char*)CTXSUFF(pVM->mm.s.pHyperHeap) + pVM->mm.s.offLookupHyper);
    for (;;)
    {
        unsigned off = offGC - pLookup->off;
        if (off < pLookup->cb)
        {
            switch (pLookup->enmType)
            {
                case MMLOOKUPHYPERTYPE_LOCKED:
                    return (RTHCPTR)((RTHCUINTPTR)pLookup->u.Locked.pvHC + off);
                case MMLOOKUPHYPERTYPE_HCPHYS:
                    return (RTHCPTR)((RTHCUINTPTR)pLookup->u.HCPhys.pvHC + off);
                default:
                    break;
            }
            AssertMsgFailed(("enmType=%d\n", pLookup->enmType));
            return (RTHCPTR)0;
        }

        /* next */
        if ((unsigned)pLookup->offNext == NIL_OFFSET)
            break;
        pLookup = (PMMLOOKUPHYPER)((char *)pLookup + pLookup->offNext);
    }

    AssertMsgFailed(("GCPtr=%p is not inside the hypervisor memory area!\n", GCPtr));
    return (RTHCPTR)0;
}


#ifdef IN_GC
/**
 * Converts a current context address in the Hypervisor memory region to a HC address.
 * The memory must have been allocated with MMGCHyperAlloc() or MMR3HyperAlloc().
 *
 * @returns HC address.
 * @param   pVM         The VM to operate on.
 * @param   Ptr         The current context address.
 * @deprecated
 */
MMDECL(RTHCPTR) MMHyper2HC(PVM pVM, uintptr_t Ptr)
{
    return MMHyperGC2HC(pVM, (RTGCPTR)Ptr);
}

#else /* !IN_GC */

/**
 * Converts a current context address in the Hypervisor memory region to a GC address.
 * The memory must have been allocated with MMHyperAlloc().
 *
 * @returns HC address.
 * @param   pVM         The VM to operate on.
 * @param   Ptr         The current context address.
 * @thread  The Emulation Thread.
 * @deprecated
 */
MMDECL(RTGCPTR) MMHyper2GC(PVM pVM, uintptr_t Ptr)
{
    return MMHyperHC2GC(pVM, (RTHCPTR)Ptr);
}

#endif /* !IN_GC */
