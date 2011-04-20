/* $Id$ */
/** @file
 * IEM - String Instruction Implementation Code Template.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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
        return VINF_SUCCESS;

    PCCPUMSELREGHID pSrcHid = iemSRegGetHid(pIemCpu, iEffSeg);
    VBOXSTRICTRC rcStrict = iemMemSegCheckReadAccessEx(pIemCpu, pSrcHid, iEffSeg);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    rcStrict = iemMemSegCheckWriteAccessEx(pIemCpu, &pCtx->esHid, X86_SREG_ES);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    int8_t const    cbIncr      = pCtx->eflags.Bits.u1DF ? -(OP_SIZE / 8) : (OP_SIZE / 8);
    ADDR_TYPE       uSrcAddrReg = pCtx->ADDR_rSI;
    ADDR_TYPE       uDstAddrReg = pCtx->ADDR_rDI;

    /*
     * The loop.
     */
    do
    {
        /*
         * Do segmentation and virtual page stuff.
         */
#if ADDR_SIZE != 64
        ADDR2_TYPE  uVirtSrcAddr = (uint32_t)pSrcHid->u64Base    + uSrcAddrReg;
        ADDR2_TYPE  uVirtDstAddr = (uint32_t)pCtx->esHid.u64Base + uDstAddrReg;
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
            && uDstAddrReg < pCtx->esHid.u32Limit
            && uDstAddrReg + (cLeftPage * (OP_SIZE / 8)) <= pCtx->esHid.u32Limit
#endif
           )
        {
            RTGCPHYS GCPhysSrcMem;
            rcStrict = iemMemPageTranslateAndCheckAccess(pIemCpu, uVirtSrcAddr, IEM_ACCESS_DATA_R, &GCPhysSrcMem);
            if (rcStrict != VINF_SUCCESS)
                break;

            RTGCPHYS GCPhysDstMem;
            rcStrict = iemMemPageTranslateAndCheckAccess(pIemCpu, uVirtDstAddr, IEM_ACCESS_DATA_W, &GCPhysDstMem);
            if (rcStrict != VINF_SUCCESS)
                break;

            /*
             * If we can map the page without trouble, do a block processing
             * until the end of the current page.
             */
            OP_TYPE *puDstMem;
            rcStrict = iemMemPageMap(pIemCpu, GCPhysDstMem, IEM_ACCESS_DATA_W, (void **)&puDstMem);
            if (rcStrict == VINF_SUCCESS)
            {
                OP_TYPE const *puSrcMem;
                rcStrict = iemMemPageMap(pIemCpu, GCPhysSrcMem, IEM_ACCESS_DATA_W, (void **)&puSrcMem);
                if (rcStrict == VINF_SUCCESS)
                {
                    /* Perform the operation. */
                    memcpy(puDstMem, puSrcMem, cLeftPage * (OP_SIZE / 8));

                    /* Update the registers. */
                    uSrcAddrReg += cLeftPage * cbIncr;
                    uDstAddrReg += cLeftPage * cbIncr;
                    uCounterReg -= cLeftPage;
                    continue;
                }
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
                break;
            rcStrict = RT_CONCAT(iemMemStoreDataU,OP_SIZE)(pIemCpu, X86_SREG_ES, uDstAddrReg, uValue);
            if (rcStrict != VINF_SUCCESS)
                break;

            uSrcAddrReg += cbIncr;
            uDstAddrReg += cbIncr;
            uCounterReg--;
            cLeftPage--;
        } while ((int32_t)cLeftPage > 0);
        if (rcStrict != VINF_SUCCESS)
            break;
    } while (uCounterReg != 0);

    /*
     * Update the registers.
     */
    pCtx->ADDR_rCX = uCounterReg;
    pCtx->ADDR_rDI = uDstAddrReg;
    pCtx->ADDR_rSI = uSrcAddrReg;
    if (rcStrict == VINF_SUCCESS)
        iemRegAddToRip(pIemCpu, cbInstr);

    return rcStrict;
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
        return VINF_SUCCESS;

    VBOXSTRICTRC rcStrict = iemMemSegCheckWriteAccessEx(pIemCpu, &pCtx->esHid, X86_SREG_ES);
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
        ADDR2_TYPE  uVirtAddr = (uint32_t)pCtx->esHid.u64Base + uAddrReg;
