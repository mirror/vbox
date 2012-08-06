/* $Id$ */
/** @file
 * IEM - String Instruction Implementation Code Template.
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


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#if OP_SIZE == 8
# define OP_rAX     al
#elif OP_SIZE == 16
# define OP_rAX     ax
#elif OP_SIZE == 32
# define OP_rAX     eax
#elif OP_SIZE == 64
# define OP_rAX     rax
#else
# error "Bad OP_SIZE."
#endif
#define OP_TYPE                     RT_CONCAT3(uint,OP_SIZE,_t)

#if ADDR_SIZE == 16
# define ADDR_rDI   di
# define ADDR_rSI   si
# define ADDR_rCX   cx
# define ADDR2_TYPE uint32_t
#elif ADDR_SIZE == 32
# define ADDR_rDI   edi
# define ADDR_rSI   esi
# define ADDR_rCX   ecx
# define ADDR2_TYPE uint32_t
#elif ADDR_SIZE == 64
# define ADDR_rDI   rdi
# define ADDR_rSI   rsi
# define ADDR_rCX   rcx
# define ADDR2_TYPE uint64_t
#else
# error "Bad ADDR_SIZE."
#endif
#define ADDR_TYPE                   RT_CONCAT3(uint,ADDR_SIZE,_t)


/**
 * Implements 'REPE CMPS'.
 */
IEM_CIMPL_DEF_1(RT_CONCAT4(iemCImpl_repe_cmps_op,OP_SIZE,_addr,ADDR_SIZE), uint8_t, iEffSeg)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Setup.
     */
    ADDR_TYPE       uCounterReg  = pCtx->ADDR_rCX;
    if (uCounterReg == 0)
    {
        iemRegAddToRip(pIemCpu, cbInstr);
        return VINF_SUCCESS;
    }

    PCCPUMSELREGHID pSrc1Hid     = iemSRegGetHid(pIemCpu, iEffSeg);
    VBOXSTRICTRC    rcStrict     = iemMemSegCheckReadAccessEx(pIemCpu, pSrc1Hid, iEffSeg);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    rcStrict = iemMemSegCheckReadAccessEx(pIemCpu, &pCtx->es, X86_SREG_ES);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    int8_t const    cbIncr       = pCtx->eflags.Bits.u1DF ? -(OP_SIZE / 8) : (OP_SIZE / 8);
    ADDR_TYPE       uSrc1AddrReg = pCtx->ADDR_rSI;
    ADDR_TYPE       uSrc2AddrReg = pCtx->ADDR_rDI;
    uint32_t        uEFlags      = pCtx->eflags.u;

    /*
     * The loop.
     */
    do
    {
        /*
         * Do segmentation and virtual page stuff.
         */
#if ADDR_SIZE != 64
        ADDR2_TYPE  uVirtSrc1Addr = (uint32_t)pSrc1Hid->u64Base + uSrc1AddrReg;
        ADDR2_TYPE  uVirtSrc2Addr = (uint32_t)pCtx->es.u64Base  + uSrc2AddrReg;
#else
        uint64_t    uVirtSrc1Addr = uSrc1AddrReg;
        uint64_t    uVirtSrc2Addr = uSrc2AddrReg;
#endif
        uint32_t    cLeftSrc1Page = (PAGE_SIZE - (uVirtSrc1Addr & PAGE_OFFSET_MASK)) / (OP_SIZE / 8);
        if (cLeftSrc1Page > uCounterReg)
            cLeftSrc1Page = uCounterReg;
        uint32_t    cLeftSrc2Page = (PAGE_SIZE - (uVirtSrc2Addr & PAGE_OFFSET_MASK)) / (OP_SIZE / 8);
        uint32_t    cLeftPage     = RT_MIN(cLeftSrc1Page, cLeftSrc2Page);

        if (   cLeftPage > 0 /* can be null if unaligned, do one fallback round. */
            && cbIncr > 0    /** @todo Implement reverse direction string ops. */
#if ADDR_SIZE != 64
            && uSrc1AddrReg < pSrc1Hid->u32Limit
            && uSrc1AddrReg + (cLeftPage * (OP_SIZE / 8)) <= pSrc1Hid->u32Limit
            && uSrc2AddrReg < pCtx->es.u32Limit
            && uSrc2AddrReg + (cLeftPage * (OP_SIZE / 8)) <= pCtx->es.u32Limit
#endif
           )
        {
            RTGCPHYS GCPhysSrc1Mem;
            rcStrict = iemMemPageTranslateAndCheckAccess(pIemCpu, uVirtSrc1Addr, IEM_ACCESS_DATA_R, &GCPhysSrc1Mem);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;

            RTGCPHYS GCPhysSrc2Mem;
            rcStrict = iemMemPageTranslateAndCheckAccess(pIemCpu, uVirtSrc2Addr, IEM_ACCESS_DATA_R, &GCPhysSrc2Mem);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;

            /*
             * If we can map the page without trouble, do a block processing
             * until the end of the current page.
             */
            PGMPAGEMAPLOCK PgLockSrc2Mem;
            OP_TYPE const *puSrc2Mem;
            rcStrict = iemMemPageMap(pIemCpu, GCPhysSrc2Mem, IEM_ACCESS_DATA_R, (void **)&puSrc2Mem, &PgLockSrc2Mem);
            if (rcStrict == VINF_SUCCESS)
            {
                PGMPAGEMAPLOCK PgLockSrc1Mem;
                OP_TYPE const *puSrc1Mem;
                rcStrict = iemMemPageMap(pIemCpu, GCPhysSrc1Mem, IEM_ACCESS_DATA_R, (void **)&puSrc1Mem, &PgLockSrc1Mem);
                if (rcStrict == VINF_SUCCESS)
                {
                    if (!memcmp(puSrc2Mem, puSrc1Mem, cLeftPage * (OP_SIZE / 8)))
                    {
                        /* All matches, only compare the last itme to get the right eflags. */
                        RT_CONCAT(iemAImpl_cmp_u,OP_SIZE)((OP_TYPE *)&puSrc1Mem[cLeftPage-1], puSrc2Mem[cLeftPage-1], &uEFlags);
                        uSrc1AddrReg += cLeftPage * cbIncr;
                        uSrc2AddrReg += cLeftPage * cbIncr;
                        uCounterReg  -= cLeftPage;
                    }
                    else
                    {
                        /* Some mismatch, compare each item (and keep volatile
                           memory in mind). */
                        uint32_t off = 0;
                        do
                        {
                            RT_CONCAT(iemAImpl_cmp_u,OP_SIZE)((OP_TYPE *)&puSrc1Mem[off], puSrc2Mem[off], &uEFlags);
                            off++;
                        } while (   off < cLeftPage
                                 && (uEFlags & X86_EFL_ZF));
                        uSrc1AddrReg += cbIncr * off;
                        uSrc2AddrReg += cbIncr * off;
                        uCounterReg  -= off;
                    }

                    /* Update the registers before looping. */
                    pCtx->ADDR_rCX = uCounterReg;
                    pCtx->ADDR_rSI = uSrc1AddrReg;
                    pCtx->ADDR_rDI = uSrc2AddrReg;
                    pCtx->eflags.u = uEFlags;

                    iemMemPageUnmap(pIemCpu, GCPhysSrc1Mem, IEM_ACCESS_DATA_R, puSrc1Mem, &PgLockSrc1Mem);
                    iemMemPageUnmap(pIemCpu, GCPhysSrc2Mem, IEM_ACCESS_DATA_R, puSrc2Mem, &PgLockSrc2Mem);
                    continue;
                }
            }
            iemMemPageUnmap(pIemCpu, GCPhysSrc2Mem, IEM_ACCESS_DATA_R, puSrc2Mem, &PgLockSrc2Mem);
        }

        /*
         * Fallback - slow processing till the end of the current page.
         * In the cross page boundrary case we will end up here with cLeftPage
         * as 0, we execute one loop then.
         */
        do
        {
            OP_TYPE uValue1;
            rcStrict = RT_CONCAT(iemMemFetchDataU,OP_SIZE)(pIemCpu, &uValue1, iEffSeg, uSrc1AddrReg);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
            OP_TYPE uValue2;
            rcStrict = RT_CONCAT(iemMemFetchDataU,OP_SIZE)(pIemCpu, &uValue2, X86_SREG_ES, uSrc2AddrReg);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
            RT_CONCAT(iemAImpl_cmp_u,OP_SIZE)(&uValue1, uValue2, &uEFlags);

            pCtx->ADDR_rSI = uSrc1AddrReg += cbIncr;
            pCtx->ADDR_rDI = uSrc2AddrReg += cbIncr;
            pCtx->ADDR_rCX = --uCounterReg;
            pCtx->eflags.u = uEFlags;
            cLeftPage--;
        } while (   (int32_t)cLeftPage > 0
                 && (uEFlags & X86_EFL_ZF));
    } while (   uCounterReg != 0
             && (uEFlags & X86_EFL_ZF));

    /*
     * Done.
     */
    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * Implements 'REPNE CMPS'.
 */
