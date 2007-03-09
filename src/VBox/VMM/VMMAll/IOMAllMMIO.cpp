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

#ifndef IN_RING3

#ifndef IN_RING0
/** @def IOMGC_MOVS_SUPPORT
 * Define IOMGC_MOVS_SUPPORT for movsb/w/d support in GC.
 */
#define IOMGC_MOVS_SUPPORT
#endif

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
#if 0
static bool iomGCCalcParamEA(PDISCPUSTATE pCpu, POP_PARAMETER pParam, PCPUMCTXCORE pRegFrame, void **ppAddr);
static unsigned iomGCGetRegSize(PDISCPUSTATE pCpu, PCOP_PARAMETER pParam);
#endif
static bool iomGCGetRegImmData(PDISCPUSTATE pCpu, PCOP_PARAMETER pParam, PCPUMCTXCORE pRegFrame, uint32_t *pu32Data, unsigned *pcbSize);
static bool iomGCSaveDataToReg(PDISCPUSTATE pCpu, PCOP_PARAMETER pParam, PCPUMCTXCORE pRegFrame, uint32_t u32Data);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * Array for accessing 32-bit general registers in VMMREGFRAME structure
 * by register's index from disasm.
 */
static unsigned g_aReg32Index[] =
{
    RT_OFFSETOF(CPUMCTXCORE, eax),        /* USE_REG_EAX */
    RT_OFFSETOF(CPUMCTXCORE, ecx),        /* USE_REG_ECX */
    RT_OFFSETOF(CPUMCTXCORE, edx),        /* USE_REG_EDX */
    RT_OFFSETOF(CPUMCTXCORE, ebx),        /* USE_REG_EBX */
    RT_OFFSETOF(CPUMCTXCORE, esp),        /* USE_REG_ESP */
    RT_OFFSETOF(CPUMCTXCORE, ebp),        /* USE_REG_EBP */
    RT_OFFSETOF(CPUMCTXCORE, esi),        /* USE_REG_ESI */
    RT_OFFSETOF(CPUMCTXCORE, edi)         /* USE_REG_EDI */
};

/**
 * Macro for accessing 32-bit general purpose registers in CPUMCTXCORE structure.
 */
#define ACCESS_REG32(p, idx) (*((uint32_t *)((char *)(p) + g_aReg32Index[idx])))

/**
 * Array for accessing 16-bit general registers in CPUMCTXCORE structure
 * by register's index from disasm.
 */
static unsigned g_aReg16Index[] =
{
    RT_OFFSETOF(CPUMCTXCORE, eax),        /* USE_REG_AX */
    RT_OFFSETOF(CPUMCTXCORE, ecx),        /* USE_REG_CX */
    RT_OFFSETOF(CPUMCTXCORE, edx),        /* USE_REG_DX */
    RT_OFFSETOF(CPUMCTXCORE, ebx),        /* USE_REG_BX */
    RT_OFFSETOF(CPUMCTXCORE, esp),        /* USE_REG_SP */
    RT_OFFSETOF(CPUMCTXCORE, ebp),        /* USE_REG_BP */
    RT_OFFSETOF(CPUMCTXCORE, esi),        /* USE_REG_SI */
    RT_OFFSETOF(CPUMCTXCORE, edi)         /* USE_REG_DI */
};

/**
 * Macro for accessing 16-bit general purpose registers in CPUMCTXCORE structure.
 */
#define ACCESS_REG16(p, idx) (*((uint16_t *)((char *)(p) + g_aReg16Index[idx])))

/**
 * Array for accessing 8-bit general registers in CPUMCTXCORE structure
 * by register's index from disasm.
 */
static unsigned g_aReg8Index[] =
{
    RT_OFFSETOF(CPUMCTXCORE, eax),        /* USE_REG_AL */
    RT_OFFSETOF(CPUMCTXCORE, ecx),        /* USE_REG_CL */
    RT_OFFSETOF(CPUMCTXCORE, edx),        /* USE_REG_DL */
    RT_OFFSETOF(CPUMCTXCORE, ebx),        /* USE_REG_BL */
    RT_OFFSETOF(CPUMCTXCORE, eax) + 1,    /* USE_REG_AH */
    RT_OFFSETOF(CPUMCTXCORE, ecx) + 1,    /* USE_REG_CH */
    RT_OFFSETOF(CPUMCTXCORE, edx) + 1,    /* USE_REG_DH */
    RT_OFFSETOF(CPUMCTXCORE, ebx) + 1     /* USE_REG_BH */
};

/**
 * Macro for accessing 8-bit general purpose registers in CPUMCTXCORE structure.
 */
#define ACCESS_REG8(p, idx) (*((uint8_t *)((char *)(p) + g_aReg8Index[idx])))

/**
 * Array for accessing segment registers in CPUMCTXCORE structure
 * by register's index from disasm.
 */
static unsigned g_aRegSegIndex[] =
{
    RT_OFFSETOF(CPUMCTXCORE, es),         /* USE_REG_ES */
    RT_OFFSETOF(CPUMCTXCORE, cs),         /* USE_REG_CS */
    RT_OFFSETOF(CPUMCTXCORE, ss),         /* USE_REG_SS */
    RT_OFFSETOF(CPUMCTXCORE, ds),         /* USE_REG_DS */
    RT_OFFSETOF(CPUMCTXCORE, fs),         /* USE_REG_FS */
    RT_OFFSETOF(CPUMCTXCORE, gs)          /* USE_REG_GS */
};

/**
 * Macro for accessing segment registers in CPUMCTXCORE structure.
 */
#define ACCESS_REGSEG(p, idx) (*((uint16_t *)((char *)(p) + g_aRegSegIndex[idx])))

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


/**
 * Wrapper which does the write and updates range statistics when such are enabled.
 * @warning VBOX_SUCCESS(rc=VINF_IOM_HC_MMIO_WRITE) is TRUE!
 */
