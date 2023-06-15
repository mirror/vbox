/* $Id$ */
/** @file
 * IEM - Instruction Decoding and Threaded Recompilation.
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
# define LOG_GROUP LOG_GROUP_IEM
#endif
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
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * A call for the threaded call table.
 */
typedef struct IEMTHRDEDCALLENTRY
{
    /** The function to call (IEMTHREADEDFUNCS). */
    uint16_t    enmFunction;
    uint16_t    uUnused0;

    /** The opcode length. */
    uint8_t     cbOpcode;
    /** The opcode chunk number.
     * @note sketches for discontiguous opcode support  */
    uint8_t     idxOpcodeChunk;
    /** The offset into the opcode chunk of this function.
     * @note sketches for discontiguous opcode support  */
    uint16_t    offOpcodeChunk;

    /** Generic parameters. */
    uint64_t    auParams[3];
} IEMTHRDEDCALLENTRY;
AssertCompileSize(IEMTHRDEDCALLENTRY, sizeof(uint64_t) * 4);
/** Pointer to a threaded call entry. */
typedef IEMTHRDEDCALLENTRY       *PIEMTHRDEDCALLENTRY;
/** Pointer to a const threaded call entry. */
typedef IEMTHRDEDCALLENTRY const *PCIEMTHRDEDCALLENTRY;



/**
 * Translation block.
 */
typedef struct IEMTB
{
    /** Next block with the same hash table entry. */
    PIEMTB volatile     pNext;
    /** List on the local VCPU for blocks. */
    RTLISTNODE          LocalList;

    /** @name What uniquely identifies the block.
     * @{ */
    RTGCPHYS            GCPhysPc;
    /** IEMTB_F_XXX (i.e. IEM_F_XXX ++). */
    uint32_t            fFlags;
    union
    {
        struct
        {
            /** @todo we actually need BASE, LIM and CS?  If we don't tie a TB to a RIP
             * range, because that's bad for PIC/PIE code on unix with address space
             * randomization enabled, the assumption is that anything involving PC
             * (RIP/EIP/IP, maybe + CS.BASE) will be done by reading current register
             * values and not embedding presumed values into the code. Thus the uCsBase
             * member here shouldn't be needed.  For the same reason, uCsLimit isn't helpful
             * either as RIP/EIP/IP may differ between address spaces.  So, before TB
             * execution we'd need to check CS.LIM against RIP+cbPC (ditto for 64-bit
             * canonicallity).
             *
             * We could bake instruction limit / canonicallity checks into the generated
             * code if we find ourselves close to the limit and should expect to run into
             * it by the end of the translation block. That would just be using a very
             * simple threshold distance and be a special IEMTB_F_XXX flag so we figure out
             * it out when picking the TB.
             *
             * The CS value is likewise useless as we'll always be using the actual CS
             * register value whenever it is relevant (mainly pushing to the stack in a
             * call, trap, whatever).
             *
             * The segment attributes should be handled via the IEM_F_MODE_XXX and
             * IEM_F_X86_CPL_MASK portions of fFlags, so we could skip those too, I think.
             * All the places where they matter, we would be in CIMPL code which would
             * consult the actual CS.ATTR and not depend on the recompiled code block.
             */
            /** The CS base. */
            uint32_t uCsBase;
            /** The CS limit (UINT32_MAX for 64-bit code). */
            uint32_t uCsLimit;
            /** The CS selector value. */
            uint16_t CS;
            /**< Relevant CS X86DESCATTR_XXX bits. */
            uint16_t fAttr;
        } x86;
    };
    /** @} */

    /** Number of bytes of opcodes covered by this block.
     * @todo Support discontiguous chunks of opcodes in same block, though maybe
     *       restrict to the initial page or smth. */
    uint32_t    cbPC;

    union
    {
        struct
        {
            /** Number of calls in paCalls. */
            uint16_t            cCalls;
            /** Number of calls allocated. */
            uint16_t            cAllocated;
            /** The call sequence table. */
            PIEMTHRDEDCALLENTRY paCalls;
        } Thrd;
    };
} IEMTB;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static VBOXSTRICTRC iemThreadedTbExec(PVMCPUCC pVCpu, PIEMTB pTb);


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define g_apfnOneByteMap    g_apfnIemThreadedRecompilerOneByteMap


