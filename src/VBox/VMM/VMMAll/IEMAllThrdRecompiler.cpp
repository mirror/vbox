/* $Id$ */
/** @file
 * IEM - Instruction Decoding and Threaded Recompilation.
 *
 * Logging group IEM_RE_THREADED assignments:
 *      - Level 1  (Log)  : Errors, exceptions, interrupts and such major events. [same as IEM]
 *      - Flow  (LogFlow) :
 *      - Level 2  (Log2) : Basic instruction execution state info. [same as IEM]
 *      - Level 3  (Log3) : More detailed execution state info. [same as IEM]
 *      - Level 4  (Log4) : Decoding mnemonics w/ EIP. [same as IEM]
 *      - Level 5  (Log5) : Decoding details. [same as IEM]
 *      - Level 6  (Log6) :
 *      - Level 7  (Log7) : TB obsoletion.
 *      - Level 8  (Log8) : TB compilation.
 *      - Level 9  (Log9) : TB exec.
 *      - Level 10 (Log10): TB block lookup.
 *      - Level 11 (Log11): TB block lookup details.
 *      - Level 12 (Log12): TB insertion.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#ifndef LOG_GROUP /* defined when included by tstIEMCheckMc.cpp */
# define LOG_GROUP LOG_GROUP_IEM_RE_THREADED
#endif
#define IEM_WITH_CODE_TLB_AND_OPCODE_BUF  /* A bit hackish, but its all in IEMInline.h. */
#define VMCPU_INCL_CPUM_GST_CTX
#include <VBox/vmm/iem.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/apic.h>
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/nem.h>
#include <VBox/vmm/gim.h>
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
# include <VBox/vmm/em.h>
# include <VBox/vmm/hm_svm.h>
#endif
#ifdef VBOX_WITH_NESTED_HWVIRT_VMX
# include <VBox/vmm/hmvmxinline.h>
#endif
#include <VBox/vmm/tm.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/dbgftrace.h>
#ifndef TST_IEM_CHECK_MC
# include "IEMInternal.h"
#endif
#include <VBox/vmm/vmcc.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/dis.h>
#include <VBox/disopcode-x86-amd64.h>
#include <iprt/asm-math.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/x86.h>

#ifndef TST_IEM_CHECK_MC
# include "IEMInline.h"
# include "IEMOpHlp.h"
# include "IEMMc.h"
#endif

#include "IEMThreadedFunctions.h"


/*
 * Narrow down configs here to avoid wasting time on unused configs here.
 */

#ifndef IEM_WITH_CODE_TLB
# error The code TLB must be enabled for the recompiler.
#endif

#ifndef IEM_WITH_DATA_TLB
# error The data TLB must be enabled for the recompiler.
#endif

#ifndef IEM_WITH_SETJMP
# error The setjmp approach must be enabled for the recompiler.
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static VBOXSTRICTRC iemThreadedTbExec(PVMCPUCC pVCpu, PIEMTB pTb);


/**
 * Calculates the effective address of a ModR/M memory operand, extended version
 * for use in the recompilers.
 *
 * Meant to be used via IEM_MC_CALC_RM_EFF_ADDR.
 *
 * May longjmp on internal error.
 *
 * @return  The effective address.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   bRm                 The ModRM byte.
 * @param   cbImmAndRspOffset   - First byte: The size of any immediate
 *                                following the effective address opcode bytes
 *                                (only for RIP relative addressing).
 *                              - Second byte: RSP displacement (for POP [ESP]).
 * @param   puInfo              Extra info: 32-bit displacement (bits 31:0) and
 *                              SIB byte (bits 39:32).
 *
 * @note This must be defined in a source file with matching
 *       IEM_WITH_CODE_TLB_AND_OPCODE_BUF define till the define is made default
 *       or implemented differently...
 */
