/* $Id$ */
/** @file
 * EM - Execution Monitor(/Manager) - All contexts
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
#define LOG_GROUP LOG_GROUP_EM
#include <VBox/em.h>
#include <VBox/mm.h>
#include <VBox/selm.h>
#include <VBox/patm.h>
#include <VBox/csam.h>
#include <VBox/pgm.h>
#include <VBox/iom.h>
#include <VBox/stam.h>
#include "EMInternal.h"
#include <VBox/vm.h>
#include <VBox/vmm.h>
#include <VBox/hwaccm.h>
#include <VBox/tm.h>
#include <VBox/pdmapi.h>

#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @def EM_ASSERT_FAULT_RETURN
 * Safety check.
 *
 * Could in theory misfire on a cross page boundary access...
 *
 * Currently disabled because the CSAM (+ PATM) patch monitoring occasionally
 * turns up an alias page instead of the original faulting one and annoying the
 * heck out of anyone running a debug build. See @bugref{2609} and @bugref{1931}.
 */
#if 0
# define EM_ASSERT_FAULT_RETURN(expr, rc) AssertReturn(expr, rc)
#else
# define EM_ASSERT_FAULT_RETURN(expr, rc) do { } while (0)
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
DECLINLINE(int) emInterpretInstructionCPU(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize);



/**
 * Get the current execution manager status.
 *
 * @returns Current status.
 */
VMMDECL(EMSTATE) EMGetState(PVM pVM)
{
    return pVM->em.s.enmState;
}

#ifndef IN_RC

/**
 * Read callback for disassembly function; supports reading bytes that cross a page boundary
 *
 * @returns VBox status code.
 * @param   pSrc        GC source pointer
 * @param   pDest       HC destination pointer
 * @param   cb          Number of bytes to read
 * @param   dwUserdata  Callback specific user data (pCpu)
 *
 */
DECLCALLBACK(int) EMReadBytes(RTUINTPTR pSrc, uint8_t *pDest, unsigned cb, void *pvUserdata)
{
    DISCPUSTATE  *pCpu     = (DISCPUSTATE *)pvUserdata;
    PVM           pVM      = (PVM)pCpu->apvUserData[0];
# ifdef IN_RING0
    int rc = PGMPhysSimpleReadGCPtr(pVM, pDest, pSrc, cb);
    AssertMsgRC(rc, ("PGMPhysSimpleReadGCPtr failed for pSrc=%RGv cb=%x\n", pSrc, cb));
# else /* IN_RING3 */
    if (!PATMIsPatchGCAddr(pVM, pSrc))
    {
        int rc = PGMPhysSimpleReadGCPtr(pVM, pDest, pSrc, cb);
        AssertRC(rc);
    }
    else
    {
        for (uint32_t i = 0; i < cb; i++)
        {
            uint8_t opcode;
            if (RT_SUCCESS(PATMR3QueryOpcode(pVM, (RTGCPTR)pSrc + i, &opcode)))
            {
                *(pDest+i) = opcode;
            }
        }
    }
# endif /* IN_RING3 */
    return VINF_SUCCESS;
}

DECLINLINE(int) emDisCoreOne(PVM pVM, DISCPUSTATE *pCpu, RTGCUINTPTR InstrGC, uint32_t *pOpsize)
{
    return DISCoreOneEx(InstrGC, pCpu->mode, EMReadBytes, pVM, pCpu,  pOpsize);
}

#else /* IN_RC */

DECLINLINE(int) emDisCoreOne(PVM pVM, DISCPUSTATE *pCpu, RTGCUINTPTR InstrGC, uint32_t *pOpsize)
{
    return DISCoreOne(pCpu, InstrGC, pOpsize);
}

#endif /* IN_RC */


/**
 * Disassembles one instruction.
 *
 * @returns VBox status code, see SELMToFlatEx and EMInterpretDisasOneEx for
 *          details.
 * @retval  VERR_INTERNAL_ERROR on DISCoreOneEx failure.
 *
 * @param   pVM             The VM handle.
 * @param   pCtxCore        The context core (used for both the mode and instruction).
 * @param   pCpu            Where to return the parsed instruction info.
 * @param   pcbInstr        Where to return the instruction size. (optional)
 */
VMMDECL(int) EMInterpretDisasOne(PVM pVM, PCCPUMCTXCORE pCtxCore, PDISCPUSTATE pCpu, unsigned *pcbInstr)
{
    RTGCPTR GCPtrInstr;
    int rc = SELMToFlatEx(pVM, DIS_SELREG_CS, pCtxCore, pCtxCore->rip, 0, &GCPtrInstr);
    if (RT_FAILURE(rc))
    {
        Log(("EMInterpretDisasOne: Failed to convert %RTsel:%RGv (cpl=%d) - rc=%Rrc !!\n",
             pCtxCore->cs, (RTGCPTR)pCtxCore->rip, pCtxCore->ss & X86_SEL_RPL, rc));
        return rc;
    }
    return EMInterpretDisasOneEx(pVM, (RTGCUINTPTR)GCPtrInstr, pCtxCore, pCpu, pcbInstr);
}


/**
 * Disassembles one instruction.
 *
 * This is used by internally by the interpreter and by trap/access handlers.
 *
 * @returns VBox status code.
 * @retval  VERR_INTERNAL_ERROR on DISCoreOneEx failure.
 *
 * @param   pVM             The VM handle.
 * @param   GCPtrInstr      The flat address of the instruction.
 * @param   pCtxCore        The context core (used to determine the cpu mode).
 * @param   pCpu            Where to return the parsed instruction info.
 * @param   pcbInstr        Where to return the instruction size. (optional)
 */
VMMDECL(int) EMInterpretDisasOneEx(PVM pVM, RTGCUINTPTR GCPtrInstr, PCCPUMCTXCORE pCtxCore, PDISCPUSTATE pCpu, unsigned *pcbInstr)
{
    int rc = DISCoreOneEx(GCPtrInstr, SELMGetCpuModeFromSelector(pVM, pCtxCore->eflags, pCtxCore->cs, (PCPUMSELREGHID)&pCtxCore->csHid),
#ifdef IN_RC
                          NULL, NULL,
#else
                          EMReadBytes, pVM,
#endif
                          pCpu, pcbInstr);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;
    AssertMsgFailed(("DISCoreOne failed to GCPtrInstr=%RGv rc=%Rrc\n", GCPtrInstr, rc));
    return VERR_INTERNAL_ERROR;
}


/**
 * Interprets the current instruction.
 *
 * @returns VBox status code.
 * @retval  VINF_*                  Scheduling instructions.
 * @retval  VERR_EM_INTERPRETER     Something we can't cope with.
 * @retval  VERR_*                  Fatal errors.
 *
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 *                      Updates the EIP if an instruction was executed successfully.
 * @param   pvFault     The fault address (CR2).
 * @param   pcbSize     Size of the write (if applicable).
 *
 * @remark  Invalid opcode exceptions have a higher priority than GP (see Intel
 *          Architecture System Developers Manual, Vol 3, 5.5) so we don't need
 *          to worry about e.g. invalid modrm combinations (!)
 */
VMMDECL(int) EMInterpretInstruction(PVM pVM, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    RTGCPTR pbCode;

    LogFlow(("EMInterpretInstruction %RGv fault %RGv\n", (RTGCPTR)pRegFrame->rip, pvFault));
    int rc = SELMToFlatEx(pVM, DIS_SELREG_CS, pRegFrame, pRegFrame->rip, 0, &pbCode);
    if (RT_SUCCESS(rc))
    {
        uint32_t    cbOp;
        DISCPUSTATE Cpu;
        Cpu.mode = SELMGetCpuModeFromSelector(pVM, pRegFrame->eflags, pRegFrame->cs, &pRegFrame->csHid);
        rc = emDisCoreOne(pVM, &Cpu, (RTGCUINTPTR)pbCode, &cbOp);
        if (RT_SUCCESS(rc))
        {
            Assert(cbOp == Cpu.opsize);
            rc = EMInterpretInstructionCPU(pVM, &Cpu, pRegFrame, pvFault, pcbSize);
            if (RT_SUCCESS(rc))
            {
                pRegFrame->rip += cbOp; /* Move on to the next instruction. */
            }
            return rc;
        }
    }
    return VERR_EM_INTERPRETER;
}


/**
 * Interprets the current instruction using the supplied DISCPUSTATE structure.
 *
 * EIP is *NOT* updated!
 *
 * @returns VBox status code.
 * @retval  VINF_*                  Scheduling instructions. When these are returned, it
 *                                  starts to get a bit tricky to know whether code was
 *                                  executed or not... We'll address this when it becomes a problem.
 * @retval  VERR_EM_INTERPRETER     Something we can't cope with.
 * @retval  VERR_*                  Fatal errors.
 *
 * @param   pVM         The VM handle.
 * @param   pCpu        The disassembler cpu state for the instruction to be interpreted.
 * @param   pRegFrame   The register frame. EIP is *NOT* changed!
 * @param   pvFault     The fault address (CR2).
 * @param   pcbSize     Size of the write (if applicable).
 *
 * @remark  Invalid opcode exceptions have a higher priority than GP (see Intel
 *          Architecture System Developers Manual, Vol 3, 5.5) so we don't need
 *          to worry about e.g. invalid modrm combinations (!)
 *
 * @todo    At this time we do NOT check if the instruction overwrites vital information.
 *          Make sure this can't happen!! (will add some assertions/checks later)
 */
VMMDECL(int) EMInterpretInstructionCPU(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    STAM_PROFILE_START(&pVM->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Emulate), a);
    int rc = emInterpretInstructionCPU(pVM, pCpu, pRegFrame, pvFault, pcbSize);
    STAM_PROFILE_STOP(&pVM->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Emulate), a);
    if (RT_SUCCESS(rc))
        STAM_COUNTER_INC(&pVM->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,InterpretSucceeded));
    else
        STAM_COUNTER_INC(&pVM->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,InterpretFailed));
    return rc;
}


/**
 * Interpret a port I/O instruction.
 *
 * @returns VBox status code suitable for scheduling.
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The context core. This will be updated on successful return.
 * @param   pCpu        The instruction to interpret.
 * @param   cbOp        The size of the instruction.
 * @remark  This may raise exceptions.
 */
VMMDECL(int) EMInterpretPortIO(PVM pVM, PCPUMCTXCORE pCtxCore, PDISCPUSTATE pCpu, uint32_t cbOp)
{
    /*
     * Hand it on to IOM.
     */
#ifdef IN_RC
    int rc = IOMGCIOPortHandler(pVM, pCtxCore, pCpu);
    if (IOM_SUCCESS(rc))
        pCtxCore->rip += cbOp;
    return rc;
#else
    AssertReleaseMsgFailed(("not implemented\n"));
    return VERR_NOT_IMPLEMENTED;
#endif
}


DECLINLINE(int) emRamRead(PVM pVM, PCPUMCTXCORE pCtxCore, void *pvDst, RTGCPTR GCPtrSrc, uint32_t cb)
{
#ifdef IN_RC
    int rc = MMGCRamRead(pVM, pvDst, (void *)GCPtrSrc, cb);
    if (RT_LIKELY(rc != VERR_ACCESS_DENIED))
        return rc;
    /*
     * The page pool cache may end up here in some cases because it
     * flushed one of the shadow mappings used by the trapping
     * instruction and it either flushed the TLB or the CPU reused it.
     */
#endif
    return PGMPhysInterpretedReadNoHandlers(pVM, pCtxCore, pvDst, GCPtrSrc, cb, /*fMayTrap*/ false);
}


DECLINLINE(int) emRamWrite(PVM pVM, PCPUMCTXCORE pCtxCore, RTGCPTR GCPtrDst, const void *pvSrc, uint32_t cb)
{
#ifdef IN_RC
    int rc = MMGCRamWrite(pVM, (void *)(uintptr_t)GCPtrDst, (void *)pvSrc, cb);
    if (RT_LIKELY(rc != VERR_ACCESS_DENIED))
        return rc;
    /*
     * The page pool cache may end up here in some cases because it
     * flushed one of the shadow mappings used by the trapping
     * instruction and it either flushed the TLB or the CPU reused it.
     * We want to play safe here, verifying that we've got write
     * access doesn't cost us much (see PGMPhysGCPtr2GCPhys()).
     */
#endif
    return PGMPhysInterpretedWriteNoHandlers(pVM, pCtxCore, GCPtrDst, pvSrc, cb, /*fMayTrap*/ false);
}


/** Convert sel:addr to a flat GC address. */
DECLINLINE(RTGCPTR) emConvertToFlatAddr(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu, POP_PARAMETER pParam, RTGCPTR pvAddr)
{
    DIS_SELREG enmPrefixSeg = DISDetectSegReg(pCpu, pParam);
    return SELMToFlat(pVM, enmPrefixSeg, pRegFrame, pvAddr);
}


#if defined(VBOX_STRICT) || defined(LOG_ENABLED)
/**
 * Get the mnemonic for the disassembled instruction.
 *
 * GC/R0 doesn't include the strings in the DIS tables because
 * of limited space.
 */
static const char *emGetMnemonic(PDISCPUSTATE pCpu)
{
    switch (pCpu->pCurInstr->opcode)
    {
        case OP_XCHG:       return "Xchg";
        case OP_DEC:        return "Dec";
        case OP_INC:        return "Inc";
        case OP_POP:        return "Pop";
        case OP_OR:         return "Or";
        case OP_AND:        return "And";
        case OP_MOV:        return "Mov";
        case OP_INVLPG:     return "InvlPg";
        case OP_CPUID:      return "CpuId";
        case OP_MOV_CR:     return "MovCRx";
        case OP_MOV_DR:     return "MovDRx";
        case OP_LLDT:       return "LLdt";
        case OP_LGDT:       return "LGdt";
        case OP_LIDT:       return "LGdt";
        case OP_CLTS:       return "Clts";
        case OP_MONITOR:    return "Monitor";
        case OP_MWAIT:      return "MWait";
        case OP_RDMSR:      return "Rdmsr";
        case OP_WRMSR:      return "Wrmsr";
        case OP_ADD:        return "Add";
        case OP_ADC:        return "Adc";
        case OP_SUB:        return "Sub";
        case OP_SBB:        return "Sbb";
        case OP_RDTSC:      return "Rdtsc";
        case OP_STI:        return "Sti";
        case OP_CLI:        return "Cli";
        case OP_XADD:       return "XAdd";
        case OP_HLT:        return "Hlt";
        case OP_IRET:       return "Iret";
        case OP_MOVNTPS:    return "MovNTPS";
        case OP_STOSWD:     return "StosWD";
        case OP_WBINVD:     return "WbInvd";
        case OP_XOR:        return "Xor";
        case OP_BTR:        return "Btr";
        case OP_BTS:        return "Bts";
        case OP_BTC:        return "Btc";
        case OP_LMSW:       return "Lmsw";
        case OP_SMSW:       return "Smsw";
        case OP_CMPXCHG:    return pCpu->prefix & PREFIX_LOCK ? "Lock CmpXchg"   : "CmpXchg";
        case OP_CMPXCHG8B:  return pCpu->prefix & PREFIX_LOCK ? "Lock CmpXchg8b" : "CmpXchg8b";

        default:
            Log(("Unknown opcode %d\n", pCpu->pCurInstr->opcode));
            return "???";
    }
}
#endif /* VBOX_STRICT || LOG_ENABLED */


