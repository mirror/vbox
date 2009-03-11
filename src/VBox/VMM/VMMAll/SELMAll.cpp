/* $Id$ */
/** @file
 * SELM All contexts.
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
#define LOG_GROUP LOG_GROUP_SELM
#include <VBox/selm.h>
#include <VBox/stam.h>
#include <VBox/mm.h>
#include <VBox/pgm.h>
#include "SELMInternal.h"
#include <VBox/vm.h>
#include <VBox/x86.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <iprt/assert.h>
#include <VBox/log.h>



#ifndef IN_RING0

/**
 * Converts a GC selector based address to a flat address.
 *
 * No limit checks are done. Use the SELMToFlat*() or SELMValidate*() functions
 * for that.
 *
 * @returns Flat address.
 * @param   pVM     VM Handle.
 * @param   Sel     Selector part.
 * @param   Addr    Address part.
 * @remarks Don't use when in long mode.
 */
VMMDECL(RTGCPTR) SELMToFlatBySel(PVM pVM, RTSEL Sel, RTGCPTR Addr)
{
    Assert(!CPUMIsGuestInLongMode(pVM));    /* DON'T USE! */

    /** @todo check the limit. */
    X86DESC    Desc;
    if (!(Sel & X86_SEL_LDT))
        Desc = pVM->selm.s.CTX_SUFF(paGdt)[Sel >> X86_SEL_SHIFT];
    else
    {
        /** @todo handle LDT pages not present! */
        PX86DESC    paLDT = (PX86DESC)((char *)pVM->selm.s.CTX_SUFF(pvLdt) + pVM->selm.s.offLdtHyper);
        Desc = paLDT[Sel >> X86_SEL_SHIFT];
    }

    return (RTGCPTR)((RTGCUINTPTR)Addr + X86DESC_BASE(Desc));
}
#endif /* !IN_RING0 */


/**
 * Converts a GC selector based address to a flat address.
 *
 * No limit checks are done. Use the SELMToFlat*() or SELMValidate*() functions
 * for that.
 *
 * @returns Flat address.
 * @param   pVM         VM Handle.
 * @param   SelReg      Selector register
 * @param   pCtxCore    CPU context
 * @param   Addr        Address part.
 */
VMMDECL(RTGCPTR) SELMToFlat(PVM pVM, DIS_SELREG SelReg, PCPUMCTXCORE pCtxCore, RTGCPTR Addr)
{
    PCPUMSELREGHID pHiddenSel;
    RTSEL          Sel;
    int            rc;

    rc = DISFetchRegSegEx(pCtxCore, SelReg, &Sel, &pHiddenSel); AssertRC(rc);

    /*
     * Deal with real & v86 mode first.
     */
    if (    CPUMIsGuestInRealMode(pVM)
        ||  pCtxCore->eflags.Bits.u1VM)
    {
        RTGCUINTPTR uFlat = (RTGCUINTPTR)Addr & 0xffff;
        if (CPUMAreHiddenSelRegsValid(pVM))
            uFlat += pHiddenSel->u64Base;
        else
            uFlat += ((RTGCUINTPTR)Sel << 4);
        return (RTGCPTR)uFlat;
    }

#ifdef IN_RING0
    Assert(CPUMAreHiddenSelRegsValid(pVM));
#else
    /** @todo when we're in 16 bits mode, we should cut off the address as well.. */
    if (!CPUMAreHiddenSelRegsValid(pVM))
        return SELMToFlatBySel(pVM, Sel, Addr);
#endif

    /* 64 bits mode: CS, DS, ES and SS are treated as if each segment base is 0 (Intel® 64 and IA-32 Architectures Software Developer's Manual: 3.4.2.1). */
    if (    CPUMIsGuestInLongMode(pVM)
        &&  pCtxCore->csHid.Attr.n.u1Long)
    {
        switch (SelReg)
        {
            case DIS_SELREG_FS:
            case DIS_SELREG_GS:
                return (RTGCPTR)(pHiddenSel->u64Base + Addr);

            default:
                return Addr;    /* base 0 */
        }
    }

    /* AMD64 manual: compatibility mode ignores the high 32 bits when calculating an effective address. */
    Assert(pHiddenSel->u64Base <= 0xffffffff);
    return ((pHiddenSel->u64Base + (RTGCUINTPTR)Addr) & 0xffffffff);
}


/**
 * Converts a GC selector based address to a flat address.
 *
 * Some basic checking is done, but not all kinds yet.
 *
 * @returns VBox status
 * @param   pVM         VM Handle.
 * @param   SelReg      Selector register
 * @param   pCtxCore    CPU context
 * @param   Addr        Address part.
 * @param   fFlags      SELMTOFLAT_FLAGS_*
 *                      GDT entires are valid.
 * @param   ppvGC       Where to store the GC flat address.
 */