RTGCPTR iemOpHlpCalcRmEffAddrJmpEx(PVMCPUCC pVCpu, uint8_t bRm, uint32_t cbImmAndRspOffset, uint64_t *puInfo) IEM_NOEXCEPT_MAY_LONGJMP
{
    Log5(("iemOpHlpCalcRmEffAddrJmp: bRm=%#x\n", bRm));
# define SET_SS_DEF() \
    do \
    { \
        if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SEG_MASK)) \
            pVCpu->iem.s.iEffSeg = X86_SREG_SS; \
    } while (0)

    if (!IEM_IS_64BIT_CODE(pVCpu))
    {
/** @todo Check the effective address size crap! */
        if (pVCpu->iem.s.enmEffAddrMode == IEMMODE_16BIT)
        {
            uint16_t u16EffAddr;

            /* Handle the disp16 form with no registers first. */
            if ((bRm & (X86_MODRM_MOD_MASK | X86_MODRM_RM_MASK)) == 6)
            {
                IEM_OPCODE_GET_NEXT_U16(&u16EffAddr);
                *puInfo = u16EffAddr;
            }
            else
            {
                /* Get the displacment. */
                switch ((bRm >> X86_MODRM_MOD_SHIFT) & X86_MODRM_MOD_SMASK)
                {
                    case 0:  u16EffAddr = 0;                             break;
                    case 1:  IEM_OPCODE_GET_NEXT_S8_SX_U16(&u16EffAddr); break;
                    case 2:  IEM_OPCODE_GET_NEXT_U16(&u16EffAddr);       break;
                    default: AssertFailedStmt(IEM_DO_LONGJMP(pVCpu, VERR_IEM_IPE_1)); /* (caller checked for these) */
                }
                *puInfo = u16EffAddr;

                /* Add the base and index registers to the disp. */
                switch (bRm & X86_MODRM_RM_MASK)
                {
                    case 0: u16EffAddr += pVCpu->cpum.GstCtx.bx + pVCpu->cpum.GstCtx.si; break;
                    case 1: u16EffAddr += pVCpu->cpum.GstCtx.bx + pVCpu->cpum.GstCtx.di; break;
                    case 2: u16EffAddr += pVCpu->cpum.GstCtx.bp + pVCpu->cpum.GstCtx.si; SET_SS_DEF(); break;
                    case 3: u16EffAddr += pVCpu->cpum.GstCtx.bp + pVCpu->cpum.GstCtx.di; SET_SS_DEF(); break;
                    case 4: u16EffAddr += pVCpu->cpum.GstCtx.si;            break;
                    case 5: u16EffAddr += pVCpu->cpum.GstCtx.di;            break;
                    case 6: u16EffAddr += pVCpu->cpum.GstCtx.bp;            SET_SS_DEF(); break;
                    case 7: u16EffAddr += pVCpu->cpum.GstCtx.bx;            break;
                }
            }

            Log5(("iemOpHlpCalcRmEffAddrJmp: EffAddr=%#06RX16 uInfo=%#RX64\n", u16EffAddr, *puInfo));
            return u16EffAddr;
        }

        Assert(pVCpu->iem.s.enmEffAddrMode == IEMMODE_32BIT);
        uint32_t u32EffAddr;
        uint64_t uInfo;

        /* Handle the disp32 form with no registers first. */
        if ((bRm & (X86_MODRM_MOD_MASK | X86_MODRM_RM_MASK)) == 5)
        {
            IEM_OPCODE_GET_NEXT_U32(&u32EffAddr);
            uInfo = u32EffAddr;
        }
        else
        {
            /* Get the register (or SIB) value. */
            uInfo = 0;
            switch ((bRm & X86_MODRM_RM_MASK))
            {
                case 0: u32EffAddr = pVCpu->cpum.GstCtx.eax; break;
                case 1: u32EffAddr = pVCpu->cpum.GstCtx.ecx; break;
                case 2: u32EffAddr = pVCpu->cpum.GstCtx.edx; break;
                case 3: u32EffAddr = pVCpu->cpum.GstCtx.ebx; break;
                case 4: /* SIB */
                {
                    uint8_t bSib; IEM_OPCODE_GET_NEXT_U8(&bSib);
                    uInfo = (uint64_t)bSib << 32;

                    /* Get the index and scale it. */
                    switch ((bSib >> X86_SIB_INDEX_SHIFT) & X86_SIB_INDEX_SMASK)
                    {
                        case 0: u32EffAddr = pVCpu->cpum.GstCtx.eax; break;
                        case 1: u32EffAddr = pVCpu->cpum.GstCtx.ecx; break;
                        case 2: u32EffAddr = pVCpu->cpum.GstCtx.edx; break;
                        case 3: u32EffAddr = pVCpu->cpum.GstCtx.ebx; break;
                        case 4: u32EffAddr = 0; /*none */ break;
                        case 5: u32EffAddr = pVCpu->cpum.GstCtx.ebp; break;
                        case 6: u32EffAddr = pVCpu->cpum.GstCtx.esi; break;
                        case 7: u32EffAddr = pVCpu->cpum.GstCtx.edi; break;
                        IEM_NOT_REACHED_DEFAULT_CASE_RET2(RTGCPTR_MAX);
                    }
                    u32EffAddr <<= (bSib >> X86_SIB_SCALE_SHIFT) & X86_SIB_SCALE_SMASK;

                    /* add base */
                    switch (bSib & X86_SIB_BASE_MASK)
                    {
                        case 0: u32EffAddr += pVCpu->cpum.GstCtx.eax; break;
                        case 1: u32EffAddr += pVCpu->cpum.GstCtx.ecx; break;
                        case 2: u32EffAddr += pVCpu->cpum.GstCtx.edx; break;
                        case 3: u32EffAddr += pVCpu->cpum.GstCtx.ebx; break;
                        case 4: u32EffAddr += pVCpu->cpum.GstCtx.esp + (cbImmAndRspOffset >> 8); SET_SS_DEF(); break;
                        case 5:
                            if ((bRm & X86_MODRM_MOD_MASK) != 0)
                            {
                                u32EffAddr += pVCpu->cpum.GstCtx.ebp;
                                SET_SS_DEF();
                            }
                            else
                            {
                                uint32_t u32Disp;
                                IEM_OPCODE_GET_NEXT_U32(&u32Disp);
                                u32EffAddr += u32Disp;
                                uInfo      |= u32Disp;
                            }
                            break;
                        case 6: u32EffAddr += pVCpu->cpum.GstCtx.esi; break;
                        case 7: u32EffAddr += pVCpu->cpum.GstCtx.edi; break;
                        IEM_NOT_REACHED_DEFAULT_CASE_RET2(RTGCPTR_MAX);
                    }
                    break;
                }
                case 5: u32EffAddr = pVCpu->cpum.GstCtx.ebp; SET_SS_DEF(); break;
                case 6: u32EffAddr = pVCpu->cpum.GstCtx.esi; break;
                case 7: u32EffAddr = pVCpu->cpum.GstCtx.edi; break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET2(RTGCPTR_MAX);
            }

            /* Get and add the displacement. */
            switch ((bRm >> X86_MODRM_MOD_SHIFT) & X86_MODRM_MOD_SMASK)
            {
                case 0:
                    break;
                case 1:
                {
                    int8_t i8Disp; IEM_OPCODE_GET_NEXT_S8(&i8Disp);
                    u32EffAddr += i8Disp;
                    uInfo      |= (uint32_t)(int32_t)i8Disp;
                    break;
                }
                case 2:
                {
                    uint32_t u32Disp; IEM_OPCODE_GET_NEXT_U32(&u32Disp);
                    u32EffAddr += u32Disp;
                    uInfo      |= u32Disp;
                    break;
                }
                default:
                    AssertFailedStmt(IEM_DO_LONGJMP(pVCpu, VERR_IEM_IPE_2)); /* (caller checked for these) */
            }
        }

        *puInfo = uInfo;
        Log5(("iemOpHlpCalcRmEffAddrJmp: EffAddr=%#010RX32 uInfo=%#RX64\n", u32EffAddr, uInfo));
        return u32EffAddr;
    }

    uint64_t u64EffAddr;
    uint64_t uInfo;

    /* Handle the rip+disp32 form with no registers first. */
    if ((bRm & (X86_MODRM_MOD_MASK | X86_MODRM_RM_MASK)) == 5)
    {
        IEM_OPCODE_GET_NEXT_S32_SX_U64(&u64EffAddr);
        uInfo = (uint32_t)u64EffAddr;
        u64EffAddr += pVCpu->cpum.GstCtx.rip + IEM_GET_INSTR_LEN(pVCpu) + (cbImmAndRspOffset & UINT32_C(0xff));
    }
    else
    {
        /* Get the register (or SIB) value. */
        uInfo = 0;
        switch ((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB)
        {
            case  0: u64EffAddr = pVCpu->cpum.GstCtx.rax; break;
            case  1: u64EffAddr = pVCpu->cpum.GstCtx.rcx; break;
            case  2: u64EffAddr = pVCpu->cpum.GstCtx.rdx; break;
            case  3: u64EffAddr = pVCpu->cpum.GstCtx.rbx; break;
            case  5: u64EffAddr = pVCpu->cpum.GstCtx.rbp; SET_SS_DEF(); break;
            case  6: u64EffAddr = pVCpu->cpum.GstCtx.rsi; break;
            case  7: u64EffAddr = pVCpu->cpum.GstCtx.rdi; break;
            case  8: u64EffAddr = pVCpu->cpum.GstCtx.r8;  break;
            case  9: u64EffAddr = pVCpu->cpum.GstCtx.r9;  break;
            case 10: u64EffAddr = pVCpu->cpum.GstCtx.r10; break;
            case 11: u64EffAddr = pVCpu->cpum.GstCtx.r11; break;
            case 13: u64EffAddr = pVCpu->cpum.GstCtx.r13; break;
            case 14: u64EffAddr = pVCpu->cpum.GstCtx.r14; break;
            case 15: u64EffAddr = pVCpu->cpum.GstCtx.r15; break;
            /* SIB */
            case 4:
            case 12:
            {
                uint8_t bSib; IEM_OPCODE_GET_NEXT_U8(&bSib);
                uInfo = (uint64_t)bSib << 32;

                /* Get the index and scale it. */
                switch (((bSib >> X86_SIB_INDEX_SHIFT) & X86_SIB_INDEX_SMASK) | pVCpu->iem.s.uRexIndex)
                {
                    case  0: u64EffAddr = pVCpu->cpum.GstCtx.rax; break;
                    case  1: u64EffAddr = pVCpu->cpum.GstCtx.rcx; break;
                    case  2: u64EffAddr = pVCpu->cpum.GstCtx.rdx; break;
                    case  3: u64EffAddr = pVCpu->cpum.GstCtx.rbx; break;
                    case  4: u64EffAddr = 0; /*none */ break;
                    case  5: u64EffAddr = pVCpu->cpum.GstCtx.rbp; break;
                    case  6: u64EffAddr = pVCpu->cpum.GstCtx.rsi; break;
                    case  7: u64EffAddr = pVCpu->cpum.GstCtx.rdi; break;
                    case  8: u64EffAddr = pVCpu->cpum.GstCtx.r8;  break;
                    case  9: u64EffAddr = pVCpu->cpum.GstCtx.r9;  break;
                    case 10: u64EffAddr = pVCpu->cpum.GstCtx.r10; break;
                    case 11: u64EffAddr = pVCpu->cpum.GstCtx.r11; break;
                    case 12: u64EffAddr = pVCpu->cpum.GstCtx.r12; break;
                    case 13: u64EffAddr = pVCpu->cpum.GstCtx.r13; break;
                    case 14: u64EffAddr = pVCpu->cpum.GstCtx.r14; break;
                    case 15: u64EffAddr = pVCpu->cpum.GstCtx.r15; break;
                    IEM_NOT_REACHED_DEFAULT_CASE_RET2(RTGCPTR_MAX);
                }
                u64EffAddr <<= (bSib >> X86_SIB_SCALE_SHIFT) & X86_SIB_SCALE_SMASK;

                /* add base */
                switch ((bSib & X86_SIB_BASE_MASK) | pVCpu->iem.s.uRexB)
                {
                    case  0: u64EffAddr += pVCpu->cpum.GstCtx.rax; break;
                    case  1: u64EffAddr += pVCpu->cpum.GstCtx.rcx; break;
                    case  2: u64EffAddr += pVCpu->cpum.GstCtx.rdx; break;
                    case  3: u64EffAddr += pVCpu->cpum.GstCtx.rbx; break;
                    case  4: u64EffAddr += pVCpu->cpum.GstCtx.rsp + (cbImmAndRspOffset >> 8); SET_SS_DEF(); break;
                    case  6: u64EffAddr += pVCpu->cpum.GstCtx.rsi; break;
                    case  7: u64EffAddr += pVCpu->cpum.GstCtx.rdi; break;
                    case  8: u64EffAddr += pVCpu->cpum.GstCtx.r8;  break;
                    case  9: u64EffAddr += pVCpu->cpum.GstCtx.r9;  break;
                    case 10: u64EffAddr += pVCpu->cpum.GstCtx.r10; break;
                    case 11: u64EffAddr += pVCpu->cpum.GstCtx.r11; break;
                    case 12: u64EffAddr += pVCpu->cpum.GstCtx.r12; break;
                    case 14: u64EffAddr += pVCpu->cpum.GstCtx.r14; break;
                    case 15: u64EffAddr += pVCpu->cpum.GstCtx.r15; break;
                    /* complicated encodings */
                    case 5:
                    case 13:
                        if ((bRm & X86_MODRM_MOD_MASK) != 0)
                        {
                            if (!pVCpu->iem.s.uRexB)
                            {
                                u64EffAddr += pVCpu->cpum.GstCtx.rbp;
                                SET_SS_DEF();
                            }
                            else
                                u64EffAddr += pVCpu->cpum.GstCtx.r13;
                        }
                        else
                        {
                            uint32_t u32Disp;
                            IEM_OPCODE_GET_NEXT_U32(&u32Disp);
                            u64EffAddr += (int32_t)u32Disp;
                            uInfo      |= u32Disp;
                        }
                        break;
                    IEM_NOT_REACHED_DEFAULT_CASE_RET2(RTGCPTR_MAX);
                }
                break;
            }
            IEM_NOT_REACHED_DEFAULT_CASE_RET2(RTGCPTR_MAX);
        }

        /* Get and add the displacement. */
        switch ((bRm >> X86_MODRM_MOD_SHIFT) & X86_MODRM_MOD_SMASK)
        {
            case 0:
                break;
            case 1:
            {
                int8_t i8Disp;
                IEM_OPCODE_GET_NEXT_S8(&i8Disp);
                u64EffAddr += i8Disp;
                uInfo      |= (uint32_t)(int32_t)i8Disp;
                break;
            }
            case 2:
            {
                uint32_t u32Disp;
                IEM_OPCODE_GET_NEXT_U32(&u32Disp);
                u64EffAddr += (int32_t)u32Disp;
                uInfo      |= u32Disp;
                break;
            }
            IEM_NOT_REACHED_DEFAULT_CASE_RET2(RTGCPTR_MAX); /* (caller checked for these) */
        }

    }

    *puInfo = uInfo;
    if (pVCpu->iem.s.enmEffAddrMode == IEMMODE_64BIT)
    {
        Log5(("iemOpHlpCalcRmEffAddrJmp: EffAddr=%#010RGv uInfo=%#RX64\n", u64EffAddr, uInfo));
        return u64EffAddr;
    }
    Assert(pVCpu->iem.s.enmEffAddrMode == IEMMODE_32BIT);
    Log5(("iemOpHlpCalcRmEffAddrJmp: EffAddr=%#010RGv uInfo=%#RX64\n", u64EffAddr & UINT32_MAX, uInfo));
    return u64EffAddr & UINT32_MAX;
}


/*
 * Translation block management.
 */

typedef struct IEMTBCACHE
{
    uint32_t cHash;
    uint32_t uHashMask;
    PIEMTB   apHash[_1M];
} IEMTBCACHE;

static IEMTBCACHE g_TbCache = { _1M, _1M - 1, }; /**< Quick and dirty. */

#define IEMTBCACHE_HASH(a_paCache, a_fTbFlags, a_GCPhysPc) \
    ( ((uint32_t)(a_GCPhysPc) ^ (a_fTbFlags)) & (a_paCache)->uHashMask)