#undef IEM_MC_CALC_RM_EFF_ADDR
#ifndef IEM_WITH_SETJMP
# define IEM_MC_CALC_RM_EFF_ADDR(a_GCPtrEff, bRm, cbImm) \
    uint64_t uEffAddrInfo; \
    IEM_MC_RETURN_ON_FAILURE(iemOpHlpCalcRmEffAddrJmpEx(pVCpu, (bRm), (cbImm), &(a_GCPtrEff), &uEffAddrInfo))
#else
# define IEM_MC_CALC_RM_EFF_ADDR(a_GCPtrEff, bRm, cbImm) \
    uint64_t uEffAddrInfo; \
    ((a_GCPtrEff) = iemOpHlpCalcRmEffAddrJmpEx(pVCpu, (bRm), (cbImm), &uEffAddrInfo))
#endif

#define IEM_MC2_EMIT_CALL_0(a_enmFunction) do { \
        IEMTHREADEDFUNCS const enmFunctionCheck = a_enmFunction; RT_NOREF(enmFunctionCheck); \
        \
        PIEMTB              const pTb   = pVCpu->iem.s.pCurTbR3; \
        PIEMTHRDEDCALLENTRY const pCall = &pTb->Thrd.paCalls[pTb->Thrd.cCalls++]; \
        pCall->enmFunction = a_enmFunction; \
        pCall->cbOpcode    = IEM_GET_INSTR_LEN(pVCpu); \
        pCall->auParams[0] = 0; \
        pCall->auParams[1] = 0; \
        pCall->auParams[2] = 0; \
    } while (0)
#define IEM_MC2_EMIT_CALL_1(a_enmFunction, a_uArg0) do { \
        IEMTHREADEDFUNCS const enmFunctionCheck = a_enmFunction; RT_NOREF(enmFunctionCheck); \
        uint64_t         const uArg0Check       = (a_uArg0);     RT_NOREF(uArg0Check); \
        \
        PIEMTB              const pTb   = pVCpu->iem.s.pCurTbR3; \
        PIEMTHRDEDCALLENTRY const pCall = &pTb->Thrd.paCalls[pTb->Thrd.cCalls++]; \
        pCall->enmFunction = a_enmFunction; \
        pCall->cbOpcode    = IEM_GET_INSTR_LEN(pVCpu); \
        pCall->auParams[0] = a_uArg0; \
        pCall->auParams[1] = 0; \
        pCall->auParams[2] = 0; \
    } while (0)
#define IEM_MC2_EMIT_CALL_2(a_enmFunction, a_uArg0, a_uArg1) do { \
        IEMTHREADEDFUNCS const enmFunctionCheck = a_enmFunction; RT_NOREF(enmFunctionCheck); \
        uint64_t         const uArg0Check       = (a_uArg0);     RT_NOREF(uArg0Check); \
        uint64_t         const uArg1Check       = (a_uArg1);     RT_NOREF(uArg1Check); \
        \
        PIEMTB              const pTb   = pVCpu->iem.s.pCurTbR3; \
        PIEMTHRDEDCALLENTRY const pCall = &pTb->Thrd.paCalls[pTb->Thrd.cCalls++]; \
        pCall->enmFunction = a_enmFunction; \
        pCall->cbOpcode    = IEM_GET_INSTR_LEN(pVCpu); \
        pCall->auParams[0] = a_uArg0; \
        pCall->auParams[1] = a_uArg1; \
        pCall->auParams[2] = 0; \
    } while (0)
#define IEM_MC2_EMIT_CALL_3(a_enmFunction, a_uArg0, a_uArg1, a_uArg2) do { \
        IEMTHREADEDFUNCS const enmFunctionCheck = a_enmFunction; RT_NOREF(enmFunctionCheck); \
        uint64_t         const uArg0Check       = (a_uArg0);     RT_NOREF(uArg0Check); \
        uint64_t         const uArg1Check       = (a_uArg1);     RT_NOREF(uArg1Check); \
        uint64_t         const uArg2Check       = (a_uArg2);     RT_NOREF(uArg2Check); \
        \
        PIEMTB              const pTb   = pVCpu->iem.s.pCurTbR3; \
        PIEMTHRDEDCALLENTRY const pCall = &pTb->Thrd.paCalls[pTb->Thrd.cCalls++]; \
        pCall->enmFunction = a_enmFunction; \
        pCall->cbOpcode    = IEM_GET_INSTR_LEN(pVCpu); \
        pCall->auParams[0] = a_uArg0; \
        pCall->auParams[1] = a_uArg1; \
        pCall->auParams[2] = a_uArg2; \
    } while (0)