VMMDECL(int) SELMToFlatEx(PVM pVM, DIS_SELREG SelReg, PCCPUMCTXCORE pCtxCore, RTGCPTR Addr, unsigned fFlags, PRTGCPTR ppvGC)
{
    /*
     * Fetch the selector first.
     */
    PCPUMSELREGHID pHiddenSel;
    RTSEL          Sel;
    int rc = DISFetchRegSegEx(pCtxCore, SelReg, &Sel, &pHiddenSel);
    AssertRC(rc);

    /*
     * Deal with real & v86 mode first.
     */
    if (    CPUMIsGuestInRealMode(pVM)
        ||  pCtxCore->eflags.Bits.u1VM)
    {
        RTGCUINTPTR uFlat = (RTGCUINTPTR)Addr & 0xffff;
        if (ppvGC)
        {
            if (    pHiddenSel
                &&  CPUMAreHiddenSelRegsValid(pVM))
                *ppvGC = (RTGCPTR)(pHiddenSel->u64Base + uFlat);
            else
                *ppvGC = (RTGCPTR)(((RTGCUINTPTR)Sel << 4) + uFlat);
        }
        return VINF_SUCCESS;
    }


    uint32_t    u32Limit;
    RTGCPTR     pvFlat;
    uint32_t    u1Present, u1DescType, u1Granularity, u4Type;

    /** @todo when we're in 16 bits mode, we should cut off the address as well.. */
#ifndef IN_RC
    if (    pHiddenSel
        &&  CPUMAreHiddenSelRegsValid(pVM))
    {
        bool fCheckLimit = true;

        u1Present     = pHiddenSel->Attr.n.u1Present;
        u1Granularity = pHiddenSel->Attr.n.u1Granularity;
        u1DescType    = pHiddenSel->Attr.n.u1DescType;
        u4Type        = pHiddenSel->Attr.n.u4Type;
        u32Limit      = pHiddenSel->u32Limit;

        /* 64 bits mode: CS, DS, ES and SS are treated as if each segment base is 0 (Intel® 64 and IA-32 Architectures Software Developer's Manual: 3.4.2.1). */
        if (    CPUMIsGuestInLongMode(pVM)
            &&  pCtxCore->csHid.Attr.n.u1Long)
        {
            fCheckLimit = false;
            switch (SelReg)
            {
                case DIS_SELREG_FS:
                case DIS_SELREG_GS:
                    pvFlat = (pHiddenSel->u64Base + Addr);
                    break;

                default:
                    pvFlat = Addr;
                    break;
            }
        }
        else
        {
            /* AMD64 manual: compatibility mode ignores the high 32 bits when calculating an effective address. */
            Assert(pHiddenSel->u64Base <= 0xffffffff);
            pvFlat = (RTGCPTR)((pHiddenSel->u64Base + (RTGCUINTPTR)Addr) & 0xffffffff);
        }

        /*
        * Check if present.
        */
        if (u1Present)
        {
            /*
            * Type check.
            */
            switch (u4Type)
            {

                /** Read only selector type. */
                case X86_SEL_TYPE_RO:
                case X86_SEL_TYPE_RO_ACC:
                case X86_SEL_TYPE_RW:
                case X86_SEL_TYPE_RW_ACC:
                case X86_SEL_TYPE_EO:
                case X86_SEL_TYPE_EO_ACC:
                case X86_SEL_TYPE_ER:
                case X86_SEL_TYPE_ER_ACC:
                    if (!(fFlags & SELMTOFLAT_FLAGS_NO_PL))
                    {
                        /** @todo fix this mess */
                    }
                    /* check limit. */
                    if (fCheckLimit && (RTGCUINTPTR)Addr > u32Limit)
                        return VERR_OUT_OF_SELECTOR_BOUNDS;
                    /* ok */
                    if (ppvGC)
                        *ppvGC = pvFlat;
                    return VINF_SUCCESS;

                case X86_SEL_TYPE_EO_CONF:
                case X86_SEL_TYPE_EO_CONF_ACC:
                case X86_SEL_TYPE_ER_CONF:
                case X86_SEL_TYPE_ER_CONF_ACC:
                    if (!(fFlags & SELMTOFLAT_FLAGS_NO_PL))
                    {
                        /** @todo fix this mess */
                    }
                    /* check limit. */
                    if (fCheckLimit && (RTGCUINTPTR)Addr > u32Limit)
                        return VERR_OUT_OF_SELECTOR_BOUNDS;
                    /* ok */
                    if (ppvGC)
                        *ppvGC = pvFlat;
                    return VINF_SUCCESS;

                case X86_SEL_TYPE_RO_DOWN:
                case X86_SEL_TYPE_RO_DOWN_ACC:
                case X86_SEL_TYPE_RW_DOWN:
                case X86_SEL_TYPE_RW_DOWN_ACC:
                    if (!(fFlags & SELMTOFLAT_FLAGS_NO_PL))
                    {
                        /** @todo fix this mess */
                    }
                    /* check limit. */
                    if (fCheckLimit)
                    {
                        if (!u1Granularity && (RTGCUINTPTR)Addr > (RTGCUINTPTR)0xffff)
                            return VERR_OUT_OF_SELECTOR_BOUNDS;
                        if ((RTGCUINTPTR)Addr <= u32Limit)
                            return VERR_OUT_OF_SELECTOR_BOUNDS;
                    }
                    /* ok */
                    if (ppvGC)
                        *ppvGC = pvFlat;
                    return VINF_SUCCESS;

                default:
                    return VERR_INVALID_SELECTOR;

            }
        }
    }
# ifndef IN_RING0
    else
# endif
#endif /* !IN_RC */
#ifndef IN_RING0
    {
        X86DESC Desc;

        if (!(Sel & X86_SEL_LDT))
        {
            if (   !(fFlags & SELMTOFLAT_FLAGS_HYPER)
                && (unsigned)(Sel & X86_SEL_MASK) >= pVM->selm.s.GuestGdtr.cbGdt)
                return VERR_INVALID_SELECTOR;
            Desc = pVM->selm.s.CTX_SUFF(paGdt)[Sel >> X86_SEL_SHIFT];
        }
        else
        {
            if ((unsigned)(Sel & X86_SEL_MASK) >= pVM->selm.s.cbLdtLimit)
                return VERR_INVALID_SELECTOR;

            /** @todo handle LDT page(s) not present! */
            PX86DESC    paLDT = (PX86DESC)((char *)pVM->selm.s.CTX_SUFF(pvLdt) + pVM->selm.s.offLdtHyper);
            Desc = paLDT[Sel >> X86_SEL_SHIFT];
        }

        /* calc limit. */
        u32Limit = X86DESC_LIMIT(Desc);
        if (Desc.Gen.u1Granularity)
            u32Limit = (u32Limit << PAGE_SHIFT) | PAGE_OFFSET_MASK;

        /* calc address assuming straight stuff. */
        pvFlat = (RTGCPTR)((RTGCUINTPTR)Addr + X86DESC_BASE(Desc));

        u1Present     = Desc.Gen.u1Present;
        u1Granularity = Desc.Gen.u1Granularity;
        u1DescType    = Desc.Gen.u1DescType;
        u4Type        = Desc.Gen.u4Type;

        /*
        * Check if present.
        */
        if (u1Present)
        {
            /*
            * Type check.
            */
# define BOTH(a, b) ((a << 16) | b)
            switch (BOTH(u1DescType, u4Type))
            {

                /** Read only selector type. */
                case BOTH(1,X86_SEL_TYPE_RO):
                case BOTH(1,X86_SEL_TYPE_RO_ACC):
                case BOTH(1,X86_SEL_TYPE_RW):
                case BOTH(1,X86_SEL_TYPE_RW_ACC):
                case BOTH(1,X86_SEL_TYPE_EO):
                case BOTH(1,X86_SEL_TYPE_EO_ACC):
                case BOTH(1,X86_SEL_TYPE_ER):
                case BOTH(1,X86_SEL_TYPE_ER_ACC):
                    if (!(fFlags & SELMTOFLAT_FLAGS_NO_PL))
                    {
                        /** @todo fix this mess */
                    }
                    /* check limit. */
                    if ((RTGCUINTPTR)Addr > u32Limit)
                        return VERR_OUT_OF_SELECTOR_BOUNDS;
                    /* ok */
                    if (ppvGC)
                        *ppvGC = pvFlat;
                    return VINF_SUCCESS;

                case BOTH(1,X86_SEL_TYPE_EO_CONF):
                case BOTH(1,X86_SEL_TYPE_EO_CONF_ACC):
                case BOTH(1,X86_SEL_TYPE_ER_CONF):
                case BOTH(1,X86_SEL_TYPE_ER_CONF_ACC):
                    if (!(fFlags & SELMTOFLAT_FLAGS_NO_PL))
                    {
                        /** @todo fix this mess */
                    }
                    /* check limit. */
                    if ((RTGCUINTPTR)Addr > u32Limit)
                        return VERR_OUT_OF_SELECTOR_BOUNDS;
                    /* ok */
                    if (ppvGC)
                        *ppvGC = pvFlat;
                    return VINF_SUCCESS;

                case BOTH(1,X86_SEL_TYPE_RO_DOWN):
                case BOTH(1,X86_SEL_TYPE_RO_DOWN_ACC):
                case BOTH(1,X86_SEL_TYPE_RW_DOWN):
                case BOTH(1,X86_SEL_TYPE_RW_DOWN_ACC):
                    if (!(fFlags & SELMTOFLAT_FLAGS_NO_PL))
                    {
                        /** @todo fix this mess */
                    }
                    /* check limit. */
                    if (!u1Granularity && (RTGCUINTPTR)Addr > (RTGCUINTPTR)0xffff)
                        return VERR_OUT_OF_SELECTOR_BOUNDS;
                    if ((RTGCUINTPTR)Addr <= u32Limit)
                        return VERR_OUT_OF_SELECTOR_BOUNDS;

                    /* ok */
                    if (ppvGC)
                        *ppvGC = pvFlat;
                    return VINF_SUCCESS;

                case BOTH(0,X86_SEL_TYPE_SYS_286_TSS_AVAIL):
                case BOTH(0,X86_SEL_TYPE_SYS_LDT):
                case BOTH(0,X86_SEL_TYPE_SYS_286_TSS_BUSY):
                case BOTH(0,X86_SEL_TYPE_SYS_286_CALL_GATE):
                case BOTH(0,X86_SEL_TYPE_SYS_TASK_GATE):
                case BOTH(0,X86_SEL_TYPE_SYS_286_INT_GATE):
                case BOTH(0,X86_SEL_TYPE_SYS_286_TRAP_GATE):
                case BOTH(0,X86_SEL_TYPE_SYS_386_TSS_AVAIL):
                case BOTH(0,X86_SEL_TYPE_SYS_386_TSS_BUSY):
                case BOTH(0,X86_SEL_TYPE_SYS_386_CALL_GATE):
                case BOTH(0,X86_SEL_TYPE_SYS_386_INT_GATE):
                case BOTH(0,X86_SEL_TYPE_SYS_386_TRAP_GATE):
                    if (!(fFlags & SELMTOFLAT_FLAGS_NO_PL))
                    {
                        /** @todo fix this mess */
                    }
                    /* check limit. */
                    if ((RTGCUINTPTR)Addr > u32Limit)
                        return VERR_OUT_OF_SELECTOR_BOUNDS;
                    /* ok */
                    if (ppvGC)
                        *ppvGC = pvFlat;
                    return VINF_SUCCESS;

                default:
                    return VERR_INVALID_SELECTOR;

            }
# undef BOTH
        }
    }
#endif /* !IN_RING0 */
    return VERR_SELECTOR_NOT_PRESENT;
}


