/* $Id$ */
/** @file
 * IOM - Input / Output Monitor - Raw-Mode Context.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_IOM
#include <VBox/vmm/iom.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/selm.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/iem.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/trpm.h>
#include "IOMInternal.h"
#include <VBox/vmm/vm.h>

#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/string.h>


#ifdef VBOX_WITH_3RD_IEM_STEP
/**
 * Converts disassembler mode to IEM mode.
 * @return IEM CPU mode.
 * @param  enmDisMode   Disassembler CPU mode.
 */
DECLINLINE(IEMMODE) iomDisModeToIemMode(DISCPUMODE enmDisMode)
{
    switch (enmDisMode)
    {
        case DISCPUMODE_16BIT: return IEMMODE_16BIT;
        case DISCPUMODE_32BIT: return IEMMODE_32BIT;
        case DISCPUMODE_64BIT: return IEMMODE_64BIT;
        default:
            AssertFailed();
            return IEMMODE_32BIT;
    }
}
#endif



/**
 * IN <AL|AX|EAX>, <DX|imm16>
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the
 *                                      status code must be passed on to EM.
 * @retval  VINF_IOM_R3_IOPORT_READ     Defer the read to ring-3. (R0/GC only)
 * @retval  VINF_EM_RAW_GUEST_TRAP      The exception was left pending. (TRPMRaiseXcptErr)
 * @retval  VINF_TRPM_XCPT_DISPATCHED   The exception was raised and dispatched for raw-mode execution. (TRPMRaiseXcptErr)
 * @retval  VINF_EM_RESCHEDULE_REM      The exception was dispatched and cannot be executed in raw-mode. (TRPMRaiseXcptErr)
 *
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pRegFrame   Pointer to CPUMCTXCORE guest registers structure.
 * @param   pCpu        Disassembler CPU state.
 */
