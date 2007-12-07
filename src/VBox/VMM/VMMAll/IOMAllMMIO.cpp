/* $Id$ */
/** @file
 * IOM - Input / Output Monitor - Guest Context.
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

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
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
 *
 * -> I don't see the problem here. MMIO ranges are by definition linear ranges. The virtual source or destination is read/written properly.
 */


DECLINLINE(int) iomRamRead(PVM pVM, void *pDest, RTGCPTR GCSrc, uint32_t cb)
{
#ifdef IN_GC
    return MMGCRamReadNoTrapHandler(pDest, GCSrc, cb);
#else
    return PGMPhysReadGCPtrSafe(pVM, pDest, GCSrc, cb);
#endif
}

DECLINLINE(int) iomRamWrite(PVM pVM, RTGCPTR GCDest, void *pSrc, uint32_t cb)
{
#ifdef IN_GC
    return MMGCRamWriteNoTrapHandler(GCDest, pSrc, cb);
#else
    return PGMPhysWriteGCPtrSafe(pVM, GCDest, pSrc, cb);
#endif
}

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
#ifdef IOMGC_MOVS_SUPPORT
static int iomGCInterpretMOVS(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPHYS GCPhysFault, PDISCPUSTATE pCpu, CTXALLSUFF(PIOMMMIORANGE) pRange)
{
    STAM_PROFILE_START(&pVM->iom.s.StatGCInstMovs, a);

    /*
     * We do not support segment prefixes or REPNE.
     */
    if (pCpu->prefix & (PREFIX_SEG | PREFIX_REPNE))
        return VINF_IOM_HC_MMIO_READ_WRITE;


    /*
     * Get bytes/words/dwords count to copy.
     */
    uint32_t cTransfers = 1;
    if (pCpu->prefix & PREFIX_REP)
    {
        cTransfers = pRegFrame->ecx;
        if (!SELMIsSelector32Bit(pVM, pRegFrame->eflags, pRegFrame->cs, &pRegFrame->csHid))
            cTransfers &= 0xffff;

        if (!cTransfers)
            return VINF_SUCCESS;
    }

    /* Get the current privilege level. */
    uint32_t cpl = CPUMGetGuestCPL(pVM, pRegFrame);

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
        RTGCUINTPTR pu8Virt;
        rc = SELMToFlatEx(pVM, pRegFrame->eflags, pRegFrame->ds, (RTGCPTR)pRegFrame->esi, &pRegFrame->dsHid,
                                SELMTOFLAT_FLAGS_HYPER | SELMTOFLAT_FLAGS_NO_PL,
                                (PRTGCPTR)&pu8Virt, NULL);
        if (VBOX_SUCCESS(rc))
        {

            /* Access verification first; we currently can't recover properly from traps inside this instruction */
            rc = PGMVerifyAccess(pVM, pu8Virt, cTransfers * cbSize, (cpl == 3) ? X86_PTE_US : 0);
            if (rc != VINF_SUCCESS)
            {
                Log(("MOVS will generate a trap -> recompiler, rc=%d\n", rc));
                STAM_PROFILE_STOP(&pVM->iom.s.StatGCInstMovsToMMIO, a2);
                return VINF_EM_RAW_EMULATE_INSTR;
            }

#ifdef IN_GC
            MMGCRamRegisterTrapHandler(pVM);
#endif

            /* copy loop. */
            while (cTransfers)
            {
                uint32_t u32Data = 0;
                rc = iomRamRead(pVM, &u32Data, (RTGCPTR)pu8Virt, cbSize);
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
#ifdef IN_GC
            MMGCRamDeregisterTrapHandler(pVM);
#endif
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
        RTGCUINTPTR pu8Virt;
        rc = SELMToFlatEx(pVM, pRegFrame->eflags, pRegFrame->es, (RTGCPTR)pRegFrame->edi, &pRegFrame->esHid,
                                SELMTOFLAT_FLAGS_HYPER | SELMTOFLAT_FLAGS_NO_PL,
                                (RTGCPTR *)&pu8Virt, NULL);
        if (VBOX_FAILURE(rc))
            return VINF_EM_RAW_GUEST_TRAP;

        /* Check if destination address is MMIO. */
        RTGCPHYS PhysDst;
        rc = PGMGstGetPage(pVM, (RTGCPTR)pu8Virt, NULL, &PhysDst);
        if (    VBOX_SUCCESS(rc)
            &&  iomMMIOGetRangeHC(&pVM->iom.s, PhysDst))
        {
            /*
             * Extra: [MMIO] -> [MMIO]
             */
            STAM_PROFILE_START(&pVM->iom.s.StatGCInstMovsMMIO, d);
            STAM_PROFILE_START(&pVM->iom.s.StatGCInstMovsFromMMIO, c);

            PhysDst |= (RTGCUINTPTR)pu8Virt & PAGE_OFFSET_MASK;
            CTXALLSUFF(PIOMMMIORANGE) pMMIODst = iomMMIOGetRange(&pVM->iom.s, PhysDst);
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
            rc = PGMVerifyAccess(pVM, pu8Virt, cTransfers * cbSize, X86_PTE_RW | ((cpl == 3) ? X86_PTE_US : 0));
            if (rc != VINF_SUCCESS)
            {
                Log(("MOVS will generate a trap -> recompiler, rc=%d\n", rc));
                STAM_PROFILE_STOP(&pVM->iom.s.StatGCInstMovsFromMMIO, c);
                return VINF_EM_RAW_EMULATE_INSTR;
            }

            /* copy loop. */
#ifdef IN_GC
            MMGCRamRegisterTrapHandler(pVM);
#endif
            while (cTransfers)
            {
                uint32_t u32Data;
                rc = iomGCMMIODoRead(pVM, pRange, Phys, &u32Data, cbSize);
                if (rc != VINF_SUCCESS)
                    break;
                rc = iomRamWrite(pVM, (RTGCPTR)pu8Virt, &u32Data, cbSize);
                if (rc != VINF_SUCCESS)
                {
                    Log(("iomRamWrite %08X size=%d failed with %d\n", pu8Virt, cbSize, rc));
                    break;
                }

                pu8Virt        += offIncrement;
                Phys           += offIncrement;
                pRegFrame->esi += offIncrement;
                pRegFrame->edi += offIncrement;
                cTransfers--;
            }
#ifdef IN_GC
            MMGCRamDeregisterTrapHandler(pVM);
#endif
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
#endif



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
     * We do not support segment prefixes or REPNE..
     */
    if (pCpu->prefix & (PREFIX_SEG | PREFIX_REPNE))
        return VINF_IOM_HC_MMIO_READ_WRITE;

    /*
     * Get bytes/words/dwords count to copy.
     */
    uint32_t cTransfers = 1;
    if (pCpu->prefix & PREFIX_REP)
    {
        cTransfers = pRegFrame->ecx;
        if (!SELMIsSelector32Bit(pVM, pRegFrame->eflags, pRegFrame->cs, &pRegFrame->csHid))
            cTransfers &= 0xffff;

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
     * We do not support segment prefixes or REP*.
     */
    if (pCpu->prefix & (PREFIX_SEG | PREFIX_REP | PREFIX_REPNE))
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
 * @param   pvUserdata  Callback specific user data (pCpu)
 *
 */
DECLCALLBACK(int) iomReadBytes(RTHCUINTPTR pSrc, uint8_t *pDest, unsigned size, void *pvUserdata)
{
    DISCPUSTATE  *pCpu     = (DISCPUSTATE *)pvUserdata;
    PVM           pVM      = (PVM)pCpu->apvUserData[0];

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
    return VBOX_SUCCESS(DISCoreOne(pCpu, InstrGC, pOpsize));
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
        AssertMsgFailed(("Internal error! cs:eip=%04x:%08x rc=%Vrc\n", pRegFrame->cs, pRegFrame->eip, rc));
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


/**
 * Reads a MMIO register.
 *
 * @returns VBox status code.
 *
 * @param   pVM         VM handle.
 * @param   GCPhys      The physical address to read.
 * @param   pu32Value   Where to store the value read.
 * @param   cbValue     The size of the register to read in bytes. 1, 2 or 4 bytes.
 */
IOMDECL(int) IOMMMIORead(PVM pVM, RTGCPHYS GCPhys, uint32_t *pu32Value, size_t cbValue)
{
/** @todo add return to ring-3 statistics when this function is used in GC! */

    /*
     * Lookup the current context range node and statistics.
     */
    CTXALLSUFF(PIOMMMIORANGE) pRange = iomMMIOGetRange(&pVM->iom.s, GCPhys);
#ifdef VBOX_WITH_STATISTICS
    PIOMMMIOSTATS pStats = iomMMIOGetStats(&pVM->iom.s, GCPhys);
    if (!pStats && (!pRange || pRange->cbSize <= PAGE_SIZE))
# ifdef IN_RING3
        pStats = iomR3MMIOStatsCreate(pVM, GCPhys, pRange ? pRange->pszDesc : NULL);
# else
        return VINF_IOM_HC_MMIO_READ;
# endif
#endif /* VBOX_WITH_STATISTICS */
#ifdef IN_RING3
    if (pRange)
#else /* !IN_RING3 */
    if (pRange && pRange->pfnReadCallback)
#endif /* !IN_RING3 */
    {
        /*
         * Perform the read and deal with the result.
         */
#ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_PROFILE_ADV_START(&pStats->CTXALLSUFF(ProfRead), a);
#endif
        int rc = pRange->pfnReadCallback(pRange->pDevIns, pRange->pvUser, GCPhys, pu32Value, cbValue);
#ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_PROFILE_ADV_STOP(&pStats->CTXALLSUFF(ProfRead), a);
        if (pStats && rc != VINF_IOM_HC_MMIO_READ)
            STAM_COUNTER_INC(&pStats->CTXALLSUFF(Read));
#endif
        switch (rc)
        {
            case VINF_SUCCESS:
            default:
                Log4(("IOMMMIORead: GCPhys=%RGp *pu32=%08RX32 cb=%d rc=%Vrc\n", GCPhys, *pu32Value, cbValue, rc));
                return rc;

            case VINF_IOM_MMIO_UNUSED_00:
                switch (cbValue)
                {
                    case 1: *(uint8_t *)pu32Value  = 0x00; break;
                    case 2: *(uint16_t *)pu32Value = 0x0000; break;
                    case 4: *(uint32_t *)pu32Value = 0x00000000; break;
                    default: AssertReleaseMsgFailed(("cbValue=%d GCPhys=%VGp\n", cbValue, GCPhys)); break;
                }
                Log4(("IOMMMIORead: GCPhys=%RGp *pu32=%08RX32 cb=%d rc=%Vrc\n", GCPhys, *pu32Value, cbValue, rc));
                return VINF_SUCCESS;

            case VINF_IOM_MMIO_UNUSED_FF:
                switch (cbValue)
                {
                    case 1: *(uint8_t *)pu32Value  = 0xff; break;
                    case 2: *(uint16_t *)pu32Value = 0xffff; break;
                    case 4: *(uint32_t *)pu32Value = 0xffffffff; break;
                    default: AssertReleaseMsgFailed(("cbValue=%d GCPhys=%VGp\n", cbValue, GCPhys)); break;
                }
                Log4(("IOMMMIORead: GCPhys=%RGp *pu32=%08RX32 cb=%d rc=%Vrc\n", GCPhys, *pu32Value, cbValue, rc));
                return VINF_SUCCESS;
        }
    }

#ifndef IN_RING3
    /*
     * Lookup the ring-3 range.
     */
    PIOMMMIORANGER3 pRangeR3 = iomMMIOGetRangeHC(&pVM->iom.s, GCPhys);
    if (pRangeR3)
    {
        if (pRangeR3->pfnReadCallback)
            return VINF_IOM_HC_MMIO_READ;
# ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_COUNTER_INC(&pStats->CTXALLSUFF(Read));
# endif
        *pu32Value = 0;
        Log4(("IOMMMIORead: GCPhys=%RGp *pu32=%08RX32 cb=%d rc=VINF_SUCCESS\n", GCPhys, *pu32Value, cbValue));
        return VINF_SUCCESS;
    }
#endif

    AssertMsgFailed(("Handlers and page tables are out of sync or something! GCPhys=%VGp cbValue=%d\n", GCPhys, cbValue));
    return VERR_INTERNAL_ERROR;
}


/**
 * Writes to a MMIO register.
 *
 * @returns VBox status code.
 *
 * @param   pVM         VM handle.
 * @param   GCPhys      The physical address to write to.
 * @param   u32Value    The value to write.
 * @param   cbValue     The size of the register to read in bytes. 1, 2 or 4 bytes.
 */
IOMDECL(int) IOMMMIOWrite(PVM pVM, RTGCPHYS GCPhys, uint32_t u32Value, size_t cbValue)
{
/** @todo add return to ring-3 statistics when this function is used in GC! */
    /*
     * Lookup the current context range node.
     */
    CTXALLSUFF(PIOMMMIORANGE) pRange = iomMMIOGetRange(&pVM->iom.s, GCPhys);
#ifdef VBOX_WITH_STATISTICS
    PIOMMMIOSTATS pStats = iomMMIOGetStats(&pVM->iom.s, GCPhys);
    if (!pStats && (!pRange || pRange->cbSize <= PAGE_SIZE))
# ifdef IN_RING3
        pStats = iomR3MMIOStatsCreate(pVM, GCPhys, pRange ? pRange->pszDesc : NULL);
# else
        return VINF_IOM_HC_MMIO_WRITE;
# endif
#endif /* VBOX_WITH_STATISTICS */

    /*
     * Perform the write if we found a range.
     */
#ifdef IN_RING3
    if (pRange)
#else /* !IN_RING3 */
    if (pRange && pRange->pfnWriteCallback)
#endif /* !IN_RING3 */
    {
#ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_PROFILE_ADV_START(&pStats->CTXALLSUFF(ProfWrite), a);
#endif
        int rc = pRange->pfnWriteCallback(pRange->pDevIns, pRange->pvUser, GCPhys, &u32Value, cbValue);
#ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_PROFILE_ADV_STOP(&pStats->CTXALLSUFF(ProfWrite), a);
        if (pStats && rc != VINF_IOM_HC_MMIO_WRITE)
            STAM_COUNTER_INC(&pStats->CTXALLSUFF(Write));
#endif
        Log4(("IOMMMIOWrite: GCPhys=%RGp u32=%08RX32 cb=%d rc=%Vrc\n", GCPhys, u32Value, cbValue, rc));
        return rc;
    }

#ifndef IN_RING3
    /*
     * Lookup the ring-3 range.
     */
    PIOMMMIORANGER3 pRangeR3 = iomMMIOGetRangeHC(&pVM->iom.s, GCPhys);
    if (pRangeR3)
    {
        if (pRangeR3->pfnWriteCallback)
            return VINF_IOM_HC_MMIO_WRITE;
# ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_COUNTER_INC(&pStats->CTXALLSUFF(Write));
# endif
        Log4(("IOMMMIOWrite: GCPhys=%RGp u32=%08RX32 cb=%d rc=%Vrc\n", GCPhys, u32Value, cbValue));
        return VINF_SUCCESS;
    }
#endif

    AssertMsgFailed(("Handlers and page tables are out of sync or something! GCPhys=%VGp cbValue=%d\n", GCPhys, cbValue));
    return VERR_INTERNAL_ERROR;
}


/**
 * [REP*] INSB/INSW/INSD
 * ES:EDI,DX[,ECX]
 *
 * @remark Assumes caller checked the access privileges (IOMInterpretCheckPortIOAccess)
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the
 *                                      status code must be passed on to EM.
 * @retval  VINF_IOM_HC_IOPORT_READ     Defer the read to ring-3. (R0/GC only)
 * @retval  VINF_EM_RAW_EMULATE_INSTR   Defer the read to the REM.
 * @retval  VINF_EM_RAW_GUEST_TRAP      The exception was left pending. (TRPMRaiseXcptErr)
 * @retval  VINF_TRPM_XCPT_DISPATCHED   The exception was raised and dispatched for raw-mode execution. (TRPMRaiseXcptErr)
 * @retval  VINF_EM_RESCHEDULE_REM      The exception was dispatched and cannot be executed in raw-mode. (TRPMRaiseXcptErr)
 *
 * @param   pVM             The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame       Pointer to CPUMCTXCORE guest registers structure.
 * @param   uPort           IO Port
 * @param   uPrefix         IO instruction prefix
 * @param   cbTransfer      Size of transfer unit
 */
IOMDECL(int) IOMInterpretINSEx(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t uPort, uint32_t uPrefix, uint32_t cbTransfer)
{
#ifdef VBOX_WITH_STATISTICS
    STAM_COUNTER_INC(&pVM->iom.s.StatGCInstIns);
#endif

    /*
     * We do not support REPNE or decrementing destination
     * pointer. Segment prefixes are deliberately ignored, as per the instruction specification.
     */
    if (   (uPrefix & PREFIX_REPNE)
        || pRegFrame->eflags.Bits.u1DF)
        return VINF_EM_RAW_EMULATE_INSTR;

    /*
     * Get bytes/words/dwords count to transfer.
     */
    RTGCUINTREG cTransfers = 1;
    if (uPrefix & PREFIX_REP)
    {
        cTransfers = pRegFrame->ecx;

        if (!SELMIsSelector32Bit(pVM, pRegFrame->eflags, pRegFrame->cs, &pRegFrame->csHid))
            cTransfers &= 0xffff;

        if (!cTransfers)
            return VINF_SUCCESS;
    }

    /* Convert destination address es:edi. */
    RTGCPTR GCPtrDst;
    int rc = SELMToFlatEx(pVM, pRegFrame->eflags, pRegFrame->es, (RTGCPTR)pRegFrame->edi, &pRegFrame->esHid,
                          SELMTOFLAT_FLAGS_HYPER | SELMTOFLAT_FLAGS_NO_PL,
                          &GCPtrDst, NULL);
    if (VBOX_FAILURE(rc))
    {
        Log(("INS destination address conversion failed -> fallback, rc=%d\n", rc));
        return VINF_EM_RAW_EMULATE_INSTR;
    }

    /* Access verification first; we can't recover from traps inside this instruction, as the port read cannot be repeated. */
    uint32_t cpl = CPUMGetGuestCPL(pVM, pRegFrame);

    rc = PGMVerifyAccess(pVM, (RTGCUINTPTR)GCPtrDst, cTransfers * cbTransfer,
                         X86_PTE_RW | ((cpl == 3) ? X86_PTE_US : 0));
    if (rc != VINF_SUCCESS)
    {
        Log(("INS will generate a trap -> fallback, rc=%d\n", rc));
        return VINF_EM_RAW_EMULATE_INSTR;
    }

    Log(("IOM: rep ins%d port %#x count %d\n", cbTransfer * 8, uPort, cTransfers));
    if (cTransfers > 1)
    {
        /* If the device supports string transfers, ask it to do as
         * much as it wants. The rest is done with single-word transfers. */
        const RTGCUINTREG cTransfersOrg = cTransfers;
        rc = IOMIOPortReadString(pVM, uPort, &GCPtrDst, &cTransfers, cbTransfer);
        AssertRC(rc); Assert(cTransfers <= cTransfersOrg);
        pRegFrame->edi += (cTransfersOrg - cTransfers) * cbTransfer;
    }

#ifdef IN_GC
    MMGCRamRegisterTrapHandler(pVM);
#endif

    while (cTransfers && rc == VINF_SUCCESS)
    {
        uint32_t u32Value;
        rc = IOMIOPortRead(pVM, uPort, &u32Value, cbTransfer);
        if (!IOM_SUCCESS(rc))
            break;
        int rc2 = iomRamWrite(pVM, GCPtrDst, &u32Value, cbTransfer);
        Assert(rc2 == VINF_SUCCESS); NOREF(rc2);
        GCPtrDst = (RTGCPTR)((RTGCUINTPTR)GCPtrDst + cbTransfer);
        pRegFrame->edi += cbTransfer;
        cTransfers--;
    }
#ifdef IN_GC
    MMGCRamDeregisterTrapHandler(pVM);
#endif

    /* Update ecx on exit. */
    if (uPrefix & PREFIX_REP)
        pRegFrame->ecx = cTransfers;

    AssertMsg(rc == VINF_SUCCESS || rc == VINF_IOM_HC_IOPORT_READ || (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST) || VBOX_FAILURE(rc), ("%Vrc\n", rc));
    return rc;
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
 * @retval  VINF_IOM_HC_IOPORT_READ     Defer the read to ring-3. (R0/GC only)
 * @retval  VINF_EM_RAW_EMULATE_INSTR   Defer the read to the REM.
 * @retval  VINF_EM_RAW_GUEST_TRAP      The exception was left pending. (TRPMRaiseXcptErr)
 * @retval  VINF_TRPM_XCPT_DISPATCHED   The exception was raised and dispatched for raw-mode execution. (TRPMRaiseXcptErr)
 * @retval  VINF_EM_RESCHEDULE_REM      The exception was dispatched and cannot be executed in raw-mode. (TRPMRaiseXcptErr)
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Pointer to CPUMCTXCORE guest registers structure.
 * @param   pCpu        Disassembler CPU state.
 */
IOMDECL(int) IOMInterpretINS(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu)
{
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
    if (RT_UNLIKELY(rc != VINF_SUCCESS))
    {
        AssertMsg(rc == VINF_EM_RAW_GUEST_TRAP || rc == VINF_TRPM_XCPT_DISPATCHED || rc == VINF_TRPM_XCPT_DISPATCHED || VBOX_FAILURE(rc), ("%Vrc\n", rc));
        return rc;
    }

    return IOMInterpretINSEx(pVM, pRegFrame, uPort, pCpu->prefix, cbSize);
}


/**
 * [REP*] OUTSB/OUTSW/OUTSD
 * DS:ESI,DX[,ECX]
 *
 * @remark  Assumes caller checked the access privileges (IOMInterpretCheckPortIOAccess)
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the
 *                                      status code must be passed on to EM.
 * @retval  VINF_IOM_HC_IOPORT_WRITE    Defer the write to ring-3. (R0/GC only)
 * @retval  VINF_EM_RAW_GUEST_TRAP      The exception was left pending. (TRPMRaiseXcptErr)
 * @retval  VINF_TRPM_XCPT_DISPATCHED   The exception was raised and dispatched for raw-mode execution. (TRPMRaiseXcptErr)
 * @retval  VINF_EM_RESCHEDULE_REM      The exception was dispatched and cannot be executed in raw-mode. (TRPMRaiseXcptErr)
 *
 * @param   pVM             The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame       Pointer to CPUMCTXCORE guest registers structure.
 * @param   uPort           IO Port
 * @param   uPrefix         IO instruction prefix
 * @param   cbTransfer      Size of transfer unit
 */
IOMDECL(int) IOMInterpretOUTSEx(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t uPort, uint32_t uPrefix, uint32_t cbTransfer)
{
#ifdef VBOX_WITH_STATISTICS
    STAM_COUNTER_INC(&pVM->iom.s.StatGCInstOuts);
#endif

    /*
     * We do not support segment prefixes, REPNE or
     * decrementing source pointer.
     */
    if (   (uPrefix & (PREFIX_SEG | PREFIX_REPNE))
        || pRegFrame->eflags.Bits.u1DF)
        return VINF_EM_RAW_EMULATE_INSTR;

    /*
     * Get bytes/words/dwords count to transfer.
     */
    RTGCUINTREG cTransfers = 1;
    if (uPrefix & PREFIX_REP)
    {
        cTransfers = pRegFrame->ecx;
        if (!SELMIsSelector32Bit(pVM, pRegFrame->eflags, pRegFrame->cs, &pRegFrame->csHid))
            cTransfers &= 0xffff;

        if (!cTransfers)
            return VINF_SUCCESS;
    }

    /* Convert source address ds:esi. */
    RTGCPTR GCPtrSrc;
    int rc = SELMToFlatEx(pVM, pRegFrame->eflags, pRegFrame->ds, (RTGCPTR)pRegFrame->esi, &pRegFrame->dsHid,
                          SELMTOFLAT_FLAGS_HYPER | SELMTOFLAT_FLAGS_NO_PL,
                          &GCPtrSrc, NULL);
    if (VBOX_FAILURE(rc))
    {
        Log(("OUTS source address conversion failed -> fallback, rc=%Vrc\n", rc));
        return VINF_EM_RAW_EMULATE_INSTR;
    }

    /* Access verification first; we currently can't recover properly from traps inside this instruction */
    uint32_t cpl = CPUMGetGuestCPL(pVM, pRegFrame);
    rc = PGMVerifyAccess(pVM, (RTGCUINTPTR)GCPtrSrc, cTransfers * cbTransfer,
                         (cpl == 3) ? X86_PTE_US : 0);
    if (rc != VINF_SUCCESS)
    {
        Log(("OUTS will generate a trap -> fallback, rc=%Vrc\n", rc));
        return VINF_EM_RAW_EMULATE_INSTR;
    }

    Log(("IOM: rep outs%d port %#x count %d\n", cbTransfer * 8, uPort, cTransfers));
    if (cTransfers > 1)
    {
        /*
         * If the device supports string transfers, ask it to do as
         * much as it wants. The rest is done with single-word transfers.
         */
        const RTGCUINTREG cTransfersOrg = cTransfers;
        rc = IOMIOPortWriteString(pVM, uPort, &GCPtrSrc, &cTransfers, cbTransfer);
        AssertRC(rc); Assert(cTransfers <= cTransfersOrg);
        pRegFrame->esi += (cTransfersOrg - cTransfers) * cbTransfer;
    }

#ifdef IN_GC
    MMGCRamRegisterTrapHandler(pVM);
#endif

    while (cTransfers && rc == VINF_SUCCESS)
    {
        uint32_t u32Value;
        rc = iomRamRead(pVM, &u32Value, GCPtrSrc, cbTransfer);
        if (rc != VINF_SUCCESS)
            break;
        rc = IOMIOPortWrite(pVM, uPort, u32Value, cbTransfer);
        if (!IOM_SUCCESS(rc))
            break;
        GCPtrSrc = (RTGCPTR)((RTUINTPTR)GCPtrSrc + cbTransfer);
        pRegFrame->esi += cbTransfer;
        cTransfers--;
    }

#ifdef IN_GC
    MMGCRamDeregisterTrapHandler(pVM);
#endif

    /* Update ecx on exit. */
    if (uPrefix & PREFIX_REP)
        pRegFrame->ecx = cTransfers;

    AssertMsg(rc == VINF_SUCCESS || rc == VINF_IOM_HC_IOPORT_WRITE || (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST) || VBOX_FAILURE(rc), ("%Vrc\n", rc));
    return rc;
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
 * @retval  VINF_IOM_HC_IOPORT_WRITE    Defer the write to ring-3. (R0/GC only)
 * @retval  VINF_EM_RAW_EMULATE_INSTR   Defer the write to the REM.
 * @retval  VINF_EM_RAW_GUEST_TRAP      The exception was left pending. (TRPMRaiseXcptErr)
 * @retval  VINF_TRPM_XCPT_DISPATCHED   The exception was raised and dispatched for raw-mode execution. (TRPMRaiseXcptErr)
 * @retval  VINF_EM_RESCHEDULE_REM      The exception was dispatched and cannot be executed in raw-mode. (TRPMRaiseXcptErr)
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Pointer to CPUMCTXCORE guest registers structure.
 * @param   pCpu        Disassembler CPU state.
 */
IOMDECL(int) IOMInterpretOUTS(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu)
{
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
        cbSize = (pCpu->opmode == CPUMODE_32BIT) ? 4 : 2;

    int rc = IOMInterpretCheckPortIOAccess(pVM, pRegFrame, uPort, cbSize);
    if (RT_UNLIKELY(rc != VINF_SUCCESS))
    {
        AssertMsg(rc == VINF_EM_RAW_GUEST_TRAP || rc == VINF_TRPM_XCPT_DISPATCHED || rc == VINF_TRPM_XCPT_DISPATCHED || VBOX_FAILURE(rc), ("%Vrc\n", rc));
        return rc;
    }

    return IOMInterpretOUTSEx(pVM, pRegFrame, uPort, pCpu->prefix, cbSize);
}