/*
 * IEM_MC_DEFER_TO_CIMPL_0 is easily wrapped up.
 *
 * Doing so will also take care of IEMOP_RAISE_DIVIDE_ERROR, IEMOP_RAISE_INVALID_LOCK_PREFIX,
 * IEMOP_RAISE_INVALID_OPCODE and their users.
 */
#undef IEM_MC_DEFER_TO_CIMPL_0_RET
#define IEM_MC_DEFER_TO_CIMPL_0_RET(a_fFlags, a_pfnCImpl) return iemThreadedRecompilerMcDeferToCImpl0(pVCpu, a_pfnCImpl)

typedef IEM_CIMPL_DECL_TYPE_0(FNIEMCIMPL0);
typedef FNIEMCIMPL0 *PFNIEMCIMPL0;

DECLINLINE(VBOXSTRICTRC) iemThreadedRecompilerMcDeferToCImpl0(PVMCPUCC pVCpu, PFNIEMCIMPL0 pfnCImpl)
{
    return pfnCImpl(pVCpu, IEM_GET_INSTR_LEN(pVCpu));
}

/** @todo deal with IEM_MC_DEFER_TO_CIMPL_1, IEM_MC_DEFER_TO_CIMPL_2 and
 *        IEM_MC_DEFER_TO_CIMPL_3 as well. */

/*
 * Include the "annotated" IEMAllInstructions*.cpp.h files.
 */
#include "IEMThreadedInstructions.cpp.h"

/*
 * Translation block management.
 */

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
        pTb->Thrd.paCalls = (PIEMTHRDEDCALLENTRY)RTMemAlloc(sizeof(IEMTHRDEDCALLENTRY) * 128);
        if (pTb->Thrd.paCalls)
        {
            pTb->Thrd.cAllocated = 128;
            pTb->Thrd.cCalls     = 0;
            pTb->pNext           = NULL;
            RTListInit(&pTb->LocalList);
            pTb->cbPC            = 0;
            pTb->GCPhysPc        = GCPhysPc;
            pTb->x86.uCsBase     = (uint32_t)pVCpu->cpum.GstCtx.cs.u64Base;
            pTb->x86.uCsLimit    = (uint32_t)pVCpu->cpum.GstCtx.cs.u32Limit;
            pTb->x86.CS          = (uint32_t)pVCpu->cpum.GstCtx.cs.Sel;
            pTb->x86.fAttr       = (uint16_t)pVCpu->cpum.GstCtx.cs.Attr.u;
            pTb->fFlags          = (pVCpu->iem.s.fExec & IEMTB_F_IEM_F_MASK) | fExtraFlags;
            pVCpu->iem.s.cTbAllocs++;
            return pTb;
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

    AssertCompile((IEMTB_F_STATE_OBSOLETE >> IEMTB_F_STATE_SHIFT) == (IEMTB_F_STATE_MASK >> IEMTB_F_STATE_SHIFT));
    pTb->fFlags |= IEMTB_F_STATE_OBSOLETE; /* works, both bits set */

    RTMemFree(pTb->Thrd.paCalls);
    pTb->Thrd.paCalls = NULL;

    RTMemFree(pTb);
    pVCpu->iem.s.cTbFrees++;
}


static PIEMTB iemThreadedTbLookup(PVMCC pVM, PVMCPUCC pVCpu, RTGCPHYS GCPhysPc, uint64_t uPc, uint32_t fExtraFlags)
{
    RT_NOREF(pVM, pVCpu, GCPhysPc, uPc, fExtraFlags);
    return NULL;
}


/*
 * Real code.
 */

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
 * @param   pVCpu   The cross context virtual CPU structure of the calling
 *                  thread.
 * @param   fReInit Clear for the first call for a TB, set for subsequent calls
 *                  from inside the compile loop where we can skip a couple of
 *                  things.
 */