#ifndef IN_RING0
/**
 * Converts a GC selector based address to a flat address.
 *
 * Some basic checking is done, but not all kinds yet.
 *
 * @returns VBox status
 * @param   pVM         VM Handle.
 * @param   eflags      Current eflags
 * @param   Sel         Selector part.
 * @param   Addr        Address part.
 * @param   pHiddenSel  Hidden selector register (can be NULL)
 * @param   fFlags      SELMTOFLAT_FLAGS_*
 *                      GDT entires are valid.
 * @param   ppvGC       Where to store the GC flat address.
 * @param   pcb         Where to store the bytes from *ppvGC which can be accessed according to
 *                      the selector. NULL is allowed.
 * @remarks Don't use when in long mode.
 */
VMMDECL(int) SELMToFlatBySelEx(PVM pVM, X86EFLAGS eflags, RTSEL Sel, RTGCPTR Addr, CPUMSELREGHID *pHiddenSel, unsigned fFlags, PRTGCPTR ppvGC, uint32_t *pcb)
{
    Assert(!CPUMIsGuestInLongMode(pVM));    /* DON'T USE! */

    /*
     * Deal with real & v86 mode first.
     */
    if (    CPUMIsGuestInRealMode(pVM)
        ||  eflags.Bits.u1VM)
    {
        RTGCUINTPTR uFlat = (RTGCUINTPTR)Addr & 0xffff;
        if (ppvGC)
        {
            if (    pHiddenSel
                &&  CPUMAreHiddenSelRegsValid(pVM))
                *ppvGC = (RTGCPTR)(pHiddenSel->u64Base + uFlat);
            else
                *ppvGC = (RTGCPTR)(((RTGCUINTPTR)Sel << 4) + uFlat);
        }
        if (pcb)
            *pcb = 0x10000 - uFlat;
        return VINF_SUCCESS;
    }


    uint32_t    u32Limit;
    RTGCPTR     pvFlat;
    uint32_t    u1Present, u1DescType, u1Granularity, u4Type;

    /** @todo when we're in 16 bits mode, we should cut off the address as well.. */
    if (    pHiddenSel
        &&  CPUMAreHiddenSelRegsValid(pVM))
    {
        u1Present     = pHiddenSel->Attr.n.u1Present;
        u1Granularity = pHiddenSel->Attr.n.u1Granularity;
        u1DescType    = pHiddenSel->Attr.n.u1DescType;
        u4Type        = pHiddenSel->Attr.n.u4Type;

        u32Limit      = pHiddenSel->u32Limit;
        pvFlat        = (RTGCPTR)(pHiddenSel->u64Base + (RTGCUINTPTR)Addr);

        if (   !CPUMIsGuestInLongMode(pVM)
            || !pHiddenSel->Attr.n.u1Long)
        {
            /* AMD64 manual: compatibility mode ignores the high 32 bits when calculating an effective address. */
            pvFlat &= 0xffffffff;
        }
    }
    else
    {
        X86DESC Desc;

        if (!(Sel & X86_SEL_LDT))
        {
            if (   !(fFlags & SELMTOFLAT_FLAGS_HYPER)
                && (unsigned)(Sel & X86_SEL_MASK) >= pVM->selm.s.GuestGdtr.cbGdt)
                return VERR_INVALID_SELECTOR;
            Desc = pVM->selm.s.CTX_SUFF(paGdt)[Sel >> X86_SEL_SHIFT];
        }
        else
        {
            if ((unsigned)(Sel & X86_SEL_MASK) >= pVM->selm.s.cbLdtLimit)
                return VERR_INVALID_SELECTOR;

            /** @todo handle LDT page(s) not present! */
            PX86DESC    paLDT = (PX86DESC)((char *)pVM->selm.s.CTX_SUFF(pvLdt) + pVM->selm.s.offLdtHyper);
            Desc = paLDT[Sel >> X86_SEL_SHIFT];
        }

        /* calc limit. */
        u32Limit = X86DESC_LIMIT(Desc);
        if (Desc.Gen.u1Granularity)
            u32Limit = (u32Limit << PAGE_SHIFT) | PAGE_OFFSET_MASK;

        /* calc address assuming straight stuff. */
        pvFlat = (RTGCPTR)((RTGCUINTPTR)Addr + X86DESC_BASE(Desc));

        u1Present     = Desc.Gen.u1Present;
        u1Granularity = Desc.Gen.u1Granularity;
        u1DescType    = Desc.Gen.u1DescType;
        u4Type        = Desc.Gen.u4Type;
    }

    /*
     * Check if present.
     */
    if (u1Present)
    {
        /*
         * Type check.
         */
#define BOTH(a, b) ((a << 16) | b)
        switch (BOTH(u1DescType, u4Type))
        {

            /** Read only selector type. */
            case BOTH(1,X86_SEL_TYPE_RO):
            case BOTH(1,X86_SEL_TYPE_RO_ACC):
            case BOTH(1,X86_SEL_TYPE_RW):
            case BOTH(1,X86_SEL_TYPE_RW_ACC):
            case BOTH(1,X86_SEL_TYPE_EO):
            case BOTH(1,X86_SEL_TYPE_EO_ACC):
            case BOTH(1,X86_SEL_TYPE_ER):
            case BOTH(1,X86_SEL_TYPE_ER_ACC):
                if (!(fFlags & SELMTOFLAT_FLAGS_NO_PL))
                {
                    /** @todo fix this mess */
                }
                /* check limit. */
                if ((RTGCUINTPTR)Addr > u32Limit)
                    return VERR_OUT_OF_SELECTOR_BOUNDS;
                /* ok */
                if (ppvGC)
                    *ppvGC = pvFlat;
                if (pcb)
                    *pcb = u32Limit - (uint32_t)Addr + 1;
                return VINF_SUCCESS;

            case BOTH(1,X86_SEL_TYPE_EO_CONF):
            case BOTH(1,X86_SEL_TYPE_EO_CONF_ACC):
            case BOTH(1,X86_SEL_TYPE_ER_CONF):
            case BOTH(1,X86_SEL_TYPE_ER_CONF_ACC):
                if (!(fFlags & SELMTOFLAT_FLAGS_NO_PL))
                {
                    /** @todo fix this mess */
                }
                /* check limit. */
                if ((RTGCUINTPTR)Addr > u32Limit)
                    return VERR_OUT_OF_SELECTOR_BOUNDS;
                /* ok */
                if (ppvGC)
                    *ppvGC = pvFlat;
                if (pcb)
                    *pcb = u32Limit - (uint32_t)Addr + 1;
                return VINF_SUCCESS;

            case BOTH(1,X86_SEL_TYPE_RO_DOWN):
            case BOTH(1,X86_SEL_TYPE_RO_DOWN_ACC):
            case BOTH(1,X86_SEL_TYPE_RW_DOWN):
            case BOTH(1,X86_SEL_TYPE_RW_DOWN_ACC):
                if (!(fFlags & SELMTOFLAT_FLAGS_NO_PL))
                {
                    /** @todo fix this mess */
                }
                /* check limit. */
                if (!u1Granularity && (RTGCUINTPTR)Addr > (RTGCUINTPTR)0xffff)
                    return VERR_OUT_OF_SELECTOR_BOUNDS;
                if ((RTGCUINTPTR)Addr <= u32Limit)
                    return VERR_OUT_OF_SELECTOR_BOUNDS;

                /* ok */
                if (ppvGC)
                    *ppvGC = pvFlat;
                if (pcb)
                    *pcb = (RTGCUINTPTR)(u1Granularity ? 0xffffffff : 0xffff) - (RTGCUINTPTR)Addr + 1;
                return VINF_SUCCESS;

            case BOTH(0,X86_SEL_TYPE_SYS_286_TSS_AVAIL):
            case BOTH(0,X86_SEL_TYPE_SYS_LDT):
            case BOTH(0,X86_SEL_TYPE_SYS_286_TSS_BUSY):
            case BOTH(0,X86_SEL_TYPE_SYS_286_CALL_GATE):
            case BOTH(0,X86_SEL_TYPE_SYS_TASK_GATE):
            case BOTH(0,X86_SEL_TYPE_SYS_286_INT_GATE):
            case BOTH(0,X86_SEL_TYPE_SYS_286_TRAP_GATE):
            case BOTH(0,X86_SEL_TYPE_SYS_386_TSS_AVAIL):
            case BOTH(0,X86_SEL_TYPE_SYS_386_TSS_BUSY):
            case BOTH(0,X86_SEL_TYPE_SYS_386_CALL_GATE):
            case BOTH(0,X86_SEL_TYPE_SYS_386_INT_GATE):
            case BOTH(0,X86_SEL_TYPE_SYS_386_TRAP_GATE):
                if (!(fFlags & SELMTOFLAT_FLAGS_NO_PL))
                {
                    /** @todo fix this mess */
                }
                /* check limit. */
                if ((RTGCUINTPTR)Addr > u32Limit)
                    return VERR_OUT_OF_SELECTOR_BOUNDS;
                /* ok */
                if (ppvGC)
                    *ppvGC = pvFlat;
                if (pcb)
                    *pcb = 0xffffffff - (RTGCUINTPTR)pvFlat + 1; /* Depends on the type.. fixme if we care. */
                return VINF_SUCCESS;

            default:
                return VERR_INVALID_SELECTOR;

        }
#undef BOTH
    }
    return VERR_SELECTOR_NOT_PRESENT;
}
#endif /* !IN_RING0 */