inline int iomGCMMIODoWrite(PVM pVM, CTXALLSUFF(PIOMMMIORANGE) pRange, RTGCPHYS GCPhysFault, const void *pvData, unsigned cbSize)
{
#ifdef VBOX_WITH_STATISTICS
    if (pRange->cbSize <= PAGE_SIZE)
    {
        PIOMMMIOSTATS pStats = iomMMIOGetStats(&pVM->iom.s, GCPhysFault);
        if (!pStats)
            return VINF_IOM_HC_MMIO_WRITE;

        int rc = pRange->pfnWriteCallback(pRange->pDevIns, pRange->pvUser, GCPhysFault, (void *)pvData, cbSize); /* @todo fix const!! */
        if (rc != VINF_IOM_HC_MMIO_WRITE)
            STAM_COUNTER_INC(&pStats->WriteGC);
        return rc;
    }
#endif
    return pRange->pfnWriteCallback(pRange->pDevIns, pRange->pvUser, GCPhysFault, (void *)pvData, cbSize);
}

/**
 * Wrapper which does the read and updates range statistics when such are enabled.
 */
inline int iomGCMMIODoRead(PVM pVM, CTXALLSUFF(PIOMMMIORANGE) pRange, RTGCPHYS GCPhysFault, void *pvData, unsigned cbSize)
{
#ifdef VBOX_WITH_STATISTICS
    if (pRange->cbSize <= PAGE_SIZE)
    {
        PIOMMMIOSTATS pStats = iomMMIOGetStats(&pVM->iom.s, GCPhysFault);
        if (!pStats)
            return VINF_IOM_HC_MMIO_READ;

        int rc = pRange->pfnReadCallback(pRange->pDevIns, pRange->pvUser, GCPhysFault, pvData, cbSize);
        if (rc != VINF_IOM_HC_MMIO_READ)
            STAM_COUNTER_INC(&pStats->ReadGC);
        return rc;
    }
#endif
    return pRange->pfnReadCallback(pRange->pDevIns, pRange->pvUser, GCPhysFault, pvData, cbSize);
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
        *pu32Data = ACCESS_REG32(pRegFrame, pParam->base.reg_gen32);
        return true;
    }

    if (pParam->flags & USE_REG_GEN16)
    {
        *pcbSize  = 2;
        *pu32Data = ACCESS_REG16(pRegFrame, pParam->base.reg_gen16);
        return true;
    }

    if (pParam->flags & USE_REG_GEN8)
    {
        *pcbSize  = 1;
        *pu32Data = ACCESS_REG8(pRegFrame, pParam->base.reg_gen8);
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
        *pu32Data = ACCESS_REGSEG(pRegFrame, pParam->base.reg_seg);
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
        ACCESS_REG32(pRegFrame, pParam->base.reg_gen32) = u32Data;
        return true;
    }

    if (pParam->flags & USE_REG_GEN16)
    {
        ACCESS_REG16(pRegFrame, pParam->base.reg_gen16) = (uint16_t)u32Data;
        return true;
    }

    if (pParam->flags & USE_REG_GEN8)
    {
        ACCESS_REG8(pRegFrame, pParam->base.reg_gen8) = (uint8_t)u32Data;
        return true;
    }

    if (pParam->flags & USE_REG_SEG)
    {
        ACCESS_REGSEG(pRegFrame, pParam->base.reg_seg) = (uint16_t)u32Data;
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
 * MOV      reg, mem         (read)
 * MOVZX    reg, mem         (read)
 * MOVSX    reg, mem         (read)
 *
 * @returns VBox status code.
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Pointer to CPUMCTXCORE guest registers structure.
 * @param   pCpu        Disassembler CPU state.
 * @param   pRange      Pointer MMIO range.
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 */
static int iomGCInterpretMOVxXRead(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu, CTXALLSUFF(PIOMMMIORANGE) pRange, RTGCPHYS GCPhysFault)
{
    /*
     * If no read handler then go to ring-3 and handle it there.
     */
    if (!pRange->pfnReadCallback)
        return VINF_IOM_HC_MMIO_READ;

    /*
     * Get the data size from parameter 2,
     * and call the handler function to get the data.
     */
    unsigned cbSize = DISGetParamSize(pCpu, &pCpu->param2);
    AssertMsg(cbSize > 0 && cbSize <= sizeof(uint32_t), ("cbSize=%d\n", cbSize));

    uint32_t u32Data = 0;
    int rc = iomGCMMIODoRead(pVM, pRange, GCPhysFault, &u32Data, cbSize);
    if (rc == VINF_SUCCESS)
    {
        /*
         * Do sign extension for MOVSX.
         */
        /** @todo checkup MOVSX implementation! */
        if (pCpu->pCurInstr->opcode == OP_MOVSX)
        {
            if (cbSize == 1)
            {
                /* DWORD <- BYTE */
                int32_t iData = (int8_t)u32Data;
                u32Data = (uint32_t)iData;
            }
            else
            {
                /* DWORD <- WORD */
                int32_t iData = (int16_t)u32Data;
                u32Data = (uint32_t)iData;
            }
        }

        /*
         * Store the result to register (parameter 1).
         */
        bool fRc = iomGCSaveDataToReg(pCpu, &pCpu->param1, pRegFrame, u32Data);
        AssertMsg(fRc, ("Failed to store register value!\n")); NOREF(fRc);
    }

    if (rc == VINF_SUCCESS)
        iomGCMMIOStatLength(pVM, cbSize);
    return rc;
}


/**
 * MOV      mem, reg|imm     (write)
 *
 * @returns VBox status code.
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Pointer to CPUMCTXCORE guest registers structure.
 * @param   pCpu        Disassembler CPU state.
 * @param   pRange      Pointer MMIO range.
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 */
static int iomGCInterpretMOVxXWrite(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu, CTXALLSUFF(PIOMMMIORANGE) pRange, RTGCPHYS GCPhysFault)
{
    /*
     * If no write handler then go to ring-3 and handle it there.
     */
    if (!pRange->pfnWriteCallback)
        return VINF_IOM_HC_MMIO_WRITE;

    /*
     * Get data to write from second parameter,
     * and call the callback to write it.
     */
    unsigned cbSize = 0;
    uint32_t u32Data  = 0;
    bool fRc = iomGCGetRegImmData(pCpu, &pCpu->param2, pRegFrame, &u32Data, &cbSize);
    AssertMsg(fRc, ("Failed to get reg/imm port number!\n")); NOREF(fRc);

    int rc = iomGCMMIODoWrite(pVM, pRange, GCPhysFault, &u32Data, cbSize);
    if (rc == VINF_SUCCESS)
        iomGCMMIOStatLength(pVM, cbSize);
    return rc;
}


/** @todo All the string MMIO stuff can do terrible things since physical contiguous mappings are
 * assumed all over the place! This must be addressed in a general way, like for example let EM do
 * all the interpretation and checking of selectors and addresses.
 */


#ifdef IOMGC_MOVS_SUPPORT
/**
 * [REP] MOVSB
 * [REP] MOVSW
 * [REP] MOVSD
 *
 * Restricted implementation.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   uErrorCode  CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 * @param   pCpu        Disassembler CPU state.
 * @param   pRange      Pointer MMIO range.
 */
static int iomGCInterpretMOVS(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPHYS GCPhysFault, PDISCPUSTATE pCpu, CTXALLSUFF(PIOMMMIORANGE) pRange)
{
    STAM_PROFILE_START(&pVM->iom.s.StatGCInstMovs, a);

    /*
     * We do not support segment prefixes, REPNE or 16-bit addressing.
     */
    if (    pCpu->prefix & (PREFIX_SEG | PREFIX_REPNE)
        || (pCpu->addrmode != CPUMODE_32BIT))
        return VINF_IOM_HC_MMIO_READ_WRITE;


    /*
     * Get bytes/words/dwords count to copy.
     */
    uint32_t cTransfers = 1;
    if (pCpu->prefix & PREFIX_REP)
    {
        cTransfers = pRegFrame->ecx;
        if (!cTransfers)
            return VINF_SUCCESS;
    }

    uint32_t cpl;
    if (pRegFrame->eflags.Bits.u1VM)
        cpl = 3;
    else
        cpl = (pRegFrame->ss & X86_SEL_RPL);

    /*
     * Get data size.
     */
    unsigned cbSize = DISGetParamSize(pCpu, &pCpu->param1);
    Assert(cbSize);
    int      offIncrement = pRegFrame->eflags.Bits.u1DF ? -(signed)cbSize : (signed)cbSize;

#ifdef VBOX_WITH_STATISTICS
    if (pVM->iom.s.cMovsMaxBytes < (cTransfers << SIZE2SHIFT(cbSize)))
        pVM->iom.s.cMovsMaxBytes = cTransfers << SIZE2SHIFT(cbSize);
#endif

    RTGCPHYS Phys = GCPhysFault;
    int rc;
    if (uErrorCode & X86_TRAP_PF_RW)
    {
        /*
         * Write operation: [Mem] -> [MMIO]
         * ds:esi (Virt Src) -> es:edi (Phys Dst)
         */
        STAM_PROFILE_START(&pVM->iom.s.StatGCInstMovsToMMIO, a2);

        /* Check callback. */
        if (!pRange->pfnWriteCallback)
        {
            STAM_PROFILE_STOP(&pVM->iom.s.StatGCInstMovsToMMIO, a2);
            return VINF_IOM_HC_MMIO_WRITE;
        }

        /* Convert source address ds:esi. */
        uint8_t *pu8Virt;
        rc = SELMToFlatEx(pVM, pRegFrame->eflags, pRegFrame->ds, (RTGCPTR)pRegFrame->esi,
                                SELMTOFLAT_FLAGS_HYPER | SELMTOFLAT_FLAGS_NO_PL,
                                (PRTGCPTR)&pu8Virt, NULL);
        if (VBOX_SUCCESS(rc))
        {

            /* Access verification first; we currently can't recover properly from traps inside this instruction */
            rc = PGMVerifyAccess(pVM, (RTGCUINTPTR)pu8Virt, cTransfers * cbSize, (cpl == 3) ? X86_PTE_US : 0);
            if (rc != VINF_SUCCESS)
            {
                Log(("MOVS will generate a trap -> recompiler, rc=%d\n", rc));
                STAM_PROFILE_STOP(&pVM->iom.s.StatGCInstMovsToMMIO, a2);
                return VINF_EM_RAW_EMULATE_INSTR;
            }

            MMGCRamRegisterTrapHandler(pVM);

            /* copy loop. */
            while (cTransfers)
            {
                uint32_t u32Data = 0;
                rc = MMGCRamReadNoTrapHandler(&u32Data, pu8Virt, cbSize);
                if (rc != VINF_SUCCESS)
                    break;
                rc = iomGCMMIODoWrite(pVM, pRange, Phys, &u32Data, cbSize);
                if (rc != VINF_SUCCESS)
                    break;

                pu8Virt        += offIncrement;
                Phys           += offIncrement;
                pRegFrame->esi += offIncrement;
                pRegFrame->edi += offIncrement;
                cTransfers--;
            }
            MMGCRamDeregisterTrapHandler(pVM);

            /* Update ecx. */
            if (pCpu->prefix & PREFIX_REP)
                pRegFrame->ecx = cTransfers;
            STAM_PROFILE_STOP(&pVM->iom.s.StatGCInstMovsToMMIO, a2);
        }
        else
            rc = VINF_IOM_HC_MMIO_READ_WRITE;
    }
    else
    {
        /*
         * Read operation: [MMIO] -> [mem] or [MMIO] -> [MMIO]
         * ds:[eSI] (Phys Src) -> es:[eDI] (Virt Dst)
         */
        /* Check callback. */
        if (!pRange->pfnReadCallback)
            return VINF_IOM_HC_MMIO_READ;

        /* Convert destination address. */
        uint8_t *pu8Virt;
        rc = SELMToFlatEx(pVM, pRegFrame->eflags, pRegFrame->es, (RTGCPTR)pRegFrame->edi,
                                SELMTOFLAT_FLAGS_HYPER | SELMTOFLAT_FLAGS_NO_PL,
                                (PRTGCPTR)&pu8Virt, NULL);
        if (VBOX_FAILURE(rc))
            return VINF_EM_RAW_GUEST_TRAP;

        /* Check if destination address is MMIO. */
        RTGCPHYS PhysDst;
        rc = PGMGstGetPage(pVM, pu8Virt, NULL, &PhysDst);
        if (    VBOX_SUCCESS(rc)
            &&  iomMMIOGetRangeHC(&pVM->iom.s, PhysDst))
        {
            /*
             * Extra: [MMIO] -> [MMIO]
             */
            STAM_PROFILE_START(&pVM->iom.s.StatGCInstMovsMMIO, d);
            STAM_PROFILE_START(&pVM->iom.s.StatGCInstMovsFromMMIO, c);

            PhysDst |= (RTGCUINTPTR)pu8Virt & PAGE_OFFSET_MASK;
            PIOMMMIORANGEGC pMMIODst = iomMMIOGetRange(&pVM->iom.s, PhysDst);
            if (    !pMMIODst
                ||  !pMMIODst->pfnWriteCallback)
            {
                STAM_PROFILE_STOP(&pVM->iom.s.StatGCInstMovsMMIO, d);
                STAM_PROFILE_STOP(&pVM->iom.s.StatGCInstMovsFromMMIO, c);
                return VINF_IOM_HC_MMIO_READ_WRITE;
            }

            /* copy loop. */
            while (cTransfers)
            {
                uint32_t u32Data;
                rc = iomGCMMIODoRead(pVM, pRange, Phys, &u32Data, cbSize);
                if (rc != VINF_SUCCESS)
                    break;
                rc = iomGCMMIODoWrite(pVM, pMMIODst, PhysDst, &u32Data, cbSize);
                if (rc != VINF_SUCCESS)
                    break;

                Phys           += offIncrement;
                PhysDst        += offIncrement;
                pRegFrame->esi += offIncrement;
                pRegFrame->edi += offIncrement;
                cTransfers--;
            }
            STAM_PROFILE_STOP(&pVM->iom.s.StatGCInstMovsMMIO, d);
            STAM_PROFILE_STOP(&pVM->iom.s.StatGCInstMovsFromMMIO, c);
        }
        else
        {
            /*
             * Normal: [MMIO] -> [Mem]
             */
            STAM_PROFILE_START(&pVM->iom.s.StatGCInstMovsFromMMIO, c);

            /* Access verification first; we currently can't recover properly from traps inside this instruction */
            rc = PGMVerifyAccess(pVM, (RTGCUINTPTR)pu8Virt, cTransfers * cbSize, X86_PTE_RW | ((cpl == 3) ? X86_PTE_US : 0));
            if (rc != VINF_SUCCESS)
            {
                Log(("MOVS will generate a trap -> recompiler, rc=%d\n", rc));
                STAM_PROFILE_STOP(&pVM->iom.s.StatGCInstMovsFromMMIO, c);
                return VINF_EM_RAW_EMULATE_INSTR;
            }

            /* copy loop. */
            MMGCRamRegisterTrapHandler(pVM);
            while (cTransfers)
            {
                uint32_t u32Data;
                rc = iomGCMMIODoRead(pVM, pRange, Phys, &u32Data, cbSize);
                if (rc != VINF_SUCCESS)
                    break;
                rc = MMGCRamWriteNoTrapHandler(pu8Virt, &u32Data, cbSize);
                if (rc != VINF_SUCCESS)
                {
                    Log(("MMGCRamWriteNoTrapHandler %08X size=%d failed with %d\n", pu8Virt, cbSize, rc));
                    break;
                }

                pu8Virt        += offIncrement;
                Phys           += offIncrement;
                pRegFrame->esi += offIncrement;
                pRegFrame->edi += offIncrement;
                cTransfers--;
            }
            MMGCRamDeregisterTrapHandler(pVM);
            STAM_PROFILE_STOP(&pVM->iom.s.StatGCInstMovsFromMMIO, c);
        }

        /* Update ecx on exit. */
        if (pCpu->prefix & PREFIX_REP)
            pRegFrame->ecx = cTransfers;
    }

    /* work statistics. */
    if (rc == VINF_SUCCESS)
    {
        STAM_PROFILE_STOP(&pVM->iom.s.StatGCInstMovs, a);
        iomGCMMIOStatLength(pVM, cbSize);
    }
    return rc;
}
#endif /* IOMGC_MOVS_SUPPORT */



/**
 * [REP] STOSB
 * [REP] STOSW
 * [REP] STOSD
 *
 * Restricted implementation.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Trap register frame.
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 * @param   pCpu        Disassembler CPU state.
 * @param   pRange      Pointer MMIO range.
 */
static int iomGCInterpretSTOS(PVM pVM, PCPUMCTXCORE pRegFrame, RTGCPHYS GCPhysFault, PDISCPUSTATE pCpu, CTXALLSUFF(PIOMMMIORANGE) pRange)
{
    STAM_PROFILE_START(&pVM->iom.s.StatGCInstStos, a);

    /*
     * We do not support segment prefixes, REPNE or 16-bit addressing.
     */
    if (    pCpu->prefix & (PREFIX_SEG | PREFIX_REPNE)
        || (pCpu->addrmode != CPUMODE_32BIT))
        return VINF_IOM_HC_MMIO_READ_WRITE;

    /*
     * Get bytes/words/dwords count to copy.
     */
    uint32_t cTransfers = 1;
    if (pCpu->prefix & PREFIX_REP)
    {
        cTransfers = pRegFrame->ecx;
        if (!cTransfers)
            return VINF_SUCCESS;
    }

    /*
     * Get data size.
     */
    unsigned cbSize = DISGetParamSize(pCpu, &pCpu->param1);
    Assert(cbSize);
    int      offIncrement = pRegFrame->eflags.Bits.u1DF ? -(signed)cbSize : (signed)cbSize;

#ifdef VBOX_WITH_STATISTICS
    if (pVM->iom.s.cStosMaxBytes < (cTransfers << SIZE2SHIFT(cbSize)))
        pVM->iom.s.cStosMaxBytes = cTransfers << SIZE2SHIFT(cbSize);
#endif


    RTGCPHYS    Phys    = GCPhysFault;
    uint32_t    u32Data = pRegFrame->eax;
    int rc;
    if (pRange->pfnFillCallback)
    {
        /*
         * Use the fill callback.
         */
        /** @todo pfnFillCallback must return number of bytes successfully written!!! */
        if (offIncrement > 0)
        {
            /* addr++ variant. */
            rc = pRange->pfnFillCallback(pRange->pDevIns, pRange->pvUser, Phys, u32Data, cbSize, cTransfers);
            if (rc == VINF_SUCCESS)
            {
                /* Update registers. */
                pRegFrame->edi += cTransfers << SIZE2SHIFT(cbSize);
                if (pCpu->prefix & PREFIX_REP)
                    pRegFrame->ecx = 0;
            }
        }
        else
        {
            /* addr-- variant. */
            rc = pRange->pfnFillCallback(pRange->pDevIns, pRange->pvUser, (Phys - (cTransfers - 1)) << SIZE2SHIFT(cbSize), u32Data, cbSize, cTransfers);
            if (rc == VINF_SUCCESS)
            {
                /* Update registers. */
                pRegFrame->edi -= cTransfers << SIZE2SHIFT(cbSize);
                if (pCpu->prefix & PREFIX_REP)
                    pRegFrame->ecx = 0;
            }
        }
    }
    else
    {
        /*
         * Use the write callback.
         */
        /* Check write callback. */
        if (!pRange->pfnWriteCallback)
            return VINF_IOM_HC_MMIO_WRITE;

        /* fill loop. */
        do
        {
            rc = iomGCMMIODoWrite(pVM, pRange, Phys, &u32Data, cbSize);
            if (rc != VINF_SUCCESS)
                break;

            Phys           += offIncrement;
            pRegFrame->edi += offIncrement;
            cTransfers--;
        } while (cTransfers);

        /* Update ecx on exit. */
        if (pCpu->prefix & PREFIX_REP)
            pRegFrame->ecx = cTransfers;
    }

    /*
     * Work statistics and return.
     */
    if (rc == VINF_SUCCESS)
    {
        STAM_PROFILE_STOP(&pVM->iom.s.StatGCInstStos, a);
        iomGCMMIOStatLength(pVM, cbSize);
    }
    return rc;
}


/**
 * [REP] LODSB
 * [REP] LODSW
 * [REP] LODSD
 *
 * Restricted implementation.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Trap register frame.
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 * @param   pCpu        Disassembler CPU state.
 * @param   pRange      Pointer MMIO range.
 */
static int iomGCInterpretLODS(PVM pVM, PCPUMCTXCORE pRegFrame, RTGCPHYS GCPhysFault, PDISCPUSTATE pCpu, CTXALLSUFF(PIOMMMIORANGE) pRange)
{
    STAM_PROFILE_START(&pVM->iom.s.StatGCInstLods, a1);

    /*
     * We do not support segment prefixes, REP* or 16-bit addressing.
     */
    if (    (pCpu->prefix & (PREFIX_SEG | PREFIX_REP | PREFIX_REPNE))
        ||  (pCpu->addrmode != CPUMODE_32BIT))
        return VINF_IOM_HC_MMIO_READ_WRITE;

    /* Check that we can handle it. */
    if (!pRange->pfnReadCallback)
        return VINF_IOM_HC_MMIO_READ;

    /*
     * Get data size.
     */
    unsigned cbSize = DISGetParamSize(pCpu, &pCpu->param2);
    Assert(cbSize);
    int     offIncrement = pRegFrame->eflags.Bits.u1DF ? -(signed)cbSize : (signed)cbSize;

    /*
     * Perform read.
     */
    int rc = iomGCMMIODoRead(pVM, pRange, GCPhysFault, &pRegFrame->eax, cbSize);
    if (rc == VINF_SUCCESS)
        pRegFrame->esi += offIncrement;

    /*
     * Work statistics and return.
     */
    if (rc == VINF_SUCCESS)
    {
        STAM_PROFILE_STOP(&pVM->iom.s.StatGCInstLods, a1);
        iomGCMMIOStatLength(pVM, cbSize);
    }
    return rc;
}


/**
 * CMP [MMIO], reg|imm
 * CMP reg|imm, [MMIO]
 *
 * Restricted implementation.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Trap register frame.
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 * @param   pCpu        Disassembler CPU state.
 * @param   pRange      Pointer MMIO range.
 */
static int iomGCInterpretCMP(PVM pVM, PCPUMCTXCORE pRegFrame, RTGCPHYS GCPhysFault, PDISCPUSTATE pCpu, CTXALLSUFF(PIOMMMIORANGE) pRange)
{
    STAM_PROFILE_START(&pVM->iom.s.StatGCInstCmp, a1);

    /* Check read callback. */
    if (!pRange->pfnReadCallback)
        return VINF_EM_RAW_GUEST_TRAP;

    /*
     * Get the operands.
     */
    unsigned cbSize = 0;
    uint32_t uData1;
    uint32_t uData2;
    int rc;
    if (iomGCGetRegImmData(pCpu, &pCpu->param1, pRegFrame, &uData1, &cbSize))
        /* cmp reg, [MMIO]. */
        rc = iomGCMMIODoRead(pVM, pRange, GCPhysFault, &uData2, cbSize);
    else if (iomGCGetRegImmData(pCpu, &pCpu->param2, pRegFrame, &uData2, &cbSize))
        /* cmp [MMIO], reg|imm. */
        rc = iomGCMMIODoRead(pVM, pRange, GCPhysFault, &uData1, cbSize);
    else
    {
        AssertMsgFailed(("Disassember CMP problem..\n"));
        rc = VERR_IOM_MMIO_HANDLER_DISASM_ERROR;
    }

    if (rc == VINF_SUCCESS)
    {
        /* Emulate CMP and update guest flags. */
        uint32_t eflags = EMEmulateCmp(uData1, uData2, cbSize);
        pRegFrame->eflags.u32 = (pRegFrame->eflags.u32 & ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF))
                              | (eflags                &  (X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF));

        STAM_PROFILE_STOP(&pVM->iom.s.StatGCInstCmp, a1);
        iomGCMMIOStatLength(pVM, cbSize);
    }

    return rc;
}