DECL_FORCE_INLINE(void) iemThreadedCompileInitDecoder(PVMCPUCC pVCpu, bool const fReInit)
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
    }
    else
    {
        Assert(pVCpu->iem.s.cActiveMappings == 0);
        Assert(pVCpu->iem.s.rcPassUp        == VINF_SUCCESS);
        Assert(pVCpu->iem.s.fEndTb          == false);
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
// // // // // // // // // // figure this out // // // //
    pVCpu->iem.s.pbInstrBuf         = NULL;
    pVCpu->iem.s.offInstrNextByte   = 0;
    pVCpu->iem.s.offCurInstrStart   = 0;
#ifdef VBOX_STRICT
    pVCpu->iem.s.GCPhysInstrBuf     = NIL_RTGCPHYS;
    pVCpu->iem.s.cbInstrBuf         = UINT16_MAX;
    pVCpu->iem.s.cbInstrBufTotal    = UINT16_MAX;
    pVCpu->iem.s.uInstrBufPc        = UINT64_C(0xc0ffc0ffcff0c0ff);
#endif
// // // // // // // // // // // // // // // // // // //
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
 * @param   fExtraFlags Extra translation block flags: IEMTB_F_TYPE_THREADED and
 *                      maybe IEMTB_F_RIP_CHECKS.
 */
static VBOXSTRICTRC iemThreadedCompile(PVMCC pVM, PVMCPUCC pVCpu, RTGCPHYS GCPhysPc, uint32_t fExtraFlags)
{
    /*
     * Allocate a new translation block.
     */
    if (!(fExtraFlags & IEMTB_F_RIP_CHECKS))
    { /* likely */  }
    else if (  !IEM_IS_64BIT_CODE(pVCpu)
             ? pVCpu->cpum.GstCtx.eip <= pVCpu->cpum.GstCtx.cs.u32Limit
             : IEM_IS_CANONICAL(pVCpu->cpum.GstCtx.rip))
    { /* likely */ }
    else
        return IEMExecOne(pVCpu);
    fExtraFlags |= IEMTB_F_STATE_COMPILING;

    PIEMTB pTb = iemThreadedTbAlloc(pVM, pVCpu, GCPhysPc, fExtraFlags);
    AssertReturn(pTb, VERR_IEM_TB_ALLOC_FAILED);

    /* Set the current TB so iemThreadedCompileLongJumped and the CIMPL
       functions may get at it. */
    pVCpu->iem.s.pCurTbR3 = pTb;

    /*
     * Now for the recomplication. (This mimicks IEMExecLots in many ways.)
     */
    iemThreadedCompileInitDecoder(pVCpu, false /*fReInit*/);
    iemThreadedCompileInitOpcodeFetching(pVCpu);
    VBOXSTRICTRC rcStrict;
    for (;;)
    {
        /* Process the next instruction. */
        uint8_t b; IEM_OPCODE_GET_FIRST_U8(&b);
        uint16_t const cCallsPrev = pTb->Thrd.cCalls;
        rcStrict = FNIEMOP_CALL(g_apfnIemThreadedRecompilerOneByteMap[b]);
        if (   rcStrict == VINF_SUCCESS
            && !pVCpu->iem.s.fEndTb)
        {
            Assert(pTb->Thrd.cCalls > cCallsPrev);
            Assert(cCallsPrev - pTb->Thrd.cCalls < 5);

        }
        else if (pTb->Thrd.cCalls > 0)
        {
            break;
        }
        else
        {
            pVCpu->iem.s.pCurTbR3 = NULL;
            iemThreadedTbFree(pVM, pVCpu, pTb);
            return rcStrict;
        }

        /* Still space in the TB? */
        if (pTb->Thrd.cCalls + 5 < pTb->Thrd.cAllocated)
            iemThreadedCompileInitDecoder(pVCpu, true /*fReInit*/);
        else
            break;
        iemThreadedCompileReInitOpcodeFetching(pVCpu);
    }

    /*
     * Complete the TB and link it.
     */

#ifdef IEM_COMPILE_ONLY_MODE
    /*
     * Execute the translation block.
     */
#endif

    return rcStrict;
}


/**
 * Executes a translation block.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu   The cross context virtual CPU structure of the calling
 *                  thread.
 * @param   pTb     The translation block to execute.
 */
static VBOXSTRICTRC iemThreadedTbExec(PVMCPUCC pVCpu, PIEMTB pTb)
{
    /* Set the current TB so CIMPL function may get at it. */
    pVCpu->iem.s.pCurTbR3 = pTb;

    /* The execution loop. */
    PCIEMTHRDEDCALLENTRY pCallEntry = pTb->Thrd.paCalls;
    uint32_t             cCallsLeft = pTb->Thrd.cCalls;
    while (cCallsLeft-- > 0)
    {
        VBOXSTRICTRC const rcStrict = g_apfnIemThreadedFunctions[pCallEntry->enmFunction](pVCpu,
                                                                                          pCallEntry->auParams[0],
                                                                                          pCallEntry->auParams[1],
                                                                                          pCallEntry->auParams[2]);
        if (RT_LIKELY(rcStrict == VINF_SUCCESS))
            pCallEntry++;
        else
        {
            pVCpu->iem.s.pCurTbR3 = NULL;

            /* Some status codes are just to get us out of this loop and
               continue in a different translation block. */
            if (rcStrict == VINF_IEM_REEXEC_MODE_CHANGED)
                return VINF_SUCCESS;
            return rcStrict;
        }
    }

    pVCpu->iem.s.pCurTbR3 = NULL;
    return VINF_SUCCESS;
}


/**
 * This is called when the PC doesn't match the current pbInstrBuf.
 */
static uint64_t iemGetPcWithPhysAndCodeMissed(PVMCPUCC pVCpu, uint64_t const uPc, PRTGCPHYS pPhys)
{
    /** @todo see iemOpcodeFetchBytesJmp */
    pVCpu->iem.s.pbInstrBuf = NULL;

    pVCpu->iem.s.offInstrNextByte = 0;
    pVCpu->iem.s.offCurInstrStart = 0;
    pVCpu->iem.s.cbInstrBuf       = 0;
    pVCpu->iem.s.cbInstrBufTotal  = 0;

    RT_NOREF(uPc, pPhys);
    return 0;
}


/** @todo need private inline decl for throw/nothrow matching IEM_WITH_SETJMP? */
DECL_FORCE_INLINE_THROW(uint64_t) iemGetPcWithPhysAndCode(PVMCPUCC pVCpu, PRTGCPHYS pPhys)
{
    /* Set uCurTbStartPc to RIP and calc the effective PC. */
    uint64_t uPc = pVCpu->cpum.GstCtx.rip;
    pVCpu->iem.s.uCurTbStartPc = uPc;
    Assert(pVCpu->cpum.GstCtx.cs.u64Base == 0 || !IEM_IS_64BIT_CODE(pVCpu));
    uPc += pVCpu->cpum.GstCtx.cs.u64Base;

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

            *pPhys = pVCpu->iem.s.GCPhysInstrBuf + off;
            return uPc;
        }
    }
    return iemGetPcWithPhysAndCodeMissed(pVCpu, uPc, pPhys);
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
    /*
     * Return IEMTB_F_RIP_CHECKS if the current PC is invalid or if it is
     * likely to go invalid before the end of the translation block.
     */
    if (IEM_IS_64BIT_CODE(pVCpu))
    {
        if (RT_LIKELY(   IEM_IS_CANONICAL(pVCpu->cpum.GstCtx.rip)
                      && IEM_IS_CANONICAL(pVCpu->cpum.GstCtx.rip + 256)))
            return IEMTB_F_TYPE_THREADED;
    }
    else
    {
        if (RT_LIKELY(   pVCpu->cpum.GstCtx.eip < pVCpu->cpum.GstCtx.cs.u32Limit
                      && (uint64_t)256 + pVCpu->cpum.GstCtx.eip < pVCpu->cpum.GstCtx.cs.u32Limit))
            return IEMTB_F_TYPE_THREADED;
    }
    return IEMTB_F_RIP_CHECKS | IEMTB_F_TYPE_THREADED;
}


