/* $Id$ */
/** @file
 * EM - Execution Monitor(/Manager) - All contexts
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#include <VBox/hwaccm.h>
#include <VBox/tm.h>

#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef DECLCALLBACK(uint32_t) PFN_EMULATE_PARAM2_UINT32(uint32_t *pu32Param1, uint32_t val2);
typedef DECLCALLBACK(uint32_t) PFN_EMULATE_PARAM2(uint32_t *pu32Param1, size_t val2);
typedef DECLCALLBACK(uint32_t) PFN_EMULATE_PARAM3(uint32_t *pu32Param1, uint32_t val2, size_t val3);
typedef DECLCALLBACK(int)      FNEMULATELOCKPARAM2(RTGCPTR GCPtrParam1, RTGCUINTREG Val2, uint32_t *pf);
typedef FNEMULATELOCKPARAM2 *PFNEMULATELOCKPARAM2;
typedef DECLCALLBACK(int)      FNEMULATELOCKPARAM3(RTGCPTR GCPtrParam1, RTGCUINTREG Val2, size_t cb, uint32_t *pf);
typedef FNEMULATELOCKPARAM3 *PFNEMULATELOCKPARAM3;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
DECLINLINE(int) emInterpretInstructionCPU(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize);


/**
 * Get the current execution manager status.
 *
 * @returns Current status.
 */
EMDECL(EMSTATE) EMGetState(PVM pVM)
{
    return pVM->em.s.enmState;
}


#ifndef IN_GC
/**
 * Read callback for disassembly function; supports reading bytes that cross a page boundary
 *
 * @returns VBox status code.
 * @param   pSrc        GC source pointer
 * @param   pDest       HC destination pointer
 * @param   size        Number of bytes to read
 * @param   dwUserdata  Callback specific user data (pCpu)
 *
 */
DECLCALLBACK(int) EMReadBytes(RTHCUINTPTR pSrc, uint8_t *pDest, unsigned size, void *pvUserdata)
{
    DISCPUSTATE  *pCpu     = (DISCPUSTATE *)pvUserdata;
    PVM           pVM      = (PVM)pCpu->apvUserData[0];
#ifdef IN_RING0
    int rc = PGMPhysReadGCPtr(pVM, pDest, pSrc, size);
    AssertRC(rc);
#else
    if (!PATMIsPatchGCAddr(pVM, pSrc))
    {
        int rc = PGMPhysReadGCPtr(pVM, pDest, pSrc, size);
        AssertRC(rc);
    }
    else
    {
        for (uint32_t i = 0; i < size; i++)
        {
            uint8_t opcode;
            if (VBOX_SUCCESS(PATMR3QueryOpcode(pVM, (RTGCPTR)pSrc + i, &opcode)))
            {
                *(pDest+i) = opcode;
            }
        }
    }
#endif /* IN_RING0 */
    return VINF_SUCCESS;
}

DECLINLINE(int) emDisCoreOne(PVM pVM, DISCPUSTATE *pCpu, RTGCUINTPTR InstrGC, uint32_t *pOpsize)
{
    return DISCoreOneEx(InstrGC, pCpu->mode, EMReadBytes, pVM, pCpu,  pOpsize);
}

#else

DECLINLINE(int) emDisCoreOne(PVM pVM, DISCPUSTATE *pCpu, RTGCUINTPTR InstrGC, uint32_t *pOpsize)
{
    return DISCoreOne(pCpu, InstrGC, pOpsize);
}

#endif


/**
 * Disassembles one instruction.
 *
 * @param   pVM             The VM handle.
 * @param   pCtxCore        The context core (used for both the mode and instruction).
 * @param   pCpu            Where to return the parsed instruction info.
 * @param   pcbInstr        Where to return the instruction size. (optional)
 */
EMDECL(int) EMInterpretDisasOne(PVM pVM, PCCPUMCTXCORE pCtxCore, PDISCPUSTATE pCpu, unsigned *pcbInstr)
{
    RTGCPTR GCPtrInstr;
    int rc = SELMValidateAndConvertCSAddr(pVM, pCtxCore->eflags, pCtxCore->ss, pCtxCore->cs, (PCPUMSELREGHID)&pCtxCore->csHid, (RTGCPTR)pCtxCore->eip, &GCPtrInstr);
    if (VBOX_FAILURE(rc))
    {
        Log(("EMInterpretDisasOne: Failed to convert %RTsel:%RX32 (cpl=%d) - rc=%Vrc !!\n",
             pCtxCore->cs, pCtxCore->eip, pCtxCore->ss & X86_SEL_RPL, rc));
        return rc;
    }
    return EMInterpretDisasOneEx(pVM, (RTGCUINTPTR)GCPtrInstr, pCtxCore, pCpu, pcbInstr);
}


/**
 * Disassembles one instruction.
 *
 * This is used by internally by the interpreter and by trap/access handlers.
 *
 * @param   pVM             The VM handle.
 * @param   GCPtrInstr      The flat address of the instruction.
 * @param   pCtxCore        The context core (used to determin the cpu mode).
 * @param   pCpu            Where to return the parsed instruction info.
 * @param   pcbInstr        Where to return the instruction size. (optional)
 */
