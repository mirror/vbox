/* $Id$ */
/** @file
 * EM - Execution Monitor(/Manager) - All contexts
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_EM
#include <VBox/vmm/em.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/selm.h>
#include <VBox/vmm/patm.h>
#include <VBox/vmm/csam.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/stam.h>
#include "EMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/hwaccm.h>
#include <VBox/vmm/tm.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include <VBox/log.h>
#include "internal/pgm.h"
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

/* Used to pass information during instruction disassembly. */
typedef struct
{
    PVM         pVM;
    PVMCPU      pVCpu;
    RTGCPTR     GCPtr;
    uint8_t     aOpcode[8];
} EMDISSTATE, *PEMDISSTATE;

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
DECLINLINE(VBOXSTRICTRC) emInterpretInstructionCPU(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame,
                                                   RTGCPTR pvFault, EMCODETYPE enmCodeType, uint32_t *pcbSize);



/**
 * Get the current execution manager status.
 *
 * @returns Current status.
 * @param   pVCpu         The VMCPU to operate on.
 */
VMMDECL(EMSTATE) EMGetState(PVMCPU pVCpu)
{
    return pVCpu->em.s.enmState;
}

/**
 * Sets the current execution manager status. (use only when you know what you're doing!)
 *
 * @param   pVCpu         The VMCPU to operate on.
 */
VMMDECL(void)    EMSetState(PVMCPU pVCpu, EMSTATE enmNewState)
{
    /* Only allowed combination: */
    Assert(pVCpu->em.s.enmState == EMSTATE_WAIT_SIPI && enmNewState == EMSTATE_HALTED);
    pVCpu->em.s.enmState = enmNewState;
}


/**
 * Read callback for disassembly function; supports reading bytes that cross a page boundary
 *
 * @returns VBox status code.
 * @param   pSrc        GC source pointer
 * @param   pDest       HC destination pointer
 * @param   cb          Number of bytes to read
 * @param   dwUserdata  Callback specific user data (pDis)
 *
 */
DECLCALLBACK(int) EMReadBytes(RTUINTPTR pSrc, uint8_t *pDest, unsigned cb, void *pvUserdata)
{
    PDISCPUSTATE  pDis   = (PDISCPUSTATE)pvUserdata;
    PEMDISSTATE   pState = (PEMDISSTATE)pDis->apvUserData[0];
    PVM           pVM    = pState->pVM;
    PVMCPU        pVCpu  = pState->pVCpu;

# ifdef IN_RING0
    int rc;

    if (    pState->GCPtr
        &&  pSrc + cb <= pState->GCPtr + sizeof(pState->aOpcode))
    {
        unsigned offset = pSrc - pState->GCPtr;

        Assert(pSrc >= pState->GCPtr);

        for (unsigned i=0; i<cb; i++)
        {
            pDest[i] = pState->aOpcode[offset + i];
        }
        return VINF_SUCCESS;
    }

    rc = PGMPhysSimpleReadGCPtr(pVCpu, pDest, pSrc, cb);
    AssertMsgRC(rc, ("PGMPhysSimpleReadGCPtr failed for pSrc=%RGv cb=%x rc=%d\n", pSrc, cb, rc));
# elif defined(IN_RING3)
    if (!PATMIsPatchGCAddr(pVM, pSrc))
    {
        int rc = PGMPhysSimpleReadGCPtr(pVCpu, pDest, pSrc, cb);
        AssertRC(rc);
    }
    else
        memcpy(pDest, PATMR3GCPtrToHCPtr(pVM, pSrc), cb);

# elif defined(IN_RC)
    if (!PATMIsPatchGCAddr(pVM, pSrc))
    {
        int rc = MMGCRamRead(pVM, pDest, (void *)(uintptr_t)pSrc, cb);
        if (rc == VERR_ACCESS_DENIED)
        {
            /* Recently flushed; access the data manually. */
            rc = PGMPhysSimpleReadGCPtr(pVCpu, pDest, pSrc, cb);
            AssertRC(rc);
        }
    }
    else /* the hypervisor region is always present. */
        memcpy(pDest, (RTRCPTR)(uintptr_t)pSrc, cb);

# endif /* IN_RING3 */
    return VINF_SUCCESS;
}


#ifndef IN_RC
DECLINLINE(int) emDisCoreOne(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, RTGCUINTPTR InstrGC, uint32_t *pOpsize)
{
    EMDISSTATE State;

    State.pVM   = pVM;
    State.pVCpu = pVCpu;
    int rc = PGMPhysSimpleReadGCPtr(pVCpu, &State.aOpcode, InstrGC, sizeof(State.aOpcode));
    if (RT_SUCCESS(rc))
    {
        State.GCPtr = InstrGC;
    }
    else
    {
        if (PAGE_ADDRESS(InstrGC) == PAGE_ADDRESS(InstrGC + sizeof(State.aOpcode) - 1))
        {
           if (rc == VERR_PAGE_TABLE_NOT_PRESENT)
              HWACCMInvalidatePage(pVCpu, InstrGC);

           Log(("emDisCoreOne: read failed with %d\n", rc));
           return rc;
        }
        State.GCPtr = NIL_RTGCPTR;
    }
    return DISCoreOneEx(InstrGC, pDis->mode, EMReadBytes, &State, pDis, pOpsize);
}

#else /* IN_RC */