IEM_CIMPL_DEF_1(RT_CONCAT4(iemCImpl_repne_cmps_op,OP_SIZE,_addr,ADDR_SIZE), uint8_t, iEffSeg)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Setup.
     */
    ADDR_TYPE       uCounterReg = pCtx->ADDR_rCX;
    if (uCounterReg == 0)
    {
        iemRegAddToRip(pIemCpu, cbInstr);
        return VINF_SUCCESS;
    }

    PCCPUMSELREGHID pSrc1Hid = iemSRegGetHid(pIemCpu, iEffSeg);
    VBOXSTRICTRC rcStrict = iemMemSegCheckReadAccessEx(pIemCpu, pSrc1Hid, iEffSeg);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    rcStrict = iemMemSegCheckReadAccessEx(pIemCpu, &pCtx->es, X86_SREG_ES);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    int8_t const    cbIncr       = pCtx->eflags.Bits.u1DF ? -(OP_SIZE / 8) : (OP_SIZE / 8);
    ADDR_TYPE       uSrc1AddrReg = pCtx->ADDR_rSI;
    ADDR_TYPE       uSrc2AddrReg = pCtx->ADDR_rDI;
    uint32_t        uEFlags      = pCtx->eflags.u;

    /*
     * The loop.
     */
    do
    {
        /*
         * Do segmentation and virtual page stuff.
         */
#if ADDR_SIZE != 64
        ADDR2_TYPE  uVirtSrc1Addr = (uint32_t)pSrc1Hid->u64Base + uSrc1AddrReg;
        ADDR2_TYPE  uVirtSrc2Addr = (uint32_t)pCtx->es.u64Base  + uSrc2AddrReg;
#else
        uint64_t    uVirtSrc1Addr = uSrc1AddrReg;
        uint64_t    uVirtSrc2Addr = uSrc2AddrReg;
#endif
        uint32_t    cLeftSrc1Page = (PAGE_SIZE - (uVirtSrc1Addr & PAGE_OFFSET_MASK)) / (OP_SIZE / 8);
        if (cLeftSrc1Page > uCounterReg)
            cLeftSrc1Page = uCounterReg;
        uint32_t    cLeftSrc2Page = (PAGE_SIZE - (uVirtSrc2Addr & PAGE_OFFSET_MASK)) / (OP_SIZE / 8);
        uint32_t    cLeftPage = RT_MIN(cLeftSrc1Page, cLeftSrc2Page);

        if (   cLeftPage > 0 /* can be null if unaligned, do one fallback round. */
            && cbIncr > 0    /** @todo Implement reverse direction string ops. */
#if ADDR_SIZE != 64
            && uSrc1AddrReg < pSrc1Hid->u32Limit
            && uSrc1AddrReg + (cLeftPage * (OP_SIZE / 8)) <= pSrc1Hid->u32Limit
            && uSrc2AddrReg < pCtx->es.u32Limit
            && uSrc2AddrReg + (cLeftPage * (OP_SIZE / 8)) <= pCtx->es.u32Limit
#endif
           )
        {
            RTGCPHYS GCPhysSrc1Mem;
            rcStrict = iemMemPageTranslateAndCheckAccess(pIemCpu, uVirtSrc1Addr, IEM_ACCESS_DATA_R, &GCPhysSrc1Mem);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;

            RTGCPHYS GCPhysSrc2Mem;
            rcStrict = iemMemPageTranslateAndCheckAccess(pIemCpu, uVirtSrc2Addr, IEM_ACCESS_DATA_R, &GCPhysSrc2Mem);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;

            /*
             * If we can map the page without trouble, do a block processing
             * until the end of the current page.
             */
            OP_TYPE const *puSrc2Mem;
            PGMPAGEMAPLOCK PgLockSrc2Mem;
            rcStrict = iemMemPageMap(pIemCpu, GCPhysSrc2Mem, IEM_ACCESS_DATA_R, (void **)&puSrc2Mem, &PgLockSrc2Mem);
            if (rcStrict == VINF_SUCCESS)
            {
                OP_TYPE const *puSrc1Mem;
                PGMPAGEMAPLOCK PgLockSrc1Mem;
                rcStrict = iemMemPageMap(pIemCpu, GCPhysSrc1Mem, IEM_ACCESS_DATA_R, (void **)&puSrc1Mem, &PgLockSrc1Mem);
                if (rcStrict == VINF_SUCCESS)
                {
                    if (memcmp(puSrc2Mem, puSrc1Mem, cLeftPage * (OP_SIZE / 8)))
                    {
                        /* All matches, only compare the last item to get the right eflags. */
                        RT_CONCAT(iemAImpl_cmp_u,OP_SIZE)((OP_TYPE *)&puSrc1Mem[cLeftPage-1], puSrc2Mem[cLeftPage-1], &uEFlags);
                        uSrc1AddrReg += cLeftPage * cbIncr;
                        uSrc2AddrReg += cLeftPage * cbIncr;
                        uCounterReg  -= cLeftPage;
                    }
                    else
                    {
                        /* Some mismatch, compare each item (and keep volatile
                           memory in mind). */
                        uint32_t off = 0;
                        do
                        {
                            RT_CONCAT(iemAImpl_cmp_u,OP_SIZE)((OP_TYPE *)&puSrc1Mem[off], puSrc2Mem[off], &uEFlags);
                            off++;
                        } while (   off < cLeftPage
                                 && !(uEFlags & X86_EFL_ZF));
                        uSrc1AddrReg += cbIncr * off;
                        uSrc2AddrReg += cbIncr * off;
                        uCounterReg  -= off;
                    }

                    /* Update the registers before looping. */
                    pCtx->ADDR_rCX = uCounterReg;
                    pCtx->ADDR_rSI = uSrc1AddrReg;
                    pCtx->ADDR_rDI = uSrc2AddrReg;
                    pCtx->eflags.u = uEFlags;

                    iemMemPageUnmap(pIemCpu, GCPhysSrc1Mem, IEM_ACCESS_DATA_R, puSrc1Mem, &PgLockSrc1Mem);
                    iemMemPageUnmap(pIemCpu, GCPhysSrc2Mem, IEM_ACCESS_DATA_R, puSrc2Mem, &PgLockSrc2Mem);
                    continue;
                }
                iemMemPageUnmap(pIemCpu, GCPhysSrc2Mem, IEM_ACCESS_DATA_R, puSrc2Mem, &PgLockSrc2Mem);
            }
        }

        /*
         * Fallback - slow processing till the end of the current page.
         * In the cross page boundrary case we will end up here with cLeftPage
         * as 0, we execute one loop then.
         */
        do
        {
            OP_TYPE uValue1;
            rcStrict = RT_CONCAT(iemMemFetchDataU,OP_SIZE)(pIemCpu, &uValue1, iEffSeg, uSrc1AddrReg);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
            OP_TYPE uValue2;
            rcStrict = RT_CONCAT(iemMemFetchDataU,OP_SIZE)(pIemCpu, &uValue2, X86_SREG_ES, uSrc2AddrReg);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
            RT_CONCAT(iemAImpl_cmp_u,OP_SIZE)(&uValue1, uValue2, &uEFlags);

            pCtx->ADDR_rSI = uSrc1AddrReg += cbIncr;
            pCtx->ADDR_rDI = uSrc2AddrReg += cbIncr;
            pCtx->ADDR_rCX = --uCounterReg;
            pCtx->eflags.u = uEFlags;
            cLeftPage--;
        } while (   (int32_t)cLeftPage > 0
                 && !(uEFlags & X86_EFL_ZF));
    } while (   uCounterReg != 0
             && !(uEFlags & X86_EFL_ZF));

    /*
     * Done.
     */
    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * Implements 'REPE SCAS'.
 */
