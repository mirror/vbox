/* $Id$ */
/** @file
 * IOM - Input / Output Monitor - Guest Context.
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
#define LOG_GROUP LOG_GROUP_IOM
#include <VBox/iom.h>
#include <VBox/cpum.h>
#include <VBox/pgm.h>
#include <VBox/selm.h>
#include <VBox/mm.h>
#include <VBox/em.h>
#include <VBox/pgm.h>
#include <VBox/trpm.h>
#include "IOMInternal.h"
#include <VBox/vm.h>

#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/string.h>


/** @def IOMGC_MOVS_SUPPORT
 * Define IOMGC_MOVS_SUPPORT for movsb/w/d support in GC.
 */
#define IOMGC_MOVS_SUPPORT


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
#if 0
static bool iomGCCalcParamEA(PDISCPUSTATE pCpu, POP_PARAMETER pParam, PCPUMCTXCORE pRegFrame, void **ppAddr);
#endif
static unsigned iomGCGetRegSize(PDISCPUSTATE pCpu, PCOP_PARAMETER pParam);
static bool iomGCGetRegImmData(PDISCPUSTATE pCpu, PCOP_PARAMETER pParam, PCPUMCTXCORE pRegFrame, uint32_t *pu32Data, unsigned *pcbSize);
static bool iomGCSaveDataToReg(PDISCPUSTATE pCpu, PCOP_PARAMETER pParam, PCPUMCTXCORE pRegFrame, uint32_t u32Data);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/

/**
 * Array for fast recode of the operand size (1/2/4/8 bytes) to bit shift value.
 */
static const unsigned g_aSize2Shift[] =
{
    ~0,    /* 0 - invalid */
    0,     /* *1 == 2^0 */
    1,     /* *2 == 2^1 */
    ~0,    /* 3 - invalid */
    2,     /* *4 == 2^2 */
    ~0,    /* 5 - invalid */
    ~0,    /* 6 - invalid */
    ~0,    /* 7 - invalid */
    3      /* *8 == 2^3 */
};

/**
 * Macro for fast recode of the operand size (1/2/4/8 bytes) to bit shift value.
 */
#define SIZE2SHIFT(cb) (g_aSize2Shift[cb])


#if 0
/**
 * Calculates effective address (offset from current segment register) for
 * instruction parameter, i.e. [eax + esi*4 + 1234h] -> virtual address.
 *
 * @returns true on success.
 * @param   pCpu                Pointer to current disassembler context.
 * @param   pParam              Pointer to parameter of instruction to calc EA.
 * @param   pRegFrame           Pointer to CPUMCTXCORE guest structure.
 * @param   ppAddr              Where to store result address.
 */
static bool iomGCCalcParamEA(PDISCPUSTATE pCpu, POP_PARAMETER pParam, PCPUMCTXCORE pRegFrame, void **ppAddr)
{
    uint8_t *pAddr = 0;

    if (pCpu->addrmode == CPUMODE_32BIT)
    {
        /* 32-bit addressing. */
        if (pParam->flags & USE_BASE)
            pAddr += ACCESS_REG32(pRegFrame, pParam->base.reg_gen32);
        if (pParam->flags & USE_INDEX)
        {
            unsigned i = ACCESS_REG32(pRegFrame, pParam->index.reg_gen);
            if (pParam->flags & USE_SCALE)
                i *= pParam->scale;
            pAddr += i;
        }
        if (pParam->flags & USE_DISPLACEMENT8)
            pAddr += pParam->disp8;
        else
            if (pParam->flags & USE_DISPLACEMENT16)
            pAddr += pParam->disp16;
        else
            if (pParam->flags & USE_DISPLACEMENT32)
            pAddr += pParam->disp32;

        if (pParam->flags & (USE_BASE | USE_INDEX | USE_DISPLACEMENT8 | USE_DISPLACEMENT16 | USE_DISPLACEMENT32))
        {
            /* EA present in parameter. */
            *ppAddr = pAddr;
            return true;
        }
    }
    else
    {
        /* 16-bit addressing. */
        if (pParam->flags & USE_BASE)
            pAddr += ACCESS_REG16(pRegFrame, pParam->base.reg_gen16);
        if (pParam->flags & USE_INDEX)
            pAddr += ACCESS_REG16(pRegFrame, pParam->index.reg_gen);
        if (pParam->flags & USE_DISPLACEMENT8)
            pAddr += pParam->disp8;
        else
            if (pParam->flags & USE_DISPLACEMENT16)
            pAddr += pParam->disp16;

        if (pParam->flags & (USE_BASE | USE_INDEX | USE_DISPLACEMENT8 | USE_DISPLACEMENT16))
        {
            /* EA present in parameter. */
            *ppAddr = pAddr;
            return true;
        }
    }

    /* Error exit. */
    return false;
}
#endif