EMDECL(int) EMInterpretDisasOneEx(PVM pVM, RTGCUINTPTR GCPtrInstr, PCCPUMCTXCORE pCtxCore, PDISCPUSTATE pCpu, unsigned *pcbInstr)
{
    int rc = DISCoreOneEx(GCPtrInstr, SELMIsSelector32Bit(pVM, pCtxCore->eflags, pCtxCore->cs, (PCPUMSELREGHID)&pCtxCore->csHid) ? CPUMODE_32BIT : CPUMODE_16BIT,
#ifdef IN_GC
                          NULL, NULL,
#else
                          EMReadBytes, pVM,
#endif
                          pCpu, pcbInstr);
    if (VBOX_SUCCESS(rc))
        return VINF_SUCCESS;
    AssertMsgFailed(("DISCoreOne failed to GCPtrInstr=%VGv rc=%Vrc\n", GCPtrInstr, rc));
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
EMDECL(int) EMInterpretInstruction(PVM pVM, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    RTGCPTR pbCode;
    int rc = SELMValidateAndConvertCSAddr(pVM, pRegFrame->eflags, pRegFrame->ss, pRegFrame->cs, &pRegFrame->csHid, (RTGCPTR)pRegFrame->eip, &pbCode);
    if (VBOX_SUCCESS(rc))
    {
        uint32_t    cbOp;
        DISCPUSTATE Cpu;
        Cpu.mode = SELMIsSelector32Bit(pVM, pRegFrame->eflags, pRegFrame->cs, &pRegFrame->csHid) ? CPUMODE_32BIT : CPUMODE_16BIT;
        rc = emDisCoreOne(pVM, &Cpu, (RTGCUINTPTR)pbCode, &cbOp);
        if (VBOX_SUCCESS(rc))
        {
            Assert(cbOp == Cpu.opsize);
            rc = EMInterpretInstructionCPU(pVM, &Cpu, pRegFrame, pvFault, pcbSize);
            if (VBOX_SUCCESS(rc))
            {
                pRegFrame->eip += cbOp; /* Move on to the next instruction. */
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
EMDECL(int) EMInterpretInstructionCPU(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    STAM_PROFILE_START(&CTXMID(pVM->em.s.CTXSUFF(pStats)->Stat,Emulate), a);
    int rc = emInterpretInstructionCPU(pVM, pCpu, pRegFrame, pvFault, pcbSize);
    STAM_PROFILE_STOP(&CTXMID(pVM->em.s.CTXSUFF(pStats)->Stat,Emulate), a);
    if (VBOX_SUCCESS(rc))
        STAM_COUNTER_INC(&pVM->em.s.CTXSUFF(pStats)->CTXMID(Stat,InterpretSucceeded));
    else
        STAM_COUNTER_INC(&pVM->em.s.CTXSUFF(pStats)->CTXMID(Stat,InterpretFailed));
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
EMDECL(int) EMInterpretPortIO(PVM pVM, PCPUMCTXCORE pCtxCore, PDISCPUSTATE pCpu, uint32_t cbOp)
{
    /*
     * Hand it on to IOM.
     */
#ifdef IN_GC
    int rc = IOMGCIOPortHandler(pVM, pCtxCore, pCpu);
    if (IOM_SUCCESS(rc))
        pCtxCore->eip += cbOp;
    return rc;
#else
    AssertReleaseMsgFailed(("not implemented\n"));
    return VERR_NOT_IMPLEMENTED;
#endif
}


DECLINLINE(int) emRamRead(PVM pVM, void *pDest, RTGCPTR GCSrc, uint32_t cb)
{
#ifdef IN_GC
    int rc = MMGCRamRead(pVM, pDest, GCSrc, cb);
    if (RT_LIKELY(rc != VERR_ACCESS_DENIED))
        return rc;
    /* 
     * The page pool cache may end up here in some cases because it 
     * flushed one of the shadow mappings used by the trapping 
     * instruction and it either flushed the TLB or the CPU reused it.
     */
    RTGCPHYS GCPhys;
    rc = PGMPhysGCPtr2GCPhys(pVM, GCSrc, &GCPhys);
    AssertRCReturn(rc, rc);
    PGMPhysRead(pVM, GCPhys, pDest, cb);
    return VINF_SUCCESS;
#else
    return PGMPhysReadGCPtrSafe(pVM, pDest, GCSrc, cb);
#endif
}

DECLINLINE(int) emRamWrite(PVM pVM, RTGCPTR GCDest, void *pSrc, uint32_t cb)
{
#ifdef IN_GC
    int rc = MMGCRamWrite(pVM, GCDest, pSrc, cb);
    if (RT_LIKELY(rc != VERR_ACCESS_DENIED))
        return rc;
    /* 
     * The page pool cache may end up here in some cases because it 
     * flushed one of the shadow mappings used by the trapping 
     * instruction and it either flushed the TLB or the CPU reused it.
     * We want to play safe here, verifying that we've got write 
     * access doesn't cost us much (see PGMPhysGCPtr2GCPhys()).
     */
    uint64_t fFlags;
    RTGCPHYS GCPhys;
    rc = PGMGstGetPage(pVM, GCDest, &fFlags, &GCPhys);
    if (RT_FAILURE(rc))
        return rc;
    if (    !(fFlags & X86_PTE_RW) 
        &&  (CPUMGetGuestCR0(pVM) & X86_CR0_WP))
        return VERR_ACCESS_DENIED;

    PGMPhysWrite(pVM, GCPhys + ((RTGCUINTPTR)GCDest & PAGE_OFFSET_MASK), pSrc, cb);
    return VINF_SUCCESS;

#else
    return PGMPhysWriteGCPtrSafe(pVM, GCDest, pSrc, cb);
#endif
}

/* Convert sel:addr to a flat GC address */
static RTGCPTR emConvertToFlatAddr(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu, POP_PARAMETER pParam, RTGCPTR pvAddr)
{
    int   prefix_seg, rc;
    RTSEL sel;
    CPUMSELREGHID *pSelHidReg;

    prefix_seg = DISDetectSegReg(pCpu, pParam);
    rc = DISFetchRegSegEx(pRegFrame, prefix_seg, &sel, &pSelHidReg);
    if (VBOX_FAILURE(rc))
        return pvAddr;

    return SELMToFlat(pVM, pRegFrame->eflags, sel, pSelHidReg, pvAddr);
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
        case OP_XOR:        return "Xor";
        case OP_OR:         return "Or";
        case OP_AND:        return "And";
        case OP_BTR:        return "Btr";
        case OP_BTS:        return "Bts";
        default:
            AssertMsgFailed(("%d\n", pCpu->pCurInstr->opcode));
            return "???";
    }
}
#endif

/**
 * XCHG instruction emulation.
 */
static int emInterpretXchg(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    OP_PARAMVAL param1, param2;

    /* Source to make DISQueryParamVal read the register value - ugly hack */
    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_SOURCE);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param2, &param2, PARAM_SOURCE);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

#ifdef IN_GC
    if (TRPMHasTrap(pVM))
    {
        if (TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW)
        {
#endif
            RTGCPTR pParam1 = 0, pParam2 = 0;
            uint32_t valpar1, valpar2;

            AssertReturn(pCpu->param1.size == pCpu->param2.size, VERR_EM_INTERPRETER);
            switch(param1.type)
            {
            case PARMTYPE_IMMEDIATE: /* register type is translated to this one too */
                valpar1 = param1.val.val32;
                break;

            case PARMTYPE_ADDRESS:
                pParam1 = (RTGCPTR)param1.val.val32;
                pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, pParam1);
#ifdef IN_GC
                /* Safety check (in theory it could cross a page boundary and fault there though) */
                AssertReturn(pParam1 == pvFault, VERR_EM_INTERPRETER);
#endif
                rc = emRamRead(pVM, &valpar1, pParam1, param1.size);
                if (VBOX_FAILURE(rc))
                {
                    AssertMsgFailed(("MMGCRamRead %VGv size=%d failed with %Vrc\n", pParam1, param1.size, rc));
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
                pParam2 = (RTGCPTR)param2.val.val32;
                pParam2 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param2, pParam2);
#ifdef IN_GC
                /* Safety check (in theory it could cross a page boundary and fault there though) */
                AssertReturn(pParam2 == pvFault, VERR_EM_INTERPRETER);
#endif
                rc = emRamRead(pVM,  &valpar2, pParam2, param2.size);
                if (VBOX_FAILURE(rc))
                {
                    AssertMsgFailed(("MMGCRamRead %VGv size=%d failed with %Vrc\n", pParam1, param1.size, rc));
                }
                break;

            case PARMTYPE_IMMEDIATE:
                valpar2 = param2.val.val32;
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
                        rc = DISWriteReg8(pRegFrame, pCpu->param1.base.reg_gen8, (uint8_t)valpar2); break;
                case 2: rc = DISWriteReg16(pRegFrame, pCpu->param1.base.reg_gen32, (uint16_t)valpar2); break;
                case 4: rc = DISWriteReg32(pRegFrame, pCpu->param1.base.reg_gen32, valpar2); break;
                default: AssertFailedReturn(VERR_EM_INTERPRETER);
                }
                if (VBOX_FAILURE(rc))
                    return VERR_EM_INTERPRETER;
            }
            else
            {
                rc = emRamWrite(pVM, pParam1, &valpar2, param1.size);
                if (VBOX_FAILURE(rc))
                {
                    AssertMsgFailed(("emRamWrite %VGv size=%d failed with %Vrc\n", pParam1, param1.size, rc));
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
                        rc = DISWriteReg8(pRegFrame, pCpu->param2.base.reg_gen8, (uint8_t)valpar1); break;
                case 2: rc = DISWriteReg16(pRegFrame, pCpu->param2.base.reg_gen32, (uint16_t)valpar1); break;
                case 4: rc = DISWriteReg32(pRegFrame, pCpu->param2.base.reg_gen32, valpar1); break;
                default: AssertFailedReturn(VERR_EM_INTERPRETER);
                }
                if (VBOX_FAILURE(rc))
                    return VERR_EM_INTERPRETER;
            }
            else
            {
                rc = emRamWrite(pVM, pParam2, &valpar1, param2.size);
                if (VBOX_FAILURE(rc))
                {
                    AssertMsgFailed(("emRamWrite %VGv size=%d failed with %Vrc\n", pParam1, param1.size, rc));
                    return VERR_EM_INTERPRETER;
                }
            }

            *pcbSize = param2.size;
            return VINF_SUCCESS;
#ifdef IN_GC
        }
    }
#endif
    return VERR_EM_INTERPRETER;
}

/**
 * INC and DEC emulation.
 */
static int emInterpretIncDec(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize,
                             PFN_EMULATE_PARAM2 pfnEmulate)
{
    OP_PARAMVAL param1;

    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_DEST);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

#ifdef IN_GC
    if (TRPMHasTrap(pVM))
    {
        if (TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW)
        {
#endif
            RTGCPTR pParam1 = 0;
            uint32_t valpar1;

            if (param1.type == PARMTYPE_ADDRESS)
            {
                pParam1 = (RTGCPTR)param1.val.val32;
                pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, pParam1);
#ifdef IN_GC
                /* Safety check (in theory it could cross a page boundary and fault there though) */
                AssertReturn(pParam1 == pvFault, VERR_EM_INTERPRETER);
#endif
                rc = emRamRead(pVM,  &valpar1, pParam1, param1.size);
                if (VBOX_FAILURE(rc))
                {
                    AssertMsgFailed(("emRamRead %VGv size=%d failed with %Vrc\n", pParam1, param1.size, rc));
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
            rc = emRamWrite(pVM, pParam1, &valpar1, param1.size);
            if (VBOX_FAILURE(rc))
            {
                AssertMsgFailed(("emRamWrite %VGv size=%d failed with %Vrc\n", pParam1, param1.size, rc));
                return VERR_EM_INTERPRETER;
            }

            /* Update guest's eflags and finish. */
            pRegFrame->eflags.u32 = (pRegFrame->eflags.u32   & ~(X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF))
                                  | (eflags & (X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF));

            /* All done! */
            *pcbSize = param1.size;
            return VINF_SUCCESS;
#ifdef IN_GC
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
    OP_PARAMVAL param1;
    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_DEST);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

#ifdef IN_GC
    if (TRPMHasTrap(pVM))
    {
        if (TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW)
        {
#endif
            RTGCPTR pParam1 = 0;
            uint32_t valpar1;
            RTGCPTR pStackVal;

            /* Read stack value first */
            if (SELMIsSelector32Bit(pVM, pRegFrame->eflags, pRegFrame->ss, &pRegFrame->ssHid) == false)
                return VERR_EM_INTERPRETER; /* No legacy 16 bits stuff here, please. */

            /* Convert address; don't bother checking limits etc, as we only read here */
            pStackVal = SELMToFlat(pVM, pRegFrame->eflags, pRegFrame->ss, &pRegFrame->ssHid, (RTGCPTR)pRegFrame->esp);
            if (pStackVal == 0)
                return VERR_EM_INTERPRETER;

            rc = emRamRead(pVM,  &valpar1, pStackVal, param1.size);
            if (VBOX_FAILURE(rc))
            {
                AssertMsgFailed(("emRamRead %VGv size=%d failed with %Vrc\n", pParam1, param1.size, rc));
                return VERR_EM_INTERPRETER;
            }

            if (param1.type == PARMTYPE_ADDRESS)
            {
                pParam1 = (RTGCPTR)param1.val.val32;

                /* pop [esp+xx] uses esp after the actual pop! */
                AssertCompile(USE_REG_ESP == USE_REG_SP);
                if (    (pCpu->param1.flags & USE_BASE)
                    &&  (pCpu->param1.flags & (USE_REG_GEN16|USE_REG_GEN32))
                    &&  pCpu->param1.base.reg_gen32 == USE_REG_ESP
                   )
                   pParam1 = (RTGCPTR)((RTGCUINTPTR)pParam1 + param1.size);

                pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, pParam1);

#ifdef IN_GC
                /* Safety check (in theory it could cross a page boundary and fault there though) */
                AssertMsgReturn(pParam1 == pvFault || (RTGCPTR)pRegFrame->esp == pvFault, ("%VGv != %VGv ss:esp=%04X:%VGv\n", pParam1, pvFault, pRegFrame->ss, pRegFrame->esp), VERR_EM_INTERPRETER);
#endif
                rc = emRamWrite(pVM, pParam1, &valpar1, param1.size);
                if (VBOX_FAILURE(rc))
                {
                    AssertMsgFailed(("emRamWrite %VGv size=%d failed with %Vrc\n", pParam1, param1.size, rc));
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
#ifdef IN_GC
        }
    }
#endif
    return VERR_EM_INTERPRETER;
}


/**
 * XOR/OR/AND Emulation.
 */
static int emInterpretOrXorAnd(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize,
                               PFN_EMULATE_PARAM3 pfnEmulate)
{
    OP_PARAMVAL param1, param2;
    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_DEST);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param2, &param2, PARAM_SOURCE);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

#ifdef DEBUG
    const char *pszInstr;

    if (pCpu->pCurInstr->opcode == OP_XOR)
        pszInstr = "Xor";
    else
    if (pCpu->pCurInstr->opcode == OP_OR)
        pszInstr = "Or";
    else
    if (pCpu->pCurInstr->opcode == OP_AND)
        pszInstr = "And";
#endif

#ifdef IN_GC
    if (TRPMHasTrap(pVM))
    {
        if (TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW)
        {
#endif
            RTGCPTR  pParam1;
            uint32_t valpar1, valpar2;

            if (pCpu->param1.size != pCpu->param2.size)
            {
                if (pCpu->param1.size < pCpu->param2.size)
                {
                    AssertMsgFailed(("%s at %VGv parameter mismatch %d vs %d!!\n", pszInstr, pRegFrame->eip, pCpu->param1.size, pCpu->param2.size)); /* should never happen! */
                    return VERR_EM_INTERPRETER;
                }
                /* Or %Ev, Ib -> just a hack to save some space; the data width of the 1st parameter determines the real width */
                pCpu->param2.size = pCpu->param1.size;
                param2.size     = param1.size;
            }

            /* The destination is always a virtual address */
            if (param1.type == PARMTYPE_ADDRESS)
            {
                pParam1 = (RTGCPTR)param1.val.val32;
                pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, pParam1);

#ifdef IN_GC
                /* Safety check (in theory it could cross a page boundary and fault there though) */
                AssertMsgReturn(pParam1 == pvFault, ("eip=%VGv, pParam1=%VGv pvFault=%VGv\n", pRegFrame->eip, pParam1, pvFault), VERR_EM_INTERPRETER);
#endif
                rc = emRamRead(pVM,  &valpar1, pParam1, param1.size);
                if (VBOX_FAILURE(rc))
                {
                    AssertMsgFailed(("emRamRead %VGv size=%d failed with %Vrc\n", pParam1, param1.size, rc));
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
                valpar2 = param2.val.val32;
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
            rc = emRamWrite(pVM, pParam1, &valpar1, param1.size);
            if (VBOX_SUCCESS(rc))
            {
                /* All done! */
                *pcbSize = param2.size;
                return VINF_SUCCESS;
            }
#ifdef IN_GC
        }
    }
#endif
    return VERR_EM_INTERPRETER;
}

#ifdef IN_GC
/**
 * LOCK XOR/OR/AND Emulation.
 */
static int emInterpretLockOrXorAnd(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, 
                                   uint32_t *pcbSize, PFNEMULATELOCKPARAM3 pfnEmulate)
{
    OP_PARAMVAL param1, param2;
    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_DEST);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param2, &param2, PARAM_SOURCE);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    if (pCpu->param1.size != pCpu->param2.size)
    {
        AssertMsgReturn(pCpu->param1.size >= pCpu->param2.size, /* should never happen! */
                        ("%s at %VGv parameter mismatch %d vs %d!!\n", emGetMnemonic(pCpu), pRegFrame->eip, pCpu->param1.size, pCpu->param2.size),
                        VERR_EM_INTERPRETER);

        /* Or %Ev, Ib -> just a hack to save some space; the data width of the 1st parameter determines the real width */
        pCpu->param2.size = pCpu->param1.size;
        param2.size       = param1.size;
    }

    /* The destination is always a virtual address */
    AssertReturn(param1.type == PARMTYPE_ADDRESS, VERR_EM_INTERPRETER);
    RTGCPTR GCPtrPar1 = (RTGCPTR)param1.val.val32;
    GCPtrPar1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, GCPtrPar1);