/**
 * XCHG instruction emulation.
 */
static int emInterpretXchg(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    OP_PARAMVAL param1, param2;

    /* Source to make DISQueryParamVal read the register value - ugly hack */
    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param2, &param2, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

#ifdef IN_RC
    if (TRPMHasTrap(pVM))
    {
        if (TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW)
        {
#endif
            RTGCPTR pParam1 = 0, pParam2 = 0;
            uint64_t valpar1, valpar2;

            AssertReturn(pCpu->param1.size == pCpu->param2.size, VERR_EM_INTERPRETER);
            switch(param1.type)
            {
            case PARMTYPE_IMMEDIATE: /* register type is translated to this one too */
                valpar1 = param1.val.val64;
                break;

            case PARMTYPE_ADDRESS:
                pParam1 = (RTGCPTR)param1.val.val64;
                pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, pParam1);
                EM_ASSERT_FAULT_RETURN(pParam1 == pvFault, VERR_EM_INTERPRETER);
                rc = emRamRead(pVM, pRegFrame, &valpar1, pParam1, param1.size);
                if (RT_FAILURE(rc))
                {
                    AssertMsgFailed(("MMGCRamRead %RGv size=%d failed with %Rrc\n", pParam1, param1.size, rc));
                    return VERR_EM_INTERPRETER;
                }
                break;

            default:
                AssertFailed();
                return VERR_EM_INTERPRETER;
            }

            switch(param2.type)
            {
            case PARMTYPE_ADDRESS:
                pParam2 = (RTGCPTR)param2.val.val64;
                pParam2 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param2, pParam2);
                EM_ASSERT_FAULT_RETURN(pParam2 == pvFault, VERR_EM_INTERPRETER);
                rc = emRamRead(pVM, pRegFrame, &valpar2, pParam2, param2.size);
                if (RT_FAILURE(rc))
                {
                    AssertMsgFailed(("MMGCRamRead %RGv size=%d failed with %Rrc\n", pParam1, param1.size, rc));
                }
                break;

            case PARMTYPE_IMMEDIATE:
                valpar2 = param2.val.val64;
                break;

            default:
                AssertFailed();
                return VERR_EM_INTERPRETER;
            }

            /* Write value of parameter 2 to parameter 1 (reg or memory address) */
            if (pParam1 == 0)
            {
                Assert(param1.type == PARMTYPE_IMMEDIATE); /* register actually */
                switch(param1.size)
                {
                case 1: //special case for AH etc
                        rc = DISWriteReg8(pRegFrame, pCpu->param1.base.reg_gen,  (uint8_t )valpar2); break;
                case 2: rc = DISWriteReg16(pRegFrame, pCpu->param1.base.reg_gen, (uint16_t)valpar2); break;
                case 4: rc = DISWriteReg32(pRegFrame, pCpu->param1.base.reg_gen, (uint32_t)valpar2); break;
                case 8: rc = DISWriteReg64(pRegFrame, pCpu->param1.base.reg_gen, valpar2); break;
                default: AssertFailedReturn(VERR_EM_INTERPRETER);
                }
                if (RT_FAILURE(rc))
                    return VERR_EM_INTERPRETER;
            }
            else
            {
                rc = emRamWrite(pVM, pRegFrame, pParam1, &valpar2, param1.size);
                if (RT_FAILURE(rc))
                {
                    AssertMsgFailed(("emRamWrite %RGv size=%d failed with %Rrc\n", pParam1, param1.size, rc));
                    return VERR_EM_INTERPRETER;
                }
            }

            /* Write value of parameter 1 to parameter 2 (reg or memory address) */
            if (pParam2 == 0)
            {
                Assert(param2.type == PARMTYPE_IMMEDIATE); /* register actually */
                switch(param2.size)
                {
                case 1: //special case for AH etc
                        rc = DISWriteReg8(pRegFrame, pCpu->param2.base.reg_gen,  (uint8_t )valpar1);    break;
                case 2: rc = DISWriteReg16(pRegFrame, pCpu->param2.base.reg_gen, (uint16_t)valpar1);    break;
                case 4: rc = DISWriteReg32(pRegFrame, pCpu->param2.base.reg_gen, (uint32_t)valpar1);    break;
                case 8: rc = DISWriteReg64(pRegFrame, pCpu->param2.base.reg_gen, valpar1);              break;
                default: AssertFailedReturn(VERR_EM_INTERPRETER);
                }
                if (RT_FAILURE(rc))
                    return VERR_EM_INTERPRETER;
            }
            else
            {
                rc = emRamWrite(pVM, pRegFrame, pParam2, &valpar1, param2.size);
                if (RT_FAILURE(rc))
                {
                    AssertMsgFailed(("emRamWrite %RGv size=%d failed with %Rrc\n", pParam1, param1.size, rc));
                    return VERR_EM_INTERPRETER;
                }
            }

            *pcbSize = param2.size;
            return VINF_SUCCESS;
#ifdef IN_RC
        }
    }
#endif
    return VERR_EM_INTERPRETER;
}


/**
 * INC and DEC emulation.
 */
static int emInterpretIncDec(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize,
                             PFNEMULATEPARAM2 pfnEmulate)
{
    OP_PARAMVAL param1;

    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_DEST);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

#ifdef IN_RC
    if (TRPMHasTrap(pVM))
    {
        if (TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW)
        {
#endif
            RTGCPTR pParam1 = 0;
            uint64_t valpar1;

            if (param1.type == PARMTYPE_ADDRESS)
            {
                pParam1 = (RTGCPTR)param1.val.val64;
                pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, pParam1);
#ifdef IN_RC
                /* Safety check (in theory it could cross a page boundary and fault there though) */
                AssertReturn(pParam1 == pvFault, VERR_EM_INTERPRETER);
#endif
                rc = emRamRead(pVM, pRegFrame,  &valpar1, pParam1, param1.size);
                if (RT_FAILURE(rc))
                {
                    AssertMsgFailed(("emRamRead %RGv size=%d failed with %Rrc\n", pParam1, param1.size, rc));
                    return VERR_EM_INTERPRETER;
                }
            }
            else
            {
                AssertFailed();
                return VERR_EM_INTERPRETER;
            }

            uint32_t eflags;

            eflags = pfnEmulate(&valpar1, param1.size);

            /* Write result back */
            rc = emRamWrite(pVM, pRegFrame, pParam1, &valpar1, param1.size);
            if (RT_FAILURE(rc))
            {
                AssertMsgFailed(("emRamWrite %RGv size=%d failed with %Rrc\n", pParam1, param1.size, rc));
                return VERR_EM_INTERPRETER;
            }

            /* Update guest's eflags and finish. */
            pRegFrame->eflags.u32 = (pRegFrame->eflags.u32   & ~(X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF))
                                  | (eflags & (X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF));

            /* All done! */
            *pcbSize = param1.size;
            return VINF_SUCCESS;
#ifdef IN_RC
        }
    }
#endif
    return VERR_EM_INTERPRETER;
}


/**
 * POP Emulation.
 */
static int emInterpretPop(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    Assert(pCpu->mode != CPUMODE_64BIT);    /** @todo check */
    OP_PARAMVAL param1;
    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_DEST);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

#ifdef IN_RC
    if (TRPMHasTrap(pVM))
    {
        if (TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW)
        {
#endif
            RTGCPTR pParam1 = 0;
            uint32_t valpar1;
            RTGCPTR pStackVal;

            /* Read stack value first */
            if (SELMGetCpuModeFromSelector(pVM, pRegFrame->eflags, pRegFrame->ss, &pRegFrame->ssHid) == CPUMODE_16BIT)
                return VERR_EM_INTERPRETER; /* No legacy 16 bits stuff here, please. */

            /* Convert address; don't bother checking limits etc, as we only read here */
            pStackVal = SELMToFlat(pVM, DIS_SELREG_SS, pRegFrame, (RTGCPTR)pRegFrame->esp);
            if (pStackVal == 0)
                return VERR_EM_INTERPRETER;

            rc = emRamRead(pVM, pRegFrame, &valpar1, pStackVal, param1.size);
            if (RT_FAILURE(rc))
            {
                AssertMsgFailed(("emRamRead %RGv size=%d failed with %Rrc\n", pParam1, param1.size, rc));
                return VERR_EM_INTERPRETER;
            }

            if (param1.type == PARMTYPE_ADDRESS)
            {
                pParam1 = (RTGCPTR)param1.val.val64;

                /* pop [esp+xx] uses esp after the actual pop! */
                AssertCompile(USE_REG_ESP == USE_REG_SP);
                if (    (pCpu->param1.flags & USE_BASE)
                    &&  (pCpu->param1.flags & (USE_REG_GEN16|USE_REG_GEN32))
                    &&  pCpu->param1.base.reg_gen == USE_REG_ESP
                   )
                   pParam1 = (RTGCPTR)((RTGCUINTPTR)pParam1 + param1.size);

                pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, pParam1);
                EM_ASSERT_FAULT_RETURN(pParam1 == pvFault || (RTGCPTR)pRegFrame->esp == pvFault, VERR_EM_INTERPRETER);
                rc = emRamWrite(pVM, pRegFrame, pParam1, &valpar1, param1.size);
                if (RT_FAILURE(rc))
                {
                    AssertMsgFailed(("emRamWrite %RGv size=%d failed with %Rrc\n", pParam1, param1.size, rc));
                    return VERR_EM_INTERPRETER;
                }

                /* Update ESP as the last step */
                pRegFrame->esp += param1.size;
            }
            else
            {
#ifndef DEBUG_bird // annoying assertion.
                AssertFailed();
#endif
                return VERR_EM_INTERPRETER;
            }

            /* All done! */
            *pcbSize = param1.size;
            return VINF_SUCCESS;
#ifdef IN_RC
        }
    }
#endif
    return VERR_EM_INTERPRETER;
}


/**
 * XOR/OR/AND Emulation.
 */
static int emInterpretOrXorAnd(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize,
                               PFNEMULATEPARAM3 pfnEmulate)
{
    OP_PARAMVAL param1, param2;

    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_DEST);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param2, &param2, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

#ifdef IN_RC
    if (TRPMHasTrap(pVM))
    {
        if (TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW)
        {
#endif
            RTGCPTR  pParam1;
            uint64_t valpar1, valpar2;

            if (pCpu->param1.size != pCpu->param2.size)
            {
                if (pCpu->param1.size < pCpu->param2.size)
                {
                    AssertMsgFailed(("%s at %RGv parameter mismatch %d vs %d!!\n", emGetMnemonic(pCpu), (RTGCPTR)pRegFrame->rip, pCpu->param1.size, pCpu->param2.size)); /* should never happen! */
                    return VERR_EM_INTERPRETER;
                }
                /* Or %Ev, Ib -> just a hack to save some space; the data width of the 1st parameter determines the real width */
                pCpu->param2.size = pCpu->param1.size;
                param2.size     = param1.size;
            }

            /* The destination is always a virtual address */
            if (param1.type == PARMTYPE_ADDRESS)
            {
                pParam1 = (RTGCPTR)param1.val.val64;
                pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, pParam1);
                EM_ASSERT_FAULT_RETURN(pParam1 == pvFault, VERR_EM_INTERPRETER);
                rc = emRamRead(pVM, pRegFrame, &valpar1, pParam1, param1.size);
                if (RT_FAILURE(rc))
                {
                    AssertMsgFailed(("emRamRead %RGv size=%d failed with %Rrc\n", pParam1, param1.size, rc));
                    return VERR_EM_INTERPRETER;
                }
            }
            else
            {
                AssertFailed();
                return VERR_EM_INTERPRETER;
            }

            /* Register or immediate data */
            switch(param2.type)
            {
            case PARMTYPE_IMMEDIATE:    /* both immediate data and register (ugly) */
                valpar2 = param2.val.val64;
                break;

            default:
                AssertFailed();
                return VERR_EM_INTERPRETER;
            }

            LogFlow(("emInterpretOrXorAnd %s %RGv %RX64 - %RX64 size %d (%d)\n", emGetMnemonic(pCpu), pParam1, valpar1, valpar2, param2.size, param1.size));

            /* Data read, emulate instruction. */
            uint32_t eflags = pfnEmulate(&valpar1, valpar2, param2.size);

            LogFlow(("emInterpretOrXorAnd %s result %RX64\n", emGetMnemonic(pCpu), valpar1));

            /* Update guest's eflags and finish. */
            pRegFrame->eflags.u32 = (pRegFrame->eflags.u32 & ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF))
                                  | (eflags                &  (X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF));

            /* And write it back */
            rc = emRamWrite(pVM, pRegFrame, pParam1, &valpar1, param1.size);
            if (RT_SUCCESS(rc))
            {
                /* All done! */
                *pcbSize = param2.size;
                return VINF_SUCCESS;
            }
#ifdef IN_RC
        }
    }
#endif
    return VERR_EM_INTERPRETER;
}


/**
 * LOCK XOR/OR/AND Emulation.
 */
static int emInterpretLockOrXorAnd(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault,
                                   uint32_t *pcbSize, PFNEMULATELOCKPARAM3 pfnEmulate)
{
    void *pvParam1;
    OP_PARAMVAL param1, param2;

#if HC_ARCH_BITS == 32
    Assert(pCpu->param1.size <= 4);
#endif

    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_DEST);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param2, &param2, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    if (pCpu->param1.size != pCpu->param2.size)
    {
        AssertMsgReturn(pCpu->param1.size >= pCpu->param2.size, /* should never happen! */
                        ("%s at %RGv parameter mismatch %d vs %d!!\n", emGetMnemonic(pCpu), (RTGCPTR)pRegFrame->rip, pCpu->param1.size, pCpu->param2.size),
                        VERR_EM_INTERPRETER);

        /* Or %Ev, Ib -> just a hack to save some space; the data width of the 1st parameter determines the real width */
        pCpu->param2.size = pCpu->param1.size;
        param2.size       = param1.size;
    }

#ifdef IN_RC
    /* Safety check (in theory it could cross a page boundary and fault there though) */
    Assert(   TRPMHasTrap(pVM)
           && (TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW));
    EM_ASSERT_FAULT_RETURN(GCPtrPar1 == pvFault, VERR_EM_INTERPRETER);
#endif

    /* Register and immediate data == PARMTYPE_IMMEDIATE */
    AssertReturn(param2.type == PARMTYPE_IMMEDIATE, VERR_EM_INTERPRETER);
    RTGCUINTREG ValPar2 = param2.val.val64;

    /* The destination is always a virtual address */
    AssertReturn(param1.type == PARMTYPE_ADDRESS, VERR_EM_INTERPRETER);

    RTGCPTR GCPtrPar1 = param1.val.val64;
    GCPtrPar1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, GCPtrPar1);
#ifdef IN_RC
    pvParam1  = (void *)GCPtrPar1;