/**
 * Allocate a translation block for threadeded recompilation.
 *
 * @returns Pointer to the translation block on success, NULL on failure.
 * @param   pVM         The cross context virtual machine structure.
 * @param   pVCpu       The cross context virtual CPU structure of the calling
 *                      thread.
 * @param   GCPhysPc    The physical address corresponding to RIP + CS.BASE.
 * @param   fExtraFlags Extra flags (IEMTB_F_XXX).
 */
static PIEMTB iemThreadedTbAlloc(PVMCC pVM, PVMCPUCC pVCpu, RTGCPHYS GCPhysPc, uint32_t fExtraFlags)
{
    /*
     * Just using the heap for now.  Will make this more efficient and
     * complicated later, don't worry. :-)
     */
    PIEMTB pTb = (PIEMTB)RTMemAlloc(sizeof(IEMTB));
    if (pTb)
    {
        unsigned const cCalls = 128;
        pTb->Thrd.paCalls = (PIEMTHRDEDCALLENTRY)RTMemAlloc(sizeof(IEMTHRDEDCALLENTRY) * cCalls);
        if (pTb->Thrd.paCalls)
        {
            pTb->pabOpcodes = (uint8_t *)RTMemAlloc(cCalls * 16); /* This will be reallocated later. */
            if (pTb->pabOpcodes)
            {
                pTb->Thrd.cAllocated        = cCalls;
                pTb->cbOpcodesAllocated     = cCalls * 16;
                pTb->Thrd.cCalls            = 0;
                pTb->cbOpcodes              = 0;
                pTb->pNext                  = NULL;
                RTListInit(&pTb->LocalList);
                pTb->GCPhysPc               = GCPhysPc;
                pTb->x86.fAttr              = (uint16_t)pVCpu->cpum.GstCtx.cs.Attr.u;
                pTb->fFlags                 = (pVCpu->iem.s.fExec & IEMTB_F_IEM_F_MASK) | fExtraFlags;
                pTb->cInstructions          = 0;

                /* Init the first opcode range. */
                pTb->cRanges                = 1;
                pTb->aRanges[0].cbOpcodes   = 0;
                pTb->aRanges[0].offOpcodes  = 0;
                pTb->aRanges[0].offPhysPage = GCPhysPc & GUEST_PAGE_OFFSET_MASK;
                pTb->aRanges[0].u2Unused    = 0;
                pTb->aRanges[0].idxPhysPage = 0;
                pTb->aGCPhysPages[0]        = NIL_RTGCPHYS;
                pTb->aGCPhysPages[1]        = NIL_RTGCPHYS;

                pVCpu->iem.s.cTbAllocs++;
                return pTb;
            }
            RTMemFree(pTb->Thrd.paCalls);
        }
        RTMemFree(pTb);
    }
    RT_NOREF(pVM);
    return NULL;
}


/**
 * Frees pTb.
 *
 * @param   pVM     The cross context virtual machine structure.
 * @param   pVCpu   The cross context virtual CPU structure of the calling
 *                  thread.
 * @param   pTb     The translation block to free..
 */
static void iemThreadedTbFree(PVMCC pVM, PVMCPUCC pVCpu, PIEMTB pTb)
{
    RT_NOREF(pVM);
    AssertPtr(pTb);

    AssertCompile(IEMTB_F_STATE_OBSOLETE == IEMTB_F_STATE_MASK);
    pTb->fFlags |= IEMTB_F_STATE_OBSOLETE; /* works, both bits set */

    /* Unlink it from the hash table: */
    uint32_t const idxHash = IEMTBCACHE_HASH(&g_TbCache, pTb->fFlags, pTb->GCPhysPc);
    PIEMTB pTbCur = g_TbCache.apHash[idxHash];
    if (pTbCur == pTb)
        g_TbCache.apHash[idxHash] = pTb->pNext;
    else
        while (pTbCur)
        {
            PIEMTB const pNextTb = pTbCur->pNext;
            if (pNextTb == pTb)
            {
                pTbCur->pNext = pTb->pNext;
                break;
            }
            pTbCur = pNextTb;
        }

    /* Free it. */
    RTMemFree(pTb->Thrd.paCalls);
    pTb->Thrd.paCalls = NULL;

    RTMemFree(pTb->pabOpcodes);
    pTb->pabOpcodes = NULL;

    RTMemFree(pTb);
    pVCpu->iem.s.cTbFrees++;
}


/**
 * Called by opcode verifier functions when they detect a problem.
 */
void iemThreadedTbObsolete(PVMCPUCC pVCpu, PIEMTB pTb)
{
    iemThreadedTbFree(pVCpu->CTX_SUFF(pVM), pVCpu, pTb);
}


static PIEMTB iemThreadedTbLookup(PVMCC pVM, PVMCPUCC pVCpu, RTGCPHYS GCPhysPc, uint32_t fExtraFlags) IEM_NOEXCEPT_MAY_LONGJMP
{
    uint32_t const fFlags  = (pVCpu->iem.s.fExec & IEMTB_F_IEM_F_MASK) | fExtraFlags | IEMTB_F_STATE_READY;
    uint32_t const idxHash = IEMTBCACHE_HASH(&g_TbCache, fFlags, GCPhysPc);
    Log10(("TB lookup: idxHash=%#x fFlags=%#x GCPhysPc=%RGp\n", idxHash, fFlags, GCPhysPc));
    PIEMTB pTb = g_TbCache.apHash[idxHash];
    while (pTb)
    {
        if (pTb->GCPhysPc == GCPhysPc)
        {
            if (pTb->fFlags == fFlags)
            {
                if (pTb->x86.fAttr == (uint16_t)pVCpu->cpum.GstCtx.cs.Attr.u)
                {
#ifdef VBOX_WITH_STATISTICS
                    pVCpu->iem.s.cTbLookupHits++;
#endif
                    return pTb;
                }
                Log11(("TB miss: CS: %#x, wanted %#x\n", pTb->x86.fAttr, (uint16_t)pVCpu->cpum.GstCtx.cs.Attr.u));
            }
            else
                Log11(("TB miss: fFlags: %#x, wanted %#x\n", pTb->fFlags, fFlags));
        }
        else
            Log11(("TB miss: GCPhysPc: %#x, wanted %#x\n", pTb->GCPhysPc, GCPhysPc));

        pTb = pTb->pNext;
    }
    RT_NOREF(pVM);
    pVCpu->iem.s.cTbLookupMisses++;
    return pTb;
}


static void iemThreadedTbAdd(PVMCC pVM, PVMCPUCC pVCpu, PIEMTB pTb)
{
    uint32_t const idxHash = IEMTBCACHE_HASH(&g_TbCache, pTb->fFlags, pTb->GCPhysPc);
    pTb->pNext = g_TbCache.apHash[idxHash];
    g_TbCache.apHash[idxHash] = pTb;
    STAM_REL_PROFILE_ADD_PERIOD(&pVCpu->iem.s.StatTbThreadedInstr, pTb->cInstructions);
    STAM_REL_PROFILE_ADD_PERIOD(&pVCpu->iem.s.StatTbThreadedCalls, pTb->Thrd.cCalls);
    if (LogIs12Enabled())
    {
        Log12(("TB added: %p %RGp LB %#x fl=%#x idxHash=%#x cRanges=%u cInstr=%u cCalls=%u\n",
               pTb, pTb->GCPhysPc, pTb->cbOpcodes, pTb->fFlags, idxHash, pTb->cRanges, pTb->cInstructions, pTb->Thrd.cCalls));
        for (uint8_t idxRange = 0; idxRange < pTb->cRanges; idxRange++)
            Log12((" range#%u: offPg=%#05x offOp=%#04x LB %#04x pg#%u=%RGp\n", idxRange, pTb->aRanges[idxRange].offPhysPage,
                   pTb->aRanges[idxRange].offOpcodes, pTb->aRanges[idxRange].cbOpcodes, pTb->aRanges[idxRange].idxPhysPage,
                   pTb->aRanges[idxRange].idxPhysPage == 0
                   ? pTb->GCPhysPc & ~(RTGCPHYS)GUEST_PAGE_OFFSET_MASK
                   : pTb->aGCPhysPages[pTb->aRanges[idxRange].idxPhysPage - 1]));
    }
    RT_NOREF(pVM);
}


/*
 * Real code.
 */

#ifdef LOG_ENABLED
/**
 * Logs the current instruction.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pszFunction The IEM function doing the execution.
 */
static void iemThreadedLogCurInstr(PVMCPUCC pVCpu, const char *pszFunction) RT_NOEXCEPT
{
# ifdef IN_RING3
    if (LogIs2Enabled())
    {
        char     szInstr[256];
        uint32_t cbInstr = 0;
        DBGFR3DisasInstrEx(pVCpu->pVMR3->pUVM, pVCpu->idCpu, 0, 0,
                           DBGF_DISAS_FLAGS_CURRENT_GUEST | DBGF_DISAS_FLAGS_DEFAULT_MODE,
                           szInstr, sizeof(szInstr), &cbInstr);

        PCX86FXSTATE pFpuCtx = &pVCpu->cpum.GstCtx.XState.x87;
        Log2(("**** %s fExec=%x pTb=%p\n"
              " eax=%08x ebx=%08x ecx=%08x edx=%08x esi=%08x edi=%08x\n"
              " eip=%08x esp=%08x ebp=%08x iopl=%d tr=%04x\n"
              " cs=%04x ss=%04x ds=%04x es=%04x fs=%04x gs=%04x efl=%08x\n"
              " fsw=%04x fcw=%04x ftw=%02x mxcsr=%04x/%04x\n"
              " %s\n"
              , pszFunction, pVCpu->iem.s.fExec, pVCpu->iem.s.pCurTbR3,
              pVCpu->cpum.GstCtx.eax, pVCpu->cpum.GstCtx.ebx, pVCpu->cpum.GstCtx.ecx, pVCpu->cpum.GstCtx.edx, pVCpu->cpum.GstCtx.esi, pVCpu->cpum.GstCtx.edi,
              pVCpu->cpum.GstCtx.eip, pVCpu->cpum.GstCtx.esp, pVCpu->cpum.GstCtx.ebp, pVCpu->cpum.GstCtx.eflags.Bits.u2IOPL, pVCpu->cpum.GstCtx.tr.Sel,
              pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.ss.Sel, pVCpu->cpum.GstCtx.ds.Sel, pVCpu->cpum.GstCtx.es.Sel,
              pVCpu->cpum.GstCtx.fs.Sel, pVCpu->cpum.GstCtx.gs.Sel, pVCpu->cpum.GstCtx.eflags.u,
              pFpuCtx->FSW, pFpuCtx->FCW, pFpuCtx->FTW, pFpuCtx->MXCSR, pFpuCtx->MXCSR_MASK,
              szInstr));

        if (LogIs3Enabled())
            DBGFR3InfoEx(pVCpu->pVMR3->pUVM, pVCpu->idCpu, "cpumguest", "verbose", NULL);
    }
    else
# endif
        LogFlow(("%s: cs:rip=%04x:%08RX64 ss:rsp=%04x:%08RX64 EFL=%06x\n", pszFunction, pVCpu->cpum.GstCtx.cs.Sel,
                 pVCpu->cpum.GstCtx.rip, pVCpu->cpum.GstCtx.ss.Sel, pVCpu->cpum.GstCtx.rsp, pVCpu->cpum.GstCtx.eflags.u));
}
#endif /* LOG_ENABLED */