/**
 * AND [MMIO], reg|imm
 * AND reg, [MMIO]
 *
 * Restricted implementation.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Trap register frame.
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 * @param   pCpu        Disassembler CPU state.
 * @param   pRange      Pointer MMIO range.
 */
static int iomGCInterpretAND(PVM pVM, PCPUMCTXCORE pRegFrame, RTGCPHYS GCPhysFault, PDISCPUSTATE pCpu, CTXALLSUFF(PIOMMMIORANGE) pRange)
{
    STAM_PROFILE_START(&pVM->iom.s.StatGCInstAnd, a1);

    /* Check read callback. */

    unsigned    cbSize = 0;
    uint32_t    uData1;
    uint32_t    uData2;
    bool        fAndWrite;
    int         rc;
    if (iomGCGetRegImmData(pCpu, &pCpu->param1, pRegFrame, &uData1, &cbSize))
    {
        /* and reg, [MMIO]. */
        fAndWrite = false;
        if (pRange->pfnReadCallback)
            rc = iomGCMMIODoRead(pVM, pRange, GCPhysFault, &uData2, cbSize);
        else
            rc = VINF_IOM_HC_MMIO_READ;
    }
    else if (iomGCGetRegImmData(pCpu, &pCpu->param2, pRegFrame, &uData2, &cbSize))
    {
        /* and [MMIO], reg|imm. */
        fAndWrite = true;
        if (pRange->pfnReadCallback && pRange->pfnWriteCallback)
            rc = iomGCMMIODoRead(pVM, pRange, GCPhysFault, &uData1, cbSize);
        else
            rc = VINF_IOM_HC_MMIO_READ_WRITE;
    }
    else
    {
        AssertMsgFailed(("Disassember AND problem..\n"));
        return VERR_IOM_MMIO_HANDLER_DISASM_ERROR;
    }

    if (rc == VINF_SUCCESS)
    {
        /* Emulate AND and update guest flags. */
        uint32_t eflags = EMEmulateAnd(&uData1, uData2, cbSize);
        if (fAndWrite)
            /* Store result to MMIO. */
            rc = iomGCMMIODoWrite(pVM, pRange, GCPhysFault, &uData1, cbSize);
        else
        {
            /* Store result to register. */
            bool fRc = iomGCSaveDataToReg(pCpu, &pCpu->param1, pRegFrame, uData1);
            AssertMsg(fRc, ("Failed to store register value!\n")); NOREF(fRc);
        }
        if (rc == VINF_SUCCESS)
        {
            /* Update guest's eflags and finish. */
            pRegFrame->eflags.u32 = (pRegFrame->eflags.u32 & ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF))
                                  | (eflags                &  (X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF));
            STAM_PROFILE_STOP(&pVM->iom.s.StatGCInstAnd, a1);
            iomGCMMIOStatLength(pVM, cbSize);
        }
    }

    return rc;
}