/**
 * Validates and converts a GC selector based code address to a flat
 * address when in real or v8086 mode.
 *
 * @returns VINF_SUCCESS.
 * @param   pVM     VM Handle.
 * @param   SelCS   Selector part.
 * @param   pHidCS  The hidden CS register part. Optional.
 * @param   Addr    Address part.
 * @param   ppvFlat Where to store the flat address.
 */
DECLINLINE(int) selmValidateAndConvertCSAddrRealMode(PVM pVM, RTSEL SelCS, PCPUMSELREGHID pHidCS, RTGCPTR Addr, PRTGCPTR ppvFlat)
{
    RTGCUINTPTR uFlat = (RTGCUINTPTR)Addr & 0xffff;
    if (!pHidCS || !CPUMAreHiddenSelRegsValid(pVM))
        uFlat += ((RTGCUINTPTR)SelCS << 4);
    else
        uFlat += pHidCS->u64Base;
    *ppvFlat = (RTGCPTR)uFlat;
    return VINF_SUCCESS;
}


#ifndef IN_RING0
/**
 * Validates and converts a GC selector based code address to a flat
 * address when in protected/long mode using the standard algorithm.
 *
 * @returns VBox status code.
 * @param   pVM     VM Handle.
 * @param   SelCPL  Current privilege level. Get this from SS - CS might be conforming!
 *                  A full selector can be passed, we'll only use the RPL part.
 * @param   SelCS   Selector part.
 * @param   Addr    Address part.
 * @param   ppvFlat Where to store the flat address.
 * @param   pcBits  Where to store the segment bitness (16/32/64). Optional.
 */