static VBOXSTRICTRC iemThreadedCompileLongJumped(PVMCC pVM, PVMCPUCC pVCpu, VBOXSTRICTRC rcStrict)
{
    RT_NOREF(pVM, pVCpu);
    return rcStrict;
}


/**
 * Initializes the decoder state when compiling TBs.
 *
 * This presumes that fExec has already be initialized.
 *
 * This is very similar to iemInitDecoder() and iemReInitDecoder(), so may need
 * to apply fixes to them as well.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the calling
 *                      thread.
 * @param   fReInit     Clear for the first call for a TB, set for subsequent
 *                      calls from inside the compile loop where we can skip a
 *                      couple of things.
 * @param   fExtraFlags The extra translation block flags when @a fReInit is
 *                      true, otherwise ignored.  Only IEMTB_F_INHIBIT_SHADOW is
 *                      checked.
 */
DECL_FORCE_INLINE(void) iemThreadedCompileInitDecoder(PVMCPUCC pVCpu, bool const fReInit, uint32_t const fExtraFlags)
{
    /* ASSUMES: That iemInitExec was already called and that anyone changing
       CPU state affecting the fExec bits since then will have updated fExec!  */
    AssertMsg((pVCpu->iem.s.fExec & ~IEM_F_USER_OPTS) == iemCalcExecFlags(pVCpu),
              ("fExec=%#x iemCalcExecModeFlags=%#x\n", pVCpu->iem.s.fExec, iemCalcExecFlags(pVCpu)));

    IEMMODE const enmMode = IEM_GET_CPU_MODE(pVCpu);

    /* Decoder state: */
    pVCpu->iem.s.enmDefAddrMode     = enmMode;  /** @todo check if this is correct... */
    pVCpu->iem.s.enmEffAddrMode     = enmMode;
    if (enmMode != IEMMODE_64BIT)
    {
        pVCpu->iem.s.enmDefOpSize   = enmMode;  /** @todo check if this is correct... */
        pVCpu->iem.s.enmEffOpSize   = enmMode;
    }
    else
    {
        pVCpu->iem.s.enmDefOpSize   = IEMMODE_32BIT;
        pVCpu->iem.s.enmEffOpSize   = IEMMODE_32BIT;
    }
    pVCpu->iem.s.fPrefixes          = 0;
    pVCpu->iem.s.uRexReg            = 0;
    pVCpu->iem.s.uRexB              = 0;
    pVCpu->iem.s.uRexIndex          = 0;
    pVCpu->iem.s.idxPrefix          = 0;
    pVCpu->iem.s.uVex3rdReg         = 0;
    pVCpu->iem.s.uVexLength         = 0;
    pVCpu->iem.s.fEvexStuff         = 0;
    pVCpu->iem.s.iEffSeg            = X86_SREG_DS;
    pVCpu->iem.s.offModRm           = 0;
    pVCpu->iem.s.iNextMapping       = 0;

    if (!fReInit)
    {
        pVCpu->iem.s.cActiveMappings        = 0;
        pVCpu->iem.s.rcPassUp               = VINF_SUCCESS;
        pVCpu->iem.s.fEndTb                 = false;
        pVCpu->iem.s.fTbCheckOpcodes        = false;
        pVCpu->iem.s.fTbBranched            = IEMBRANCHED_F_NO;
        pVCpu->iem.s.fTbCrossedPage         = false;
        pVCpu->iem.s.cInstrTillIrqCheck     = !(fExtraFlags & IEMTB_F_INHIBIT_SHADOW) ? 32 : 0;
        pVCpu->iem.s.fTbCurInstrIsSti       = false;
    }
    else
    {
        Assert(pVCpu->iem.s.cActiveMappings == 0);
        Assert(pVCpu->iem.s.rcPassUp        == VINF_SUCCESS);
        Assert(pVCpu->iem.s.fEndTb          == false);
        Assert(pVCpu->iem.s.fTbCrossedPage  == false);
    }

#ifdef DBGFTRACE_ENABLED
    switch (IEM_GET_CPU_MODE(pVCpu))
    {
        case IEMMODE_64BIT:
            RTTraceBufAddMsgF(pVCpu->CTX_SUFF(pVM)->CTX_SUFF(hTraceBuf), "I64/%u %08llx", IEM_GET_CPL(pVCpu), pVCpu->cpum.GstCtx.rip);
            break;
        case IEMMODE_32BIT:
            RTTraceBufAddMsgF(pVCpu->CTX_SUFF(pVM)->CTX_SUFF(hTraceBuf), "I32/%u %04x:%08x", IEM_GET_CPL(pVCpu), pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.eip);
            break;
        case IEMMODE_16BIT:
            RTTraceBufAddMsgF(pVCpu->CTX_SUFF(pVM)->CTX_SUFF(hTraceBuf), "I16/%u %04x:%04x", IEM_GET_CPL(pVCpu), pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.eip);
            break;
    }
#endif
}


/**
 * Initializes the opcode fetcher when starting the compilation.
 *
 * @param   pVCpu   The cross context virtual CPU structure of the calling
 *                  thread.
 */
DECL_FORCE_INLINE(void) iemThreadedCompileInitOpcodeFetching(PVMCPUCC pVCpu)
{
    /* Almost everything is done by iemGetPcWithPhysAndCode() already.  We just need to initialize the index into abOpcode. */
#ifdef IEM_WITH_CODE_TLB_AND_OPCODE_BUF
    pVCpu->iem.s.offOpcode          = 0;
#else
    RT_NOREF(pVCpu);
#endif
}


/**
 * Re-initializes the opcode fetcher between instructions while compiling.
 *
 * @param   pVCpu   The cross context virtual CPU structure of the calling
 *                  thread.
 */
DECL_FORCE_INLINE(void) iemThreadedCompileReInitOpcodeFetching(PVMCPUCC pVCpu)
{
    if (pVCpu->iem.s.pbInstrBuf)
    {
        uint64_t off = pVCpu->cpum.GstCtx.rip;
        Assert(pVCpu->cpum.GstCtx.cs.u64Base == 0 || !IEM_IS_64BIT_CODE(pVCpu));
        off += pVCpu->cpum.GstCtx.cs.u64Base;
        off -= pVCpu->iem.s.uInstrBufPc;
        if (off < pVCpu->iem.s.cbInstrBufTotal)
        {
            pVCpu->iem.s.offInstrNextByte = (uint32_t)off;
            pVCpu->iem.s.offCurInstrStart = (uint16_t)off;
            if ((uint16_t)off + 15 <= pVCpu->iem.s.cbInstrBufTotal)
                pVCpu->iem.s.cbInstrBuf = (uint16_t)off + 15;
            else
                pVCpu->iem.s.cbInstrBuf = pVCpu->iem.s.cbInstrBufTotal;
        }
        else
        {
            pVCpu->iem.s.pbInstrBuf       = NULL;
            pVCpu->iem.s.offInstrNextByte = 0;
            pVCpu->iem.s.offCurInstrStart = 0;
            pVCpu->iem.s.cbInstrBuf       = 0;
            pVCpu->iem.s.cbInstrBufTotal  = 0;
            pVCpu->iem.s.GCPhysInstrBuf   = NIL_RTGCPHYS;
        }
    }
    else
    {
        pVCpu->iem.s.offInstrNextByte = 0;
        pVCpu->iem.s.offCurInstrStart = 0;
        pVCpu->iem.s.cbInstrBuf       = 0;
        pVCpu->iem.s.cbInstrBufTotal  = 0;
#ifdef VBOX_STRICT
        pVCpu->iem.s.GCPhysInstrBuf   = NIL_RTGCPHYS;
#endif
    }
#ifdef IEM_WITH_CODE_TLB_AND_OPCODE_BUF
    pVCpu->iem.s.offOpcode          = 0;
#endif
}


DECLINLINE(void) iemThreadedCopyOpcodeBytesInline(PCVMCPUCC pVCpu, uint8_t *pbDst, uint8_t cbInstr)
{
    switch (cbInstr)
    {
        default: AssertMsgFailed(("%#x\n", cbInstr)); RT_FALL_THROUGH();
        case 15:    pbDst[14] = pVCpu->iem.s.abOpcode[14]; RT_FALL_THROUGH();
        case 14:    pbDst[13] = pVCpu->iem.s.abOpcode[13]; RT_FALL_THROUGH();
        case 13:    pbDst[12] = pVCpu->iem.s.abOpcode[12]; RT_FALL_THROUGH();
        case 12:    pbDst[11] = pVCpu->iem.s.abOpcode[11]; RT_FALL_THROUGH();
        case 11:    pbDst[10] = pVCpu->iem.s.abOpcode[10]; RT_FALL_THROUGH();
        case 10:    pbDst[9]  = pVCpu->iem.s.abOpcode[9];  RT_FALL_THROUGH();
        case 9:     pbDst[8]  = pVCpu->iem.s.abOpcode[8];  RT_FALL_THROUGH();
        case 8:     pbDst[7]  = pVCpu->iem.s.abOpcode[7];  RT_FALL_THROUGH();
        case 7:     pbDst[6]  = pVCpu->iem.s.abOpcode[6];  RT_FALL_THROUGH();
        case 6:     pbDst[5]  = pVCpu->iem.s.abOpcode[5];  RT_FALL_THROUGH();
        case 5:     pbDst[4]  = pVCpu->iem.s.abOpcode[4];  RT_FALL_THROUGH();
        case 4:     pbDst[3]  = pVCpu->iem.s.abOpcode[3];  RT_FALL_THROUGH();
        case 3:     pbDst[2]  = pVCpu->iem.s.abOpcode[2];  RT_FALL_THROUGH();
        case 2:     pbDst[1]  = pVCpu->iem.s.abOpcode[1];  RT_FALL_THROUGH();
        case 1:     pbDst[0]  = pVCpu->iem.s.abOpcode[0];  break;
    }
}


