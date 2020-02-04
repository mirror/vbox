/* $Id$ */
/** @file
 * SELM All contexts.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_SELM
#include <VBox/vmm/selm.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/hm.h>
#include "SELMInternal.h"
#include <VBox/vmm/vmcc.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <iprt/assert.h>
#include <VBox/vmm/vmm.h>
#include <iprt/x86.h>
#include <iprt/string.h>



/**
 * Converts a GC selector based address to a flat address.
 *
 * No limit checks are done. Use the SELMToFlat*() or SELMValidate*() functions
 * for that.
 *
 * @returns Flat address.
 * @param   pVM         The cross context VM structure.
 * @param   SelReg      Selector register
 * @param   pCtxCore    CPU context
 * @param   Addr        Address part.
 */
VMMDECL(RTGCPTR) SELMToFlat(PVMCC pVM, DISSELREG SelReg, PCPUMCTXCORE pCtxCore, RTGCPTR Addr)
{
    PCPUMSELREG    pSReg;
    PVMCPUCC       pVCpu = VMMGetCpu(pVM);

    int rc = DISFetchRegSegEx(pCtxCore, SelReg, &pSReg); AssertRC(rc);

    /*
     * Deal with real & v86 mode first.
     */
    if (    pCtxCore->eflags.Bits.u1VM
        ||  CPUMIsGuestInRealMode(pVCpu))
    {
        uint32_t uFlat = (uint32_t)Addr & 0xffff;
        if (CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, pSReg))
            uFlat += (uint32_t)pSReg->u64Base;
        else
            uFlat += (uint32_t)pSReg->Sel << 4;
        return (RTGCPTR)uFlat;
    }

    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, pSReg));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pCtxCore->cs));

    /* 64 bits mode: CS, DS, ES and SS are treated as if each segment base is 0
       (Intel® 64 and IA-32 Architectures Software Developer's Manual: 3.4.2.1). */
    if (    pCtxCore->cs.Attr.n.u1Long
        &&  CPUMIsGuestInLongMode(pVCpu))
    {
        switch (SelReg)
        {
            case DISSELREG_FS:
            case DISSELREG_GS:
                return (RTGCPTR)(pSReg->u64Base + Addr);

            default:
                return Addr;    /* base 0 */
        }
    }

    /* AMD64 manual: compatibility mode ignores the high 32 bits when calculating an effective address. */
    Assert(pSReg->u64Base <= 0xffffffff);
    return (uint32_t)pSReg->u64Base + (uint32_t)Addr;
}


/**
 * Converts a GC selector based address to a flat address.
 *
 * Some basic checking is done, but not all kinds yet.
 *
 * @returns VBox status
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   SelReg      Selector register.
 * @param   pCtxCore    CPU context.
 * @param   Addr        Address part.
 * @param   fFlags      SELMTOFLAT_FLAGS_*
 *                      GDT entires are valid.
 * @param   ppvGC       Where to store the GC flat address.
 */
