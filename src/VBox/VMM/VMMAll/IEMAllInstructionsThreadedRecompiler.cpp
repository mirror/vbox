/* $Id$ */
/** @file
 * IEM - Instruction Decoding and Emulation.
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
#include <iprt/string.h>
#include <iprt/x86.h>

#ifndef TST_IEM_CHECK_MC
# include "IEMInline.h"
# include "IEMOpHlp.h"
# include "IEMMc.h"
#endif

#include "IEMThreadedFunctions.h"


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

/** @name IEMTB_F_XXX - Translation block flags.
 * @{ */
#define IEMTB_F_MODE_MASK               UINT32_C(0x00000007)
#define IEMTB_F_MODE_X86_16BIT          UINT32_C(0x00000001)
#define IEMTB_F_MODE_X86_32BIT          UINT32_C(0x00000002)
#define IEMTB_F_MODE_X86_32BIT_FLAT     UINT32_C(0x00000003)
#define IEMTB_F_MODE_X86_64BIT          UINT32_C(0x00000004)

#define IEMTB_F_COMPILING   RT_BIT_32(0)
#define IEMTB_F_NATIVE      RT_BIT_32(1)
/** @} */

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
    uint64_t            uPc;
    uint32_t            fFlags;
    union
    {
        struct
        {
            /** The CS base. */
            uint32_t uCsBase;
            /** The CS limit (UINT32_MAX for 64-bit code). */
            uint32_t uCsLimit;
            /** The CS selector value. */
            uint16_t CS;
            /**< Relevant X86DESCATTR_XXX bits. */
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
            uint32_t            cCalls;
            /** Number of calls allocated. */
            uint32_t            cAllocated;
            /** The call sequence table. */
            PIEMTHRDEDCALLENTRY paCalls;
        } Thrd;
    };


} IEMTB;


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
#undef IEM_MC_DEFER_TO_CIMPL_0
#define IEM_MC_DEFER_TO_CIMPL_0(a_pfnCImpl) iemThreadedRecompilerMcDeferToCImpl0(pVCpu, a_pfnCImpl)

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
 * Real code.
 */

static VBOXSTRICTRC iemThreadedCompile(PVMCCV pVM, PVMCPUCC pVCpu)
{
    RT_NOREF(pVM, pVCpu, pTb);
    return VERR_NOT_IMPLEMENTED;
}


static VBOXSTRICTRC iemThreadedCompileLongJumped(PVMCCV pVM, PVMCPUCC pVCpu, VBOXSTRICTRC rcStrict)
{
    RT_NOREF(pVM, pVCpu);
    return rcStrict;
}


static PIEMTB iemThreadedTbLookup(PVMCCV pVM, PVMCPUCC pVCpu)
{
    RT_NOREF(pVM, pVCpu);
    return NULL;
}


static VBOXSTRICTRC iemThreadedTbExec(PVMCCV pVM, PVMCPUCC pVCpu, PIEMTB pTb)
{
    RT_NOREF(pVM, pVCpu, pTb);
    return VERR_NOT_IMPLEMENTED;
}


VMMDECL(VBOXSTRICTRC) IEMExecRecompilerThreaded(PVMCC pVM, PVMCPUCC pVCpu)
{
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
                pTb = iemThreadedTbLookup(pVM, pVCpu);
                if (pTb)
                    rcStrict = iemThreadedTbExec(pVM, pVCpu, pTb);
                else
                    rcStrict = iemThreadedCompile(pVM, pVCpu);
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

