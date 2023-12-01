/* $Id$ */
/** @file
 * IEM - Interpreted Execution Manager - R/W Memory Functions Template.
 */

/*
 * Copyright (C) 2011-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/* Check template parameters. */
#ifndef TMPL_MEM_TYPE
# error "TMPL_MEM_TYPE is undefined"
#endif
#ifndef TMPL_MEM_TYPE_ALIGN
# define TMPL_MEM_TYPE_ALIGN     (sizeof(TMPL_MEM_TYPE) - 1)
#endif
#ifndef TMPL_MEM_FN_SUFF
# error "TMPL_MEM_FN_SUFF is undefined"
#endif
#ifndef TMPL_MEM_FMT_TYPE
# error "TMPL_MEM_FMT_TYPE is undefined"
#endif
#ifndef TMPL_MEM_FMT_DESC
# error "TMPL_MEM_FMT_DESC is undefined"
#endif


/**
 * Standard fetch function.
 *
 * This is used by CImpl code, so it needs to be kept even when IEM_WITH_SETJMP
 * is defined.
 */
VBOXSTRICTRC RT_CONCAT(iemMemFetchData,TMPL_MEM_FN_SUFF)(PVMCPUCC pVCpu, TMPL_MEM_TYPE *puDst,
                                                         uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT
{
    /* The lazy approach for now... */
    TMPL_MEM_TYPE const *puSrc;
    VBOXSTRICTRC rc = iemMemMap(pVCpu, (void **)&puSrc, sizeof(*puSrc), iSegReg, GCPtrMem,
                                IEM_ACCESS_DATA_R, TMPL_MEM_TYPE_ALIGN);
    if (rc == VINF_SUCCESS)
    {
        *puDst = *puSrc;
        rc = iemMemCommitAndUnmap(pVCpu, (void *)puSrc, IEM_ACCESS_DATA_R);
        Log2(("IEM RD " TMPL_MEM_FMT_DESC " %d|%RGv: " TMPL_MEM_FMT_TYPE "\n", iSegReg, GCPtrMem, *puDst));
    }
    return rc;
}


#ifdef IEM_WITH_SETJMP
/**
 * Safe/fallback fetch function that longjmps on error.
 */
# ifdef TMPL_MEM_BY_REF
void
RT_CONCAT3(iemMemFetchData,TMPL_MEM_FN_SUFF,SafeJmp)(PVMCPUCC pVCpu, TMPL_MEM_TYPE *pDst, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP
{
#  if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3)
    pVCpu->iem.s.DataTlb.cTlbSafeReadPath++;
#  endif
    TMPL_MEM_TYPE const *pSrc = (TMPL_MEM_TYPE const *)iemMemMapJmp(pVCpu, sizeof(*pSrc), iSegReg, GCPtrMem,
                                                                    IEM_ACCESS_DATA_R, TMPL_MEM_TYPE_ALIGN);
    *pDst = *pSrc;
    iemMemCommitAndUnmapJmp(pVCpu, (void *)pSrc, IEM_ACCESS_DATA_R);
    Log2(("IEM RD " TMPL_MEM_FMT_DESC " %d|%RGv: " TMPL_MEM_FMT_TYPE "\n", iSegReg, GCPtrMem, pDst));
}
# else /* !TMPL_MEM_BY_REF */
TMPL_MEM_TYPE
RT_CONCAT3(iemMemFetchData,TMPL_MEM_FN_SUFF,SafeJmp)(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP
{
#  if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3)
    pVCpu->iem.s.DataTlb.cTlbSafeReadPath++;
#  endif
    TMPL_MEM_TYPE const *puSrc = (TMPL_MEM_TYPE const *)iemMemMapJmp(pVCpu, sizeof(*puSrc), iSegReg, GCPtrMem,
                                                                     IEM_ACCESS_DATA_R, TMPL_MEM_TYPE_ALIGN);
    TMPL_MEM_TYPE const  uRet = *puSrc;
    iemMemCommitAndUnmapJmp(pVCpu, (void *)puSrc, IEM_ACCESS_DATA_R);
    Log2(("IEM RD " TMPL_MEM_FMT_DESC " %d|%RGv: " TMPL_MEM_FMT_TYPE "\n", iSegReg, GCPtrMem, uRet));
    return uRet;
}
# endif /* !TMPL_MEM_BY_REF */
#endif /* IEM_WITH_SETJMP */



/**
 * Standard store function.
 *
 * This is used by CImpl code, so it needs to be kept even when IEM_WITH_SETJMP
 * is defined.
 */
VBOXSTRICTRC RT_CONCAT(iemMemStoreData,TMPL_MEM_FN_SUFF)(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem,
#ifdef TMPL_MEM_BY_REF
                                                         TMPL_MEM_TYPE const *pValue) RT_NOEXCEPT
#else
                                                         TMPL_MEM_TYPE uValue) RT_NOEXCEPT
#endif
{
    /* The lazy approach for now... */
    TMPL_MEM_TYPE *puDst;
    VBOXSTRICTRC rc = iemMemMap(pVCpu, (void **)&puDst, sizeof(*puDst), iSegReg, GCPtrMem, IEM_ACCESS_DATA_W, TMPL_MEM_TYPE_ALIGN);
    if (rc == VINF_SUCCESS)
    {
#ifdef TMPL_MEM_BY_REF
        *puDst = *pValue;
#else
        *puDst = uValue;
#endif
        rc = iemMemCommitAndUnmap(pVCpu, puDst, IEM_ACCESS_DATA_W);
#ifdef TMPL_MEM_BY_REF
        Log6(("IEM WR " TMPL_MEM_FMT_DESC " %d|%RGv: " TMPL_MEM_FMT_TYPE "\n", iSegReg, GCPtrMem, pValue));
#else
        Log6(("IEM WR " TMPL_MEM_FMT_DESC " %d|%RGv: " TMPL_MEM_FMT_TYPE "\n", iSegReg, GCPtrMem, uValue));
#endif
    }
    return rc;
}


#ifdef IEM_WITH_SETJMP
/**
 * Stores a data byte, longjmp on error.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 * @param   uValue              The value to store.
 */
void RT_CONCAT3(iemMemStoreData,TMPL_MEM_FN_SUFF,SafeJmp)(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem,
#ifdef TMPL_MEM_BY_REF
                                                          TMPL_MEM_TYPE const *pValue) IEM_NOEXCEPT_MAY_LONGJMP
#else
                                                          TMPL_MEM_TYPE uValue) IEM_NOEXCEPT_MAY_LONGJMP
#endif
{
# if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3)
    pVCpu->iem.s.DataTlb.cTlbSafeWritePath++;
# endif
#ifdef TMPL_MEM_BY_REF
    Log6(("IEM WR " TMPL_MEM_FMT_DESC " %d|%RGv: " TMPL_MEM_FMT_TYPE "\n", iSegReg, GCPtrMem, pValue));
#else
    Log6(("IEM WR " TMPL_MEM_FMT_DESC " %d|%RGv: " TMPL_MEM_FMT_TYPE "\n", iSegReg, GCPtrMem, uValue));
#endif
    TMPL_MEM_TYPE *puDst = (TMPL_MEM_TYPE *)iemMemMapJmp(pVCpu, sizeof(*puDst), iSegReg, GCPtrMem,
                                                         IEM_ACCESS_DATA_W, TMPL_MEM_TYPE_ALIGN);
#ifdef TMPL_MEM_BY_REF
    *puDst = *pValue;
#else
    *puDst = uValue;
#endif
    iemMemCommitAndUnmapJmp(pVCpu, puDst, IEM_ACCESS_DATA_W);
}
#endif /* IEM_WITH_SETJMP */


#ifdef IEM_WITH_SETJMP

/**
 * Maps a data buffer for read+write direct access (or via a bounce buffer),
 * longjmp on error.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pbUnmapInfo         Pointer to unmap info variable.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 */
TMPL_MEM_TYPE *
RT_CONCAT3(iemMemMapData,TMPL_MEM_FN_SUFF,RwSafeJmp)(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                     uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP
{
# if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3)
    pVCpu->iem.s.DataTlb.cTlbSafeWritePath++;
# endif
    Log8(("IEM RW/map " TMPL_MEM_FMT_DESC " %d|%RGv\n", iSegReg, GCPtrMem));
    *pbUnmapInfo = 1 | ((IEM_ACCESS_TYPE_READ  | IEM_ACCESS_TYPE_WRITE) << 4); /* zero is for the TLB hit */
    return (TMPL_MEM_TYPE *)iemMemMapJmp(pVCpu, sizeof(TMPL_MEM_TYPE), iSegReg, GCPtrMem, IEM_ACCESS_DATA_RW, TMPL_MEM_TYPE_ALIGN);
}


/**
 * Maps a data buffer for writeonly direct access (or via a bounce buffer),
 * longjmp on error.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pbUnmapInfo         Pointer to unmap info variable.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 */
TMPL_MEM_TYPE *
RT_CONCAT3(iemMemMapData,TMPL_MEM_FN_SUFF,WoSafeJmp)(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                     uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP
{
# if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3)
    pVCpu->iem.s.DataTlb.cTlbSafeWritePath++;
# endif
    Log8(("IEM WO/map " TMPL_MEM_FMT_DESC " %d|%RGv\n", iSegReg, GCPtrMem));
    *pbUnmapInfo = 1 | (IEM_ACCESS_TYPE_WRITE << 4); /* zero is for the TLB hit */
    return (TMPL_MEM_TYPE *)iemMemMapJmp(pVCpu, sizeof(TMPL_MEM_TYPE), iSegReg, GCPtrMem, IEM_ACCESS_DATA_W, TMPL_MEM_TYPE_ALIGN);
}


/**
 * Maps a data buffer for readonly direct access (or via a bounce buffer),
 * longjmp on error.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pbUnmapInfo         Pointer to unmap info variable.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 */
TMPL_MEM_TYPE const *
RT_CONCAT3(iemMemMapData,TMPL_MEM_FN_SUFF,RoSafeJmp)(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                     uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP
{
# if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3)
    pVCpu->iem.s.DataTlb.cTlbSafeWritePath++;
# endif
    Log4(("IEM RO/map " TMPL_MEM_FMT_DESC " %d|%RGv\n", iSegReg, GCPtrMem));
    *pbUnmapInfo = 1 | (IEM_ACCESS_TYPE_READ << 4); /* zero is for the TLB hit */
    return (TMPL_MEM_TYPE *)iemMemMapJmp(pVCpu, sizeof(TMPL_MEM_TYPE), iSegReg, GCPtrMem, IEM_ACCESS_DATA_R, TMPL_MEM_TYPE_ALIGN);
}

#endif /* IEM_WITH_SETJMP */


#ifdef TMPL_MEM_WITH_STACK

/**
 * Pushes an item onto the stack, regular version.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the
 *                              calling thread.
 * @param   uValue              The value to push.
 */
VBOXSTRICTRC RT_CONCAT(iemMemStackPush,TMPL_MEM_FN_SUFF)(PVMCPUCC pVCpu, TMPL_MEM_TYPE uValue) RT_NOEXCEPT
{
    /* Increment the stack pointer. */
    uint64_t    uNewRsp;
    RTGCPTR     GCPtrTop = iemRegGetRspForPush(pVCpu, sizeof(TMPL_MEM_TYPE), &uNewRsp);

    /* Write the dword the lazy way. */
    TMPL_MEM_TYPE *puDst;
    VBOXSTRICTRC rc = iemMemMap(pVCpu, (void **)&puDst, sizeof(TMPL_MEM_TYPE), X86_SREG_SS, GCPtrTop,
                                IEM_ACCESS_STACK_W, TMPL_MEM_TYPE_ALIGN);
    if (rc == VINF_SUCCESS)
    {
        *puDst = uValue;
        rc = iemMemCommitAndUnmap(pVCpu, puDst, IEM_ACCESS_STACK_W);

        /* Commit the new RSP value unless we an access handler made trouble. */
        if (rc == VINF_SUCCESS)
        {
            Log12(("IEM WR " TMPL_MEM_FMT_DESC " SS|%RGv (%RX64->%RX64): " TMPL_MEM_FMT_TYPE "\n",
                   GCPtrTop, pVCpu->cpum.GstCtx.rsp, uNewRsp, uValue));
            pVCpu->cpum.GstCtx.rsp = uNewRsp;
            return VINF_SUCCESS;
        }
    }

    return rc;
}


/**
 * Pops an item off the stack.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the
 *                              calling thread.
 * @param   puValue             Where to store the popped value.
 */
VBOXSTRICTRC RT_CONCAT(iemMemStackPop,TMPL_MEM_FN_SUFF)(PVMCPUCC pVCpu, TMPL_MEM_TYPE *puValue) RT_NOEXCEPT
{
    /* Increment the stack pointer. */
    uint64_t    uNewRsp;
    RTGCPTR     GCPtrTop = iemRegGetRspForPop(pVCpu, sizeof(TMPL_MEM_TYPE), &uNewRsp);

    /* Write the word the lazy way. */
    TMPL_MEM_TYPE const *puSrc;
    VBOXSTRICTRC rc = iemMemMap(pVCpu, (void **)&puSrc, sizeof(TMPL_MEM_TYPE), X86_SREG_SS, GCPtrTop,
                                IEM_ACCESS_STACK_R, TMPL_MEM_TYPE_ALIGN);
    if (rc == VINF_SUCCESS)
    {
        *puValue = *puSrc;
        rc = iemMemCommitAndUnmap(pVCpu, (void *)puSrc, IEM_ACCESS_STACK_R);

        /* Commit the new RSP value. */
        if (rc == VINF_SUCCESS)
        {
            Log10(("IEM RD " TMPL_MEM_FMT_DESC " SS|%RGv (%RX64->%RX64): " TMPL_MEM_FMT_TYPE "\n",
                   GCPtrTop, pVCpu->cpum.GstCtx.rsp, uNewRsp, *puValue));
            pVCpu->cpum.GstCtx.rsp = uNewRsp;
            return VINF_SUCCESS;
        }
    }
    return rc;
}


/**
 * Pushes an item onto the stack, using a temporary stack pointer.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the
 *                              calling thread.
 * @param   uValue              The value to push.
 * @param   pTmpRsp             Pointer to the temporary stack pointer.
 */
VBOXSTRICTRC RT_CONCAT3(iemMemStackPush,TMPL_MEM_FN_SUFF,Ex)(PVMCPUCC pVCpu, TMPL_MEM_TYPE uValue, PRTUINT64U pTmpRsp) RT_NOEXCEPT
{
    /* Increment the stack pointer. */
    RTUINT64U   NewRsp = *pTmpRsp;
    RTGCPTR     GCPtrTop = iemRegGetRspForPushEx(pVCpu, &NewRsp, sizeof(TMPL_MEM_TYPE));

    /* Write the word the lazy way. */
    TMPL_MEM_TYPE *puDst;
    VBOXSTRICTRC rc = iemMemMap(pVCpu, (void **)&puDst, sizeof(TMPL_MEM_TYPE), X86_SREG_SS, GCPtrTop,
                                IEM_ACCESS_STACK_W, TMPL_MEM_TYPE_ALIGN);
    if (rc == VINF_SUCCESS)
    {
        *puDst = uValue;
        rc = iemMemCommitAndUnmap(pVCpu, puDst, IEM_ACCESS_STACK_W);

        /* Commit the new RSP value unless we an access handler made trouble. */
        if (rc == VINF_SUCCESS)
        {
            Log12(("IEM WR " TMPL_MEM_FMT_DESC " SS|%RGv (%RX64->%RX64): " TMPL_MEM_FMT_TYPE " [ex]\n",
                   GCPtrTop, pTmpRsp->u, NewRsp.u, uValue));
            *pTmpRsp = NewRsp;
            return VINF_SUCCESS;
        }
    }
    return rc;
}


/**
 * Pops an item off the stack, using a temporary stack pointer.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the
 *                              calling thread.
 * @param   puValue             Where to store the popped value.
 * @param   pTmpRsp             Pointer to the temporary stack pointer.
 */
VBOXSTRICTRC
RT_CONCAT3(iemMemStackPop,TMPL_MEM_FN_SUFF,Ex)(PVMCPUCC pVCpu, TMPL_MEM_TYPE *puValue, PRTUINT64U pTmpRsp) RT_NOEXCEPT
{
    /* Increment the stack pointer. */
    RTUINT64U   NewRsp = *pTmpRsp;
    RTGCPTR     GCPtrTop = iemRegGetRspForPopEx(pVCpu, &NewRsp, sizeof(TMPL_MEM_TYPE));

    /* Write the word the lazy way. */
    TMPL_MEM_TYPE const *puSrc;
    VBOXSTRICTRC rc = iemMemMap(pVCpu, (void **)&puSrc, sizeof(TMPL_MEM_TYPE), X86_SREG_SS, GCPtrTop,
                                IEM_ACCESS_STACK_R, TMPL_MEM_TYPE_ALIGN);
    if (rc == VINF_SUCCESS)
    {
        *puValue = *puSrc;
        rc = iemMemCommitAndUnmap(pVCpu, (void *)puSrc, IEM_ACCESS_STACK_R);

        /* Commit the new RSP value. */
        if (rc == VINF_SUCCESS)
        {
            Log10(("IEM RD " TMPL_MEM_FMT_DESC " SS|%RGv (%RX64->%RX64): " TMPL_MEM_FMT_TYPE " [ex]\n",
                   GCPtrTop, pTmpRsp->u, NewRsp.u, *puValue));
            *pTmpRsp = NewRsp;
            return VINF_SUCCESS;
        }
    }
    return rc;
}


# ifdef IEM_WITH_SETJMP

/**
 * Safe/fallback stack push function that longjmps on error.
 */
void RT_CONCAT3(iemMemStackPush,TMPL_MEM_FN_SUFF,SafeJmp)(PVMCPUCC pVCpu, TMPL_MEM_TYPE uValue) IEM_NOEXCEPT_MAY_LONGJMP
{
# if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3)
    pVCpu->iem.s.DataTlb.cTlbSafeWritePath++;
# endif

    /* Decrement the stack pointer (prep). */
    uint64_t      uNewRsp;
    RTGCPTR const GCPtrTop = iemRegGetRspForPush(pVCpu, sizeof(TMPL_MEM_TYPE), &uNewRsp);

    /* Write the data. */
    TMPL_MEM_TYPE *puDst = (TMPL_MEM_TYPE *)iemMemMapJmp(pVCpu, sizeof(TMPL_MEM_TYPE), X86_SREG_SS, GCPtrTop,
                                                         IEM_ACCESS_STACK_W, TMPL_MEM_TYPE_ALIGN);
    *puDst = uValue;
    iemMemCommitAndUnmapJmp(pVCpu, puDst, IEM_ACCESS_STACK_W);

    /* Commit the RSP change. */
    Log12(("IEM WR " TMPL_MEM_FMT_DESC " SS|%RGv (%RX64->%RX64): " TMPL_MEM_FMT_TYPE "\n",
           GCPtrTop, pVCpu->cpum.GstCtx.rsp, uNewRsp, uValue));
    pVCpu->cpum.GstCtx.rsp = uNewRsp;
}


/**
 * Safe/fallback stack pop function that longjmps on error.
 */
TMPL_MEM_TYPE RT_CONCAT3(iemMemStackPop,TMPL_MEM_FN_SUFF,SafeJmp)(PVMCPUCC pVCpu) IEM_NOEXCEPT_MAY_LONGJMP
{
# if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3)
    pVCpu->iem.s.DataTlb.cTlbSafeReadPath++;
# endif

    /* Increment the stack pointer. */
    uint64_t      uNewRsp;
    RTGCPTR const GCPtrTop = iemRegGetRspForPop(pVCpu, sizeof(TMPL_MEM_TYPE), &uNewRsp);

    /* Read the data. */
    TMPL_MEM_TYPE const *puSrc = (TMPL_MEM_TYPE const *)iemMemMapJmp(pVCpu, sizeof(TMPL_MEM_TYPE), X86_SREG_SS, GCPtrTop,
                                                                     IEM_ACCESS_STACK_R, TMPL_MEM_TYPE_ALIGN);
    TMPL_MEM_TYPE const  uRet = *puSrc;
    iemMemCommitAndUnmapJmp(pVCpu, (void *)puSrc, IEM_ACCESS_STACK_R);

    /* Commit the RSP change and return the popped value. */
    Log10(("IEM RD " TMPL_MEM_FMT_DESC " SS|%RGv (%RX64->%RX64): " TMPL_MEM_FMT_TYPE "\n",
           GCPtrTop, pVCpu->cpum.GstCtx.rsp, uNewRsp, uRet));
    pVCpu->cpum.GstCtx.rsp = uNewRsp;

    return uRet;
}

#  ifdef TMPL_WITH_PUSH_SREG
/**
 * Safe/fallback stack push function that longjmps on error.
 */
void RT_CONCAT3(iemMemStackPush,TMPL_MEM_FN_SUFF,SRegSafeJmp)(PVMCPUCC pVCpu, TMPL_MEM_TYPE uValue) IEM_NOEXCEPT_MAY_LONGJMP
{
# if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3)
    pVCpu->iem.s.DataTlb.cTlbSafeWritePath++;
# endif

    /* Decrement the stack pointer (prep). */
    uint64_t      uNewRsp;
    RTGCPTR const GCPtrTop = iemRegGetRspForPush(pVCpu, sizeof(TMPL_MEM_TYPE), &uNewRsp);

    /* Write the data. */
    /* The intel docs talks about zero extending the selector register
       value.  My actual intel CPU here might be zero extending the value
       but it still only writes the lower word... */
    /** @todo Test this on new HW and on AMD and in 64-bit mode.  Also test what
     * happens when crossing an electric page boundrary, is the high word checked
     * for write accessibility or not? Probably it is.  What about segment limits?
     * It appears this behavior is also shared with trap error codes.
     *
     * Docs indicate the behavior changed maybe in Pentium or Pentium Pro. Check
     * ancient hardware when it actually did change. */
    uint16_t *puDst = (uint16_t *)iemMemMapJmp(pVCpu, sizeof(uint16_t), X86_SREG_SS, GCPtrTop,
                                               IEM_ACCESS_STACK_W, sizeof(uint16_t) - 1); /** @todo 2 or 4 alignment check for PUSH SS? */
    *puDst = (uint16_t)uValue;
    iemMemCommitAndUnmapJmp(pVCpu, puDst, IEM_ACCESS_STACK_W);

    /* Commit the RSP change. */
    Log12(("IEM WR " TMPL_MEM_FMT_DESC " SS|%RGv (%RX64->%RX64): " TMPL_MEM_FMT_TYPE " [sreg]\n",
           GCPtrTop, pVCpu->cpum.GstCtx.rsp, uNewRsp, uValue));
    pVCpu->cpum.GstCtx.rsp = uNewRsp;
}
#  endif /* TMPL_WITH_PUSH_SREG */

# endif /* IEM_WITH_SETJMP */

#endif /* TMPL_MEM_WITH_STACK */

/* clean up */
#undef TMPL_MEM_TYPE
#undef TMPL_MEM_TYPE_ALIGN
#undef TMPL_MEM_FN_SUFF
#undef TMPL_MEM_FMT_TYPE
#undef TMPL_MEM_FMT_DESC
#undef TMPL_MEM_BY_REF
#undef TMPL_WITH_PUSH_SREG