#else
        uint64_t    uVirtAddr = uAddrReg;
#endif
        uint32_t    cLeftPage = (PAGE_SIZE - (uVirtAddr & PAGE_OFFSET_MASK)) / (OP_SIZE / 8);
        if (cLeftPage > uCounterReg)
            cLeftPage = uCounterReg;
        if (   cLeftPage > 0 /* can be null if unaligned, do one fallback round. */
            && cbIncr > 0    /** @todo Implement reverse direction string ops. */
#if ADDR_SIZE != 64
            && uAddrReg < pCtx->esHid.u32Limit
            && uAddrReg + (cLeftPage * (OP_SIZE / 8)) <= pCtx->esHid.u32Limit
#endif
           )
        {
            RTGCPHYS GCPhysMem;
            rcStrict = iemMemPageTranslateAndCheckAccess(pIemCpu, uVirtAddr, IEM_ACCESS_DATA_W, &GCPhysMem);
            if (rcStrict != VINF_SUCCESS)
                break;

            /*
             * If we can map the page without trouble, do a block processing
             * until the end of the current page.
             */
            OP_TYPE *puMem;
            rcStrict = iemMemPageMap(pIemCpu, GCPhysMem, IEM_ACCESS_DATA_W, (void **)&puMem);
            if (rcStrict == VINF_SUCCESS)
            {
                /* Update the regs first so we can loop on cLeftPage. */
                uCounterReg -= cLeftPage;
                uAddrReg    += cLeftPage * cbIncr;

                /* Do the memsetting. */
#if OP_SIZE == 8
                memset(puMem, uValue, cLeftPage);
/*#elif OP_SIZE == 32
                ASMMemFill32(puMem, cLeftPage * (OP_SIZE / 8), uValue);*/
#else
                while (cLeftPage-- > 0)
                    *puMem++ = uValue;
#endif

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
                break;
            uAddrReg += cbIncr;
            uCounterReg--;
            cLeftPage--;
        } while ((int32_t)cLeftPage > 0);
        if (rcStrict != VINF_SUCCESS)
            break;
    } while (uCounterReg != 0);

    /*
     * Update the registers.
     */
    pCtx->ADDR_rCX = uCounterReg;
    pCtx->ADDR_rDI = uAddrReg;
    if (rcStrict == VINF_SUCCESS)
        iemRegAddToRip(pIemCpu, cbInstr);

    return rcStrict;
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
    rcStrict = IOMIOPortRead(pVM, pCtx->dx, &u32Value, OP_SIZE / 8);
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
            AssertLogRelFailedReturn(VERR_INTERNAL_ERROR_3);
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
        return VINF_SUCCESS;

    rcStrict = iemMemSegCheckWriteAccessEx(pIemCpu, &pCtx->esHid, X86_SREG_ES);
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
        ADDR2_TYPE  uVirtAddr = (uint32_t)pCtx->esHid.u64Base + uAddrReg;
#else
        uint64_t    uVirtAddr = uAddrReg;