DECLINLINE(int) selmValidateAndConvertCSAddrStd(PVM pVM, RTSEL SelCPL, RTSEL SelCS, RTGCPTR Addr, PRTGCPTR ppvFlat, uint32_t *pcBits)
{
    Assert(!CPUMAreHiddenSelRegsValid(pVM));

    /** @todo validate limit! */
    X86DESC    Desc;
    if (!(SelCS & X86_SEL_LDT))
        Desc = pVM->selm.s.CTX_SUFF(paGdt)[SelCS >> X86_SEL_SHIFT];
    else
    {
        /** @todo handle LDT page(s) not present! */
        PX86DESC    paLDT = (PX86DESC)((char *)pVM->selm.s.CTX_SUFF(pvLdt) + pVM->selm.s.offLdtHyper);
        Desc = paLDT[SelCS >> X86_SEL_SHIFT];
    }

    /*
     * Check if present.
     */
    if (Desc.Gen.u1Present)
    {
        /*
         * Type check.
         */
        if (    Desc.Gen.u1DescType == 1
            &&  (Desc.Gen.u4Type & X86_SEL_TYPE_CODE))
        {
            /*
             * Check level.
             */
            unsigned uLevel = RT_MAX(SelCPL & X86_SEL_RPL, SelCS & X86_SEL_RPL);
            if (    !(Desc.Gen.u4Type & X86_SEL_TYPE_CONF)
                ?   uLevel <= Desc.Gen.u2Dpl
                :   uLevel >= Desc.Gen.u2Dpl /* hope I got this right now... */
                    )
            {
                /*
                 * Limit check.
                 */
                uint32_t    u32Limit = X86DESC_LIMIT(Desc);
                if (Desc.Gen.u1Granularity)
                    u32Limit = (u32Limit << PAGE_SHIFT) | PAGE_OFFSET_MASK;
                if ((RTGCUINTPTR)Addr <= u32Limit)
                {
                    *ppvFlat = (RTGCPTR)((RTGCUINTPTR)Addr + X86DESC_BASE(Desc));
                    if (pcBits)
                        *pcBits = Desc.Gen.u1DefBig ? 32 : 16; /** @todo GUEST64 */
                    return VINF_SUCCESS;
                }
                return VERR_OUT_OF_SELECTOR_BOUNDS;
            }
            return VERR_INVALID_RPL;
        }
        return VERR_NOT_CODE_SELECTOR;
    }
    return VERR_SELECTOR_NOT_PRESENT;
}
#endif /* !IN_RING0 */


/**
 * Validates and converts a GC selector based code address to a flat
 * address when in protected/long mode using the standard algorithm.
 *
 * @returns VBox status code.
 * @param   pVM     VM Handle.
 * @param   SelCPL  Current privilege level. Get this from SS - CS might be conforming!
 *                  A full selector can be passed, we'll only use the RPL part.
 * @param   SelCS   Selector part.
 * @param   Addr    Address part.
 * @param   ppvFlat Where to store the flat address.
 */