#else
    PGMPAGEMAPLOCK Lock;
    rc = PGMPhysGCPtr2CCPtr(pVM, GCPtrPar1, &pvParam1, &Lock);
    AssertRCReturn(rc, VERR_EM_INTERPRETER);
#endif

    /* Try emulate it with a one-shot #PF handler in place. (RC) */
    Log2(("%s %RGv imm%d=%RX64\n", emGetMnemonic(pCpu), GCPtrPar1, pCpu->param2.size*8, ValPar2));

    RTGCUINTREG32 eflags = 0;
#ifdef IN_RC
    MMGCRamRegisterTrapHandler(pVM);
#endif
    rc = pfnEmulate(pvParam1, ValPar2, pCpu->param2.size, &eflags);
#ifdef IN_RC
    MMGCRamDeregisterTrapHandler(pVM);
#else
    PGMPhysReleasePageMappingLock(pVM, &Lock);
#endif
    if (RT_FAILURE(rc))
    {
        Log(("%s %RGv imm%d=%RX64-> emulation failed due to page fault!\n", emGetMnemonic(pCpu), GCPtrPar1, pCpu->param2.size*8, ValPar2));
        return VERR_EM_INTERPRETER;
    }

    /* Update guest's eflags and finish. */
    pRegFrame->eflags.u32 = (pRegFrame->eflags.u32 & ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF))
                          | (eflags                &  (X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF));

    *pcbSize = param2.size;
    return VINF_SUCCESS;
}


/**
 * ADD, ADC & SUB Emulation.
 */
static int emInterpretAddSub(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize,
                             PFNEMULATEPARAM3 pfnEmulate)
{
    OP_PARAMVAL param1, param2;
    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_DEST);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param2, &param2, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

#ifdef IN_RC
    if (TRPMHasTrap(pVM))
    {
        if (TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW)
        {
#endif
            RTGCPTR  pParam1;
            uint64_t valpar1, valpar2;

            if (pCpu->param1.size != pCpu->param2.size)
            {
                if (pCpu->param1.size < pCpu->param2.size)
                {
                    AssertMsgFailed(("%s at %RGv parameter mismatch %d vs %d!!\n", emGetMnemonic(pCpu), (RTGCPTR)pRegFrame->rip, pCpu->param1.size, pCpu->param2.size)); /* should never happen! */
                    return VERR_EM_INTERPRETER;
                }
                /* Or %Ev, Ib -> just a hack to save some space; the data width of the 1st parameter determines the real width */
                pCpu->param2.size = pCpu->param1.size;
                param2.size     = param1.size;
            }

            /* The destination is always a virtual address */
            if (param1.type == PARMTYPE_ADDRESS)
            {
                pParam1 = (RTGCPTR)param1.val.val64;
                pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, pParam1);
                EM_ASSERT_FAULT_RETURN(pParam1 == pvFault, VERR_EM_INTERPRETER);
                rc = emRamRead(pVM, pRegFrame, &valpar1, pParam1, param1.size);
                if (RT_FAILURE(rc))
                {
                    AssertMsgFailed(("emRamRead %RGv size=%d failed with %Rrc\n", pParam1, param1.size, rc));
                    return VERR_EM_INTERPRETER;
                }
            }
            else
            {
#ifndef DEBUG_bird
                AssertFailed();
#endif
                return VERR_EM_INTERPRETER;
            }

            /* Register or immediate data */
            switch(param2.type)
            {
            case PARMTYPE_IMMEDIATE:    /* both immediate data and register (ugly) */
                valpar2 = param2.val.val64;
                break;

            default:
                AssertFailed();
                return VERR_EM_INTERPRETER;
            }

            /* Data read, emulate instruction. */
            uint32_t eflags = pfnEmulate(&valpar1, valpar2, param2.size);

            /* Update guest's eflags and finish. */
            pRegFrame->eflags.u32 = (pRegFrame->eflags.u32 & ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF))
                                  | (eflags                &  (X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF));

            /* And write it back */
            rc = emRamWrite(pVM, pRegFrame, pParam1, &valpar1, param1.size);
            if (RT_SUCCESS(rc))
            {
                /* All done! */
                *pcbSize = param2.size;
                return VINF_SUCCESS;
            }
#ifdef IN_RC
        }
    }
#endif
    return VERR_EM_INTERPRETER;
}


/**
 * ADC Emulation.
 */
static int emInterpretAdc(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    if (pRegFrame->eflags.Bits.u1CF)
        return emInterpretAddSub(pVM, pCpu, pRegFrame, pvFault, pcbSize, EMEmulateAdcWithCarrySet);
    else
        return emInterpretAddSub(pVM, pCpu, pRegFrame, pvFault, pcbSize, EMEmulateAdd);
}


/**
 * BTR/C/S Emulation.
 */
static int emInterpretBitTest(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize,
                              PFNEMULATEPARAM2UINT32 pfnEmulate)
{
    OP_PARAMVAL param1, param2;
    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_DEST);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param2, &param2, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

#ifdef IN_RC
    if (TRPMHasTrap(pVM))
    {
        if (TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW)
        {
#endif
            RTGCPTR  pParam1;
            uint64_t valpar1 = 0, valpar2;
            uint32_t eflags;

            /* The destination is always a virtual address */
            if (param1.type != PARMTYPE_ADDRESS)
                return VERR_EM_INTERPRETER;

            pParam1 = (RTGCPTR)param1.val.val64;
            pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, pParam1);

            /* Register or immediate data */
            switch(param2.type)
            {
            case PARMTYPE_IMMEDIATE:    /* both immediate data and register (ugly) */
                valpar2 = param2.val.val64;
                break;

            default:
                AssertFailed();
                return VERR_EM_INTERPRETER;
            }

            Log2(("emInterpret%s: pvFault=%RGv pParam1=%RGv val2=%x\n", emGetMnemonic(pCpu), pvFault, pParam1, valpar2));
            pParam1 = (RTGCPTR)((RTGCUINTPTR)pParam1 + valpar2/8);
            EM_ASSERT_FAULT_RETURN((RTGCPTR)((RTGCUINTPTR)pParam1 & ~3) == pvFault, VERR_EM_INTERPRETER);
            rc = emRamRead(pVM, pRegFrame, &valpar1, pParam1, 1);
            if (RT_FAILURE(rc))
            {
                AssertMsgFailed(("emRamRead %RGv size=%d failed with %Rrc\n", pParam1, param1.size, rc));
                return VERR_EM_INTERPRETER;
            }

            Log2(("emInterpretBtx: val=%x\n", valpar1));
            /* Data read, emulate bit test instruction. */
            eflags = pfnEmulate(&valpar1, valpar2 & 0x7);

            Log2(("emInterpretBtx: val=%x CF=%d\n", valpar1, !!(eflags & X86_EFL_CF)));

            /* Update guest's eflags and finish. */
            pRegFrame->eflags.u32 = (pRegFrame->eflags.u32 & ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF))
                                  | (eflags                &  (X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF));

            /* And write it back */
            rc = emRamWrite(pVM, pRegFrame, pParam1, &valpar1, 1);
            if (RT_SUCCESS(rc))
            {
                /* All done! */
                *pcbSize = 1;
                return VINF_SUCCESS;
            }
#ifdef IN_RC
        }
    }
#endif
    return VERR_EM_INTERPRETER;
}


/**
 * LOCK BTR/C/S Emulation.
 */
static int emInterpretLockBitTest(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault,
                                  uint32_t *pcbSize, PFNEMULATELOCKPARAM2 pfnEmulate)
{
    void *pvParam1;

    OP_PARAMVAL param1, param2;
    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_DEST);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param2, &param2, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    /* The destination is always a virtual address */
    if (param1.type != PARMTYPE_ADDRESS)
        return VERR_EM_INTERPRETER;

    /* Register and immediate data == PARMTYPE_IMMEDIATE */
    AssertReturn(param2.type == PARMTYPE_IMMEDIATE, VERR_EM_INTERPRETER);
    uint64_t ValPar2 = param2.val.val64;

    /* Adjust the parameters so what we're dealing with is a bit within the byte pointed to. */
    RTGCPTR GCPtrPar1 = param1.val.val64;
    GCPtrPar1 = (GCPtrPar1 + ValPar2 / 8);
    ValPar2 &= 7;

    GCPtrPar1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, GCPtrPar1);
#ifdef IN_RC
    Assert(TRPMHasTrap(pVM));
    EM_ASSERT_FAULT_RETURN((RTGCPTR)((RTGCUINTPTR)GCPtrPar1 & ~(RTGCUINTPTR)3) == pvFault, VERR_EM_INTERPRETER);
#endif

#ifdef IN_RC
    pvParam1  = (void *)GCPtrPar1;
#else
    PGMPAGEMAPLOCK Lock;
    rc = PGMPhysGCPtr2CCPtr(pVM, GCPtrPar1, &pvParam1, &Lock);
    AssertRCReturn(rc, VERR_EM_INTERPRETER);
#endif

    Log2(("emInterpretLockBitTest %s: pvFault=%RGv GCPtrPar1=%RGv imm=%RX64\n", emGetMnemonic(pCpu), pvFault, GCPtrPar1, ValPar2));

    /* Try emulate it with a one-shot #PF handler in place. (RC) */
    RTGCUINTREG32 eflags = 0;
#ifdef IN_RC
    MMGCRamRegisterTrapHandler(pVM);
#endif
    rc = pfnEmulate(pvParam1, ValPar2, &eflags);
#ifdef IN_RC
    MMGCRamDeregisterTrapHandler(pVM);
#else
    PGMPhysReleasePageMappingLock(pVM, &Lock);
#endif
    if (RT_FAILURE(rc))
    {
        Log(("emInterpretLockBitTest %s: %RGv imm%d=%RX64 -> emulation failed due to page fault!\n",
             emGetMnemonic(pCpu), GCPtrPar1, pCpu->param2.size*8, ValPar2));
        return VERR_EM_INTERPRETER;
    }

    Log2(("emInterpretLockBitTest %s: GCPtrPar1=%RGv imm=%RX64 CF=%d\n", emGetMnemonic(pCpu), GCPtrPar1, ValPar2, !!(eflags & X86_EFL_CF)));

    /* Update guest's eflags and finish. */
    pRegFrame->eflags.u32 = (pRegFrame->eflags.u32 & ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF))
                          | (eflags                &  (X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF));

    *pcbSize = 1;
    return VINF_SUCCESS;
}


/**
 * MOV emulation.
 */
static int emInterpretMov(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    OP_PARAMVAL param1, param2;
    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_DEST);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param2, &param2, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

#ifdef IN_RC
    if (TRPMHasTrap(pVM))
    {
        if (TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW)
        {
#else
        /** @todo Make this the default and don't rely on TRPM information. */
        if (param1.type == PARMTYPE_ADDRESS)
        {
#endif
            RTGCPTR pDest;
            uint64_t val64;

            switch(param1.type)
            {
            case PARMTYPE_IMMEDIATE:
                if(!(param1.flags & (PARAM_VAL32|PARAM_VAL64)))
                    return VERR_EM_INTERPRETER;
                /* fallthru */

            case PARMTYPE_ADDRESS:
                pDest = (RTGCPTR)param1.val.val64;
                pDest = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, pDest);
                break;

            default:
                AssertFailed();
                return VERR_EM_INTERPRETER;
            }

            switch(param2.type)
            {
            case PARMTYPE_IMMEDIATE: /* register type is translated to this one too */
                val64 = param2.val.val64;
                break;

            default:
                Log(("emInterpretMov: unexpected type=%d rip=%RGv\n", param2.type, (RTGCPTR)pRegFrame->rip));
                return VERR_EM_INTERPRETER;
            }
#ifdef LOG_ENABLED
            if (pCpu->mode == CPUMODE_64BIT)
                LogFlow(("EMInterpretInstruction at %RGv: OP_MOV %RGv <- %RX64 (%d) &val64=%RHv\n", (RTGCPTR)pRegFrame->rip, pDest, val64, param2.size, &val64));
            else
                LogFlow(("EMInterpretInstruction at %08RX64: OP_MOV %RGv <- %08X  (%d) &val64=%RHv\n", pRegFrame->rip, pDest, (uint32_t)val64, param2.size, &val64));
#endif

            Assert(param2.size <= 8 && param2.size > 0);
            EM_ASSERT_FAULT_RETURN(pDest == pvFault, VERR_EM_INTERPRETER);
            rc = emRamWrite(pVM, pRegFrame, pDest, &val64, param2.size);
            if (RT_FAILURE(rc))
                return VERR_EM_INTERPRETER;

            *pcbSize = param2.size;
        }
        else
        { /* read fault */
            RTGCPTR pSrc;
            uint64_t val64;

            /* Source */
            switch(param2.type)
            {
            case PARMTYPE_IMMEDIATE:
                if(!(param2.flags & (PARAM_VAL32|PARAM_VAL64)))
                    return VERR_EM_INTERPRETER;
                /* fallthru */

            case PARMTYPE_ADDRESS:
                pSrc = (RTGCPTR)param2.val.val64;
                pSrc = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param2, pSrc);
                break;

            default:
                return VERR_EM_INTERPRETER;
            }

            Assert(param1.size <= 8 && param1.size > 0);
            EM_ASSERT_FAULT_RETURN(pSrc == pvFault, VERR_EM_INTERPRETER);
            rc = emRamRead(pVM, pRegFrame, &val64, pSrc, param1.size);
            if (RT_FAILURE(rc))
                return VERR_EM_INTERPRETER;

            /* Destination */
            switch(param1.type)
            {
            case PARMTYPE_REGISTER:
                switch(param1.size)
                {
                case 1: rc = DISWriteReg8(pRegFrame, pCpu->param1.base.reg_gen,  (uint8_t) val64); break;
                case 2: rc = DISWriteReg16(pRegFrame, pCpu->param1.base.reg_gen, (uint16_t)val64); break;
                case 4: rc = DISWriteReg32(pRegFrame, pCpu->param1.base.reg_gen, (uint32_t)val64); break;
                case 8: rc = DISWriteReg64(pRegFrame, pCpu->param1.base.reg_gen, val64); break;
                default:
                    return VERR_EM_INTERPRETER;
                }
                if (RT_FAILURE(rc))
                    return rc;
                break;

            default:
                return VERR_EM_INTERPRETER;
            }
#ifdef LOG_ENABLED
            if (pCpu->mode == CPUMODE_64BIT)
                LogFlow(("EMInterpretInstruction: OP_MOV %RGv -> %RX64 (%d)\n", pSrc, val64, param1.size));
            else
                LogFlow(("EMInterpretInstruction: OP_MOV %RGv -> %08X (%d)\n", pSrc, (uint32_t)val64, param1.size));
#endif
        }
        return VINF_SUCCESS;
#ifdef IN_RC
    }
#endif
    return VERR_EM_INTERPRETER;
}


#ifndef IN_RC
/**
 * [REP] STOSWD emulation
 */