# ifdef IN_GC
    /* Safety check (in theory it could cross a page boundary and fault there though) */
    Assert(   TRPMHasTrap(pVM)
           && (TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW));
    AssertMsgReturn(GCPtrPar1 == pvFault, ("eip=%VGv, GCPtrPar1=%VGv pvFault=%VGv\n", pRegFrame->eip, GCPtrPar1, pvFault), VERR_EM_INTERPRETER);
# endif

    /* Register and immediate data == PARMTYPE_IMMEDIATE */
    AssertReturn(param2.type == PARMTYPE_IMMEDIATE, VERR_EM_INTERPRETER);
    RTGCUINTREG ValPar2 = param2.val.val32;

    /* Try emulate it with a one-shot #PF handler in place. */
    Log2(("%s %RGv imm%d=%RGr\n", emGetMnemonic(pCpu), GCPtrPar1, pCpu->param2.size*8, ValPar2));

    RTGCUINTREG eflags = 0;
    MMGCRamRegisterTrapHandler(pVM);
    rc = pfnEmulate(GCPtrPar1, ValPar2, pCpu->param2.size, &eflags);
    MMGCRamDeregisterTrapHandler(pVM);

    if (RT_FAILURE(rc))
    {
        Log(("%s %RGv imm%d=%RGr -> emulation failed due to page fault!\n", emGetMnemonic(pCpu), GCPtrPar1, pCpu->param2.size*8, ValPar2));
        return VERR_EM_INTERPRETER;
    }

    /* Update guest's eflags and finish. */
    pRegFrame->eflags.u32 = (pRegFrame->eflags.u32 & ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF))
                          | (eflags                &  (X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF));

    *pcbSize = param2.size;
    return VINF_SUCCESS;
}
#endif

/**
 * ADD, ADC & SUB Emulation.
 */