IEM_CIMPL_DEF_0(RT_CONCAT4(iemCImpl_repe_scas_,OP_rAX,_m,ADDR_SIZE))
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Setup.
     */
    ADDR_TYPE       uCounterReg = pCtx->ADDR_rCX;
    if (uCounterReg == 0)
    {
        iemRegAddToRip(pIemCpu, cbInstr);
        return VINF_SUCCESS;
    }

    VBOXSTRICTRC rcStrict = iemMemSegCheckReadAccessEx(pIemCpu, &pCtx->es, X86_SREG_ES);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    int8_t const    cbIncr      = pCtx->eflags.Bits.u1DF ? -(OP_SIZE / 8) : (OP_SIZE / 8);
    OP_TYPE const   uValueReg   = pCtx->OP_rAX;
    ADDR_TYPE       uAddrReg    = pCtx->ADDR_rDI;
    uint32_t        uEFlags     = pCtx->eflags.u;

    /*
     * The loop.
     */
    do
    {
        /*
         * Do segmentation and virtual page stuff.
         */
#if ADDR_SIZE != 64
        ADDR2_TYPE  uVirtAddr = (uint32_t)pCtx->es.u64Base + uAddrReg;
#else
        uint64_t    uVirtAddr = uAddrReg;
#endif
        uint32_t    cLeftPage = (PAGE_SIZE - (uVirtAddr & PAGE_OFFSET_MASK)) / (OP_SIZE / 8);
        if (cLeftPage > uCounterReg)
            cLeftPage = uCounterReg;
        if (   cLeftPage > 0 /* can be null if unaligned, do one fallback round. */
            && cbIncr > 0    /** @todo Implement reverse direction string ops. */
#if ADDR_SIZE != 64
            && uAddrReg < pCtx->es.u32Limit
            && uAddrReg + (cLeftPage * (OP_SIZE / 8)) <= pCtx->es.u32Limit
#endif
           )
        {
            RTGCPHYS GCPhysMem;
            rcStrict = iemMemPageTranslateAndCheckAccess(pIemCpu, uVirtAddr, IEM_ACCESS_DATA_R, &GCPhysMem);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;

            /*
             * If we can map the page without trouble, do a block processing
             * until the end of the current page.
             */
            PGMPAGEMAPLOCK PgLockMem;
            OP_TYPE const *puMem;
            rcStrict = iemMemPageMap(pIemCpu, GCPhysMem, IEM_ACCESS_DATA_R, (void **)&puMem, &PgLockMem);
            if (rcStrict == VINF_SUCCESS)
            {
                /* Search till we find a mismatching item. */
                OP_TYPE  uTmpValue;
                bool     fQuit;
                uint32_t i = 0;
                do
                {
                    uTmpValue = puMem[i++];
                    fQuit = uTmpValue != uValueReg;
                } while (i < cLeftPage && !fQuit);

                /* Update the regs. */
                RT_CONCAT(iemAImpl_cmp_u,OP_SIZE)((OP_TYPE *)&uValueReg, uTmpValue, &uEFlags);
                pCtx->ADDR_rCX = uCounterReg -= i;
                pCtx->ADDR_rDI = uAddrReg    += i * cbIncr;
                pCtx->eflags.u = uEFlags;
                Assert(!(uEFlags & X86_EFL_ZF) == fQuit);
                iemMemPageUnmap(pIemCpu, GCPhysMem, IEM_ACCESS_DATA_R, puMem, &PgLockMem);
                if (fQuit)
                    break;


                /* If unaligned, we drop thru and do the page crossing access
                   below. Otherwise, do the next page. */
                if (!(uVirtAddr & (OP_SIZE - 1)))
                    continue;
                if (uCounterReg == 0)
                    break;
                cLeftPage = 0;
            }
        }

        /*
         * Fallback - slow processing till the end of the current page.
         * In the cross page boundrary case we will end up here with cLeftPage
         * as 0, we execute one loop then.
         */
        do
        {
            OP_TYPE uTmpValue;
            rcStrict = RT_CONCAT(iemMemFetchDataU,OP_SIZE)(pIemCpu, &uTmpValue, X86_SREG_ES, uAddrReg);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
            RT_CONCAT(iemAImpl_cmp_u,OP_SIZE)((OP_TYPE *)&uValueReg, uTmpValue, &uEFlags);

            pCtx->ADDR_rDI = uAddrReg += cbIncr;
            pCtx->ADDR_rCX = --uCounterReg;
            pCtx->eflags.u = uEFlags;
            cLeftPage--;
        } while (   (int32_t)cLeftPage > 0
                 && (uEFlags & X86_EFL_ZF));
    } while (   uCounterReg != 0
             && (uEFlags & X86_EFL_ZF));

    /*
     * Done.
     */
    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * Implements 'REPNE SCAS'.
 */