static int emInterpretStosWD(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    int      rc;
    RTGCPTR  GCDest, GCOffset;
    uint32_t cbSize;
    uint64_t cTransfers;
    int      offIncrement;

    /* Don't support any but these three prefix bytes. */
    if ((pCpu->prefix & ~(PREFIX_ADDRSIZE|PREFIX_OPSIZE|PREFIX_REP|PREFIX_REX)))
        return VERR_EM_INTERPRETER;

    switch (pCpu->addrmode)
    {
    case CPUMODE_16BIT:
        GCOffset   = pRegFrame->di;
        cTransfers = pRegFrame->cx;
        break;
    case CPUMODE_32BIT:
        GCOffset   = pRegFrame->edi;
        cTransfers = pRegFrame->ecx;
        break;
    case CPUMODE_64BIT:
        GCOffset   = pRegFrame->rdi;
        cTransfers = pRegFrame->rcx;
        break;
    default:
        AssertFailed();
        return VERR_EM_INTERPRETER;
    }

    GCDest = SELMToFlat(pVM, DIS_SELREG_ES, pRegFrame, GCOffset);
    switch (pCpu->opmode)
    {
    case CPUMODE_16BIT:
        cbSize = 2;
        break;
    case CPUMODE_32BIT:
        cbSize = 4;
        break;
    case CPUMODE_64BIT:
        cbSize = 8;
        break;
    default:
        AssertFailed();
        return VERR_EM_INTERPRETER;
    }

    offIncrement = pRegFrame->eflags.Bits.u1DF ? -(signed)cbSize : (signed)cbSize;

    if (!(pCpu->prefix & PREFIX_REP))
    {
        LogFlow(("emInterpretStosWD dest=%04X:%RGv (%RGv) cbSize=%d\n", pRegFrame->es, GCOffset, GCDest, cbSize));

        rc = emRamWrite(pVM, pRegFrame, GCDest, &pRegFrame->rax, cbSize);
        if (RT_FAILURE(rc))
            return VERR_EM_INTERPRETER;
        Assert(rc == VINF_SUCCESS);

        /* Update (e/r)di. */
        switch (pCpu->addrmode)
        {
        case CPUMODE_16BIT:
            pRegFrame->di  += offIncrement;
            break;
        case CPUMODE_32BIT:
            pRegFrame->edi += offIncrement;
            break;
        case CPUMODE_64BIT:
            pRegFrame->rdi += offIncrement;
            break;
        default:
            AssertFailed();
            return VERR_EM_INTERPRETER;
        }

    }
    else
    {
        if (!cTransfers)
            return VINF_SUCCESS;

        /*
         * Do *not* try emulate cross page stuff here because we don't know what might
         * be waiting for us on the subsequent pages. The caller has only asked us to
         * ignore access handlers fro the current page.
         * This also fends off big stores which would quickly kill PGMR0DynMap.
         */
        if (    cbSize > PAGE_SIZE
            ||  cTransfers > PAGE_SIZE
            ||  (GCDest >> PAGE_SHIFT) != ((GCDest + offIncrement * cTransfers) >> PAGE_SHIFT))
        {
            Log(("STOSWD is crosses pages, chicken out to the recompiler; GCDest=%RGv cbSize=%#x offIncrement=%d cTransfers=%#x\n",
                 GCDest, cbSize, offIncrement, cTransfers));
            return VERR_EM_INTERPRETER;
        }

        LogFlow(("emInterpretStosWD dest=%04X:%RGv (%RGv) cbSize=%d cTransfers=%x DF=%d\n", pRegFrame->es, GCOffset, GCDest, cbSize, cTransfers, pRegFrame->eflags.Bits.u1DF));
        /* Access verification first; we currently can't recover properly from traps inside this instruction */
        rc = PGMVerifyAccess(pVM, GCDest - ((offIncrement > 0) ? 0 : ((cTransfers-1) * cbSize)),
                             cTransfers * cbSize,
                             X86_PTE_RW | (CPUMGetGuestCPL(pVM, pRegFrame) == 3 ? X86_PTE_US : 0));
        if (rc != VINF_SUCCESS)
        {
            Log(("STOSWD will generate a trap -> recompiler, rc=%d\n", rc));
            return VERR_EM_INTERPRETER;
        }

        /* REP case */
        while (cTransfers)
        {
            rc = emRamWrite(pVM, pRegFrame, GCDest, &pRegFrame->rax, cbSize);
            if (RT_FAILURE(rc))
            {
                rc = VERR_EM_INTERPRETER;
                break;
            }

            Assert(rc == VINF_SUCCESS);
            GCOffset += offIncrement;
            GCDest   += offIncrement;
            cTransfers--;
        }

        /* Update the registers. */
        switch (pCpu->addrmode)
        {
        case CPUMODE_16BIT:
            pRegFrame->di = GCOffset;
            pRegFrame->cx = cTransfers;
            break;
        case CPUMODE_32BIT:
            pRegFrame->edi = GCOffset;
            pRegFrame->ecx = cTransfers;
            break;
        case CPUMODE_64BIT:
            pRegFrame->rdi = GCOffset;
            pRegFrame->rcx = cTransfers;
            break;
        default:
            AssertFailed();
            return VERR_EM_INTERPRETER;
        }
    }

    *pcbSize = cbSize;
    return rc;
}
#endif /* !IN_RC */

#ifndef IN_RC

/**
 * [LOCK] CMPXCHG emulation.
 */
static int emInterpretCmpXchg(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    OP_PARAMVAL param1, param2;

#if HC_ARCH_BITS == 32 && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL_IN_R0)
    Assert(pCpu->param1.size <= 4);
#endif

    /* Source to make DISQueryParamVal read the register value - ugly hack */
    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param2, &param2, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    uint64_t valpar;
    switch(param2.type)
    {
    case PARMTYPE_IMMEDIATE: /* register actually */
        valpar = param2.val.val64;
        break;

    default:
        return VERR_EM_INTERPRETER;
    }

    PGMPAGEMAPLOCK Lock;
    RTGCPTR  GCPtrPar1;
    void    *pvParam1;
    uint64_t eflags;

    AssertReturn(pCpu->param1.size == pCpu->param2.size, VERR_EM_INTERPRETER);
    switch(param1.type)
    {
    case PARMTYPE_ADDRESS:
        GCPtrPar1 = param1.val.val64;
        GCPtrPar1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, GCPtrPar1);

        rc = PGMPhysGCPtr2CCPtr(pVM, GCPtrPar1, &pvParam1, &Lock);
        AssertRCReturn(rc, VERR_EM_INTERPRETER);
        break;

    default:
        return VERR_EM_INTERPRETER;
    }

    LogFlow(("%s %RGv rax=%RX64 %RX64\n", emGetMnemonic(pCpu), GCPtrPar1, pRegFrame->rax, valpar));

    if (pCpu->prefix & PREFIX_LOCK)
        eflags = EMEmulateLockCmpXchg(pvParam1, &pRegFrame->rax, valpar, pCpu->param2.size);
    else
        eflags = EMEmulateCmpXchg(pvParam1, &pRegFrame->rax, valpar, pCpu->param2.size);

    LogFlow(("%s %RGv rax=%RX64 %RX64 ZF=%d\n", emGetMnemonic(pCpu), GCPtrPar1, pRegFrame->rax, valpar, !!(eflags & X86_EFL_ZF)));

    /* Update guest's eflags and finish. */
    pRegFrame->eflags.u32 =   (pRegFrame->eflags.u32 & ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF))
                            | (eflags                &  (X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF));

    *pcbSize = param2.size;
    PGMPhysReleasePageMappingLock(pVM, &Lock);
    return VINF_SUCCESS;
}


/**
 * [LOCK] CMPXCHG8B emulation.
 */
static int emInterpretCmpXchg8b(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    Assert(pCpu->mode != CPUMODE_64BIT);    /** @todo check */
    OP_PARAMVAL param1;

    /* Source to make DISQueryParamVal read the register value - ugly hack */
    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    RTGCPTR  GCPtrPar1;
    void    *pvParam1;
    uint64_t eflags;
    PGMPAGEMAPLOCK Lock;

    AssertReturn(pCpu->param1.size == 8, VERR_EM_INTERPRETER);
    switch(param1.type)
    {
    case PARMTYPE_ADDRESS:
        GCPtrPar1 = param1.val.val64;
        GCPtrPar1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, GCPtrPar1);

        rc = PGMPhysGCPtr2CCPtr(pVM, GCPtrPar1, &pvParam1, &Lock);
        AssertRCReturn(rc, VERR_EM_INTERPRETER);
        break;

    default:
        return VERR_EM_INTERPRETER;
    }

    LogFlow(("%s %RGv=%08x eax=%08x\n", emGetMnemonic(pCpu), pvParam1, pRegFrame->eax));

    if (pCpu->prefix & PREFIX_LOCK)
        eflags = EMEmulateLockCmpXchg8b(pvParam1, &pRegFrame->eax, &pRegFrame->edx, pRegFrame->ebx, pRegFrame->ecx);
    else
        eflags = EMEmulateCmpXchg8b(pvParam1, &pRegFrame->eax, &pRegFrame->edx, pRegFrame->ebx, pRegFrame->ecx);

    LogFlow(("%s %RGv=%08x eax=%08x ZF=%d\n", emGetMnemonic(pCpu), pvParam1, pRegFrame->eax, !!(eflags & X86_EFL_ZF)));

    /* Update guest's eflags and finish; note that *only* ZF is affected. */
    pRegFrame->eflags.u32 =   (pRegFrame->eflags.u32 & ~(X86_EFL_ZF))
                            | (eflags                &  (X86_EFL_ZF));

    *pcbSize = 8;
    PGMPhysReleasePageMappingLock(pVM, &Lock);
    return VINF_SUCCESS;
}

#else /* IN_RC */

/**
 * [LOCK] CMPXCHG emulation.
 */
static int emInterpretCmpXchg(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    Assert(pCpu->mode != CPUMODE_64BIT);    /** @todo check */
    OP_PARAMVAL param1, param2;

    /* Source to make DISQueryParamVal read the register value - ugly hack */
    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param2, &param2, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    if (TRPMHasTrap(pVM))
    {
        if (TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW)
        {
            RTRCPTR pParam1;
            uint32_t valpar, eflags;

            AssertReturn(pCpu->param1.size == pCpu->param2.size, VERR_EM_INTERPRETER);
            switch(param1.type)
            {
            case PARMTYPE_ADDRESS:
                pParam1 = (RTRCPTR)param1.val.val64;
                pParam1 = (RTRCPTR)emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, (RTGCPTR)(RTRCUINTPTR)pParam1);
                EM_ASSERT_FAULT_RETURN(pParam1 == (RTRCPTR)pvFault, VERR_EM_INTERPRETER);
                break;

            default:
                return VERR_EM_INTERPRETER;
            }

            switch(param2.type)
            {
            case PARMTYPE_IMMEDIATE: /* register actually */
                valpar = param2.val.val32;
                break;

            default:
                return VERR_EM_INTERPRETER;
            }

            LogFlow(("%s %RRv eax=%08x %08x\n", emGetMnemonic(pCpu), pParam1, pRegFrame->eax, valpar));

            MMGCRamRegisterTrapHandler(pVM);
            if (pCpu->prefix & PREFIX_LOCK)
                rc = EMGCEmulateLockCmpXchg(pParam1, &pRegFrame->eax, valpar, pCpu->param2.size, &eflags);
            else
                rc = EMGCEmulateCmpXchg(pParam1, &pRegFrame->eax, valpar, pCpu->param2.size, &eflags);
            MMGCRamDeregisterTrapHandler(pVM);

            if (RT_FAILURE(rc))
            {
                Log(("%s %RGv eax=%08x %08x -> emulation failed due to page fault!\n", emGetMnemonic(pCpu), pParam1, pRegFrame->eax, valpar));
                return VERR_EM_INTERPRETER;
            }

            LogFlow(("%s %RRv eax=%08x %08x ZF=%d\n", emGetMnemonic(pCpu), pParam1, pRegFrame->eax, valpar, !!(eflags & X86_EFL_ZF)));

            /* Update guest's eflags and finish. */
            pRegFrame->eflags.u32 = (pRegFrame->eflags.u32 & ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF))
                                  | (eflags                &  (X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF));

            *pcbSize = param2.size;
            return VINF_SUCCESS;
        }
    }
    return VERR_EM_INTERPRETER;
}


/**
 * [LOCK] CMPXCHG8B emulation.
 */
static int emInterpretCmpXchg8b(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    Assert(pCpu->mode != CPUMODE_64BIT);    /** @todo check */
    OP_PARAMVAL param1;

    /* Source to make DISQueryParamVal read the register value - ugly hack */
    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    if (TRPMHasTrap(pVM))
    {
        if (TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW)
        {
            RTRCPTR pParam1;
            uint32_t eflags;

            AssertReturn(pCpu->param1.size == 8, VERR_EM_INTERPRETER);
            switch(param1.type)
            {
            case PARMTYPE_ADDRESS:
                pParam1 = (RTRCPTR)param1.val.val64;
                pParam1 = (RTRCPTR)emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, (RTGCPTR)(RTRCUINTPTR)pParam1);
                EM_ASSERT_FAULT_RETURN(pParam1 == (RTRCPTR)pvFault, VERR_EM_INTERPRETER);
                break;

            default:
                return VERR_EM_INTERPRETER;
            }

            LogFlow(("%s %RRv=%08x eax=%08x\n", emGetMnemonic(pCpu), pParam1, pRegFrame->eax));

            MMGCRamRegisterTrapHandler(pVM);
            if (pCpu->prefix & PREFIX_LOCK)
                rc = EMGCEmulateLockCmpXchg8b(pParam1, &pRegFrame->eax, &pRegFrame->edx, pRegFrame->ebx, pRegFrame->ecx, &eflags);
            else
                rc = EMGCEmulateCmpXchg8b(pParam1, &pRegFrame->eax, &pRegFrame->edx, pRegFrame->ebx, pRegFrame->ecx, &eflags);
            MMGCRamDeregisterTrapHandler(pVM);

            if (RT_FAILURE(rc))
            {
                Log(("%s %RGv=%08x eax=%08x -> emulation failed due to page fault!\n", emGetMnemonic(pCpu), pParam1, pRegFrame->eax));
                return VERR_EM_INTERPRETER;
            }

            LogFlow(("%s %RGv=%08x eax=%08x ZF=%d\n", emGetMnemonic(pCpu), pParam1, pRegFrame->eax, !!(eflags & X86_EFL_ZF)));

            /* Update guest's eflags and finish; note that *only* ZF is affected. */
            pRegFrame->eflags.u32 = (pRegFrame->eflags.u32 & ~(X86_EFL_ZF))
                                  | (eflags                &  (X86_EFL_ZF));

            *pcbSize = 8;
            return VINF_SUCCESS;
        }
    }
    return VERR_EM_INTERPRETER;
}

#endif /* IN_RC */

#ifdef IN_RC
/**
 * [LOCK] XADD emulation.
 */