VMMDECL(int) SELMToFlatEx(PVMCPU pVCpu, DISSELREG SelReg, PCPUMCTXCORE pCtxCore, RTGCPTR Addr, uint32_t fFlags, PRTGCPTR ppvGC)
{
    /*
     * Fetch the selector first.
     */
    PCPUMSELREG pSReg;
    int rc = DISFetchRegSegEx(pCtxCore, SelReg, &pSReg);
    AssertRCReturn(rc, rc); AssertPtr(pSReg);

    /*
     * Deal with real & v86 mode first.
     */
    if (    pCtxCore->eflags.Bits.u1VM
        ||  CPUMIsGuestInRealMode(pVCpu))
    {
        if (ppvGC)
        {
            uint32_t uFlat = (uint32_t)Addr & 0xffff;
            if (CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, pSReg))
                *ppvGC = (uint32_t)pSReg->u64Base + uFlat;
            else
                *ppvGC = ((uint32_t)pSReg->Sel << 4) + uFlat;
        }
        return VINF_SUCCESS;
    }

    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, pSReg));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pCtxCore->cs));

    /* 64 bits mode: CS, DS, ES and SS are treated as if each segment base is 0
       (Intel® 64 and IA-32 Architectures Software Developer's Manual: 3.4.2.1). */
    RTGCPTR  pvFlat;
    bool     fCheckLimit   = true;
    if (    pCtxCore->cs.Attr.n.u1Long
        &&  CPUMIsGuestInLongMode(pVCpu))
    {
        fCheckLimit = false;
        switch (SelReg)
        {
            case DISSELREG_FS:
            case DISSELREG_GS:
                pvFlat = pSReg->u64Base + Addr;
                break;

            default:
                pvFlat = Addr;
                break;
        }
    }
    else
    {
        /* AMD64 manual: compatibility mode ignores the high 32 bits when calculating an effective address. */
        Assert(pSReg->u64Base <= UINT32_C(0xffffffff));
        pvFlat  = (uint32_t)pSReg->u64Base + (uint32_t)Addr;
        Assert(pvFlat <= UINT32_MAX);
    }

    /*
     * Check type if present.
     */
    if (pSReg->Attr.n.u1Present)
    {
        switch (pSReg->Attr.n.u4Type)
        {
            /* Read only selector type. */
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
                if (fCheckLimit && Addr > pSReg->u32Limit)
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
                if (fCheckLimit && Addr > pSReg->u32Limit)
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
                    if (!pSReg->Attr.n.u1Granularity && Addr > UINT32_C(0xffff))
                        return VERR_OUT_OF_SELECTOR_BOUNDS;
                    if (Addr <= pSReg->u32Limit)
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
    return VERR_SELECTOR_NOT_PRESENT;
}



/**
 * Validates and converts a GC selector based code address to a flat
 * address when in real or v8086 mode.
 *
 * @returns VINF_SUCCESS.
 * @param   pVCpu   The cross context virtual CPU structure.
 * @param   SelCS   Selector part.
 * @param   pSReg   The hidden CS register part. Optional.
 * @param   Addr    Address part.
 * @param   ppvFlat Where to store the flat address.
 */
DECLINLINE(int) selmValidateAndConvertCSAddrRealMode(PVMCPU pVCpu, RTSEL SelCS, PCCPUMSELREGHID pSReg, RTGCPTR Addr,
                                                     PRTGCPTR ppvFlat)
{
    NOREF(pVCpu);
    uint32_t uFlat = Addr & 0xffff;
    if (!pSReg || !CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, pSReg))
        uFlat += (uint32_t)SelCS << 4;
    else
        uFlat += (uint32_t)pSReg->u64Base;
    *ppvFlat = uFlat;
    return VINF_SUCCESS;
}


/**
 * Validates and converts a GC selector based code address to a flat address
 * when in protected/long mode using the standard hidden selector registers
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   SelCPL      Current privilege level.  Get this from SS - CS might be
 *                      conforming!  A full selector can be passed, we'll only
 *                      use the RPL part.
 * @param   SelCS       Selector part.
 * @param   pSRegCS     The full CS selector register.
 * @param   Addr        The address (think IP/EIP/RIP).
 * @param   ppvFlat     Where to store the flat address upon successful return.
 */