IEM_CIMPL_DEF_0(RT_CONCAT4(iemCImpl_repne_scas_,OP_rAX,_m,ADDR_SIZE))
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Setup.
     */
    ADDR_TYPE       uCounterReg = pCtx->ADDR_rCX;
    if (uCounterReg == 0)
    {
        iemRegAddToRip(pIemCpu, cbInstr);
        return VINF_SUCCESS;
    }

    VBOXSTRICTRC rcStrict = iemMemSegCheckReadAccessEx(pIemCpu, &pCtx->es, X86_SREG_ES);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    int8_t const    cbIncr      = pCtx->eflags.Bits.u1DF ? -(OP_SIZE / 8) : (OP_SIZE / 8);
    OP_TYPE const   uValueReg   = pCtx->OP_rAX;
    ADDR_TYPE       uAddrReg    = pCtx->ADDR_rDI;
    uint32_t        uEFlags     = pCtx->eflags.u;

    /*
     * The loop.
     */
    do
    {
        /*
         * Do segmentation and virtual page stuff.
         */
#if ADDR_SIZE != 64
        ADDR2_TYPE  uVirtAddr = (uint32_t)pCtx->es.u64Base + uAddrReg;
#else
        uint64_t    uVirtAddr = uAddrReg;
#endif
        uint32_t    cLeftPage = (PAGE_SIZE - (uVirtAddr & PAGE_OFFSET_MASK)) / (OP_SIZE / 8);
        if (cLeftPage > uCounterReg)
            cLeftPage = uCounterReg;
        if (   cLeftPage > 0 /* can be null if unaligned, do one fallback round. */
            && cbIncr > 0    /** @todo Implement reverse direction string ops. */
#if ADDR_SIZE != 64
            && uAddrReg < pCtx->es.u32Limit
            && uAddrReg + (cLeftPage * (OP_SIZE / 8)) <= pCtx->es.u32Limit
#endif
           )
        {
            RTGCPHYS GCPhysMem;
            rcStrict = iemMemPageTranslateAndCheckAccess(pIemCpu, uVirtAddr, IEM_ACCESS_DATA_R, &GCPhysMem);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;

            /*
             * If we can map the page without trouble, do a block processing
             * until the end of the current page.
             */
            PGMPAGEMAPLOCK PgLockMem;
            OP_TYPE const *puMem;
            rcStrict = iemMemPageMap(pIemCpu, GCPhysMem, IEM_ACCESS_DATA_R, (void **)&puMem, &PgLockMem);
            if (rcStrict == VINF_SUCCESS)
            {
                /* Search till we find a mismatching item. */
                OP_TYPE  uTmpValue;
                bool     fQuit;
                uint32_t i = 0;
                do
                {
                    uTmpValue = puMem[i++];
                    fQuit = uTmpValue == uValueReg;
                } while (i < cLeftPage && !fQuit);

                /* Update the regs. */
                RT_CONCAT(iemAImpl_cmp_u,OP_SIZE)((OP_TYPE *)&uValueReg, uTmpValue, &uEFlags);
                pCtx->ADDR_rCX = uCounterReg -= i;
                pCtx->ADDR_rDI = uAddrReg    += i * cbIncr;
                pCtx->eflags.u = uEFlags;
                Assert(!!(uEFlags & X86_EFL_ZF) == fQuit);
                iemMemPageUnmap(pIemCpu, GCPhysMem, IEM_ACCESS_DATA_R, puMem, &PgLockMem);
                if (fQuit)
                    break;


                /* If unaligned, we drop thru and do the page crossing access
                   below. Otherwise, do the next page. */
                if (!(uVirtAddr & (OP_SIZE - 1)))
                    continue;
                if (uCounterReg == 0)
                    break;
                cLeftPage = 0;
            }
        }

        /*
         * Fallback - slow processing till the end of the current page.
         * In the cross page boundrary case we will end up here with cLeftPage
         * as 0, we execute one loop then.
         */
        do
        {
            OP_TYPE uTmpValue;
            rcStrict = RT_CONCAT(iemMemFetchDataU,OP_SIZE)(pIemCpu, &uTmpValue, X86_SREG_ES, uAddrReg);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
            RT_CONCAT(iemAImpl_cmp_u,OP_SIZE)((OP_TYPE *)&uValueReg, uTmpValue, &uEFlags);
            pCtx->ADDR_rDI = uAddrReg += cbIncr;
            pCtx->ADDR_rCX = --uCounterReg;
            pCtx->eflags.u = uEFlags;
            cLeftPage--;
        } while (   (int32_t)cLeftPage > 0
                 && !(uEFlags & X86_EFL_ZF));
    } while (   uCounterReg != 0
             && !(uEFlags & X86_EFL_ZF));

    /*
     * Done.
     */
    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}




/**
 * Implements 'REP MOVS'.
 */
IEM_CIMPL_DEF_1(RT_CONCAT4(iemCImpl_rep_movs_op,OP_SIZE,_addr,ADDR_SIZE), uint8_t, iEffSeg)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Setup.
     */
    ADDR_TYPE       uCounterReg = pCtx->ADDR_rCX;
    if (uCounterReg == 0)
    {
        iemRegAddToRip(pIemCpu, cbInstr);
        return VINF_SUCCESS;
    }

    PCCPUMSELREGHID pSrcHid = iemSRegGetHid(pIemCpu, iEffSeg);
    VBOXSTRICTRC rcStrict = iemMemSegCheckReadAccessEx(pIemCpu, pSrcHid, iEffSeg);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    rcStrict = iemMemSegCheckWriteAccessEx(pIemCpu, &pCtx->es, X86_SREG_ES);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    int8_t const    cbIncr      = pCtx->eflags.Bits.u1DF ? -(OP_SIZE / 8) : (OP_SIZE / 8);
    ADDR_TYPE       uSrcAddrReg = pCtx->ADDR_rSI;
    ADDR_TYPE       uDstAddrReg = pCtx->ADDR_rDI;

    /*
     * If we're reading back what we write, we have to let the verfication code
     * to prevent a false positive.
     * Note! This doesn't take aliasing or wrapping into account - lazy bird.
     */
#ifdef IEM_VERIFICATION_MODE
    if (   IEM_VERIFICATION_ENABLED(pIemCpu)
        && (cbIncr > 0
            ?    uSrcAddrReg <= uDstAddrReg
              && uSrcAddrReg + cbIncr * uCounterReg > uDstAddrReg
            :    uDstAddrReg <= uSrcAddrReg
              && uDstAddrReg + cbIncr * uCounterReg > uSrcAddrReg))
        pIemCpu->fOverlappingMovs = true;