VMMDECL(VBOXSTRICTRC) IEMExecRecompilerThreaded(PVMCC pVM, PVMCPUCC pVCpu)
{
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
#ifdef IEM_WITH_SETJMP
        IEM_TRY_SETJMP(pVCpu, rcStrict)
#endif
        {
            for (;;)
            {
                /* Translate PC to physical address, we'll need this for both lookup and compilation. */
                RTGCPHYS       GCPhysPc;
                uint64_t const uPc         = iemGetPcWithPhysAndCode(pVCpu, &GCPhysPc);
                uint32_t const fExtraFlags = iemGetTbFlagsForCurrentPc(pVCpu);

                pTb = iemThreadedTbLookup(pVM, pVCpu, GCPhysPc, uPc, fExtraFlags);
                if (pTb)
                    rcStrict = iemThreadedTbExec(pVCpu, pTb);
                else
                    rcStrict = iemThreadedCompile(pVM, pVCpu, GCPhysPc, fExtraFlags);
                if (rcStrict == VINF_SUCCESS)
                { /* likely */ }
                else
                    return rcStrict;
            }
        }
#ifdef IEM_WITH_SETJMP
        IEM_CATCH_LONGJMP_BEGIN(pVCpu, rcStrict);
        {
            pVCpu->iem.s.cLongJumps++;
            if (pTb)
                return rcStrict;
            return iemThreadedCompileLongJumped(pVM, pVCpu, rcStrict);
        }
        IEM_CATCH_LONGJMP_END(pVCpu);
#endif
    }
}