/**
 * Calculates the size of register parameter.
 *
 * @returns 1, 2, 4 on success.
 * @returns 0 if non-register parameter.
 * @param   pCpu                Pointer to current disassembler context.
 * @param   pParam              Pointer to parameter of instruction to proccess.
 */
static unsigned iomGCGetRegSize(PDISCPUSTATE pCpu, PCOP_PARAMETER pParam)
{
    if (pParam->flags & (USE_BASE | USE_INDEX | USE_SCALE | USE_DISPLACEMENT8 | USE_DISPLACEMENT16 | USE_DISPLACEMENT32 | USE_IMMEDIATE8 | USE_IMMEDIATE16 | USE_IMMEDIATE32 | USE_IMMEDIATE16_SX8 | USE_IMMEDIATE32_SX8))
        return 0;

    if (pParam->flags & USE_REG_GEN32)
        return 4;

    if (pParam->flags & USE_REG_GEN16)
        return 2;

    if (pParam->flags & USE_REG_GEN8)
        return 1;

    if (pParam->flags & USE_REG_SEG)
        return 2;
    return 0;
}

/**
 * Returns the contents of register or immediate data of instruction's parameter.
 *
 * @returns true on success.
 *
 * @param   pCpu                Pointer to current disassembler context.
 * @param   pParam              Pointer to parameter of instruction to proccess.
 * @param   pRegFrame           Pointer to CPUMCTXCORE guest structure.
 * @param   pu32Data            Where to store retrieved data.
 * @param   pcbSize             Where to store the size of data (1, 2, 4).
 */
static bool iomGCGetRegImmData(PDISCPUSTATE pCpu, PCOP_PARAMETER pParam, PCPUMCTXCORE pRegFrame, uint32_t *pu32Data, unsigned *pcbSize)
{
    if (pParam->flags & (USE_BASE | USE_INDEX | USE_SCALE | USE_DISPLACEMENT8 | USE_DISPLACEMENT16 | USE_DISPLACEMENT32))
    {
        *pcbSize  = 0;
        *pu32Data = 0;
        return false;
    }

    if (pParam->flags & USE_REG_GEN32)
    {
        *pcbSize  = 4;
        DISFetchReg32(pRegFrame, pParam->base.reg_gen32, pu32Data);
        return true;
    }

    if (pParam->flags & USE_REG_GEN16)
    {
        *pcbSize  = 2;
        DISFetchReg16(pRegFrame, pParam->base.reg_gen16, (uint16_t *)pu32Data);
        return true;
    }

    if (pParam->flags & USE_REG_GEN8)
    {
        *pcbSize  = 1;
        DISFetchReg8(pRegFrame, pParam->base.reg_gen8, (uint8_t *)pu32Data);
        return true;
    }

    if (pParam->flags & (USE_IMMEDIATE32|USE_IMMEDIATE32_SX8))
    {
        *pcbSize  = 4;
        *pu32Data = (uint32_t)pParam->parval;
        return true;
    }

    if (pParam->flags & (USE_IMMEDIATE16|USE_IMMEDIATE16_SX8))
    {
        *pcbSize  = 2;
        *pu32Data = (uint16_t)pParam->parval;
        return true;
    }

    if (pParam->flags & USE_IMMEDIATE8)
    {
        *pcbSize  = 1;
        *pu32Data = (uint8_t)pParam->parval;
        return true;
    }

    if (pParam->flags & USE_REG_SEG)
    {
        *pcbSize  = 2;
        DISFetchRegSeg(pRegFrame, pParam->base.reg_seg, (RTSEL *)pu32Data);
        return true;
    } /* Else - error. */

    *pcbSize  = 0;
    *pu32Data = 0;
    return false;
}