static int emInterpretXAdd(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    Assert(pCpu->mode != CPUMODE_64BIT);    /** @todo check */
    OP_PARAMVAL param1;
    uint32_t *pParamReg2;
    size_t cbSizeParamReg2;

    /* Source to make DISQueryParamVal read the register value - ugly hack */
    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamRegPtr(pRegFrame, pCpu, &pCpu->param2, (void **)&pParamReg2, &cbSizeParamReg2);
    Assert(cbSizeParamReg2 <= 4);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    if (TRPMHasTrap(pVM))
    {
        if (TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW)
        {
            RTRCPTR pParam1;
            uint32_t eflags;

            AssertReturn(pCpu->param1.size == pCpu->param2.size, VERR_EM_INTERPRETER);
            switch(param1.type)
            {
            case PARMTYPE_ADDRESS:
                pParam1 = (RTRCPTR)param1.val.val64;
                pParam1 = (RTRCPTR)emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, (RTGCPTR)(RTRCUINTPTR)pParam1);
                EM_ASSERT_FAULT_RETURN(pParam1 == (RTRCPTR)pvFault, VERR_EM_INTERPRETER);
                break;

            default:
                return VERR_EM_INTERPRETER;
            }

            LogFlow(("XAdd %RRv=%08x reg=%08x\n", pParam1, *pParamReg2));

            MMGCRamRegisterTrapHandler(pVM);
            if (pCpu->prefix & PREFIX_LOCK)
                rc = EMGCEmulateLockXAdd(pParam1, pParamReg2, cbSizeParamReg2, &eflags);
            else
                rc = EMGCEmulateXAdd(pParam1, pParamReg2, cbSizeParamReg2, &eflags);
            MMGCRamDeregisterTrapHandler(pVM);

            if (RT_FAILURE(rc))
            {
                Log(("XAdd %RGv reg=%08x -> emulation failed due to page fault!\n", pParam1, *pParamReg2));
                return VERR_EM_INTERPRETER;
            }

            LogFlow(("XAdd %RGv reg=%08x ZF=%d\n", pParam1, *pParamReg2, !!(eflags & X86_EFL_ZF)));

            /* Update guest's eflags and finish. */
            pRegFrame->eflags.u32 = (pRegFrame->eflags.u32 & ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF))
                                  | (eflags                &  (X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF));

            *pcbSize = cbSizeParamReg2;
            return VINF_SUCCESS;
        }
    }
    return VERR_EM_INTERPRETER;
}
#endif /* IN_RC */


#ifdef IN_RC
/**
 * Interpret IRET (currently only to V86 code)
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 *
 */
VMMDECL(int) EMInterpretIret(PVM pVM, PCPUMCTXCORE pRegFrame)
{
    RTGCUINTPTR pIretStack = (RTGCUINTPTR)pRegFrame->esp;
    RTGCUINTPTR eip, cs, esp, ss, eflags, ds, es, fs, gs, uMask;
    int         rc;

    Assert(!CPUMIsGuestIn64BitCode(pVM, pRegFrame));

    rc  = emRamRead(pVM, pRegFrame, &eip,      (RTGCPTR)pIretStack      , 4);
    rc |= emRamRead(pVM, pRegFrame, &cs,       (RTGCPTR)(pIretStack + 4), 4);
    rc |= emRamRead(pVM, pRegFrame, &eflags,   (RTGCPTR)(pIretStack + 8), 4);
    AssertRCReturn(rc, VERR_EM_INTERPRETER);
    AssertReturn(eflags & X86_EFL_VM, VERR_EM_INTERPRETER);

    rc |= emRamRead(pVM, pRegFrame, &esp,      (RTGCPTR)(pIretStack + 12), 4);
    rc |= emRamRead(pVM, pRegFrame, &ss,       (RTGCPTR)(pIretStack + 16), 4);
    rc |= emRamRead(pVM, pRegFrame, &es,       (RTGCPTR)(pIretStack + 20), 4);
    rc |= emRamRead(pVM, pRegFrame, &ds,       (RTGCPTR)(pIretStack + 24), 4);
    rc |= emRamRead(pVM, pRegFrame, &fs,       (RTGCPTR)(pIretStack + 28), 4);
    rc |= emRamRead(pVM, pRegFrame, &gs,       (RTGCPTR)(pIretStack + 32), 4);
    AssertRCReturn(rc, VERR_EM_INTERPRETER);

    pRegFrame->eip = eip & 0xffff;
    pRegFrame->cs  = cs;

    /* Mask away all reserved bits */
    uMask = X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_TF | X86_EFL_IF | X86_EFL_DF | X86_EFL_OF | X86_EFL_IOPL | X86_EFL_NT | X86_EFL_RF | X86_EFL_VM | X86_EFL_AC | X86_EFL_VIF | X86_EFL_VIP | X86_EFL_ID;
    eflags &= uMask;

#ifndef IN_RING0
    CPUMRawSetEFlags(pVM, pRegFrame, eflags);
#endif
    Assert((pRegFrame->eflags.u32 & (X86_EFL_IF|X86_EFL_IOPL)) == X86_EFL_IF);

    pRegFrame->esp = esp;
    pRegFrame->ss  = ss;
    pRegFrame->ds  = ds;
    pRegFrame->es  = es;
    pRegFrame->fs  = fs;
    pRegFrame->gs  = gs;

    return VINF_SUCCESS;
}
#endif /* IN_RC */


/**
 * IRET Emulation.
 */
static int emInterpretIret(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    /* only allow direct calls to EMInterpretIret for now */
    return VERR_EM_INTERPRETER;
}

/**
 * WBINVD Emulation.
 */
static int emInterpretWbInvd(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    /* Nothing to do. */
    return VINF_SUCCESS;
}


/**
 * Interpret INVLPG
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 * @param   pAddrGC     Operand address
 *
 */
VMMDECL(int) EMInterpretInvlpg(PVM pVM, PCPUMCTXCORE pRegFrame, RTGCPTR pAddrGC)
{
    int rc;

    /** @todo is addr always a flat linear address or ds based
     * (in absence of segment override prefixes)????
     */
#ifdef IN_RC
    LogFlow(("RC: EMULATE: invlpg %RGv\n", pAddrGC));
#endif
    rc = PGMInvalidatePage(pVM, pAddrGC);
    if (    rc == VINF_SUCCESS
        ||  rc == VINF_PGM_SYNC_CR3 /* we can rely on the FF */)
        return VINF_SUCCESS;
    AssertMsgReturn(   rc == VERR_REM_FLUSHED_PAGES_OVERFLOW
                    || rc == VINF_EM_RAW_EMULATE_INSTR,
                    ("%Rrc addr=%RGv\n", rc, pAddrGC),
                    VERR_EM_INTERPRETER);
    return rc;
}


/**
 * INVLPG Emulation.
 */
static int emInterpretInvlPg(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    OP_PARAMVAL param1;
    RTGCPTR     addr;

    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    switch(param1.type)
    {
    case PARMTYPE_IMMEDIATE:
    case PARMTYPE_ADDRESS:
        if(!(param1.flags & (PARAM_VAL32|PARAM_VAL64)))
            return VERR_EM_INTERPRETER;
        addr = (RTGCPTR)param1.val.val64;
        break;

    default:
        return VERR_EM_INTERPRETER;
    }

    /** @todo is addr always a flat linear address or ds based
     * (in absence of segment override prefixes)????
     */
#ifdef IN_RC
    LogFlow(("RC: EMULATE: invlpg %RGv\n", addr));
#endif
    rc = PGMInvalidatePage(pVM, addr);
    if (    rc == VINF_SUCCESS
        ||  rc == VINF_PGM_SYNC_CR3 /* we can rely on the FF */)
        return VINF_SUCCESS;
    AssertMsgReturn(   rc == VERR_REM_FLUSHED_PAGES_OVERFLOW
                    || rc == VINF_EM_RAW_EMULATE_INSTR,
                    ("%Rrc addr=%RGv\n", rc, addr),
                    VERR_EM_INTERPRETER);
    return rc;
}


/**
 * Interpret CPUID given the parameters in the CPU context
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 *
 */
VMMDECL(int) EMInterpretCpuId(PVM pVM, PCPUMCTXCORE pRegFrame)
{
    uint32_t iLeaf = pRegFrame->eax;

    /* cpuid clears the high dwords of the affected 64 bits registers. */
    pRegFrame->rax = 0;
    pRegFrame->rbx = 0;
    pRegFrame->rcx = 0;
    pRegFrame->rdx = 0;

    /* Note: operates the same in 64 and non-64 bits mode. */
    CPUMGetGuestCpuId(pVM, iLeaf, &pRegFrame->eax, &pRegFrame->ebx, &pRegFrame->ecx, &pRegFrame->edx);
    Log(("Emulate: CPUID %x -> %08x %08x %08x %08x\n", iLeaf, pRegFrame->eax, pRegFrame->ebx, pRegFrame->ecx, pRegFrame->edx));
    return VINF_SUCCESS;
}


/**
 * CPUID Emulation.
 */
static int emInterpretCpuId(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    int rc = EMInterpretCpuId(pVM, pRegFrame);
    return rc;
}


/**
 * Interpret CRx read
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 * @param   DestRegGen  General purpose register index (USE_REG_E**))
 * @param   SrcRegCRx   CRx register index (USE_REG_CR*)
 *
 */
VMMDECL(int) EMInterpretCRxRead(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t DestRegGen, uint32_t SrcRegCrx)
{
    int      rc;
    uint64_t val64;

    if (SrcRegCrx == USE_REG_CR8)
    {
        val64 = 0;
        rc = PDMApicGetTPR(pVM, (uint8_t *)&val64, NULL);
        AssertMsgRCReturn(rc, ("PDMApicGetTPR failed\n"), VERR_EM_INTERPRETER);
    }
    else
    {
        rc = CPUMGetGuestCRx(pVM, SrcRegCrx, &val64);
        AssertMsgRCReturn(rc, ("CPUMGetGuestCRx %d failed\n", SrcRegCrx), VERR_EM_INTERPRETER);
    }

    if (CPUMIsGuestIn64BitCode(pVM, pRegFrame))
        rc = DISWriteReg64(pRegFrame, DestRegGen, val64);
    else
        rc = DISWriteReg32(pRegFrame, DestRegGen, val64);

    if(RT_SUCCESS(rc))
    {
        LogFlow(("MOV_CR: gen32=%d CR=%d val=%RX64\n", DestRegGen, SrcRegCrx, val64));
        return VINF_SUCCESS;
    }
    return VERR_EM_INTERPRETER;
}



/**
 * Interpret CLTS
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 *
 */
VMMDECL(int) EMInterpretCLTS(PVM pVM)
{
    uint64_t cr0 = CPUMGetGuestCR0(pVM);
    if (!(cr0 & X86_CR0_TS))
        return VINF_SUCCESS;
    return CPUMSetGuestCR0(pVM, cr0 & ~X86_CR0_TS);
}

/**
 * CLTS Emulation.
 */
static int emInterpretClts(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    return EMInterpretCLTS(pVM);
}


/**
 * Update CRx
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 * @param   DestRegCRx  CRx register index (USE_REG_CR*)
 * @param   val         New CRx value
 *
 */
static int emUpdateCRx(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t DestRegCrx, uint64_t val)
{
    uint64_t oldval;
    uint64_t msrEFER;
    int      rc, rc2;

    /** @todo Clean up this mess. */
    LogFlow(("EMInterpretCRxWrite at %RGv CR%d <- %RX64\n", (RTGCPTR)pRegFrame->rip, DestRegCrx, val));
    switch (DestRegCrx)
    {
    case USE_REG_CR0:
        oldval = CPUMGetGuestCR0(pVM);
#ifdef IN_RC
        /* CR0.WP and CR0.AM changes require a reschedule run in ring 3. */
        if (    (val    & (X86_CR0_WP | X86_CR0_AM))
            !=  (oldval & (X86_CR0_WP | X86_CR0_AM)))
            return VERR_EM_INTERPRETER;
#endif
        rc = VINF_SUCCESS;
        CPUMSetGuestCR0(pVM, val);
        val = CPUMGetGuestCR0(pVM);
        if (    (oldval & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE))
            !=  (val    & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE)))
        {
            /* global flush */
            rc = PGMFlushTLB(pVM, CPUMGetGuestCR3(pVM), true /* global */);
            AssertRCReturn(rc, rc);
        }

        /* Deal with long mode enabling/disabling. */
        msrEFER = CPUMGetGuestEFER(pVM);
        if (msrEFER & MSR_K6_EFER_LME)
        {
            if (    !(oldval & X86_CR0_PG)
                &&  (val & X86_CR0_PG))
            {
                /* Illegal to have an active 64 bits CS selector (AMD Arch. Programmer's Manual Volume 2: Table 14-5) */
                if (pRegFrame->csHid.Attr.n.u1Long)
                {
                    AssertMsgFailed(("Illegal enabling of paging with CS.u1Long = 1!!\n"));
                    return VERR_EM_INTERPRETER; /* @todo generate #GP(0) */
                }

                /* Illegal to switch to long mode before activating PAE first (AMD Arch. Programmer's Manual Volume 2: Table 14-5) */
                if (!(CPUMGetGuestCR4(pVM) & X86_CR4_PAE))
                {
                    AssertMsgFailed(("Illegal enabling of paging with PAE disabled!!\n"));
                    return VERR_EM_INTERPRETER; /* @todo generate #GP(0) */
                }
                msrEFER |= MSR_K6_EFER_LMA;
            }
            else
            if (    (oldval & X86_CR0_PG)
                &&  !(val & X86_CR0_PG))
            {
                msrEFER &= ~MSR_K6_EFER_LMA;
                /* @todo Do we need to cut off rip here? High dword of rip is undefined, so it shouldn't really matter. */
            }
            CPUMSetGuestEFER(pVM, msrEFER);
        }
        rc2 = PGMChangeMode(pVM, CPUMGetGuestCR0(pVM), CPUMGetGuestCR4(pVM), CPUMGetGuestEFER(pVM));
        return rc2 == VINF_SUCCESS ? rc : rc2;

    case USE_REG_CR2:
        rc = CPUMSetGuestCR2(pVM, val); AssertRC(rc);
        return VINF_SUCCESS;

    case USE_REG_CR3:
        /* Reloading the current CR3 means the guest just wants to flush the TLBs */
        rc = CPUMSetGuestCR3(pVM, val); AssertRC(rc);
        if (CPUMGetGuestCR0(pVM) & X86_CR0_PG)
        {
            /* flush */
            rc = PGMFlushTLB(pVM, val, !(CPUMGetGuestCR4(pVM) & X86_CR4_PGE));
            AssertRCReturn(rc, rc);
        }
        return rc;

    case USE_REG_CR4:
        oldval = CPUMGetGuestCR4(pVM);
        rc = CPUMSetGuestCR4(pVM, val); AssertRC(rc);
        val = CPUMGetGuestCR4(pVM);

        /* Illegal to disable PAE when long mode is active. (AMD Arch. Programmer's Manual Volume 2: Table 14-5) */
        msrEFER = CPUMGetGuestEFER(pVM);
        if (    (msrEFER & MSR_K6_EFER_LMA)
            &&  (oldval & X86_CR4_PAE)
            &&  !(val & X86_CR4_PAE))
        {
            return VERR_EM_INTERPRETER; /** @todo generate #GP(0) */
        }

        rc = VINF_SUCCESS;
        if (    (oldval & (X86_CR4_PGE|X86_CR4_PAE|X86_CR4_PSE))
            !=  (val    & (X86_CR4_PGE|X86_CR4_PAE|X86_CR4_PSE)))
        {
            /* global flush */
            rc = PGMFlushTLB(pVM, CPUMGetGuestCR3(pVM), true /* global */);
            AssertRCReturn(rc, rc);
        }

        /* Feeling extremely lazy. */