#endif

    /*
     * The loop.
     */
    do
    {
        /*
         * Do segmentation and virtual page stuff.
         */
#if ADDR_SIZE != 64
        ADDR2_TYPE  uVirtSrcAddr = (uint32_t)pSrcHid->u64Base + uSrcAddrReg;
        ADDR2_TYPE  uVirtDstAddr = (uint32_t)pCtx->es.u64Base + uDstAddrReg;
#else
        uint64_t    uVirtSrcAddr = uSrcAddrReg;
        uint64_t    uVirtDstAddr = uDstAddrReg;
#endif
        uint32_t    cLeftSrcPage = (PAGE_SIZE - (uVirtSrcAddr & PAGE_OFFSET_MASK)) / (OP_SIZE / 8);
        if (cLeftSrcPage > uCounterReg)
            cLeftSrcPage = uCounterReg;
        uint32_t    cLeftDstPage = (PAGE_SIZE - (uVirtDstAddr & PAGE_OFFSET_MASK)) / (OP_SIZE / 8);
        uint32_t    cLeftPage = RT_MIN(cLeftSrcPage, cLeftDstPage);

        if (   cLeftPage > 0 /* can be null if unaligned, do one fallback round. */
            && cbIncr > 0    /** @todo Implement reverse direction string ops. */
#if ADDR_SIZE != 64
            && uSrcAddrReg < pSrcHid->u32Limit
            && uSrcAddrReg + (cLeftPage * (OP_SIZE / 8)) <= pSrcHid->u32Limit
            && uDstAddrReg < pCtx->es.u32Limit
            && uDstAddrReg + (cLeftPage * (OP_SIZE / 8)) <= pCtx->es.u32Limit
#endif
           )
        {
            RTGCPHYS GCPhysSrcMem;
            rcStrict = iemMemPageTranslateAndCheckAccess(pIemCpu, uVirtSrcAddr, IEM_ACCESS_DATA_R, &GCPhysSrcMem);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;

            RTGCPHYS GCPhysDstMem;
            rcStrict = iemMemPageTranslateAndCheckAccess(pIemCpu, uVirtDstAddr, IEM_ACCESS_DATA_W, &GCPhysDstMem);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;

            /*
             * If we can map the page without trouble, do a block processing
             * until the end of the current page.
             */
            PGMPAGEMAPLOCK PgLockDstMem;
            OP_TYPE *puDstMem;
            rcStrict = iemMemPageMap(pIemCpu, GCPhysDstMem, IEM_ACCESS_DATA_W, (void **)&puDstMem, &PgLockDstMem);
            if (rcStrict == VINF_SUCCESS)
            {
                PGMPAGEMAPLOCK PgLockSrcMem;
                OP_TYPE const *puSrcMem;
                rcStrict = iemMemPageMap(pIemCpu, GCPhysSrcMem, IEM_ACCESS_DATA_R, (void **)&puSrcMem, &PgLockSrcMem);
                if (rcStrict == VINF_SUCCESS)
                {
                    Assert(   (GCPhysSrcMem         >> PAGE_SHIFT) != (GCPhysDstMem         >> PAGE_SHIFT)
                           || ((uintptr_t)puSrcMem  >> PAGE_SHIFT) == ((uintptr_t)puDstMem  >> PAGE_SHIFT));

                    /* Perform the operation exactly (don't use memcpy to avoid
                       having to consider how its implementation would affect
                       any overlapping source and destination area). */
                    OP_TYPE const  *puSrcCur = puSrcMem;
                    OP_TYPE        *puDstCur = puDstMem;
                    uint32_t        cTodo    = cLeftPage;
                    while (cTodo-- > 0)
                        *puDstCur++ = *puSrcCur++;

                    /* Update the registers. */
                    pCtx->ADDR_rSI = uSrcAddrReg += cLeftPage * cbIncr;
                    pCtx->ADDR_rDI = uDstAddrReg += cLeftPage * cbIncr;
                    pCtx->ADDR_rCX = uCounterReg -= cLeftPage;

                    iemMemPageUnmap(pIemCpu, GCPhysSrcMem, IEM_ACCESS_DATA_R, puSrcMem, &PgLockSrcMem);
                    iemMemPageUnmap(pIemCpu, GCPhysDstMem, IEM_ACCESS_DATA_W, puDstMem, &PgLockDstMem);
                    continue;
                }
                iemMemPageUnmap(pIemCpu, GCPhysDstMem, IEM_ACCESS_DATA_W, puDstMem, &PgLockDstMem);
            }
        }

        /*
         * Fallback - slow processing till the end of the current page.
         * In the cross page boundrary case we will end up here with cLeftPage
         * as 0, we execute one loop then.
         */
        do
        {
            OP_TYPE uValue;
            rcStrict = RT_CONCAT(iemMemFetchDataU,OP_SIZE)(pIemCpu, &uValue, iEffSeg, uSrcAddrReg);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
            rcStrict = RT_CONCAT(iemMemStoreDataU,OP_SIZE)(pIemCpu, X86_SREG_ES, uDstAddrReg, uValue);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;

            pCtx->ADDR_rSI = uSrcAddrReg += cbIncr;
            pCtx->ADDR_rDI = uDstAddrReg += cbIncr;
            pCtx->ADDR_rCX = --uCounterReg;
            cLeftPage--;
        } while ((int32_t)cLeftPage > 0);
    } while (uCounterReg != 0);

    /*
     * Done.
     */
    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * Implements 'REP STOS'.
 */
IEM_CIMPL_DEF_0(RT_CONCAT4(iemCImpl_stos_,OP_rAX,_m,ADDR_SIZE))
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Setup.
     */
    ADDR_TYPE       uCounterReg = pCtx->ADDR_rCX;
    if (uCounterReg == 0)
    {
        iemRegAddToRip(pIemCpu, cbInstr);
        return VINF_SUCCESS;
    }

    VBOXSTRICTRC rcStrict = iemMemSegCheckWriteAccessEx(pIemCpu, &pCtx->es, X86_SREG_ES);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    int8_t const    cbIncr      = pCtx->eflags.Bits.u1DF ? -(OP_SIZE / 8) : (OP_SIZE / 8);
    OP_TYPE const   uValue      = pCtx->OP_rAX;
    ADDR_TYPE       uAddrReg    = pCtx->ADDR_rDI;

    /*
     * The loop.
     */
    do
    {
        /*
         * Do segmentation and virtual page stuff.
         */
#if ADDR_SIZE != 64
        ADDR2_TYPE  uVirtAddr = (uint32_t)pCtx->es.u64Base + uAddrReg;
#else
        uint64_t    uVirtAddr = uAddrReg;
#endif
        uint32_t    cLeftPage = (PAGE_SIZE - (uVirtAddr & PAGE_OFFSET_MASK)) / (OP_SIZE / 8);
        if (cLeftPage > uCounterReg)
            cLeftPage = uCounterReg;
        if (   cLeftPage > 0 /* can be null if unaligned, do one fallback round. */
            && cbIncr > 0    /** @todo Implement reverse direction string ops. */
#if ADDR_SIZE != 64
            && uAddrReg < pCtx->es.u32Limit
            && uAddrReg + (cLeftPage * (OP_SIZE / 8)) <= pCtx->es.u32Limit
#endif
           )
        {
            RTGCPHYS GCPhysMem;
            rcStrict = iemMemPageTranslateAndCheckAccess(pIemCpu, uVirtAddr, IEM_ACCESS_DATA_W, &GCPhysMem);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;

            /*
             * If we can map the page without trouble, do a block processing
             * until the end of the current page.
             */
            PGMPAGEMAPLOCK PgLockMem;
            OP_TYPE *puMem;
            rcStrict = iemMemPageMap(pIemCpu, GCPhysMem, IEM_ACCESS_DATA_W, (void **)&puMem, &PgLockMem);
            if (rcStrict == VINF_SUCCESS)
            {
                /* Update the regs first so we can loop on cLeftPage. */
                pCtx->ADDR_rCX = uCounterReg -= cLeftPage;
                pCtx->ADDR_rDI = uAddrReg    += cLeftPage * cbIncr;

                /* Do the memsetting. */
#if OP_SIZE == 8
                memset(puMem, uValue, cLeftPage);
/*#elif OP_SIZE == 32
                ASMMemFill32(puMem, cLeftPage * (OP_SIZE / 8), uValue);*/
#else
                while (cLeftPage-- > 0)
                    *puMem++ = uValue;
#endif

                iemMemPageUnmap(pIemCpu, GCPhysMem, IEM_ACCESS_DATA_W, puMem, &PgLockMem);

                /* If unaligned, we drop thru and do the page crossing access
                   below. Otherwise, do the next page. */
                if (!(uVirtAddr & (OP_SIZE - 1)))
                    continue;
                if (uCounterReg == 0)
                    break;
                cLeftPage = 0;
            }
        }

        /*
         * Fallback - slow processing till the end of the current page.
         * In the cross page boundrary case we will end up here with cLeftPage
         * as 0, we execute one loop then.
         */
        do
        {
            rcStrict = RT_CONCAT(iemMemStoreDataU,OP_SIZE)(pIemCpu, X86_SREG_ES, uAddrReg, uValue);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
            pCtx->ADDR_rDI = uAddrReg += cbIncr;
            pCtx->ADDR_rCX = --uCounterReg;
            cLeftPage--;
        } while ((int32_t)cLeftPage > 0);
    } while (uCounterReg != 0);

    /*
     * Done.
     */
    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * Implements 'REP LODS'.
 */