/**
 * TEST [MMIO], reg|imm
 * TEST reg, [MMIO]
 *
 * Restricted implementation.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Trap register frame.
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 * @param   pCpu        Disassembler CPU state.
 * @param   pRange      Pointer MMIO range.
 */
static int iomGCInterpretTEST(PVM pVM, PCPUMCTXCORE pRegFrame, RTGCPHYS GCPhysFault, PDISCPUSTATE pCpu, CTXALLSUFF(PIOMMMIORANGE) pRange)
{
    STAM_PROFILE_START(&pVM->iom.s.StatGCInstTest, a1);

    /* Check read callback. */

    unsigned    cbSize = 0;
    uint32_t    uData1;
    uint32_t    uData2;
    int         rc;

    if (iomGCGetRegImmData(pCpu, &pCpu->param1, pRegFrame, &uData1, &cbSize))
    {
        /* and test, [MMIO]. */
        if (pRange->pfnReadCallback)
            rc = iomGCMMIODoRead(pVM, pRange, GCPhysFault, &uData2, cbSize);
        else
            rc = VINF_IOM_HC_MMIO_READ;
    }
    else if (iomGCGetRegImmData(pCpu, &pCpu->param2, pRegFrame, &uData2, &cbSize))
    {
        /* test [MMIO], reg|imm. */
        if (pRange->pfnReadCallback)
            rc = iomGCMMIODoRead(pVM, pRange, GCPhysFault, &uData1, cbSize);
        else
            rc = VINF_IOM_HC_MMIO_READ;
    }
    else
    {
        AssertMsgFailed(("Disassember TEST problem..\n"));
        return VERR_IOM_MMIO_HANDLER_DISASM_ERROR;
    }

    if (rc == VINF_SUCCESS)
    {
        /* Emulate TEST (=AND without write back) and update guest EFLAGS. */
        uint32_t eflags = EMEmulateAnd(&uData1, uData2, cbSize);
        pRegFrame->eflags.u32 = (pRegFrame->eflags.u32 & ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF))
                              | (eflags                &  (X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF));
        STAM_PROFILE_STOP(&pVM->iom.s.StatGCInstTest, a1);
        iomGCMMIOStatLength(pVM, cbSize);
    }

    return rc;
}