static int emInterpretAddSub(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize,
                             PFN_EMULATE_PARAM3 pfnEmulate)
{
    OP_PARAMVAL param1, param2;
    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_DEST);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param2, &param2, PARAM_SOURCE);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

#ifdef DEBUG
    const char *pszInstr;

    if (pCpu->pCurInstr->opcode == OP_SUB)
        pszInstr = "Sub";
    else
    if (pCpu->pCurInstr->opcode == OP_ADD)
        pszInstr = "Add";
    else
    if (pCpu->pCurInstr->opcode == OP_ADC)
        pszInstr = "Adc";
#endif

#ifdef IN_GC
    if (TRPMHasTrap(pVM))
    {
        if (TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW)
        {
#endif
            RTGCPTR  pParam1;
            uint32_t valpar1, valpar2;

            if (pCpu->param1.size != pCpu->param2.size)
            {
                if (pCpu->param1.size < pCpu->param2.size)
                {
                    AssertMsgFailed(("%s at %VGv parameter mismatch %d vs %d!!\n", pszInstr, pRegFrame->eip, pCpu->param1.size, pCpu->param2.size)); /* should never happen! */
                    return VERR_EM_INTERPRETER;
                }
                /* Or %Ev, Ib -> just a hack to save some space; the data width of the 1st parameter determines the real width */
                pCpu->param2.size = pCpu->param1.size;
                param2.size     = param1.size;
            }

            /* The destination is always a virtual address */
            if (param1.type == PARMTYPE_ADDRESS)
            {
                pParam1 = (RTGCPTR)param1.val.val32;
                pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, pParam1);

#ifdef IN_GC
                /* Safety check (in theory it could cross a page boundary and fault there though) */
                AssertReturn(pParam1 == pvFault, VERR_EM_INTERPRETER);
#endif
                rc = emRamRead(pVM,  &valpar1, pParam1, param1.size);
                if (VBOX_FAILURE(rc))
                {
                    AssertMsgFailed(("emRamRead %VGv size=%d failed with %Vrc\n", pParam1, param1.size, rc));
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
                valpar2 = param2.val.val32;
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
            rc = emRamWrite(pVM, pParam1, &valpar1, param1.size);
            if (VBOX_SUCCESS(rc))
            {
                /* All done! */
                *pcbSize = param2.size;
                return VINF_SUCCESS;
            }
#ifdef IN_GC
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
                              PFN_EMULATE_PARAM2_UINT32 pfnEmulate)
{
    OP_PARAMVAL param1, param2;
    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_DEST);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param2, &param2, PARAM_SOURCE);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

#ifdef LOG_ENABLED
    const char *pszInstr;

    if (pCpu->pCurInstr->opcode == OP_BTR)
        pszInstr = "Btr";
    else
    if (pCpu->pCurInstr->opcode == OP_BTS)
        pszInstr = "Bts";
    else
    if (pCpu->pCurInstr->opcode == OP_BTC)
        pszInstr = "Btc";
#endif

#ifdef IN_GC
    if (TRPMHasTrap(pVM))
    {
        if (TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW)
        {
#endif
            RTGCPTR  pParam1;
            uint32_t valpar1 = 0, valpar2;
            uint32_t eflags;

            /* The destination is always a virtual address */
            if (param1.type != PARMTYPE_ADDRESS)
                return VERR_EM_INTERPRETER;

            pParam1 = (RTGCPTR)param1.val.val32;
            pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, pParam1);

            /* Register or immediate data */
            switch(param2.type)
            {
            case PARMTYPE_IMMEDIATE:    /* both immediate data and register (ugly) */
                valpar2 = param2.val.val32;
                break;

            default:
                AssertFailed();
                return VERR_EM_INTERPRETER;
            }

            Log2(("emInterpret%s: pvFault=%VGv pParam1=%VGv val2=%x\n", pszInstr, pvFault, pParam1, valpar2));
            pParam1 = (RTGCPTR)((RTGCUINTPTR)pParam1 + valpar2/8);
#ifdef IN_GC
            /* Safety check. */
            AssertMsgReturn((RTGCPTR)((RTGCUINTPTR)pParam1 & ~3) == pvFault, ("pParam1=%VGv pvFault=%VGv\n", pParam1, pvFault), VERR_EM_INTERPRETER);
#endif
            rc = emRamRead(pVM, &valpar1, pParam1, 1);
            if (VBOX_FAILURE(rc))
            {
                AssertMsgFailed(("emRamRead %VGv size=%d failed with %Vrc\n", pParam1, param1.size, rc));
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
            rc = emRamWrite(pVM, pParam1, &valpar1, 1);
            if (VBOX_SUCCESS(rc))
            {
                /* All done! */
                *pcbSize = 1;
                return VINF_SUCCESS;
            }
#ifdef IN_GC
        }
    }
#endif
    return VERR_EM_INTERPRETER;
}

#ifdef IN_GC
/**
 * LOCK BTR/C/S Emulation.
 */
static int emInterpretLockBitTest(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, 
                                  uint32_t *pcbSize, PFNEMULATELOCKPARAM2 pfnEmulate)
{
    OP_PARAMVAL param1, param2;
    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_DEST);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param2, &param2, PARAM_SOURCE);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    /* The destination is always a virtual address */
    if (param1.type != PARMTYPE_ADDRESS)
        return VERR_EM_INTERPRETER;

    RTGCPTR GCPtrPar1 = (RTGCPTR)param1.val.val32;
    GCPtrPar1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, GCPtrPar1);

    /* Register and immediate data == PARMTYPE_IMMEDIATE */
    AssertReturn(param2.type == PARMTYPE_IMMEDIATE, VERR_EM_INTERPRETER);
    RTGCUINTREG ValPar2 = param2.val.val32;

    Log2(("emInterpretLockBitTest %s: pvFault=%VGv GCPtrPar1=%RGv imm=%RGr\n", emGetMnemonic(pCpu), pvFault, GCPtrPar1, ValPar2));

    /* Adjust the parameters so what we're dealing with is a bit within the byte pointed to. */
    GCPtrPar1 = (RTGCPTR)((RTGCUINTPTR)GCPtrPar1 + ValPar2 / 8);
    ValPar2 &= 7;
# ifdef IN_GC
    Assert(TRPMHasTrap(pVM));
    AssertMsgReturn((RTGCPTR)((RTGCUINTPTR)GCPtrPar1 & ~(RTGCUINTPTR)3) == pvFault, 
                    ("GCPtrPar1=%VGv pvFault=%VGv\n", GCPtrPar1, pvFault), 
                    VERR_EM_INTERPRETER);
# endif

    /* Try emulate it with a one-shot #PF handler in place. */
    RTGCUINTREG eflags = 0;
    MMGCRamRegisterTrapHandler(pVM);
    rc = pfnEmulate(GCPtrPar1, ValPar2, &eflags);
    MMGCRamDeregisterTrapHandler(pVM);

    if (RT_FAILURE(rc))
    {
        Log(("emInterpretLockBitTest %s: %RGv imm%d=%RGr -> emulation failed due to page fault!\n", 
             emGetMnemonic(pCpu), GCPtrPar1, pCpu->param2.size*8, ValPar2));
        return VERR_EM_INTERPRETER;
    }

    Log2(("emInterpretLockBitTest %s: GCPtrPar1=%RGv imm=%RGr CF=%d\n", emGetMnemonic(pCpu), GCPtrPar1, ValPar2, !!(eflags & X86_EFL_CF)));

    /* Update guest's eflags and finish. */
    pRegFrame->eflags.u32 = (pRegFrame->eflags.u32 & ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF))
                          | (eflags                &  (X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF));

    *pcbSize = 1;
    return VINF_SUCCESS;
}
#endif /* IN_GC */

/**
 * MOV emulation.
 */
static int emInterpretMov(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    OP_PARAMVAL param1, param2;
    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_DEST);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param2, &param2, PARAM_SOURCE);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

#ifdef IN_GC
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
            uint32_t val32;

            switch(param1.type)
            {
            case PARMTYPE_IMMEDIATE:
                if(!(param1.flags & PARAM_VAL32))
                    return VERR_EM_INTERPRETER;
                /* fallthru */

            case PARMTYPE_ADDRESS:
                pDest = (RTGCPTR)param1.val.val32;
                pDest = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, pDest);
                break;

            default:
                AssertFailed();
                return VERR_EM_INTERPRETER;
            }

            switch(param2.type)
            {
            case PARMTYPE_IMMEDIATE: /* register type is translated to this one too */
                val32 = param2.val.val32;
                break;

            default:
                Log(("emInterpretMov: unexpected type=%d eip=%VGv\n", param2.type, pRegFrame->eip));
                return VERR_EM_INTERPRETER;
            }
            LogFlow(("EMInterpretInstruction at %08x: OP_MOV %08X <- %08X (%d) &val32=%08x\n", pRegFrame->eip, pDest, val32, param2.size, &val32));

            Assert(param2.size <= 4 && param2.size > 0);

