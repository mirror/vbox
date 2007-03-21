/* $Id$ */
/** @file
 * EM - Execution Monitor(/Manager) - All contexts
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
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
 *   Internal Functions                                                        *
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
DECLCALLBACK(int32_t) EMReadBytes(RTHCUINTPTR pSrc, uint8_t *pDest, uint32_t size, RTHCUINTPTR dwUserdata)
{
    DISCPUSTATE  *pCpu     = (DISCPUSTATE *)dwUserdata;
    PVM           pVM      = (PVM)pCpu->dwUserData[0];
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

inline int emDisCoreOne(PVM pVM, DISCPUSTATE *pCpu, RTGCUINTPTR InstrGC, uint32_t *pOpsize)
{
    return DISCoreOneEx(InstrGC, pCpu->mode, EMReadBytes, pVM, pCpu, pOpsize);
}

#else

inline int emDisCoreOne(PVM pVM, DISCPUSTATE *pCpu, RTGCUINTPTR InstrGC, uint32_t *pOpsize)
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
    /*
     * Only allow 32-bit code.
     */
    if (SELMIsSelector32Bit(pVM, pRegFrame->eflags, pRegFrame->cs, &pRegFrame->csHid))
    {
        RTGCPTR pbCode;
        int rc = SELMValidateAndConvertCSAddr(pVM, pRegFrame->eflags, pRegFrame->ss, pRegFrame->cs, &pRegFrame->csHid, (RTGCPTR)pRegFrame->eip, &pbCode);
        if (VBOX_SUCCESS(rc))
        {
            uint32_t    cbOp;
            DISCPUSTATE Cpu;
            Cpu.mode = CPUMODE_32BIT;
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
    if (rc == VINF_SUCCESS)
        pCtxCore->eip += cbOp;
    return rc;
#else
    AssertReleaseMsgFailed(("not implemented\n"));
    return VERR_NOT_IMPLEMENTED;
#endif
}


inline int emRamRead(PVM pVM, void *pDest, RTGCPTR GCSrc, uint32_t cb)
{
#ifdef IN_GC
    return MMGCRamRead(pVM, pDest, GCSrc, cb);
#else
    int         rc;
    RTGCPHYS    GCPhys;
    RTGCUINTPTR offset;

    offset = GCSrc & PAGE_OFFSET_MASK;

    rc = PGMPhysGCPtr2GCPhys(pVM, GCSrc, &GCPhys);
    AssertRCReturn(rc, rc);
    PGMPhysRead(pVM, GCPhys + offset, pDest, cb);
    return VINF_SUCCESS;
#endif
}

inline int emRamWrite(PVM pVM, RTGCPTR GCDest, void *pSrc, uint32_t cb)
{
#ifdef IN_GC
    return MMGCRamWrite(pVM, GCDest, pSrc, cb);
#else
    int         rc;
    RTGCPHYS    GCPhys;
    RTGCUINTPTR offset;

    offset = GCDest & PAGE_OFFSET_MASK;
    rc = PGMPhysGCPtr2GCPhys(pVM, GCDest, &GCPhys);
    AssertRCReturn(rc, rc);
    PGMPhysWrite(pVM, GCPhys + offset, pSrc, cb);
    return VINF_SUCCESS;
#endif
}

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
#ifdef IN_GC
                /* Safety check (in theory it could cross a page boundary and fault there though) */
                AssertReturn(pParam1 == (RTGCPTR)pvFault, VERR_EM_INTERPRETER);
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
#ifdef IN_GC
                /* Safety check (in theory it could cross a page boundary and fault there though) */
                AssertReturn(pParam2 == (RTGCPTR)pvFault, VERR_EM_INTERPRETER);
#endif
                rc = emRamRead(pVM, &valpar2, pParam2, param2.size);
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
static int emInterpretIncDec(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
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
#ifdef IN_GC
                /* Safety check (in theory it could cross a page boundary and fault there though) */
                AssertReturn(pParam1 == (RTGCPTR)pvFault, VERR_EM_INTERPRETER);
#endif
                rc = emRamRead(pVM, &valpar1, pParam1, param1.size);
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

            if (pCpu->pCurInstr->opcode == OP_DEC)
                eflags = EMEmulateDec(&valpar1, param1.size);
            else
                eflags = EMEmulateInc(&valpar1, param1.size);

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

            rc = emRamRead(pVM, &valpar1, pStackVal, param1.size);
            if (VBOX_FAILURE(rc))
            {
                AssertMsgFailed(("emRamRead %VGv size=%d failed with %Vrc\n", pParam1, param1.size, rc));
                return VERR_EM_INTERPRETER;
            }

            if (param1.type == PARMTYPE_ADDRESS)
            {
                pParam1 = (RTGCPTR)param1.val.val32;

#ifdef IN_GC
                /* Safety check (in theory it could cross a page boundary and fault there though) */
                AssertMsgReturn(pParam1 == (RTGCPTR)pvFault, ("%VGv != %VGv\n", pParam1, pvFault), VERR_EM_INTERPRETER);
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
 * OR Emulation.
 */
static int emInterpretOr(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
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
#endif
            RTGCPTR  pParam1;
            uint32_t valpar1, valpar2;

            if (pCpu->param1.size != pCpu->param2.size)
            {
                if (pCpu->param1.size < pCpu->param2.size)
                {
                    AssertMsgFailed(("Or at %VGv parameter mismatch %d vs %d!!\n", pRegFrame->eip, pCpu->param1.size, pCpu->param2.size)); /* should never happen! */
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

#ifdef IN_GC
                /* Safety check (in theory it could cross a page boundary and fault there though) */
                AssertReturn(pParam1 == (RTGCPTR)pvFault, VERR_EM_INTERPRETER);
#endif
                rc = emRamRead(pVM, &valpar1, pParam1, param1.size);
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

            /* Data read, emulate OR. */
            uint32_t eflags = EMEmulateOr(&valpar1, valpar2, param2.size);

            /* Update guest's eflags and finish. */
            pRegFrame->eflags.u32 = (pRegFrame->eflags.u32 & ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF))
                                  | (eflags & (X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF));

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
 * XOR Emulation.
 */
static int emInterpretXor(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
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
#endif
            RTGCPTR  pParam1;
            uint32_t valpar1, valpar2;

            if (pCpu->param1.size != pCpu->param2.size)
            {
                if (pCpu->param1.size < pCpu->param2.size)
                {
                    AssertMsgFailed(("Xor at %VGv parameter mismatch %d vs %d!!\n", pRegFrame->eip, pCpu->param1.size, pCpu->param2.size)); /* should never happen! */
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

#ifdef IN_GC
                /* Safety check (in theory it could cross a page boundary and fault there though) */
                AssertReturn(pParam1 == (RTGCPTR)pvFault, VERR_EM_INTERPRETER);
#endif
                rc = emRamRead(pVM, &valpar1, pParam1, param1.size);
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

            /* Data read, emulate XOR. */
            uint32_t eflags = EMEmulateXor(&valpar1, valpar2, param2.size);

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
 * AND Emulation.
 */
static int emInterpretAnd(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
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
#endif
            RTGCPTR  pParam1;
            uint32_t valpar1, valpar2;

            if (pCpu->param1.size != pCpu->param2.size)
            {
                if (pCpu->param1.size < pCpu->param2.size)
                {
                    AssertMsgFailed(("And at %VGv parameter mismatch %d vs %d!!\n", pRegFrame->eip, pCpu->param1.size, pCpu->param2.size)); /* should never happen! */
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

#ifdef IN_GC
                /* Safety check (in theory it could cross a page boundary and fault there though) */
                AssertMsgReturn(pParam1 == pvFault, ("pParam1 = %VGv pvFault = %VGv\n", pParam1, pvFault), VERR_EM_INTERPRETER);
#endif
                rc = emRamRead(pVM, &valpar1, pParam1, param1.size);
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

            /* Data read, emulate AND. */
            uint32_t eflags = EMEmulateAnd(&valpar1, valpar2, param2.size);

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
 * ADD Emulation.
 */
static int emInterpretAdd(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
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
#endif
            RTGCPTR  pParam1;
            uint32_t valpar1, valpar2;

            if (pCpu->param1.size != pCpu->param2.size)
            {
                if (pCpu->param1.size < pCpu->param2.size)
                {
                    AssertMsgFailed(("Add at %VGv parameter mismatch %d vs %d!!\n", pRegFrame->eip, pCpu->param1.size, pCpu->param2.size)); /* should never happen! */
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

#ifdef IN_GC
                /* Safety check (in theory it could cross a page boundary and fault there though) */
                AssertReturn(pParam1 == (RTGCPTR)pvFault, VERR_EM_INTERPRETER);
#endif
                rc = emRamRead(pVM, &valpar1, pParam1, param1.size);
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

            /* Data read, emulate ADD. */
            uint32_t eflags = EMEmulateAdd(&valpar1, valpar2, param2.size);

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
 * @todo combine with add
 */
static int emInterpretAdc(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
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
#endif
            RTGCPTR  pParam1;
            uint32_t valpar1, valpar2;

            if (pCpu->param1.size != pCpu->param2.size)
            {
                if (pCpu->param1.size < pCpu->param2.size)
                {
                    AssertMsgFailed(("Adc at %VGv parameter mismatch %d vs %d!!\n", pRegFrame->eip, pCpu->param1.size, pCpu->param2.size)); /* should never happen! */
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

#ifdef IN_GC
                /* Safety check (in theory it could cross a page boundary and fault there though) */
                AssertReturn(pParam1 == (RTGCPTR)pvFault, VERR_EM_INTERPRETER);
#endif
                rc = emRamRead(pVM, &valpar1, pParam1, param1.size);
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

            /* Data read, emulate ADC. */
            uint32_t eflags;

            if (pRegFrame->eflags.u32 & X86_EFL_CF)
                eflags = EMEmulateAdcWithCarrySet(&valpar1, valpar2, param2.size);
            else
                eflags = EMEmulateAdd(&valpar1, valpar2, param2.size);

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
 * SUB Emulation.
 */
static int emInterpretSub(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
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
#endif
            RTGCPTR  pParam1;
            uint32_t valpar1, valpar2;

            if (pCpu->param1.size != pCpu->param2.size)
            {
                if (pCpu->param1.size < pCpu->param2.size)
                {
                    AssertMsgFailed(("Sub at %VGv parameter mismatch %d vs %d!!\n", pRegFrame->eip, pCpu->param1.size, pCpu->param2.size)); /* should never happen! */
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

#ifdef IN_GC
                /* Safety check (in theory it could cross a page boundary and fault there though) */
                AssertReturn(pParam1 == (RTGCPTR)pvFault, VERR_EM_INTERPRETER);
#endif
                rc = emRamRead(pVM, &valpar1, pParam1, param1.size);
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

            /* Data read, emulate SUB. */
            uint32_t eflags = EMEmulateSub(&valpar1, valpar2, param2.size);

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
                break;

            default:
                return VERR_EM_INTERPRETER;
            }

            Assert(param1.size <= 4 && param1.size > 0);
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

    rc  = emRamRead(pVM, &eip,      (RTGCPTR)pIretStack    , 4);
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
    if (    (OldCr0 & (X86_CR0_TS | X86_CR0_EM | X86_CR0_MP | X86_CR0_AM | X86_CR0_WP))
        !=  (NewCr0 & (X86_CR0_TS | X86_CR0_EM | X86_CR0_MP | X86_CR0_AM | X86_CR0_WP)))
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
    uint32_t Cr0 = CPUMGetGuestCR0(pVM);
    if (!(Cr0 & X86_CR0_TS))
        return VINF_SUCCESS;

#ifdef IN_GC
    /* Need to change the hyper CR0? Doing it the lazy way then. */
    Log(("EMInterpretCLTS: CR0: %#x->%#x => R3\n", Cr0, Cr0 & ~X86_CR0_TS));
    VM_FF_SET(pVM, VM_FF_TO_R3);
#endif
    return CPUMSetGuestCR0(pVM, Cr0 & ~X86_CR0_TS);
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

    int rc = DISFetchReg32(pRegFrame, SrcRegGen, &val32);
    if (VBOX_SUCCESS(rc))
    {
        switch (DestRegCrx)
        {
        case USE_REG_CR0:
            oldval = CPUMGetGuestCR0(pVM);
#ifndef IN_RING3
            /* CR0.WP changes require a reschedule run in ring 3. */
            if ((val32 & X86_CR0_WP) != (oldval & X86_CR0_WP))
                return VERR_EM_INTERPRETER;
#endif
            rc = CPUMSetGuestCR0(pVM, val32); AssertRC(rc); /** @todo CPUSetGuestCR0 stuff should be void, this is silly. */
            val32 = CPUMGetGuestCR0(pVM);
            if (    (oldval & (X86_CR0_PG|X86_CR0_WP|X86_CR0_PE))
                !=  (val32  & (X86_CR0_PG|X86_CR0_WP|X86_CR0_PE)))
            {
                /* global flush */
                rc = PGMFlushTLB(pVM, CPUMGetGuestCR3(pVM), true /* global */);
                AssertRCReturn(rc, rc);
            }
# ifdef IN_GC
            /* Feeling extremely lazy. */
            if (    (oldval & (X86_CR0_TS|X86_CR0_EM|X86_CR0_MP|X86_CR0_AM))
                !=  (val32  & (X86_CR0_TS|X86_CR0_EM|X86_CR0_MP|X86_CR0_AM)))
            {
                Log(("emInterpretMovCRx: CR0: %#x->%#x => R3\n", oldval, val32));
                VM_FF_SET(pVM, VM_FF_TO_R3);
            }
# endif
            return PGMChangeMode(pVM, CPUMGetGuestCR0(pVM), CPUMGetGuestCR4(pVM), 0);

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
#ifndef IN_RING3
            /** @todo is flipping of the X86_CR4_PAE bit handled correctly here? */
#endif
            rc = CPUMSetGuestCR4(pVM, val32); AssertRC(rc);
            val32 = CPUMGetGuestCR4(pVM);
            if (    (oldval & (X86_CR4_PGE|X86_CR4_PAE|X86_CR4_PSE))
                !=  (val32  & (X86_CR4_PGE|X86_CR4_PAE|X86_CR4_PSE)))
            {
                /* global flush */
                rc = PGMFlushTLB(pVM, CPUMGetGuestCR3(pVM), true /* global */);
                AssertRCReturn(rc, rc);
            }
# ifndef IN_RING3 /** @todo check this out IN_RING0! */
            /* Feeling extremely lazy. */
            if (    (oldval & (X86_CR4_OSFSXR|X86_CR4_OSXMMEEXCPT|X86_CR4_PCE|X86_CR4_MCE|X86_CR4_PAE|X86_CR4_DE|X86_CR4_TSD|X86_CR4_PVI|X86_CR4_VME))
                !=  (val32  & (X86_CR4_OSFSXR|X86_CR4_OSXMMEEXCPT|X86_CR4_PCE|X86_CR4_MCE|X86_CR4_PAE|X86_CR4_DE|X86_CR4_TSD|X86_CR4_PVI|X86_CR4_VME)))
            {
                Log(("emInterpretMovCRx: CR4: %#x->%#x => R3\n", oldval, val32));
                VM_FF_SET(pVM, VM_FF_TO_R3);
            }
# endif
            return PGMChangeMode(pVM, CPUMGetGuestCR0(pVM), CPUMGetGuestCR4(pVM), 0);

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


#ifdef IN_GC
/**
 * RDTSC Emulation.
 */
static int emInterpretRdtsc(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    unsigned uCR4 = CPUMGetGuestCR4(pVM);

    if (uCR4 & X86_CR4_TSD)
        return VERR_EM_INTERPRETER; /* genuine #GP */

    uint64_t uTicks = TMCpuTickGet(pVM);

    pRegFrame->eax = uTicks;
    pRegFrame->edx = (uTicks >> 32ULL);

    return VINF_SUCCESS;
}
#endif

/**
 * MONITOR Emulation.
 */
static int emInterpretMonitor(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    if (pRegFrame->ecx != 0)
        return VERR_EM_INTERPRETER; /* illegal value. */

#ifdef IN_GC
    if (pRegFrame->eflags.Bits.u1VM || (pRegFrame->ss & X86_SEL_RPL) != 1)
#else
    if (pRegFrame->eflags.Bits.u1VM || (pRegFrame->ss & X86_SEL_RPL) != 0)
#endif
        return VERR_EM_INTERPRETER; /* supervisor only */

    return VINF_SUCCESS;
}


/**
 * MWAIT Emulation.
 */
static int emInterpretMWait(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize)
{
    if (pRegFrame->ecx != 0)
        return VERR_EM_INTERPRETER; /* illegal value. */

#ifdef IN_GC
    if (pRegFrame->eflags.Bits.u1VM || (pRegFrame->ss & X86_SEL_RPL) != 1)
#else
    if (pRegFrame->eflags.Bits.u1VM || (pRegFrame->ss & X86_SEL_RPL) != 0)
#endif
        return VERR_EM_INTERPRETER; /* supervisor only */

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
#ifdef IN_GC
    if (pRegFrame->eflags.Bits.u1VM || (pRegFrame->ss & X86_SEL_RPL) != 1)
#else
    if (pRegFrame->eflags.Bits.u1VM || (pRegFrame->ss & X86_SEL_RPL) != 0)
#endif
    {
        Log(("WARNING: refusing instruction emulation for user-mode code!!\n"));
        STAM_COUNTER_INC(&pVM->em.s.CTXSUFF(pStats)->CTXMID(Stat,FailedUserMode));
        return VERR_EM_INTERPRETER;
    }

    /* In HWACCM mode we can execute 16 bits code. Our emulation above can't cope with that yet. */
    /** @note if not in HWACCM mode, then we will never execute 16 bits code, so don't bother checking. */
    if (HWACCMIsEnabled(pVM) && !SELMIsSelector32Bit(pVM, pRegFrame->eflags, pRegFrame->cs, &pRegFrame->csHid))
        return VERR_EM_INTERPRETER;

    /** @note we could ignore PREFIX_LOCK here. Need to take special precautions when/if we support SMP in the guest.
     */
    if (pCpu->prefix & (PREFIX_REPNE | PREFIX_REP | PREFIX_SEG | PREFIX_LOCK))
    {
        //Log(("EMInterpretInstruction: wrong prefix!!\n"));
        STAM_COUNTER_INC(&pVM->em.s.CTXSUFF(pStats)->CTXMID(Stat,FailedPrefix));
        return VERR_EM_INTERPRETER;
    }

    int rc;
    switch (pCpu->pCurInstr->opcode)
    {
#define INTERPRET_CASE_EX(opcode,Instr,InstrFn) \
        case opcode:\
            rc = emInterpret##InstrFn(pVM, pCpu, pRegFrame, pvFault, pcbSize); \
            if (VBOX_SUCCESS(rc)) \
                STAM_COUNTER_INC(&pVM->em.s.CTXSUFF(pStats)->CTXMID(Stat,Instr)); \
            else \
                STAM_COUNTER_INC(&pVM->em.s.CTXSUFF(pStats)->CTXMID(Stat,Failed##Instr)); \
            return rc
#define INTERPRET_CASE(opcode,Instr) INTERPRET_CASE_EX(opcode,Instr,Instr)
#define INTERPRET_STAT_CASE(opcode,Instr) \
        case opcode: STAM_COUNTER_INC(&pVM->em.s.CTXSUFF(pStats)->CTXMID(Stat,Failed##Instr)); return VERR_EM_INTERPRETER;

        INTERPRET_CASE(OP_XCHG,Xchg);
        INTERPRET_CASE_EX(OP_DEC,Dec,IncDec);
        INTERPRET_CASE_EX(OP_INC,Inc,IncDec);
        INTERPRET_CASE(OP_POP,Pop);
        INTERPRET_CASE(OP_OR,Or);
        INTERPRET_CASE(OP_XOR,Xor);
        INTERPRET_CASE(OP_MOV,Mov);
        INTERPRET_CASE(OP_AND,And);
        INTERPRET_CASE(OP_INVLPG,InvlPg);
        INTERPRET_CASE(OP_CPUID,CpuId);
        INTERPRET_CASE(OP_MOV_CR,MovCRx);
        INTERPRET_CASE(OP_MOV_DR,MovDRx);
        INTERPRET_CASE(OP_LLDT,LLdt);
        INTERPRET_CASE(OP_MONITOR, Monitor);
        INTERPRET_CASE(OP_MWAIT, MWait);
        INTERPRET_CASE(OP_ADD,Add);
        INTERPRET_CASE(OP_ADC,Adc);
        INTERPRET_CASE(OP_SUB,Sub);
#ifdef IN_GC
        INTERPRET_CASE(OP_RDTSC,Rdtsc);
        INTERPRET_CASE(OP_STI,Sti);
#endif
        INTERPRET_CASE(OP_HLT,Hlt);
        INTERPRET_CASE(OP_IRET,Iret);
#ifdef VBOX_WITH_STATISTICS
        INTERPRET_STAT_CASE(OP_BTR,Btr);
        INTERPRET_STAT_CASE(OP_BTS,Bts);
        INTERPRET_STAT_CASE(OP_CMPXCHG,CmpXchg);
        INTERPRET_STAT_CASE(OP_MOVNTPS,MovNTPS);
        INTERPRET_STAT_CASE(OP_STOSWD,StosWD);
        INTERPRET_STAT_CASE(OP_WBINVD,WbInvd);
#endif
        default:
            Log3(("emInterpretInstructionCPU: opcode=%d\n", pCpu->pCurInstr->opcode));
            STAM_COUNTER_INC(&pVM->em.s.CTXSUFF(pStats)->CTXMID(Stat,FailedMisc));
            return VERR_EM_INTERPRETER;
#undef INTERPRET_STAT_CASE
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