/**
 * Saves data to 8/16/32 general purpose or segment register defined by
 * instruction's parameter.
 *
 * @returns true on success.
 * @param   pCpu                Pointer to current disassembler context.
 * @param   pParam              Pointer to parameter of instruction to proccess.
 * @param   pRegFrame           Pointer to CPUMCTXCORE guest structure.
 * @param   u32Data             8/16/32 bit data to store.
 */
static bool iomGCSaveDataToReg(PDISCPUSTATE pCpu, PCOP_PARAMETER pParam, PCPUMCTXCORE pRegFrame, unsigned u32Data)
{
    if (pParam->flags & (USE_BASE | USE_INDEX | USE_SCALE | USE_DISPLACEMENT8 | USE_DISPLACEMENT16 | USE_DISPLACEMENT32 | USE_IMMEDIATE8 | USE_IMMEDIATE16 | USE_IMMEDIATE32 | USE_IMMEDIATE32_SX8 | USE_IMMEDIATE16_SX8))
    {
        return false;
    }

    if (pParam->flags & USE_REG_GEN32)
    {
        DISWriteReg32(pRegFrame, pParam->base.reg_gen32, u32Data);
        return true;
    }

    if (pParam->flags & USE_REG_GEN16)
    {
        DISWriteReg16(pRegFrame, pParam->base.reg_gen16, (uint16_t)u32Data);
        return true;
    }

    if (pParam->flags & USE_REG_GEN8)
    {
        DISWriteReg8(pRegFrame, pParam->base.reg_gen8, (uint8_t)u32Data);
        return true;
    }

    if (pParam->flags & USE_REG_SEG)
    {
        DISWriteRegSeg(pRegFrame, pParam->base.reg_seg, (RTSEL)u32Data);
        return true;
    }

    /* Else - error. */
    return false;
}


/*
 * Internal - statistics only.
 */
inline void iomGCMMIOStatLength(PVM pVM, unsigned cb)
{
#ifdef VBOX_WITH_STATISTICS
    switch (cb)
    {
        case 1:
            STAM_COUNTER_INC(&pVM->iom.s.StatGCMMIO1Byte);
            break;
        case 2:
            STAM_COUNTER_INC(&pVM->iom.s.StatGCMMIO2Bytes);
            break;
        case 4:
            STAM_COUNTER_INC(&pVM->iom.s.StatGCMMIO4Bytes);
            break;
        default:
            /* No way. */
            AssertMsgFailed(("Invalid data length %d\n", cb));
            break;
    }
#else
    NOREF(pVM); NOREF(cb);
#endif
}


/**
 * IN <AL|AX|EAX>, <DX|imm16>
 *
 * @returns VBox status code.
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Pointer to CPUMCTXCORE guest registers structure.
 * @param   pCpu        Disassembler CPU state.
 */
IOMDECL(int) IOMInterpretIN(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu)
{
#ifdef VBOX_WITH_STATISTICS
    STAM_COUNTER_INC(&pVM->iom.s.StatGCInstIn);
#endif

    /*
     * Get port number from second parameter.
     * And get the register size from the first parameter.
     */
    uint32_t    uPort = 0;
    unsigned    cbSize = 0;
    bool fRc = iomGCGetRegImmData(pCpu, &pCpu->param2, pRegFrame, &uPort, &cbSize);
    AssertMsg(fRc, ("Failed to get reg/imm port number!\n")); NOREF(fRc);

    cbSize = iomGCGetRegSize(pCpu, &pCpu->param1);
    Assert(cbSize > 0);
    int rc = IOMInterpretCheckPortIOAccess(pVM, pRegFrame, uPort, cbSize);
    if (rc == VINF_SUCCESS)
    {
        /*
         * Attemp to read the port.
         */
        uint32_t    u32Data = ~0U;
        rc = IOMIOPortRead(pVM, uPort, &u32Data, cbSize);
        if (rc == VINF_SUCCESS)
        {
            /*
             * Store the result in the AL|AX|EAX register.
             */
            fRc = iomGCSaveDataToReg(pCpu, &pCpu->param1, pRegFrame, u32Data);
            AssertMsg(fRc, ("Failed to store register value!\n")); NOREF(fRc);
        }
    }
    return rc;
}