DECLINLINE(int) selmValidateAndConvertCSAddrHidden(PVM pVM, RTSEL SelCPL, RTSEL SelCS, PCPUMSELREGHID pHidCS, RTGCPTR Addr, PRTGCPTR ppvFlat)
{
    /*
     * Check if present.
     */
    if (pHidCS->Attr.n.u1Present)
    {
        /*
         * Type check.
         */
        if (     pHidCS->Attr.n.u1DescType == 1
            &&  (pHidCS->Attr.n.u4Type & X86_SEL_TYPE_CODE))
        {
            /*
             * Check level.
             */
            unsigned uLevel = RT_MAX(SelCPL & X86_SEL_RPL, SelCS & X86_SEL_RPL);
            if (    !(pHidCS->Attr.n.u4Type & X86_SEL_TYPE_CONF)
                ?   uLevel <= pHidCS->Attr.n.u2Dpl
                :   uLevel >= pHidCS->Attr.n.u2Dpl /* hope I got this right now... */
                    )
            {
                /* 64 bits mode: CS, DS, ES and SS are treated as if each segment base is 0 (Intel® 64 and IA-32 Architectures Software Developer's Manual: 3.4.2.1). */
                if (    CPUMIsGuestInLongMode(pVM)
                    &&  pHidCS->Attr.n.u1Long)
                {
                    *ppvFlat = Addr;
                    return VINF_SUCCESS;
                }

                /*
                 * Limit check. Note that the limit in the hidden register is the
                 * final value. The granularity bit was included in its calculation.
                 */
                uint32_t u32Limit = pHidCS->u32Limit;
                if ((RTGCUINTPTR)Addr <= u32Limit)
                {
                    *ppvFlat = (RTGCPTR)(  (RTGCUINTPTR)Addr + pHidCS->u64Base );
                    return VINF_SUCCESS;
                }
                return VERR_OUT_OF_SELECTOR_BOUNDS;
            }
            Log(("Invalid RPL Attr.n.u4Type=%x cpl=%x dpl=%x\n", pHidCS->Attr.n.u4Type, uLevel, pHidCS->Attr.n.u2Dpl));
            return VERR_INVALID_RPL;
        }
        return VERR_NOT_CODE_SELECTOR;
    }
    return VERR_SELECTOR_NOT_PRESENT;
}


#ifdef IN_RC
/**
 * Validates and converts a GC selector based code address to a flat address.
 *
 * This is like SELMValidateAndConvertCSAddr + SELMIsSelector32Bit but with
 * invalid hidden CS data. It's customized for dealing efficiently with CS
 * at GC trap time.
 *
 * @returns VBox status code.
 * @param   pVM          VM Handle.
 * @param   eflags       Current eflags
 * @param   SelCPL       Current privilege level. Get this from SS - CS might be conforming!
 *                       A full selector can be passed, we'll only use the RPL part.
 * @param   SelCS        Selector part.
 * @param   Addr         Address part.
 * @param   ppvFlat      Where to store the flat address.
 * @param   pcBits       Where to store the 64-bit/32-bit/16-bit indicator.
 */
VMMDECL(int) SELMValidateAndConvertCSAddrGCTrap(PVM pVM, X86EFLAGS eflags, RTSEL SelCPL, RTSEL SelCS, RTGCPTR Addr, PRTGCPTR ppvFlat, uint32_t *pcBits)
{
    if (    CPUMIsGuestInRealMode(pVM)
        ||  eflags.Bits.u1VM)
    {
        *pcBits = 16;
        return selmValidateAndConvertCSAddrRealMode(pVM, SelCS, NULL, Addr, ppvFlat);
    }
    return selmValidateAndConvertCSAddrStd(pVM, SelCPL, SelCS, Addr, ppvFlat, pcBits);
}
#endif /* IN_RC */


/**
 * Validates and converts a GC selector based code address to a flat address.
 *
 * @returns VBox status code.
 * @param   pVM          VM Handle.
 * @param   eflags       Current eflags
 * @param   SelCPL       Current privilege level. Get this from SS - CS might be conforming!
 *                       A full selector can be passed, we'll only use the RPL part.
 * @param   SelCS        Selector part.
 * @param   pHiddenSel   The hidden CS selector register.
 * @param   Addr         Address part.
 * @param   ppvFlat      Where to store the flat address.
 */
VMMDECL(int) SELMValidateAndConvertCSAddr(PVM pVM, X86EFLAGS eflags, RTSEL SelCPL, RTSEL SelCS, CPUMSELREGHID *pHiddenCSSel, RTGCPTR Addr, PRTGCPTR ppvFlat)
{
    if (    CPUMIsGuestInRealMode(pVM)
        ||  eflags.Bits.u1VM)
        return selmValidateAndConvertCSAddrRealMode(pVM, SelCS, pHiddenCSSel, Addr, ppvFlat);

#ifdef IN_RING0
    Assert(CPUMAreHiddenSelRegsValid(pVM));
#else
    /** @todo when we're in 16 bits mode, we should cut off the address as well? (like in selmValidateAndConvertCSAddrRealMode) */
    if (!CPUMAreHiddenSelRegsValid(pVM))
        return selmValidateAndConvertCSAddrStd(pVM, SelCPL, SelCS, Addr, ppvFlat, NULL);
#endif
    return selmValidateAndConvertCSAddrHidden(pVM, SelCPL, SelCS, pHiddenCSSel, Addr, ppvFlat);
}


#ifndef IN_RING0
/**
 * Return the cpu mode corresponding to the (CS) selector
 *
 * @returns DISCPUMODE according to the selector type (16, 32 or 64 bits)
 * @param   pVM     VM Handle.
 * @param   Sel     The selector.
 */
static DISCPUMODE selmGetCpuModeFromSelector(PVM pVM, RTSEL Sel)
{
    Assert(!CPUMAreHiddenSelRegsValid(pVM));

    /** @todo validate limit! */
    X86DESC Desc;
    if (!(Sel & X86_SEL_LDT))
        Desc = pVM->selm.s.CTX_SUFF(paGdt)[Sel >> X86_SEL_SHIFT];
    else
    {
        /** @todo handle LDT page(s) not present! */
        PX86DESC   paLDT = (PX86DESC)((char *)pVM->selm.s.CTX_SUFF(pvLdt) + pVM->selm.s.offLdtHyper);
        Desc = paLDT[Sel >> X86_SEL_SHIFT];
    }
    return (Desc.Gen.u1DefBig) ? CPUMODE_32BIT : CPUMODE_16BIT;
}
#endif /* !IN_RING0 */