/**
 * Called by IEM_MC2_BEGIN_EMIT_CALLS() under one of these conditions:
 *
 *      - CS LIM check required.
 *      - Must recheck opcode bytes.
 *      - Previous instruction branched.
 *      - TLB load detected, probably due to page crossing.
 *
 * @returns true if everything went well, false if we're out of space in the TB
 *          (e.g. opcode ranges) or needs to start doing CS.LIM checks.
 * @param   pVCpu       The cross context virtual CPU structure of the calling
 *                      thread.
 * @param   pTb         The translation block being compiled.
 */
bool iemThreadedCompileBeginEmitCallsComplications(PVMCPUCC pVCpu, PIEMTB pTb)
{
    Assert((pVCpu->iem.s.GCPhysInstrBuf & GUEST_PAGE_OFFSET_MASK) == 0);
#if 0
    if (pVCpu->cpum.GstCtx.rip >= 0xc0000000 && !LogIsEnabled())
        RTLogChangeFlags(NULL, 0, RTLOGFLAGS_DISABLED);
#endif

    /*
     * If we're not in 64-bit mode and not already checking CS.LIM we need to
     * see if it's needed to start checking.
     */
    bool           fConsiderCsLimChecking;
    uint32_t const fMode = pVCpu->iem.s.fExec & IEM_F_MODE_MASK;
    if (   fMode == IEM_F_MODE_X86_64BIT
        || (pTb->fFlags & IEMTB_F_CS_LIM_CHECKS)
        || fMode == IEM_F_MODE_X86_32BIT_PROT_FLAT
        || fMode == IEM_F_MODE_X86_32BIT_FLAT)
        fConsiderCsLimChecking = false; /* already enabled or not needed */
    else
    {
        int64_t const offFromLim = (int64_t)pVCpu->cpum.GstCtx.cs.u32Limit - (int64_t)pVCpu->cpum.GstCtx.eip;
        if (offFromLim >= GUEST_PAGE_SIZE + 16 - (int32_t)(pVCpu->cpum.GstCtx.cs.u64Base & GUEST_PAGE_OFFSET_MASK))
            fConsiderCsLimChecking = true; /* likely */
        else
        {
            Log8(("%04x:%08RX64: Needs CS.LIM checks (%#RX64)\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip, offFromLim));
            return false;
        }
    }

    /*
     * Prepare call now, even before we know if can accept the instruction in this TB.
     * This allows us amending parameters w/o making every case suffer.
     */
    uint8_t const             cbInstr   = IEM_GET_INSTR_LEN(pVCpu);
    uint16_t const            offOpcode = pTb->cbOpcodes;
    uint8_t                   idxRange  = pTb->cRanges - 1;

    PIEMTHRDEDCALLENTRY const pCall     = &pTb->Thrd.paCalls[pTb->Thrd.cCalls];
    pCall->offOpcode   = offOpcode;
    pCall->idxRange    = idxRange;
    pCall->cbOpcode    = cbInstr;
    pCall->auParams[0] = cbInstr;
    pCall->auParams[1] = idxRange;
    pCall->auParams[2] = offOpcode - pTb->aRanges[idxRange].offOpcodes;

/** @todo check if we require IEMTB_F_CS_LIM_CHECKS for any new page we've
 *        gotten onto.  If we do, stop */

    /*
     * Case 1: We've branched (RIP changed).
     *
     * Sub-case 1a: Same page, no TLB load (fTbCrossedPage is false).
     *         Req: 1 extra range, no extra phys.
     *
     * Sub-case 1b: Different page but no page boundrary crossing, so TLB load
     *              necessary (fTbCrossedPage is true).
     *         Req: 1 extra range, probably 1 extra phys page entry.
     *
     * Sub-case 1c: Different page, so TLB load necessary (fTbCrossedPage is true),
     *              but in addition we cross into the following page and require
     *              another TLB load.
     *         Req: 2 extra ranges, probably 2 extra phys page entries.
     *
     * Sub-case 1d: Same page, so no initial TLB load necessary, but we cross into
     *              the following page (thus fTbCrossedPage is true).
     *         Req: 2 extra ranges, probably 1 extra phys page entry.
     *
     * Note! The setting fTbCrossedPage is done by the iemOpcodeFetchBytesJmp, but
     *       it may trigger "spuriously" from the CPU point of view because of
     *       physical page changes that'll invalid the physical TLB and trigger a
     *       call to the function.  In theory this be a big deal, just a bit
     *       performance loss as we'll pick the LoadingTlb variants.
     *
     * Note! We do not currently optimize branching to the next instruction (sorry
     *       32-bit PIC code).  We could maybe do that in the branching code that
     *       sets (or not) fTbBranched.
     */
    /** @todo Optimize 'jmp .next_instr' and 'call .next_instr'. Seen the jmp
     *        variant in win 3.1 code and the call variant in 32-bit linux PIC
     *        code.  This'll require filtering out far jmps and calls, as they
     *        load CS which should technically be considered indirect since the
     *        GDT/LDT entry's base address can be modified independently from
     *        the code. */
    if (pVCpu->iem.s.fTbBranched != 0)
    {
        if (   !pVCpu->iem.s.fTbCrossedPage       /* 1a */
            || pVCpu->iem.s.offCurInstrStart >= 0 /* 1b */ )
        {
            /* 1a + 1b - instruction fully within the branched to page. */
            Assert(pVCpu->iem.s.offCurInstrStart >= 0);
            Assert(pVCpu->iem.s.offCurInstrStart + cbInstr <= GUEST_PAGE_SIZE);

            if (!(pVCpu->iem.s.fTbBranched & IEMBRANCHED_F_ZERO))
            {
                /* Check that we've got a free range. */
                idxRange += 1;
                if (idxRange < RT_ELEMENTS(pTb->aRanges))
                { /* likely */ }
                else
                {
                    Log8(("%04x:%08RX64: out of ranges after branch\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));
                    return false;
                }
                pCall->idxRange    = idxRange;
                pCall->auParams[1] = idxRange;
                pCall->auParams[2] = 0;

                /* Check that we've got a free page slot. */
                AssertCompile(RT_ELEMENTS(pTb->aGCPhysPages) == 2);
                RTGCPHYS const GCPhysNew = pVCpu->iem.s.GCPhysInstrBuf & ~(RTGCPHYS)GUEST_PAGE_OFFSET_MASK;
                if ((pTb->GCPhysPc & ~(RTGCPHYS)GUEST_PAGE_OFFSET_MASK) == GCPhysNew)
                    pTb->aRanges[idxRange].idxPhysPage = 0;
                else if (   pTb->aGCPhysPages[0] == NIL_RTGCPHYS
                         || pTb->aGCPhysPages[0] == GCPhysNew)
                {
                    pTb->aGCPhysPages[0] = GCPhysNew;
                    pTb->aRanges[idxRange].idxPhysPage = 1;
                }
                else if (   pTb->aGCPhysPages[1] == NIL_RTGCPHYS
                         || pTb->aGCPhysPages[1] == GCPhysNew)
                {
                    pTb->aGCPhysPages[1] = GCPhysNew;
                    pTb->aRanges[idxRange].idxPhysPage = 2;
                }
                else
                {
                    Log8(("%04x:%08RX64: out of aGCPhysPages entires after branch\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));
                    return false;
                }

                /* Finish setting up the new range. */
                pTb->aRanges[idxRange].offPhysPage = pVCpu->iem.s.offCurInstrStart;
                pTb->aRanges[idxRange].offOpcodes  = offOpcode;
                pTb->aRanges[idxRange].cbOpcodes   = cbInstr;
                pTb->aRanges[idxRange].u2Unused    = 0;
                pTb->cRanges++;
            }
            else
            {
                Log8(("%04x:%08RX64: zero byte jump\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));
                pTb->aRanges[idxRange].cbOpcodes += cbInstr;
            }

            /* Determin which function we need to load & check.
               Note! For jumps to a new page, we'll set both fTbBranched and
                     fTbCrossedPage to avoid unnecessary TLB work for intra
                     page branching */
            if (   (pVCpu->iem.s.fTbBranched & (IEMBRANCHED_F_INDIRECT | IEMBRANCHED_F_FAR)) /* Far is basically indirect. */
                || pVCpu->iem.s.fTbCrossedPage)
                pCall->enmFunction = pTb->fFlags & IEMTB_F_CS_LIM_CHECKS
                                   ? kIemThreadedFunc_BltIn_CheckCsLimAndOpcodesLoadingTlb
                                   : !fConsiderCsLimChecking
                                   ? kIemThreadedFunc_BltIn_CheckOpcodesLoadingTlb
                                   : kIemThreadedFunc_BltIn_CheckOpcodesLoadingTlbConsiderCsLim;
            else if (pVCpu->iem.s.fTbBranched & (IEMBRANCHED_F_CONDITIONAL | /* paranoia: */ IEMBRANCHED_F_DIRECT))
                pCall->enmFunction = pTb->fFlags & IEMTB_F_CS_LIM_CHECKS
                                   ? kIemThreadedFunc_BltIn_CheckCsLimAndPcAndOpcodes
                                   : !fConsiderCsLimChecking
                                   ? kIemThreadedFunc_BltIn_CheckPcAndOpcodes
                                   : kIemThreadedFunc_BltIn_CheckPcAndOpcodesConsiderCsLim;
            else
            {
                Assert(pVCpu->iem.s.fTbBranched & IEMBRANCHED_F_RELATIVE);
                pCall->enmFunction = pTb->fFlags & IEMTB_F_CS_LIM_CHECKS
                                   ? kIemThreadedFunc_BltIn_CheckCsLimAndOpcodes
                                   : !fConsiderCsLimChecking
                                   ? kIemThreadedFunc_BltIn_CheckOpcodes
                                   : kIemThreadedFunc_BltIn_CheckOpcodesConsiderCsLim;
            }
        }
        else
        {
            /* 1c + 1d - instruction crosses pages. */
            Assert(pVCpu->iem.s.offCurInstrStart < 0);
            Assert(pVCpu->iem.s.offCurInstrStart + cbInstr > 0);

            /* Lazy bird: Check that this isn't case 1c, since we've already
                          load the first physical address.  End the TB and
                          make it a case 2b instead.

                          Hmm. Too much bother to detect, so just do the same
                          with case 1d as well. */
#if 0       /** @todo get back to this later when we've got the actual branch code in
             *        place. */
            uint8_t const cbStartPage = (uint8_t)-pVCpu->iem.s.offCurInstrStart;

            /* Check that we've got two free ranges. */
            if (idxRange + 2 < RT_ELEMENTS(pTb->aRanges))
            { /* likely */ }
            else
                return false;
            idxRange += 1;
            pCall->idxRange    = idxRange;
            pCall->auParams[1] = idxRange;
            pCall->auParams[2] = 0;

            /* ... */

#else
            Log8(("%04x:%08RX64: complicated post-branch condition, ending TB.\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));
            return false;
#endif
        }
    }

    /*
     * Case 2: Page crossing.
     *
     * Sub-case 2a: The instruction starts on the first byte in the next page.
     *
     * Sub-case 2b: The instruction has opcode bytes in both the current and
     *              following page.
     *
     * Both cases requires a new range table entry and probably a new physical
     * page entry.  The difference is in which functions to emit and whether to
     * add bytes to the current range.
     */
    else if (pVCpu->iem.s.fTbCrossedPage)
    {
        /* Check that we've got a free range. */
        idxRange += 1;
        if (idxRange < RT_ELEMENTS(pTb->aRanges))
        { /* likely */ }
        else
        {
            Log8(("%04x:%08RX64: out of ranges while crossing page\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));
            return false;
        }

        /* Check that we've got a free page slot. */
        AssertCompile(RT_ELEMENTS(pTb->aGCPhysPages) == 2);
        RTGCPHYS const GCPhysNew = pVCpu->iem.s.GCPhysInstrBuf & ~(RTGCPHYS)GUEST_PAGE_OFFSET_MASK;
        if ((pTb->GCPhysPc & ~(RTGCPHYS)GUEST_PAGE_OFFSET_MASK) == GCPhysNew)
            pTb->aRanges[idxRange].idxPhysPage = 0;
        else if (   pTb->aGCPhysPages[0] == NIL_RTGCPHYS
                 || pTb->aGCPhysPages[0] == GCPhysNew)
        {
            pTb->aGCPhysPages[0] = GCPhysNew;
            pTb->aRanges[idxRange].idxPhysPage = 1;
        }
        else if (   pTb->aGCPhysPages[1] == NIL_RTGCPHYS
                 || pTb->aGCPhysPages[1] == GCPhysNew)
        {
            pTb->aGCPhysPages[1] = GCPhysNew;
            pTb->aRanges[idxRange].idxPhysPage = 2;
        }
        else
        {
            Log8(("%04x:%08RX64: out of aGCPhysPages entires while crossing page\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));
            return false;
        }

        if (((pTb->aRanges[idxRange - 1].offPhysPage + pTb->aRanges[idxRange - 1].cbOpcodes) & GUEST_PAGE_OFFSET_MASK) == 0)
        {
            Assert(pVCpu->iem.s.offCurInstrStart == 0);
            pCall->idxRange    = idxRange;
            pCall->auParams[1] = idxRange;
            pCall->auParams[2] = 0;

            /* Finish setting up the new range. */
            pTb->aRanges[idxRange].offPhysPage = pVCpu->iem.s.offCurInstrStart;
            pTb->aRanges[idxRange].offOpcodes  = offOpcode;
            pTb->aRanges[idxRange].cbOpcodes   = cbInstr;
            pTb->aRanges[idxRange].u2Unused    = 0;
            pTb->cRanges++;

            /* Determin which function we need to load & check. */
            pCall->enmFunction = pTb->fFlags & IEMTB_F_CS_LIM_CHECKS
                               ? kIemThreadedFunc_BltIn_CheckCsLimAndOpcodesOnNewPageLoadingTlb
                               : !fConsiderCsLimChecking
                               ? kIemThreadedFunc_BltIn_CheckOpcodesOnNewPageLoadingTlb
                               : kIemThreadedFunc_BltIn_CheckOpcodesOnNewPageLoadingTlbConsiderCsLim;
        }
        else
        {
            Assert(pVCpu->iem.s.offCurInstrStart < 0);
            Assert(pVCpu->iem.s.offCurInstrStart + cbInstr > 0);
            uint8_t const cbStartPage = (uint8_t)-pVCpu->iem.s.offCurInstrStart;
            pCall->auParams[0] |= (uint64_t)cbStartPage << 32;

            /* We've good. Split the instruction over the old and new range table entries. */
            pTb->aRanges[idxRange - 1].cbOpcodes += cbStartPage;

            pTb->aRanges[idxRange].offPhysPage    = 0;
            pTb->aRanges[idxRange].offOpcodes     = offOpcode + cbStartPage;
            pTb->aRanges[idxRange].cbOpcodes      = cbInstr   - cbStartPage;
            pTb->aRanges[idxRange].u2Unused       = 0;
            pTb->cRanges++;

            /* Determin which function we need to load & check. */
            if (pVCpu->iem.s.fTbCheckOpcodes)
                pCall->enmFunction = pTb->fFlags & IEMTB_F_CS_LIM_CHECKS
                                   ? kIemThreadedFunc_BltIn_CheckCsLimAndOpcodesAcrossPageLoadingTlb
                                   : !fConsiderCsLimChecking
                                   ? kIemThreadedFunc_BltIn_CheckOpcodesAcrossPageLoadingTlb
                                   : kIemThreadedFunc_BltIn_CheckOpcodesAcrossPageLoadingTlbConsiderCsLim;
            else
                pCall->enmFunction = pTb->fFlags & IEMTB_F_CS_LIM_CHECKS
                                   ? kIemThreadedFunc_BltIn_CheckCsLimAndOpcodesOnNextPageLoadingTlb
                                   : !fConsiderCsLimChecking
                                   ? kIemThreadedFunc_BltIn_CheckOpcodesOnNextPageLoadingTlb
                                   : kIemThreadedFunc_BltIn_CheckOpcodesOnNextPageLoadingTlbConsiderCsLim;
        }
    }

    /*
     * Regular case: No new range required.
     */
    else
    {
        Assert(pVCpu->iem.s.fTbCheckOpcodes || (pTb->fFlags & IEMTB_F_CS_LIM_CHECKS));
        if (pVCpu->iem.s.fTbCheckOpcodes)
            pCall->enmFunction = pTb->fFlags & IEMTB_F_CS_LIM_CHECKS
                               ? kIemThreadedFunc_BltIn_CheckCsLimAndOpcodes
                               : kIemThreadedFunc_BltIn_CheckOpcodes;
        else
            pCall->enmFunction = kIemThreadedFunc_BltIn_CheckCsLim;

        iemThreadedCopyOpcodeBytesInline(pVCpu, &pTb->pabOpcodes[offOpcode], cbInstr);
        pTb->cbOpcodes                    = offOpcode + cbInstr;
        pTb->aRanges[idxRange].cbOpcodes += cbInstr;
        Assert(pTb->cbOpcodes <= pTb->cbOpcodesAllocated);
    }

    /*
     * Commit the call.
     */
    pTb->Thrd.cCalls++;

    /*
     * Clear state.
     */
    pVCpu->iem.s.fTbBranched     = IEMBRANCHED_F_NO;
    pVCpu->iem.s.fTbCrossedPage  = false;
    pVCpu->iem.s.fTbCheckOpcodes = false;

    /*
     * Copy opcode bytes.
     */
    iemThreadedCopyOpcodeBytesInline(pVCpu, &pTb->pabOpcodes[offOpcode], cbInstr);
    pTb->cbOpcodes = offOpcode + cbInstr;
    Assert(pTb->cbOpcodes <= pTb->cbOpcodesAllocated);

    return true;
}


/**
 * Worker for iemThreadedCompileBeginEmitCallsComplications and
 * iemThreadedCompileCheckIrq that checks for pending delivarable events.
 *
 * @returns true if anything is pending, false if not.
 * @param   pVCpu       The cross context virtual CPU structure of the calling
 *                      thread.
 */
DECL_FORCE_INLINE(bool) iemThreadedCompileIsIrqOrForceFlagPending(PVMCPUCC pVCpu)
{
    uint64_t fCpu = pVCpu->fLocalForcedActions;
    fCpu &= VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC | VMCPU_FF_INTERRUPT_NMI | VMCPU_FF_INTERRUPT_SMI;
#if 1
    /** @todo this isn't even close to the NMI/IRQ conditions in EM. */
    if (RT_LIKELY(   !fCpu
                  || (   !(fCpu & ~(VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC))
                      && (   !pVCpu->cpum.GstCtx.rflags.Bits.u1IF
                          || CPUMIsInInterruptShadow(&pVCpu->cpum.GstCtx))) ))
        return false;
    return true;
#else
    return false;
#endif

}


/**
 * Called by IEM_MC2_BEGIN_EMIT_CALLS() when IEM_CIMPL_F_CHECK_IRQ_BEFORE is
 * set.
 *
 * @returns true if we should continue, false if an IRQ is deliverable or a
 *          relevant force flag is pending.
 * @param   pVCpu       The cross context virtual CPU structure of the calling
 *                      thread.
 * @param   pTb         The translation block being compiled.
 * @sa      iemThreadedCompileCheckIrq
 */
bool iemThreadedCompileEmitIrqCheckBefore(PVMCPUCC pVCpu, PIEMTB pTb)
{
    /*
     * Skip this we've already emitted a call after the previous instruction
     * or if it's the first call, as we're always checking FFs between blocks.
     */
    uint32_t const idxCall = pTb->Thrd.cCalls;
    if (   idxCall > 0
        && pTb->Thrd.paCalls[idxCall - 1].enmFunction != kIemThreadedFunc_BltIn_CheckIrq)
    {
        /* Emit the call. */
        AssertReturn(idxCall < pTb->Thrd.cAllocated, false);
        PIEMTHRDEDCALLENTRY pCall = &pTb->Thrd.paCalls[idxCall];
        pTb->Thrd.cCalls = (uint16_t)(idxCall + 1);
        pCall->enmFunction = kIemThreadedFunc_BltIn_CheckIrq;
        pCall->uUnused0    = 0;
        pCall->offOpcode   = 0;
        pCall->cbOpcode    = 0;
        pCall->idxRange    = 0;
        pCall->auParams[0] = 0;
        pCall->auParams[1] = 0;
        pCall->auParams[2] = 0;
        LogFunc(("%04x:%08RX64\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));

        /* Reset the IRQ check value. */
        pVCpu->iem.s.cInstrTillIrqCheck = !CPUMIsInInterruptShadow(&pVCpu->cpum.GstCtx) ? 32 : 0;

        /*
         * Check for deliverable IRQs and pending force flags.
         */
        return !iemThreadedCompileIsIrqOrForceFlagPending(pVCpu);
    }
    return true; /* continue */
}


/**
 * Emits an IRQ check call and checks for pending IRQs.
 *
 * @returns true if we should continue, false if an IRQ is deliverable or a
 *          relevant force flag is pending.
 * @param   pVCpu       The cross context virtual CPU structure of the calling
 *                      thread.
 * @param   pTb         The transation block.
 * @sa      iemThreadedCompileBeginEmitCallsComplications
 */
static bool iemThreadedCompileCheckIrqAfter(PVMCPUCC pVCpu, PIEMTB pTb)
{
    /* Check again in a little bit, unless it is immediately following an STI
       in which case we *must* check immediately after the next instruction
       as well in case it's executed with interrupt inhibition.  We could
       otherwise miss the interrupt window. See the irq2 wait2 varaiant in
       bs3-timers-1 which is doing sti + sti + cli. */
    if (!pVCpu->iem.s.fTbCurInstrIsSti)
        pVCpu->iem.s.cInstrTillIrqCheck = 32;
    else
    {
        pVCpu->iem.s.fTbCurInstrIsSti   = false;
        pVCpu->iem.s.cInstrTillIrqCheck = 0;
    }
    LogFunc(("%04x:%08RX64\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));

    /*
     * Emit the call.
     */
    AssertReturn(pTb->Thrd.cCalls < pTb->Thrd.cAllocated, false);
    PIEMTHRDEDCALLENTRY pCall = &pTb->Thrd.paCalls[pTb->Thrd.cCalls++];
    pCall->enmFunction = kIemThreadedFunc_BltIn_CheckIrq;
    pCall->uUnused0    = 0;
    pCall->offOpcode   = 0;
    pCall->cbOpcode    = 0;
    pCall->idxRange    = 0;
    pCall->auParams[0] = 0;
    pCall->auParams[1] = 0;
    pCall->auParams[2] = 0;

    /*
     * Check for deliverable IRQs and pending force flags.
     */
    return !iemThreadedCompileIsIrqOrForceFlagPending(pVCpu);
}


/**
 * Compiles a new TB and executes it.
 *
 * We combine compilation and execution here as it makes it simpler code flow
 * in the main loop and it allows interpreting while compiling if we want to
 * explore that option.
 *
 * @returns Strict VBox status code.
 * @param   pVM         The cross context virtual machine structure.
 * @param   pVCpu       The cross context virtual CPU structure of the calling
 *                      thread.
 * @param   GCPhysPc    The physical address corresponding to the current
 *                      RIP+CS.BASE.
 * @param   fExtraFlags Extra translation block flags: IEMTB_F_TYPE_THREADED and
 *                      maybe IEMTB_F_RIP_CHECKS.
 */
static VBOXSTRICTRC iemThreadedCompile(PVMCC pVM, PVMCPUCC pVCpu, RTGCPHYS GCPhysPc, uint32_t fExtraFlags) IEM_NOEXCEPT_MAY_LONGJMP
{
    /*
     * Allocate a new translation block.
     */
    PIEMTB pTb = iemThreadedTbAlloc(pVM, pVCpu, GCPhysPc, fExtraFlags | IEMTB_F_STATE_COMPILING);
    AssertReturn(pTb, VERR_IEM_TB_ALLOC_FAILED);

    /* Set the current TB so iemThreadedCompileLongJumped and the CIMPL
       functions may get at it. */
    pVCpu->iem.s.pCurTbR3 = pTb;

#if 0
    /* Make sure the CheckIrq condition matches the one in EM. */
    iemThreadedCompileCheckIrqAfter(pVCpu, pTb);
    const uint32_t cZeroCalls = 1;
#else
    const uint32_t cZeroCalls = 0;
#endif

    /*
     * Now for the recomplication. (This mimicks IEMExecLots in many ways.)
     */
    iemThreadedCompileInitDecoder(pVCpu, false /*fReInit*/, fExtraFlags);
    iemThreadedCompileInitOpcodeFetching(pVCpu);
    VBOXSTRICTRC rcStrict;
    for (;;)
    {
        /* Process the next instruction. */
#ifdef LOG_ENABLED
        iemThreadedLogCurInstr(pVCpu, "CC");
        uint16_t const uCsLog  = pVCpu->cpum.GstCtx.cs.Sel;
        uint64_t const uRipLog = pVCpu->cpum.GstCtx.rip;
#endif
        uint8_t b; IEM_OPCODE_GET_FIRST_U8(&b);
        uint16_t const cCallsPrev = pTb->Thrd.cCalls;

        rcStrict = FNIEMOP_CALL(g_apfnIemThreadedRecompilerOneByteMap[b]);
        if (   rcStrict == VINF_SUCCESS
            && pVCpu->iem.s.rcPassUp == VINF_SUCCESS
            && !pVCpu->iem.s.fEndTb)
        {
            Assert(pTb->Thrd.cCalls > cCallsPrev);
            Assert(cCallsPrev - pTb->Thrd.cCalls < 5);

            pVCpu->iem.s.cInstructions++;
        }
        else
        {
            Log8(("%04x:%08RX64: End TB - %u instr, %u calls, rc=%d\n",
                  uCsLog, uRipLog, pTb->cInstructions, pTb->Thrd.cCalls, VBOXSTRICTRC_VAL(rcStrict)));
            if (rcStrict == VINF_IEM_RECOMPILE_END_TB)
                rcStrict = VINF_SUCCESS;

            if (pTb->Thrd.cCalls > cZeroCalls)
            {
                if (cCallsPrev != pTb->Thrd.cCalls)
                    pVCpu->iem.s.cInstructions++;
                break;
            }

            pVCpu->iem.s.pCurTbR3 = NULL;
            iemThreadedTbFree(pVM, pVCpu, pTb);
            return iemExecStatusCodeFiddling(pVCpu, rcStrict);
        }

        /* Check for IRQs? */
        if (pVCpu->iem.s.cInstrTillIrqCheck > 0)
            pVCpu->iem.s.cInstrTillIrqCheck--;
        else if (!iemThreadedCompileCheckIrqAfter(pVCpu, pTb))
            break;

        /* Still space in the TB? */
        if (   pTb->Thrd.cCalls + 5 < pTb->Thrd.cAllocated
            && pTb->cbOpcodes + 16 <= pTb->cbOpcodesAllocated)
            iemThreadedCompileInitDecoder(pVCpu, true /*fReInit*/, 0);
        else
        {
            Log8(("%04x:%08RX64: End TB - %u instr, %u calls, %u opcode bytes - full\n",
                  uCsLog, uRipLog, pTb->cInstructions, pTb->Thrd.cCalls, pTb->cbOpcodes));
            break;
        }
        iemThreadedCompileReInitOpcodeFetching(pVCpu);
    }

    /*
     * Complete the TB and link it.
     */
    pTb->fFlags = (pTb->fFlags & ~IEMTB_F_STATE_MASK) | IEMTB_F_STATE_READY;
    iemThreadedTbAdd(pVM, pVCpu, pTb);

#ifdef IEM_COMPILE_ONLY_MODE
    /*
     * Execute the translation block.
     */
#endif

    return iemExecStatusCodeFiddling(pVCpu, rcStrict);
}


/**
 * Executes a translation block.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu   The cross context virtual CPU structure of the calling
 *                  thread.
 * @param   pTb     The translation block to execute.
 */
static VBOXSTRICTRC iemThreadedTbExec(PVMCPUCC pVCpu, PIEMTB pTb) IEM_NOEXCEPT_MAY_LONGJMP
{
    /* Check the opcodes in the first page before starting execution. */
    Assert(!(pVCpu->iem.s.GCPhysInstrBuf & (RTGCPHYS)GUEST_PAGE_OFFSET_MASK));
    Assert(pTb->aRanges[0].cbOpcodes <= pVCpu->iem.s.cbInstrBufTotal - pVCpu->iem.s.offInstrNextByte);
    if (memcmp(pTb->pabOpcodes, &pVCpu->iem.s.pbInstrBuf[pTb->aRanges[0].offPhysPage], pTb->aRanges[0].cbOpcodes) == 0)
    { /* likely */ }
    else
    {
        Log7(("TB obsolete: %p GCPhys=%RGp\n", pTb, pTb->GCPhysPc));
        iemThreadedTbFree(pVCpu->pVMR3, pVCpu, pTb);
        return VINF_SUCCESS;
    }

    /* Set the current TB so CIMPL function may get at it. */
    pVCpu->iem.s.pCurTbR3 = pTb;
    pVCpu->iem.s.cTbExec++;

    /* The execution loop. */
#ifdef LOG_ENABLED
    uint64_t             uRipPrev   = UINT64_MAX;
#endif
    PCIEMTHRDEDCALLENTRY pCallEntry = pTb->Thrd.paCalls;
    uint32_t             cCallsLeft = pTb->Thrd.cCalls;
    while (cCallsLeft-- > 0)
    {
#ifdef LOG_ENABLED
        if (pVCpu->cpum.GstCtx.rip != uRipPrev)
        {
            uRipPrev = pVCpu->cpum.GstCtx.rip;
            iemThreadedLogCurInstr(pVCpu, "EX");
        }
        Log9(("%04x:%08RX64: #%d - %d %s\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip,
              pTb->Thrd.cCalls - cCallsLeft - 1, pCallEntry->enmFunction, g_apszIemThreadedFunctions[pCallEntry->enmFunction]));
#endif
        VBOXSTRICTRC const rcStrict = g_apfnIemThreadedFunctions[pCallEntry->enmFunction](pVCpu,
                                                                                          pCallEntry->auParams[0],
                                                                                          pCallEntry->auParams[1],
                                                                                          pCallEntry->auParams[2]);
        if (RT_LIKELY(   rcStrict == VINF_SUCCESS
                      && pVCpu->iem.s.rcPassUp == VINF_SUCCESS /** @todo this isn't great. */))
            pCallEntry++;
        else
        {
            pVCpu->iem.s.pCurTbR3 = NULL;

            /* Some status codes are just to get us out of this loop and
               continue in a different translation block. */
            if (rcStrict == VINF_IEM_REEXEC_BREAK)
                return iemExecStatusCodeFiddling(pVCpu, VINF_SUCCESS);
            return iemExecStatusCodeFiddling(pVCpu, rcStrict);
        }
    }

    pVCpu->iem.s.cInstructions += pTb->cInstructions;
    pVCpu->iem.s.pCurTbR3 = NULL;
    return VINF_SUCCESS;
}


/**
 * This is called when the PC doesn't match the current pbInstrBuf.
 *
 * Upon return, we're ready for opcode fetching.  But please note that
 * pbInstrBuf can be NULL iff the memory doesn't have readable backing (i.e.
 * MMIO or unassigned).
 */
static RTGCPHYS iemGetPcWithPhysAndCodeMissed(PVMCPUCC pVCpu)
{
    pVCpu->iem.s.pbInstrBuf       = NULL;
    pVCpu->iem.s.offCurInstrStart = 0;
    pVCpu->iem.s.offInstrNextByte = 0;
    iemOpcodeFetchBytesJmp(pVCpu, 0, NULL);
    return pVCpu->iem.s.GCPhysInstrBuf + pVCpu->iem.s.offCurInstrStart;
}


/** @todo need private inline decl for throw/nothrow matching IEM_WITH_SETJMP? */
DECL_FORCE_INLINE_THROW(RTGCPHYS) iemGetPcWithPhysAndCode(PVMCPUCC pVCpu)
{
    /*
     * Set uCurTbStartPc to RIP and calc the effective PC.
     */
    uint64_t uPc = pVCpu->cpum.GstCtx.rip;
    pVCpu->iem.s.uCurTbStartPc = uPc;
    Assert(pVCpu->cpum.GstCtx.cs.u64Base == 0 || !IEM_IS_64BIT_CODE(pVCpu));
    uPc += pVCpu->cpum.GstCtx.cs.u64Base;

    /*
     * Advance within the current buffer (PAGE) when possible.
     */
    if (pVCpu->iem.s.pbInstrBuf)
    {
        uint64_t off = uPc - pVCpu->iem.s.uInstrBufPc;
        if (off < pVCpu->iem.s.cbInstrBufTotal)
        {
            pVCpu->iem.s.offInstrNextByte = (uint32_t)off;
            pVCpu->iem.s.offCurInstrStart = (uint16_t)off;
            if ((uint16_t)off + 15 <= pVCpu->iem.s.cbInstrBufTotal)
                pVCpu->iem.s.cbInstrBuf = (uint16_t)off + 15;
            else
                pVCpu->iem.s.cbInstrBuf = pVCpu->iem.s.cbInstrBufTotal;

            return pVCpu->iem.s.GCPhysInstrBuf + off;
        }
    }
    return iemGetPcWithPhysAndCodeMissed(pVCpu);
}


/**
 * Determines the extra IEMTB_F_XXX flags.
 *
 * @returns IEMTB_F_TYPE_THREADED and maybe IEMTB_F_RIP_CHECKS.
 * @param   pVCpu   The cross context virtual CPU structure of the calling
 *                  thread.
 */
DECL_FORCE_INLINE(uint32_t) iemGetTbFlagsForCurrentPc(PVMCPUCC pVCpu)
{
    uint32_t fRet = IEMTB_F_TYPE_THREADED;

    /*
     * Determine the inhibit bits.
     */
    if (!(pVCpu->cpum.GstCtx.rflags.uBoth & (IEMTB_F_INHIBIT_SHADOW | IEMTB_F_INHIBIT_NMI)))
    { /* typical */ }
    else
    {
        if (CPUMIsInInterruptShadow(&pVCpu->cpum.GstCtx))
            fRet |= IEMTB_F_INHIBIT_SHADOW;
        if (CPUMAreInterruptsInhibitedByNmiEx(&pVCpu->cpum.GstCtx))
            fRet |= IEMTB_F_INHIBIT_NMI;
    }

    /*
     * Return IEMTB_F_CS_LIM_CHECKS if the current PC is invalid or if it is
     * likely to go invalid before the end of the translation block.
     */
    if (IEM_IS_64BIT_CODE(pVCpu))
        return fRet;

    int64_t const offFromLim = (int64_t)pVCpu->cpum.GstCtx.cs.u32Limit - (int64_t)pVCpu->cpum.GstCtx.eip;
    if (offFromLim >= X86_PAGE_SIZE + 16 - (int32_t)(pVCpu->cpum.GstCtx.cs.u64Base & GUEST_PAGE_OFFSET_MASK))
        return fRet;
    return fRet | IEMTB_F_CS_LIM_CHECKS;
}


VMMDECL(VBOXSTRICTRC) IEMExecRecompilerThreaded(PVMCC pVM, PVMCPUCC pVCpu)
{
    /*
     * See if there is an interrupt pending in TRPM, inject it if we can.
     */
    if (!TRPMHasTrap(pVCpu))
    { /* likely */ }
    else
    {
        VBOXSTRICTRC rcStrict = iemExecInjectPendingTrap(pVCpu);
        if (RT_LIKELY(rcStrict == VINF_SUCCESS))
        { /*likely */ }
        else
            return rcStrict;
    }

    /*
     * Init the execution environment.
     */
    iemInitExec(pVCpu, 0 /*fExecOpts*/);

    /*
     * Run-loop.
     *
     * If we're using setjmp/longjmp we combine all the catching here to avoid
     * having to call setjmp for each block we're executing.
     */
    for (;;)
    {
        PIEMTB       pTb = NULL;
        VBOXSTRICTRC rcStrict;
        IEM_TRY_SETJMP(pVCpu, rcStrict)
        {
            uint32_t const cPollRate = 511; /* EM.cpp passes 4095 to IEMExecLots, so an eigth of that seems reasonable for now. */
            for (uint32_t iIterations = 0; ; iIterations++)
            {
                /* Translate PC to physical address, we'll need this for both lookup and compilation. */
                RTGCPHYS const GCPhysPc    = iemGetPcWithPhysAndCode(pVCpu);
                uint32_t const fExtraFlags = iemGetTbFlagsForCurrentPc(pVCpu);

                pTb = iemThreadedTbLookup(pVM, pVCpu, GCPhysPc, fExtraFlags);
                if (pTb)
                    rcStrict = iemThreadedTbExec(pVCpu, pTb);
                else
                    rcStrict = iemThreadedCompile(pVM, pVCpu, GCPhysPc, fExtraFlags);
                if (rcStrict == VINF_SUCCESS)
                {
                    Assert(pVCpu->iem.s.cActiveMappings == 0);

                    uint64_t fCpu = pVCpu->fLocalForcedActions;
                    fCpu &= VMCPU_FF_ALL_MASK & ~(  VMCPU_FF_PGM_SYNC_CR3
                                                  | VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL
                                                  | VMCPU_FF_TLB_FLUSH
                                                  | VMCPU_FF_UNHALT );
                    /** @todo this isn't even close to the NMI/IRQ conditions in EM. */
                    if (RT_LIKELY(   (   !fCpu
                                      || (   !(fCpu & ~(VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC))
                                          && (   !pVCpu->cpum.GstCtx.rflags.Bits.u1IF
                                              || CPUMIsInInterruptShadow(&pVCpu->cpum.GstCtx) )) )
                                  && !VM_FF_IS_ANY_SET(pVM, VM_FF_ALL_MASK) ))
                    {
                        if (RT_LIKELY(   (iIterations & cPollRate) != 0
                                      || !TMTimerPollBool(pVM, pVCpu)))
                        {

                        }
                        else
                            return VINF_SUCCESS;
                    }
                    else
                        return VINF_SUCCESS;
                }
                else
                    return rcStrict;
            }
        }
        IEM_CATCH_LONGJMP_BEGIN(pVCpu, rcStrict);
        {
            pVCpu->iem.s.cLongJumps++;
            if (pVCpu->iem.s.cActiveMappings > 0)
                iemMemRollback(pVCpu);

            /* If pTb isn't NULL we're in iemThreadedTbExec. */
            if (!pTb)
            {
                /* If pCurTbR3 is NULL, we're in iemGetPcWithPhysAndCode.*/
                pTb = pVCpu->iem.s.pCurTbR3;
                if (pTb)
                {
                    /* If the pCurTbR3 block is in compiling state, we're in iemThreadedCompile,
                       otherwise it's iemThreadedTbExec inside iemThreadedCompile (compile option). */
                    if ((pTb->fFlags & IEMTB_F_STATE_MASK) == IEMTB_F_STATE_COMPILING)
                        return iemThreadedCompileLongJumped(pVM, pVCpu, rcStrict);
                }
            }
            return rcStrict;
        }
        IEM_CATCH_LONGJMP_END(pVCpu);
    }
}