/**
 * OUT <DX|imm16>, <AL|AX|EAX>
 *
 * @returns VBox status code.
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Pointer to CPUMCTXCORE guest registers structure.
 * @param   pCpu        Disassembler CPU state.
 */
IOMDECL(int) IOMInterpretOUT(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu)
{
#ifdef VBOX_WITH_STATISTICS
    STAM_COUNTER_INC(&pVM->iom.s.StatGCInstOut);
#endif

    /*
     * Get port number from first parameter.
     * And get the register size and value from the second parameter.
     */
    uint32_t    uPort = 0;
    unsigned    cbSize = 0;
    bool fRc = iomGCGetRegImmData(pCpu, &pCpu->param1, pRegFrame, &uPort, &cbSize);
    AssertMsg(fRc, ("Failed to get reg/imm port number!\n")); NOREF(fRc);

    int rc = IOMInterpretCheckPortIOAccess(pVM, pRegFrame, uPort, cbSize);
    if (rc == VINF_SUCCESS)
    {
        uint32_t    u32Data = 0;
        fRc = iomGCGetRegImmData(pCpu, &pCpu->param2, pRegFrame, &u32Data, &cbSize);
        AssertMsg(fRc, ("Failed to get reg value!\n")); NOREF(fRc);

        /*
         * Attemp to write to the port.
         */
        rc = IOMIOPortWrite(pVM, uPort, u32Data, cbSize);
    }
    return rc;
}


/**
 * [REP*] INSB/INSW/INSD
 * ES:EDI,DX[,ECX]
 *
 * @returns VBox status code.
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Pointer to CPUMCTXCORE guest registers structure.
 * @param   pCpu        Disassembler CPU state.
 */