#if 0 /* CSAM/PATM translates aliases which causes this to incorrectly trigger. See #2609 and #1498. */
#ifdef IN_GC
            /* Safety check (in theory it could cross a page boundary and fault there though) */
            AssertMsgReturn(pDest == pvFault, ("eip=%VGv pDest=%VGv pvFault=%VGv\n", pRegFrame->eip, pDest, pvFault), VERR_EM_INTERPRETER);
#endif
#endif
            rc = emRamWrite(pVM, pDest, &val32, param2.size);
            if (VBOX_FAILURE(rc))
                return VERR_EM_INTERPRETER;

            *pcbSize = param2.size;
        }
        else
        { /* read fault */
            RTGCPTR pSrc;
            uint32_t val32;

            /* Source */
            switch(param2.type)
            {
            case PARMTYPE_IMMEDIATE:
                if(!(param2.flags & PARAM_VAL32))
                    return VERR_EM_INTERPRETER;
                /* fallthru */

            case PARMTYPE_ADDRESS:
                pSrc = (RTGCPTR)param2.val.val32;
                pSrc = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param2, pSrc);
                break;

            default:
                return VERR_EM_INTERPRETER;
            }

            Assert(param1.size <= 4 && param1.size > 0);
#ifdef IN_GC
            /* Safety check (in theory it could cross a page boundary and fault there though) */
            AssertReturn(pSrc == pvFault, VERR_EM_INTERPRETER);
#endif
            rc = emRamRead(pVM, &val32, pSrc, param1.size);
            if (VBOX_FAILURE(rc))
                return VERR_EM_INTERPRETER;

            /* Destination */
            switch(param1.type)
            {
            case PARMTYPE_REGISTER:
                switch(param1.size)
                {
                case 1: rc = DISWriteReg8(pRegFrame, pCpu->param1.base.reg_gen8, (uint8_t)val32); break;
                case 2: rc = DISWriteReg16(pRegFrame, pCpu->param1.base.reg_gen16, (uint16_t)val32); break;
                case 4: rc = DISWriteReg32(pRegFrame, pCpu->param1.base.reg_gen32, val32); break;
                default:
                    return VERR_EM_INTERPRETER;
                }
                if (VBOX_FAILURE(rc))
                    return rc;
                break;

            default:
                return VERR_EM_INTERPRETER;
            }
            LogFlow(("EMInterpretInstruction: OP_MOV %08X -> %08X (%d)\n", pSrc, val32, param1.size));
        }
        return VINF_SUCCESS;
#ifdef IN_GC
    }
#endif
    return VERR_EM_INTERPRETER;
}

/*
 * [LOCK] CMPXCHG emulation.
 */
#ifdef IN_GC
static int emInterpretCmpXchg(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    OP_PARAMVAL param1, param2;

#ifdef LOG_ENABLED
    char *pszInstr;

    if (pCpu->prefix & PREFIX_LOCK)
        pszInstr = "Lock CmpXchg";
    else
        pszInstr = "CmpXchg";
#endif

    /* Source to make DISQueryParamVal read the register value - ugly hack */
    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_SOURCE);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param2, &param2, PARAM_SOURCE);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    if (TRPMHasTrap(pVM))
    {
        if (TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW)
        {
            RTGCPTR pParam1;
            uint32_t valpar, eflags;
#ifdef VBOX_STRICT
            uint32_t valpar1 = 0; /// @todo used uninitialized...
#endif

            AssertReturn(pCpu->param1.size == pCpu->param2.size, VERR_EM_INTERPRETER);
            switch(param1.type)
            {
            case PARMTYPE_ADDRESS:
                pParam1 = (RTGCPTR)param1.val.val32;
                pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, pParam1);

                /* Safety check (in theory it could cross a page boundary and fault there though) */
                AssertMsgReturn(pParam1 == pvFault, ("eip=%VGv pParam1=%VGv pvFault=%VGv\n", pRegFrame->eip, pParam1, pvFault), VERR_EM_INTERPRETER);
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

            LogFlow(("%s %VGv=%08x eax=%08x %08x\n", pszInstr, pParam1, valpar1, pRegFrame->eax, valpar));

            MMGCRamRegisterTrapHandler(pVM);
            if (pCpu->prefix & PREFIX_LOCK)
                rc = EMGCEmulateLockCmpXchg(pParam1, &pRegFrame->eax, valpar, pCpu->param2.size, &eflags);
            else
                rc = EMGCEmulateCmpXchg(pParam1, &pRegFrame->eax, valpar, pCpu->param2.size, &eflags);
            MMGCRamDeregisterTrapHandler(pVM);

            if (VBOX_FAILURE(rc))
            {
                Log(("%s %VGv=%08x eax=%08x %08x -> emulation failed due to page fault!\n", pszInstr, pParam1, valpar1, pRegFrame->eax, valpar));
                return VERR_EM_INTERPRETER;
            }

            LogFlow(("%s %VGv=%08x eax=%08x %08x ZF=%d\n", pszInstr, pParam1, valpar1, pRegFrame->eax, valpar, !!(eflags & X86_EFL_ZF)));

            /* Update guest's eflags and finish. */
            pRegFrame->eflags.u32 = (pRegFrame->eflags.u32 & ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF))
                                  | (eflags                &  (X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF));

            *pcbSize = param2.size;
            return VINF_SUCCESS;
        }
    }
    return VERR_EM_INTERPRETER;
}

/*
 * [LOCK] CMPXCHG8B emulation.
 */
static int emInterpretCmpXchg8b(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    OP_PARAMVAL param1;

#ifdef LOG_ENABLED
    char *pszInstr;

    if (pCpu->prefix & PREFIX_LOCK)
        pszInstr = "Lock CmpXchg8b";
    else
        pszInstr = "CmpXchg8b";
#endif

    /* Source to make DISQueryParamVal read the register value - ugly hack */
    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_SOURCE);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    if (TRPMHasTrap(pVM))
    {
        if (TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW)
        {
            RTGCPTR pParam1;
            uint32_t eflags;

            AssertReturn(pCpu->param1.size == 8, VERR_EM_INTERPRETER);
            switch(param1.type)
            {
            case PARMTYPE_ADDRESS:
                pParam1 = (RTGCPTR)param1.val.val32;
                pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, pParam1);

                /* Safety check (in theory it could cross a page boundary and fault there though) */
                AssertMsgReturn(pParam1 == pvFault, ("eip=%VGv pParam1=%VGv pvFault=%VGv\n", pRegFrame->eip, pParam1, pvFault), VERR_EM_INTERPRETER);
                break;

            default:
                return VERR_EM_INTERPRETER;
            }

            LogFlow(("%s %VGv=%08x eax=%08x\n", pszInstr, pParam1, pRegFrame->eax));

            MMGCRamRegisterTrapHandler(pVM);
            if (pCpu->prefix & PREFIX_LOCK)
                rc = EMGCEmulateLockCmpXchg8b(pParam1, &pRegFrame->eax, &pRegFrame->edx, pRegFrame->ebx, pRegFrame->ecx, &eflags);
            else
                rc = EMGCEmulateCmpXchg8b(pParam1, &pRegFrame->eax, &pRegFrame->edx, pRegFrame->ebx, pRegFrame->ecx, &eflags);
            MMGCRamDeregisterTrapHandler(pVM);

            if (VBOX_FAILURE(rc))
            {
                Log(("%s %VGv=%08x eax=%08x -> emulation failed due to page fault!\n", pszInstr, pParam1, pRegFrame->eax));
                return VERR_EM_INTERPRETER;
            }

            LogFlow(("%s %VGv=%08x eax=%08x ZF=%d\n", pszInstr, pParam1, pRegFrame->eax, !!(eflags & X86_EFL_ZF)));

            /* Update guest's eflags and finish. */
            pRegFrame->eflags.u32 = (pRegFrame->eflags.u32 & ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF))
                                  | (eflags                &  (X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF));

            *pcbSize = 8;
            return VINF_SUCCESS;
        }
    }
    return VERR_EM_INTERPRETER;
}
#endif

/*
 * [LOCK] XADD emulation.
 */