DECLINLINE(int) selmValidateAndConvertCSAddrHidden(PVMCPU pVCpu, RTSEL SelCPL, RTSEL SelCS, PCCPUMSELREGHID pSRegCS,
                                                   RTGCPTR Addr, PRTGCPTR ppvFlat)
{
    NOREF(SelCPL); NOREF(SelCS);

    /*
     * Check if present.
     */
    if (pSRegCS->Attr.n.u1Present)
    {
        /*
         * Type check.
         */
        if (     pSRegCS->Attr.n.u1DescType == 1
            &&  (pSRegCS->Attr.n.u4Type & X86_SEL_TYPE_CODE))
        {
            /* 64 bits mode: CS, DS, ES and SS are treated as if each segment base is 0
               (Intel® 64 and IA-32 Architectures Software Developer's Manual: 3.4.2.1). */
            if (    pSRegCS->Attr.n.u1Long
                &&  CPUMIsGuestInLongMode(pVCpu))
            {
                *ppvFlat = Addr;
                return VINF_SUCCESS;
            }

            /*
             * Limit check. Note that the limit in the hidden register is the
             * final value. The granularity bit was included in its calculation.
             */
            uint32_t u32Limit = pSRegCS->u32Limit;
            if ((uint32_t)Addr <= u32Limit)
            {
                *ppvFlat = (uint32_t)Addr + (uint32_t)pSRegCS->u64Base;
                return VINF_SUCCESS;
            }

            return VERR_OUT_OF_SELECTOR_BOUNDS;
        }
        return VERR_NOT_CODE_SELECTOR;
    }
    return VERR_SELECTOR_NOT_PRESENT;
}


/**
 * Validates and converts a GC selector based code address to a flat address.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   Efl         Current EFLAGS.
 * @param   SelCPL      Current privilege level.  Get this from SS - CS might be
 *                      conforming!  A full selector can be passed, we'll only
 *                      use the RPL part.
 * @param   SelCS       Selector part.
 * @param   pSRegCS     The full CS selector register.
 * @param   Addr        The address (think IP/EIP/RIP).
 * @param   ppvFlat     Where to store the flat address upon successful return.
 */
VMMDECL(int) SELMValidateAndConvertCSAddr(PVMCPU pVCpu, X86EFLAGS Efl, RTSEL SelCPL, RTSEL SelCS, PCPUMSELREG pSRegCS,
                                          RTGCPTR Addr, PRTGCPTR ppvFlat)
{
    if (    Efl.Bits.u1VM
        ||  CPUMIsGuestInRealMode(pVCpu))
        return selmValidateAndConvertCSAddrRealMode(pVCpu, SelCS, pSRegCS, Addr, ppvFlat);

    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, pSRegCS));
    Assert(pSRegCS->Sel == SelCS);

    return selmValidateAndConvertCSAddrHidden(pVCpu, SelCPL, SelCS, pSRegCS, Addr, ppvFlat);
}


/**
 * Gets info about the current TSS.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if we've got a TSS loaded.
 * @retval  VERR_SELM_NO_TSS if we haven't got a TSS (rather unlikely).
 *
 * @param   pVM                 The cross context VM structure.
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   pGCPtrTss           Where to store the TSS address.
 * @param   pcbTss              Where to store the TSS size limit.
 * @param   pfCanHaveIOBitmap   Where to store the can-have-I/O-bitmap indicator. (optional)
 */
VMMDECL(int) SELMGetTSSInfo(PVM pVM, PVMCPU pVCpu, PRTGCUINTPTR pGCPtrTss, PRTGCUINTPTR pcbTss, bool *pfCanHaveIOBitmap)
{
    NOREF(pVM);

    /*
     * The TR hidden register is always valid.
     */
    CPUMSELREGHID trHid;
    RTSEL tr = CPUMGetGuestTR(pVCpu, &trHid);
    if (!(tr & X86_SEL_MASK_OFF_RPL))
        return VERR_SELM_NO_TSS;

    *pGCPtrTss = trHid.u64Base;
    *pcbTss    = trHid.u32Limit + (trHid.u32Limit != UINT32_MAX); /* be careful. */
    if (pfCanHaveIOBitmap)
        *pfCanHaveIOBitmap = trHid.Attr.n.u4Type == X86_SEL_TYPE_SYS_386_TSS_AVAIL
                          || trHid.Attr.n.u4Type == X86_SEL_TYPE_SYS_386_TSS_BUSY;
    return VINF_SUCCESS;
}