static VBOXSTRICTRC iomRCInterpretIN(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu)
{
#ifdef IN_RC
    STAM_COUNTER_INC(&pVM->iom.s.StatInstIn);
#endif

    /*
     * Get port number from second parameter.
     * And get the register size from the first parameter.
     */
    uint64_t    uPort = 0;
    unsigned    cbSize = 0;
    bool fRc = iomGetRegImmData(pCpu, &pCpu->Param2, pRegFrame, &uPort, &cbSize);
    AssertMsg(fRc, ("Failed to get reg/imm port number!\n")); NOREF(fRc);

    cbSize = DISGetParamSize(pCpu, &pCpu->Param1);
    Assert(cbSize > 0);
    VBOXSTRICTRC rcStrict = IOMInterpretCheckPortIOAccess(pVM, pRegFrame, uPort, cbSize);
    if (rcStrict == VINF_SUCCESS)
    {
        /*
         * Attempt to read the port.
         */
        uint32_t u32Data = UINT32_C(0xffffffff);
        rcStrict = IOMIOPortRead(pVM, pVCpu, uPort, &u32Data, cbSize);
        if (IOM_SUCCESS(rcStrict))
        {
            /*
             * Store the result in the AL|AX|EAX register.
             */
            fRc = iomSaveDataToReg(pCpu, &pCpu->Param1, pRegFrame, u32Data);
            AssertMsg(fRc, ("Failed to store register value!\n")); NOREF(fRc);
        }
        else
            AssertMsg(rcStrict == VINF_IOM_R3_IOPORT_READ || RT_FAILURE(rcStrict), ("%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
    }
    else
        AssertMsg(rcStrict == VINF_EM_RAW_GUEST_TRAP || rcStrict == VINF_TRPM_XCPT_DISPATCHED || RT_FAILURE(rcStrict), ("%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));

    return rcStrict;
}


/**
 * OUT <DX|imm16>, <AL|AX|EAX>
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the
 *                                      status code must be passed on to EM.
 * @retval  VINF_IOM_R3_IOPORT_WRITE    Defer the write to ring-3. (R0/GC only)
 * @retval  VINF_IOM_R3_IOPORT_COMMIT_WRITE Defer the write to ring-3. (R0/GC only)
 * @retval  VINF_EM_RAW_GUEST_TRAP      The exception was left pending. (TRPMRaiseXcptErr)
 * @retval  VINF_TRPM_XCPT_DISPATCHED   The exception was raised and dispatched for raw-mode execution. (TRPMRaiseXcptErr)
 * @retval  VINF_EM_RESCHEDULE_REM      The exception was dispatched and cannot be executed in raw-mode. (TRPMRaiseXcptErr)
 *
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pRegFrame   Pointer to CPUMCTXCORE guest registers structure.
 * @param   pCpu        Disassembler CPU state.
 */
static VBOXSTRICTRC iomRCInterpretOUT(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu)
{
#ifdef IN_RC
    STAM_COUNTER_INC(&pVM->iom.s.StatInstOut);
#endif

    /*
     * Get port number from first parameter.
     * And get the register size and value from the second parameter.
     */
    uint64_t    uPort = 0;
    unsigned    cbSize = 0;
    bool fRc = iomGetRegImmData(pCpu, &pCpu->Param1, pRegFrame, &uPort, &cbSize);
    AssertMsg(fRc, ("Failed to get reg/imm port number!\n")); NOREF(fRc);

    VBOXSTRICTRC rcStrict = IOMInterpretCheckPortIOAccess(pVM, pRegFrame, uPort, cbSize);
    if (rcStrict == VINF_SUCCESS)
    {
        uint64_t u64Data = 0;
        fRc = iomGetRegImmData(pCpu, &pCpu->Param2, pRegFrame, &u64Data, &cbSize);
        AssertMsg(fRc, ("Failed to get reg value!\n")); NOREF(fRc);

        /*
         * Attempt to write to the port.
         */
        rcStrict = IOMIOPortWrite(pVM, pVCpu, uPort, u64Data, cbSize);
        AssertMsg(rcStrict == VINF_SUCCESS || rcStrict == VINF_IOM_R3_IOPORT_WRITE || rcStrict == VINF_IOM_R3_IOPORT_COMMIT_WRITE
                  || (rcStrict >= VINF_EM_FIRST && rcStrict <= VINF_EM_LAST) || RT_FAILURE(rcStrict),
                  ("%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
    }
    else
        AssertMsg(rcStrict == VINF_EM_RAW_GUEST_TRAP || rcStrict == VINF_TRPM_XCPT_DISPATCHED || RT_FAILURE(rcStrict), ("%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
    return rcStrict;
}


/**
 * [REP*] INSB/INSW/INSD
 * ES:EDI,DX[,ECX]
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the
 *                                      status code must be passed on to EM.
 * @retval  VINF_IOM_R3_IOPORT_READ     Defer the read to ring-3. (R0/GC only)
 * @retval  VINF_EM_RAW_EMULATE_INSTR   Defer the read to the REM.
 * @retval  VINF_EM_RAW_GUEST_TRAP      The exception was left pending. (TRPMRaiseXcptErr)
 * @retval  VINF_TRPM_XCPT_DISPATCHED   The exception was raised and dispatched for raw-mode execution. (TRPMRaiseXcptErr)
 * @retval  VINF_EM_RESCHEDULE_REM      The exception was dispatched and cannot be executed in raw-mode. (TRPMRaiseXcptErr)
 *
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pRegFrame   Pointer to CPUMCTXCORE guest registers structure.
 * @param   pCpu        Disassembler CPU state.
 */
static VBOXSTRICTRC iomRCInterpretINS(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu)
{
#ifdef VBOX_WITH_3RD_IEM_STEP
    uint8_t cbValue = pCpu->pCurInstr->uOpcode == OP_INSB ? 1
                    : pCpu->uOpMode == DISCPUMODE_16BIT ? 2 : 4;       /* dword in both 32 & 64 bits mode */
    return IEMExecStringIoRead(pVCpu,
                               cbValue,
                               iomDisModeToIemMode((DISCPUMODE)pCpu->uCpuMode),
                               RT_BOOL(pCpu->fPrefix & (DISPREFIX_REPNE | DISPREFIX_REP)),
                               pCpu->cbInstr);
#else
    /*
     * Get port number directly from the register (no need to bother the
     * disassembler). And get the I/O register size from the opcode / prefix.
     */
    RTIOPORT    Port = pRegFrame->edx & 0xffff;
    unsigned    cb;
    if (pCpu->pCurInstr->uOpcode == OP_INSB)
        cb = 1;
    else
        cb = (pCpu->uOpMode == DISCPUMODE_16BIT) ? 2 : 4;       /* dword in both 32 & 64 bits mode */

    VBOXSTRICTRC rcStrict = IOMInterpretCheckPortIOAccess(pVM, pRegFrame, Port, cb);
    if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
    {
        AssertMsg(rcStrict == VINF_EM_RAW_GUEST_TRAP || rcStrict == VINF_TRPM_XCPT_DISPATCHED || RT_FAILURE(rcStrict), ("%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
        return rcStrict;
    }

    return IOMInterpretINSEx(pVM, pVCpu, pRegFrame, Port, pCpu->fPrefix, (DISCPUMODE)pCpu->uAddrMode, cb);
#endif
}


/**
 * [REP*] OUTSB/OUTSW/OUTSD
 * DS:ESI,DX[,ECX]
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the
 *                                      status code must be passed on to EM.
 * @retval  VINF_IOM_R3_IOPORT_WRITE    Defer the write to ring-3. (R0/GC only)
 * @retval  VINF_IOM_R3_IOPORT_COMMIT_WRITE    Defer the write to ring-3. (R0/GC only)
 * @retval  VINF_EM_RAW_EMULATE_INSTR   Defer the write to the REM.
 * @retval  VINF_EM_RAW_GUEST_TRAP      The exception was left pending. (TRPMRaiseXcptErr)
 * @retval  VINF_TRPM_XCPT_DISPATCHED   The exception was raised and dispatched for raw-mode execution. (TRPMRaiseXcptErr)
 * @retval  VINF_EM_RESCHEDULE_REM      The exception was dispatched and cannot be executed in raw-mode. (TRPMRaiseXcptErr)
 *
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pRegFrame   Pointer to CPUMCTXCORE guest registers structure.
 * @param   pCpu        Disassembler CPU state.
 */
static VBOXSTRICTRC iomRCInterpretOUTS(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu)
{
#ifdef VBOX_WITH_3RD_IEM_STEP
    uint8_t cbValue = pCpu->pCurInstr->uOpcode == OP_OUTSB ? 1
                    : pCpu->uOpMode == DISCPUMODE_16BIT ? 2 : 4;       /* dword in both 32 & 64 bits mode */
    return IEMExecStringIoWrite(pVCpu,
                                cbValue,
                                iomDisModeToIemMode((DISCPUMODE)pCpu->uCpuMode),
                                RT_BOOL(pCpu->fPrefix & (DISPREFIX_REPNE | DISPREFIX_REP)),
                                pCpu->cbInstr,
                                pCpu->fPrefix & DISPREFIX_SEG ? pCpu->idxSegPrefix : X86_SREG_DS);
#else
    /*
     * Get port number from the first parameter.
     * And get the I/O register size from the opcode / prefix.
     */
    uint64_t    Port = 0;
    unsigned    cb;
    bool fRc = iomGetRegImmData(pCpu, &pCpu->Param1, pRegFrame, &Port, &cb);
    AssertMsg(fRc, ("Failed to get reg/imm port number!\n")); NOREF(fRc);
    if (pCpu->pCurInstr->uOpcode == OP_OUTSB)
        cb = 1;
    else
        cb = (pCpu->uOpMode == DISCPUMODE_16BIT) ? 2 : 4;       /* dword in both 32 & 64 bits mode */

    VBOXSTRICTRC rcStrict = IOMInterpretCheckPortIOAccess(pVM, pRegFrame, Port, cb);
    if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
    {
        AssertMsg(rcStrict == VINF_EM_RAW_GUEST_TRAP || rcStrict == VINF_TRPM_XCPT_DISPATCHED || RT_FAILURE(rcStrict), ("%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
        return rcStrict;
    }

    return IOMInterpretOUTSEx(pVM, pVCpu, pRegFrame, Port, pCpu->fPrefix, (DISCPUMODE)pCpu->uAddrMode, cb);
#endif
}



/**
 * Attempts to service an IN/OUT instruction.
 *
 * The \#GP trap handler in RC will call this function if the opcode causing
 * the trap is a in or out type instruction. (Call it indirectly via EM that
 * is.)
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the
 *                                      status code must be passed on to EM.
 * @retval  VINF_EM_RESCHEDULE_REM      The exception was dispatched and cannot be executed in raw-mode. (TRPMRaiseXcptErr)
 * @retval  VINF_EM_RAW_EMULATE_INSTR   Defer the read to the REM.
 * @retval  VINF_IOM_R3_IOPORT_READ     Defer the read to ring-3.
 * @retval  VINF_EM_RAW_GUEST_TRAP      The exception was left pending. (TRPMRaiseXcptErr)
 * @retval  VINF_TRPM_XCPT_DISPATCHED   The exception was raised and dispatched for raw-mode execution. (TRPMRaiseXcptErr)
 *
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pRegFrame   Pointer to CPUMCTXCORE guest registers structure.
 * @param   pCpu        Disassembler CPU state.
 */
VMMRCDECL(VBOXSTRICTRC) IOMRCIOPortHandler(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu)
{
    switch (pCpu->pCurInstr->uOpcode)
    {
        case OP_IN:
            return iomRCInterpretIN(pVM, pVCpu, pRegFrame, pCpu);

        case OP_OUT:
            return iomRCInterpretOUT(pVM, pVCpu, pRegFrame, pCpu);

        case OP_INSB:
        case OP_INSWD:
            return iomRCInterpretINS(pVM, pVCpu, pRegFrame, pCpu);

        case OP_OUTSB:
        case OP_OUTSWD:
            return iomRCInterpretOUTS(pVM, pVCpu, pRegFrame, pCpu);

        /*
         * The opcode wasn't know to us, freak out.
         */
        default:
            AssertMsgFailed(("Unknown I/O port access opcode %d.\n", pCpu->pCurInstr->uOpcode));
            return VERR_IOM_IOPORT_UNKNOWN_OPCODE;
    }
}