#ifdef IN_GC
static int emInterpretXAdd(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    OP_PARAMVAL param1;
    uint32_t *pParamReg2;
    size_t cbSizeParamReg2;

    /* Source to make DISQueryParamVal read the register value - ugly hack */
    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_SOURCE);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    rc = DISQueryParamRegPtr(pRegFrame, pCpu, &pCpu->param2, &pParamReg2, &cbSizeParamReg2);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    if (TRPMHasTrap(pVM))
    {
        if (TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW)
        {
            RTGCPTR pParam1;
            uint32_t eflags;
#ifdef VBOX_STRICT
            uint32_t valpar1 = 0; /// @todo used uninitialized...
#endif

            AssertReturn(pCpu->param1.size == pCpu->param2.size, VERR_EM_INTERPRETER);
            switch(param1.type)
            {
            case PARMTYPE_ADDRESS:
                pParam1 = (RTGCPTR)param1.val.val32;
                pParam1 = emConvertToFlatAddr(pVM, pRegFrame, pCpu, &pCpu->param1, pParam1);

                /* Safety check (in theory it could cross a page boundary and fault there though) */
                AssertMsgReturn(pParam1 == pvFault, ("eip=%VGv pParam1=%VGv pvFault=%VGv\n", pRegFrame->eip, pParam1, pvFault), VERR_EM_INTERPRETER);
                break;

            default:
                return VERR_EM_INTERPRETER;
            }

            LogFlow(("XAdd %VGv=%08x reg=%08x\n", pParam1, *pParamReg2));

            MMGCRamRegisterTrapHandler(pVM);
            if (pCpu->prefix & PREFIX_LOCK)
                rc = EMGCEmulateLockXAdd(pParam1, pParamReg2, cbSizeParamReg2, &eflags);
            else
                rc = EMGCEmulateXAdd(pParam1, pParamReg2, cbSizeParamReg2, &eflags);
            MMGCRamDeregisterTrapHandler(pVM);

            if (VBOX_FAILURE(rc))
            {
                Log(("XAdd %VGv=%08x reg=%08x -> emulation failed due to page fault!\n", pParam1, valpar1, *pParamReg2));
                return VERR_EM_INTERPRETER;
            }

            LogFlow(("XAdd %VGv=%08x reg=%08x ZF=%d\n", pParam1, valpar1, *pParamReg2, !!(eflags & X86_EFL_ZF)));

            /* Update guest's eflags and finish. */
            pRegFrame->eflags.u32 = (pRegFrame->eflags.u32 & ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF))
                                  | (eflags                &  (X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF));

            *pcbSize = cbSizeParamReg2;
            return VINF_SUCCESS;
        }
    }
    return VERR_EM_INTERPRETER;
}
#endif

/**
 * Interpret IRET (currently only to V86 code)
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 *
 */
EMDECL(int) EMInterpretIret(PVM pVM, PCPUMCTXCORE pRegFrame)
{
    RTGCUINTPTR pIretStack = (RTGCUINTPTR)pRegFrame->esp;
    RTGCUINTPTR eip, cs, esp, ss, eflags, ds, es, fs, gs, uMask;
    int         rc;

    rc  = emRamRead(pVM, &eip,      (RTGCPTR)pIretStack      , 4);
    rc |= emRamRead(pVM, &cs,       (RTGCPTR)(pIretStack + 4), 4);
    rc |= emRamRead(pVM, &eflags,   (RTGCPTR)(pIretStack + 8), 4);
    AssertRCReturn(rc, VERR_EM_INTERPRETER);
    AssertReturn(eflags & X86_EFL_VM, VERR_EM_INTERPRETER);

    rc |= emRamRead(pVM, &esp,      (RTGCPTR)(pIretStack + 12), 4);
    rc |= emRamRead(pVM, &ss,       (RTGCPTR)(pIretStack + 16), 4);
    rc |= emRamRead(pVM, &es,       (RTGCPTR)(pIretStack + 20), 4);
    rc |= emRamRead(pVM, &ds,       (RTGCPTR)(pIretStack + 24), 4);
    rc |= emRamRead(pVM, &fs,       (RTGCPTR)(pIretStack + 28), 4);
    rc |= emRamRead(pVM, &gs,       (RTGCPTR)(pIretStack + 32), 4);
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


/**
 * IRET Emulation.
 */
static int emInterpretIret(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    /* only allow direct calls to EMInterpretIret for now */
    return VERR_EM_INTERPRETER;
}

/**
 * INVLPG Emulation.
 */

/**
 * Interpret INVLPG
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 * @param   pAddrGC     Operand address
 *
 */
EMDECL(int) EMInterpretInvlpg(PVM pVM, PCPUMCTXCORE pRegFrame, RTGCPTR pAddrGC)
{
    int rc;

    /** @todo is addr always a flat linear address or ds based
     * (in absence of segment override prefixes)????
     */
#ifdef IN_GC
    // Note: we could also use PGMFlushPage here, but it currently doesn't always use invlpg!!!!!!!!!!
    LogFlow(("GC: EMULATE: invlpg %08X\n", pAddrGC));
    rc = PGMGCInvalidatePage(pVM, pAddrGC);
#else
    rc = PGMInvalidatePage(pVM, pAddrGC);
#endif
    if (VBOX_SUCCESS(rc))
        return VINF_SUCCESS;
    Log(("PGMInvalidatePage %VGv returned %VGv (%d)\n", pAddrGC, rc, rc));
    Assert(rc == VERR_REM_FLUSHED_PAGES_OVERFLOW);
    /** @todo r=bird: we shouldn't ignore returns codes like this... I'm 99% sure the error is fatal. */
    return VERR_EM_INTERPRETER;
}

static int emInterpretInvlPg(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    OP_PARAMVAL param1;
    RTGCPTR     addr;

    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_SOURCE);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    switch(param1.type)
    {
    case PARMTYPE_IMMEDIATE:
    case PARMTYPE_ADDRESS:
        if(!(param1.flags & PARAM_VAL32))
            return VERR_EM_INTERPRETER;
        addr = (RTGCPTR)param1.val.val32;
        break;

    default:
        return VERR_EM_INTERPRETER;
    }

    /** @todo is addr always a flat linear address or ds based
     * (in absence of segment override prefixes)????
     */
#ifdef IN_GC
    // Note: we could also use PGMFlushPage here, but it currently doesn't always use invlpg!!!!!!!!!!
    LogFlow(("GC: EMULATE: invlpg %08X\n", addr));
    rc = PGMGCInvalidatePage(pVM, addr);
#else
    rc = PGMInvalidatePage(pVM, addr);
#endif
    if (VBOX_SUCCESS(rc))
        return VINF_SUCCESS;
    /** @todo r=bird: we shouldn't ignore returns codes like this... I'm 99% sure the error is fatal. */
    return VERR_EM_INTERPRETER;
}

/**
 * CPUID Emulation.
 */

/**
 * Interpret CPUID given the parameters in the CPU context
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 *
 */
EMDECL(int) EMInterpretCpuId(PVM pVM, PCPUMCTXCORE pRegFrame)
{
    CPUMGetGuestCpuId(pVM, pRegFrame->eax, &pRegFrame->eax, &pRegFrame->ebx, &pRegFrame->ecx, &pRegFrame->edx);
    return VINF_SUCCESS;
}

static int emInterpretCpuId(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    uint32_t iLeaf = pRegFrame->eax; NOREF(iLeaf);

    int rc = EMInterpretCpuId(pVM, pRegFrame);
    Log(("Emulate: CPUID %x -> %08x %08x %08x %08x\n", iLeaf, pRegFrame->eax, pRegFrame->ebx, pRegFrame->ecx, pRegFrame->edx));
    return rc;
}

/**
 * MOV CRx Emulation.
 */

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
EMDECL(int) EMInterpretCRxRead(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t DestRegGen, uint32_t SrcRegCrx)
{
    uint32_t val32;

    int rc = CPUMGetGuestCRx(pVM, SrcRegCrx, &val32);
    AssertMsgRCReturn(rc, ("CPUMGetGuestCRx %d failed\n", SrcRegCrx), VERR_EM_INTERPRETER);
    rc = DISWriteReg32(pRegFrame, DestRegGen, val32);
    if(VBOX_SUCCESS(rc))
    {
        LogFlow(("MOV_CR: gen32=%d CR=%d val=%08x\n", DestRegGen, SrcRegCrx, val32));
        return VINF_SUCCESS;
    }
    return VERR_EM_INTERPRETER;
}


/**
 * Interpret LMSW
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   u16Data     LMSW source data.
 *
 */