/**
 * XCHG [MMIO], reg
 * XCHG reg, [MMIO]
 *
 * Restricted implementation.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Trap register frame.
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 * @param   pCpu        Disassembler CPU state.
 * @param   pRange      Pointer MMIO range.
 */
static int iomGCInterpretXCHG(PVM pVM, PCPUMCTXCORE pRegFrame, RTGCPHYS GCPhysFault, PDISCPUSTATE pCpu, CTXALLSUFF(PIOMMMIORANGE) pRange)
{
    STAM_PROFILE_START(&pVM->iom.s.StatGCInstTest, a1);

    /* Check read callback. */
    unsigned    cbSize = 0;
    uint32_t    uData1;
    uint32_t    uData2;
    int         rc;

    if (!pRange->pfnReadCallback || !pRange->pfnWriteCallback)
    {
        rc = VINF_IOM_HC_MMIO_READ;
        goto end;
    }

    if (iomGCGetRegImmData(pCpu, &pCpu->param1, pRegFrame, &uData1, &cbSize))
    {
        /* xchg reg, [MMIO]. */
        rc = iomGCMMIODoRead(pVM, pRange, GCPhysFault, &uData2, cbSize);
        if (rc == VINF_SUCCESS)
        {
            /* Store result to MMIO. */
            rc = iomGCMMIODoWrite(pVM, pRange, GCPhysFault, &uData1, cbSize);

            if (rc == VINF_SUCCESS)
            {
                /* Store result to register. */
                bool fRc = iomGCSaveDataToReg(pCpu, &pCpu->param1, pRegFrame, uData2);
                AssertMsg(fRc, ("Failed to store register value!\n")); NOREF(fRc);
            }
            else
                Assert(rc == VINF_IOM_HC_MMIO_WRITE || rc == VINF_PATM_HC_MMIO_PATCH_WRITE);
        }
        else
            Assert(rc == VINF_IOM_HC_MMIO_READ || rc == VINF_PATM_HC_MMIO_PATCH_READ);
    }
    else
    if (iomGCGetRegImmData(pCpu, &pCpu->param2, pRegFrame, &uData2, &cbSize))
    {
        /* xchg [MMIO], reg. */
        rc = iomGCMMIODoRead(pVM, pRange, GCPhysFault, &uData1, cbSize);
        if (rc == VINF_SUCCESS)
        {
            /* Store result to MMIO. */
            rc = iomGCMMIODoWrite(pVM, pRange, GCPhysFault, &uData2, cbSize);

            if (rc == VINF_SUCCESS)
            {
                /* Store result to register. */
                bool fRc = iomGCSaveDataToReg(pCpu, &pCpu->param2, pRegFrame, uData1);
                AssertMsg(fRc, ("Failed to store register value!\n")); NOREF(fRc);
            }
            else
                Assert(rc == VINF_IOM_HC_MMIO_WRITE || rc == VINF_PATM_HC_MMIO_PATCH_WRITE);
        }
        else
            Assert(rc == VINF_IOM_HC_MMIO_READ || rc == VINF_PATM_HC_MMIO_PATCH_READ);
    }
    else
    {
        AssertMsgFailed(("Disassember XCHG problem..\n"));
        rc = VERR_IOM_MMIO_HANDLER_DISASM_ERROR;
        goto end;
    }

end:
    STAM_PROFILE_STOP(&pVM->iom.s.StatGCInstTest, a1);
    return rc;
}