DECLINLINE(int) emDisCoreOne(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, RTGCUINTPTR InstrGC, uint32_t *pOpsize)
{
    EMDISSTATE State;

    State.pVM   = pVM;
    State.pVCpu = pVCpu;
    State.GCPtr = InstrGC;

    return DISCoreOneEx(InstrGC, pDis->mode, EMReadBytes, &State, pDis, pOpsize);
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
 * @param   pVCpu           The VMCPU handle.
 * @param   pCtxCore        The context core (used for both the mode and instruction).
 * @param   pDis            Where to return the parsed instruction info.
 * @param   pcbInstr        Where to return the instruction size. (optional)
 */
VMMDECL(int) EMInterpretDisasOne(PVM pVM, PVMCPU pVCpu, PCCPUMCTXCORE pCtxCore, PDISCPUSTATE pDis, unsigned *pcbInstr)
{
    RTGCPTR GCPtrInstr;
    int rc = SELMToFlatEx(pVM, DIS_SELREG_CS, pCtxCore, pCtxCore->rip, 0, &GCPtrInstr);
    if (RT_FAILURE(rc))
    {
        Log(("EMInterpretDisasOne: Failed to convert %RTsel:%RGv (cpl=%d) - rc=%Rrc !!\n",
             pCtxCore->cs, (RTGCPTR)pCtxCore->rip, pCtxCore->ss & X86_SEL_RPL, rc));
        return rc;
    }
    return EMInterpretDisasOneEx(pVM, pVCpu, (RTGCUINTPTR)GCPtrInstr, pCtxCore, pDis, pcbInstr);
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
 * @param   pVCpu           The VMCPU handle.
 * @param   GCPtrInstr      The flat address of the instruction.
 * @param   pCtxCore        The context core (used to determine the cpu mode).
 * @param   pDis            Where to return the parsed instruction info.
 * @param   pcbInstr        Where to return the instruction size. (optional)
 */
VMMDECL(int) EMInterpretDisasOneEx(PVM pVM, PVMCPU pVCpu, RTGCUINTPTR GCPtrInstr, PCCPUMCTXCORE pCtxCore, PDISCPUSTATE pDis, unsigned *pcbInstr)
{
    int        rc;
    EMDISSTATE State;

    State.pVM   = pVM;
    State.pVCpu = pVCpu;

#ifdef IN_RC
    State.GCPtr = GCPtrInstr;
#else /* ring 0/3 */
    rc = PGMPhysSimpleReadGCPtr(pVCpu, &State.aOpcode, GCPtrInstr, sizeof(State.aOpcode));
    if (RT_SUCCESS(rc))
    {
        State.GCPtr = GCPtrInstr;
    }
    else
    {
        if (PAGE_ADDRESS(GCPtrInstr) == PAGE_ADDRESS(GCPtrInstr + sizeof(State.aOpcode) - 1))
        {
           if (rc == VERR_PAGE_TABLE_NOT_PRESENT)
              HWACCMInvalidatePage(pVCpu, GCPtrInstr);

           Log(("EMInterpretDisasOneEx: read failed with %d\n", rc));
           return rc;
        }
        State.GCPtr = NIL_RTGCPTR;
    }
#endif

    rc = DISCoreOneEx(GCPtrInstr, SELMGetCpuModeFromSelector(pVM, pCtxCore->eflags, pCtxCore->cs, (PCPUMSELREGHID)&pCtxCore->csHid),
                      EMReadBytes, &State,
                      pDis, pcbInstr);
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
 * @param   pVCpu       The VMCPU handle.
 * @param   pRegFrame   The register frame.
 *                      Updates the EIP if an instruction was executed successfully.
 * @param   pvFault     The fault address (CR2).
 * @param   pcbSize     Size of the write (if applicable).
 *
 * @remark  Invalid opcode exceptions have a higher priority than GP (see Intel
 *          Architecture System Developers Manual, Vol 3, 5.5) so we don't need
 *          to worry about e.g. invalid modrm combinations (!)
 */
VMMDECL(VBOXSTRICTRC) EMInterpretInstruction(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    RTGCPTR pbCode;

    LogFlow(("EMInterpretInstruction %RGv fault %RGv\n", (RTGCPTR)pRegFrame->rip, pvFault));
    VBOXSTRICTRC rc = SELMToFlatEx(pVM, DIS_SELREG_CS, pRegFrame, pRegFrame->rip, 0, &pbCode);
    if (RT_SUCCESS(rc))
    {
        uint32_t     cbOp;
        PDISCPUSTATE pDis = &pVCpu->em.s.DisState;
        pDis->mode = SELMGetCpuModeFromSelector(pVM, pRegFrame->eflags, pRegFrame->cs, &pRegFrame->csHid);
        rc = emDisCoreOne(pVM, pVCpu, pDis, (RTGCUINTPTR)pbCode, &cbOp);
        if (RT_SUCCESS(rc))
        {
            Assert(cbOp == pDis->opsize);
            rc = EMInterpretInstructionCPU(pVM, pVCpu, pDis, pRegFrame, pvFault, EMCODETYPE_SUPERVISOR, pcbSize);
            if (RT_SUCCESS(rc))
                pRegFrame->rip += cbOp; /* Move on to the next instruction. */

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
 * @returns VBox strict status code.
 * @retval  VINF_*                  Scheduling instructions. When these are returned, it
 *                                  starts to get a bit tricky to know whether code was
 *                                  executed or not... We'll address this when it becomes a problem.
 * @retval  VERR_EM_INTERPRETER     Something we can't cope with.
 * @retval  VERR_*                  Fatal errors.
 *
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU handle.
 * @param   pDis        The disassembler cpu state for the instruction to be
 *                      interpreted.
 * @param   pRegFrame   The register frame. EIP is *NOT* changed!
 * @param   pvFault     The fault address (CR2).
 * @param   pcbSize     Size of the write (if applicable).
 * @param   enmCodeType Code type (user/supervisor)
 *
 * @remark  Invalid opcode exceptions have a higher priority than GP (see Intel
 *          Architecture System Developers Manual, Vol 3, 5.5) so we don't need
 *          to worry about e.g. invalid modrm combinations (!)
 *
 * @todo    At this time we do NOT check if the instruction overwrites vital information.
 *          Make sure this can't happen!! (will add some assertions/checks later)
 */
VMMDECL(VBOXSTRICTRC) EMInterpretInstructionCPU(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame,
                                                RTGCPTR pvFault, EMCODETYPE enmCodeType, uint32_t *pcbSize)
{
    STAM_PROFILE_START(&pVCpu->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Emulate), a);
    VBOXSTRICTRC rc = emInterpretInstructionCPU(pVM, pVCpu, pDis, pRegFrame, pvFault, enmCodeType, pcbSize);
    STAM_PROFILE_STOP(&pVCpu->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Emulate), a);
    if (RT_SUCCESS(rc))
        STAM_COUNTER_INC(&pVCpu->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,InterpretSucceeded));
    else
        STAM_COUNTER_INC(&pVCpu->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,InterpretFailed));
    return rc;
}


/**
 * Interpret a port I/O instruction.
 *
 * @returns VBox status code suitable for scheduling.
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU handle.
 * @param   pCtxCore    The context core. This will be updated on successful return.
 * @param   pDis        The instruction to interpret.
 * @param   cbOp        The size of the instruction.
 * @remark  This may raise exceptions.
 */
VMMDECL(VBOXSTRICTRC) EMInterpretPortIO(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pCtxCore, PDISCPUSTATE pDis, uint32_t cbOp)
{
    /*
     * Hand it on to IOM.
     */
#ifdef IN_RC
    VBOXSTRICTRC rcStrict = IOMGCIOPortHandler(pVM, pCtxCore, pDis);
    if (IOM_SUCCESS(rcStrict))
        pCtxCore->rip += cbOp;
    return rcStrict;
#else
    AssertReleaseMsgFailed(("not implemented\n"));
    return VERR_NOT_IMPLEMENTED;
#endif
}


DECLINLINE(int) emRamRead(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pCtxCore, void *pvDst, RTGCPTR GCPtrSrc, uint32_t cb)
{
#ifdef IN_RC
    int rc = MMGCRamRead(pVM, pvDst, (void *)(uintptr_t)GCPtrSrc, cb);
    if (RT_LIKELY(rc != VERR_ACCESS_DENIED))
        return rc;
    /*
     * The page pool cache may end up here in some cases because it
     * flushed one of the shadow mappings used by the trapping
     * instruction and it either flushed the TLB or the CPU reused it.
     */
#endif
    return PGMPhysInterpretedReadNoHandlers(pVCpu, pCtxCore, pvDst, GCPtrSrc, cb, /*fMayTrap*/ false);
}


DECLINLINE(int) emRamWrite(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pCtxCore, RTGCPTR GCPtrDst, const void *pvSrc, uint32_t cb)
{
    /* Don't use MMGCRamWrite here as it does not respect zero pages, shared
       pages or write monitored pages. */
    return PGMPhysInterpretedWriteNoHandlers(pVCpu, pCtxCore, GCPtrDst, pvSrc, cb, /*fMayTrap*/ false);
}


/** Convert sel:addr to a flat GC address. */
DECLINLINE(RTGCPTR) emConvertToFlatAddr(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pDis, POP_PARAMETER pParam, RTGCPTR pvAddr)
{
    DIS_SELREG enmPrefixSeg = DISDetectSegReg(pDis, pParam);
    return SELMToFlat(pVM, enmPrefixSeg, pRegFrame, pvAddr);
}


#if defined(VBOX_STRICT) || defined(LOG_ENABLED)
/**
 * Get the mnemonic for the disassembled instruction.
 *
 * GC/R0 doesn't include the strings in the DIS tables because
 * of limited space.
 */
static const char *emGetMnemonic(PDISCPUSTATE pDis)
{
    switch (pDis->pCurInstr->opcode)
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
        case OP_LIDT:       return "LIdt";
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
        case OP_CMPXCHG:    return pDis->prefix & PREFIX_LOCK ? "Lock CmpXchg"   : "CmpXchg";
        case OP_CMPXCHG8B:  return pDis->prefix & PREFIX_LOCK ? "Lock CmpXchg8b" : "CmpXchg8b";

        default:
            Log(("Unknown opcode %d\n", pDis->pCurInstr->opcode));
            return "???";
    }
}
#endif /* VBOX_STRICT || LOG_ENABLED */


/**
 * XCHG instruction emulation.
 */
static int emInterpretXchg(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    OP_PARAMVAL param1, param2;

    /* Source to make DISQueryParamVal read the register value - ugly hack */
    int rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param1, &param1, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param2, &param2, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

#ifdef IN_RC
    if (TRPMHasTrap(pVCpu))
    {
        if (TRPMGetErrorCode(pVCpu) & X86_TRAP_PF_RW)
        {
#endif
            RTGCPTR pParam1 = 0, pParam2 = 0;
            uint64_t valpar1, valpar2;

            AssertReturn(pDis->param1.size == pDis->param2.size, VERR_EM_INTERPRETER);
            switch(param1.type)
            {
            case PARMTYPE_IMMEDIATE: /* register type is translated to this one too */
                valpar1 = param1.val.val64;
                break;

            case PARMTYPE_ADDRESS:
                pParam1 = (RTGCPTR)param1.val.val64;
                pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pDis, &pDis->param1, pParam1);
                EM_ASSERT_FAULT_RETURN(pParam1 == pvFault, VERR_EM_INTERPRETER);
                rc = emRamRead(pVM, pVCpu, pRegFrame, &valpar1, pParam1, param1.size);
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
                pParam2 = emConvertToFlatAddr(pVM, pRegFrame, pDis, &pDis->param2, pParam2);
                EM_ASSERT_FAULT_RETURN(pParam2 == pvFault, VERR_EM_INTERPRETER);
                rc = emRamRead(pVM, pVCpu, pRegFrame, &valpar2, pParam2, param2.size);
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
                        rc = DISWriteReg8(pRegFrame, pDis->param1.base.reg_gen,  (uint8_t )valpar2); break;
                case 2: rc = DISWriteReg16(pRegFrame, pDis->param1.base.reg_gen, (uint16_t)valpar2); break;
                case 4: rc = DISWriteReg32(pRegFrame, pDis->param1.base.reg_gen, (uint32_t)valpar2); break;
                case 8: rc = DISWriteReg64(pRegFrame, pDis->param1.base.reg_gen, valpar2); break;
                default: AssertFailedReturn(VERR_EM_INTERPRETER);
                }
                if (RT_FAILURE(rc))
                    return VERR_EM_INTERPRETER;
            }
            else
            {
                rc = emRamWrite(pVM, pVCpu, pRegFrame, pParam1, &valpar2, param1.size);
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
                        rc = DISWriteReg8(pRegFrame, pDis->param2.base.reg_gen,  (uint8_t )valpar1);    break;
                case 2: rc = DISWriteReg16(pRegFrame, pDis->param2.base.reg_gen, (uint16_t)valpar1);    break;
                case 4: rc = DISWriteReg32(pRegFrame, pDis->param2.base.reg_gen, (uint32_t)valpar1);    break;
                case 8: rc = DISWriteReg64(pRegFrame, pDis->param2.base.reg_gen, valpar1);              break;
                default: AssertFailedReturn(VERR_EM_INTERPRETER);
                }
                if (RT_FAILURE(rc))
                    return VERR_EM_INTERPRETER;
            }
            else
            {
                rc = emRamWrite(pVM, pVCpu, pRegFrame, pParam2, &valpar1, param2.size);
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
static int emInterpretIncDec(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize,
                             PFNEMULATEPARAM2 pfnEmulate)
{
    OP_PARAMVAL param1;

    int rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param1, &param1, PARAM_DEST);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

#ifdef IN_RC
    if (TRPMHasTrap(pVCpu))
    {
        if (TRPMGetErrorCode(pVCpu) & X86_TRAP_PF_RW)
        {
#endif
            RTGCPTR pParam1 = 0;
            uint64_t valpar1;

            if (param1.type == PARMTYPE_ADDRESS)
            {
                pParam1 = (RTGCPTR)param1.val.val64;
                pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pDis, &pDis->param1, pParam1);
#ifdef IN_RC
                /* Safety check (in theory it could cross a page boundary and fault there though) */
                EM_ASSERT_FAULT_RETURN(pParam1 == pvFault, VERR_EM_INTERPRETER);
#endif
                rc = emRamRead(pVM, pVCpu, pRegFrame,  &valpar1, pParam1, param1.size);
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
            rc = emRamWrite(pVM, pVCpu, pRegFrame, pParam1, &valpar1, param1.size);
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
static int emInterpretPop(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    Assert(pDis->mode != CPUMODE_64BIT);    /** @todo check */
    OP_PARAMVAL param1;
    int rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param1, &param1, PARAM_DEST);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

#ifdef IN_RC
    if (TRPMHasTrap(pVCpu))
    {
        if (TRPMGetErrorCode(pVCpu) & X86_TRAP_PF_RW)
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

            rc = emRamRead(pVM, pVCpu, pRegFrame, &valpar1, pStackVal, param1.size);
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
                if (    (pDis->param1.flags & USE_BASE)
                    &&  (pDis->param1.flags & (USE_REG_GEN16|USE_REG_GEN32))
                    &&  pDis->param1.base.reg_gen == USE_REG_ESP
                   )
                   pParam1 = (RTGCPTR)((RTGCUINTPTR)pParam1 + param1.size);

                pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pDis, &pDis->param1, pParam1);
                EM_ASSERT_FAULT_RETURN(pParam1 == pvFault || (RTGCPTR)pRegFrame->esp == pvFault, VERR_EM_INTERPRETER);
                rc = emRamWrite(pVM, pVCpu, pRegFrame, pParam1, &valpar1, param1.size);
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
static int emInterpretOrXorAnd(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize,
                               PFNEMULATEPARAM3 pfnEmulate)
{
    OP_PARAMVAL param1, param2;

    int rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param1, &param1, PARAM_DEST);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param2, &param2, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

#ifdef IN_RC
    if (TRPMHasTrap(pVCpu))
    {
        if (TRPMGetErrorCode(pVCpu) & X86_TRAP_PF_RW)
        {
#endif
            RTGCPTR  pParam1;
            uint64_t valpar1, valpar2;

            if (pDis->param1.size != pDis->param2.size)
            {
                if (pDis->param1.size < pDis->param2.size)
                {
                    AssertMsgFailed(("%s at %RGv parameter mismatch %d vs %d!!\n", emGetMnemonic(pDis), (RTGCPTR)pRegFrame->rip, pDis->param1.size, pDis->param2.size)); /* should never happen! */
                    return VERR_EM_INTERPRETER;
                }
                /* Or %Ev, Ib -> just a hack to save some space; the data width of the 1st parameter determines the real width */
                pDis->param2.size = pDis->param1.size;
                param2.size     = param1.size;
            }

            /* The destination is always a virtual address */
            if (param1.type == PARMTYPE_ADDRESS)
            {
                pParam1 = (RTGCPTR)param1.val.val64;
                pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pDis, &pDis->param1, pParam1);
                EM_ASSERT_FAULT_RETURN(pParam1 == pvFault, VERR_EM_INTERPRETER);
                rc = emRamRead(pVM, pVCpu, pRegFrame, &valpar1, pParam1, param1.size);
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

            LogFlow(("emInterpretOrXorAnd %s %RGv %RX64 - %RX64 size %d (%d)\n", emGetMnemonic(pDis), pParam1, valpar1, valpar2, param2.size, param1.size));

            /* Data read, emulate instruction. */
            uint32_t eflags = pfnEmulate(&valpar1, valpar2, param2.size);

            LogFlow(("emInterpretOrXorAnd %s result %RX64\n", emGetMnemonic(pDis), valpar1));

            /* Update guest's eflags and finish. */
            pRegFrame->eflags.u32 = (pRegFrame->eflags.u32 & ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF))
                                  | (eflags                &  (X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF));

            /* And write it back */
            rc = emRamWrite(pVM, pVCpu, pRegFrame, pParam1, &valpar1, param1.size);
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
static int emInterpretLockOrXorAnd(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault,
                                   uint32_t *pcbSize, PFNEMULATELOCKPARAM3 pfnEmulate)
{
    void *pvParam1;
    OP_PARAMVAL param1, param2;

#if HC_ARCH_BITS == 32 && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL_IN_R0)
    Assert(pDis->param1.size <= 4);
#endif

    int rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param1, &param1, PARAM_DEST);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param2, &param2, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    if (pDis->param1.size != pDis->param2.size)
    {
        AssertMsgReturn(pDis->param1.size >= pDis->param2.size, /* should never happen! */
                        ("%s at %RGv parameter mismatch %d vs %d!!\n", emGetMnemonic(pDis), (RTGCPTR)pRegFrame->rip, pDis->param1.size, pDis->param2.size),
                        VERR_EM_INTERPRETER);

        /* Or %Ev, Ib -> just a hack to save some space; the data width of the 1st parameter determines the real width */
        pDis->param2.size = pDis->param1.size;
        param2.size       = param1.size;
    }

#ifdef IN_RC
    /* Safety check (in theory it could cross a page boundary and fault there though) */
    Assert(   TRPMHasTrap(pVCpu)
           && (TRPMGetErrorCode(pVCpu) & X86_TRAP_PF_RW));
    EM_ASSERT_FAULT_RETURN(GCPtrPar1 == pvFault, VERR_EM_INTERPRETER);
#endif

    /* Register and immediate data == PARMTYPE_IMMEDIATE */
    AssertReturn(param2.type == PARMTYPE_IMMEDIATE, VERR_EM_INTERPRETER);
    RTGCUINTREG ValPar2 = param2.val.val64;

    /* The destination is always a virtual address */
    AssertReturn(param1.type == PARMTYPE_ADDRESS, VERR_EM_INTERPRETER);

    RTGCPTR GCPtrPar1 = param1.val.val64;
    GCPtrPar1 = emConvertToFlatAddr(pVM, pRegFrame, pDis, &pDis->param1, GCPtrPar1);
    PGMPAGEMAPLOCK Lock;
    rc = PGMPhysGCPtr2CCPtr(pVCpu, GCPtrPar1, &pvParam1, &Lock);
    AssertRCReturn(rc, VERR_EM_INTERPRETER);

    /* Try emulate it with a one-shot #PF handler in place. (RC) */
    Log2(("%s %RGv imm%d=%RX64\n", emGetMnemonic(pDis), GCPtrPar1, pDis->param2.size*8, ValPar2));

    RTGCUINTREG32 eflags = 0;
    rc = pfnEmulate(pvParam1, ValPar2, pDis->param2.size, &eflags);
    PGMPhysReleasePageMappingLock(pVM, &Lock);
    if (RT_FAILURE(rc))
    {
        Log(("%s %RGv imm%d=%RX64-> emulation failed due to page fault!\n", emGetMnemonic(pDis), GCPtrPar1, pDis->param2.size*8, ValPar2));
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
static int emInterpretAddSub(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize,
                             PFNEMULATEPARAM3 pfnEmulate)
{
    OP_PARAMVAL param1, param2;
    int rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param1, &param1, PARAM_DEST);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param2, &param2, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

#ifdef IN_RC
    if (TRPMHasTrap(pVCpu))
    {
        if (TRPMGetErrorCode(pVCpu) & X86_TRAP_PF_RW)
        {
#endif
            RTGCPTR  pParam1;
            uint64_t valpar1, valpar2;

            if (pDis->param1.size != pDis->param2.size)
            {
                if (pDis->param1.size < pDis->param2.size)
                {
                    AssertMsgFailed(("%s at %RGv parameter mismatch %d vs %d!!\n", emGetMnemonic(pDis), (RTGCPTR)pRegFrame->rip, pDis->param1.size, pDis->param2.size)); /* should never happen! */
                    return VERR_EM_INTERPRETER;
                }
                /* Or %Ev, Ib -> just a hack to save some space; the data width of the 1st parameter determines the real width */
                pDis->param2.size = pDis->param1.size;
                param2.size     = param1.size;
            }

            /* The destination is always a virtual address */
            if (param1.type == PARMTYPE_ADDRESS)
            {
                pParam1 = (RTGCPTR)param1.val.val64;
                pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pDis, &pDis->param1, pParam1);
                EM_ASSERT_FAULT_RETURN(pParam1 == pvFault, VERR_EM_INTERPRETER);
                rc = emRamRead(pVM, pVCpu, pRegFrame, &valpar1, pParam1, param1.size);
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
            rc = emRamWrite(pVM, pVCpu, pRegFrame, pParam1, &valpar1, param1.size);
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
static int emInterpretAdc(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    if (pRegFrame->eflags.Bits.u1CF)
        return emInterpretAddSub(pVM, pVCpu, pDis, pRegFrame, pvFault, pcbSize, EMEmulateAdcWithCarrySet);
    else
        return emInterpretAddSub(pVM, pVCpu, pDis, pRegFrame, pvFault, pcbSize, EMEmulateAdd);
}


/**
 * BTR/C/S Emulation.
 */
static int emInterpretBitTest(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize,
                              PFNEMULATEPARAM2UINT32 pfnEmulate)
{
    OP_PARAMVAL param1, param2;
    int rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param1, &param1, PARAM_DEST);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param2, &param2, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

#ifdef IN_RC
    if (TRPMHasTrap(pVCpu))
    {
        if (TRPMGetErrorCode(pVCpu) & X86_TRAP_PF_RW)
        {
#endif
            RTGCPTR  pParam1;
            uint64_t valpar1 = 0, valpar2;
            uint32_t eflags;

            /* The destination is always a virtual address */
            if (param1.type != PARMTYPE_ADDRESS)
                return VERR_EM_INTERPRETER;

            pParam1 = (RTGCPTR)param1.val.val64;
            pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pDis, &pDis->param1, pParam1);

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

            Log2(("emInterpret%s: pvFault=%RGv pParam1=%RGv val2=%x\n", emGetMnemonic(pDis), pvFault, pParam1, valpar2));
            pParam1 = (RTGCPTR)((RTGCUINTPTR)pParam1 + valpar2/8);
            EM_ASSERT_FAULT_RETURN((RTGCPTR)((RTGCUINTPTR)pParam1 & ~3) == pvFault, VERR_EM_INTERPRETER);
            rc = emRamRead(pVM, pVCpu, pRegFrame, &valpar1, pParam1, 1);
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
            rc = emRamWrite(pVM, pVCpu, pRegFrame, pParam1, &valpar1, 1);
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
static int emInterpretLockBitTest(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault,
                                  uint32_t *pcbSize, PFNEMULATELOCKPARAM2 pfnEmulate)
{
    void *pvParam1;

    OP_PARAMVAL param1, param2;
    int rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param1, &param1, PARAM_DEST);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param2, &param2, PARAM_SOURCE);
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

    GCPtrPar1 = emConvertToFlatAddr(pVM, pRegFrame, pDis, &pDis->param1, GCPtrPar1);
#ifdef IN_RC
    Assert(TRPMHasTrap(pVCpu));
    EM_ASSERT_FAULT_RETURN((RTGCPTR)((RTGCUINTPTR)GCPtrPar1 & ~(RTGCUINTPTR)3) == pvFault, VERR_EM_INTERPRETER);
#endif

    PGMPAGEMAPLOCK Lock;
    rc = PGMPhysGCPtr2CCPtr(pVCpu, GCPtrPar1, &pvParam1, &Lock);
    AssertRCReturn(rc, VERR_EM_INTERPRETER);

    Log2(("emInterpretLockBitTest %s: pvFault=%RGv GCPtrPar1=%RGv imm=%RX64\n", emGetMnemonic(pDis), pvFault, GCPtrPar1, ValPar2));

    /* Try emulate it with a one-shot #PF handler in place. (RC) */
    RTGCUINTREG32 eflags = 0;
    rc = pfnEmulate(pvParam1, ValPar2, &eflags);
    PGMPhysReleasePageMappingLock(pVM, &Lock);
    if (RT_FAILURE(rc))
    {
        Log(("emInterpretLockBitTest %s: %RGv imm%d=%RX64 -> emulation failed due to page fault!\n",
             emGetMnemonic(pDis), GCPtrPar1, pDis->param2.size*8, ValPar2));
        return VERR_EM_INTERPRETER;
    }

    Log2(("emInterpretLockBitTest %s: GCPtrPar1=%RGv imm=%RX64 CF=%d\n", emGetMnemonic(pDis), GCPtrPar1, ValPar2, !!(eflags & X86_EFL_CF)));

    /* Update guest's eflags and finish. */
    pRegFrame->eflags.u32 = (pRegFrame->eflags.u32 & ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF))
                          | (eflags                &  (X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF));

    *pcbSize = 1;
    return VINF_SUCCESS;
}


/**
 * MOV emulation.
 */
static int emInterpretMov(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    OP_PARAMVAL param1, param2;
    int rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param1, &param1, PARAM_DEST);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param2, &param2, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

#ifdef IN_RC
    if (TRPMHasTrap(pVCpu))
    {
        if (TRPMGetErrorCode(pVCpu) & X86_TRAP_PF_RW)
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
                pDest = emConvertToFlatAddr(pVM, pRegFrame, pDis, &pDis->param1, pDest);
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
            if (pDis->mode == CPUMODE_64BIT)
                LogFlow(("EMInterpretInstruction at %RGv: OP_MOV %RGv <- %RX64 (%d) &val64=%RHv\n", (RTGCPTR)pRegFrame->rip, pDest, val64, param2.size, &val64));
            else
                LogFlow(("EMInterpretInstruction at %08RX64: OP_MOV %RGv <- %08X  (%d) &val64=%RHv\n", pRegFrame->rip, pDest, (uint32_t)val64, param2.size, &val64));
#endif

            Assert(param2.size <= 8 && param2.size > 0);
            EM_ASSERT_FAULT_RETURN(pDest == pvFault, VERR_EM_INTERPRETER);
            rc = emRamWrite(pVM, pVCpu, pRegFrame, pDest, &val64, param2.size);
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
                pSrc = emConvertToFlatAddr(pVM, pRegFrame, pDis, &pDis->param2, pSrc);
                break;

            default:
                return VERR_EM_INTERPRETER;
            }

            Assert(param1.size <= 8 && param1.size > 0);
            EM_ASSERT_FAULT_RETURN(pSrc == pvFault, VERR_EM_INTERPRETER);
            rc = emRamRead(pVM, pVCpu, pRegFrame, &val64, pSrc, param1.size);
            if (RT_FAILURE(rc))
                return VERR_EM_INTERPRETER;

            /* Destination */
            switch(param1.type)
            {
            case PARMTYPE_REGISTER:
                switch(param1.size)
                {
                case 1: rc = DISWriteReg8(pRegFrame, pDis->param1.base.reg_gen,  (uint8_t) val64); break;
                case 2: rc = DISWriteReg16(pRegFrame, pDis->param1.base.reg_gen, (uint16_t)val64); break;
                case 4: rc = DISWriteReg32(pRegFrame, pDis->param1.base.reg_gen, (uint32_t)val64); break;
                case 8: rc = DISWriteReg64(pRegFrame, pDis->param1.base.reg_gen, val64); break;
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
            if (pDis->mode == CPUMODE_64BIT)
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
static int emInterpretStosWD(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    int      rc;
    RTGCPTR  GCDest, GCOffset;
    uint32_t cbSize;
    uint64_t cTransfers;
    int      offIncrement;

    /* Don't support any but these three prefix bytes. */
    if ((pDis->prefix & ~(PREFIX_ADDRSIZE|PREFIX_OPSIZE|PREFIX_REP|PREFIX_REX)))
        return VERR_EM_INTERPRETER;

    switch (pDis->addrmode)
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
    switch (pDis->opmode)
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

    if (!(pDis->prefix & PREFIX_REP))
    {
        LogFlow(("emInterpretStosWD dest=%04X:%RGv (%RGv) cbSize=%d\n", pRegFrame->es, GCOffset, GCDest, cbSize));

        rc = emRamWrite(pVM, pVCpu, pRegFrame, GCDest, &pRegFrame->rax, cbSize);
        if (RT_FAILURE(rc))
            return VERR_EM_INTERPRETER;
        Assert(rc == VINF_SUCCESS);

        /* Update (e/r)di. */
        switch (pDis->addrmode)
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
        rc = PGMVerifyAccess(pVCpu, GCDest - ((offIncrement > 0) ? 0 : ((cTransfers-1) * cbSize)),
                             cTransfers * cbSize,
                             X86_PTE_RW | (CPUMGetGuestCPL(pVCpu, pRegFrame) == 3 ? X86_PTE_US : 0));
        if (rc != VINF_SUCCESS)
        {
            Log(("STOSWD will generate a trap -> recompiler, rc=%d\n", rc));
            return VERR_EM_INTERPRETER;
        }

        /* REP case */
        while (cTransfers)
        {
            rc = emRamWrite(pVM, pVCpu, pRegFrame, GCDest, &pRegFrame->rax, cbSize);
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
        switch (pDis->addrmode)
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


/**
 * [LOCK] CMPXCHG emulation.
 */
static int emInterpretCmpXchg(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    OP_PARAMVAL param1, param2;

#if HC_ARCH_BITS == 32 && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL_IN_R0)
    Assert(pDis->param1.size <= 4);
#endif

    /* Source to make DISQueryParamVal read the register value - ugly hack */
    int rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param1, &param1, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param2, &param2, PARAM_SOURCE);
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

    AssertReturn(pDis->param1.size == pDis->param2.size, VERR_EM_INTERPRETER);
    switch(param1.type)
    {
    case PARMTYPE_ADDRESS:
        GCPtrPar1 = param1.val.val64;
        GCPtrPar1 = emConvertToFlatAddr(pVM, pRegFrame, pDis, &pDis->param1, GCPtrPar1);

        rc = PGMPhysGCPtr2CCPtr(pVCpu, GCPtrPar1, &pvParam1, &Lock);
        AssertRCReturn(rc, VERR_EM_INTERPRETER);
        break;

    default:
        return VERR_EM_INTERPRETER;
    }

    LogFlow(("%s %RGv rax=%RX64 %RX64\n", emGetMnemonic(pDis), GCPtrPar1, pRegFrame->rax, valpar));

    if (pDis->prefix & PREFIX_LOCK)
        eflags = EMEmulateLockCmpXchg(pvParam1, &pRegFrame->rax, valpar, pDis->param2.size);
    else
        eflags = EMEmulateCmpXchg(pvParam1, &pRegFrame->rax, valpar, pDis->param2.size);

    LogFlow(("%s %RGv rax=%RX64 %RX64 ZF=%d\n", emGetMnemonic(pDis), GCPtrPar1, pRegFrame->rax, valpar, !!(eflags & X86_EFL_ZF)));

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
static int emInterpretCmpXchg8b(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    Assert(pDis->mode != CPUMODE_64BIT);    /** @todo check */
    OP_PARAMVAL param1;

    /* Source to make DISQueryParamVal read the register value - ugly hack */
    int rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param1, &param1, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    RTGCPTR  GCPtrPar1;
    void    *pvParam1;
    uint64_t eflags;
    PGMPAGEMAPLOCK Lock;

    AssertReturn(pDis->param1.size == 8, VERR_EM_INTERPRETER);
    switch(param1.type)
    {
    case PARMTYPE_ADDRESS:
        GCPtrPar1 = param1.val.val64;
        GCPtrPar1 = emConvertToFlatAddr(pVM, pRegFrame, pDis, &pDis->param1, GCPtrPar1);

        rc = PGMPhysGCPtr2CCPtr(pVCpu, GCPtrPar1, &pvParam1, &Lock);
        AssertRCReturn(rc, VERR_EM_INTERPRETER);
        break;

    default:
        return VERR_EM_INTERPRETER;
    }

    LogFlow(("%s %RGv=%08x eax=%08x\n", emGetMnemonic(pDis), pvParam1, pRegFrame->eax));

    if (pDis->prefix & PREFIX_LOCK)
        eflags = EMEmulateLockCmpXchg8b(pvParam1, &pRegFrame->eax, &pRegFrame->edx, pRegFrame->ebx, pRegFrame->ecx);
    else
        eflags = EMEmulateCmpXchg8b(pvParam1, &pRegFrame->eax, &pRegFrame->edx, pRegFrame->ebx, pRegFrame->ecx);

    LogFlow(("%s %RGv=%08x eax=%08x ZF=%d\n", emGetMnemonic(pDis), pvParam1, pRegFrame->eax, !!(eflags & X86_EFL_ZF)));

    /* Update guest's eflags and finish; note that *only* ZF is affected. */
    pRegFrame->eflags.u32 =   (pRegFrame->eflags.u32 & ~(X86_EFL_ZF))
                            | (eflags                &  (X86_EFL_ZF));

    *pcbSize = 8;
    PGMPhysReleasePageMappingLock(pVM, &Lock);
    return VINF_SUCCESS;
}


#ifdef IN_RC /** @todo test+enable for HWACCM as well. */
/**
 * [LOCK] XADD emulation.
 */
static int emInterpretXAdd(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    Assert(pDis->mode != CPUMODE_64BIT);    /** @todo check */
    OP_PARAMVAL param1;
    void *pvParamReg2;
    size_t cbParamReg2;

    /* Source to make DISQueryParamVal read the register value - ugly hack */
    int rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param1, &param1, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamRegPtr(pRegFrame, pDis, &pDis->param2, &pvParamReg2, &cbParamReg2);
    Assert(cbParamReg2 <= 4);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

#ifdef IN_RC
    if (TRPMHasTrap(pVCpu))
    {
        if (TRPMGetErrorCode(pVCpu) & X86_TRAP_PF_RW)
        {
#endif
            RTGCPTR         GCPtrPar1;
            void           *pvParam1;
            uint32_t        eflags;
            PGMPAGEMAPLOCK  Lock;

            AssertReturn(pDis->param1.size == pDis->param2.size, VERR_EM_INTERPRETER);
            switch(param1.type)
            {
            case PARMTYPE_ADDRESS:
                GCPtrPar1 = emConvertToFlatAddr(pVM, pRegFrame, pDis, &pDis->param1, (RTRCUINTPTR)param1.val.val64);
#ifdef IN_RC
                EM_ASSERT_FAULT_RETURN(GCPtrPar1 == pvFault, VERR_EM_INTERPRETER);
#endif

                rc = PGMPhysGCPtr2CCPtr(pVCpu, GCPtrPar1, &pvParam1, &Lock);
                AssertRCReturn(rc, VERR_EM_INTERPRETER);
                break;

            default:
                return VERR_EM_INTERPRETER;
            }

            LogFlow(("XAdd %RGv=%p reg=%08llx\n", GCPtrPar1, pvParam1, *(uint64_t *)pvParamReg2));

            if (pDis->prefix & PREFIX_LOCK)
                eflags = EMEmulateLockXAdd(pvParam1, pvParamReg2, cbParamReg2);
            else
                eflags = EMEmulateXAdd(pvParam1, pvParamReg2, cbParamReg2);

            LogFlow(("XAdd %RGv=%p reg=%08llx ZF=%d\n", GCPtrPar1, pvParam1, *(uint64_t *)pvParamReg2, !!(eflags & X86_EFL_ZF) ));

            /* Update guest's eflags and finish. */
            pRegFrame->eflags.u32 = (pRegFrame->eflags.u32 & ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF))
                                  | (eflags                &  (X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF));

            *pcbSize = cbParamReg2;
            PGMPhysReleasePageMappingLock(pVM, &Lock);
            return VINF_SUCCESS;
#ifdef IN_RC
        }
    }

    return VERR_EM_INTERPRETER;
#endif
}
#endif /* IN_RC */


#ifdef IN_RC
/**
 * Interpret IRET (currently only to V86 code)
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU handle.
 * @param   pRegFrame   The register frame.
 *
 */
VMMDECL(int) EMInterpretIret(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame)
{
    RTGCUINTPTR pIretStack = (RTGCUINTPTR)pRegFrame->esp;
    RTGCUINTPTR eip, cs, esp, ss, eflags, ds, es, fs, gs, uMask;
    int         rc;

    Assert(!CPUMIsGuestIn64BitCode(pVCpu, pRegFrame));

    rc  = emRamRead(pVM, pVCpu, pRegFrame, &eip,      (RTGCPTR)pIretStack      , 4);
    rc |= emRamRead(pVM, pVCpu, pRegFrame, &cs,       (RTGCPTR)(pIretStack + 4), 4);
    rc |= emRamRead(pVM, pVCpu, pRegFrame, &eflags,   (RTGCPTR)(pIretStack + 8), 4);
    AssertRCReturn(rc, VERR_EM_INTERPRETER);
    AssertReturn(eflags & X86_EFL_VM, VERR_EM_INTERPRETER);

    rc |= emRamRead(pVM, pVCpu, pRegFrame, &esp,      (RTGCPTR)(pIretStack + 12), 4);
    rc |= emRamRead(pVM, pVCpu, pRegFrame, &ss,       (RTGCPTR)(pIretStack + 16), 4);
    rc |= emRamRead(pVM, pVCpu, pRegFrame, &es,       (RTGCPTR)(pIretStack + 20), 4);
    rc |= emRamRead(pVM, pVCpu, pRegFrame, &ds,       (RTGCPTR)(pIretStack + 24), 4);
    rc |= emRamRead(pVM, pVCpu, pRegFrame, &fs,       (RTGCPTR)(pIretStack + 28), 4);
    rc |= emRamRead(pVM, pVCpu, pRegFrame, &gs,       (RTGCPTR)(pIretStack + 32), 4);
    AssertRCReturn(rc, VERR_EM_INTERPRETER);

    pRegFrame->eip = eip & 0xffff;
    pRegFrame->cs  = cs;

    /* Mask away all reserved bits */
    uMask = X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_TF | X86_EFL_IF | X86_EFL_DF | X86_EFL_OF | X86_EFL_IOPL | X86_EFL_NT | X86_EFL_RF | X86_EFL_VM | X86_EFL_AC | X86_EFL_VIF | X86_EFL_VIP | X86_EFL_ID;
    eflags &= uMask;

#ifndef IN_RING0
    CPUMRawSetEFlags(pVCpu, pRegFrame, eflags);
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
static int emInterpretIret(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    /* only allow direct calls to EMInterpretIret for now */
    return VERR_EM_INTERPRETER;
}

/**
 * WBINVD Emulation.
 */
static int emInterpretWbInvd(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    /* Nothing to do. */
    return VINF_SUCCESS;
}


/**
 * Interpret INVLPG
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU handle.
 * @param   pRegFrame   The register frame.
 * @param   pAddrGC     Operand address
 *
 */
VMMDECL(VBOXSTRICTRC) EMInterpretInvlpg(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pAddrGC)
{
    /** @todo is addr always a flat linear address or ds based
     * (in absence of segment override prefixes)????
     */
#ifdef IN_RC
    LogFlow(("RC: EMULATE: invlpg %RGv\n", pAddrGC));
#endif
    VBOXSTRICTRC rc = PGMInvalidatePage(pVCpu, pAddrGC);
    if (    rc == VINF_SUCCESS
        ||  rc == VINF_PGM_SYNC_CR3 /* we can rely on the FF */)
        return VINF_SUCCESS;
    AssertMsgReturn(rc == VINF_EM_RAW_EMULATE_INSTR,
                    ("%Rrc addr=%RGv\n", VBOXSTRICTRC_VAL(rc), pAddrGC),
                    VERR_EM_INTERPRETER);
    return rc;
}


/**
 * INVLPG Emulation.
 */
static VBOXSTRICTRC emInterpretInvlPg(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    OP_PARAMVAL param1;
    RTGCPTR     addr;

    VBOXSTRICTRC rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param1, &param1, PARAM_SOURCE);
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
    rc = PGMInvalidatePage(pVCpu, addr);
    if (    rc == VINF_SUCCESS
        ||  rc == VINF_PGM_SYNC_CR3 /* we can rely on the FF */)
        return VINF_SUCCESS;
    AssertMsgReturn(rc == VINF_EM_RAW_EMULATE_INSTR,
                    ("%Rrc addr=%RGv\n", VBOXSTRICTRC_VAL(rc), addr),
                    VERR_EM_INTERPRETER);
    return rc;
}

/** @todo change all these EMInterpretXXX methods to VBOXSTRICTRC. */

/**
 * Interpret CPUID given the parameters in the CPU context
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU handle.
 * @param   pRegFrame   The register frame.
 *
 */
VMMDECL(int) EMInterpretCpuId(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame)
{
    uint32_t iLeaf = pRegFrame->eax;

    /* cpuid clears the high dwords of the affected 64 bits registers. */
    pRegFrame->rax = 0;
    pRegFrame->rbx = 0;
    pRegFrame->rcx &= UINT64_C(0x00000000ffffffff);
    pRegFrame->rdx = 0;

    /* Note: operates the same in 64 and non-64 bits mode. */
    CPUMGetGuestCpuId(pVCpu, iLeaf, &pRegFrame->eax, &pRegFrame->ebx, &pRegFrame->ecx, &pRegFrame->edx);
    Log(("Emulate: CPUID %x -> %08x %08x %08x %08x\n", iLeaf, pRegFrame->eax, pRegFrame->ebx, pRegFrame->ecx, pRegFrame->edx));
    return VINF_SUCCESS;
}


/**
 * CPUID Emulation.
 */
static int emInterpretCpuId(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    int rc = EMInterpretCpuId(pVM, pVCpu, pRegFrame);
    return rc;
}


/**
 * Interpret CRx read
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU handle.
 * @param   pRegFrame   The register frame.
 * @param   DestRegGen  General purpose register index (USE_REG_E**))
 * @param   SrcRegCRx   CRx register index (USE_REG_CR*)
 *
 */
VMMDECL(int) EMInterpretCRxRead(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, uint32_t DestRegGen, uint32_t SrcRegCrx)
{
    uint64_t val64;
    int rc = CPUMGetGuestCRx(pVCpu, SrcRegCrx, &val64);
    AssertMsgRCReturn(rc, ("CPUMGetGuestCRx %d failed\n", SrcRegCrx), VERR_EM_INTERPRETER);

    if (CPUMIsGuestIn64BitCode(pVCpu, pRegFrame))
        rc = DISWriteReg64(pRegFrame, DestRegGen, val64);
    else
        rc = DISWriteReg32(pRegFrame, DestRegGen, val64);

    if (RT_SUCCESS(rc))
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
 * @param   pVCpu       The VMCPU handle.
 *
 */
VMMDECL(int) EMInterpretCLTS(PVM pVM, PVMCPU pVCpu)
{
    uint64_t cr0 = CPUMGetGuestCR0(pVCpu);
    if (!(cr0 & X86_CR0_TS))
        return VINF_SUCCESS;
    return CPUMSetGuestCR0(pVCpu, cr0 & ~X86_CR0_TS);
}

/**
 * CLTS Emulation.
 */
static int emInterpretClts(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    return EMInterpretCLTS(pVM, pVCpu);
}


/**
 * Update CRx
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU handle.
 * @param   pRegFrame   The register frame.
 * @param   DestRegCRx  CRx register index (USE_REG_CR*)
 * @param   val         New CRx value
 *
 */
static int emUpdateCRx(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, uint32_t DestRegCrx, uint64_t val)
{
    uint64_t oldval;
    uint64_t msrEFER;
    int      rc, rc2;

    /** @todo Clean up this mess. */
    LogFlow(("EMInterpretCRxWrite at %RGv CR%d <- %RX64\n", (RTGCPTR)pRegFrame->rip, DestRegCrx, val));
    switch (DestRegCrx)
    {
    case USE_REG_CR0:
        oldval = CPUMGetGuestCR0(pVCpu);
#ifdef IN_RC
        /* CR0.WP and CR0.AM changes require a reschedule run in ring 3. */
        if (    (val    & (X86_CR0_WP | X86_CR0_AM))
            !=  (oldval & (X86_CR0_WP | X86_CR0_AM)))
            return VERR_EM_INTERPRETER;
#endif
        rc = VINF_SUCCESS;
        CPUMSetGuestCR0(pVCpu, val);
        val = CPUMGetGuestCR0(pVCpu);
        if (    (oldval & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE))
            !=  (val    & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE)))
        {
            /* global flush */
            rc = PGMFlushTLB(pVCpu, CPUMGetGuestCR3(pVCpu), true /* global */);
            AssertRCReturn(rc, rc);
        }

        /* Deal with long mode enabling/disabling. */
        msrEFER = CPUMGetGuestEFER(pVCpu);
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
                if (!(CPUMGetGuestCR4(pVCpu) & X86_CR4_PAE))
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
            CPUMSetGuestEFER(pVCpu, msrEFER);
        }
        rc2 = PGMChangeMode(pVCpu, CPUMGetGuestCR0(pVCpu), CPUMGetGuestCR4(pVCpu), CPUMGetGuestEFER(pVCpu));
        return rc2 == VINF_SUCCESS ? rc : rc2;

    case USE_REG_CR2:
        rc = CPUMSetGuestCR2(pVCpu, val); AssertRC(rc);
        return VINF_SUCCESS;

    case USE_REG_CR3:
        /* Reloading the current CR3 means the guest just wants to flush the TLBs */
        rc = CPUMSetGuestCR3(pVCpu, val); AssertRC(rc);
        if (CPUMGetGuestCR0(pVCpu) & X86_CR0_PG)
        {
            /* flush */
            rc = PGMFlushTLB(pVCpu, val, !(CPUMGetGuestCR4(pVCpu) & X86_CR4_PGE));
            AssertRC(rc);
        }
        return rc;

    case USE_REG_CR4:
        oldval = CPUMGetGuestCR4(pVCpu);
        rc = CPUMSetGuestCR4(pVCpu, val); AssertRC(rc);
        val = CPUMGetGuestCR4(pVCpu);

        /* Illegal to disable PAE when long mode is active. (AMD Arch. Programmer's Manual Volume 2: Table 14-5) */
        msrEFER = CPUMGetGuestEFER(pVCpu);
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
            rc = PGMFlushTLB(pVCpu, CPUMGetGuestCR3(pVCpu), true /* global */);
            AssertRCReturn(rc, rc);
        }

        /* Feeling extremely lazy. */
# ifdef IN_RC
        if (    (oldval & (X86_CR4_OSFSXR|X86_CR4_OSXMMEEXCPT|X86_CR4_PCE|X86_CR4_MCE|X86_CR4_PAE|X86_CR4_DE|X86_CR4_TSD|X86_CR4_PVI|X86_CR4_VME))
            !=  (val    & (X86_CR4_OSFSXR|X86_CR4_OSXMMEEXCPT|X86_CR4_PCE|X86_CR4_MCE|X86_CR4_PAE|X86_CR4_DE|X86_CR4_TSD|X86_CR4_PVI|X86_CR4_VME)))
        {
            Log(("emInterpretMovCRx: CR4: %#RX64->%#RX64 => R3\n", oldval, val));
            VMCPU_FF_SET(pVCpu, VMCPU_FF_TO_R3);
        }
# endif
        if ((val ^ oldval) & X86_CR4_VME)
            VMCPU_FF_SET(pVCpu, VMCPU_FF_SELM_SYNC_TSS);

        rc2 = PGMChangeMode(pVCpu, CPUMGetGuestCR0(pVCpu), CPUMGetGuestCR4(pVCpu), CPUMGetGuestEFER(pVCpu));
        return rc2 == VINF_SUCCESS ? rc : rc2;

    case USE_REG_CR8:
        return PDMApicSetTPR(pVCpu, val << 4);  /* cr8 bits 3-0 correspond to bits 7-4 of the task priority mmio register. */

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
 * @param   pVCpu       The VMCPU handle.
 * @param   pRegFrame   The register frame.
 * @param   DestRegCRx  CRx register index (USE_REG_CR*)
 * @param   SrcRegGen   General purpose register index (USE_REG_E**))
 *
 */
VMMDECL(int) EMInterpretCRxWrite(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, uint32_t DestRegCrx, uint32_t SrcRegGen)
{
    uint64_t val;
    int      rc;

    if (CPUMIsGuestIn64BitCode(pVCpu, pRegFrame))
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
        return emUpdateCRx(pVM, pVCpu, pRegFrame, DestRegCrx, val);

    return VERR_EM_INTERPRETER;
}

/**
 * Interpret LMSW
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU handle.
 * @param   pRegFrame   The register frame.
 * @param   u16Data     LMSW source data.
 *
 */
VMMDECL(int) EMInterpretLMSW(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, uint16_t u16Data)
{
    uint64_t OldCr0 = CPUMGetGuestCR0(pVCpu);

    /* Only PE, MP, EM and TS can be changed; note that PE can't be cleared by this instruction. */
    uint64_t NewCr0 = ( OldCr0 & ~(             X86_CR0_MP | X86_CR0_EM | X86_CR0_TS))
                    | (u16Data &  (X86_CR0_PE | X86_CR0_MP | X86_CR0_EM | X86_CR0_TS));

    return emUpdateCRx(pVM, pVCpu, pRegFrame, USE_REG_CR0, NewCr0);
}

/**
 * LMSW Emulation.
 */
static int emInterpretLmsw(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    OP_PARAMVAL param1;
    uint32_t    val;

    int rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param1, &param1, PARAM_SOURCE);
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
    return EMInterpretLMSW(pVM, pVCpu, pRegFrame, val);
}

#ifdef EM_EMULATE_SMSW
/**
 * SMSW Emulation.
 */
static int emInterpretSmsw(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    OP_PARAMVAL param1;
    uint64_t    cr0 = CPUMGetGuestCR0(pVCpu);

    int rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param1, &param1, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    switch(param1.type)
    {
    case PARMTYPE_IMMEDIATE:
        if(param1.size != sizeof(uint16_t))
            return VERR_EM_INTERPRETER;
        LogFlow(("emInterpretSmsw %d <- cr0 (%x)\n", pDis->param1.base.reg_gen, cr0));
        rc = DISWriteReg16(pRegFrame, pDis->param1.base.reg_gen, cr0);
        break;

    case PARMTYPE_ADDRESS:
    {
        RTGCPTR pParam1;

        /* Actually forced to 16 bits regardless of the operand size. */
        if(param1.size != sizeof(uint16_t))
            return VERR_EM_INTERPRETER;

        pParam1 = (RTGCPTR)param1.val.val64;
        pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pDis, &pDis->param1, pParam1);
        LogFlow(("emInterpretSmsw %RGv <- cr0 (%x)\n", pParam1, cr0));

        rc = emRamWrite(pVM, pVCpu, pRegFrame, pParam1, &cr0, sizeof(uint16_t));
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
static int emInterpretMovCRx(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    if ((pDis->param1.flags == USE_REG_GEN32 || pDis->param1.flags == USE_REG_GEN64) && pDis->param2.flags == USE_REG_CR)
        return EMInterpretCRxRead(pVM, pVCpu, pRegFrame, pDis->param1.base.reg_gen, pDis->param2.base.reg_ctrl);

    if (pDis->param1.flags == USE_REG_CR && (pDis->param2.flags == USE_REG_GEN32 || pDis->param2.flags == USE_REG_GEN64))
        return EMInterpretCRxWrite(pVM, pVCpu, pRegFrame, pDis->param1.base.reg_ctrl, pDis->param2.base.reg_gen);

    AssertMsgFailedReturn(("Unexpected control register move\n"), VERR_EM_INTERPRETER);
    return VERR_EM_INTERPRETER;
}


/**
 * Interpret DRx write
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU handle.
 * @param   pRegFrame   The register frame.
 * @param   DestRegDRx  DRx register index (USE_REG_DR*)
 * @param   SrcRegGen   General purpose register index (USE_REG_E**))
 *
 */
VMMDECL(int) EMInterpretDRxWrite(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, uint32_t DestRegDrx, uint32_t SrcRegGen)
{
    uint64_t val;
    int      rc;

    if (CPUMIsGuestIn64BitCode(pVCpu, pRegFrame))
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
        rc = CPUMSetGuestDRx(pVCpu, DestRegDrx, val);
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
 * @param   pVCpu       The VMCPU handle.
 * @param   pRegFrame   The register frame.
 * @param   DestRegGen  General purpose register index (USE_REG_E**))
 * @param   SrcRegDRx   DRx register index (USE_REG_DR*)
 *
 */
VMMDECL(int) EMInterpretDRxRead(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, uint32_t DestRegGen, uint32_t SrcRegDrx)
{
    uint64_t val64;

    int rc = CPUMGetGuestDRx(pVCpu, SrcRegDrx, &val64);
    AssertMsgRCReturn(rc, ("CPUMGetGuestDRx %d failed\n", SrcRegDrx), VERR_EM_INTERPRETER);
    if (CPUMIsGuestIn64BitCode(pVCpu, pRegFrame))
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
static int emInterpretMovDRx(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    int rc = VERR_EM_INTERPRETER;

    if((pDis->param1.flags == USE_REG_GEN32 || pDis->param1.flags == USE_REG_GEN64) && pDis->param2.flags == USE_REG_DBG)
    {
        rc = EMInterpretDRxRead(pVM, pVCpu, pRegFrame, pDis->param1.base.reg_gen, pDis->param2.base.reg_dbg);
    }
    else
    if(pDis->param1.flags == USE_REG_DBG && (pDis->param2.flags == USE_REG_GEN32 || pDis->param2.flags == USE_REG_GEN64))
    {
        rc = EMInterpretDRxWrite(pVM, pVCpu, pRegFrame, pDis->param1.base.reg_dbg, pDis->param2.base.reg_gen);
    }
    else
        AssertMsgFailed(("Unexpected debug register move\n"));

    return rc;
}


/**
 * LLDT Emulation.
 */
static int emInterpretLLdt(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    OP_PARAMVAL param1;
    RTSEL       sel;

    int rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param1, &param1, PARAM_SOURCE);
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
    AssertReturn(CPUMIsGuestInRealMode(pVCpu), VERR_EM_INTERPRETER);
    CPUMSetGuestLDTR(pVCpu, sel);
    return VINF_SUCCESS;
#else
    if (sel == 0)
    {
        if (CPUMGetHyperLDTR(pVCpu) == 0)
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
static int emInterpretLIGdt(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    OP_PARAMVAL param1;
    RTGCPTR     pParam1;
    X86XDTR32   dtr32;

    Log(("Emulate %s at %RGv\n", emGetMnemonic(pDis), (RTGCPTR)pRegFrame->rip));

    /* Only for the VT-x real-mode emulation case. */
    AssertReturn(CPUMIsGuestInRealMode(pVCpu), VERR_EM_INTERPRETER);

    int rc = DISQueryParamVal(pRegFrame, pDis, &pDis->param1, &param1, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    switch(param1.type)
    {
    case PARMTYPE_ADDRESS:
        pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pDis, &pDis->param1, param1.val.val16);
        break;

    default:
        return VERR_EM_INTERPRETER;
    }

    rc = emRamRead(pVM, pVCpu, pRegFrame, &dtr32, pParam1, sizeof(dtr32));
    AssertRCReturn(rc, VERR_EM_INTERPRETER);

    if (!(pDis->prefix & PREFIX_OPSIZE))
        dtr32.uAddr &= 0xffffff; /* 16 bits operand size */

    if (pDis->pCurInstr->opcode == OP_LIDT)
        CPUMSetGuestIDTR(pVCpu, dtr32.uAddr, dtr32.cb);
    else
        CPUMSetGuestGDTR(pVCpu, dtr32.uAddr, dtr32.cb);

    return VINF_SUCCESS;
}
#endif


#ifdef IN_RC
/**
 * STI Emulation.
 *
 * @remark the instruction following sti is guaranteed to be executed before any interrupts are dispatched
 */
static int emInterpretSti(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
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

    pVCpu->em.s.GCPtrInhibitInterrupts = pRegFrame->eip + pDis->opsize;
    VMCPU_FF_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);

    return VINF_SUCCESS;
}
#endif /* IN_RC */


/**
 * HLT Emulation.
 */
static VBOXSTRICTRC
emInterpretHlt(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    return VINF_EM_HALT;
}


/**
 * Interpret RDTSC
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU handle.
 * @param   pRegFrame   The register frame.
 *
 */
VMMDECL(int) EMInterpretRdtsc(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame)
{
    unsigned uCR4 = CPUMGetGuestCR4(pVCpu);

    if (uCR4 & X86_CR4_TSD)
        return VERR_EM_INTERPRETER; /* genuine #GP */

    uint64_t uTicks = TMCpuTickGet(pVCpu);

    /* Same behaviour in 32 & 64 bits mode */
    pRegFrame->rax = (uint32_t)uTicks;
    pRegFrame->rdx = (uTicks >> 32ULL);

    return VINF_SUCCESS;
}

/**
 * Interpret RDTSCP
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU handle.
 * @param   pCtx        The CPU context.
 *
 */
VMMDECL(int) EMInterpretRdtscp(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    unsigned uCR4 = CPUMGetGuestCR4(pVCpu);

    if (!CPUMGetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_RDTSCP))
    {
        AssertFailed();
        return VERR_EM_INTERPRETER; /* genuine #UD */
    }

    if (uCR4 & X86_CR4_TSD)
        return VERR_EM_INTERPRETER; /* genuine #GP */

    uint64_t uTicks = TMCpuTickGet(pVCpu);

    /* Same behaviour in 32 & 64 bits mode */
    pCtx->rax = (uint32_t)uTicks;
    pCtx->rdx = (uTicks >> 32ULL);
    /* Low dword of the TSC_AUX msr only. */
    CPUMQueryGuestMsr(pVCpu, MSR_K8_TSC_AUX, &pCtx->rcx);
    pCtx->rcx &= UINT32_C(0xffffffff);

    return VINF_SUCCESS;
}

/**
 * RDTSC Emulation.
 */
static int emInterpretRdtsc(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    return EMInterpretRdtsc(pVM, pVCpu, pRegFrame);
}

/**
 * Interpret RDPMC
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU handle.
 * @param   pRegFrame   The register frame.
 *
 */
VMMDECL(int) EMInterpretRdpmc(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame)
{
    unsigned uCR4 = CPUMGetGuestCR4(pVCpu);

    /* If X86_CR4_PCE is not set, then CPL must be zero. */
    if (    !(uCR4 & X86_CR4_PCE)
        &&  CPUMGetGuestCPL(pVCpu, pRegFrame) != 0)
    {
        Assert(CPUMGetGuestCR0(pVCpu) & X86_CR0_PE);
        return VERR_EM_INTERPRETER; /* genuine #GP */
    }

    /* Just return zero here; rather tricky to properly emulate this, especially as the specs are a mess. */
    pRegFrame->rax = 0;
    pRegFrame->rdx = 0;
    /** @todo We should trigger a #GP here if the cpu doesn't support the index in ecx. */
    return VINF_SUCCESS;
}

/**
 * RDPMC Emulation
 */
static int emInterpretRdpmc(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    return EMInterpretRdpmc(pVM, pVCpu, pRegFrame);
}

/**
 * MONITOR Emulation.
 */
VMMDECL(int) EMInterpretMonitor(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame)
{
    uint32_t u32Dummy, u32ExtFeatures, cpl;

    if (pRegFrame->ecx != 0)
    {
        Log(("emInterpretMonitor: unexpected ecx=%x -> recompiler!!\n", pRegFrame->ecx));
        return VERR_EM_INTERPRETER; /* illegal value. */
    }

    /* Get the current privilege level. */
    cpl = CPUMGetGuestCPL(pVCpu, pRegFrame);
    if (cpl != 0)
        return VERR_EM_INTERPRETER; /* supervisor only */

    CPUMGetGuestCpuId(pVCpu, 1, &u32Dummy, &u32Dummy, &u32ExtFeatures, &u32Dummy);
    if (!(u32ExtFeatures & X86_CPUID_FEATURE_ECX_MONITOR))
        return VERR_EM_INTERPRETER; /* not supported */

    pVCpu->em.s.mwait.uMonitorEAX = pRegFrame->rax;
    pVCpu->em.s.mwait.uMonitorECX = pRegFrame->rcx;
    pVCpu->em.s.mwait.uMonitorEDX = pRegFrame->rdx;
    pVCpu->em.s.mwait.fWait |= EMMWAIT_FLAG_MONITOR_ACTIVE;
    return VINF_SUCCESS;
}

static int emInterpretMonitor(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    return EMInterpretMonitor(pVM, pVCpu, pRegFrame);
}


/**
 * MWAIT Emulation.
 */
VMMDECL(VBOXSTRICTRC) EMInterpretMWait(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame)
{
    uint32_t u32Dummy, u32ExtFeatures, cpl, u32MWaitFeatures;

    /* Get the current privilege level. */
    cpl = CPUMGetGuestCPL(pVCpu, pRegFrame);
    if (cpl != 0)
        return VERR_EM_INTERPRETER; /* supervisor only */

    CPUMGetGuestCpuId(pVCpu, 1, &u32Dummy, &u32Dummy, &u32ExtFeatures, &u32Dummy);
    if (!(u32ExtFeatures & X86_CPUID_FEATURE_ECX_MONITOR))
        return VERR_EM_INTERPRETER; /* not supported */

    /*
     * CPUID.05H.ECX[0] defines support for power management extensions (eax)
     * CPUID.05H.ECX[1] defines support for interrupts as break events for mwait even when IF=0
     */
    CPUMGetGuestCpuId(pVCpu, 5, &u32Dummy, &u32Dummy, &u32MWaitFeatures, &u32Dummy);
    if (pRegFrame->ecx > 1)
    {
        Log(("EMInterpretMWait: unexpected ecx value %x -> recompiler\n", pRegFrame->ecx));
        return VERR_EM_INTERPRETER; /* illegal value. */
    }

    if (pRegFrame->ecx)
    {
        if (!(u32MWaitFeatures & X86_CPUID_MWAIT_ECX_BREAKIRQIF0))
        {
            Log(("EMInterpretMWait: unsupported X86_CPUID_MWAIT_ECX_BREAKIRQIF0 -> recompiler\n"));
            return VERR_EM_INTERPRETER; /* illegal value. */
        }

        pVCpu->em.s.mwait.fWait = EMMWAIT_FLAG_ACTIVE | EMMWAIT_FLAG_BREAKIRQIF0;
    }
    else
        pVCpu->em.s.mwait.fWait = EMMWAIT_FLAG_ACTIVE;

    pVCpu->em.s.mwait.uMWaitEAX = pRegFrame->rax;
    pVCpu->em.s.mwait.uMWaitECX = pRegFrame->rcx;

    /** @todo not completely correct */
    return VINF_EM_HALT;
}

static VBOXSTRICTRC emInterpretMWait(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    return EMInterpretMWait(pVM, pVCpu, pRegFrame);
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
    case MSR_IA32_MISC_ENABLE:
        return "MSR_IA32_MISC_ENABLE";
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
        return "MSR_IA32_PERF_STATUS";
    case MSR_IA32_PLATFORM_INFO:
        return "MSR_IA32_PLATFORM_INFO";
    case MSR_IA32_PERF_CTL:
        return "Unsupported MSR_IA32_PERF_CTL";
    case MSR_K7_PERFCTR0:
        return "Unsupported MSR_K7_PERFCTR0";
    case MSR_K7_PERFCTR1:
        return "Unsupported MSR_K7_PERFCTR1";
    case MSR_K7_PERFCTR2:
        return "Unsupported MSR_K7_PERFCTR2";
    case MSR_K7_PERFCTR3:
        return "Unsupported MSR_K7_PERFCTR3";
    case MSR_IA32_PMC0:
        return "Unsupported MSR_IA32_PMC0";
    case MSR_IA32_PMC1:
        return "Unsupported MSR_IA32_PMC1";
    case MSR_IA32_PMC2:
        return "Unsupported MSR_IA32_PMC2";
    case MSR_IA32_PMC3:
        return "Unsupported MSR_IA32_PMC3";
    }
    return "Unknown MSR";
}
#endif /* LOG_ENABLED */


/**
 * Interpret RDMSR
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU handle.
 * @param   pRegFrame   The register frame.
 */
VMMDECL(int) EMInterpretRdmsr(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame)
{
    /** @todo According to the Intel manuals, there's a REX version of RDMSR that is slightly different.
     *  That version clears the high dwords of both RDX & RAX */

    /* Get the current privilege level. */
    if (CPUMGetGuestCPL(pVCpu, pRegFrame) != 0)
        return VERR_EM_INTERPRETER; /* supervisor only */

    uint64_t uValue;
    int rc = CPUMQueryGuestMsr(pVCpu, pRegFrame->ecx, &uValue);
    if (RT_UNLIKELY(rc != VINF_SUCCESS))
    {
        Assert(rc == VERR_CPUM_RAISE_GP_0);
        return VERR_EM_INTERPRETER;
    }
    pRegFrame->rax = (uint32_t) uValue;
    pRegFrame->rdx = (uint32_t)(uValue >> 32);
    LogFlow(("EMInterpretRdmsr %s (%x) -> %RX64\n", emMSRtoString(pRegFrame->ecx), pRegFrame->ecx, uValue));
    return rc;
}


/**
 * RDMSR Emulation.
 */
static int emInterpretRdmsr(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    /* Note: The Intel manual claims there's a REX version of RDMSR that's slightly
             different, so we play safe by completely disassembling the instruction. */
    Assert(!(pDis->prefix & PREFIX_REX));
    return EMInterpretRdmsr(pVM, pVCpu, pRegFrame);
}


/**
 * Interpret WRMSR
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU handle.
 * @param   pRegFrame   The register frame.
 */
VMMDECL(int) EMInterpretWrmsr(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame)
{
    /* Check the current privilege level, this instruction is supervisor only. */
    if (CPUMGetGuestCPL(pVCpu, pRegFrame) != 0)
        return VERR_EM_INTERPRETER; /** @todo raise \#GP(0) */

    int rc = CPUMSetGuestMsr(pVCpu, pRegFrame->ecx, RT_MAKE_U64(pRegFrame->eax, pRegFrame->edx));
    if (rc != VINF_SUCCESS)
    {
        Assert(rc == VERR_CPUM_RAISE_GP_0);
        return VERR_EM_INTERPRETER;
    }
    LogFlow(("EMInterpretWrmsr %s (%x) val=%RX64\n", emMSRtoString(pRegFrame->ecx), pRegFrame->ecx,
             RT_MAKE_U64(pRegFrame->eax, pRegFrame->edx)));
    return rc;
}


/**
 * WRMSR Emulation.
 */
static int emInterpretWrmsr(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    return EMInterpretWrmsr(pVM, pVCpu, pRegFrame);
}


/**
 * Internal worker.
 * @copydoc EMInterpretInstructionCPU
 */
DECLINLINE(VBOXSTRICTRC) emInterpretInstructionCPU(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame,
                                                   RTGCPTR pvFault, EMCODETYPE enmCodeType, uint32_t *pcbSize)
{
    Assert(enmCodeType == EMCODETYPE_SUPERVISOR || enmCodeType == EMCODETYPE_ALL);
    Assert(pcbSize);
    *pcbSize = 0;

    if (enmCodeType == EMCODETYPE_SUPERVISOR)
    {
        /*
         * Only supervisor guest code!!
         * And no complicated prefixes.
         */
        /* Get the current privilege level. */
        uint32_t cpl = CPUMGetGuestCPL(pVCpu, pRegFrame);
        if (    cpl != 0
            &&  pDis->pCurInstr->opcode != OP_RDTSC)    /* rdtsc requires emulation in ring 3 as well */
        {
            Log(("WARNING: refusing instruction emulation for user-mode code!!\n"));
            STAM_COUNTER_INC(&pVCpu->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,FailedUserMode));
            return VERR_EM_INTERPRETER;
        }
    }
    else
        Log2(("emInterpretInstructionCPU allowed to interpret user-level code!!\n"));

#ifdef IN_RC
    if (    (pDis->prefix & (PREFIX_REPNE | PREFIX_REP))
        ||  (   (pDis->prefix & PREFIX_LOCK)
             && pDis->pCurInstr->opcode != OP_CMPXCHG
             && pDis->pCurInstr->opcode != OP_CMPXCHG8B
             && pDis->pCurInstr->opcode != OP_XADD
             && pDis->pCurInstr->opcode != OP_OR
             && pDis->pCurInstr->opcode != OP_AND
             && pDis->pCurInstr->opcode != OP_XOR
             && pDis->pCurInstr->opcode != OP_BTR
            )
       )
#else
    if (    (pDis->prefix & PREFIX_REPNE)
        ||  (   (pDis->prefix & PREFIX_REP)
             && pDis->pCurInstr->opcode != OP_STOSWD
            )
        ||  (   (pDis->prefix & PREFIX_LOCK)
             && pDis->pCurInstr->opcode != OP_OR
             && pDis->pCurInstr->opcode != OP_AND
             && pDis->pCurInstr->opcode != OP_XOR
             && pDis->pCurInstr->opcode != OP_BTR
             && pDis->pCurInstr->opcode != OP_CMPXCHG
             && pDis->pCurInstr->opcode != OP_CMPXCHG8B
            )
       )
#endif
    {
        //Log(("EMInterpretInstruction: wrong prefix!!\n"));
        STAM_COUNTER_INC(&pVCpu->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,FailedPrefix));
        return VERR_EM_INTERPRETER;
    }

#if HC_ARCH_BITS == 32
    /*
     * Unable to emulate most >4 bytes accesses in 32 bits mode.
     * Whitelisted instructions are safe.
     */
    if (    pDis->param1.size > 4
        &&  CPUMIsGuestIn64BitCode(pVCpu, pRegFrame))
    {
        uint32_t uOpCode = pDis->pCurInstr->opcode;
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
            switch (pDis->pCurInstr->opcode)
            {
#  define INTERPRET_FAILED_CASE(opcode, Instr) \
                case opcode: STAM_COUNTER_INC(&pVCpu->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Failed##Instr)); break;
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
                    STAM_COUNTER_INC(&pVCpu->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,FailedMisc));
                    break;
            }
# endif /* VBOX_WITH_STATISTICS */
            return VERR_EM_INTERPRETER;
        }
    }
#endif

    VBOXSTRICTRC rc;
#if (defined(VBOX_STRICT) || defined(LOG_ENABLED))
    LogFlow(("emInterpretInstructionCPU %s\n", emGetMnemonic(pDis)));
#endif
    switch (pDis->pCurInstr->opcode)
    {
        /*
         * Macros for generating the right case statements.
         */
# define INTERPRET_CASE_EX_LOCK_PARAM3(opcode, Instr, InstrFn, pfnEmulate, pfnEmulateLock) \
        case opcode:\
            if (pDis->prefix & PREFIX_LOCK) \
                rc = emInterpretLock##InstrFn(pVM, pVCpu, pDis, pRegFrame, pvFault, pcbSize, pfnEmulateLock); \
            else \
                rc = emInterpret##InstrFn(pVM, pVCpu, pDis, pRegFrame, pvFault, pcbSize, pfnEmulate); \
            if (RT_SUCCESS(rc)) \
                STAM_COUNTER_INC(&pVCpu->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Instr)); \
            else \
                STAM_COUNTER_INC(&pVCpu->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Failed##Instr)); \
            return rc
#define INTERPRET_CASE_EX_PARAM3(opcode, Instr, InstrFn, pfnEmulate) \
        case opcode:\
            rc = emInterpret##InstrFn(pVM, pVCpu, pDis, pRegFrame, pvFault, pcbSize, pfnEmulate); \
            if (RT_SUCCESS(rc)) \
                STAM_COUNTER_INC(&pVCpu->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Instr)); \
            else \
                STAM_COUNTER_INC(&pVCpu->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Failed##Instr)); \
            return rc

#define INTERPRET_CASE_EX_PARAM2(opcode, Instr, InstrFn, pfnEmulate) \
            INTERPRET_CASE_EX_PARAM3(opcode, Instr, InstrFn, pfnEmulate)
#define INTERPRET_CASE_EX_LOCK_PARAM2(opcode, Instr, InstrFn, pfnEmulate, pfnEmulateLock) \
            INTERPRET_CASE_EX_LOCK_PARAM3(opcode, Instr, InstrFn, pfnEmulate, pfnEmulateLock)

#define INTERPRET_CASE(opcode, Instr) \
        case opcode:\
            rc = emInterpret##Instr(pVM, pVCpu, pDis, pRegFrame, pvFault, pcbSize); \
            if (RT_SUCCESS(rc)) \
                STAM_COUNTER_INC(&pVCpu->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Instr)); \
            else \
                STAM_COUNTER_INC(&pVCpu->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Failed##Instr)); \
            return rc

#define INTERPRET_CASE_EX_DUAL_PARAM2(opcode, Instr, InstrFn) \
        case opcode:\
            rc = emInterpret##InstrFn(pVM, pVCpu, pDis, pRegFrame, pvFault, pcbSize); \
            if (RT_SUCCESS(rc)) \
                STAM_COUNTER_INC(&pVCpu->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Instr)); \
            else \
                STAM_COUNTER_INC(&pVCpu->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Failed##Instr)); \
            return rc

#define INTERPRET_STAT_CASE(opcode, Instr) \
        case opcode: STAM_COUNTER_INC(&pVCpu->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,Failed##Instr)); return VERR_EM_INTERPRETER;

        /*
         * The actual case statements.
         */
        INTERPRET_CASE(OP_XCHG,Xchg);
        INTERPRET_CASE_EX_PARAM2(OP_DEC,Dec, IncDec, EMEmulateDec);
        INTERPRET_CASE_EX_PARAM2(OP_INC,Inc, IncDec, EMEmulateInc);
        INTERPRET_CASE(OP_POP,Pop);
        INTERPRET_CASE_EX_LOCK_PARAM3(OP_OR, Or,  OrXorAnd, EMEmulateOr, EMEmulateLockOr);
        INTERPRET_CASE_EX_LOCK_PARAM3(OP_XOR,Xor, OrXorAnd, EMEmulateXor, EMEmulateLockXor);
        INTERPRET_CASE_EX_LOCK_PARAM3(OP_AND,And, OrXorAnd, EMEmulateAnd, EMEmulateLockAnd);
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
        INTERPRET_CASE(OP_RDPMC,Rdpmc);
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
            Log3(("emInterpretInstructionCPU: opcode=%d\n", pDis->pCurInstr->opcode));
            STAM_COUNTER_INC(&pVCpu->em.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,FailedMisc));
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
 * @param   pVCpu       The VMCPU handle.
 * @param   PC          The PC.
 */
VMMDECL(void) EMSetInhibitInterruptsPC(PVMCPU pVCpu, RTGCUINTPTR PC)
{
    pVCpu->em.s.GCPtrInhibitInterrupts = PC;
    VMCPU_FF_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
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
 * @param   pVCpu       The VMCPU handle.
 *
 */
VMMDECL(RTGCUINTPTR) EMGetInhibitInterruptsPC(PVMCPU pVCpu)
{
    return pVCpu->em.s.GCPtrInhibitInterrupts;
}

/**
 * Locks REM execution to a single VCpu
 *
 * @param   pVM         VM handle.
 */
VMMDECL(void) EMRemLock(PVM pVM)
{
    if (!PDMCritSectIsInitialized(&pVM->em.s.CritSectREM))
        return;     /* early init */

    Assert(!PGMIsLockOwner(pVM));
    Assert(!IOMIsLockOwner(pVM));
    int rc = PDMCritSectEnter(&pVM->em.s.CritSectREM, VERR_SEM_BUSY);
    AssertMsg(rc == VINF_SUCCESS, ("%Rrc\n", rc));
}

/**
 * Unlocks REM execution
 *
 * @param   pVM         VM handle.
 */
VMMDECL(void) EMRemUnlock(PVM pVM)
{
    if (!PDMCritSectIsInitialized(&pVM->em.s.CritSectREM))
        return;     /* early init */

    PDMCritSectLeave(&pVM->em.s.CritSectREM);
}

/**
 * Check if this VCPU currently owns the REM lock.
 *
 * @returns bool owner/not owner
 * @param   pVM         The VM to operate on.
 */
VMMDECL(bool) EMRemIsLockOwner(PVM pVM)
{
    if (!PDMCritSectIsInitialized(&pVM->em.s.CritSectREM))
        return true;   /* early init */

    return PDMCritSectIsOwner(&pVM->em.s.CritSectREM);
}

/**
 * Try to acquire the REM lock.
 *
 * @returns VBox status code
 * @param   pVM         The VM to operate on.
 */
VMMDECL(int) EMRemTryLock(PVM pVM)
{
    if (!PDMCritSectIsInitialized(&pVM->em.s.CritSectREM))
        return VINF_SUCCESS; /* early init */

    return PDMCritSectTryEnter(&pVM->em.s.CritSectREM);
}

/**
 * Determine if we should continue after encountering a hlt or mwait instruction
 *
 * @returns boolean
 * @param   pVCpu           The VMCPU to operate on.
 * @param   pCtx            Current CPU context
 */
VMMDECL(bool) EMShouldContinueAfterHalt(PVMCPU pVCpu, PCPUMCTX pCtx)
{
    if (    pCtx->eflags.Bits.u1IF
        ||  ((pVCpu->em.s.mwait.fWait & (EMMWAIT_FLAG_ACTIVE | EMMWAIT_FLAG_BREAKIRQIF0)) == (EMMWAIT_FLAG_ACTIVE | EMMWAIT_FLAG_BREAKIRQIF0)))
    {
        pVCpu->em.s.mwait.fWait &= ~(EMMWAIT_FLAG_ACTIVE | EMMWAIT_FLAG_BREAKIRQIF0);
        return !!VMCPU_FF_ISPENDING(pVCpu, (VMCPU_FF_INTERRUPT_APIC|VMCPU_FF_INTERRUPT_PIC));
    }

    return false;
}