IOMDECL(int) IOMInterpretINS(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu)
{
#ifdef VBOX_WITH_STATISTICS
    STAM_COUNTER_INC(&pVM->iom.s.StatGCInstIns);
#endif

    /*
     * We do not support REPNE, 16-bit addressing or decrementing destination
     * pointer. Segment prefixes are deliberately ignored, as per the instruction specification.
     */
    if (   pCpu->prefix & PREFIX_REPNE
        || (pCpu->addrmode != CPUMODE_32BIT)
        || pRegFrame->eflags.Bits.u1DF)
        return VINF_IOM_HC_IOPORT_READ;

    /*
     * Get port number directly from the register (no need to bother the
     * disassembler). And get the I/O register size from the opcode / prefix.
     */
    uint32_t    uPort = pRegFrame->edx & 0xffff;
    unsigned    cbSize = 0;
    if (pCpu->pCurInstr->opcode == OP_INSB)
        cbSize = 1;
    else
        cbSize = pCpu->opmode == CPUMODE_32BIT ? 4 : 2;

    int rc = IOMInterpretCheckPortIOAccess(pVM, pRegFrame, uPort, cbSize);
    if (rc == VINF_SUCCESS)
    {
        /*
         * Get bytes/words/dwords count to transfer.
         */
        RTGCUINTREG cTransfers = 1;
        if (pCpu->prefix & PREFIX_REP)
        {
            cTransfers = pRegFrame->ecx;
            if (!cTransfers)
                return VINF_SUCCESS;
        }

        /* Convert destination address es:edi. */
        RTGCPTR GCPtrDst;
        rc = SELMToFlatEx(pVM, pRegFrame->eflags, pRegFrame->es, (RTGCPTR)pRegFrame->edi, &pRegFrame->esHid, 
                          SELMTOFLAT_FLAGS_HYPER | SELMTOFLAT_FLAGS_NO_PL,
                          &GCPtrDst, NULL);
        if (VBOX_FAILURE(rc))
        {
            Log(("INS destination address conversion failed -> fallback, rc=%d\n", rc));
            return VINF_IOM_HC_IOPORT_READ;
        }

        /* Access verification first; we can't recover from traps inside this instruction, as the port read cannot be repeated. */
        uint32_t cpl = CPUMGetGuestCPL(pVM, pRegFrame);

        rc = PGMVerifyAccess(pVM, (RTGCUINTPTR)GCPtrDst, cTransfers * cbSize,
                             X86_PTE_RW | ((cpl == 3) ? X86_PTE_US : 0));
        if (rc != VINF_SUCCESS)
        {
            Log(("INS will generate a trap -> fallback, rc=%d\n", rc));
            return VINF_IOM_HC_IOPORT_READ;
        }

        Log(("IOMGC: rep ins%d port %#x count %d\n", cbSize * 8, uPort, cTransfers));
        MMGCRamRegisterTrapHandler(pVM);

        /* If the device supports string transfers, ask it to do as
         * much as it wants. The rest is done with single-word transfers. */
        const RTGCUINTREG cTransfersOrg = cTransfers;
        rc = IOMIOPortReadString(pVM, uPort, &GCPtrDst, &cTransfers, cbSize);
        AssertRC(rc); Assert(cTransfers <= cTransfersOrg);
        pRegFrame->edi += (cTransfersOrg - cTransfers) * cbSize;

        while (cTransfers && rc == VINF_SUCCESS)
        {
            uint32_t u32Value;
            rc = IOMIOPortRead(pVM, uPort, &u32Value, cbSize);
            if (rc == VINF_IOM_HC_IOPORT_READ || VBOX_FAILURE(rc))
                break;
            int rc2 = MMGCRamWriteNoTrapHandler(GCPtrDst, &u32Value, cbSize);
            Assert(rc2 == VINF_SUCCESS); NOREF(rc2);
            GCPtrDst = (RTGCPTR)((RTGCUINTPTR)GCPtrDst + cbSize);
            pRegFrame->edi += cbSize;
            cTransfers--;
        }
        MMGCRamDeregisterTrapHandler(pVM);

        /* Update ecx on exit. */
        if (pCpu->prefix & PREFIX_REP)
            pRegFrame->ecx = cTransfers;
    }
    return rc;
}


/**
 * [REP*] OUTSB/OUTSW/OUTSD
 * DS:ESI,DX[,ECX]
 *
 * @returns VBox status code.
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Pointer to CPUMCTXCORE guest registers structure.
 * @param   pCpu        Disassembler CPU state.
 */
