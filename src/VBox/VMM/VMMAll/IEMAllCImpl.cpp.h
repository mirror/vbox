/* $Id$ */
/** @file
 * IEM - Instruction Implementation in C/C++ (code include).
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


/** @name Misc Helpers
 * @{
 */

/**
 * Checks if we are allowed to access the given I/O port, raising the
 * appropriate exceptions if we aren't (or if the I/O bitmap is not
 * accessible).
 *
 * @returns Strict VBox status code.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pCtx                The register context.
 * @param   u16Port             The port number.
 * @param   cbOperand           The operand size.
 */
DECLINLINE(VBOXSTRICTRC) iemHlpCheckPortIOPermission(PIEMCPU pIemCpu, PCCPUMCTX pCtx, uint16_t u16Port, uint8_t cbOperand)
{
    if (   (pCtx->cr0 & X86_CR0_PE)
        && (    pIemCpu->uCpl > pCtx->eflags.Bits.u2IOPL
            ||  pCtx->eflags.Bits.u1VM) )
    {
        /** @todo I/O port permission bitmap check */
        AssertFailedReturn(VERR_NOT_IMPLEMENTED);
    }
    return VINF_SUCCESS;
}


#if 0
/**
 * Calculates the parity bit.
 *
 * @returns true if the bit is set, false if not.
 * @param   u8Result            The least significant byte of the result.
 */
static bool iemHlpCalcParityFlag(uint8_t u8Result)
{
    /*
     * Parity is set if the number of bits in the least significant byte of
     * the result is even.
     */
    uint8_t cBits;
    cBits  = u8Result & 1;              /* 0 */
    u8Result >>= 1;
    cBits += u8Result & 1;
    u8Result >>= 1;
    cBits += u8Result & 1;
    u8Result >>= 1;
    cBits += u8Result & 1;
    u8Result >>= 1;
    cBits += u8Result & 1;              /* 4 */
    u8Result >>= 1;
    cBits += u8Result & 1;
    u8Result >>= 1;
    cBits += u8Result & 1;
    u8Result >>= 1;
    cBits += u8Result & 1;
    return !(cBits & 1);
}
#endif /* not used */


/**
 * Updates the specified flags according to a 8-bit result.
 *
 * @param   pIemCpu             The.
 * @param   u8Result            The result to set the flags according to.
 * @param   fToUpdate           The flags to update.
 * @param   fUndefined          The flags that are specified as undefined.
 */
static void iemHlpUpdateArithEFlagsU8(PIEMCPU pIemCpu, uint8_t u8Result, uint32_t fToUpdate, uint32_t fUndefined)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    uint32_t fEFlags = pCtx->eflags.u;
    iemAImpl_test_u8(&u8Result, u8Result, &fEFlags);
    pCtx->eflags.u &= ~(fToUpdate | fUndefined);
    pCtx->eflags.u |= (fToUpdate | fUndefined) & fEFlags;
}


/** @} */

/** @name C Implementations
 * @{
 */

/**
 * Implements a 16-bit popa.
 */