#endif
        uint32_t    cLeftPage = (PAGE_SIZE - (uVirtAddr & PAGE_OFFSET_MASK)) / (OP_SIZE / 8);
        if (cLeftPage > uCounterReg)
            cLeftPage = uCounterReg;
        if (   cLeftPage > 0 /* can be null if unaligned, do one fallback round. */
            && cbIncr > 0    /** @todo Implement reverse direction string ops. */
#if ADDR_SIZE != 64
            && uAddrReg < pCtx->esHid.u32Limit
            && uAddrReg + (cLeftPage * (OP_SIZE / 8)) <= pCtx->esHid.u32Limit
#endif
           )
        {
            RTGCPHYS GCPhysMem;
            rcStrict = iemMemPageTranslateAndCheckAccess(pIemCpu, uVirtAddr, IEM_ACCESS_DATA_W, &GCPhysMem);
            if (rcStrict != VINF_SUCCESS)
                break;

            /*
             * If we can map the page without trouble, we would've liked to use
             * an string I/O method to do the work, but the current IOM
             * interface doesn't match our current approach. So, do a regular
             * loop instead.
             */
            /** @todo Change the I/O manager interface to make use of
             *        mapped buffers instead of leaving those bits to the
             *        device implementation? */
            OP_TYPE *puMem;
            rcStrict = iemMemPageMap(pIemCpu, GCPhysMem, IEM_ACCESS_DATA_W, (void **)&puMem);
            if (rcStrict == VINF_SUCCESS)
            {
                while (cLeftPage-- > 0)
                {
                    uint32_t u32Value;
                    rcStrict = IOMIOPortRead(pVM, u16Port, &u32Value, OP_SIZE / 8);
                    if (!IOM_SUCCESS(rcStrict))
                        break;
                    *puMem++     = (OP_TYPE)u32Value;
                    uAddrReg    += cbIncr;
                    uCounterReg -= 1;

                    if (rcStrict != VINF_SUCCESS)
                    {
                        /** @todo massage rc */
                        break;
                    }
                }
                if (rcStrict != VINF_SUCCESS)
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
                break;

            uint32_t u32Value;
            rcStrict = IOMIOPortRead(pVM, u16Port, &u32Value, OP_SIZE / 8);
            if (!IOM_SUCCESS(rcStrict))
                break;

            VBOXSTRICTRC rcStrict2 = iemMemCommitAndUnmap(pIemCpu, puMem, IEM_ACCESS_DATA_W);
            AssertLogRelBreakStmt(rcStrict2 == VINF_SUCCESS, rcStrict = VERR_INTERNAL_ERROR_3); /* See non-rep version. */

            uAddrReg += cbIncr;
            uCounterReg--;
            cLeftPage--;
            if (rcStrict != VINF_SUCCESS)
            {
                /** @todo massage IOM status codes! */
                break;
            }
        } while ((int32_t)cLeftPage > 0);
        if (rcStrict != VINF_SUCCESS)
            break;
    } while (uCounterReg != 0);

    /*
     * Update the registers.
     */
    pCtx->ADDR_rCX = uCounterReg;
    pCtx->ADDR_rDI = uAddrReg;
    if (rcStrict == VINF_SUCCESS)
        iemRegAddToRip(pIemCpu, cbInstr);

    return rcStrict;
}


/**
 * Implements 'OUTS' (no rep)
 */
IEM_CIMPL_DEF_0(RT_CONCAT4(iemCImpl_outs_op,OP_SIZE,_addr,ADDR_SIZE))
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
    rcStrict = RT_CONCAT(iemMemFetchDataU,OP_SIZE)(pIemCpu, &uValue, X86_SREG_ES, pCtx->ADDR_rDI);
    if (rcStrict == VINF_SUCCESS)
    {
        rcStrict = IOMIOPortWrite(pVM, pCtx->dx, uValue, OP_SIZE / 8);
        if (IOM_SUCCESS(rcStrict))
        {
            if (!pCtx->eflags.Bits.u1DF)
                pCtx->ADDR_rDI += OP_SIZE / 8;
            else
                pCtx->ADDR_rDI -= OP_SIZE / 8;
            iemRegAddToRip(pIemCpu, cbInstr);
            /** @todo massage IOM status codes. */
        }
    }
    return rcStrict;
}

/**
 * Implements 'REP OUTS'.
 */