# ifdef IN_RC
        if (    (oldval & (X86_CR4_OSFSXR|X86_CR4_OSXMMEEXCPT|X86_CR4_PCE|X86_CR4_MCE|X86_CR4_PAE|X86_CR4_DE|X86_CR4_TSD|X86_CR4_PVI|X86_CR4_VME))
            !=  (val    & (X86_CR4_OSFSXR|X86_CR4_OSXMMEEXCPT|X86_CR4_PCE|X86_CR4_MCE|X86_CR4_PAE|X86_CR4_DE|X86_CR4_TSD|X86_CR4_PVI|X86_CR4_VME)))
        {
            Log(("emInterpretMovCRx: CR4: %#RX64->%#RX64 => R3\n", oldval, val));
            VM_FF_SET(pVM, VM_FF_TO_R3);
        }
# endif
        if ((val ^ oldval) & X86_CR4_VME)
            VM_FF_SET(pVM, VM_FF_SELM_SYNC_TSS);

        rc2 = PGMChangeMode(pVM, CPUMGetGuestCR0(pVM), CPUMGetGuestCR4(pVM), CPUMGetGuestEFER(pVM));
        return rc2 == VINF_SUCCESS ? rc : rc2;

    case USE_REG_CR8:
        return PDMApicSetTPR(pVM, val);

    default:
        AssertFailed();
    case USE_REG_CR1: /* illegal op */
        break;
    }
    return VERR_EM_INTERPRETER;
}

/**
 * Interpret CRx write
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 * @param   DestRegCRx  CRx register index (USE_REG_CR*)
 * @param   SrcRegGen   General purpose register index (USE_REG_E**))
 *
 */
VMMDECL(int) EMInterpretCRxWrite(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t DestRegCrx, uint32_t SrcRegGen)
{
    uint64_t val;
    int      rc;

    if (CPUMIsGuestIn64BitCode(pVM, pRegFrame))
    {
        rc = DISFetchReg64(pRegFrame, SrcRegGen, &val);
    }
    else
    {
        uint32_t val32;
        rc = DISFetchReg32(pRegFrame, SrcRegGen, &val32);
        val = val32;
    }

    if (RT_SUCCESS(rc))
        return emUpdateCRx(pVM, pRegFrame, DestRegCrx, val);

    return VERR_EM_INTERPRETER;
}

/**
 * Interpret LMSW
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 * @param   u16Data     LMSW source data.
 *
 */
VMMDECL(int) EMInterpretLMSW(PVM pVM, PCPUMCTXCORE pRegFrame, uint16_t u16Data)
{
    uint64_t OldCr0 = CPUMGetGuestCR0(pVM);

    /* Only PE, MP, EM and TS can be changed; note that PE can't be cleared by this instruction. */
    uint64_t NewCr0 = ( OldCr0 & ~(             X86_CR0_MP | X86_CR0_EM | X86_CR0_TS))
                    | (u16Data &  (X86_CR0_PE | X86_CR0_MP | X86_CR0_EM | X86_CR0_TS));

    return emUpdateCRx(pVM, pRegFrame, USE_REG_CR0, NewCr0);
}

/**
 * LMSW Emulation.
 */
static int emInterpretLmsw(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    OP_PARAMVAL param1;
    uint32_t    val;

    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    switch(param1.type)
    {
    case PARMTYPE_IMMEDIATE:
    case PARMTYPE_ADDRESS:
        if(!(param1.flags & PARAM_VAL16))
            return VERR_EM_INTERPRETER;
        val = param1.val.val32;
        break;

    default:
        return VERR_EM_INTERPRETER;
    }

    LogFlow(("emInterpretLmsw %x\n", val));
    return EMInterpretLMSW(pVM, pRegFrame, val);
}

#ifdef EM_EMULATE_SMSW
/**
 * SMSW Emulation.
 */
static int emInterpretSmsw(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    OP_PARAMVAL param1;
    uint64_t    cr0 = CPUMGetGuestCR0(pVM);

    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    switch(param1.type)
    {
    case PARMTYPE_IMMEDIATE:
        if(param1.size != sizeof(uint16_t))
            return VERR_EM_INTERPRETER;
        LogFlow(("emInterpretSmsw %d <- cr0 (%x)\n", pCpu->param1.base.reg_gen, cr0));
        rc = DISWriteReg16(pRegFrame, pCpu->param1.base.reg_gen, cr0);
        break;

    case PARMTYPE_ADDRESS:
    {
        RTGCPTR pParam1;

        /* Actually forced to 16 bits regardless of the operand size. */
        if(param1.size != sizeof(uint16_t))
            return VERR_EM_INTERPRETER;

        pParam1 = (RTGCPTR)param1.val.val64;
        pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, pParam1);
        LogFlow(("emInterpretSmsw %VGv <- cr0 (%x)\n", pParam1, cr0));

        rc = emRamWrite(pVM, pRegFrame, pParam1, &cr0, sizeof(uint16_t));
        if (RT_FAILURE(rc))
        {
            AssertMsgFailed(("emRamWrite %RGv size=%d failed with %Rrc\n", pParam1, param1.size, rc));
            return VERR_EM_INTERPRETER;
        }
        break;
    }

    default:
        return VERR_EM_INTERPRETER;
    }

    LogFlow(("emInterpretSmsw %x\n", cr0));
    return rc;
}
#endif

/**
 * MOV CRx
 */
static int emInterpretMovCRx(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    if ((pCpu->param1.flags == USE_REG_GEN32 || pCpu->param1.flags == USE_REG_GEN64) && pCpu->param2.flags == USE_REG_CR)
        return EMInterpretCRxRead(pVM, pRegFrame, pCpu->param1.base.reg_gen, pCpu->param2.base.reg_ctrl);

    if (pCpu->param1.flags == USE_REG_CR && (pCpu->param2.flags == USE_REG_GEN32 || pCpu->param2.flags == USE_REG_GEN64))
        return EMInterpretCRxWrite(pVM, pRegFrame, pCpu->param1.base.reg_ctrl, pCpu->param2.base.reg_gen);

    AssertMsgFailedReturn(("Unexpected control register move\n"), VERR_EM_INTERPRETER);
    return VERR_EM_INTERPRETER;
}


/**
 * Interpret DRx write
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 * @param   DestRegDRx  DRx register index (USE_REG_DR*)
 * @param   SrcRegGen   General purpose register index (USE_REG_E**))
 *
 */
VMMDECL(int) EMInterpretDRxWrite(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t DestRegDrx, uint32_t SrcRegGen)
{
    uint64_t val;
    int      rc;

    if (CPUMIsGuestIn64BitCode(pVM, pRegFrame))
    {
        rc = DISFetchReg64(pRegFrame, SrcRegGen, &val);
    }
    else
    {
        uint32_t val32;
        rc = DISFetchReg32(pRegFrame, SrcRegGen, &val32);
        val = val32;
    }

    if (RT_SUCCESS(rc))
    {
        /** @todo we don't fail if illegal bits are set/cleared for e.g. dr7 */
        rc = CPUMSetGuestDRx(pVM, DestRegDrx, val);
        if (RT_SUCCESS(rc))
            return rc;
        AssertMsgFailed(("CPUMSetGuestDRx %d failed\n", DestRegDrx));
    }
    return VERR_EM_INTERPRETER;
}


/**
 * Interpret DRx read
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 * @param   DestRegGen  General purpose register index (USE_REG_E**))
 * @param   SrcRegDRx   DRx register index (USE_REG_DR*)
 *
 */
VMMDECL(int) EMInterpretDRxRead(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t DestRegGen, uint32_t SrcRegDrx)
{
    uint64_t val64;

    int rc = CPUMGetGuestDRx(pVM, SrcRegDrx, &val64);
    AssertMsgRCReturn(rc, ("CPUMGetGuestDRx %d failed\n", SrcRegDrx), VERR_EM_INTERPRETER);
    if (CPUMIsGuestIn64BitCode(pVM, pRegFrame))
    {
        rc = DISWriteReg64(pRegFrame, DestRegGen, val64);
    }
    else
        rc = DISWriteReg32(pRegFrame, DestRegGen, (uint32_t)val64);

    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    return VERR_EM_INTERPRETER;
}


/**
 * MOV DRx
 */
static int emInterpretMovDRx(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    int rc = VERR_EM_INTERPRETER;

    if((pCpu->param1.flags == USE_REG_GEN32 || pCpu->param1.flags == USE_REG_GEN64) && pCpu->param2.flags == USE_REG_DBG)
    {
        rc = EMInterpretDRxRead(pVM, pRegFrame, pCpu->param1.base.reg_gen, pCpu->param2.base.reg_dbg);
    }
    else
    if(pCpu->param1.flags == USE_REG_DBG && (pCpu->param2.flags == USE_REG_GEN32 || pCpu->param2.flags == USE_REG_GEN64))
    {
        rc = EMInterpretDRxWrite(pVM, pRegFrame, pCpu->param1.base.reg_dbg, pCpu->param2.base.reg_gen);
    }
    else
        AssertMsgFailed(("Unexpected debug register move\n"));

    return rc;
}


/**
 * LLDT Emulation.
 */
static int emInterpretLLdt(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    OP_PARAMVAL param1;
    RTSEL       sel;

    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    switch(param1.type)
    {
    case PARMTYPE_ADDRESS:
        return VERR_EM_INTERPRETER; //feeling lazy right now

    case PARMTYPE_IMMEDIATE:
        if(!(param1.flags & PARAM_VAL16))
            return VERR_EM_INTERPRETER;
        sel = (RTSEL)param1.val.val16;
        break;

    default:
        return VERR_EM_INTERPRETER;
    }

#ifdef IN_RING0
    /* Only for the VT-x real-mode emulation case. */
    AssertReturn(CPUMIsGuestInRealMode(pVM), VERR_EM_INTERPRETER);
    CPUMSetGuestLDTR(pVM, sel);
    return VINF_SUCCESS;
#else
    if (sel == 0)
    {
        if (CPUMGetHyperLDTR(pVM) == 0)
        {
            // this simple case is most frequent in Windows 2000 (31k - boot & shutdown)
            return VINF_SUCCESS;
        }
    }
    //still feeling lazy
    return VERR_EM_INTERPRETER;
#endif
}

#ifdef IN_RING0
/**
 * LIDT/LGDT Emulation.
 */
static int emInterpretLIGdt(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    OP_PARAMVAL param1;
    RTGCPTR     pParam1;
    X86XDTR32   dtr32;

    Log(("Emulate %s at %RGv\n", emGetMnemonic(pCpu), (RTGCPTR)pRegFrame->rip));

    /* Only for the VT-x real-mode emulation case. */
    AssertReturn(CPUMIsGuestInRealMode(pVM), VERR_EM_INTERPRETER);

    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    switch(param1.type)
    {
    case PARMTYPE_ADDRESS:
        pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, param1.val.val16);
        break;

    default:
        return VERR_EM_INTERPRETER;
    }

    rc = emRamRead(pVM, pRegFrame, &dtr32, pParam1, sizeof(dtr32));
    AssertRCReturn(rc, VERR_EM_INTERPRETER);

    if (!(pCpu->prefix & PREFIX_OPSIZE))
        dtr32.uAddr &= 0xffffff; /* 16 bits operand size */

    if (pCpu->pCurInstr->opcode == OP_LIDT)
        CPUMSetGuestIDTR(pVM, dtr32.uAddr, dtr32.cb);
    else
        CPUMSetGuestGDTR(pVM, dtr32.uAddr, dtr32.cb);

    return VINF_SUCCESS;
}
#endif


#ifdef IN_RC
/**
 * STI Emulation.
 *
 * @remark the instruction following sti is guaranteed to be executed before any interrupts are dispatched
 */
static int emInterpretSti(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    PPATMGCSTATE pGCState = PATMQueryGCState(pVM);

    if(!pGCState)
    {
        Assert(pGCState);
        return VERR_EM_INTERPRETER;
    }
    pGCState->uVMFlags |= X86_EFL_IF;

    Assert(pRegFrame->eflags.u32 & X86_EFL_IF);
    Assert(pvFault == SELMToFlat(pVM, DIS_SELREG_CS, pRegFrame, (RTGCPTR)pRegFrame->rip));

    pVM->em.s.GCPtrInhibitInterrupts = pRegFrame->eip + pCpu->opsize;
    VM_FF_SET(pVM, VM_FF_INHIBIT_INTERRUPTS);

    return VINF_SUCCESS;
}
#endif /* IN_RC */


/**
 * HLT Emulation.
 */
static int emInterpretHlt(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    return VINF_EM_HALT;
}


/**
 * Interpret RDTSC
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 *
 */
VMMDECL(int) EMInterpretRdtsc(PVM pVM, PCPUMCTXCORE pRegFrame)
{
    unsigned uCR4 = CPUMGetGuestCR4(pVM);

    if (uCR4 & X86_CR4_TSD)
        return VERR_EM_INTERPRETER; /* genuine #GP */

    uint64_t uTicks = TMCpuTickGet(pVM);

    /* Same behaviour in 32 & 64 bits mode */
    pRegFrame->rax = (uint32_t)uTicks;
    pRegFrame->rdx = (uTicks >> 32ULL);

    return VINF_SUCCESS;
}

VMMDECL(int) EMInterpretRdtscp(PVM pVM, PCPUMCTX pCtx)
{
    unsigned uCR4 = CPUMGetGuestCR4(pVM);

    if (!CPUMGetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_RDTSCP))
    {
        AssertFailed();
        return VERR_EM_INTERPRETER; /* genuine #UD */
    }

    if (uCR4 & X86_CR4_TSD)
        return VERR_EM_INTERPRETER; /* genuine #GP */

    uint64_t uTicks = TMCpuTickGet(pVM);

    /* Same behaviour in 32 & 64 bits mode */
    pCtx->rax = (uint32_t)uTicks;
    pCtx->rdx = (uTicks >> 32ULL);
    /* Low dword of the TSC_AUX msr only. */
    pCtx->rcx = (uint32_t)CPUMGetGuestMsr(pVM, MSR_K8_TSC_AUX);

    return VINF_SUCCESS;
}

/**
 * RDTSC Emulation.
 */
static int emInterpretRdtsc(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    return EMInterpretRdtsc(pVM, pRegFrame);
}


/**
 * MONITOR Emulation.
 */