/**
 * Return the cpu mode corresponding to the (CS) selector
 *
 * @returns DISCPUMODE according to the selector type (16, 32 or 64 bits)
 * @param   pVM        VM Handle.
 * @param   eflags     Current eflags register
 * @param   Sel        The selector.
 * @param   pHiddenSel The hidden selector register.
 */
VMMDECL(DISCPUMODE) SELMGetCpuModeFromSelector(PVM pVM, X86EFLAGS eflags, RTSEL Sel, CPUMSELREGHID *pHiddenSel)
{
#ifdef IN_RING0
    Assert(CPUMAreHiddenSelRegsValid(pVM));
#else  /* !IN_RING0 */
    if (!CPUMAreHiddenSelRegsValid(pVM))
    {
        /*
         * Deal with real & v86 mode first.
         */
        if (    CPUMIsGuestInRealMode(pVM)
            ||  eflags.Bits.u1VM)
            return CPUMODE_16BIT;

        return selmGetCpuModeFromSelector(pVM, Sel);
    }
#endif /* !IN_RING0 */
    if (    CPUMIsGuestInLongMode(pVM)
        &&  pHiddenSel->Attr.n.u1Long)
        return CPUMODE_64BIT;

    /* Else compatibility or 32 bits mode. */
    return (pHiddenSel->Attr.n.u1DefBig) ? CPUMODE_32BIT : CPUMODE_16BIT;
}


/**
 * Returns Hypervisor's Trap 08 (\#DF) selector.
 *
 * @returns Hypervisor's Trap 08 (\#DF) selector.
 * @param   pVM     VM Handle.
 */
VMMDECL(RTSEL) SELMGetTrap8Selector(PVM pVM)
{
    return pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS_TRAP08];
}


/**
 * Sets EIP of Hypervisor's Trap 08 (\#DF) TSS.
 *
 * @param   pVM     VM Handle.
 * @param   u32EIP  EIP of Trap 08 handler.
 */
VMMDECL(void) SELMSetTrap8EIP(PVM pVM, uint32_t u32EIP)
{
    pVM->selm.s.TssTrap08.eip = u32EIP;
}


/**
 * Sets ss:esp for ring1 in main Hypervisor's TSS.
 *
 * @param   pVM     VM Handle.
 * @param   ss      Ring1 SS register value. Pass 0 if invalid.
 * @param   esp     Ring1 ESP register value.
 */
void selmSetRing1Stack(PVM pVM, uint32_t ss, RTGCPTR32 esp)
{
    Assert((ss & 1) || esp == 0);
    pVM->selm.s.Tss.ss1  = ss;
    pVM->selm.s.Tss.esp1 = (uint32_t)esp;
}


#ifndef IN_RING0
/**
 * Gets ss:esp for ring1 in main Hypervisor's TSS.
 *
 * Returns SS=0 if the ring-1 stack isn't valid.
 *
 * @returns VBox status code.
 * @param   pVM     VM Handle.
 * @param   pSS     Ring1 SS register value.
 * @param   pEsp    Ring1 ESP register value.
 */
VMMDECL(int) SELMGetRing1Stack(PVM pVM, uint32_t *pSS, PRTGCPTR32 pEsp)
{
    if (pVM->selm.s.fSyncTSSRing0Stack)
    {
        RTGCPTR GCPtrTss = pVM->selm.s.GCPtrGuestTss;
        int     rc;
        VBOXTSS tss;

        Assert(pVM->selm.s.GCPtrGuestTss && pVM->selm.s.cbMonitoredGuestTss);

# ifdef IN_RC
        bool    fTriedAlready = false;

l_tryagain:
        rc  = MMGCRamRead(pVM, &tss.ss0,  (RCPTRTYPE(void *))(GCPtrTss + RT_OFFSETOF(VBOXTSS, ss0)), sizeof(tss.ss0));
        rc |= MMGCRamRead(pVM, &tss.esp0, (RCPTRTYPE(void *))(GCPtrTss + RT_OFFSETOF(VBOXTSS, esp0)), sizeof(tss.esp0));
#  ifdef DEBUG
        rc |= MMGCRamRead(pVM, &tss.offIoBitmap, (RCPTRTYPE(void *))(GCPtrTss + RT_OFFSETOF(VBOXTSS, offIoBitmap)), sizeof(tss.offIoBitmap));
#  endif

        if (RT_FAILURE(rc))
        {
            if (!fTriedAlready)
            {
                /* Shadow page might be out of sync. Sync and try again */
                /** @todo might cross page boundary */
                fTriedAlready = true;
                rc = PGMPrefetchPage(pVM, (RTGCPTR)GCPtrTss);
                if (rc != VINF_SUCCESS)
                    return rc;
                goto l_tryagain;
            }
            AssertMsgFailed(("Unable to read TSS structure at %08X\n", GCPtrTss));
            return rc;
        }

# else /* !IN_RC */
        /* Reading too much. Could be cheaper than two seperate calls though. */
        rc = PGMPhysSimpleReadGCPtr(pVM, &tss, GCPtrTss, sizeof(VBOXTSS));
        if (RT_FAILURE(rc))
        {
            AssertReleaseMsgFailed(("Unable to read TSS structure at %08X\n", GCPtrTss));
            return rc;
        }
# endif /* !IN_RC */

# ifdef LOG_ENABLED
        uint32_t ssr0  = pVM->selm.s.Tss.ss1;
        uint32_t espr0 = pVM->selm.s.Tss.esp1;
        ssr0 &= ~1;

        if (ssr0 != tss.ss0 || espr0 != tss.esp0)
            Log(("SELMGetRing1Stack: Updating TSS ring 0 stack to %04X:%08X\n", tss.ss0, tss.esp0));

        Log(("offIoBitmap=%#x\n", tss.offIoBitmap));
# endif
        /* Update our TSS structure for the guest's ring 1 stack */
        selmSetRing1Stack(pVM, tss.ss0 | 1, (RTGCPTR32)tss.esp0);
        pVM->selm.s.fSyncTSSRing0Stack = false;
    }

    *pSS  = pVM->selm.s.Tss.ss1;
    *pEsp = (RTGCPTR32)pVM->selm.s.Tss.esp1;

    return VINF_SUCCESS;
}
#endif /* !IN_RING0 */