EMDECL(int) EMInterpretLMSW(PVM pVM, uint16_t u16Data)
{
    uint32_t OldCr0 = CPUMGetGuestCR0(pVM);

    /* don't use this path to go into protected mode! */
    Assert(OldCr0 & X86_CR0_PE);
    if (!(OldCr0 & X86_CR0_PE))
        return VERR_EM_INTERPRETER;

    /* Only PE, MP, EM and TS can be changed; note that PE can't be cleared by this instruction. */
    uint32_t NewCr0 = ( OldCr0 & ~(             X86_CR0_MP | X86_CR0_EM | X86_CR0_TS))
                    | (u16Data &  (X86_CR0_PE | X86_CR0_MP | X86_CR0_EM | X86_CR0_TS));

#ifdef IN_GC
    /* Need to change the hyper CR0? Doing it the lazy way then. */
    if (    (OldCr0 & (X86_CR0_AM | X86_CR0_WP))
        !=  (NewCr0 & (X86_CR0_AM | X86_CR0_WP)))
    {
        Log(("EMInterpretLMSW: CR0: %#x->%#x => R3\n", OldCr0, NewCr0));
        VM_FF_SET(pVM, VM_FF_TO_R3);
    }
#endif

    return CPUMSetGuestCR0(pVM, NewCr0);
}


/**
 * Interpret CLTS
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 *
 */
EMDECL(int) EMInterpretCLTS(PVM pVM)
{
    uint32_t cr0 = CPUMGetGuestCR0(pVM);
    if (!(cr0 & X86_CR0_TS))
        return VINF_SUCCESS;
    return CPUMSetGuestCR0(pVM, cr0 & ~X86_CR0_TS);
}

static int emInterpretClts(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    return EMInterpretCLTS(pVM);
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
EMDECL(int) EMInterpretCRxWrite(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t DestRegCrx, uint32_t SrcRegGen)
{
    uint32_t val32;
    uint32_t oldval;
/** @todo Clean up this mess. */

/** @todo AMD64 */
    int rc = DISFetchReg32(pRegFrame, SrcRegGen, &val32);
    if (VBOX_SUCCESS(rc))
    {
        switch (DestRegCrx)
        {
        case USE_REG_CR0:
            oldval = CPUMGetGuestCR0(pVM);
#ifdef IN_GC
            /* CR0.WP and CR0.AM changes require a reschedule run in ring 3. */
            if (    (val32 & (X86_CR0_WP | X86_CR0_AM)) 
                !=  (oldval & (X86_CR0_WP | X86_CR0_AM)))
                return VERR_EM_INTERPRETER;
#endif
            CPUMSetGuestCR0(pVM, val32);
            val32 = CPUMGetGuestCR0(pVM);
            if (    (oldval & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE))
                !=  (val32  & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE)))
            {
                /* global flush */
                rc = PGMFlushTLB(pVM, CPUMGetGuestCR3(pVM), true /* global */);
                AssertRCReturn(rc, rc);
            }
            return PGMChangeMode(pVM, CPUMGetGuestCR0(pVM), CPUMGetGuestCR4(pVM), CPUMGetGuestEFER(pVM));

        case USE_REG_CR2:
            rc = CPUMSetGuestCR2(pVM, val32); AssertRC(rc);
            return VINF_SUCCESS;

        case USE_REG_CR3:
            /* Reloading the current CR3 means the guest just wants to flush the TLBs */
            rc = CPUMSetGuestCR3(pVM, val32); AssertRC(rc);
            if (CPUMGetGuestCR0(pVM) & X86_CR0_PG)
            {
                /* flush */
                rc = PGMFlushTLB(pVM, val32, !(CPUMGetGuestCR4(pVM) & X86_CR4_PGE));
                AssertRCReturn(rc, rc);
            }
            return VINF_SUCCESS;

        case USE_REG_CR4:
            oldval = CPUMGetGuestCR4(pVM);
            rc = CPUMSetGuestCR4(pVM, val32); AssertRC(rc);
            val32 = CPUMGetGuestCR4(pVM);
            if (    (oldval & (X86_CR4_PGE|X86_CR4_PAE|X86_CR4_PSE))
                !=  (val32  & (X86_CR4_PGE|X86_CR4_PAE|X86_CR4_PSE)))
            {
                /* global flush */
                rc = PGMFlushTLB(pVM, CPUMGetGuestCR3(pVM), true /* global */);
                AssertRCReturn(rc, rc);
            }
# ifdef IN_GC
            /* Feeling extremely lazy. */
            if (    (oldval & (X86_CR4_OSFSXR|X86_CR4_OSXMMEEXCPT|X86_CR4_PCE|X86_CR4_MCE|X86_CR4_PAE|X86_CR4_DE|X86_CR4_TSD|X86_CR4_PVI|X86_CR4_VME))
                !=  (val32  & (X86_CR4_OSFSXR|X86_CR4_OSXMMEEXCPT|X86_CR4_PCE|X86_CR4_MCE|X86_CR4_PAE|X86_CR4_DE|X86_CR4_TSD|X86_CR4_PVI|X86_CR4_VME)))
            {
                Log(("emInterpretMovCRx: CR4: %#x->%#x => R3\n", oldval, val32));
                VM_FF_SET(pVM, VM_FF_TO_R3);
            }
# endif
            return PGMChangeMode(pVM, CPUMGetGuestCR0(pVM), CPUMGetGuestCR4(pVM), CPUMGetGuestEFER(pVM));

        default:
            AssertFailed();
        case USE_REG_CR1: /* illegal op */
            break;
        }
    }
    return VERR_EM_INTERPRETER;
}

static int emInterpretMovCRx(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    if (pCpu->param1.flags == USE_REG_GEN32 && pCpu->param2.flags == USE_REG_CR)
        return EMInterpretCRxRead(pVM, pRegFrame, pCpu->param1.base.reg_gen32, pCpu->param2.base.reg_ctrl);
    if (pCpu->param1.flags == USE_REG_CR && pCpu->param2.flags == USE_REG_GEN32)
        return EMInterpretCRxWrite(pVM, pRegFrame, pCpu->param1.base.reg_ctrl, pCpu->param2.base.reg_gen32);
    AssertMsgFailedReturn(("Unexpected control register move\n"), VERR_EM_INTERPRETER);
    return VERR_EM_INTERPRETER;
}