IEM_CIMPL_DEF_0(RT_CONCAT4(iemCImpl_rep_outs_op,OP_SIZE,_addr,ADDR_SIZE))
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
        return VINF_SUCCESS;

    rcStrict = iemMemSegCheckReadAccessEx(pIemCpu, &pCtx->esHid, X86_SREG_ES);
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
        ADDR2_TYPE  uVirtAddr = (uint32_t)pCtx->esHid.u64Base + uAddrReg;
#else
        uint64_t    uVirtAddr = uAddrReg;
#endif
        uint32_t    cLeftPage = (PAGE_SIZE - (uVirtAddr & PAGE_OFFSET_MASK)) / (OP_SIZE / 8);
        if (cLeftPage > uCounterReg)
            cLeftPage = uCounterReg;
        if (   cLeftPage > 0 /* can be null if unaligned, do one fallback round. */
            && cbIncr > 0    /** @todo Implement reverse direction string ops. */
#if ADDR_SIZE != 64
            && uAddrReg < pCtx->esHid.u32Limit
            && uAddrReg + (cLeftPage * (OP_SIZE / 8)) <= pCtx->esHid.u32Limit
#endif
           )
        {
            RTGCPHYS GCPhysMem;
            rcStrict = iemMemPageTranslateAndCheckAccess(pIemCpu, uVirtAddr, IEM_ACCESS_DATA_R, &GCPhysMem);
            if (rcStrict != VINF_SUCCESS)
                break;

            /*
             * If we can map the page without trouble, we would've liked to use
             * an string I/O method to do the work, but the current IOM
             * interface doesn't match our current approach. So, do a regular
             * loop instead.
             */
            /** @todo Change the I/O manager interface to make use of
             *        mapped buffers instead of leaving those bits to the
             *        device implementation? */
            OP_TYPE const *puMem;
            rcStrict = iemMemPageMap(pIemCpu, GCPhysMem, IEM_ACCESS_DATA_R, (void **)&puMem);
            if (rcStrict == VINF_SUCCESS)
            {
                while (cLeftPage-- > 0)
                {
                    uint32_t u32Value = *puMem++;
                    rcStrict = IOMIOPortWrite(pVM, u16Port, u32Value, OP_SIZE / 8);
                    if (!IOM_SUCCESS(rcStrict))
                        break;
                    uAddrReg    += cbIncr;
                    uCounterReg -= 1;

                    if (rcStrict != VINF_SUCCESS)
                    {
                        /** @todo massage IOM rc */
                        break;
                    }
                }
                if (rcStrict != VINF_SUCCESS)
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
         *
         * Note! We ASSUME the CPU will raise #PF or #GP before access the
         *       I/O port, otherwise it wouldn't really be restartable.
         */
        /** @todo investigate what the CPU actually does with \#PF/\#GP
         *        during INS. */
        do
        {
            OP_TYPE uValue;
            rcStrict = RT_CONCAT(iemMemFetchDataU,OP_SIZE)(pIemCpu, &uValue, X86_SREG_ES, uAddrReg);
            if (rcStrict != VINF_SUCCESS)
                break;

            rcStrict = IOMIOPortWrite(pVM, u16Port, uValue, OP_SIZE / 8);
            if (!IOM_SUCCESS(rcStrict))
                break;

            uAddrReg += cbIncr;
            uCounterReg--;
            cLeftPage--;
            if (rcStrict != VINF_SUCCESS)
            {
                /** @todo massage IOM status codes! */
                break;
            }
        } while ((int32_t)cLeftPage > 0);
        if (rcStrict != VINF_SUCCESS)
            break;
    } while (uCounterReg != 0);

    /*
     * Update the registers.
     */
    pCtx->ADDR_rCX = uCounterReg;
    pCtx->ADDR_rDI = uAddrReg;
    if (rcStrict == VINF_SUCCESS)
        iemRegAddToRip(pIemCpu, cbInstr);

    return rcStrict;
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