IEM_CIMPL_DEF_0(iemCImpl_popa_16)
{
    PCPUMCTX        pCtx        = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR         GCPtrStart  = iemRegGetEffRsp(pCtx);
    RTGCPTR         GCPtrLast   = GCPtrStart + 15;
    VBOXSTRICTRC    rcStrict;

    /*
     * The docs are a bit hard to comprehend here, but it looks like we wrap
     * around in real mode as long as none of the individual "popa" crosses the
     * end of the stack segment.  In protected mode we check the whole access
     * in one go.  For efficiency, only do the word-by-word thing if we're in
     * danger of wrapping around.
     */
    /** @todo do popa boundary / wrap-around checks.  */
    if (RT_UNLIKELY(   IEM_IS_REAL_OR_V86_MODE(pIemCpu)
                    && (pCtx->csHid.u32Limit < GCPtrLast)) ) /* ASSUMES 64-bit RTGCPTR */
    {
        /* word-by-word */
        RTUINT64U TmpRsp;
        TmpRsp.u = pCtx->rsp;
        rcStrict = iemMemStackPopU16Ex(pIemCpu, &pCtx->di, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPopU16Ex(pIemCpu, &pCtx->si, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPopU16Ex(pIemCpu, &pCtx->bp, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
        {
            iemRegAddToRspEx(&TmpRsp, 2, pCtx); /* sp */
            rcStrict = iemMemStackPopU16Ex(pIemCpu, &pCtx->bx, &TmpRsp);
        }
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPopU16Ex(pIemCpu, &pCtx->dx, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPopU16Ex(pIemCpu, &pCtx->cx, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPopU16Ex(pIemCpu, &pCtx->ax, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
        {
            pCtx->rsp = TmpRsp.u;
            iemRegAddToRip(pIemCpu, cbInstr);
        }
    }
    else
    {
        uint16_t const *pa16Mem = NULL;
        rcStrict = iemMemMap(pIemCpu, (void **)&pa16Mem, 16, X86_SREG_SS, GCPtrStart, IEM_ACCESS_STACK_R);
        if (rcStrict == VINF_SUCCESS)
        {
            pCtx->di = pa16Mem[7 - X86_GREG_xDI];
            pCtx->si = pa16Mem[7 - X86_GREG_xSI];
            pCtx->bp = pa16Mem[7 - X86_GREG_xBP];
            /* skip sp */
            pCtx->bx = pa16Mem[7 - X86_GREG_xBX];
            pCtx->dx = pa16Mem[7 - X86_GREG_xDX];
            pCtx->cx = pa16Mem[7 - X86_GREG_xCX];
            pCtx->ax = pa16Mem[7 - X86_GREG_xAX];
            rcStrict = iemMemCommitAndUnmap(pIemCpu, (void *)pa16Mem, IEM_ACCESS_STACK_R);
            if (rcStrict == VINF_SUCCESS)
            {
                iemRegAddToRsp(pCtx, 16);
                iemRegAddToRip(pIemCpu, cbInstr);
            }
        }
    }
    return rcStrict;
}


/**
 * Implements a 32-bit popa.
 */
IEM_CIMPL_DEF_0(iemCImpl_popa_32)
{
    PCPUMCTX        pCtx        = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR         GCPtrStart  = iemRegGetEffRsp(pCtx);
    RTGCPTR         GCPtrLast   = GCPtrStart + 31;
    VBOXSTRICTRC    rcStrict;

    /*
     * The docs are a bit hard to comprehend here, but it looks like we wrap
     * around in real mode as long as none of the individual "popa" crosses the
     * end of the stack segment.  In protected mode we check the whole access
     * in one go.  For efficiency, only do the word-by-word thing if we're in
     * danger of wrapping around.
     */
    /** @todo do popa boundary / wrap-around checks.  */
    if (RT_UNLIKELY(   IEM_IS_REAL_OR_V86_MODE(pIemCpu)
                    && (pCtx->csHid.u32Limit < GCPtrLast)) ) /* ASSUMES 64-bit RTGCPTR */
    {
        /* word-by-word */
        RTUINT64U TmpRsp;
        TmpRsp.u = pCtx->rsp;
        rcStrict = iemMemStackPopU32Ex(pIemCpu, &pCtx->edi, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPopU32Ex(pIemCpu, &pCtx->esi, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPopU32Ex(pIemCpu, &pCtx->ebp, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
        {
            iemRegAddToRspEx(&TmpRsp, 2, pCtx); /* sp */
            rcStrict = iemMemStackPopU32Ex(pIemCpu, &pCtx->ebx, &TmpRsp);
        }
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPopU32Ex(pIemCpu, &pCtx->edx, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPopU32Ex(pIemCpu, &pCtx->ecx, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPopU32Ex(pIemCpu, &pCtx->eax, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
        {
#if 1  /** @todo what actually happens with the high bits when we're in 16-bit mode? */
            pCtx->rdi &= UINT32_MAX;
            pCtx->rsi &= UINT32_MAX;
            pCtx->rbp &= UINT32_MAX;
            pCtx->rbx &= UINT32_MAX;
            pCtx->rdx &= UINT32_MAX;
            pCtx->rcx &= UINT32_MAX;
            pCtx->rax &= UINT32_MAX;
#endif
            pCtx->rsp = TmpRsp.u;
            iemRegAddToRip(pIemCpu, cbInstr);
        }
    }
    else
    {
        uint32_t const *pa32Mem;
        rcStrict = iemMemMap(pIemCpu, (void **)&pa32Mem, 32, X86_SREG_SS, GCPtrStart, IEM_ACCESS_STACK_R);
        if (rcStrict == VINF_SUCCESS)
        {
            pCtx->rdi = pa32Mem[7 - X86_GREG_xDI];
            pCtx->rsi = pa32Mem[7 - X86_GREG_xSI];
            pCtx->rbp = pa32Mem[7 - X86_GREG_xBP];
            /* skip esp */
            pCtx->rbx = pa32Mem[7 - X86_GREG_xBX];
            pCtx->rdx = pa32Mem[7 - X86_GREG_xDX];
            pCtx->rcx = pa32Mem[7 - X86_GREG_xCX];
            pCtx->rax = pa32Mem[7 - X86_GREG_xAX];
            rcStrict = iemMemCommitAndUnmap(pIemCpu, (void *)pa32Mem, IEM_ACCESS_STACK_R);
            if (rcStrict == VINF_SUCCESS)
            {
                iemRegAddToRsp(pCtx, 32);
                iemRegAddToRip(pIemCpu, cbInstr);
            }
        }
    }
    return rcStrict;
}


/**
 * Implements a 16-bit pusha.
 */
IEM_CIMPL_DEF_0(iemCImpl_pusha_16)
{
    PCPUMCTX        pCtx        = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR         GCPtrTop    = iemRegGetEffRsp(pCtx);
    RTGCPTR         GCPtrBottom = GCPtrTop - 15;
    VBOXSTRICTRC    rcStrict;

    /*
     * The docs are a bit hard to comprehend here, but it looks like we wrap
     * around in real mode as long as none of the individual "pushd" crosses the
     * end of the stack segment.  In protected mode we check the whole access
     * in one go.  For efficiency, only do the word-by-word thing if we're in
     * danger of wrapping around.
     */
    /** @todo do pusha boundary / wrap-around checks.  */
    if (RT_UNLIKELY(   GCPtrBottom > GCPtrTop
                    && IEM_IS_REAL_OR_V86_MODE(pIemCpu) ) )
    {
        /* word-by-word */
        RTUINT64U TmpRsp;
        TmpRsp.u = pCtx->rsp;
        rcStrict = iemMemStackPushU16Ex(pIemCpu, pCtx->ax, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU16Ex(pIemCpu, pCtx->cx, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU16Ex(pIemCpu, pCtx->dx, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU16Ex(pIemCpu, pCtx->bx, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU16Ex(pIemCpu, pCtx->sp, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU16Ex(pIemCpu, pCtx->bp, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU16Ex(pIemCpu, pCtx->si, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU16Ex(pIemCpu, pCtx->di, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
        {
            pCtx->rsp = TmpRsp.u;
            iemRegAddToRip(pIemCpu, cbInstr);
        }
    }
    else
    {
        GCPtrBottom--;
        uint16_t *pa16Mem = NULL;
        rcStrict = iemMemMap(pIemCpu, (void **)&pa16Mem, 16, X86_SREG_SS, GCPtrBottom, IEM_ACCESS_STACK_W);
        if (rcStrict == VINF_SUCCESS)
        {
            pa16Mem[7 - X86_GREG_xDI] = pCtx->di;
            pa16Mem[7 - X86_GREG_xSI] = pCtx->si;
            pa16Mem[7 - X86_GREG_xBP] = pCtx->bp;
            pa16Mem[7 - X86_GREG_xSP] = pCtx->sp;
            pa16Mem[7 - X86_GREG_xBX] = pCtx->bx;
            pa16Mem[7 - X86_GREG_xDX] = pCtx->dx;
            pa16Mem[7 - X86_GREG_xCX] = pCtx->cx;
            pa16Mem[7 - X86_GREG_xAX] = pCtx->ax;
            rcStrict = iemMemCommitAndUnmap(pIemCpu, (void *)pa16Mem, IEM_ACCESS_STACK_W);
            if (rcStrict == VINF_SUCCESS)
            {
                iemRegSubFromRsp(pCtx, 16);
                iemRegAddToRip(pIemCpu, cbInstr);
            }
        }
    }
    return rcStrict;
}


/**
 * Implements a 32-bit pusha.
 */
IEM_CIMPL_DEF_0(iemCImpl_pusha_32)
{
    PCPUMCTX        pCtx        = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR         GCPtrTop    = iemRegGetEffRsp(pCtx);
    RTGCPTR         GCPtrBottom = GCPtrTop - 31;
    VBOXSTRICTRC    rcStrict;

    /*
     * The docs are a bit hard to comprehend here, but it looks like we wrap
     * around in real mode as long as none of the individual "pusha" crosses the
     * end of the stack segment.  In protected mode we check the whole access
     * in one go.  For efficiency, only do the word-by-word thing if we're in
     * danger of wrapping around.
     */
    /** @todo do pusha boundary / wrap-around checks.  */
    if (RT_UNLIKELY(   GCPtrBottom > GCPtrTop
                    && IEM_IS_REAL_OR_V86_MODE(pIemCpu) ) )
    {
        /* word-by-word */
        RTUINT64U TmpRsp;
        TmpRsp.u = pCtx->rsp;
        rcStrict = iemMemStackPushU32Ex(pIemCpu, pCtx->eax, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU32Ex(pIemCpu, pCtx->ecx, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU32Ex(pIemCpu, pCtx->edx, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU32Ex(pIemCpu, pCtx->ebx, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU32Ex(pIemCpu, pCtx->esp, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU32Ex(pIemCpu, pCtx->ebp, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU32Ex(pIemCpu, pCtx->esi, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU32Ex(pIemCpu, pCtx->edi, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
        {
            pCtx->rsp = TmpRsp.u;
            iemRegAddToRip(pIemCpu, cbInstr);
        }
    }
    else
    {
        GCPtrBottom--;
        uint32_t *pa32Mem;
        rcStrict = iemMemMap(pIemCpu, (void **)&pa32Mem, 32, X86_SREG_SS, GCPtrBottom, IEM_ACCESS_STACK_W);
        if (rcStrict == VINF_SUCCESS)
        {
            pa32Mem[7 - X86_GREG_xDI] = pCtx->edi;
            pa32Mem[7 - X86_GREG_xSI] = pCtx->esi;
            pa32Mem[7 - X86_GREG_xBP] = pCtx->ebp;
            pa32Mem[7 - X86_GREG_xSP] = pCtx->esp;
            pa32Mem[7 - X86_GREG_xBX] = pCtx->ebx;
            pa32Mem[7 - X86_GREG_xDX] = pCtx->edx;
            pa32Mem[7 - X86_GREG_xCX] = pCtx->ecx;
            pa32Mem[7 - X86_GREG_xAX] = pCtx->eax;
            rcStrict = iemMemCommitAndUnmap(pIemCpu, pa32Mem, IEM_ACCESS_STACK_W);
            if (rcStrict == VINF_SUCCESS)
            {
                iemRegSubFromRsp(pCtx, 32);
                iemRegAddToRip(pIemCpu, cbInstr);
            }
        }
    }
    return rcStrict;
}


/**
 * Implements pushf.
 *
 *
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_1(iemCImpl_pushf, IEMMODE, enmEffOpSize)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * If we're in V8086 mode some care is required (which is why we're in
     * doing this in a C implementation).
     */
    uint32_t fEfl = pCtx->eflags.u;
    if (   (fEfl & X86_EFL_VM)
        && X86_EFL_GET_IOPL(fEfl) != 3 )
    {
        Assert(pCtx->cr0 & X86_CR0_PE);
        if (   enmEffOpSize != IEMMODE_16BIT
            || !(pCtx->cr4 & X86_CR4_VME))
            return iemRaiseGeneralProtectionFault0(pIemCpu);
        fEfl &= ~X86_EFL_IF;          /* (RF and VM are out of range) */
        fEfl |= (fEfl & X86_EFL_VIF) >> (19 - 9);
        return iemMemStackPushU16(pIemCpu, (uint16_t)fEfl);
    }

    /*
     * Ok, clear RF and VM and push the flags.
     */
    fEfl &= ~(X86_EFL_RF | X86_EFL_VM);

    VBOXSTRICTRC rcStrict;
    switch (enmEffOpSize)
    {
        case IEMMODE_16BIT:
            rcStrict = iemMemStackPushU16(pIemCpu, (uint16_t)fEfl);
            break;
        case IEMMODE_32BIT:
            rcStrict = iemMemStackPushU32(pIemCpu, fEfl);
            break;
        case IEMMODE_64BIT:
            rcStrict = iemMemStackPushU64(pIemCpu, fEfl);
            break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * Implements popf.
 *
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_1(iemCImpl_popf, IEMMODE, enmEffOpSize)
{
    PCPUMCTX        pCtx    = pIemCpu->CTX_SUFF(pCtx);
    uint32_t const  fEflOld = pCtx->eflags.u;
    VBOXSTRICTRC    rcStrict;
    uint32_t        fEflNew;

    /*
     * V8086 is special as usual.
     */
    if (fEflOld & X86_EFL_VM)
    {
        /*
         * Almost anything goes if IOPL is 3.
         */
        if (X86_EFL_GET_IOPL(fEflOld) == 3)
        {
            switch (enmEffOpSize)
            {
                case IEMMODE_16BIT:
                {
                    uint16_t u16Value;
                    rcStrict = iemMemStackPopU16(pIemCpu, &u16Value);
                    if (rcStrict != VINF_SUCCESS)
                        return rcStrict;
                    fEflNew = u16Value | (fEflOld & UINT32_C(0xffff0000));
                    break;
                }
                case IEMMODE_32BIT:
                    rcStrict = iemMemStackPopU32(pIemCpu, &fEflNew);
                    if (rcStrict != VINF_SUCCESS)
                        return rcStrict;
                    break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }

            fEflNew &=   X86_EFL_POPF_BITS & ~(X86_EFL_IOPL);
            fEflNew |= ~(X86_EFL_POPF_BITS & ~(X86_EFL_IOPL)) & fEflOld;
        }
        /*
         * Interrupt flag virtualization with CR4.VME=1.
         */
        else if (   enmEffOpSize == IEMMODE_16BIT
                 && (pCtx->cr4 & X86_CR4_VME) )
        {
            uint16_t    u16Value;
            RTUINT64U   TmpRsp;
            TmpRsp.u = pCtx->rsp;
            rcStrict = iemMemStackPopU16Ex(pIemCpu, &u16Value, &TmpRsp);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;

            /** @todo Is the popf VME #GP(0) delivered after updating RSP+RIP
             *        or before? */
            if (    (   (u16Value & X86_EFL_IF)
                     && (fEflOld  & X86_EFL_VIP))
                ||  (u16Value & X86_EFL_TF) )
                return iemRaiseGeneralProtectionFault0(pIemCpu);

            fEflNew = u16Value | (fEflOld & UINT32_C(0xffff0000) & ~X86_EFL_VIF);
            fEflNew |= (fEflNew & X86_EFL_IF) << (19 - 9);
            fEflNew &=   X86_EFL_POPF_BITS & ~(X86_EFL_IOPL | X86_EFL_IF);
            fEflNew |= ~(X86_EFL_POPF_BITS & ~(X86_EFL_IOPL | X86_EFL_IF)) & fEflOld;

            pCtx->rsp = TmpRsp.u;
        }
        else
            return iemRaiseGeneralProtectionFault0(pIemCpu);

    }
    /*
     * Not in V8086 mode.
     */
    else
    {
        /* Pop the flags. */
        switch (enmEffOpSize)
        {
            case IEMMODE_16BIT:
            {
                uint16_t u16Value;
                rcStrict = iemMemStackPopU16(pIemCpu, &u16Value);
                if (rcStrict != VINF_SUCCESS)
                    return rcStrict;
                fEflNew = u16Value | (fEflOld & UINT32_C(0xffff0000));
                break;
            }
            case IEMMODE_32BIT:
            case IEMMODE_64BIT:
                rcStrict = iemMemStackPopU32(pIemCpu, &fEflNew);
                if (rcStrict != VINF_SUCCESS)
                    return rcStrict;
                break;
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }

        /* Merge them with the current flags. */
        if (   (fEflNew & (X86_EFL_IOPL | X86_EFL_IF)) == (fEflOld & (X86_EFL_IOPL | X86_EFL_IF))
            || pIemCpu->uCpl == 0)
        {
            fEflNew &=  X86_EFL_POPF_BITS;
            fEflNew |= ~X86_EFL_POPF_BITS & fEflOld;
        }
        else if (pIemCpu->uCpl <= X86_EFL_GET_IOPL(fEflOld))
        {
            fEflNew &=   X86_EFL_POPF_BITS & ~(X86_EFL_IOPL);
            fEflNew |= ~(X86_EFL_POPF_BITS & ~(X86_EFL_IOPL)) & fEflOld;
        }
        else
        {
            fEflNew &=   X86_EFL_POPF_BITS & ~(X86_EFL_IOPL | X86_EFL_IF);
            fEflNew |= ~(X86_EFL_POPF_BITS & ~(X86_EFL_IOPL | X86_EFL_IF)) & fEflOld;
        }
    }

    /*
     * Commit the flags.
     */
    Assert(fEflNew & RT_BIT_32(1));
    pCtx->eflags.u = fEflNew;
    iemRegAddToRip(pIemCpu, cbInstr);

    return VINF_SUCCESS;
}


/**
 * Implements an indirect call.
 *
 * @param   uNewPC          The new program counter (RIP) value (loaded from the
 *                          operand).
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_1(iemCImpl_call_16, uint16_t, uNewPC)
{
    PCPUMCTX pCtx   = pIemCpu->CTX_SUFF(pCtx);
    uint16_t uOldPC = pCtx->ip + cbInstr;
    if (uNewPC > pCtx->csHid.u32Limit)
        return iemRaiseGeneralProtectionFault0(pIemCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU16(pIemCpu, uOldPC);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    pCtx->rip = uNewPC;
    return VINF_SUCCESS;

}


/**
 * Implements a 16-bit relative call.
 *
 * @param   offDisp      The displacment offset.
 */
IEM_CIMPL_DEF_1(iemCImpl_call_rel_16, int16_t, offDisp)
{
    PCPUMCTX pCtx   = pIemCpu->CTX_SUFF(pCtx);
    uint16_t uOldPC = pCtx->ip + cbInstr;
    uint16_t uNewPC = uOldPC + offDisp;
    if (uNewPC > pCtx->csHid.u32Limit)
        return iemRaiseGeneralProtectionFault0(pIemCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU16(pIemCpu, uOldPC);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    pCtx->rip = uNewPC;
    return VINF_SUCCESS;
}


/**
 * Implements a 32-bit indirect call.
 *
 * @param   uNewPC          The new program counter (RIP) value (loaded from the
 *                          operand).
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_1(iemCImpl_call_32, uint32_t, uNewPC)
{
    PCPUMCTX pCtx   = pIemCpu->CTX_SUFF(pCtx);
    uint32_t uOldPC = pCtx->eip + cbInstr;
    if (uNewPC > pCtx->csHid.u32Limit)
        return iemRaiseGeneralProtectionFault0(pIemCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU32(pIemCpu, uOldPC);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    pCtx->rip = uNewPC;
    return VINF_SUCCESS;

}


/**
 * Implements a 32-bit relative call.
 *
 * @param   offDisp      The displacment offset.
 */
IEM_CIMPL_DEF_1(iemCImpl_call_rel_32, int32_t, offDisp)
{
    PCPUMCTX pCtx   = pIemCpu->CTX_SUFF(pCtx);
    uint32_t uOldPC = pCtx->eip + cbInstr;
    uint32_t uNewPC = uOldPC + offDisp;
    if (uNewPC > pCtx->csHid.u32Limit)
        return iemRaiseGeneralProtectionFault0(pIemCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU32(pIemCpu, uOldPC);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    pCtx->rip = uNewPC;
    return VINF_SUCCESS;
}


/**
 * Implements a 64-bit indirect call.
 *
 * @param   uNewPC          The new program counter (RIP) value (loaded from the
 *                          operand).
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_1(iemCImpl_call_64, uint64_t, uNewPC)
{
    PCPUMCTX pCtx   = pIemCpu->CTX_SUFF(pCtx);
    uint64_t uOldPC = pCtx->rip + cbInstr;
    if (!IEM_IS_CANONICAL(uNewPC))
        return iemRaiseGeneralProtectionFault0(pIemCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU64(pIemCpu, uOldPC);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    pCtx->rip = uNewPC;
    return VINF_SUCCESS;

}


/**
 * Implements a 64-bit relative call.
 *
 * @param   offDisp      The displacment offset.
 */
IEM_CIMPL_DEF_1(iemCImpl_call_rel_64, int64_t, offDisp)
{
    PCPUMCTX pCtx   = pIemCpu->CTX_SUFF(pCtx);
    uint64_t uOldPC = pCtx->rip + cbInstr;
    uint64_t uNewPC = uOldPC + offDisp;
    if (!IEM_IS_CANONICAL(uNewPC))
        return iemRaiseNotCanonical(pIemCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU64(pIemCpu, uOldPC);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    pCtx->rip = uNewPC;
    return VINF_SUCCESS;
}


/**
 * Implements far jumps.
 *
 * @param   uSel            The selector.
 * @param   offSeg          The segment offset.
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_3(iemCImpl_FarJmp, uint16_t, uSel, uint32_t, offSeg, IEMMODE, enmEffOpSize)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Real mode and V8086 mode are easy.  The only snag seems to be that
     * CS.limit doesn't change and the limit check is done against the current
     * limit.
     */
    if (   pIemCpu->enmCpuMode == IEMMODE_16BIT
        && IEM_IS_REAL_OR_V86_MODE(pIemCpu))
    {
        if (offSeg > pCtx->csHid.u32Limit)
            return iemRaiseGeneralProtectionFault0(pIemCpu);

        if (enmEffOpSize == IEMMODE_16BIT) /** @todo WRONG, must pass this. */
            pCtx->rip       = offSeg;
        else
            pCtx->rip       = offSeg & UINT16_MAX;
        pCtx->cs            = uSel;
        pCtx->csHid.u64Base = (uint32_t)uSel << 4;
        /** @todo REM reset the accessed bit (see on jmp far16 after disabling
         *        PE.  Check with VT-x and AMD-V. */
#ifdef IEM_VERIFICATION_MODE
        pCtx->csHid.Attr.u  &= ~X86_SEL_TYPE_ACCESSED;
#endif
        return VINF_SUCCESS;
    }

    /*
     * Protected mode. Need to parse the specified descriptor...
     */
    if (!(uSel & (X86_SEL_MASK | X86_SEL_LDT)))
    {
        Log(("jmpf %04x:%08x -> invalid selector, #GP(0)\n", uSel, offSeg));
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    }

    /* Fetch the descriptor. */
    IEMSELDESC Desc;
    VBOXSTRICTRC rcStrict = iemMemFetchSelDesc(pIemCpu, &Desc, uSel);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    /* Is it there? */
    if (!Desc.Legacy.Gen.u1Present) /** @todo this is probably checked too early. Testcase! */
    {
        Log(("jmpf %04x:%08x -> segment not present\n", uSel, offSeg));
        return iemRaiseSelectorNotPresentBySelector(pIemCpu, uSel);
    }

    /*
     * Deal with it according to its type.
     */
    if (Desc.Legacy.Gen.u1DescType)
    {
        /* Only code segments. */
        if (!(Desc.Legacy.Gen.u4Type & X86_SEL_TYPE_CODE))
        {
            Log(("jmpf %04x:%08x -> not a code selector (u4Type=%#x).\n", uSel, offSeg, Desc.Legacy.Gen.u4Type));
            return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uSel);
        }

        /* L vs D. */
        if (   Desc.Legacy.Gen.u1Long
            && Desc.Legacy.Gen.u1DefBig
            && IEM_IS_LONG_MODE(pIemCpu))
        {
            Log(("jmpf %04x:%08x -> both L and D are set.\n", uSel, offSeg));
            return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uSel);
        }

        /* DPL/RPL/CPL check, where conforming segments makes a difference. */
        if (!(Desc.Legacy.Gen.u4Type & X86_SEL_TYPE_CONF))
        {
            if (Desc.Legacy.Gen.u2Dpl > pIemCpu->uCpl)
            {
                Log(("jmpf %04x:%08x -> DPL violation (conforming); DPL=%d CPL=%u\n",
                     uSel, offSeg, Desc.Legacy.Gen.u2Dpl, pIemCpu->uCpl));
                return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uSel);
            }
        }
        else
        {
            if (Desc.Legacy.Gen.u2Dpl != pIemCpu->uCpl)
            {
                Log(("jmpf %04x:%08x -> CPL != DPL; DPL=%d CPL=%u\n", uSel, offSeg, Desc.Legacy.Gen.u2Dpl, pIemCpu->uCpl));
                return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uSel);
            }
            if ((uSel & X86_SEL_RPL) > pIemCpu->uCpl)
            {
                Log(("jmpf %04x:%08x -> RPL > DPL; RPL=%d CPL=%u\n", uSel, offSeg, (uSel & X86_SEL_RPL), pIemCpu->uCpl));
                return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uSel);
            }
        }

        /* Limit check. (Should alternatively check for non-canonical addresses
           here, but that is ruled out by offSeg being 32-bit, right?) */
        uint64_t u64Base;
        uint32_t cbLimit = X86DESC_LIMIT(Desc.Legacy);
        if (Desc.Legacy.Gen.u1Granularity)
            cbLimit = (cbLimit << PAGE_SHIFT) | PAGE_OFFSET_MASK;
        if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
            u64Base = 0;
        else
        {
            if (offSeg > cbLimit)
            {
                Log(("jmpf %04x:%08x -> out of bounds (%#x)\n", uSel, offSeg, cbLimit));
                return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uSel);
            }
            u64Base = X86DESC_BASE(Desc.Legacy);
        }

        /*
         * Ok, everything checked out fine.  Now set the accessed bit before
         * committing the result into CS, CSHID and RIP.
         */
        if (!(Desc.Legacy.Gen.u4Type & X86_SEL_TYPE_ACCESSED))
        {
            rcStrict = iemMemMarkSelDescAccessed(pIemCpu, uSel);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
#ifdef IEM_VERIFICATION_MODE /** @todo check what VT-x and AMD-V does. */
            Desc.Legacy.Gen.u4Type |= X86_SEL_TYPE_ACCESSED;
#endif
        }

        /* commit */
        pCtx->rip = offSeg;
        pCtx->cs  = uSel & (X86_SEL_MASK | X86_SEL_LDT);
        pCtx->cs |= pIemCpu->uCpl; /** @todo is this right for conforming segs? or in general? */
        pCtx->csHid.Attr.u   = (Desc.Legacy.u >> (16+16+8)) & UINT32_C(0xf0ff);
        pCtx->csHid.u32Limit = cbLimit;
        pCtx->csHid.u64Base  = u64Base;
        /** @todo check if the hidden bits are loaded correctly for 64-bit
         *        mode.  */
        return VINF_SUCCESS;
    }

    /*
     * System selector.
     */
    if (IEM_IS_LONG_MODE(pIemCpu))
        switch (Desc.Legacy.Gen.u4Type)
        {
            case AMD64_SEL_TYPE_SYS_LDT:
            case AMD64_SEL_TYPE_SYS_TSS_AVAIL:
            case AMD64_SEL_TYPE_SYS_TSS_BUSY:
            case AMD64_SEL_TYPE_SYS_CALL_GATE:
            case AMD64_SEL_TYPE_SYS_INT_GATE:
            case AMD64_SEL_TYPE_SYS_TRAP_GATE:
                /* Call various functions to do the work. */
                AssertFailedReturn(VERR_NOT_IMPLEMENTED);
            default:
                Log(("jmpf %04x:%08x -> wrong sys selector (64-bit): %d\n", uSel, offSeg, Desc.Legacy.Gen.u4Type));
                return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uSel);

        }
    switch (Desc.Legacy.Gen.u4Type)
    {
        case X86_SEL_TYPE_SYS_286_TSS_AVAIL:
        case X86_SEL_TYPE_SYS_LDT:
        case X86_SEL_TYPE_SYS_286_CALL_GATE:
        case X86_SEL_TYPE_SYS_TASK_GATE:
        case X86_SEL_TYPE_SYS_286_INT_GATE:
        case X86_SEL_TYPE_SYS_286_TRAP_GATE:
        case X86_SEL_TYPE_SYS_386_TSS_AVAIL:
        case X86_SEL_TYPE_SYS_386_CALL_GATE:
        case X86_SEL_TYPE_SYS_386_INT_GATE:
        case X86_SEL_TYPE_SYS_386_TRAP_GATE:
            /* Call various functions to do the work. */
            AssertFailedReturn(VERR_NOT_IMPLEMENTED);

        case X86_SEL_TYPE_SYS_286_TSS_BUSY:
        case X86_SEL_TYPE_SYS_386_TSS_BUSY:
            /* Call various functions to do the work. */
            AssertFailedReturn(VERR_NOT_IMPLEMENTED);

        default:
            Log(("jmpf %04x:%08x -> wrong sys selector (32-bit): %d\n", uSel, offSeg, Desc.Legacy.Gen.u4Type));
            return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uSel);
    }
}


/**
 * Implements far calls.
 *
 * @param   uSel        The selector.
 * @param   offSeg      The segment offset.
 * @param   enmOpSize   The operand size (in case we need it).
 */
IEM_CIMPL_DEF_3(iemCImpl_callf, uint16_t, uSel, uint64_t, offSeg, IEMMODE, enmOpSize)
{
    PCPUMCTX        pCtx = pIemCpu->CTX_SUFF(pCtx);
    VBOXSTRICTRC    rcStrict;
    uint64_t        uNewRsp;
    void           *pvRet;

    /*
     * Real mode and V8086 mode are easy.  The only snag seems to be that
     * CS.limit doesn't change and the limit check is done against the current
     * limit.
     */
    if (   pIemCpu->enmCpuMode == IEMMODE_16BIT
        && IEM_IS_REAL_OR_V86_MODE(pIemCpu))
    {
        Assert(enmOpSize == IEMMODE_16BIT || enmOpSize == IEMMODE_32BIT);

        /* Check stack first - may #SS(0). */
        rcStrict = iemMemStackPushBeginSpecial(pIemCpu, enmOpSize == IEMMODE_32BIT ? 6 : 4,
                                               &pvRet, &uNewRsp);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;

        /* Check the target address range. */
        if (offSeg > UINT32_MAX)
            return iemRaiseGeneralProtectionFault0(pIemCpu);

        /* Everything is fine, push the return address. */
        if (enmOpSize == IEMMODE_16BIT)
        {
            ((uint16_t *)pvRet)[0] = pCtx->ip + cbInstr;
            ((uint16_t *)pvRet)[1] = pCtx->cs;
        }
        else
        {
            ((uint32_t *)pvRet)[0] = pCtx->eip + cbInstr;
            ((uint16_t *)pvRet)[3] = pCtx->cs;
        }
        rcStrict = iemMemStackPushCommitSpecial(pIemCpu, pvRet, uNewRsp);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;

        /* Branch. */
        pCtx->rip           = offSeg;
        pCtx->cs            = uSel;
        pCtx->csHid.u64Base = (uint32_t)uSel << 4;
        /** @todo Does REM reset the accessed bit here to? (See on jmp far16
         *        after disabling PE.) Check with VT-x and AMD-V. */
#ifdef IEM_VERIFICATION_MODE
        pCtx->csHid.Attr.u  &= ~X86_SEL_TYPE_ACCESSED;
#endif
        return VINF_SUCCESS;
    }

    AssertFailedReturn(VERR_NOT_IMPLEMENTED);
}


/**
 * Implements retf.
 *
 * @param   enmEffOpSize    The effective operand size.
 * @param   cbPop           The amount of arguments to pop from the stack
 *                          (bytes).
 */
IEM_CIMPL_DEF_2(iemCImpl_retf, IEMMODE, enmEffOpSize, uint16_t, cbPop)
{
    PCPUMCTX        pCtx = pIemCpu->CTX_SUFF(pCtx);
    VBOXSTRICTRC    rcStrict;
    uint64_t        uNewRsp;

    /*
     * Real mode and V8086 mode are easy.
     */
    if (   pIemCpu->enmCpuMode == IEMMODE_16BIT
        && IEM_IS_REAL_OR_V86_MODE(pIemCpu))
    {
        Assert(enmEffOpSize == IEMMODE_32BIT || enmEffOpSize == IEMMODE_16BIT);
        uint16_t const *pu16Frame;
        rcStrict = iemMemStackPopBeginSpecial(pIemCpu, enmEffOpSize == IEMMODE_32BIT ? 8 : 4,
                                              (void const **)&pu16Frame, &uNewRsp);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
        uint32_t uNewEip;
        uint16_t uNewCS;
        if (enmEffOpSize == IEMMODE_32BIT)
        {
            uNewCS  = pu16Frame[2];
            uNewEip = RT_MAKE_U32(pu16Frame[0], pu16Frame[1]);
        }
        else
        {
            uNewCS  = pu16Frame[1];
            uNewEip = pu16Frame[0];
        }
        /** @todo check how this is supposed to work if sp=0xfffe. */

        /* Check the limit of the new EIP. */
        /** @todo Intel pseudo code only does the limit check for 16-bit
         *        operands, AMD does not make any distinction. What is right? */
        if (uNewEip > pCtx->csHid.u32Limit)
            return iemRaiseSelectorBounds(pIemCpu, X86_SREG_CS, IEM_ACCESS_INSTRUCTION);

        /* commit the operation. */
        rcStrict = iemMemStackPopCommitSpecial(pIemCpu, pu16Frame, uNewRsp);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
        pCtx->rip           = uNewEip;
        pCtx->cs            = uNewCS;
        pCtx->csHid.u64Base = (uint32_t)uNewCS << 4;
        /** @todo do we load attribs and limit as well? */
        if (cbPop)
            iemRegAddToRsp(pCtx, cbPop);
        return VINF_SUCCESS;
    }

    AssertFailed();
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Implements retn.
 *
 * We're doing this in C because of the \#GP that might be raised if the popped
 * program counter is out of bounds.
 *
 * @param   enmEffOpSize    The effective operand size.
 * @param   cbPop           The amount of arguments to pop from the stack
 *                          (bytes).
 */
IEM_CIMPL_DEF_2(iemCImpl_retn, IEMMODE, enmEffOpSize, uint16_t, cbPop)
{
    PCPUMCTX        pCtx = pIemCpu->CTX_SUFF(pCtx);

    /* Fetch the RSP from the stack. */
    VBOXSTRICTRC    rcStrict;
    RTUINT64U       NewRip;
    RTUINT64U       NewRsp;
    NewRsp.u = pCtx->rsp;
    switch (enmEffOpSize)
    {
        case IEMMODE_16BIT:
            NewRip.u = 0;
            rcStrict = iemMemStackPopU16Ex(pIemCpu, &NewRip.Words.w0, &NewRsp);
            break;
        case IEMMODE_32BIT:
            NewRip.u = 0;
            rcStrict = iemMemStackPopU32Ex(pIemCpu, &NewRip.DWords.dw0, &NewRsp);
            break;
        case IEMMODE_64BIT:
            rcStrict = iemMemStackPopU64Ex(pIemCpu, &NewRip.u, &NewRsp);
            break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    /* Check the new RSP before loading it. */
    /** @todo Should test this as the intel+amd pseudo code doesn't mention half
     *        of it.  The canonical test is performed here and for call. */
    if (enmEffOpSize != IEMMODE_64BIT)
    {
        if (NewRip.DWords.dw0 > pCtx->csHid.u32Limit)
        {
            Log(("retn newrip=%llx - out of bounds (%x) -> #GP\n", NewRip.u, pCtx->csHid.u32Limit));
            return iemRaiseSelectorBounds(pIemCpu, X86_SREG_CS, IEM_ACCESS_INSTRUCTION);
        }
    }
    else
    {
        if (!IEM_IS_CANONICAL(NewRip.u))
        {
            Log(("retn newrip=%llx - not canonical -> #GP\n", NewRip.u));
            return iemRaiseNotCanonical(pIemCpu);
        }
    }

    /* Commit it. */
    pCtx->rip = NewRip.u;
    pCtx->rsp = NewRsp.u;
    if (cbPop)
        iemRegAddToRsp(pCtx, cbPop);

    return VINF_SUCCESS;
}


/**
 * Implements leave.
 *
 * We're doing this in C because messing with the stack registers is annoying
 * since they depends on SS attributes.
 *
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_1(iemCImpl_leave, IEMMODE, enmEffOpSize)
{
    PCPUMCTX        pCtx = pIemCpu->CTX_SUFF(pCtx);

    /* Calculate the intermediate RSP from RBP and the stack attributes. */
    RTUINT64U       NewRsp;
    if (pCtx->ssHid.Attr.n.u1Long)
    {
        /** @todo Check that LEAVE actually preserve the high EBP bits. */
        NewRsp.u = pCtx->rsp;
        NewRsp.Words.w0 = pCtx->bp;
    }
    else if (pCtx->ssHid.Attr.n.u1DefBig)
        NewRsp.u = pCtx->ebp;
    else
        NewRsp.u = pCtx->rbp;

    /* Pop RBP according to the operand size. */
    VBOXSTRICTRC    rcStrict;
    RTUINT64U       NewRbp;
    switch (enmEffOpSize)
    {
        case IEMMODE_16BIT:
            NewRbp.u = pCtx->rbp;
            rcStrict = iemMemStackPopU16Ex(pIemCpu, &NewRbp.Words.w0, &NewRsp);
            break;
        case IEMMODE_32BIT:
            NewRbp.u = 0;
            rcStrict = iemMemStackPopU32Ex(pIemCpu, &NewRbp.DWords.dw0, &NewRsp);
            break;
        case IEMMODE_64BIT:
            rcStrict = iemMemStackPopU64Ex(pIemCpu, &NewRbp.u, &NewRsp);
            break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;


    /* Commit it. */
    pCtx->rbp = NewRbp.u;
    pCtx->rsp = NewRsp.u;
    iemRegAddToRip(pIemCpu, cbInstr);

    return VINF_SUCCESS;
}


/**
 * Implements int3 and int XX.
 *
 * @param   u8Int       The interrupt vector number.
 * @param   fIsBpInstr  Is it the breakpoint instruction.
 */
IEM_CIMPL_DEF_2(iemCImpl_int, uint8_t, u8Int, bool, fIsBpInstr)
{
    Assert(pIemCpu->cXcptRecursions == 0);
    return iemRaiseXcptOrInt(pIemCpu,
                             cbInstr,
                             u8Int,
                             (fIsBpInstr ? IEM_XCPT_FLAGS_BP_INSTR : 0) | IEM_XCPT_FLAGS_T_SOFT_INT,
                             0,
                             0);
}


/**
 * Implements iret for real mode and V8086 mode.
 *
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_1(iemCImpl_iret_real_v8086, IEMMODE, enmEffOpSize)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * iret throws an exception if VME isn't enabled.
     */
    if (   pCtx->eflags.Bits.u1VM
        && !(pCtx->cr4 & X86_CR4_VME))
        return iemRaiseGeneralProtectionFault0(pIemCpu);

    /*
     * Do the stack bits, but don't commit RSP before everything checks
     * out right.
     */
    Assert(enmEffOpSize == IEMMODE_32BIT || enmEffOpSize == IEMMODE_16BIT);
    VBOXSTRICTRC    rcStrict;
    RTCPTRUNION     uFrame;
    uint16_t        uNewCS;
    uint32_t        uNewEip;
    uint32_t        uNewFlags;
    uint64_t        uNewRsp;
    if (enmEffOpSize == IEMMODE_32BIT)
    {
        rcStrict = iemMemStackPopBeginSpecial(pIemCpu, 12, &uFrame.pv, &uNewRsp);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
        uNewEip    = uFrame.pu32[0];
        uNewCS     = (uint16_t)uFrame.pu32[1];
        uNewFlags  = uFrame.pu32[2];
        uNewFlags &= X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF
                   | X86_EFL_TF | X86_EFL_IF | X86_EFL_DF | X86_EFL_OF | X86_EFL_IOPL | X86_EFL_NT
                   | X86_EFL_RF /*| X86_EFL_VM*/ | X86_EFL_AC /*|X86_EFL_VIF*/ /*|X86_EFL_VIP*/
                   | X86_EFL_ID;
        uNewFlags |= pCtx->eflags.u & (X86_EFL_VM | X86_EFL_VIF | X86_EFL_VIP | X86_EFL_1);
    }
    else
    {
        rcStrict = iemMemStackPopBeginSpecial(pIemCpu, 6, &uFrame.pv, &uNewRsp);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
        uNewEip    = uFrame.pu16[0];
        uNewCS     = uFrame.pu16[1];
        uNewFlags  = uFrame.pu16[2];
        uNewFlags &= X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF
                   | X86_EFL_TF | X86_EFL_IF | X86_EFL_DF | X86_EFL_OF | X86_EFL_IOPL | X86_EFL_NT;
        uNewFlags |= pCtx->eflags.u & (UINT32_C(0xffff0000) | X86_EFL_1);
        /** @todo The intel pseudo code does not indicate what happens to
         *        reserved flags. We just ignore them. */
    }
    /** @todo Check how this is supposed to work if sp=0xfffe. */

    /*
     * Check the limit of the new EIP.
     */
    /** @todo Only the AMD pseudo code check the limit here, what's
     *        right? */
    if (uNewEip > pCtx->csHid.u32Limit)
        return iemRaiseSelectorBounds(pIemCpu, X86_SREG_CS, IEM_ACCESS_INSTRUCTION);

    /*
     * V8086 checks and flag adjustments
     */
    if (pCtx->eflags.Bits.u1VM)
    {
        if (pCtx->eflags.Bits.u2IOPL == 3)
        {
            /* Preserve IOPL and clear RF. */
            uNewFlags &=                 ~(X86_EFL_IOPL | X86_EFL_RF);
            uNewFlags |= pCtx->eflags.u & (X86_EFL_IOPL);
        }
        else if (   enmEffOpSize == IEMMODE_16BIT
                 && (   !(uNewFlags & X86_EFL_IF)
                     || !pCtx->eflags.Bits.u1VIP )
                 && !(uNewFlags & X86_EFL_TF)   )
        {
            /* Move IF to VIF, clear RF and preserve IF and IOPL.*/
            uNewFlags &= ~X86_EFL_VIF;
            uNewFlags |= (uNewFlags & X86_EFL_IF) << (19 - 9);
            uNewFlags &=                 ~(X86_EFL_IF | X86_EFL_IOPL | X86_EFL_RF);
            uNewFlags |= pCtx->eflags.u & (X86_EFL_IF | X86_EFL_IOPL);
        }
        else
            return iemRaiseGeneralProtectionFault0(pIemCpu);
    }

    /*
     * Commit the operation.
     */
    rcStrict = iemMemStackPopCommitSpecial(pIemCpu, uFrame.pv, uNewRsp);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;
    pCtx->rip           = uNewEip;
    pCtx->cs            = uNewCS;
    pCtx->csHid.u64Base = (uint32_t)uNewCS << 4;
    /** @todo do we load attribs and limit as well? */
    Assert(uNewFlags & X86_EFL_1);
    pCtx->eflags.u      = uNewFlags;

    return VINF_SUCCESS;
}


/**
 * Implements iret for protected mode
 *
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_1(iemCImpl_iret_prot, IEMMODE, enmEffOpSize)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Nested task return.
     */
    if (pCtx->eflags.Bits.u1NT)
    {
        AssertFailedReturn(VERR_NOT_IMPLEMENTED);
    }
    /*
     * Normal return.
     */
    else
    {
        /*
         * Do the stack bits, but don't commit RSP before everything checks
         * out right.
         */
        Assert(enmEffOpSize == IEMMODE_32BIT || enmEffOpSize == IEMMODE_16BIT);
        VBOXSTRICTRC    rcStrict;
        RTCPTRUNION     uFrame;
        uint16_t        uNewCS;
        uint32_t        uNewEip;
        uint32_t        uNewFlags;
        uint64_t        uNewRsp;
        if (enmEffOpSize == IEMMODE_32BIT)
        {
            rcStrict = iemMemStackPopBeginSpecial(pIemCpu, 12, &uFrame.pv, &uNewRsp);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
            uNewEip    = uFrame.pu32[0];
            uNewCS     = (uint16_t)uFrame.pu32[1];
            uNewFlags  = uFrame.pu32[2];
        }
        else
        {
            rcStrict = iemMemStackPopBeginSpecial(pIemCpu, 6, &uFrame.pv, &uNewRsp);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
            uNewEip    = uFrame.pu16[0];
            uNewCS     = uFrame.pu16[1];
            uNewFlags  = uFrame.pu16[2];
        }
        rcStrict = iemMemCommitAndUnmap(pIemCpu, (void *)uFrame.pv, IEM_ACCESS_STACK_R); /* don't use iemMemStackPopCommitSpecial here. */
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;

        /*
         * What are we returning to?
         */
        if (   (uNewFlags & X86_EFL_VM)
            && pIemCpu->uCpl == 0)
        {
            /* V8086 mode! */
            AssertFailedReturn(VERR_NOT_IMPLEMENTED);
        }
        else
        {
            /*
             * Protected mode.
             */
            /* Read the CS descriptor. */
            if (!(uNewCS & (X86_SEL_MASK | X86_SEL_LDT)))
            {
                Log(("iret %04x:%08x -> invalid CS selector, #GP(0)\n", uNewCS, uNewEip));
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            }

            IEMSELDESC DescCS;
            rcStrict = iemMemFetchSelDesc(pIemCpu, &DescCS, uNewCS);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;

            /* Must be a code descriptor. */
            if (!DescCS.Legacy.Gen.u1DescType)
            {
                Log(("iret %04x:%08x - CS is system segment (%#x) -> #GP\n", uNewCS, uNewEip, DescCS.Legacy.Gen.u4Type));
                return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uNewCS);
            }
            if (!(DescCS.Legacy.Gen.u4Type & X86_SEL_TYPE_CODE))
            {
                Log(("iret %04x:%08x - not code segment (%#x) -> #GP\n", uNewCS, uNewEip, DescCS.Legacy.Gen.u4Type));
                return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uNewCS);
            }

            /* Privilege checks. */
            if ((uNewCS & X86_SEL_RPL) < pIemCpu->uCpl)
            {
                Log(("iret %04x:%08x - RPL < CPL (%d) -> #GP\n", uNewCS, uNewEip, pIemCpu->uCpl));
                return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uNewCS);
            }
            if (   (DescCS.Legacy.Gen.u4Type & X86_SEL_TYPE_CONF)
                && (uNewCS & X86_SEL_RPL) < DescCS.Legacy.Gen.u2Dpl)
            {
                Log(("iret %04x:%08x - RPL < DPL (%d) -> #GP\n", uNewCS, uNewEip, DescCS.Legacy.Gen.u2Dpl));
                return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uNewCS);
            }

            /* Present? */
            if (!DescCS.Legacy.Gen.u1Present)
            {
                Log(("iret %04x:%08x - CS not present -> #NP\n", uNewCS, uNewEip));
                return iemRaiseSelectorNotPresentBySelector(pIemCpu, uNewCS);
            }

            uint32_t cbLimitCS = X86DESC_LIMIT(DescCS.Legacy);
            if (DescCS.Legacy.Gen.u1Granularity)
                cbLimitCS = (cbLimitCS << PAGE_SHIFT) | PAGE_OFFSET_MASK;

            /*
             * Different level?
             */
            if ((uNewCS & X86_SEL_RPL) != pIemCpu->uCpl)
            {
                AssertFailedReturn(VERR_NOT_IMPLEMENTED);
            }
            /*
             * Same level.
             */
            else
            {
                /* Check EIP. */
                if (uNewEip > cbLimitCS)
                {
                    Log(("iret %04x:%08x - EIP is out of bounds (%#x) -> #GP(0)\n", uNewCS, uNewEip, cbLimitCS));
                    return iemRaiseSelectorBoundsBySelector(pIemCpu, uNewCS);
                }

                /*
                 * Commit the changes, marking CS first since it may fail.
                 */
                if (!(DescCS.Legacy.Gen.u4Type & X86_SEL_TYPE_ACCESSED))
                {
                    rcStrict = iemMemMarkSelDescAccessed(pIemCpu, uNewCS);
                    if (rcStrict != VINF_SUCCESS)
                        return rcStrict;
                    DescCS.Legacy.Gen.u4Type |= X86_SEL_TYPE_ACCESSED;
                }

                pCtx->rip               = uNewEip;
                pCtx->cs                = uNewCS;
                pCtx->csHid.Attr.u      = X86DESC_GET_HID_ATTR(DescCS.Legacy);
                pCtx->csHid.u32Limit    = cbLimitCS;
                pCtx->csHid.u64Base     = X86DESC_BASE(DescCS.Legacy);
                pCtx->rsp               = uNewRsp;

                uint32_t fEFlagsMask = X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF  | X86_EFL_SF
                                     | X86_EFL_TF | X86_EFL_DF | X86_EFL_OF | X86_EFL_NT;
                if (enmEffOpSize != IEMMODE_16BIT)
                    fEFlagsMask |= X86_EFL_RF | X86_EFL_AC | X86_EFL_ID;
                if (pIemCpu->uCpl == 0)
                    fEFlagsMask |= X86_EFL_IF | X86_EFL_IOPL | X86_EFL_VIF | X86_EFL_VIP; /* VM is 0 */
                else if (pIemCpu->uCpl <= pCtx->eflags.Bits.u2IOPL)
                    fEFlagsMask |= X86_EFL_IF;
                pCtx->eflags.u         &= ~fEFlagsMask;
                pCtx->eflags.u         |= fEFlagsMask & uNewFlags;
                /* Done! */
            }
        }
    }

    return VINF_SUCCESS;
}


/**
 * Implements iret for long mode
 *
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_1(iemCImpl_iret_long, IEMMODE, enmEffOpSize)
{
    //PCPUMCTX        pCtx = pIemCpu->CTX_SUFF(pCtx);
    //VBOXSTRICTRC    rcStrict;
    //uint64_t        uNewRsp;

    return VERR_NOT_IMPLEMENTED;
}


/**
 * Implements iret.
 *
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_1(iemCImpl_iret, IEMMODE, enmEffOpSize)
{
    /*
     * Call a mode specific worker.
     */
    if (   pIemCpu->enmCpuMode == IEMMODE_16BIT
        && IEM_IS_REAL_OR_V86_MODE(pIemCpu))
        return IEM_CIMPL_CALL_1(iemCImpl_iret_real_v8086, enmEffOpSize);
    if (IEM_IS_LONG_MODE(pIemCpu))
        return IEM_CIMPL_CALL_1(iemCImpl_iret_long, enmEffOpSize);

    return     IEM_CIMPL_CALL_1(iemCImpl_iret_prot, enmEffOpSize);
}


/**
 * Common worker for 'pop SReg', 'mov SReg, GReg' and 'lXs GReg, reg/mem'.
 *
 * @param   iSegReg     The segment register number (valid).
 * @param   uSel        The new selector value.
 */
IEM_CIMPL_DEF_2(iemCImpl_LoadSReg, uint8_t, iSegReg, uint16_t, uSel)
{
    PCPUMCTX        pCtx = pIemCpu->CTX_SUFF(pCtx);
    uint16_t       *pSel = iemSRegRef(pIemCpu, iSegReg);
    PCPUMSELREGHID  pHid = iemSRegGetHid(pIemCpu, iSegReg);

    Assert(iSegReg <= X86_SREG_GS && iSegReg != X86_SREG_CS);

    /*
     * Real mode and V8086 mode are easy.
     */
    if (   pIemCpu->enmCpuMode == IEMMODE_16BIT
        && IEM_IS_REAL_OR_V86_MODE(pIemCpu))
    {
        *pSel           = uSel;
        pHid->u64Base   = (uint32_t)uSel << 4;
        /** @todo Does the CPU actually load limits and attributes in the
         *        real/V8086 mode segment load case?  It doesn't for CS in far
         *        jumps...  Affects unreal mode.  */
        pHid->u32Limit          = 0xffff;
        pHid->Attr.u = 0;
        pHid->Attr.n.u1Present  = 1;
        pHid->Attr.n.u1DescType = 1;
        pHid->Attr.n.u4Type     = iSegReg != X86_SREG_CS
                                ? X86_SEL_TYPE_RW
                                : X86_SEL_TYPE_READ | X86_SEL_TYPE_CODE;

        iemRegAddToRip(pIemCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /*
     * Protected mode.
     *
     * Check if it's a null segment selector value first, that's OK for DS, ES,
     * FS and GS.  If not null, then we have to load and parse the descriptor.
     */
    if (!(uSel & (X86_SEL_MASK | X86_SEL_LDT)))
    {
        if (iSegReg == X86_SREG_SS)
        {
            if (   pIemCpu->enmCpuMode != IEMMODE_64BIT
                || pIemCpu->uCpl != 0
                || uSel != 0) /** @todo We cannot 'mov ss, 3' in 64-bit kernel mode, can we?  */
            {
                Log(("load sreg -> invalid stack selector, #GP(0)\n", uSel));
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            }

            /* In 64-bit kernel mode, the stack can be 0 because of the way
               interrupts are dispatched when in kernel ctx. Just load the
               selector value into the register and leave the hidden bits
               as is. */
            *pSel = uSel;
            iemRegAddToRip(pIemCpu, cbInstr);
            return VINF_SUCCESS;
        }

        *pSel = uSel;   /* Not RPL, remember :-) */
        if (   pIemCpu->enmCpuMode == IEMMODE_64BIT
            && iSegReg != X86_SREG_FS
            && iSegReg != X86_SREG_GS)
        {
            /** @todo figure out what this actually does, it works. Needs
             *        testcase! */
            pHid->Attr.u           = 0;
            pHid->Attr.n.u1Present = 1;
            pHid->Attr.n.u1Long    = 1;
            pHid->Attr.n.u4Type    = X86_SEL_TYPE_RW;
            pHid->Attr.n.u2Dpl     = 3;
            pHid->u32Limit         = 0;
            pHid->u64Base          = 0;
        }
        else
        {
            pHid->Attr.u   = 0;
            pHid->u32Limit = 0;
            pHid->u64Base  = 0;
        }
        iemRegAddToRip(pIemCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* Fetch the descriptor. */
    IEMSELDESC Desc;
    VBOXSTRICTRC rcStrict = iemMemFetchSelDesc(pIemCpu, &Desc, uSel);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    /* Check GPs first. */
    if (!Desc.Legacy.Gen.u1DescType)
    {
        Log(("load sreg %d - system selector (%#x) -> #GP\n", iSegReg, uSel, Desc.Legacy.Gen.u4Type));
        return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uSel);
    }
    if (iSegReg == X86_SREG_SS) /* SS gets different treatment */
    {
        if (   (Desc.Legacy.Gen.u4Type & X86_SEL_TYPE_CODE)
            || !(Desc.Legacy.Gen.u4Type & X86_SEL_TYPE_WRITE) )
        {
            Log(("load sreg SS, %#x - code or read only (%#x) -> #GP\n", uSel, Desc.Legacy.Gen.u4Type));
            return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uSel);
        }
        if (    (Desc.Legacy.Gen.u4Type & X86_SEL_TYPE_CODE)
            || !(Desc.Legacy.Gen.u4Type & X86_SEL_TYPE_WRITE) )
        {
            Log(("load sreg SS, %#x - code or read only (%#x) -> #GP\n", uSel, Desc.Legacy.Gen.u4Type));
            return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uSel);
        }
        if ((uSel & X86_SEL_RPL) != pIemCpu->uCpl)
        {
            Log(("load sreg SS, %#x - RPL and CPL (%d) differs -> #GP\n", uSel, pIemCpu->uCpl));
            return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uSel);
        }
        if (Desc.Legacy.Gen.u2Dpl != pIemCpu->uCpl)
        {
            Log(("load sreg SS, %#x - DPL (%d) and CPL (%d) differs -> #GP\n", uSel, Desc.Legacy.Gen.u2Dpl, pIemCpu->uCpl));
            return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uSel);
        }
    }
    else
    {
        if ((Desc.Legacy.Gen.u4Type & (X86_SEL_TYPE_CODE | X86_SEL_TYPE_READ)) == X86_SEL_TYPE_CODE)
        {
            Log(("load sreg%u, %#x - execute only segment -> #GP\n", iSegReg, uSel));
            return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uSel);
        }
        if (   (Desc.Legacy.Gen.u4Type & (X86_SEL_TYPE_CODE | X86_SEL_TYPE_CONF))
            != (X86_SEL_TYPE_CODE | X86_SEL_TYPE_CONF))
        {
#if 0 /* this is what intel says. */
            if (   (uSel & X86_SEL_RPL) > Desc.Legacy.Gen.u2Dpl
                && pIemCpu->uCpl        > Desc.Legacy.Gen.u2Dpl)
            {
                Log(("load sreg%u, %#x - both RPL (%d) and CPL (%d) are greater than DPL (%d) -> #GP\n",
                     iSegReg, uSel, (uSel & X86_SEL_RPL), pIemCpu->uCpl, Desc.Legacy.Gen.u2Dpl));
                return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uSel);
            }
#else /* this is what makes more sense. */
            if ((unsigned)(uSel & X86_SEL_RPL) > Desc.Legacy.Gen.u2Dpl)
            {
                Log(("load sreg%u, %#x - RPL (%d) is greater than DPL (%d) -> #GP\n",
                     iSegReg, uSel, (uSel & X86_SEL_RPL), Desc.Legacy.Gen.u2Dpl));
                return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uSel);
            }
            if (pIemCpu->uCpl > Desc.Legacy.Gen.u2Dpl)
            {
                Log(("load sreg%u, %#x - CPL (%d) is greater than DPL (%d) -> #GP\n",
                     iSegReg, uSel, pIemCpu->uCpl, Desc.Legacy.Gen.u2Dpl));
                return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uSel);
            }
#endif
        }
    }

    /* Is it there? */
    if (!Desc.Legacy.Gen.u1Present)
    {
        Log(("load sreg%d,%#x - segment not present -> #NP\n", iSegReg, uSel));
        return iemRaiseSelectorNotPresentBySelector(pIemCpu, uSel);
    }

    /* The the base and limit. */
    uint64_t u64Base;
    uint32_t cbLimit = X86DESC_LIMIT(Desc.Legacy);
    if (Desc.Legacy.Gen.u1Granularity)
        cbLimit = (cbLimit << PAGE_SHIFT) | PAGE_OFFSET_MASK;

    if (   pIemCpu->enmCpuMode == IEMMODE_64BIT
        && iSegReg < X86_SREG_FS)
        u64Base = 0;
    else
        u64Base = X86DESC_BASE(Desc.Legacy);

    /*
     * Ok, everything checked out fine.  Now set the accessed bit before
     * committing the result into the registers.
     */
    if (!(Desc.Legacy.Gen.u4Type & X86_SEL_TYPE_ACCESSED))
    {
        rcStrict = iemMemMarkSelDescAccessed(pIemCpu, uSel);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
        Desc.Legacy.Gen.u4Type |= X86_SEL_TYPE_ACCESSED;
    }

    /* commit */
    *pSel = uSel;
    pHid->Attr.u   = (Desc.Legacy.u >> (16+16+8)) & UINT32_C(0xf0ff); /** @todo do we have a define for 0xf0ff? */
    pHid->u32Limit = cbLimit;
    pHid->u64Base  = u64Base;

    /** @todo check if the hidden bits are loaded correctly for 64-bit
     *        mode.  */

    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * Implements 'mov SReg, r/m'.
 *
 * @param   iSegReg     The segment register number (valid).
 * @param   uSel        The new selector value.
 */
IEM_CIMPL_DEF_2(iemCImpl_load_SReg, uint8_t, iSegReg, uint16_t, uSel)
{
    VBOXSTRICTRC rcStrict = IEM_CIMPL_CALL_2(iemCImpl_LoadSReg, iSegReg, uSel);
    if (rcStrict == VINF_SUCCESS)
    {
        if (iSegReg == X86_SREG_SS)
        {
            PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
            EMSetInhibitInterruptsPC(IEMCPU_TO_VMCPU(pIemCpu), pCtx->rip);
        }
    }
    return rcStrict;
}


/**
 * Implements 'pop SReg'.
 *
 * @param   iSegReg         The segment register number (valid).
 * @param   enmEffOpSize    The efficient operand size (valid).
 */
IEM_CIMPL_DEF_2(iemCImpl_pop_Sreg, uint8_t, iSegReg, IEMMODE, enmEffOpSize)
{
    PCPUMCTX        pCtx = pIemCpu->CTX_SUFF(pCtx);
    VBOXSTRICTRC    rcStrict;

    /*
     * Read the selector off the stack and join paths with mov ss, reg.
     */
    RTUINT64U TmpRsp;
    TmpRsp.u = pCtx->rsp;
    switch (enmEffOpSize)
    {
        case IEMMODE_16BIT:
        {
            uint16_t uSel;
            rcStrict = iemMemStackPopU16Ex(pIemCpu, &uSel, &TmpRsp);
            if (rcStrict == VINF_SUCCESS)
                rcStrict = IEM_CIMPL_CALL_2(iemCImpl_LoadSReg, iSegReg, uSel);
            break;
        }

        case IEMMODE_32BIT:
        {
            uint32_t u32Value;
            rcStrict = iemMemStackPopU32Ex(pIemCpu, &u32Value, &TmpRsp);
            if (rcStrict == VINF_SUCCESS)
                rcStrict = IEM_CIMPL_CALL_2(iemCImpl_LoadSReg, iSegReg, (uint16_t)u32Value);
            break;
        }

        case IEMMODE_64BIT:
        {
            uint64_t u64Value;
            rcStrict = iemMemStackPopU64Ex(pIemCpu, &u64Value, &TmpRsp);
            if (rcStrict == VINF_SUCCESS)
                rcStrict = IEM_CIMPL_CALL_2(iemCImpl_LoadSReg, iSegReg, (uint16_t)u64Value);
            break;
        }
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }

    /*
     * Commit the stack on success.
     */
    if (rcStrict == VINF_SUCCESS)
    {
        pCtx->rsp = TmpRsp.u;
        if (iSegReg == X86_SREG_SS)
            EMSetInhibitInterruptsPC(IEMCPU_TO_VMCPU(pIemCpu), pCtx->rip);
    }
    return rcStrict;
}


/**
 * Implements lgs, lfs, les, lds & lss.
 */
IEM_CIMPL_DEF_5(iemCImpl_load_SReg_Greg,
                uint16_t, uSel,
                uint64_t, offSeg,
                uint8_t,  iSegReg,
                uint8_t,  iGReg,
                IEMMODE,  enmEffOpSize)
{
    PCPUMCTX        pCtx = pIemCpu->CTX_SUFF(pCtx);
    VBOXSTRICTRC    rcStrict;

    /*
     * Use iemCImpl_LoadSReg to do the tricky segment register loading.
     */
    /** @todo verify and test that mov, pop and lXs works the segment
     *        register loading in the exact same way. */
    rcStrict = IEM_CIMPL_CALL_2(iemCImpl_LoadSReg, iSegReg, uSel);
    if (rcStrict == VINF_SUCCESS)
    {
        switch (enmEffOpSize)
        {
            case IEMMODE_16BIT:
                *(uint16_t *)iemGRegRef(pIemCpu, iGReg) = offSeg;
                break;
            case IEMMODE_32BIT:
                *(uint64_t *)iemGRegRef(pIemCpu, iGReg) = offSeg;
                break;
            case IEMMODE_64BIT:
                *(uint64_t *)iemGRegRef(pIemCpu, iGReg) = offSeg;
                break;
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }

    return rcStrict;
}


/**
 * Implements lgdt.
 *
 * @param   iEffSeg         The segment of the new ldtr contents
 * @param   GCPtrEffSrc     The address of the new ldtr contents.
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_3(iemCImpl_lgdt, uint8_t, iEffSeg, RTGCPTR, GCPtrEffSrc, IEMMODE, enmEffOpSize)
{
    if (pIemCpu->uCpl != 0)
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    Assert(!pIemCpu->CTX_SUFF(pCtx)->eflags.Bits.u1VM);

    /*
     * Fetch the limit and base address.
     */
    uint16_t cbLimit;
    RTGCPTR  GCPtrBase;
    VBOXSTRICTRC rcStrict = iemMemFetchDataXdtr(pIemCpu, &cbLimit, &GCPtrBase, iEffSeg, GCPtrEffSrc, enmEffOpSize);
    if (rcStrict == VINF_SUCCESS)
    {
        if (!IEM_VERIFICATION_ENABLED(pIemCpu))
            rcStrict = CPUMSetGuestGDTR(IEMCPU_TO_VMCPU(pIemCpu), GCPtrBase, cbLimit);
        else
        {
            PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
            pCtx->gdtr.cbGdt = cbLimit;
            pCtx->gdtr.pGdt  = GCPtrBase;
        }
        if (rcStrict == VINF_SUCCESS)
            iemRegAddToRip(pIemCpu, cbInstr);
    }
    return rcStrict;
}


/**
 * Implements lidt.
 *
 * @param   iEffSeg         The segment of the new ldtr contents
 * @param   GCPtrEffSrc     The address of the new ldtr contents.
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_3(iemCImpl_lidt, uint8_t, iEffSeg, RTGCPTR, GCPtrEffSrc, IEMMODE, enmEffOpSize)
{
    if (pIemCpu->uCpl != 0)
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    Assert(!pIemCpu->CTX_SUFF(pCtx)->eflags.Bits.u1VM);

    /*
     * Fetch the limit and base address.
     */
    uint16_t cbLimit;
    RTGCPTR  GCPtrBase;
    VBOXSTRICTRC rcStrict = iemMemFetchDataXdtr(pIemCpu, &cbLimit, &GCPtrBase, iEffSeg, GCPtrEffSrc, enmEffOpSize);
    if (rcStrict == VINF_SUCCESS)
    {
        if (!IEM_VERIFICATION_ENABLED(pIemCpu))
            rcStrict = CPUMSetGuestIDTR(IEMCPU_TO_VMCPU(pIemCpu), GCPtrBase, cbLimit);
        else
        {
            PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
            pCtx->idtr.cbIdt = cbLimit;
            pCtx->idtr.pIdt  = GCPtrBase;
        }
        if (rcStrict == VINF_SUCCESS)
            iemRegAddToRip(pIemCpu, cbInstr);
    }
    return rcStrict;
}


/**
 * Implements lldt.
 *
 * @param   uNewLdt     The new LDT selector value.
 */
IEM_CIMPL_DEF_1(iemCImpl_lldt, uint16_t, uNewLdt)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Check preconditions.
     */
    if (IEM_IS_REAL_OR_V86_MODE(pIemCpu))
    {
        Log(("lldt %04x - real or v8086 mode -> #GP(0)\n", uNewLdt));
        return iemRaiseUndefinedOpcode(pIemCpu);
    }
    if (pIemCpu->uCpl != 0)
    {
        Log(("lldt %04x - CPL is %d -> #GP(0)\n", uNewLdt, pIemCpu->uCpl));
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    }
    if (uNewLdt & X86_SEL_LDT)
    {
        Log(("lldt %04x - LDT selector -> #GP\n", uNewLdt));
        return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uNewLdt);
    }

    /*
     * Now, loading a NULL selector is easy.
     */
    if ((uNewLdt & X86_SEL_MASK) == 0)
    {
        Log(("lldt %04x: Loading NULL selector.\n",  uNewLdt));
        /** @todo check if the actual value is loaded or if it's always 0. */
        if (!IEM_VERIFICATION_ENABLED(pIemCpu))
            CPUMSetGuestLDTR(IEMCPU_TO_VMCPU(pIemCpu), 0);
        else
            pCtx->ldtr = 0;
        pCtx->ldtrHid.Attr.u   = 0;
        pCtx->ldtrHid.u64Base  = 0;
        pCtx->ldtrHid.u32Limit = 0;

        iemRegAddToRip(pIemCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /*
     * Read the descriptor.
     */
    IEMSELDESC Desc;
    VBOXSTRICTRC rcStrict = iemMemFetchSelDesc(pIemCpu, &Desc, uNewLdt);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    /* Check GPs first. */
    if (Desc.Legacy.Gen.u1DescType)
    {
        Log(("lldt %#x - not system selector (type %x) -> #GP\n", uNewLdt, Desc.Legacy.Gen.u4Type));
        return iemRaiseGeneralProtectionFault(pIemCpu, uNewLdt & X86_SEL_MASK);
    }
    if (Desc.Legacy.Gen.u4Type != X86_SEL_TYPE_SYS_LDT)
    {
        Log(("lldt %#x - not LDT selector (type %x) -> #GP\n", uNewLdt, Desc.Legacy.Gen.u4Type));
        return iemRaiseGeneralProtectionFault(pIemCpu, uNewLdt & X86_SEL_MASK);
    }
    uint64_t u64Base;
    if (!IEM_IS_LONG_MODE(pIemCpu))
        u64Base = X86DESC_BASE(Desc.Legacy);
    else
    {
        if (Desc.Long.Gen.u5Zeros)
        {
            Log(("lldt %#x - u5Zeros=%#x -> #GP\n", uNewLdt, Desc.Long.Gen.u5Zeros));
            return iemRaiseGeneralProtectionFault(pIemCpu, uNewLdt & X86_SEL_MASK);
        }

        u64Base = X86DESC64_BASE(Desc.Long);
        if (!IEM_IS_CANONICAL(u64Base))
        {
            Log(("lldt %#x - non-canonical base address %#llx -> #GP\n", uNewLdt, u64Base));
            return iemRaiseGeneralProtectionFault(pIemCpu, uNewLdt & X86_SEL_MASK);
        }
    }

    /* NP */
    if (!Desc.Legacy.Gen.u1Present)
    {
        Log(("lldt %#x - segment not present -> #NP\n", uNewLdt));
        return iemRaiseSelectorNotPresentBySelector(pIemCpu, uNewLdt);
    }

    /*
     * It checks out alright, update the registers.
     */
/** @todo check if the actual value is loaded or if the RPL is dropped */
    if (!IEM_VERIFICATION_ENABLED(pIemCpu))
        CPUMSetGuestLDTR(IEMCPU_TO_VMCPU(pIemCpu), uNewLdt & X86_SEL_MASK);
    else
        pCtx->ldtr = uNewLdt & X86_SEL_MASK;
    pCtx->ldtrHid.Attr.u   = X86DESC_GET_HID_ATTR(Desc.Legacy);
    pCtx->ldtrHid.u32Limit = X86DESC_LIMIT(Desc.Legacy);
    pCtx->ldtrHid.u64Base  = u64Base;

    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * Implements lldt.
 *
 * @param   uNewLdt     The new LDT selector value.
 */
IEM_CIMPL_DEF_1(iemCImpl_ltr, uint16_t, uNewTr)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Check preconditions.
     */
    if (IEM_IS_REAL_OR_V86_MODE(pIemCpu))
    {
        Log(("ltr %04x - real or v8086 mode -> #GP(0)\n", uNewTr));
        return iemRaiseUndefinedOpcode(pIemCpu);
    }
    if (pIemCpu->uCpl != 0)
    {
        Log(("ltr %04x - CPL is %d -> #GP(0)\n", uNewTr, pIemCpu->uCpl));
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    }
    if (uNewTr & X86_SEL_LDT)
    {
        Log(("ltr %04x - LDT selector -> #GP\n", uNewTr));
        return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uNewTr);
    }
    if ((uNewTr & X86_SEL_MASK) == 0)
    {
        Log(("ltr %04x - NULL selector -> #GP(0)\n", uNewTr));
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    }

    /*
     * Read the descriptor.
     */
    IEMSELDESC Desc;
    VBOXSTRICTRC rcStrict = iemMemFetchSelDesc(pIemCpu, &Desc, uNewTr);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    /* Check GPs first. */
    if (Desc.Legacy.Gen.u1DescType)
    {
        Log(("ltr %#x - not system selector (type %x) -> #GP\n", uNewTr, Desc.Legacy.Gen.u4Type));
        return iemRaiseGeneralProtectionFault(pIemCpu, uNewTr & X86_SEL_MASK);
    }
    if (   Desc.Legacy.Gen.u4Type != X86_SEL_TYPE_SYS_386_TSS_AVAIL /* same as AMD64_SEL_TYPE_SYS_TSS_AVAIL */
        && (   Desc.Legacy.Gen.u4Type != X86_SEL_TYPE_SYS_286_TSS_AVAIL
            || IEM_IS_LONG_MODE(pIemCpu)) )
    {
        Log(("ltr %#x - not an available TSS selector (type %x) -> #GP\n", uNewTr, Desc.Legacy.Gen.u4Type));
        return iemRaiseGeneralProtectionFault(pIemCpu, uNewTr & X86_SEL_MASK);
    }
    uint64_t u64Base;
    if (!IEM_IS_LONG_MODE(pIemCpu))
        u64Base = X86DESC_BASE(Desc.Legacy);
    else
    {
        if (Desc.Long.Gen.u5Zeros)
        {
            Log(("ltr %#x - u5Zeros=%#x -> #GP\n", uNewTr, Desc.Long.Gen.u5Zeros));
            return iemRaiseGeneralProtectionFault(pIemCpu, uNewTr & X86_SEL_MASK);
        }

        u64Base = X86DESC64_BASE(Desc.Long);
        if (!IEM_IS_CANONICAL(u64Base))
        {
            Log(("ltr %#x - non-canonical base address %#llx -> #GP\n", uNewTr, u64Base));
            return iemRaiseGeneralProtectionFault(pIemCpu, uNewTr & X86_SEL_MASK);
        }
    }

    /* NP */
    if (!Desc.Legacy.Gen.u1Present)
    {
        Log(("ltr %#x - segment not present -> #NP\n", uNewTr));
        return iemRaiseSelectorNotPresentBySelector(pIemCpu, uNewTr);
    }

    /*
     * Set it busy.
     * Note! Intel says this should lock down the whole descriptor, but we'll
     *       restrict our selves to 32-bit for now due to lack of inline
     *       assembly and such.
     */
    void *pvDesc;
    rcStrict = iemMemMap(pIemCpu, &pvDesc, 8, UINT8_MAX, pCtx->gdtr.pGdt, IEM_ACCESS_DATA_RW);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;
    switch ((uintptr_t)pvDesc & 3)
    {
        case 0: ASMAtomicBitSet(pvDesc, 40 + 1); break;
        case 1: ASMAtomicBitSet((uint8_t *)pvDesc + 3, 40 + 1 - 24); break;
        case 2: ASMAtomicBitSet((uint8_t *)pvDesc + 3, 40 + 1 - 16); break;
        case 3: ASMAtomicBitSet((uint8_t *)pvDesc + 3, 40 + 1 -  8); break;
    }
    rcStrict = iemMemMap(pIemCpu, &pvDesc, 8, UINT8_MAX, pCtx->gdtr.pGdt, IEM_ACCESS_DATA_RW);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;
    Desc.Legacy.Gen.u4Type |= X86_SEL_TYPE_SYS_TSS_BUSY_MASK;

    /*
     * It checks out alright, update the registers.
     */
/** @todo check if the actual value is loaded or if the RPL is dropped */
    if (!IEM_VERIFICATION_ENABLED(pIemCpu))
        CPUMSetGuestTR(IEMCPU_TO_VMCPU(pIemCpu), uNewTr & X86_SEL_MASK);
    else
        pCtx->tr = uNewTr & X86_SEL_MASK;
    pCtx->trHid.Attr.u   = X86DESC_GET_HID_ATTR(Desc.Legacy);
    pCtx->trHid.u32Limit = X86DESC_LIMIT(Desc.Legacy);
    pCtx->trHid.u64Base  = u64Base;

    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * Implements mov GReg,CRx.
 *
 * @param   iGReg           The general register to store the CRx value in.
 * @param   iCrReg          The CRx register to read (valid).
 */
IEM_CIMPL_DEF_2(iemCImpl_mov_Rd_Cd, uint8_t, iGReg, uint8_t, iCrReg)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
    if (pIemCpu->uCpl != 0)
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    Assert(!pCtx->eflags.Bits.u1VM);

    /* read it */
    uint64_t crX;
    switch (iCrReg)
    {
        case 0: crX = pCtx->cr0; break;
        case 2: crX = pCtx->cr2; break;
        case 3: crX = pCtx->cr3; break;
        case 4: crX = pCtx->cr4; break;
        case 8:
            if (!IEM_VERIFICATION_ENABLED(pIemCpu))
                AssertFailedReturn(VERR_NOT_IMPLEMENTED); /** @todo implement CR8 reading and writing. */
            else
                crX = 0xff;
            break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET(); /* call checks */
    }

    /* store it */
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
        *(uint64_t *)iemGRegRef(pIemCpu, iGReg) = crX;
    else
        *(uint64_t *)iemGRegRef(pIemCpu, iGReg) = (uint32_t)crX;

    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * Used to implemented 'mov CRx,GReg' and 'lmsw r/m16'.
 *
 * @param   iCrReg          The CRx register to write (valid).
 * @param   uNewCrX         The new value.
 */
IEM_CIMPL_DEF_2(iemCImpl_load_CrX, uint8_t, iCrReg, uint64_t, uNewCrX)
{
    PCPUMCTX        pCtx  = pIemCpu->CTX_SUFF(pCtx);
    PVMCPU          pVCpu = IEMCPU_TO_VMCPU(pIemCpu);
    VBOXSTRICTRC    rcStrict;
    int             rc;

    /*
     * Try store it.
     * Unfortunately, CPUM only does a tiny bit of the work.
     */
    switch (iCrReg)
    {
        case 0:
        {
            /*
             * Perform checks.
             */
            uint64_t const uOldCrX = pCtx->cr0;
            uNewCrX |= X86_CR0_ET; /* hardcoded */

            /* Check for reserved bits. */
            uint32_t const fValid = X86_CR0_PE | X86_CR0_MP | X86_CR0_EM | X86_CR0_TS
                                  | X86_CR0_ET | X86_CR0_NE | X86_CR0_WP | X86_CR0_AM
                                  | X86_CR0_NW | X86_CR0_CD | X86_CR0_PG;
            if (uNewCrX & ~(uint64_t)fValid)
            {
                Log(("Trying to set reserved CR0 bits: NewCR0=%#llx InvalidBits=%#llx\n", uNewCrX, uNewCrX & ~(uint64_t)fValid));
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            }

            /* Check for invalid combinations. */
            if (    (uNewCrX & X86_CR0_PG)
                && !(uNewCrX & X86_CR0_PE) )
            {
                Log(("Trying to set CR0.PG without CR0.PE\n"));
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            }

            if (   !(uNewCrX & X86_CR0_CD)
                && (uNewCrX & X86_CR0_NW) )
            {
                Log(("Trying to clear CR0.CD while leaving CR0.NW set\n"));
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            }

            /* Long mode consistency checks. */
            if (    (uNewCrX & X86_CR0_PG)
                && !(uOldCrX & X86_CR0_PG)
                &&  (pCtx->msrEFER & MSR_K6_EFER_LME) )
            {
                if (!(pCtx->cr4 & X86_CR4_PAE))
                {
                    Log(("Trying to enabled long mode paging without CR4.PAE set\n"));
                    return iemRaiseGeneralProtectionFault0(pIemCpu);
                }
                if (pCtx->csHid.Attr.n.u1Long)
                {
                    Log(("Trying to enabled long mode paging with a long CS descriptor loaded.\n"));
                    return iemRaiseGeneralProtectionFault0(pIemCpu);
                }
            }

            /** @todo check reserved PDPTR bits as AMD states. */

            /*
             * Change CR0.
             */
            if (!IEM_VERIFICATION_ENABLED(pIemCpu))
            {
                rc = CPUMSetGuestCR0(pVCpu, uNewCrX);
                AssertRCSuccessReturn(rc, RT_FAILURE_NP(rc) ? rc : VERR_INTERNAL_ERROR_3);
            }
            else
                pCtx->cr0 = uNewCrX;
            Assert(pCtx->cr0 == uNewCrX);

            /*
             * Change EFER.LMA if entering or leaving long mode.
             */
            if (   (uNewCrX & X86_CR0_PG) != (uOldCrX & X86_CR0_PG)
                && (pCtx->msrEFER & MSR_K6_EFER_LME) )
            {
                uint64_t NewEFER = pCtx->msrEFER;
                if (uNewCrX & X86_CR0_PG)
                    NewEFER |= MSR_K6_EFER_LME;
                else
                    NewEFER &= ~MSR_K6_EFER_LME;

                if (!IEM_VERIFICATION_ENABLED(pIemCpu))
                    CPUMSetGuestEFER(pVCpu, NewEFER);
                else
                    pCtx->msrEFER = NewEFER;
                Assert(pCtx->msrEFER == NewEFER);
            }

            /*
             * Inform PGM.
             */
            if (!IEM_VERIFICATION_ENABLED(pIemCpu))
            {
                if (    (uNewCrX & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE))
                    !=  (uOldCrX & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE)) )
                {
                    rc = PGMFlushTLB(pVCpu, pCtx->cr3, true /* global */);
                    AssertRCReturn(rc, rc);
                    /* ignore informational status codes */
                }
                rcStrict = PGMChangeMode(pVCpu, pCtx->cr0, pCtx->cr4, pCtx->msrEFER);
                /** @todo Status code management.  */
            }
            else
                rcStrict = VINF_SUCCESS;
            break;
        }

        /*
         * CR2 can be changed without any restrictions.
         */
        case 2:
            pCtx->cr2 = uNewCrX;
            rcStrict  = VINF_SUCCESS;
            break;

        /*
         * CR3 is relatively simple, although AMD and Intel have different
         * accounts of how setting reserved bits are handled.  We take intel's
         * word for the lower bits and AMD's for the high bits (63:52).
         */
        /** @todo Testcase: Setting reserved bits in CR3, especially before
         *        enabling paging. */
        case 3:
        {
            /* check / mask the value. */
            if (uNewCrX & UINT64_C(0xfff0000000000000))
            {
                Log(("Trying to load CR3 with invalid high bits set: %#llx\n", uNewCrX));
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            }

            uint64_t fValid;
            if (   (pCtx->cr4 & X86_CR4_PAE)
                && (pCtx->msrEFER & MSR_K6_EFER_LME))
                fValid = UINT64_C(0x000ffffffffff014);
            else if (pCtx->cr4 & X86_CR4_PAE)
                fValid = UINT64_C(0xfffffff4);
            else
                fValid = UINT64_C(0xfffff014);
            if (uNewCrX & ~fValid)
            {
                Log(("Automatically clearing reserved bits in CR3 load: NewCR3=%#llx ClearedBits=%#llx\n",
                     uNewCrX, uNewCrX & ~fValid));
                uNewCrX &= fValid;
            }

            /** @todo If we're in PAE mode we should check the PDPTRs for
             *        invalid bits. */

            /* Make the change. */
            if (!IEM_VERIFICATION_ENABLED(pIemCpu))
            {
                rc = CPUMSetGuestCR3(pVCpu, uNewCrX);
                AssertRCSuccessReturn(rc, rc);
            }
            else
                pCtx->cr3 = uNewCrX;

            /* Inform PGM. */
            if (!IEM_VERIFICATION_ENABLED(pIemCpu))
            {
                if (pCtx->cr0 & X86_CR0_PG)
                {
                    rc = PGMFlushTLB(pVCpu, pCtx->cr3, !(pCtx->cr3 & X86_CR4_PGE));
                    AssertRCReturn(rc, rc);
                    /* ignore informational status codes */
                    /** @todo status code management */
                }
            }
            rcStrict = VINF_SUCCESS;
            break;
        }

        /*
         * CR4 is a bit more tedious as there are bits which cannot be cleared
         * under some circumstances and such.
         */
        case 4:
        {
            uint64_t const uOldCrX = pCtx->cr0;

            /* reserved bits */
            uint32_t fValid = X86_CR4_VME | X86_CR4_PVI
                            | X86_CR4_TSD | X86_CR4_DE
                            | X86_CR4_PSE | X86_CR4_PAE
                            | X86_CR4_MCE | X86_CR4_PGE
                            | X86_CR4_PCE | X86_CR4_OSFSXR
                            | X86_CR4_OSXMMEEXCPT;
            //if (xxx)
            //    fValid |= X86_CR4_VMXE;
            //if (xxx)
            //    fValid |= X86_CR4_OSXSAVE;
            if (uNewCrX & ~(uint64_t)fValid)
            {
                Log(("Trying to set reserved CR4 bits: NewCR4=%#llx InvalidBits=%#llx\n", uNewCrX, uNewCrX & ~(uint64_t)fValid));
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            }

            /* long mode checks. */
            if (   (uOldCrX & X86_CR4_PAE)
                && !(uNewCrX & X86_CR4_PAE)
                && (pCtx->msrEFER & MSR_K6_EFER_LMA) )
            {
                Log(("Trying to set clear CR4.PAE while long mode is active\n"));
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            }


            /*
             * Change it.
             */
            if (!IEM_VERIFICATION_ENABLED(pIemCpu))
            {
                rc = CPUMSetGuestCR4(pVCpu, uNewCrX);
                AssertRCSuccessReturn(rc, rc);
            }
            else
                pCtx->cr4 = uNewCrX;
            Assert(pCtx->cr4 == uNewCrX);

            /*
             * Notify SELM and PGM.
             */
            if (!IEM_VERIFICATION_ENABLED(pIemCpu))
            {
                /* SELM - VME may change things wrt to the TSS shadowing. */
                if ((uNewCrX ^ uOldCrX) & X86_CR4_VME)
                    VMCPU_FF_SET(pVCpu, VMCPU_FF_SELM_SYNC_TSS);

                /* PGM - flushing and mode. */
                if (    (uNewCrX & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE))
                    !=  (uOldCrX & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE)) )
                {
                    rc = PGMFlushTLB(pVCpu, pCtx->cr3, true /* global */);
                    AssertRCReturn(rc, rc);
                    /* ignore informational status codes */
                }
                rcStrict = PGMChangeMode(pVCpu, pCtx->cr0, pCtx->cr4, pCtx->msrEFER);
                /** @todo Status code management.  */
            }
            else
                rcStrict = VINF_SUCCESS;
            break;
        }

        /*
         * CR8 maps to the APIC TPR.
         */
        case 8:
            if (!IEM_VERIFICATION_ENABLED(pIemCpu))
                AssertFailedReturn(VERR_NOT_IMPLEMENTED); /** @todo implement CR8 reading and writing. */
            else
                rcStrict = VINF_SUCCESS;
            break;

        IEM_NOT_REACHED_DEFAULT_CASE_RET(); /* call checks */
    }

    /*
     * Advance the RIP on success.
     */
    /** @todo Status code management.  */
    if (rcStrict == VINF_SUCCESS)
        iemRegAddToRip(pIemCpu, cbInstr);
    return rcStrict;

}


/**
 * Implements mov CRx,GReg.
 *
 * @param   iCrReg          The CRx register to write (valid).
 * @param   iGReg           The general register to load the DRx value from.
 */
IEM_CIMPL_DEF_2(iemCImpl_mov_Cd_Rd, uint8_t, iCrReg, uint8_t, iGReg)
{
    if (pIemCpu->uCpl != 0)
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    Assert(!pIemCpu->CTX_SUFF(pCtx)->eflags.Bits.u1VM);

    /*
     * Read the new value from the source register and call common worker.
     */
    uint64_t uNewCrX;
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
        uNewCrX = iemGRegFetchU64(pIemCpu, iGReg);
    else
        uNewCrX = iemGRegFetchU32(pIemCpu, iGReg);
    return IEM_CIMPL_CALL_2(iemCImpl_load_CrX, iCrReg, uNewCrX);
}


/**
 * Implements 'LMSW r/m16'
 *
 * @param   u16NewMsw       The new value.
 */
IEM_CIMPL_DEF_1(iemCImpl_lmsw, uint16_t, u16NewMsw)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    if (pIemCpu->uCpl != 0)
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    Assert(!pCtx->eflags.Bits.u1VM);

    /*
     * Compose the new CR0 value and call common worker.
     */
    uint64_t uNewCr0 = pCtx->cr0     & ~(X86_CR0_MP | X86_CR0_EM | X86_CR0_TS);
    uNewCr0 |= u16NewMsw & (X86_CR0_PE | X86_CR0_MP | X86_CR0_EM | X86_CR0_TS);
    return IEM_CIMPL_CALL_2(iemCImpl_load_CrX, /*cr*/ 0, uNewCr0);
}


/**
 * Implements 'CLTS'.
 */
IEM_CIMPL_DEF_0(iemCImpl_clts)
{
    if (pIemCpu->uCpl != 0)
        return iemRaiseGeneralProtectionFault0(pIemCpu);

    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
    uint64_t uNewCr0 = pCtx->cr0;
    uNewCr0 &= ~X86_CR0_TS;
    return IEM_CIMPL_CALL_2(iemCImpl_load_CrX, /*cr*/ 0, uNewCr0);
}


/**
 * Implements mov GReg,DRx.
 *
 * @param   iGReg           The general register to store the DRx value in.
 * @param   iDrReg          The DRx register to read (0-7).
 */
IEM_CIMPL_DEF_2(iemCImpl_mov_Rd_Dd, uint8_t, iGReg, uint8_t, iDrReg)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Check preconditions.
     */

    /* Raise GPs. */
    if (pIemCpu->uCpl != 0)
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    Assert(!pCtx->eflags.Bits.u1VM);

    if (   (iDrReg == 4 || iDrReg == 5)
        && (pCtx->cr4 & X86_CR4_DE) )
    {
        Log(("mov r%u,dr%u: CR4.DE=1 -> #GP(0)\n", iGReg, iDrReg));
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    }

    /* Raise #DB if general access detect is enabled. */
    if (pCtx->dr[7] & X86_DR7_GD)
    {
        Log(("mov r%u,dr%u: DR7.GD=1 -> #DB\n", iGReg, iDrReg));
        return iemRaiseDebugException(pIemCpu);
    }

    /*
     * Read the debug register and store it in the specified general register.
     */
    uint64_t drX;
    switch (iDrReg)
    {
        case 0: drX = pCtx->dr[0]; break;
        case 1: drX = pCtx->dr[1]; break;
        case 2: drX = pCtx->dr[2]; break;
        case 3: drX = pCtx->dr[3]; break;
        case 6:
        case 4:
            drX = pCtx->dr[6];
            drX &= ~RT_BIT_32(12);
            drX |= UINT32_C(0xffff0ff0);
            break;
        case 7:
        case 5:
            drX = pCtx->dr[7];
            drX &= ~(RT_BIT_32(11) | RT_BIT_32(12) | RT_BIT_32(14) | RT_BIT_32(15));
            drX |= RT_BIT_32(10);
            break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET(); /* call checks */
    }

    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
        *(uint64_t *)iemGRegRef(pIemCpu, iGReg) = drX;
    else
        *(uint64_t *)iemGRegRef(pIemCpu, iGReg) = (uint32_t)drX;

    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * Implements mov DRx,GReg.
 *
 * @param   iDrReg          The DRx register to write (valid).
 * @param   iGReg           The general register to load the DRx value from.
 */
IEM_CIMPL_DEF_2(iemCImpl_mov_Dd_Rd, uint8_t, iDrReg, uint8_t, iGReg)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Check preconditions.
     */
    if (pIemCpu->uCpl != 0)
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    Assert(!pCtx->eflags.Bits.u1VM);

    if (   (iDrReg == 4 || iDrReg == 5)
        && (pCtx->cr4 & X86_CR4_DE) )
    {
        Log(("mov dr%u,r%u: CR4.DE=1 -> #GP(0)\n", iDrReg, iGReg));
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    }

    /* Raise #DB if general access detect is enabled. */
    /** @todo is \#DB/DR7.GD raised before any reserved high bits in DR7/DR6
     *        \#GP? */
    if (pCtx->dr[7] & X86_DR7_GD)
    {
        Log(("mov dr%u,r%u: DR7.GD=1 -> #DB\n", iDrReg, iGReg));
        return iemRaiseDebugException(pIemCpu);
    }

    /*
     * Read the new value from the source register.
     */
    uint64_t uNewDrX;
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
        uNewDrX = iemGRegFetchU64(pIemCpu, iGReg);
    else
        uNewDrX = iemGRegFetchU32(pIemCpu, iGReg);

    /*
     * Adjust it.
     */
    switch (iDrReg)
    {
        case 0:
        case 1:
        case 2:
        case 3:
            /* nothing to adjust */
            break;

        case 6:
        case 4:
            if (uNewDrX & UINT64_C(0xffffffff00000000))
            {
                Log(("mov dr%u,%#llx: DR6 high bits are not zero -> #GP(0)\n", iDrReg, uNewDrX));
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            }
            uNewDrX &= ~RT_BIT_32(12);
            uNewDrX |= UINT32_C(0xffff0ff0);
            break;

        case 7:
        case 5:
            if (uNewDrX & UINT64_C(0xffffffff00000000))
            {
                Log(("mov dr%u,%#llx: DR7 high bits are not zero -> #GP(0)\n", iDrReg, uNewDrX));
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            }
            uNewDrX &= ~(RT_BIT_32(11) | RT_BIT_32(12) | RT_BIT_32(14) | RT_BIT_32(15));
            uNewDrX |= RT_BIT_32(10);
            break;

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }

    /*
     * Do the actual setting.
     */
    if (!IEM_VERIFICATION_ENABLED(pIemCpu))
    {
        int rc = CPUMSetGuestDRx(IEMCPU_TO_VMCPU(pIemCpu), iDrReg, uNewDrX);
        AssertRCSuccessReturn(rc, RT_SUCCESS_NP(rc) ? VERR_INTERNAL_ERROR : rc);
    }
    else
        pCtx->dr[iDrReg] = uNewDrX;

    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * Implements RDTSC.
 */
IEM_CIMPL_DEF_0(iemCImpl_rdtsc)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Check preconditions.
     */
    if (!IEM_IS_INTEL_CPUID_FEATURE_PRESENT_EDX(X86_CPUID_FEATURE_EDX_TSC))
        return iemRaiseUndefinedOpcode(pIemCpu);

    if (   (pCtx->cr4 & X86_CR4_TSD)
        && pIemCpu->uCpl != 0)
    {
        Log(("rdtsc: CR4.TSD and CPL=%u -> #GP(0)\n", pIemCpu->uCpl));
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    }

    /*
     * Do the job.
     */
    uint64_t uTicks = TMCpuTickGet(IEMCPU_TO_VMCPU(pIemCpu));
    pCtx->rax = (uint32_t)uTicks;
    pCtx->rdx = uTicks >> 32;
#ifdef IEM_VERIFICATION_MODE
    pIemCpu->fIgnoreRaxRdx = true;
#endif

    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * Implements 'IN eAX, port'.
 *
 * @param   u16Port     The source port.
 * @param   cbReg       The register size.
 */
IEM_CIMPL_DEF_2(iemCImpl_in, uint16_t, u16Port, uint8_t, cbReg)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * CPL check
     */
    VBOXSTRICTRC rcStrict = iemHlpCheckPortIOPermission(pIemCpu, pCtx, u16Port, cbReg);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    /*
     * Perform the I/O.
     */
    uint32_t u32Value;
    if (!IEM_VERIFICATION_ENABLED(pIemCpu))
        rcStrict = IOMIOPortRead(IEMCPU_TO_VM(pIemCpu), u16Port, &u32Value, cbReg);
    else
        rcStrict = iemVerifyFakeIOPortRead(pIemCpu, u16Port, &u32Value, cbReg);
    if (IOM_SUCCESS(rcStrict))
    {
        switch (cbReg)
        {
            case 1: pCtx->al  = (uint8_t)u32Value;  break;
            case 2: pCtx->ax  = (uint16_t)u32Value; break;
            case 4: pCtx->rax = u32Value;           break;
            default: AssertFailedReturn(VERR_INTERNAL_ERROR_3);
        }
        iemRegAddToRip(pIemCpu, cbInstr);
        pIemCpu->cPotentialExits++;
    }
    /** @todo massage rcStrict. */
    return rcStrict;
}


/**
 * Implements 'IN eAX, DX'.
 *
 * @param   cbReg       The register size.
 */
IEM_CIMPL_DEF_1(iemCImpl_in_eAX_DX, uint8_t, cbReg)
{
    return IEM_CIMPL_CALL_2(iemCImpl_in, pIemCpu->CTX_SUFF(pCtx)->dx, cbReg);
}


/**
 * Implements 'OUT port, eAX'.
 *
 * @param   u16Port     The destination port.
 * @param   cbReg       The register size.
 */
IEM_CIMPL_DEF_2(iemCImpl_out, uint16_t, u16Port, uint8_t, cbReg)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * CPL check
     */
    if (   (pCtx->cr0 & X86_CR0_PE)
        && (    pIemCpu->uCpl > pCtx->eflags.Bits.u2IOPL
            ||  pCtx->eflags.Bits.u1VM) )
    {
        /** @todo I/O port permission bitmap check */
        AssertFailedReturn(VERR_NOT_IMPLEMENTED);
    }

    /*
     * Perform the I/O.
     */
    uint32_t u32Value;
    switch (cbReg)
    {
        case 1: u32Value = pCtx->al;  break;
        case 2: u32Value = pCtx->ax;  break;
        case 4: u32Value = pCtx->eax; break;
        default: AssertFailedReturn(VERR_INTERNAL_ERROR_3);
    }
    VBOXSTRICTRC rc;
    if (!IEM_VERIFICATION_ENABLED(pIemCpu))
        rc = IOMIOPortWrite(IEMCPU_TO_VM(pIemCpu), u16Port, u32Value, cbReg);
    else
        rc = iemVerifyFakeIOPortWrite(pIemCpu, u16Port, u32Value, cbReg);
    if (IOM_SUCCESS(rc))
    {
        iemRegAddToRip(pIemCpu, cbInstr);
        pIemCpu->cPotentialExits++;
        /** @todo massage rc. */
    }
    return rc;
}


/**
 * Implements 'OUT DX, eAX'.
 *
 * @param   cbReg       The register size.
 */
IEM_CIMPL_DEF_1(iemCImpl_out_DX_eAX, uint8_t, cbReg)
{
    return IEM_CIMPL_CALL_2(iemCImpl_out, pIemCpu->CTX_SUFF(pCtx)->dx, cbReg);
}


/**
 * Implements 'CLI'.
 */
IEM_CIMPL_DEF_0(iemCImpl_cli)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    if (pCtx->cr0 & X86_CR0_PE)
    {
        uint8_t const uIopl = pCtx->eflags.Bits.u2IOPL;
        if (!pCtx->eflags.Bits.u1VM)
        {
            if (pIemCpu->uCpl <= uIopl)
                pCtx->eflags.Bits.u1IF = 0;
            else if (   pIemCpu->uCpl == 3
                     && (pCtx->cr4 & X86_CR4_PVI) )
                pCtx->eflags.Bits.u1VIF = 0;
            else
                return iemRaiseGeneralProtectionFault0(pIemCpu);
        }
        /* V8086 */
        else if (uIopl == 3)
            pCtx->eflags.Bits.u1IF = 0;
        else if (   uIopl < 3
                 && (pCtx->cr4 & X86_CR4_VME) )
            pCtx->eflags.Bits.u1VIF = 0;
        else
            return iemRaiseGeneralProtectionFault0(pIemCpu);
    }
    /* real mode */
    else
        pCtx->eflags.Bits.u1IF = 0;
    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * Implements 'STI'.
 */
IEM_CIMPL_DEF_0(iemCImpl_sti)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    if (pCtx->cr0 & X86_CR0_PE)
    {
        uint8_t const uIopl = pCtx->eflags.Bits.u2IOPL;
        if (!pCtx->eflags.Bits.u1VM)
        {
            if (pIemCpu->uCpl <= uIopl)
                pCtx->eflags.Bits.u1IF = 1;
            else if (   pIemCpu->uCpl == 3
                     && (pCtx->cr4 & X86_CR4_PVI)
                     && !pCtx->eflags.Bits.u1VIP )
                pCtx->eflags.Bits.u1VIF = 1;
            else
                return iemRaiseGeneralProtectionFault0(pIemCpu);
        }
        /* V8086 */
        else if (uIopl == 3)
            pCtx->eflags.Bits.u1IF = 1;
        else if (   uIopl < 3
                 && (pCtx->cr4 & X86_CR4_VME)
                 && !pCtx->eflags.Bits.u1VIP )
            pCtx->eflags.Bits.u1VIF = 1;
        else
            return iemRaiseGeneralProtectionFault0(pIemCpu);
    }
    /* real mode */
    else
        pCtx->eflags.Bits.u1IF = 1;

    iemRegAddToRip(pIemCpu, cbInstr);
    EMSetInhibitInterruptsPC(IEMCPU_TO_VMCPU(pIemCpu), pCtx->rip);
    return VINF_SUCCESS;
}


/**
 * Implements 'HLT'.
 */
IEM_CIMPL_DEF_0(iemCImpl_hlt)
{
    if (pIemCpu->uCpl != 0)
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_EM_HALT;
}


/**
 * Implements 'CPUID'.
 */
IEM_CIMPL_DEF_0(iemCImpl_cpuid)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    CPUMGetGuestCpuId(IEMCPU_TO_VMCPU(pIemCpu), pCtx->eax, &pCtx->eax, &pCtx->ebx, &pCtx->ecx, &pCtx->edx);
    pCtx->rax &= UINT32_C(0xffffffff);
    pCtx->rbx &= UINT32_C(0xffffffff);
    pCtx->rcx &= UINT32_C(0xffffffff);
    pCtx->rdx &= UINT32_C(0xffffffff);

    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * Implements 'AAD'.
 *
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_1(iemCImpl_aad, uint8_t, bImm)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    uint16_t const ax = pCtx->ax;
    uint8_t const  al = (uint8_t)ax + (uint8_t)(ax >> 8) * bImm;
    pCtx->ax = al;
    iemHlpUpdateArithEFlagsU8(pIemCpu, al,
                              X86_EFL_SF | X86_EFL_SF | X86_EFL_PF,
                              X86_EFL_OF | X86_EFL_AF | X86_EFL_CF);

    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}




/*
 * Instantiate the various string operation combinations.
 */
#define OP_SIZE     8
#define ADDR_SIZE   16
#include "IEMAllCImplStrInstr.cpp.h"
#define OP_SIZE     8
#define ADDR_SIZE   32
#include "IEMAllCImplStrInstr.cpp.h"
#define OP_SIZE     8
#define ADDR_SIZE   64
#include "IEMAllCImplStrInstr.cpp.h"

#define OP_SIZE     16
#define ADDR_SIZE   16
#include "IEMAllCImplStrInstr.cpp.h"
#define OP_SIZE     16
#define ADDR_SIZE   32
#include "IEMAllCImplStrInstr.cpp.h"
#define OP_SIZE     16
#define ADDR_SIZE   64
#include "IEMAllCImplStrInstr.cpp.h"

#define OP_SIZE     32
#define ADDR_SIZE   16
#include "IEMAllCImplStrInstr.cpp.h"
#define OP_SIZE     32
#define ADDR_SIZE   32
#include "IEMAllCImplStrInstr.cpp.h"
#define OP_SIZE     32
#define ADDR_SIZE   64
#include "IEMAllCImplStrInstr.cpp.h"

#define OP_SIZE     64
#define ADDR_SIZE   32
#include "IEMAllCImplStrInstr.cpp.h"
#define OP_SIZE     64
#define ADDR_SIZE   64
#include "IEMAllCImplStrInstr.cpp.h"


/**
 * Implements 'FINIT' and 'FNINIT'.
 *
 * @param   fCheckXcpts     Whether to check for umasked pending exceptions or
 *                          not.
 */
IEM_CIMPL_DEF_1(iemCImpl_finit, bool, fCheckXcpts)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    if (pCtx->cr0 & (X86_CR0_EM | X86_CR0_TS))
        return iemRaiseDeviceNotAvailable(pIemCpu);
    /** @todo trigger pending exceptions:
        if (fCheckXcpts && TODO )
        return iemRaiseMathFault(pIemCpu);
     */

    if (iemFRegIsFxSaveFormat(pIemCpu))
    {
        pCtx->fpu.FCW   = 0x37f;
        pCtx->fpu.FSW   = 0;
        pCtx->fpu.FTW   = 0x00;         /* 0 - empty. */
        pCtx->fpu.FPUDP = 0;
        pCtx->fpu.DS    = 0; //??
        pCtx->fpu.FPUIP = 0;
        pCtx->fpu.CS    = 0; //??
        pCtx->fpu.FOP   = 0;
    }
    else
    {
        PX86FPUSTATE pFpu = (PX86FPUSTATE)&pCtx->fpu;
        pFpu->FCW       = 0x37f;
        pFpu->FSW       = 0;
        pFpu->FTW       = 0xffff;       /* 11 - empty */
        pFpu->FPUOO     = 0; //??
        pFpu->FPUOS     = 0; //??
        pFpu->FPUIP     = 0;
        pFpu->CS        = 0; //??
        pFpu->FOP       = 0;
    }

    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}


/** @} */

