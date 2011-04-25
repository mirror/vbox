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
 * @param   uSel        The selector.
 * @param   offSeg      The segment offset.
 */
IEM_CIMPL_DEF_2(iemCImpl_FarJmp, uint16_t, uSel, uint32_t, offSeg)
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

        if (pIemCpu->enmEffOpSize == IEMMODE_16BIT) /** @todo WRONG, must pass this. */
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
    if (!Desc.Legacy.Gen.u1Present)
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
            return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
        }

        /* L vs D. */
        if (   Desc.Legacy.Gen.u1Long
            && Desc.Legacy.Gen.u1DefBig
            && IEM_IS_LONG_MODE(pIemCpu))
        {
            Log(("jmpf %04x:%08x -> both L and D are set.\n", uSel, offSeg));
            return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
        }

        /* DPL/RPL/CPL check, where conforming segments makes a difference. */
        if (!(Desc.Legacy.Gen.u4Type & X86_SEL_TYPE_CONF))
        {
            if (Desc.Legacy.Gen.u2Dpl > pIemCpu->uCpl)
            {
                Log(("jmpf %04x:%08x -> DPL violation (conforming); DPL=%d CPL=%u\n",
                     uSel, offSeg, Desc.Legacy.Gen.u2Dpl, pIemCpu->uCpl));
                return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
            }
        }
        else
        {
            if (Desc.Legacy.Gen.u2Dpl != pIemCpu->uCpl)
            {
                Log(("jmpf %04x:%08x -> CPL != DPL; DPL=%d CPL=%u\n", uSel, offSeg, Desc.Legacy.Gen.u2Dpl, pIemCpu->uCpl));
                return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
            }
            if ((uSel & X86_SEL_RPL) > pIemCpu->uCpl)
            {
                Log(("jmpf %04x:%08x -> RPL > DPL; RPL=%d CPL=%u\n", uSel, offSeg, (uSel & X86_SEL_RPL), pIemCpu->uCpl));
                return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
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
                return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
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
            Desc.Legacy.Gen.u4Type |= X86_SEL_TYPE_ACCESSED;
        }

        /* commit */
        pCtx->rip = offSeg;
        pCtx->cs  = uSel & (X86_SEL_MASK | X86_SEL_LDT);
        pCtx->cs |= pIemCpu->uCpl; /** @todo is this right for conforming segs? or in general? */
        pCtx->csHid.Attr.u   = (Desc.Legacy.u >> (16+16+8)) & UINT32_C(0xf0ff);
#ifdef IEM_VERIFICATION_MODE
        pCtx->csHid.Attr.u &= ~(uint32_t)X86_SEL_TYPE_ACCESSED; /** @todo check what VT-x and AMD-V does here. */
#endif
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
                return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));

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
            return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
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
        uint16_t uNewCs;
        if (enmEffOpSize == IEMMODE_32BIT)
        {
            uNewCs  = pu16Frame[2];
            uNewEip = RT_MAKE_U32(pu16Frame[0], pu16Frame[1]);
        }
        else
        {
            uNewCs  = pu16Frame[1];
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
        pCtx->cs            = uNewCs;
        pCtx->csHid.u64Base = (uint32_t)uNewCs << 4;
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
 * Implements int3 and int XX.
 *
 * @param   u8Int       The interrupt vector number.
 * @param   fIsBpInstr  Is it the breakpoint instruction.
 */
IEM_CIMPL_DEF_2(iemCImpl_int, uint8_t, u8Int, bool, fIsBpInstr)
{
    /** @todo we should call TRPM to do this job.  */
    VBOXSTRICTRC    rcStrict;
    PCPUMCTX        pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Real mode is easy.
     */
    if (   pIemCpu->enmCpuMode == IEMMODE_16BIT
        && IEM_IS_REAL_MODE(pIemCpu))
    {
        /* read the IDT entry. */
        if (pCtx->idtr.cbIdt < UINT32_C(4) * u8Int + 3)
            return iemRaiseGeneralProtectionFault(pIemCpu, X86_TRAP_ERR_IDT | ((uint16_t)u8Int << X86_TRAP_ERR_SEL_SHIFT));
        RTFAR16 Idte;
        rcStrict = iemMemFetchDataU32(pIemCpu, (uint32_t *)&Idte, UINT8_MAX, pCtx->idtr.pIdt + UINT32_C(4) * u8Int);
        if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
            return rcStrict;

        /* push the stack frame. */
        uint16_t *pu16Frame;
        uint64_t  uNewRsp;
        rcStrict = iemMemStackPushBeginSpecial(pIemCpu, 6, (void **)&pu16Frame, &uNewRsp);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;

        pu16Frame[2] = (uint16_t)pCtx->eflags.u;
        pu16Frame[1] = (uint16_t)pCtx->cs;
        pu16Frame[0] = pCtx->ip + cbInstr;
        rcStrict = iemMemStackPushCommitSpecial(pIemCpu, pu16Frame, uNewRsp);
        if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
            return rcStrict;

        /* load the vector address into cs:ip. */
        pCtx->cs               = Idte.sel;
        pCtx->csHid.u64Base    = (uint32_t)Idte.sel << 4;
        /** @todo do we load attribs and limit as well? Should we check against limit like far jump? */
        pCtx->rip              = Idte.off;
        pCtx->eflags.Bits.u1IF = 0;
        return VINF_SUCCESS;
    }

    AssertFailed();
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Implements iret.
 *
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_1(iemCImpl_iret, IEMMODE, enmEffOpSize)
{
    PCPUMCTX        pCtx = pIemCpu->CTX_SUFF(pCtx);
    VBOXSTRICTRC    rcStrict;
    uint64_t        uNewRsp;

    /*
     * Real mode is easy, V8086 mode is relative similar.
     */
    if (   pIemCpu->enmCpuMode == IEMMODE_16BIT
        && IEM_IS_REAL_OR_V86_MODE(pIemCpu))
    {
        /* iret throws an exception if VME isn't enabled.  */
        if (   pCtx->eflags.Bits.u1VM
            && !(pCtx->cr4 & X86_CR4_VME))
            return iemRaiseGeneralProtectionFault0(pIemCpu);

        /* Do the stack bits, but don't commit RSP before everything checks
           out right. */
        union
        {
            uint32_t const *pu32;
            uint16_t const *pu16;
            void const     *pv;
        } uFrame;
        Assert(enmEffOpSize == IEMMODE_32BIT || enmEffOpSize == IEMMODE_16BIT);
        uint16_t uNewCs;
        uint32_t uNewEip;
        uint32_t uNewFlags;
        if (enmEffOpSize == IEMMODE_32BIT)
        {
            rcStrict = iemMemStackPopBeginSpecial(pIemCpu, 12, &uFrame.pv, &uNewRsp);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
            uNewEip    = uFrame.pu32[0];
            uNewCs     = (uint16_t)uFrame.pu32[1];
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
            uNewCs     = uFrame.pu16[1];
            uNewFlags  = uFrame.pu16[2];
            uNewFlags &= X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF
                       | X86_EFL_TF | X86_EFL_IF | X86_EFL_DF | X86_EFL_OF | X86_EFL_IOPL | X86_EFL_NT;
            uNewFlags |= pCtx->eflags.u & (UINT16_C(0xffff0000) | X86_EFL_1);
            /** @todo The intel pseudo code does not indicate what happens to
             *        reserved flags. We just ignore them. */
        }
        /** @todo Check how this is supposed to work if sp=0xfffe. */

        /* Check the limit of the new EIP. */
        /** @todo Only the AMD pseudo code check the limit here, what's
         *        right? */
        if (uNewEip > pCtx->csHid.u32Limit)
            return iemRaiseSelectorBounds(pIemCpu, X86_SREG_CS, IEM_ACCESS_INSTRUCTION);

        /* V8086 checks and flag adjustments */
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

        /* commit the operation. */
        rcStrict = iemMemStackPopCommitSpecial(pIemCpu, uFrame.pv, uNewRsp);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
        pCtx->rip           = uNewEip;
        pCtx->cs            = uNewCs;
        pCtx->csHid.u64Base = (uint32_t)uNewCs << 4;
        /** @todo do we load attribs and limit as well? */
        Assert(uNewFlags & X86_EFL_1);
        pCtx->eflags.u      = uNewFlags;

        return VINF_SUCCESS;
    }


    AssertFailed();
    return VERR_NOT_IMPLEMENTED;
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
        return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
    }
    if (iSegReg == X86_SREG_SS) /* SS gets different treatment */
    {
        if (   (Desc.Legacy.Gen.u4Type & X86_SEL_TYPE_CODE)
            || !(Desc.Legacy.Gen.u4Type & X86_SEL_TYPE_WRITE) )
        {
            Log(("load sreg SS, %#x - code or read only (%#x) -> #GP\n", uSel, Desc.Legacy.Gen.u4Type));
            return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
        }
        if (    (Desc.Legacy.Gen.u4Type & X86_SEL_TYPE_CODE)
            || !(Desc.Legacy.Gen.u4Type & X86_SEL_TYPE_WRITE) )
        {
            Log(("load sreg SS, %#x - code or read only (%#x) -> #GP\n", uSel, Desc.Legacy.Gen.u4Type));
            return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
        }
        if ((uSel & X86_SEL_RPL) != pIemCpu->uCpl)
        {
            Log(("load sreg SS, %#x - RPL and CPL (%d) differs -> #GP\n", uSel, pIemCpu->uCpl));
            return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
        }
        if (Desc.Legacy.Gen.u2Dpl != pIemCpu->uCpl)
        {
            Log(("load sreg SS, %#x - DPL (%d) and CPL (%d) differs -> #GP\n", uSel, Desc.Legacy.Gen.u2Dpl, pIemCpu->uCpl));
            return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
        }
    }
    else
    {
        if ((Desc.Legacy.Gen.u4Type & (X86_SEL_TYPE_CODE | X86_SEL_TYPE_READ)) == X86_SEL_TYPE_CODE)
        {
            Log(("load sreg%u, %#x - execute only segment -> #GP\n", iSegReg, uSel));
            return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
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
                return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
            }
#else /* this is what makes more sense. */
            if ((unsigned)(uSel & X86_SEL_RPL) > Desc.Legacy.Gen.u2Dpl)
            {
                Log(("load sreg%u, %#x - RPL (%d) is greater than DPL (%d) -> #GP\n",
                     iSegReg, uSel, (uSel & X86_SEL_RPL), Desc.Legacy.Gen.u2Dpl));
                return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
            }
            if (pIemCpu->uCpl > Desc.Legacy.Gen.u2Dpl)
            {
                Log(("load sreg%u, %#x - CPL (%d) is greater than DPL (%d) -> #GP\n",
                     iSegReg, uSel, pIemCpu->uCpl, Desc.Legacy.Gen.u2Dpl));
                return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
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
IEM_CIMPL_DEF_2(iemOpCImpl_pop_Sreg, uint8_t, iSegReg, IEMMODE, enmEffOpSize)
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
        if (IEM_VERIFICATION_ENABLED(pIemCpu))
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
        if (IEM_VERIFICATION_ENABLED(pIemCpu))
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
            if (IEM_VERIFICATION_ENABLED(pIemCpu))
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
 * Implements mov CRx,GReg.
 *
 * @param   iCrReg          The CRx register to read (valid).
 * @param   iGReg           The general register to store the CRx value in.
 */
IEM_CIMPL_DEF_2(iemCImpl_mov_Cd_Rd, uint8_t, iCrReg, uint8_t, iGReg)
{
    PCPUMCTX        pCtx  = pIemCpu->CTX_SUFF(pCtx);
    PVMCPU          pVCpu = IEMCPU_TO_VMCPU(pIemCpu);
    VBOXSTRICTRC    rcStrict;
    int             rc;

    if (pIemCpu->uCpl != 0)
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    Assert(!pCtx->eflags.Bits.u1VM);

    /*
     * Read the new value from the source register.
     */
    uint64_t NewCrX;
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
        NewCrX = iemGRegFetchU64(pIemCpu, iGReg);
    else
        NewCrX = iemGRegFetchU32(pIemCpu, iGReg);

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
            uint64_t const OldCrX = pCtx->cr0;
            NewCrX |= X86_CR0_ET; /* hardcoded */

            /* Check for reserved bits. */
            uint32_t const fValid = X86_CR0_PE | X86_CR0_MP | X86_CR0_EM | X86_CR0_TS
                                  | X86_CR0_ET | X86_CR0_NE | X86_CR0_WP | X86_CR0_AM
                                  | X86_CR0_NW | X86_CR0_CD | X86_CR0_PG;
            if (NewCrX & ~(uint64_t)fValid)
            {
                Log(("Trying to set reserved CR0 bits: NewCR0=%#llx InvalidBits=%#llx\n", NewCrX, NewCrX & ~(uint64_t)fValid));
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            }

            /* Check for invalid combinations. */
            if (    (NewCrX & X86_CR0_PG)
                && !(NewCrX & X86_CR0_PE) )
            {
                Log(("Trying to set CR0.PG without CR0.PE\n"));
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            }

            if (   !(NewCrX & X86_CR0_CD)
                && (NewCrX & X86_CR0_NW) )
            {
                Log(("Trying to clear CR0.CD while leaving CR0.NW set\n"));
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            }

            /* Long mode consistency checks. */
            if (    (NewCrX & X86_CR0_PG)
                && !(OldCrX & X86_CR0_PG)
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
            if (IEM_VERIFICATION_ENABLED(pIemCpu))
            {
                rc = CPUMSetGuestCR0(pVCpu, NewCrX);
                AssertRCSuccessReturn(rc, RT_FAILURE_NP(rc) ? rc : VERR_INTERNAL_ERROR_3);
            }
            else
                pCtx->cr0 = NewCrX;
            Assert(pCtx->cr0 == NewCrX);

            /*
             * Change EFER.LMA if entering or leaving long mode.
             */
            if (   (NewCrX & X86_CR0_PG) != (OldCrX & X86_CR0_PG)
                && (pCtx->msrEFER & MSR_K6_EFER_LME) )
            {
                uint64_t NewEFER = pCtx->msrEFER;
                if (NewCrX & X86_CR0_PG)
                    NewEFER |= MSR_K6_EFER_LME;
                else
                    NewEFER &= ~MSR_K6_EFER_LME;

                if (IEM_VERIFICATION_ENABLED(pIemCpu))
                    CPUMSetGuestEFER(pVCpu, NewEFER);
                else
                    pCtx->msrEFER = NewEFER;
                Assert(pCtx->msrEFER == NewEFER);
            }

            /*
             * Inform PGM.
             */
            if (IEM_VERIFICATION_ENABLED(pIemCpu))
            {
                if (    (NewCrX & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE))
                    !=  (OldCrX & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE)) )
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
            pCtx->cr2 = NewCrX;
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
            if (NewCrX & UINT64_C(0xfff0000000000000))
            {
                Log(("Trying to load CR3 with invalid high bits set: %#llx\n", NewCrX));
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
            if (NewCrX & ~fValid)
            {
                Log(("Automatically clearing reserved bits in CR3 load: NewCR3=%#llx ClearedBits=%#llx\n",
                     NewCrX, NewCrX & ~fValid));
                NewCrX &= fValid;
            }

            /** @todo If we're in PAE mode we should check the PDPTRs for
             *        invalid bits. */

            /* Make the change. */
            if (IEM_VERIFICATION_ENABLED(pIemCpu))
            {
                rc = CPUMSetGuestCR3(pVCpu, NewCrX);
                AssertRCSuccessReturn(rc, rc);
            }
            else
                pCtx->cr3 = NewCrX;

            /* Inform PGM. */
            if (IEM_VERIFICATION_ENABLED(pIemCpu))
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
            uint64_t const OldCrX = pCtx->cr0;

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
            if (NewCrX & ~(uint64_t)fValid)
            {
                Log(("Trying to set reserved CR4 bits: NewCR4=%#llx InvalidBits=%#llx\n", NewCrX, NewCrX & ~(uint64_t)fValid));
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            }

            /* long mode checks. */
            if (   (OldCrX & X86_CR4_PAE)
                && !(NewCrX & X86_CR4_PAE)
                && (pCtx->msrEFER & MSR_K6_EFER_LMA) )
            {
                Log(("Trying to set clear CR4.PAE while long mode is active\n"));
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            }


            /*
             * Change it.
             */
            if (IEM_VERIFICATION_ENABLED(pIemCpu))
            {
                rc = CPUMSetGuestCR4(pVCpu, NewCrX);
                AssertRCSuccessReturn(rc, rc);
            }
            else
                pCtx->cr4 = NewCrX;
            Assert(pCtx->cr4 == NewCrX);

            /*
             * Notify SELM and PGM.
             */
            if (IEM_VERIFICATION_ENABLED(pIemCpu))
            {
                /* SELM - VME may change things wrt to the TSS shadowing. */
                if ((NewCrX ^ OldCrX) & X86_CR4_VME)
                    VMCPU_FF_SET(pVCpu, VMCPU_FF_SELM_SYNC_TSS);

                /* PGM - flushing and mode. */
                if (    (NewCrX & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE))
                    !=  (OldCrX & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE)) )
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
            if (IEM_VERIFICATION_ENABLED(pIemCpu))
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
    if (IEM_VERIFICATION_ENABLED(pIemCpu))
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
    if (IEM_VERIFICATION_ENABLED(pIemCpu))
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


/** @} */