/**
 * Returns Guest TSS pointer
 *
 * @returns Pointer to the guest TSS, RTRCPTR_MAX if not being monitored.
 * @param   pVM     VM Handle.
 */
VMMDECL(RTGCPTR) SELMGetGuestTSS(PVM pVM)
{
    return (RTGCPTR)pVM->selm.s.GCPtrGuestTss;
}


/**
 * Validates a CS selector.
 *
 * @returns VBox status code.
 * @param   pSelInfo    Pointer to the selector information for the CS selector.
 * @param   SelCPL      The selector defining the CPL (SS).
 */
VMMDECL(int) SELMSelInfoValidateCS(PCSELMSELINFO pSelInfo, RTSEL SelCPL)
{
    /*
     * Check if present.
     */
    if (pSelInfo->Raw.Gen.u1Present)
    {
        /*
         * Type check.
         */
        if (    pSelInfo->Raw.Gen.u1DescType == 1
            &&  (pSelInfo->Raw.Gen.u4Type & X86_SEL_TYPE_CODE))
        {
            /*
             * Check level.
             */
            unsigned uLevel = RT_MAX(SelCPL & X86_SEL_RPL, pSelInfo->Sel & X86_SEL_RPL);
            if (    !(pSelInfo->Raw.Gen.u4Type & X86_SEL_TYPE_CONF)
                ?   uLevel <= pSelInfo->Raw.Gen.u2Dpl
                :   uLevel >= pSelInfo->Raw.Gen.u2Dpl /* hope I got this right now... */
                    )
                return VINF_SUCCESS;
            return VERR_INVALID_RPL;
        }
        return VERR_NOT_CODE_SELECTOR;
    }
    return VERR_SELECTOR_NOT_PRESENT;
}

#ifndef IN_RING0

/**
 * Gets the hypervisor code selector (CS).
 * @returns CS selector.
 * @param   pVM     The VM handle.
 */
VMMDECL(RTSEL) SELMGetHyperCS(PVM pVM)
{
    return pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS];
}


/**
 * Gets the 64-mode hypervisor code selector (CS64).
 * @returns CS selector.
 * @param   pVM     The VM handle.
 */
VMMDECL(RTSEL) SELMGetHyperCS64(PVM pVM)
{
    return pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS64];
}


/**
 * Gets the hypervisor data selector (DS).
 * @returns DS selector.
 * @param   pVM     The VM handle.
 */
VMMDECL(RTSEL) SELMGetHyperDS(PVM pVM)
{
    return pVM->selm.s.aHyperSel[SELM_HYPER_SEL_DS];
}


/**
 * Gets the hypervisor TSS selector.
 * @returns TSS selector.
 * @param   pVM     The VM handle.
 */
VMMDECL(RTSEL) SELMGetHyperTSS(PVM pVM)
{
    return pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS];
}


/**
 * Gets the hypervisor TSS Trap 8 selector.
 * @returns TSS Trap 8 selector.
 * @param   pVM     The VM handle.
 */
VMMDECL(RTSEL) SELMGetHyperTSSTrap08(PVM pVM)
{
    return pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS_TRAP08];
}

/**
 * Gets the address for the hypervisor GDT.
 *
 * @returns The GDT address.
 * @param   pVM     The VM handle.
 * @remark  This is intended only for very special use, like in the world
 *          switchers. Don't exploit this API!
 */
VMMDECL(RTRCPTR) SELMGetHyperGDT(PVM pVM)
{
    /*
     * Always convert this from the HC pointer since we can be
     * called before the first relocation and have to work correctly
     * without having dependencies on the relocation order.
     */
    return (RTRCPTR)MMHyperR3ToRC(pVM, pVM->selm.s.paGdtR3);
}

#endif /* !IN_RING0 */

/**
 * Gets info about the current TSS.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if we've got a TSS loaded.
 * @retval  VERR_SELM_NO_TSS if we haven't got a TSS (rather unlikely).
 *
 * @param   pVM                 The VM handle.
 * @param   pGCPtrTss           Where to store the TSS address.
 * @param   pcbTss              Where to store the TSS size limit.
 * @param   pfCanHaveIOBitmap   Where to store the can-have-I/O-bitmap indicator. (optional)
 */
VMMDECL(int) SELMGetTSSInfo(PVM pVM, PRTGCUINTPTR pGCPtrTss, PRTGCUINTPTR pcbTss, bool *pfCanHaveIOBitmap)
{
    /*
     * The TR hidden register is always valid.
     */
    CPUMSELREGHID trHid;
    RTSEL tr = CPUMGetGuestTR(pVM, &trHid);
    if (!(tr & X86_SEL_MASK))
        return VERR_SELM_NO_TSS;

    *pGCPtrTss = trHid.u64Base;
    *pcbTss    = trHid.u32Limit + (trHid.u32Limit != UINT32_MAX); /* be careful. */
    if (pfCanHaveIOBitmap)
        *pfCanHaveIOBitmap = trHid.Attr.n.u4Type == X86_SEL_TYPE_SYS_386_TSS_AVAIL
                          || trHid.Attr.n.u4Type == X86_SEL_TYPE_SYS_386_TSS_BUSY;
    return VINF_SUCCESS;
}



/**
 * Notification callback which is called whenever there is a chance that a CR3
 * value might have changed.
 * This is called by PGM.
 *
 * @param   pVM       The VM handle
 */
VMMDECL(void) SELMShadowCR3Changed(PVM pVM)
{
    pVM->selm.s.Tss.cr3       = PGMGetHyperCR3(pVM);
    pVM->selm.s.TssTrap08.cr3 = PGMGetInterRCCR3(pVM);
}