/**
 * MOV DRx
 */

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
EMDECL(int) EMInterpretDRxWrite(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t DestRegDrx, uint32_t SrcRegGen)
{
    uint32_t val32;

    int rc = DISFetchReg32(pRegFrame, SrcRegGen, &val32);
    if (VBOX_SUCCESS(rc))
    {
        rc = CPUMSetGuestDRx(pVM, DestRegDrx, val32);
        if (VBOX_SUCCESS(rc))
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
EMDECL(int) EMInterpretDRxRead(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t DestRegGen, uint32_t SrcRegDrx)
{
    uint32_t val32;

    int rc = CPUMGetGuestDRx(pVM, SrcRegDrx, &val32);
    AssertMsgRCReturn(rc, ("CPUMGetGuestDRx %d failed\n", SrcRegDrx), VERR_EM_INTERPRETER);
    rc = DISWriteReg32(pRegFrame, DestRegGen, val32);
    if (VBOX_SUCCESS(rc))
        return VINF_SUCCESS;
    return VERR_EM_INTERPRETER;
}

static int emInterpretMovDRx(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    int rc = VERR_EM_INTERPRETER;

    if(pCpu->param1.flags == USE_REG_GEN32 && pCpu->param2.flags == USE_REG_DBG)
    {
        rc = EMInterpretDRxRead(pVM, pRegFrame, pCpu->param1.base.reg_gen32, pCpu->param2.base.reg_dbg);
    }
    else
    if(pCpu->param1.flags == USE_REG_DBG && pCpu->param2.flags == USE_REG_GEN32)
    {
        rc = EMInterpretDRxWrite(pVM, pRegFrame, pCpu->param1.base.reg_dbg, pCpu->param2.base.reg_gen32);
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
    if(VBOX_FAILURE(rc))
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
}

#ifdef IN_GC
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
    Assert(pvFault == SELMToFlat(pVM, pRegFrame->eflags, pRegFrame->cs, &pRegFrame->csHid, (RTGCPTR)pRegFrame->eip));

    pVM->em.s.GCPtrInhibitInterrupts = pRegFrame->eip + pCpu->opsize;
    VM_FF_SET(pVM, VM_FF_INHIBIT_INTERRUPTS);

    return VINF_SUCCESS;
}
#endif /* IN_GC */


/**
 * HLT Emulation.
 */
static int emInterpretHlt(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    return VINF_EM_HALT;
}


/**
 * RDTSC Emulation.
 */

/**
 * Interpret RDTSC
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 *
 */
EMDECL(int) EMInterpretRdtsc(PVM pVM, PCPUMCTXCORE pRegFrame)
{
    unsigned uCR4 = CPUMGetGuestCR4(pVM);

    if (uCR4 & X86_CR4_TSD)
        return VERR_EM_INTERPRETER; /* genuine #GP */

    uint64_t uTicks = TMCpuTickGet(pVM);

    pRegFrame->eax = uTicks;
    pRegFrame->edx = (uTicks >> 32ULL);

    return VINF_SUCCESS;
}

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
        STAM_COUNTER_INC(&pVM->em.s.CTXSUFF(pStats)->CTXMID(Stat,FailedUserMode));
        return VERR_EM_INTERPRETER;
    }

#ifdef IN_GC
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
    if (pCpu->prefix & (PREFIX_REPNE | PREFIX_REP | PREFIX_LOCK))
#endif
    {
        //Log(("EMInterpretInstruction: wrong prefix!!\n"));
        STAM_COUNTER_INC(&pVM->em.s.CTXSUFF(pStats)->CTXMID(Stat,FailedPrefix));
        return VERR_EM_INTERPRETER;
    }

    int rc;
    switch (pCpu->pCurInstr->opcode)
    {
#ifdef IN_GC
# define INTERPRET_CASE_EX_LOCK_PARAM3(opcode, Instr, InstrFn, pfnEmulate, pfnEmulateLock) \
        case opcode:\
            if (pCpu->prefix & PREFIX_LOCK) \
                rc = emInterpretLock##InstrFn(pVM, pCpu, pRegFrame, pvFault, pcbSize, pfnEmulateLock); \
            else \
                rc = emInterpret##InstrFn(pVM, pCpu, pRegFrame, pvFault, pcbSize, pfnEmulate); \
            if (VBOX_SUCCESS(rc)) \
                STAM_COUNTER_INC(&pVM->em.s.CTXSUFF(pStats)->CTXMID(Stat,Instr)); \
            else \
                STAM_COUNTER_INC(&pVM->em.s.CTXSUFF(pStats)->CTXMID(Stat,Failed##Instr)); \
            return rc
#else
# define INTERPRET_CASE_EX_LOCK_PARAM3(opcode, Instr, InstrFn, pfnEmulate, pfnEmulateLock) \
        INTERPRET_CASE_EX_PARAM3(opcode, Instr, InstrFn, pfnEmulate)
#endif 
#define INTERPRET_CASE_EX_PARAM3(opcode, Instr, InstrFn, pfnEmulate) \
        case opcode:\
            rc = emInterpret##InstrFn(pVM, pCpu, pRegFrame, pvFault, pcbSize, pfnEmulate); \
            if (VBOX_SUCCESS(rc)) \
                STAM_COUNTER_INC(&pVM->em.s.CTXSUFF(pStats)->CTXMID(Stat,Instr)); \
            else \
                STAM_COUNTER_INC(&pVM->em.s.CTXSUFF(pStats)->CTXMID(Stat,Failed##Instr)); \
            return rc

#define INTERPRET_CASE_EX_PARAM2(opcode, Instr, InstrFn, pfnEmulate) \
            INTERPRET_CASE_EX_PARAM3(opcode, Instr, InstrFn, pfnEmulate)
#define INTERPRET_CASE_EX_LOCK_PARAM2(opcode, Instr, InstrFn, pfnEmulate, pfnEmulateLock) \
            INTERPRET_CASE_EX_LOCK_PARAM3(opcode, Instr, InstrFn, pfnEmulate, pfnEmulateLock)

#define INTERPRET_CASE(opcode, Instr) \
        case opcode:\
            rc = emInterpret##Instr(pVM, pCpu, pRegFrame, pvFault, pcbSize); \
            if (VBOX_SUCCESS(rc)) \
                STAM_COUNTER_INC(&pVM->em.s.CTXSUFF(pStats)->CTXMID(Stat,Instr)); \
            else \
                STAM_COUNTER_INC(&pVM->em.s.CTXSUFF(pStats)->CTXMID(Stat,Failed##Instr)); \
            return rc
#define INTERPRET_STAT_CASE(opcode, Instr) \
        case opcode: STAM_COUNTER_INC(&pVM->em.s.CTXSUFF(pStats)->CTXMID(Stat,Failed##Instr)); return VERR_EM_INTERPRETER;

        INTERPRET_CASE(OP_XCHG,Xchg);
        INTERPRET_CASE_EX_PARAM2(OP_DEC,Dec, IncDec, EMEmulateDec);
        INTERPRET_CASE_EX_PARAM2(OP_INC,Inc, IncDec, EMEmulateInc);
        INTERPRET_CASE(OP_POP,Pop);
        INTERPRET_CASE_EX_LOCK_PARAM3(OP_OR, Or, OrXorAnd, EMEmulateOr, EMEmulateLockOr);
        INTERPRET_CASE_EX_PARAM3(OP_XOR,Xor, OrXorAnd, EMEmulateXor);
        INTERPRET_CASE_EX_PARAM3(OP_AND,And, OrXorAnd, EMEmulateAnd);
        INTERPRET_CASE(OP_MOV,Mov);
        INTERPRET_CASE(OP_INVLPG,InvlPg);
        INTERPRET_CASE(OP_CPUID,CpuId);
        INTERPRET_CASE(OP_MOV_CR,MovCRx);
        INTERPRET_CASE(OP_MOV_DR,MovDRx);
        INTERPRET_CASE(OP_LLDT,LLdt);
        INTERPRET_CASE(OP_CLTS,Clts);
        INTERPRET_CASE(OP_MONITOR, Monitor);
        INTERPRET_CASE(OP_MWAIT, MWait);
        INTERPRET_CASE_EX_PARAM3(OP_ADD,Add, AddSub, EMEmulateAdd);
        INTERPRET_CASE_EX_PARAM3(OP_SUB,Sub, AddSub, EMEmulateSub);
        INTERPRET_CASE(OP_ADC,Adc);
        INTERPRET_CASE_EX_LOCK_PARAM2(OP_BTR,Btr, BitTest, EMEmulateBtr, EMEmulateLockBtr);
        INTERPRET_CASE_EX_PARAM2(OP_BTS,Bts, BitTest, EMEmulateBts);
        INTERPRET_CASE_EX_PARAM2(OP_BTC,Btc, BitTest, EMEmulateBtc);
        INTERPRET_CASE(OP_RDTSC,Rdtsc);
#ifdef IN_GC
        INTERPRET_CASE(OP_STI,Sti);
        INTERPRET_CASE(OP_CMPXCHG, CmpXchg);
        INTERPRET_CASE(OP_CMPXCHG8B, CmpXchg8b);
        INTERPRET_CASE(OP_XADD, XAdd);
#endif
        INTERPRET_CASE(OP_HLT,Hlt);
        INTERPRET_CASE(OP_IRET,Iret);
#ifdef VBOX_WITH_STATISTICS
#ifndef IN_GC
        INTERPRET_STAT_CASE(OP_CMPXCHG,CmpXchg);
        INTERPRET_STAT_CASE(OP_CMPXCHG8B, CmpXchg8b);
        INTERPRET_STAT_CASE(OP_XADD, XAdd);
#endif
        INTERPRET_STAT_CASE(OP_MOVNTPS,MovNTPS);
        INTERPRET_STAT_CASE(OP_STOSWD,StosWD);
        INTERPRET_STAT_CASE(OP_WBINVD,WbInvd);
#endif
        default:
            Log3(("emInterpretInstructionCPU: opcode=%d\n", pCpu->pCurInstr->opcode));
            STAM_COUNTER_INC(&pVM->em.s.CTXSUFF(pStats)->CTXMID(Stat,FailedMisc));
            return VERR_EM_INTERPRETER;
#undef INTERPRET_CASE_EX_PARAM2
#undef INTERPRET_STAT_CASE
#undef INTERPRET_CASE_EX
#undef INTERPRET_CASE
    }
    AssertFailed();
    return VERR_INTERNAL_ERROR;
}


/**
 * Sets the PC for which interrupts should be inhibited.
 *
 * @param   pVM         The VM handle.
 * @param   PC          The PC.
 */
EMDECL(void) EMSetInhibitInterruptsPC(PVM pVM, RTGCUINTPTR PC)
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
EMDECL(RTGCUINTPTR) EMGetInhibitInterruptsPC(PVM pVM)
{
    return pVM->em.s.GCPtrInhibitInterrupts;
}