#ifdef IN_RING0

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
DECLCALLBACK(int32_t) iomReadBytes(RTHCUINTPTR pSrc, uint8_t *pDest, uint32_t size, RTHCUINTPTR dwUserdata)
{
    DISCPUSTATE  *pCpu     = (DISCPUSTATE *)dwUserdata;
    PVM           pVM      = (PVM)pCpu->dwUserData[0];

    int rc = PGMPhysReadGCPtr(pVM, pDest, pSrc, size);
    AssertRC(rc);
    return rc;
}

inline bool iomDisCoreOne(PVM pVM, DISCPUSTATE *pCpu, RTGCUINTPTR InstrGC, uint32_t *pOpsize)
{
    return VBOX_SUCCESS(DISCoreOneEx(InstrGC, pCpu->mode, iomReadBytes, pVM, pCpu, pOpsize));
}
#else
inline bool iomDisCoreOne(PVM pVM, DISCPUSTATE *pCpu, RTGCUINTPTR InstrGC, uint32_t *pOpsize)
{
    return DISCoreOne(pCpu, InstrGC, pOpsize);
}

#endif

/**
 * \#PF Handler callback for MMIO ranges.
 * Note: we are on ring0 in Hypervisor and interrupts are disabled.
 *
 * @returns VBox status code (appropriate for GC return).
 * @param   pVM         VM Handle.
 * @param   uErrorCode  CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address (cr2).
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 * @param   pvUser      Pointer to the MMIO ring-3 range entry.
 */