IOMDECL(int) IOMInterpretOUTS(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu)
{
#ifdef VBOX_WITH_STATISTICS
    STAM_COUNTER_INC(&pVM->iom.s.StatGCInstOuts);
#endif

    /*
     * We do not support segment prefixes, REPNE, 16-bit addressing or
     * decrementing source pointer.
     */
    if (   pCpu->prefix & (PREFIX_SEG | PREFIX_REPNE)
        || (pCpu->addrmode != CPUMODE_32BIT)
        || pRegFrame->eflags.Bits.u1DF)
        return VINF_IOM_HC_IOPORT_WRITE;

    /*
     * Get port number from the first parameter.
     * And get the I/O register size from the opcode / prefix.
     */
    uint32_t    uPort = 0;
    unsigned    cbSize = 0;
    bool fRc = iomGCGetRegImmData(pCpu, &pCpu->param1, pRegFrame, &uPort, &cbSize);
    AssertMsg(fRc, ("Failed to get reg/imm port number!\n")); NOREF(fRc);
    if (pCpu->pCurInstr->opcode == OP_OUTSB)
        cbSize = 1;
    else
        cbSize = pCpu->opmode == CPUMODE_32BIT ? 4 : 2;

    int rc = IOMInterpretCheckPortIOAccess(pVM, pRegFrame, uPort, cbSize);
    if (rc == VINF_SUCCESS)
    {
        /*
         * Get bytes/words/dwords count to transfer.
         */
        RTGCUINTREG cTransfers = 1;
        if (pCpu->prefix & PREFIX_REP)
        {
            cTransfers = pRegFrame->ecx;
            if (!cTransfers)
                return VINF_SUCCESS;
        }

        /* Convert source address ds:esi. */
        RTGCPTR GCPtrSrc;
        rc = SELMToFlatEx(pVM, pRegFrame->eflags, pRegFrame->ds, (RTGCPTR)pRegFrame->esi, &pRegFrame->dsHid,
                          SELMTOFLAT_FLAGS_HYPER | SELMTOFLAT_FLAGS_NO_PL,
                          &GCPtrSrc, NULL);
        if (VBOX_FAILURE(rc))
        {
            Log(("OUTS source address conversion failed -> fallback, rc=%d\n", rc));
            return VINF_IOM_HC_IOPORT_WRITE;
        }

        /* Access verification first; we currently can't recover properly from traps inside this instruction */
        uint32_t cpl = CPUMGetGuestCPL(pVM, pRegFrame);
        rc = PGMVerifyAccess(pVM, (RTGCUINTPTR)GCPtrSrc, cTransfers * cbSize,
                             (cpl == 3) ? X86_PTE_US : 0);
        if (rc != VINF_SUCCESS)
        {
            Log(("OUTS will generate a trap -> fallback, rc=%d\n", rc));
            return VINF_IOM_HC_IOPORT_WRITE;
        }

        Log(("IOMGC: rep outs%d port %#x count %d\n", cbSize * 8, uPort, cTransfers));
        MMGCRamRegisterTrapHandler(pVM);

        /*
         * If the device supports string transfers, ask it to do as
         * much as it wants. The rest is done with single-word transfers.
         */
        const RTGCUINTREG cTransfersOrg = cTransfers;
        rc = IOMIOPortWriteString(pVM, uPort, &GCPtrSrc, &cTransfers, cbSize);
        AssertRC(rc); Assert(cTransfers <= cTransfersOrg);
        pRegFrame->esi += (cTransfersOrg - cTransfers) * cbSize;

        while (cTransfers && rc == VINF_SUCCESS)
        {
            uint32_t u32Value;
            rc = MMGCRamReadNoTrapHandler(&u32Value, GCPtrSrc, cbSize);
            if (rc != VINF_SUCCESS)
                break;
            rc = IOMIOPortWrite(pVM, uPort, u32Value, cbSize);
            if (rc == VINF_IOM_HC_IOPORT_WRITE)
                break;
            GCPtrSrc = (RTGCPTR)((RTUINTPTR)GCPtrSrc + cbSize);
            pRegFrame->esi += cbSize;
            cTransfers--;
        }

        MMGCRamDeregisterTrapHandler(pVM);

        /* Update ecx on exit. */
        if (pCpu->prefix & PREFIX_REP)
            pRegFrame->ecx = cTransfers;
    }
    return rc;
}


/**
 * Attempts to service an IN/OUT instruction.
 *
 * The \#GP trap handler in GC will call this function if the opcode causing the
 * trap is a in or out type instruction.
 *
 * @returns VBox status code.
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Pointer to CPUMCTXCORE guest registers structure.
 * @param   pCpu        Disassembler CPU state.
 */
IOMGCDECL(int) IOMGCIOPortHandler(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu)
{
    switch (pCpu->pCurInstr->opcode)
    {
        case OP_IN:
            return IOMInterpretIN(pVM, pRegFrame, pCpu);

        case OP_OUT:
            return IOMInterpretOUT(pVM, pRegFrame, pCpu);

        case OP_INSB:
        case OP_INSWD:
            return IOMInterpretINS(pVM, pRegFrame, pCpu);

        case OP_OUTSB:
        case OP_OUTSWD:
            return IOMInterpretOUTS(pVM, pRegFrame, pCpu);

        /*
         * The opcode wasn't know to us, freak out.
         */
        default:
            AssertMsgFailed(("Unknown I/O port access opcode %d.\n", pCpu->pCurInstr->opcode));
            return VERR_INTERNAL_ERROR;
    }
}