static int emInterpretMonitor(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    uint32_t u32Dummy, u32ExtFeatures, cpl;

    Assert(pCpu->mode != CPUMODE_64BIT);    /** @todo check */
    if (pRegFrame->ecx != 0)
        return VERR_EM_INTERPRETER; /* illegal value. */

    /* Get the current privilege level. */
    cpl = CPUMGetGuestCPL(pVM, pRegFrame);
    if (cpl != 0)
        return VERR_EM_INTERPRETER; /* supervisor only */

    CPUMGetGuestCpuId(pVM, 1, &u32Dummy, &u32Dummy, &u32ExtFeatures, &u32Dummy);
    if (!(u32ExtFeatures & X86_CPUID_FEATURE_ECX_MONITOR))
        return VERR_EM_INTERPRETER; /* not supported */

    return VINF_SUCCESS;
}


/**
 * MWAIT Emulation.
 */
static int emInterpretMWait(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    uint32_t u32Dummy, u32ExtFeatures, cpl;

    Assert(pCpu->mode != CPUMODE_64BIT);    /** @todo check */
    if (pRegFrame->ecx != 0)
        return VERR_EM_INTERPRETER; /* illegal value. */

    /* Get the current privilege level. */
    cpl = CPUMGetGuestCPL(pVM, pRegFrame);
    if (cpl != 0)
        return VERR_EM_INTERPRETER; /* supervisor only */

    CPUMGetGuestCpuId(pVM, 1, &u32Dummy, &u32Dummy, &u32ExtFeatures, &u32Dummy);
    if (!(u32ExtFeatures & X86_CPUID_FEATURE_ECX_MONITOR))
        return VERR_EM_INTERPRETER; /* not supported */

    /** @todo not completely correct */
    return VINF_EM_HALT;
}


#ifdef LOG_ENABLED
static const char *emMSRtoString(uint32_t uMsr)
{
    switch (uMsr)
    {
    case MSR_IA32_APICBASE:
        return "MSR_IA32_APICBASE";
    case MSR_IA32_CR_PAT:
        return "MSR_IA32_CR_PAT";
    case MSR_IA32_SYSENTER_CS:
        return "MSR_IA32_SYSENTER_CS";
    case MSR_IA32_SYSENTER_EIP:
        return "MSR_IA32_SYSENTER_EIP";
    case MSR_IA32_SYSENTER_ESP:
        return "MSR_IA32_SYSENTER_ESP";
    case MSR_K6_EFER:
        return "MSR_K6_EFER";
    case MSR_K8_SF_MASK:
        return "MSR_K8_SF_MASK";
    case MSR_K6_STAR:
        return "MSR_K6_STAR";
    case MSR_K8_LSTAR:
        return "MSR_K8_LSTAR";
    case MSR_K8_CSTAR:
        return "MSR_K8_CSTAR";
    case MSR_K8_FS_BASE:
        return "MSR_K8_FS_BASE";
    case MSR_K8_GS_BASE:
        return "MSR_K8_GS_BASE";
    case MSR_K8_KERNEL_GS_BASE:
        return "MSR_K8_KERNEL_GS_BASE";
    case MSR_K8_TSC_AUX:
        return "MSR_K8_TSC_AUX";
    case MSR_IA32_BIOS_SIGN_ID:
        return "Unsupported MSR_IA32_BIOS_SIGN_ID";
    case MSR_IA32_PLATFORM_ID:
        return "Unsupported MSR_IA32_PLATFORM_ID";
    case MSR_IA32_BIOS_UPDT_TRIG:
        return "Unsupported MSR_IA32_BIOS_UPDT_TRIG";
    case MSR_IA32_TSC:
        return "MSR_IA32_TSC";
    case MSR_IA32_MTRR_CAP:
        return "Unsupported MSR_IA32_MTRR_CAP";
    case MSR_IA32_MCP_CAP:
        return "Unsupported MSR_IA32_MCP_CAP";
    case MSR_IA32_MCP_STATUS:
        return "Unsupported MSR_IA32_MCP_STATUS";
    case MSR_IA32_MCP_CTRL:
        return "Unsupported MSR_IA32_MCP_CTRL";
    case MSR_IA32_MTRR_DEF_TYPE:
        return "Unsupported MSR_IA32_MTRR_DEF_TYPE";
    case MSR_K7_EVNTSEL0:
        return "Unsupported MSR_K7_EVNTSEL0";
    case MSR_K7_EVNTSEL1:
        return "Unsupported MSR_K7_EVNTSEL1";
    case MSR_K7_EVNTSEL2:
        return "Unsupported MSR_K7_EVNTSEL2";
    case MSR_K7_EVNTSEL3:
        return "Unsupported MSR_K7_EVNTSEL3";
    case MSR_IA32_MC0_CTL:
        return "Unsupported MSR_IA32_MC0_CTL";
    case MSR_IA32_MC0_STATUS:
        return "Unsupported MSR_IA32_MC0_STATUS";
    case MSR_IA32_PERFEVTSEL0:
        return "Unsupported MSR_IA32_PERFEVTSEL0";
    case MSR_IA32_PERFEVTSEL1:
        return "Unsupported MSR_IA32_PERFEVTSEL1";
    case MSR_IA32_PERF_STATUS:
        return "Unsupported MSR_IA32_PERF_STATUS";
    case MSR_IA32_PERF_CTL:
        return "Unsupported MSR_IA32_PERF_CTL";
    }
    return "Unknown MSR";
}
#endif /* LOG_ENABLED */


/**
 * Interpret RDMSR
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 *
 */
VMMDECL(int) EMInterpretRdmsr(PVM pVM, PCPUMCTXCORE pRegFrame)
{
    uint32_t u32Dummy, u32Features, cpl;
    uint64_t val;
    CPUMCTX *pCtx;
    int      rc = VINF_SUCCESS;

    /** @todo According to the Intel manuals, there's a REX version of RDMSR that is slightly different.
     *  That version clears the high dwords of both RDX & RAX */
    pCtx = CPUMQueryGuestCtxPtr(pVM);

    /* Get the current privilege level. */
    cpl = CPUMGetGuestCPL(pVM, pRegFrame);
    if (cpl != 0)
        return VERR_EM_INTERPRETER; /* supervisor only */

    CPUMGetGuestCpuId(pVM, 1, &u32Dummy, &u32Dummy, &u32Dummy, &u32Features);
    if (!(u32Features & X86_CPUID_FEATURE_EDX_MSR))
        return VERR_EM_INTERPRETER; /* not supported */

    switch (pRegFrame->ecx)
    {
    case MSR_IA32_TSC:
        val = TMCpuTickGet(pVM);
        break;

    case MSR_IA32_APICBASE:
        rc = PDMApicGetBase(pVM, &val);
        AssertRC(rc);
        break;

    case MSR_IA32_CR_PAT:
        val = pCtx->msrPAT;
        break;

    case MSR_IA32_SYSENTER_CS:
        val = pCtx->SysEnter.cs;
        break;

    case MSR_IA32_SYSENTER_EIP:
        val = pCtx->SysEnter.eip;
        break;

    case MSR_IA32_SYSENTER_ESP:
        val = pCtx->SysEnter.esp;
        break;

    case MSR_K6_EFER:
        val = pCtx->msrEFER;
        break;

    case MSR_K8_SF_MASK:
        val = pCtx->msrSFMASK;
        break;

    case MSR_K6_STAR:
        val = pCtx->msrSTAR;
        break;

    case MSR_K8_LSTAR:
        val = pCtx->msrLSTAR;
        break;

    case MSR_K8_CSTAR:
        val = pCtx->msrCSTAR;
        break;

    case MSR_K8_FS_BASE:
        val = pCtx->fsHid.u64Base;
        break;

    case MSR_K8_GS_BASE:
        val = pCtx->gsHid.u64Base;
        break;

    case MSR_K8_KERNEL_GS_BASE:
        val = pCtx->msrKERNELGSBASE;
        break;

    case MSR_K8_TSC_AUX:
        val = CPUMGetGuestMsr(pVM, MSR_K8_TSC_AUX);
        break;

#if 0 /*def IN_RING0 */
    case MSR_IA32_PLATFORM_ID:
    case MSR_IA32_BIOS_SIGN_ID:
        if (CPUMGetCPUVendor(pVM) == CPUMCPUVENDOR_INTEL)
        {
            /* Available since the P6 family. VT-x implies that this feature is present. */
            if (pRegFrame->ecx == MSR_IA32_PLATFORM_ID)
                val = ASMRdMsr(MSR_IA32_PLATFORM_ID);
            else
            if (pRegFrame->ecx == MSR_IA32_BIOS_SIGN_ID)
                val = ASMRdMsr(MSR_IA32_BIOS_SIGN_ID);
            break;
        }
        /* no break */
#endif
    default:
        /* In X2APIC specification this range is reserved for APIC control. */
        if ((pRegFrame->ecx >= MSR_IA32_APIC_START) && (pRegFrame->ecx < MSR_IA32_APIC_END))
            rc = PDMApicReadMSR(pVM, VMMGetCpuId(pVM), pRegFrame->ecx, &val);
        else
            /* We should actually trigger a #GP here, but don't as that might cause more trouble. */
            val = 0;
        break;
    }
    LogFlow(("EMInterpretRdmsr %s (%x) -> val=%RX64\n", emMSRtoString(pRegFrame->ecx), pRegFrame->ecx, val));
    if (rc == VINF_SUCCESS)
    {
        pRegFrame->rax = (uint32_t) val;
        pRegFrame->rdx = (uint32_t) (val >> 32ULL);
    }
    return rc;
}


/**
 * RDMSR Emulation.
 */
static int emInterpretRdmsr(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    /* Note: the Intel manual claims there's a REX version of RDMSR that's slightly different, so we play safe by completely disassembling the instruction. */
    Assert(!(pCpu->prefix & PREFIX_REX));
    return EMInterpretRdmsr(pVM, pRegFrame);
}


/**
 * Interpret WRMSR
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 */
VMMDECL(int) EMInterpretWrmsr(PVM pVM, PCPUMCTXCORE pRegFrame)
{
    uint32_t u32Dummy, u32Features, cpl;
    uint64_t val;
    CPUMCTX *pCtx;

    /* Note: works the same in 32 and 64 bits modes. */
    pCtx = CPUMQueryGuestCtxPtr(pVM);

    /* Get the current privilege level. */
    cpl = CPUMGetGuestCPL(pVM, pRegFrame);
    if (cpl != 0)
        return VERR_EM_INTERPRETER; /* supervisor only */

    CPUMGetGuestCpuId(pVM, 1, &u32Dummy, &u32Dummy, &u32Dummy, &u32Features);
    if (!(u32Features & X86_CPUID_FEATURE_EDX_MSR))
        return VERR_EM_INTERPRETER; /* not supported */

    val = RT_MAKE_U64(pRegFrame->eax, pRegFrame->edx);
    LogFlow(("EMInterpretWrmsr %s (%x) val=%RX64\n", emMSRtoString(pRegFrame->ecx), pRegFrame->ecx, val));
    switch (pRegFrame->ecx)
    {
    case MSR_IA32_APICBASE:
    {
        int rc = PDMApicSetBase(pVM, val);
        AssertRC(rc);
        break;
    }

    case MSR_IA32_CR_PAT:
        pCtx->msrPAT = val;
        break;

    case MSR_IA32_SYSENTER_CS:
        pCtx->SysEnter.cs = val & 0xffff; /* 16 bits selector */
        break;

    case MSR_IA32_SYSENTER_EIP:
        pCtx->SysEnter.eip = val;
        break;

    case MSR_IA32_SYSENTER_ESP:
        pCtx->SysEnter.esp = val;
        break;

    case MSR_K6_EFER:
    {
        uint64_t uMask = 0;
        uint64_t oldval = pCtx->msrEFER;

        /* Filter out those bits the guest is allowed to change. (e.g. LMA is read-only) */
        CPUMGetGuestCpuId(pVM, 0x80000001, &u32Dummy, &u32Dummy, &u32Dummy, &u32Features);
        if (u32Features & X86_CPUID_AMD_FEATURE_EDX_NX)
            uMask |= MSR_K6_EFER_NXE;
        if (u32Features & X86_CPUID_AMD_FEATURE_EDX_LONG_MODE)
            uMask |= MSR_K6_EFER_LME;
        if (u32Features & X86_CPUID_AMD_FEATURE_EDX_SEP)
            uMask |= MSR_K6_EFER_SCE;
        if (u32Features & X86_CPUID_AMD_FEATURE_EDX_FFXSR)
            uMask |= MSR_K6_EFER_FFXSR;

        /* Check for illegal MSR_K6_EFER_LME transitions: not allowed to change LME if paging is enabled. (AMD Arch. Programmer's Manual Volume 2: Table 14-5) */
        if (    ((pCtx->msrEFER & MSR_K6_EFER_LME) != (val & uMask & MSR_K6_EFER_LME))
            &&  (pCtx->cr0 & X86_CR0_PG))
        {
            AssertMsgFailed(("Illegal MSR_K6_EFER_LME change: paging is enabled!!\n"));
            return VERR_EM_INTERPRETER; /* @todo generate #GP(0) */
        }

        /* There are a few more: e.g. MSR_K6_EFER_LMSLE */
        AssertMsg(!(val & ~(MSR_K6_EFER_NXE|MSR_K6_EFER_LME|MSR_K6_EFER_LMA /* ignored anyway */ |MSR_K6_EFER_SCE|MSR_K6_EFER_FFXSR)), ("Unexpected value %RX64\n", val));
        pCtx->msrEFER = (pCtx->msrEFER & ~uMask) | (val & uMask);

        /* AMD64 Architecture Programmer's Manual: 15.15 TLB Control; flush the TLB if MSR_K6_EFER_NXE, MSR_K6_EFER_LME or MSR_K6_EFER_LMA are changed. */
        if ((oldval & (MSR_K6_EFER_NXE|MSR_K6_EFER_LME|MSR_K6_EFER_LMA)) != (pCtx->msrEFER & (MSR_K6_EFER_NXE|MSR_K6_EFER_LME|MSR_K6_EFER_LMA)))
            HWACCMFlushTLB(pVM);

        break;
    }

    case MSR_K8_SF_MASK:
        pCtx->msrSFMASK = val;
        break;

    case MSR_K6_STAR:
        pCtx->msrSTAR = val;
        break;

    case MSR_K8_LSTAR:
        pCtx->msrLSTAR = val;
        break;

    case MSR_K8_CSTAR:
        pCtx->msrCSTAR = val;
        break;

    case MSR_K8_FS_BASE:
        pCtx->fsHid.u64Base = val;
        break;

    case MSR_K8_GS_BASE:
        pCtx->gsHid.u64Base = val;
        break;

    case MSR_K8_KERNEL_GS_BASE:
        pCtx->msrKERNELGSBASE = val;
        break;

    case MSR_K8_TSC_AUX:
        CPUMSetGuestMsr(pVM, MSR_K8_TSC_AUX, val);
        break;

    default:
        /* In X2APIC specification this range is reserved for APIC control. */
        if ((pRegFrame->ecx >=  MSR_IA32_APIC_START) && (pRegFrame->ecx <  MSR_IA32_APIC_END))
            return PDMApicWriteMSR(pVM, VMMGetCpuId(pVM), pRegFrame->ecx, val);

        /* We should actually trigger a #GP here, but don't as that might cause more trouble. */
        break;
    }
    return VINF_SUCCESS;
}