IEM_CIMPL_DEF_1(RT_CONCAT4(iemCImpl_lods_,OP_rAX,_m,ADDR_SIZE), int8_t, iEffSeg)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Setup.
     */
    ADDR_TYPE       uCounterReg = pCtx->ADDR_rCX;
    if (uCounterReg == 0)
    {
        iemRegAddToRip(pIemCpu, cbInstr);
        return VINF_SUCCESS;
    }

    PCCPUMSELREGHID pSrcHid = iemSRegGetHid(pIemCpu, iEffSeg);
    VBOXSTRICTRC rcStrict = iemMemSegCheckReadAccessEx(pIemCpu, pSrcHid, iEffSeg);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    int8_t const    cbIncr      = pCtx->eflags.Bits.u1DF ? -(OP_SIZE / 8) : (OP_SIZE / 8);
    ADDR_TYPE       uAddrReg    = pCtx->ADDR_rSI;

    /*
     * The loop.
     */
    do
    {
        /*
         * Do segmentation and virtual page stuff.
         */
#if ADDR_SIZE != 64
        ADDR2_TYPE  uVirtAddr = (uint32_t)pSrcHid->u64Base + uAddrReg;
#else
        uint64_t    uVirtAddr = uAddrReg;
#endif
        uint32_t    cLeftPage = (PAGE_SIZE - (uVirtAddr & PAGE_OFFSET_MASK)) / (OP_SIZE / 8);
        if (cLeftPage > uCounterReg)
            cLeftPage = uCounterReg;
        if (   cLeftPage > 0 /* can be null if unaligned, do one fallback round. */
            && cbIncr > 0    /** @todo Implement reverse direction string ops. */
#if ADDR_SIZE != 64
            && uAddrReg < pSrcHid->u32Limit
            && uAddrReg + (cLeftPage * (OP_SIZE / 8)) <= pSrcHid->u32Limit
#endif
           )
        {
            RTGCPHYS GCPhysMem;
            rcStrict = iemMemPageTranslateAndCheckAccess(pIemCpu, uVirtAddr, IEM_ACCESS_DATA_R, &GCPhysMem);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;

            /*
             * If we can map the page without trouble, we can get away with
             * just reading the last value on the page.
             */
            PGMPAGEMAPLOCK PgLockMem;
            OP_TYPE const *puMem;
            rcStrict = iemMemPageMap(pIemCpu, GCPhysMem, IEM_ACCESS_DATA_R, (void **)&puMem, &PgLockMem);
            if (rcStrict == VINF_SUCCESS)
            {
                /* Only get the last byte, the rest doesn't matter in direct access mode. */
#if OP_SIZE == 32
                pCtx->rax      = puMem[cLeftPage - 1];
#else
                pCtx->OP_rAX   = puMem[cLeftPage - 1];
#endif
                pCtx->ADDR_rCX = uCounterReg -= cLeftPage;
                pCtx->ADDR_rSI = uAddrReg    += cLeftPage * cbIncr;
                iemMemPageUnmap(pIemCpu, GCPhysMem, IEM_ACCESS_DATA_R, puMem, &PgLockMem);

                /* If unaligned, we drop thru and do the page crossing access
                   below. Otherwise, do the next page. */
                if (!(uVirtAddr & (OP_SIZE - 1)))
                    continue;
                if (uCounterReg == 0)
                    break;
                cLeftPage = 0;
            }
        }

        /*
         * Fallback - slow processing till the end of the current page.
         * In the cross page boundrary case we will end up here with cLeftPage
         * as 0, we execute one loop then.
         */
        do
        {
            OP_TYPE uTmpValue;
            rcStrict = RT_CONCAT(iemMemFetchDataU,OP_SIZE)(pIemCpu, &uTmpValue, iEffSeg, uAddrReg);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
#if OP_SIZE == 32
            pCtx->rax      = uTmpValue;
#else
            pCtx->OP_rAX   = uTmpValue;
#endif
            pCtx->ADDR_rSI = uAddrReg += cbIncr;
            pCtx->ADDR_rCX = --uCounterReg;
            cLeftPage--;
        } while ((int32_t)cLeftPage > 0);
        if (rcStrict != VINF_SUCCESS)
            break;
    } while (uCounterReg != 0);

    /*
     * Done.
     */
    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}


#if OP_SIZE != 64

/**
 * Implements 'INS' (no rep)
 */
IEM_CIMPL_DEF_0(RT_CONCAT4(iemCImpl_ins_op,OP_SIZE,_addr,ADDR_SIZE))
{
    PVM             pVM  = IEMCPU_TO_VM(pIemCpu);
    PCPUMCTX        pCtx = pIemCpu->CTX_SUFF(pCtx);
    VBOXSTRICTRC    rcStrict;

    /*
     * ASSUMES the #GP for I/O permission is taken first, then any #GP for
     * segmentation and finally any #PF due to virtual address translation.
     * ASSUMES nothing is read from the I/O port before traps are taken.
     */
    rcStrict = iemHlpCheckPortIOPermission(pIemCpu, pCtx, pCtx->dx, OP_SIZE / 8);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    OP_TYPE        *puMem;
    rcStrict = iemMemMap(pIemCpu, (void **)&puMem, OP_SIZE / 8, X86_SREG_ES, pCtx->ADDR_rDI, IEM_ACCESS_DATA_W);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    uint32_t        u32Value;
    if (!IEM_VERIFICATION_ENABLED(pIemCpu))
        rcStrict = IOMIOPortRead(pVM, pCtx->dx, &u32Value, OP_SIZE / 8);
    else
        rcStrict = iemVerifyFakeIOPortRead(pIemCpu, pCtx->dx, &u32Value, OP_SIZE / 8);
    if (IOM_SUCCESS(rcStrict))
    {
        VBOXSTRICTRC rcStrict2 = iemMemCommitAndUnmap(pIemCpu, puMem, IEM_ACCESS_DATA_W);
        if (RT_LIKELY(rcStrict2 == VINF_SUCCESS))
        {
            if (!pCtx->eflags.Bits.u1DF)
                pCtx->ADDR_rDI += OP_SIZE / 8;
            else
                pCtx->ADDR_rDI -= OP_SIZE / 8;
            iemRegAddToRip(pIemCpu, cbInstr);
        }
        /* iemMemMap already check permissions, so this may only be real errors
           or access handlers medling. The access handler case is going to
           cause misbehavior if the instruction is re-interpreted or smth. So,
           we fail with an internal error here instead. */
        else
            AssertLogRelFailedReturn(VERR_IEM_IPE_1);
    }
    return rcStrict;
}


/**
 * Implements 'REP INS'.
 */