IOMDECL(int) IOMMMIOHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, RTGCPHYS GCPhysFault, void *pvUser)
{
    STAM_PROFILE_START(&pVM->iom.s.StatGCMMIOHandler, a);
    NOREF(pvUser); /** @todo implement pvUser usage! */
    Log3(("IOMMMIOHandler: GCPhys=%RGp uErr=%#x pvFault=%p eip=%RGv\n",
          GCPhysFault, uErrorCode, pvFault, pRegFrame->eip));

    /*
     * Find the corresponding MMIO range.
     */
    CTXALLSUFF(PIOMMMIORANGE) pRange = iomMMIOGetRange(&pVM->iom.s, GCPhysFault);
    if (!pRange)
    {
#ifdef VBOX_WITH_STATISTICS
        PIOMMMIOSTATS pStats = iomMMIOGetStats(&pVM->iom.s, GCPhysFault);
        if (pStats)
        {
            if (uErrorCode & X86_TRAP_PF_RW)
                STAM_COUNTER_INC(&pStats->WriteGCToR3);
            else
                STAM_COUNTER_INC(&pStats->ReadGCToR3);
        }
#endif
        PIOMMMIORANGER3 pRangeR3 = iomMMIOGetRangeHC(&pVM->iom.s, GCPhysFault);
        if (pRangeR3)
        {
            STAM_PROFILE_START(&pVM->iom.s.StatGCMMIOHandler, a);
            STAM_COUNTER_INC(&pVM->iom.s.StatGCMMIOFailures);
            return (uErrorCode & X86_TRAP_PF_RW) ? VINF_IOM_HC_MMIO_WRITE : VINF_IOM_HC_MMIO_READ;
        }

        /*
         * Now, why are we here...
         */
        AssertMsgFailed(("Internal error! GCPhysFault=%x\n", GCPhysFault));
        return VERR_IOM_MMIO_HANDLER_BOGUS_CALL;
    }

    /*
     * Convert CS:EIP to linear address and initialize the disassembler.
     */
    DISCPUSTATE cpu;
    cpu.mode = SELMIsSelector32Bit(pVM, pRegFrame->eflags, pRegFrame->cs, &pRegFrame->csHid) ? CPUMODE_32BIT : CPUMODE_16BIT;

    RTGCPTR pvCode;
    int rc = SELMValidateAndConvertCSAddr(pVM, pRegFrame->eflags, pRegFrame->ss, pRegFrame->cs, &pRegFrame->csHid, (RTGCPTR)(cpu.mode == CPUMODE_32BIT ? pRegFrame->eip : pRegFrame->eip & 0xffff), &pvCode);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Internal error! cs:eip=%04x:%08x\n", pRegFrame->cs, pRegFrame->eip));
        return VERR_IOM_MMIO_HANDLER_BOGUS_CALL;
    }

    /*
     * Disassemble the instruction and interprete it.
     */
    unsigned cbOp;
    if (iomDisCoreOne(pVM, &cpu, (RTGCUINTPTR)pvCode, &cbOp))
    {
        switch (cpu.pCurInstr->opcode)
        {
            case OP_MOV:
            case OP_MOVZX:
            case OP_MOVSX:
            {
                STAM_PROFILE_START(&pVM->iom.s.StatGCInstMov, b);
                if (uErrorCode & X86_TRAP_PF_RW)
                    rc = iomGCInterpretMOVxXWrite(pVM, pRegFrame, &cpu, pRange, GCPhysFault);
                else
                    rc = iomGCInterpretMOVxXRead(pVM, pRegFrame, &cpu, pRange, GCPhysFault);
                if (rc == VINF_SUCCESS)
                    STAM_PROFILE_STOP(&pVM->iom.s.StatGCInstMov, b);
                break;
            }


#ifdef IOMGC_MOVS_SUPPORT
            case OP_MOVSB:
            case OP_MOVSWD:
                rc = iomGCInterpretMOVS(pVM, uErrorCode, pRegFrame, GCPhysFault, &cpu, pRange);
                break;
#endif

            case OP_STOSB:
            case OP_STOSWD:
                Assert(uErrorCode & X86_TRAP_PF_RW);
                rc = iomGCInterpretSTOS(pVM, pRegFrame, GCPhysFault, &cpu, pRange);
                break;

            case OP_LODSB:
            case OP_LODSWD:
                Assert(!(uErrorCode & X86_TRAP_PF_RW));
                rc = iomGCInterpretLODS(pVM, pRegFrame, GCPhysFault, &cpu, pRange);
                break;


            case OP_CMP:
                Assert(!(uErrorCode & X86_TRAP_PF_RW));
                rc = iomGCInterpretCMP(pVM, pRegFrame, GCPhysFault, &cpu, pRange);
                break;

            case OP_AND:
                rc = iomGCInterpretAND(pVM, pRegFrame, GCPhysFault, &cpu, pRange);
                break;

            case OP_TEST:
                Assert(!(uErrorCode & X86_TRAP_PF_RW));
                rc = iomGCInterpretTEST(pVM, pRegFrame, GCPhysFault, &cpu, pRange);
                break;

            case OP_XCHG:
                rc = iomGCInterpretXCHG(pVM, pRegFrame, GCPhysFault, &cpu, pRange);
                break;


            /*
             * The instruction isn't supported. Hand it on to ring-3.
             */
            default:
                STAM_COUNTER_INC(&pVM->iom.s.StatGCInstOther);
                rc = (uErrorCode & X86_TRAP_PF_RW) ? VINF_IOM_HC_MMIO_WRITE : VINF_IOM_HC_MMIO_READ;
                break;
        }
    }
    else
    {
        AssertMsgFailed(("Disassembler freaked out!\n"));
        rc = VERR_IOM_MMIO_HANDLER_DISASM_ERROR;
    }

    /*
     * On success advance EIP.
     */
    if (rc == VINF_SUCCESS)
        pRegFrame->eip += cbOp;
    else
    {
        STAM_COUNTER_INC(&pVM->iom.s.StatGCMMIOFailures);
#ifdef VBOX_WITH_STATISTICS
        switch (rc)
        {
            case VINF_IOM_HC_MMIO_READ:
            case VINF_IOM_HC_MMIO_WRITE:
            case VINF_IOM_HC_MMIO_READ_WRITE:
            {
                PIOMMMIOSTATS pStats = iomMMIOGetStats(&pVM->iom.s, GCPhysFault);
                if (pStats)
                {
                    if (uErrorCode & X86_TRAP_PF_RW)
                        STAM_COUNTER_INC(&pStats->WriteGCToR3);
                    else
                        STAM_COUNTER_INC(&pStats->ReadGCToR3);
                }
            }
            break;
        }
#endif
    }
    STAM_PROFILE_STOP(&pVM->iom.s.StatGCMMIOHandler, a);
    return rc;
}


#endif /* !IN_RING3 */