/**
 * WRMSR Emulation.
 */
static int emInterpretWrmsr(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    return EMInterpretWrmsr(pVM, pRegFrame);
}


/**
 * Internal worker.
 * @copydoc EMInterpretInstructionCPU
 */
DECLINLINE(int) emInterpretInstructionCPU(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    Assert(pcbSize);
    *pcbSize = 0;

    /*
     * Only supervisor guest code!!
     * And no complicated prefixes.
     */
    /* Get the current privilege level. */
    uint32_t cpl = CPUMGetGuestCPL(pVM, pRegFrame);
    if (    cpl != 0
        &&  pCpu->pCurInstr->opcode != OP_RDTSC)    /* rdtsc requires emulation in ring 3 as well */
    {
        Log(("WARNING: refusing instruction emulation for user-mode code!!\n"));
        STAM_COUNTER_INC(&pVM->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,FailedUserMode));
        return VERR_EM_INTERPRETER;
    }

#ifdef IN_RC
    if (    (pCpu->prefix & (PREFIX_REPNE | PREFIX_REP))
        ||  (   (pCpu->prefix & PREFIX_LOCK)
             && pCpu->pCurInstr->opcode != OP_CMPXCHG
             && pCpu->pCurInstr->opcode != OP_CMPXCHG8B
             && pCpu->pCurInstr->opcode != OP_XADD
             && pCpu->pCurInstr->opcode != OP_OR
             && pCpu->pCurInstr->opcode != OP_BTR
            )
       )
#else
    if (    (pCpu->prefix & PREFIX_REPNE)
        ||  (   (pCpu->prefix & PREFIX_REP)
             && pCpu->pCurInstr->opcode != OP_STOSWD
            )
        ||  (   (pCpu->prefix & PREFIX_LOCK)
             && pCpu->pCurInstr->opcode != OP_OR
             && pCpu->pCurInstr->opcode != OP_BTR
             && pCpu->pCurInstr->opcode != OP_CMPXCHG
             && pCpu->pCurInstr->opcode != OP_CMPXCHG8B
            )
       )
#endif
    {
        //Log(("EMInterpretInstruction: wrong prefix!!\n"));
        STAM_COUNTER_INC(&pVM->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,FailedPrefix));
        return VERR_EM_INTERPRETER;
    }

#if HC_ARCH_BITS == 32
    /*
     * Unable to emulate most >4 bytes accesses in 32 bits mode.
     * Whitelisted instructions are safe.
     */
    if (    pCpu->param1.size > 4
        &&  CPUMIsGuestIn64BitCode(pVM, pRegFrame))
    {
        uint32_t uOpCode = pCpu->pCurInstr->opcode;
        if (    uOpCode != OP_STOSWD
            &&  uOpCode != OP_MOV
            &&  uOpCode != OP_CMPXCHG8B
            &&  uOpCode != OP_XCHG
            &&  uOpCode != OP_BTS
            &&  uOpCode != OP_BTR
            &&  uOpCode != OP_BTC
# ifdef VBOX_WITH_HYBRID_32BIT_KERNEL_IN_R0
            &&  uOpCode != OP_CMPXCHG /* solaris */
            &&  uOpCode != OP_AND     /* windows */
            &&  uOpCode != OP_OR      /* windows */
            &&  uOpCode != OP_XOR     /* because we can */
            &&  uOpCode != OP_ADD     /* windows (dripple) */
            &&  uOpCode != OP_ADC     /* because we can */
            &&  uOpCode != OP_SUB     /* because we can */
            /** @todo OP_BTS or is that a different kind of failure? */
# endif
            )
        {
# ifdef VBOX_WITH_STATISTICS
            switch (pCpu->pCurInstr->opcode)
            {
#  define INTERPRET_FAILED_CASE(opcode, Instr) \
                case opcode: STAM_COUNTER_INC(&pVM->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Failed##Instr)); break;
                INTERPRET_FAILED_CASE(OP_XCHG,Xchg);
                INTERPRET_FAILED_CASE(OP_DEC,Dec);
                INTERPRET_FAILED_CASE(OP_INC,Inc);
                INTERPRET_FAILED_CASE(OP_POP,Pop);
                INTERPRET_FAILED_CASE(OP_OR, Or);
                INTERPRET_FAILED_CASE(OP_XOR,Xor);
                INTERPRET_FAILED_CASE(OP_AND,And);
                INTERPRET_FAILED_CASE(OP_MOV,Mov);
                INTERPRET_FAILED_CASE(OP_STOSWD,StosWD);
                INTERPRET_FAILED_CASE(OP_INVLPG,InvlPg);
                INTERPRET_FAILED_CASE(OP_CPUID,CpuId);
                INTERPRET_FAILED_CASE(OP_MOV_CR,MovCRx);
                INTERPRET_FAILED_CASE(OP_MOV_DR,MovDRx);
                INTERPRET_FAILED_CASE(OP_LLDT,LLdt);
                INTERPRET_FAILED_CASE(OP_LIDT,LIdt);
                INTERPRET_FAILED_CASE(OP_LGDT,LGdt);
                INTERPRET_FAILED_CASE(OP_LMSW,Lmsw);
                INTERPRET_FAILED_CASE(OP_CLTS,Clts);
                INTERPRET_FAILED_CASE(OP_MONITOR,Monitor);
                INTERPRET_FAILED_CASE(OP_MWAIT,MWait);
                INTERPRET_FAILED_CASE(OP_RDMSR,Rdmsr);
                INTERPRET_FAILED_CASE(OP_WRMSR,Wrmsr);
                INTERPRET_FAILED_CASE(OP_ADD,Add);
                INTERPRET_FAILED_CASE(OP_SUB,Sub);
                INTERPRET_FAILED_CASE(OP_ADC,Adc);
                INTERPRET_FAILED_CASE(OP_BTR,Btr);
                INTERPRET_FAILED_CASE(OP_BTS,Bts);
                INTERPRET_FAILED_CASE(OP_BTC,Btc);
                INTERPRET_FAILED_CASE(OP_RDTSC,Rdtsc);
                INTERPRET_FAILED_CASE(OP_CMPXCHG, CmpXchg);
                INTERPRET_FAILED_CASE(OP_STI, Sti);
                INTERPRET_FAILED_CASE(OP_XADD,XAdd);
                INTERPRET_FAILED_CASE(OP_CMPXCHG8B,CmpXchg8b);
                INTERPRET_FAILED_CASE(OP_HLT, Hlt);
                INTERPRET_FAILED_CASE(OP_IRET,Iret);
                INTERPRET_FAILED_CASE(OP_WBINVD,WbInvd);
                INTERPRET_FAILED_CASE(OP_MOVNTPS,MovNTPS);
#  undef INTERPRET_FAILED_CASE
                default:
                    STAM_COUNTER_INC(&pVM->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,FailedMisc));
                    break;
            }
# endif /* VBOX_WITH_STATISTICS */
            return VERR_EM_INTERPRETER;
        }
    }
#endif

    int rc;
#if (defined(VBOX_STRICT) || defined(LOG_ENABLED))
    LogFlow(("emInterpretInstructionCPU %s\n", emGetMnemonic(pCpu)));
#endif
    switch (pCpu->pCurInstr->opcode)
    {
        /*
         * Macros for generating the right case statements.
         */
# define INTERPRET_CASE_EX_LOCK_PARAM3(opcode, Instr, InstrFn, pfnEmulate, pfnEmulateLock) \
        case opcode:\
            if (pCpu->prefix & PREFIX_LOCK) \
                rc = emInterpretLock##InstrFn(pVM, pCpu, pRegFrame, pvFault, pcbSize, pfnEmulateLock); \
            else \
                rc = emInterpret##InstrFn(pVM, pCpu, pRegFrame, pvFault, pcbSize, pfnEmulate); \
            if (RT_SUCCESS(rc)) \
                STAM_COUNTER_INC(&pVM->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Instr)); \
            else \
                STAM_COUNTER_INC(&pVM->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Failed##Instr)); \
            return rc
#define INTERPRET_CASE_EX_PARAM3(opcode, Instr, InstrFn, pfnEmulate) \
        case opcode:\
            rc = emInterpret##InstrFn(pVM, pCpu, pRegFrame, pvFault, pcbSize, pfnEmulate); \
            if (RT_SUCCESS(rc)) \
                STAM_COUNTER_INC(&pVM->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Instr)); \
            else \
                STAM_COUNTER_INC(&pVM->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Failed##Instr)); \
            return rc

#define INTERPRET_CASE_EX_PARAM2(opcode, Instr, InstrFn, pfnEmulate) \
            INTERPRET_CASE_EX_PARAM3(opcode, Instr, InstrFn, pfnEmulate)
#define INTERPRET_CASE_EX_LOCK_PARAM2(opcode, Instr, InstrFn, pfnEmulate, pfnEmulateLock) \
            INTERPRET_CASE_EX_LOCK_PARAM3(opcode, Instr, InstrFn, pfnEmulate, pfnEmulateLock)

#define INTERPRET_CASE(opcode, Instr) \
        case opcode:\
            rc = emInterpret##Instr(pVM, pCpu, pRegFrame, pvFault, pcbSize); \
            if (RT_SUCCESS(rc)) \
                STAM_COUNTER_INC(&pVM->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Instr)); \
            else \
                STAM_COUNTER_INC(&pVM->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Failed##Instr)); \
            return rc

#define INTERPRET_CASE_EX_DUAL_PARAM2(opcode, Instr, InstrFn) \
        case opcode:\
            rc = emInterpret##InstrFn(pVM, pCpu, pRegFrame, pvFault, pcbSize); \
            if (RT_SUCCESS(rc)) \
                STAM_COUNTER_INC(&pVM->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Instr)); \
            else \
                STAM_COUNTER_INC(&pVM->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Failed##Instr)); \
            return rc

#define INTERPRET_STAT_CASE(opcode, Instr) \
        case opcode: STAM_COUNTER_INC(&pVM->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Failed##Instr)); return VERR_EM_INTERPRETER;

        /*
         * The actual case statements.
         */
        INTERPRET_CASE(OP_XCHG,Xchg);
        INTERPRET_CASE_EX_PARAM2(OP_DEC,Dec, IncDec, EMEmulateDec);
        INTERPRET_CASE_EX_PARAM2(OP_INC,Inc, IncDec, EMEmulateInc);
        INTERPRET_CASE(OP_POP,Pop);
        INTERPRET_CASE_EX_LOCK_PARAM3(OP_OR, Or, OrXorAnd, EMEmulateOr, EMEmulateLockOr);
        INTERPRET_CASE_EX_PARAM3(OP_XOR,Xor, OrXorAnd, EMEmulateXor);
        INTERPRET_CASE_EX_PARAM3(OP_AND,And, OrXorAnd, EMEmulateAnd);
        INTERPRET_CASE(OP_MOV,Mov);
#ifndef IN_RC
        INTERPRET_CASE(OP_STOSWD,StosWD);
#endif
        INTERPRET_CASE(OP_INVLPG,InvlPg);
        INTERPRET_CASE(OP_CPUID,CpuId);
        INTERPRET_CASE(OP_MOV_CR,MovCRx);
        INTERPRET_CASE(OP_MOV_DR,MovDRx);
#ifdef IN_RING0
        INTERPRET_CASE_EX_DUAL_PARAM2(OP_LIDT, LIdt, LIGdt);
        INTERPRET_CASE_EX_DUAL_PARAM2(OP_LGDT, LGdt, LIGdt);
#endif
        INTERPRET_CASE(OP_LLDT,LLdt);
        INTERPRET_CASE(OP_LMSW,Lmsw);
#ifdef EM_EMULATE_SMSW
        INTERPRET_CASE(OP_SMSW,Smsw);
#endif
        INTERPRET_CASE(OP_CLTS,Clts);
        INTERPRET_CASE(OP_MONITOR, Monitor);
        INTERPRET_CASE(OP_MWAIT, MWait);
        INTERPRET_CASE(OP_RDMSR, Rdmsr);
        INTERPRET_CASE(OP_WRMSR, Wrmsr);
        INTERPRET_CASE_EX_PARAM3(OP_ADD,Add, AddSub, EMEmulateAdd);
        INTERPRET_CASE_EX_PARAM3(OP_SUB,Sub, AddSub, EMEmulateSub);
        INTERPRET_CASE(OP_ADC,Adc);
        INTERPRET_CASE_EX_LOCK_PARAM2(OP_BTR,Btr, BitTest, EMEmulateBtr, EMEmulateLockBtr);
        INTERPRET_CASE_EX_PARAM2(OP_BTS,Bts, BitTest, EMEmulateBts);
        INTERPRET_CASE_EX_PARAM2(OP_BTC,Btc, BitTest, EMEmulateBtc);
        INTERPRET_CASE(OP_RDTSC,Rdtsc);
        INTERPRET_CASE(OP_CMPXCHG, CmpXchg);
#ifdef IN_RC
        INTERPRET_CASE(OP_STI,Sti);
        INTERPRET_CASE(OP_XADD, XAdd);
#endif
        INTERPRET_CASE(OP_CMPXCHG8B, CmpXchg8b);
        INTERPRET_CASE(OP_HLT,Hlt);
        INTERPRET_CASE(OP_IRET,Iret);
        INTERPRET_CASE(OP_WBINVD,WbInvd);
#ifdef VBOX_WITH_STATISTICS
# ifndef IN_RC
        INTERPRET_STAT_CASE(OP_XADD, XAdd);
# endif
        INTERPRET_STAT_CASE(OP_MOVNTPS,MovNTPS);
#endif

        default:
            Log3(("emInterpretInstructionCPU: opcode=%d\n", pCpu->pCurInstr->opcode));
            STAM_COUNTER_INC(&pVM->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,FailedMisc));
            return VERR_EM_INTERPRETER;

#undef INTERPRET_CASE_EX_PARAM2
#undef INTERPRET_STAT_CASE
#undef INTERPRET_CASE_EX
#undef INTERPRET_CASE
    } /* switch (opcode) */
    AssertFailed();
    return VERR_INTERNAL_ERROR;
}


/**
 * Sets the PC for which interrupts should be inhibited.
 *
 * @param   pVM         The VM handle.
 * @param   PC          The PC.
 */
VMMDECL(void) EMSetInhibitInterruptsPC(PVM pVM, RTGCUINTPTR PC)
{
    pVM->em.s.GCPtrInhibitInterrupts = PC;
    VM_FF_SET(pVM, VM_FF_INHIBIT_INTERRUPTS);
}


/**
 * Gets the PC for which interrupts should be inhibited.
 *
 * There are a few instructions which inhibits or delays interrupts
 * for the instruction following them. These instructions are:
 *      - STI
 *      - MOV SS, r/m16
 *      - POP SS
 *
 * @returns The PC for which interrupts should be inhibited.
 * @param   pVM         VM handle.
 *
 */
VMMDECL(RTGCUINTPTR) EMGetInhibitInterruptsPC(PVM pVM)
{
    return pVM->em.s.GCPtrInhibitInterrupts;
}