IEM_CIMPL_DEF_0(RT_CONCAT4(iemCImpl_rep_ins_op,OP_SIZE,_addr,ADDR_SIZE))
{
    PVM         pVM  = IEMCPU_TO_VM(pIemCpu);
    PCPUMCTX    pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Setup.
     */
    uint16_t const  u16Port    = pCtx->dx;
    VBOXSTRICTRC rcStrict = iemHlpCheckPortIOPermission(pIemCpu, pCtx, u16Port, OP_SIZE / 8);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    ADDR_TYPE       uCounterReg = pCtx->ADDR_rCX;
    if (uCounterReg == 0)
    {
        iemRegAddToRip(pIemCpu, cbInstr);
        return VINF_SUCCESS;
    }

    rcStrict = iemMemSegCheckWriteAccessEx(pIemCpu, &pCtx->es, X86_SREG_ES);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    int8_t const    cbIncr      = pCtx->eflags.Bits.u1DF ? -(OP_SIZE / 8) : (OP_SIZE / 8);
    ADDR_TYPE       uAddrReg    = pCtx->ADDR_rDI;

    /*
     * The loop.
     */
    do
    {
        /*
         * Do segmentation and virtual page stuff.
         */
#if ADDR_SIZE != 64
        ADDR2_TYPE  uVirtAddr = (uint32_t)pCtx->es.u64Base + uAddrReg;
#else
        uint64_t    uVirtAddr = uAddrReg;
#endif
        uint32_t    cLeftPage = (PAGE_SIZE - (uVirtAddr & PAGE_OFFSET_MASK)) / (OP_SIZE / 8);
        if (cLeftPage > uCounterReg)
            cLeftPage = uCounterReg;
        if (   cLeftPage > 0 /* can be null if unaligned, do one fallback round. */
            && cbIncr > 0    /** @todo Implement reverse direction string ops. */
#if ADDR_SIZE != 64
            && uAddrReg < pCtx->es.u32Limit
            && uAddrReg + (cLeftPage * (OP_SIZE / 8)) <= pCtx->es.u32Limit
#endif
           )
        {
            RTGCPHYS GCPhysMem;
            rcStrict = iemMemPageTranslateAndCheckAccess(pIemCpu, uVirtAddr, IEM_ACCESS_DATA_W, &GCPhysMem);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;

            /*
             * If we can map the page without trouble, we would've liked to use
             * an string I/O method to do the work, but the current IOM
             * interface doesn't match our current approach. So, do a regular
             * loop instead.
             */
            /** @todo Change the I/O manager interface to make use of
             *        mapped buffers instead of leaving those bits to the
             *        device implementation? */
            PGMPAGEMAPLOCK PgLockMem;
            OP_TYPE *puMem;
            rcStrict = iemMemPageMap(pIemCpu, GCPhysMem, IEM_ACCESS_DATA_W, (void **)&puMem, &PgLockMem);
            if (rcStrict == VINF_SUCCESS)
            {
                uint32_t off = 0;
                while (off < cLeftPage)
                {
                    uint32_t u32Value;
                    if (!IEM_VERIFICATION_ENABLED(pIemCpu))
                        rcStrict = IOMIOPortRead(pVM, u16Port, &u32Value, OP_SIZE / 8);
                    else
                        rcStrict = iemVerifyFakeIOPortRead(pIemCpu, u16Port, &u32Value, OP_SIZE / 8);
                    if (IOM_SUCCESS(rcStrict))
                    {
                        puMem[off]     = (OP_TYPE)u32Value;
                        pCtx->ADDR_rDI = uAddrReg += cbIncr;
                        pCtx->ADDR_rCX = --uCounterReg;
                    }
                    if (rcStrict != VINF_SUCCESS)
                    {
                        if (IOM_SUCCESS(rcStrict))
                            rcStrict = iemSetPassUpStatus(pIemCpu, rcStrict);
                        if (uCounterReg == 0)
                            iemRegAddToRip(pIemCpu, cbInstr);
                        iemMemPageUnmap(pIemCpu, GCPhysMem, IEM_ACCESS_DATA_W, puMem, &PgLockMem);
                        return rcStrict;
                    }
                    off++;
                }
                iemMemPageUnmap(pIemCpu, GCPhysMem, IEM_ACCESS_DATA_W, puMem, &PgLockMem);

                /* If unaligned, we drop thru and do the page crossing access
                   below. Otherwise, do the next page. */
                if (!(uVirtAddr & (OP_SIZE - 1)))
                    continue;
                if (uCounterReg == 0)
                    break;
                cLeftPage = 0;
            }
        }

        /*
         * Fallback - slow processing till the end of the current page.
         * In the cross page boundrary case we will end up here with cLeftPage
         * as 0, we execute one loop then.
         *
         * Note! We ASSUME the CPU will raise #PF or #GP before access the
         *       I/O port, otherwise it wouldn't really be restartable.
         */
        /** @todo investigate what the CPU actually does with \#PF/\#GP
         *        during INS. */
        do
        {
            OP_TYPE *puMem;
            rcStrict = iemMemMap(pIemCpu, (void **)&puMem, OP_SIZE / 8, X86_SREG_ES, uAddrReg, IEM_ACCESS_DATA_W);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;

            uint32_t u32Value;
            if (!IEM_VERIFICATION_ENABLED(pIemCpu))
                rcStrict = IOMIOPortRead(pVM, u16Port, &u32Value, OP_SIZE / 8);
            else
                rcStrict = iemVerifyFakeIOPortRead(pIemCpu, u16Port, &u32Value, OP_SIZE / 8);
            if (!IOM_SUCCESS(rcStrict))
                return rcStrict;

            *puMem = (OP_TYPE)u32Value;
            VBOXSTRICTRC rcStrict2 = iemMemCommitAndUnmap(pIemCpu, puMem, IEM_ACCESS_DATA_W);
            AssertLogRelReturn(rcStrict2 == VINF_SUCCESS, VERR_IEM_IPE_1); /* See non-rep version. */

            pCtx->ADDR_rDI = uAddrReg += cbIncr;
            pCtx->ADDR_rCX = --uCounterReg;

            cLeftPage--;
            if (rcStrict != VINF_SUCCESS)
            {
                if (IOM_SUCCESS(rcStrict))
                    rcStrict = iemSetPassUpStatus(pIemCpu, rcStrict);
                if (uCounterReg == 0)
                    iemRegAddToRip(pIemCpu, cbInstr);
                return rcStrict;
            }
        } while ((int32_t)cLeftPage > 0);
    } while (uCounterReg != 0);

    /*
     * Done.
     */
    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * Implements 'OUTS' (no rep)
 */
IEM_CIMPL_DEF_1(RT_CONCAT4(iemCImpl_outs_op,OP_SIZE,_addr,ADDR_SIZE), uint8_t, iEffSeg)
{
    PVM             pVM  = IEMCPU_TO_VM(pIemCpu);
    PCPUMCTX        pCtx = pIemCpu->CTX_SUFF(pCtx);
    VBOXSTRICTRC    rcStrict;

    /*
     * ASSUMES the #GP for I/O permission is taken first, then any #GP for
     * segmentation and finally any #PF due to virtual address translation.
     * ASSUMES nothing is read from the I/O port before traps are taken.
     */
    rcStrict = iemHlpCheckPortIOPermission(pIemCpu, pCtx, pCtx->dx, OP_SIZE / 8);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    OP_TYPE uValue;
    rcStrict = RT_CONCAT(iemMemFetchDataU,OP_SIZE)(pIemCpu, &uValue, iEffSeg, pCtx->ADDR_rSI);
    if (rcStrict == VINF_SUCCESS)
    {
        if (!IEM_VERIFICATION_ENABLED(pIemCpu))
            rcStrict = IOMIOPortWrite(pVM, pCtx->dx, uValue, OP_SIZE / 8);
        else
            rcStrict = iemVerifyFakeIOPortWrite(pIemCpu, pCtx->dx, uValue, OP_SIZE / 8);
        if (IOM_SUCCESS(rcStrict))
        {
            if (!pCtx->eflags.Bits.u1DF)
                pCtx->ADDR_rSI += OP_SIZE / 8;
            else
                pCtx->ADDR_rSI -= OP_SIZE / 8;
            iemRegAddToRip(pIemCpu, cbInstr);
            if (rcStrict != VINF_SUCCESS)
                rcStrict = iemSetPassUpStatus(pIemCpu, rcStrict);
        }
    }
    return rcStrict;
}


/**
 * Implements 'REP OUTS'.
 */
IEM_CIMPL_DEF_1(RT_CONCAT4(iemCImpl_rep_outs_op,OP_SIZE,_addr,ADDR_SIZE), uint8_t, iEffSeg)
{
    PVM         pVM  = IEMCPU_TO_VM(pIemCpu);
    PCPUMCTX    pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Setup.
     */
    uint16_t const  u16Port    = pCtx->dx;
    VBOXSTRICTRC rcStrict = iemHlpCheckPortIOPermission(pIemCpu, pCtx, u16Port, OP_SIZE / 8);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    ADDR_TYPE       uCounterReg = pCtx->ADDR_rCX;
    if (uCounterReg == 0)
    {
        iemRegAddToRip(pIemCpu, cbInstr);
        return VINF_SUCCESS;
    }

    PCCPUMSELREGHID pHid = iemSRegGetHid(pIemCpu, iEffSeg);
    rcStrict = iemMemSegCheckReadAccessEx(pIemCpu, pHid, iEffSeg);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    int8_t const    cbIncr      = pCtx->eflags.Bits.u1DF ? -(OP_SIZE / 8) : (OP_SIZE / 8);
    ADDR_TYPE       uAddrReg    = pCtx->ADDR_rSI;

    /*
     * The loop.
     */
    do
    {
        /*
         * Do segmentation and virtual page stuff.
         */
#if ADDR_SIZE != 64
        ADDR2_TYPE  uVirtAddr = (uint32_t)pHid->u64Base + uAddrReg;
#else
        uint64_t    uVirtAddr = uAddrReg;
#endif
        uint32_t    cLeftPage = (PAGE_SIZE - (uVirtAddr & PAGE_OFFSET_MASK)) / (OP_SIZE / 8);
        if (cLeftPage > uCounterReg)
            cLeftPage = uCounterReg;
        if (   cLeftPage > 0 /* can be null if unaligned, do one fallback round. */
            && cbIncr > 0    /** @todo Implement reverse direction string ops. */
#if ADDR_SIZE != 64
            && uAddrReg < pHid->u32Limit
            && uAddrReg + (cLeftPage * (OP_SIZE / 8)) <= pHid->u32Limit
#endif
           )
        {
            RTGCPHYS GCPhysMem;
            rcStrict = iemMemPageTranslateAndCheckAccess(pIemCpu, uVirtAddr, IEM_ACCESS_DATA_R, &GCPhysMem);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;

            /*
             * If we can map the page without trouble, we would've liked to use
             * an string I/O method to do the work, but the current IOM
             * interface doesn't match our current approach. So, do a regular
             * loop instead.
             */
            /** @todo Change the I/O manager interface to make use of
             *        mapped buffers instead of leaving those bits to the
             *        device implementation? */
            PGMPAGEMAPLOCK PgLockMem;
            OP_TYPE const *puMem;
            rcStrict = iemMemPageMap(pIemCpu, GCPhysMem, IEM_ACCESS_DATA_R, (void **)&puMem, &PgLockMem);
            if (rcStrict == VINF_SUCCESS)
            {
                uint32_t off = 0;
                while (off < cLeftPage)
                {
                    uint32_t u32Value = *puMem++;
                    if (!IEM_VERIFICATION_ENABLED(pIemCpu))
                        rcStrict = IOMIOPortWrite(pVM, u16Port, u32Value, OP_SIZE / 8);
                    else
                        rcStrict = iemVerifyFakeIOPortWrite(pIemCpu, u16Port, u32Value, OP_SIZE / 8);
                    if (IOM_SUCCESS(rcStrict))
                    {
                        pCtx->ADDR_rSI = uAddrReg += cbIncr;
                        pCtx->ADDR_rCX = --uCounterReg;
                    }
                    if (rcStrict != VINF_SUCCESS)
                    {
                        if (IOM_SUCCESS(rcStrict))
                            rcStrict = iemSetPassUpStatus(pIemCpu, rcStrict);
                        if (uCounterReg == 0)
                            iemRegAddToRip(pIemCpu, cbInstr);
                        iemMemPageUnmap(pIemCpu, GCPhysMem, IEM_ACCESS_DATA_R, puMem, &PgLockMem);
                        return rcStrict;
                    }
                    off++;
                }
                iemMemPageUnmap(pIemCpu, GCPhysMem, IEM_ACCESS_DATA_R, puMem, &PgLockMem);

                /* If unaligned, we drop thru and do the page crossing access
                   below. Otherwise, do the next page. */
                if (!(uVirtAddr & (OP_SIZE - 1)))
                    continue;
                if (uCounterReg == 0)
                    break;
                cLeftPage = 0;
            }
        }

        /*
         * Fallback - slow processing till the end of the current page.
         * In the cross page boundrary case we will end up here with cLeftPage
         * as 0, we execute one loop then.
         *
         * Note! We ASSUME the CPU will raise #PF or #GP before access the
         *       I/O port, otherwise it wouldn't really be restartable.
         */
        /** @todo investigate what the CPU actually does with \#PF/\#GP
         *        during INS. */
        do
        {
            OP_TYPE uValue;
            rcStrict = RT_CONCAT(iemMemFetchDataU,OP_SIZE)(pIemCpu, &uValue, iEffSeg, uAddrReg);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;

            if (!IEM_VERIFICATION_ENABLED(pIemCpu))
                rcStrict = IOMIOPortWrite(pVM, u16Port, uValue, OP_SIZE / 8);
            else
                rcStrict = iemVerifyFakeIOPortWrite(pIemCpu, u16Port, uValue, OP_SIZE / 8);
            if (IOM_SUCCESS(rcStrict))
            {
                pCtx->ADDR_rSI = uAddrReg += cbIncr;
                pCtx->ADDR_rCX = --uCounterReg;
                cLeftPage--;
            }
            if (rcStrict != VINF_SUCCESS)
            {
                if (IOM_SUCCESS(rcStrict))
                    rcStrict = iemSetPassUpStatus(pIemCpu, rcStrict);
                if (uCounterReg == 0)
                    iemRegAddToRip(pIemCpu, cbInstr);
                return rcStrict;
            }
        } while ((int32_t)cLeftPage > 0);
    } while (uCounterReg != 0);

    /*
     * Done.
     */
    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}

#endif /* OP_SIZE != 64-bit */


#undef OP_rAX
#undef OP_SIZE
#undef ADDR_SIZE
#undef ADDR_rDI
#undef ADDR_rSI
#undef ADDR_rCX
#undef ADDR_rIP
#undef ADDR2_TYPE
#undef ADDR_TYPE
#undef ADDR2_TYPE

