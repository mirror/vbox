/* $Id$ */
/** @file
 * IOM - Input / Output Monitor - Any Context.
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
#include <VBox/mm.h>
#include <VBox/param.h>
#include "IOMInternal.h"
#include <VBox/vm.h>
#include <VBox/selm.h>
#include <VBox/trpm.h>
#include <VBox/pgm.h>
#include <VBox/cpum.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>


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

#ifndef IN_RING0
/**
 * Registers a Port IO GC handler.
 *
 * This API is called by PDM on behalf of a device. Devices must first register ring-3 ranges
 * using IOMIOPortRegisterR3() before calling this function.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM                 VM handle.
 * @param   pDevIns             PDM device instance owning the port range.
 * @param   PortStart           First port number in the range.
 * @param   cPorts              Number of ports to register.
 * @param   pvUser              User argument for the callbacks.
 * @param   pfnOutCallback      Pointer to function which is gonna handle OUT operations in GC.
 * @param   pfnInCallback       Pointer to function which is gonna handle IN operations in GC.
 * @param   pfnOutStrCallback   Pointer to function which is gonna handle string OUT operations in GC.
 * @param   pfnInStrCallback    Pointer to function which is gonna handle string IN operations in GC.
 * @param   pszDesc             Pointer to description string. This must not be freed.
 */
IOMDECL(int)  IOMIOPortRegisterGC(PVM pVM, PPDMDEVINS pDevIns, RTIOPORT PortStart, RTUINT cPorts, RTGCPTR pvUser,
                                  GCPTRTYPE(PFNIOMIOPORTOUT) pfnOutCallback, GCPTRTYPE(PFNIOMIOPORTIN) pfnInCallback,
                                  GCPTRTYPE(PFNIOMIOPORTOUTSTRING) pfnOutStrCallback, GCPTRTYPE(PFNIOMIOPORTINSTRING) pfnInStrCallback, const char *pszDesc)
{
    LogFlow(("IOMIOPortRegisterGC: pDevIns=%p PortStart=%#x cPorts=%#x pvUser=%VGv pfnOutCallback=%VGv pfnInCallback=%VGv pfnOutStrCallback=%VGv  pfnInStrCallback=%VGv pszDesc=%s\n",
             pDevIns, PortStart, cPorts, pvUser, pfnOutCallback, pfnInCallback, pfnOutStrCallback, pfnInStrCallback, pszDesc));

    /*
     * Validate input.
     */
    if (    (RTUINT)PortStart + cPorts <= (RTUINT)PortStart
        ||  (RTUINT)PortStart + cPorts > 0x10000)
    {
        AssertMsgFailed(("Invalid port range %#x-%#x! (%s)\n", PortStart, (RTUINT)PortStart + (cPorts - 1), pszDesc));
        return VERR_IOM_INVALID_IOPORT_RANGE;
    }
    RTIOPORT PortLast = PortStart + (cPorts - 1);
    if (!pfnOutCallback && !pfnInCallback)
    {
        AssertMsgFailed(("Invalid port range %#x-%#x! No callbacks! (%s)\n", PortStart, PortLast, pszDesc));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Validate that there are ring-3 ranges for the ports.
     */
    RTIOPORT Port = PortStart;
    while (Port <= PortLast && Port >= PortStart)
    {
        PIOMIOPORTRANGER3 pRange = (PIOMIOPORTRANGER3)RTAvlroIOPortRangeGet(&pVM->iom.s.CTXSUFF(pTrees)->IOPortTreeR3, Port);
        if (!pRange)
        {
            AssertMsgFailed(("No R3! Port=#x %#x-%#x! (%s)\n", Port, PortStart, (unsigned)PortStart + cPorts - 1, pszDesc));
            return VERR_IOM_NO_HC_IOPORT_RANGE;
        }
#ifndef IOM_NO_PDMINS_CHECKS
        #ifndef IN_GC
        if (pRange->pDevIns != pDevIns)
        #else
        if (pRange->pDevIns != MMHyperGC2HC(pVM, pDevIns))
        #endif
        {
            AssertMsgFailed(("Not owner! Port=%#x %#x-%#x! (%s)\n", Port, PortStart, (unsigned)PortStart + cPorts - 1, pszDesc));
            return VERR_IOM_NOT_IOPORT_RANGE_OWNER;
        }
#endif
        Port = pRange->Core.KeyLast + 1;
    }

    /* Flush the IO port lookup cache */
    IOMFlushCache(pVM);

    /*
     * Allocate new range record and initialize it.
     */
    PIOMIOPORTRANGEGC pRange;
    int rc = MMHyperAlloc(pVM, sizeof(*pRange), 0, MM_TAG_IOM, (void **)&pRange);
    if (VBOX_SUCCESS(rc))
    {
        pRange->Core.Key        = PortStart;
        pRange->Core.KeyLast    = PortLast;
        pRange->Port            = PortStart;
        pRange->cPorts          = cPorts;
        pRange->pvUser          = pvUser;
        pRange->pfnOutCallback  = pfnOutCallback;
        pRange->pfnInCallback   = pfnInCallback;
        pRange->pfnOutStrCallback = pfnOutStrCallback;
        pRange->pfnInStrCallback = pfnInStrCallback;
        #ifdef IN_GC
        pRange->pDevIns         = pDevIns;
        pRange->pszDesc         = MMHyperGC2HC(pVM, (void *)pszDesc);
        #else
        pRange->pDevIns         = MMHyperHC2GC(pVM, pDevIns);
        pRange->pszDesc         = pszDesc;
        #endif

        /*
         * Insert it.
         */
        if (RTAvlroIOPortInsert(&pVM->iom.s.CTXSUFF(pTrees)->IOPortTreeGC, &pRange->Core))
            return VINF_SUCCESS;

        /* conflict. */
        AssertMsgFailed(("Port range %#x-%#x (%s) conflicts with existing range(s)!\n", PortStart, (unsigned)PortStart + cPorts - 1, pszDesc));
        MMHyperFree(pVM, pRange);
        rc = VERR_IOM_IOPORT_RANGE_CONFLICT;
    }

    return rc;
}


/**
 * Registers a Memory Mapped I/O GC handler range.
 *
 * This API is called by PDM on behalf of a device. Devices must first register ring-3 ranges
 * using IOMMMIORegisterR3() before calling this function.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM                 VM handle.
 * @param   pDevIns             PDM device instance owning the MMIO range.
 * @param   GCPhysStart         First physical address in the range.
 * @param   cbRange             The size of the range (in bytes).
 * @param   pvUser              User argument for the callbacks.
 * @param   pfnWriteCallback    Pointer to function which is gonna handle Write operations.
 * @param   pfnReadCallback     Pointer to function which is gonna handle Read operations.
 * @param   pfnFillCallback     Pointer to function which is gonna handle Fill/memset operations.
 * @param   pszDesc             Pointer to description string. This must not be freed.
 */
IOMDECL(int)  IOMMMIORegisterGC(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTGCPTR pvUser,
                                GCPTRTYPE(PFNIOMMMIOWRITE) pfnWriteCallback, GCPTRTYPE(PFNIOMMMIOREAD) pfnReadCallback,
                                GCPTRTYPE(PFNIOMMMIOFILL) pfnFillCallback, const char *pszDesc)
{
    LogFlow(("IOMMMIORegisterGC: pDevIns=%p GCPhysStart=%VGp cbRange=%#x pvUser=%VGv pfnWriteCallback=%#x pfnReadCallback=%#x pfnFillCallback=%#x pszDesc=%s\n",
             pDevIns, GCPhysStart, cbRange, pvUser, pfnWriteCallback, pfnReadCallback, pfnFillCallback, pszDesc));

    /*
     * Validate input.
     */
    if (!pfnWriteCallback && !pfnReadCallback)
    {
        AssertMsgFailed(("No callbacks! %VGp LB%#x %s\n", GCPhysStart, cbRange, pszDesc));
        return VERR_INVALID_PARAMETER;
    }
    RTGCPHYS GCPhysLast = GCPhysStart + (cbRange - 1);
    if (GCPhysLast < GCPhysStart)
    {
        AssertMsgFailed(("Wrapped! %VGp LB%#x %s\n", GCPhysStart, cbRange, pszDesc));
        return VERR_IOM_INVALID_MMIO_RANGE;
    }

    /*
     * Check that a ring-3 MMIO range exists.
     */
    RTGCPHYS GCPhys = GCPhysStart;
    while (GCPhys <= GCPhysLast && GCPhys >= GCPhysStart)
    {
        PIOMMMIORANGER3 pRange = (PIOMMMIORANGER3)RTAvlroGCPhysRangeGet(&pVM->iom.s.CTXSUFF(pTrees)->MMIOTreeR3, GCPhys);
        if (!pRange)
        {
            AssertMsgFailed(("No R3 range! GCPhys=%VGp %VGp LB%#x %s\n", GCPhys, GCPhysStart, cbRange, pszDesc));
            return VERR_IOM_NO_HC_MMIO_RANGE;
        }
#ifndef IOM_NO_PDMINS_CHECKS
        #ifndef IN_GC
        if (pRange->pDevIns != pDevIns)
        #else
        if (pRange->pDevIns != MMHyperGC2HC(pVM, pDevIns))
        #endif
        {
            AssertMsgFailed(("Not owner! GCPhys=%VGp %VGp LB%#x %s / %#x-%#x %s\n", GCPhys, GCPhysStart, cbRange, pszDesc,
                             pRange->Core.Key, pRange->Core.KeyLast, MMHyper2HC(pVM, (uintptr_t)pRange->pszDesc)));
            return VERR_IOM_NOT_MMIO_RANGE_OWNER;
        }
#endif /* !IOM_NO_PDMINS_CHECKS */
        /* next */
        Assert(GCPhys <= pRange->Core.KeyLast);
        GCPhys = pRange->Core.KeyLast + 1;
    }


    /*
     * Allocate new range record and initialize it.
     */
    PIOMMMIORANGEGC pRange;
    int rc = MMHyperAlloc(pVM, sizeof(*pRange), 0, MM_TAG_IOM, (void **)&pRange);
    if (VBOX_SUCCESS(rc))
    {
        pRange->Core.Key        = GCPhysStart;
        pRange->Core.KeyLast    = GCPhysStart + (cbRange - 1);
        pRange->GCPhys          = GCPhysStart;
        pRange->cbSize          = cbRange;
        pRange->pvUser          = pvUser;
        pRange->pfnReadCallback = pfnReadCallback;
        pRange->pfnWriteCallback= pfnWriteCallback;
        pRange->pfnFillCallback = pfnFillCallback;
#ifdef IN_GC
        pRange->pDevIns         = pDevIns;
        pRange->pszDesc         = MMHyperGC2HC(pVM, (void *)pszDesc);
#else
        pRange->pDevIns         = MMHyperHC2GC(pVM, pDevIns);
        pRange->pszDesc         = pszDesc;
#endif

        /*
         * Try insert it.
         */
        if (RTAvlroGCPhysInsert(&pVM->iom.s.CTXSUFF(pTrees)->MMIOTreeGC, &pRange->Core))
            return VINF_SUCCESS;

        AssertMsgFailed(("Conflict! %VGp LB%#x %s\n", GCPhysStart, cbRange, pszDesc));
        MMHyperFree(pVM, pRange);
        rc = VERR_IOM_MMIO_RANGE_CONFLICT;
    }

    return rc;
}
#endif

/**
 * Registers a Port IO R0 handler.
 *
 * This API is called by PDM on behalf of a device. Devices must first register ring-3 ranges
 * using IOMR3IOPortRegisterR3() before calling this function.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM                 VM handle.
 * @param   pDevIns             PDM device instance owning the port range.
 * @param   PortStart           First port number in the range.
 * @param   cPorts              Number of ports to register.
 * @param   pvUser              User argument for the callbacks.
 * @param   pfnOutCallback      Pointer to function which is gonna handle OUT operations in GC.
 * @param   pfnInCallback       Pointer to function which is gonna handle IN operations in GC.
 * @param   pfnOutStrCallback   Pointer to function which is gonna handle OUT operations in GC.
 * @param   pfnInStrCallback    Pointer to function which is gonna handle IN operations in GC.
 * @param   pszDesc             Pointer to description string. This must not be freed.
 */
IOMDECL(int)  IOMIOPortRegisterR0(PVM pVM, PPDMDEVINS pDevIns, RTIOPORT PortStart, RTUINT cPorts, RTR0PTR pvUser,
                                  R0PTRTYPE(PFNIOMIOPORTOUT) pfnOutCallback, R0PTRTYPE(PFNIOMIOPORTIN) pfnInCallback,
                                  R0PTRTYPE(PFNIOMIOPORTOUTSTRING) pfnOutStrCallback, R0PTRTYPE(PFNIOMIOPORTINSTRING) pfnInStrCallback,
                                  const char *pszDesc)
{
    LogFlow(("IOMIOPortRegisterR0: pDevIns=%p PortStart=%#x cPorts=%#x pvUser=%VHv pfnOutCallback=%VGv pfnInCallback=%VGv pfnOutStrCallback=%VGv  pfnInStrCallback=%VGv pszDesc=%s\n",
             pDevIns, PortStart, cPorts, pvUser, pfnOutCallback, pfnInCallback, pfnOutStrCallback, pfnInStrCallback, pszDesc));

    /*
     * Validate input.
     */
    if (    (RTUINT)PortStart + cPorts <= (RTUINT)PortStart
        ||  (RTUINT)PortStart + cPorts > 0x10000)
    {
        AssertMsgFailed(("Invalid port range %#x-%#x! (%s)\n", PortStart, (RTUINT)PortStart + (cPorts - 1), pszDesc));
        return VERR_IOM_INVALID_IOPORT_RANGE;
    }
    RTIOPORT PortLast = PortStart + (cPorts - 1);
    if (!pfnOutCallback && !pfnInCallback)
    {
        AssertMsgFailed(("Invalid port range %#x-%#x! No callbacks! (%s)\n", PortStart, PortLast, pszDesc));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Validate that there are ring-3 ranges for the ports.
     */
    RTIOPORT Port = PortStart;
    while (Port <= PortLast && Port >= PortStart)
    {
        PIOMIOPORTRANGER3 pRange = (PIOMIOPORTRANGER3)RTAvlroIOPortRangeGet(&pVM->iom.s.CTXSUFF(pTrees)->IOPortTreeR3, Port);
        if (!pRange)
        {
            AssertMsgFailed(("No R3! Port=#x %#x-%#x! (%s)\n", Port, PortStart, (unsigned)PortStart + cPorts - 1, pszDesc));
            return VERR_IOM_NO_HC_IOPORT_RANGE;
        }
#ifndef IOM_NO_PDMINS_CHECKS
# ifndef IN_GC
        if (pRange->pDevIns != pDevIns)
# else
        if (pRange->pDevIns != MMHyperGC2HC(pVM, pDevIns))
# endif
        {
            AssertMsgFailed(("Not owner! Port=%#x %#x-%#x! (%s)\n", Port, PortStart, (unsigned)PortStart + cPorts - 1, pszDesc));
            return VERR_IOM_NOT_IOPORT_RANGE_OWNER;
        }
#endif
        Port = pRange->Core.KeyLast + 1;
    }

    /* Flush the IO port lookup cache */
    IOMFlushCache(pVM);

    /*
     * Allocate new range record and initialize it.
     */
    PIOMIOPORTRANGER0 pRange;
    int rc = MMHyperAlloc(pVM, sizeof(*pRange), 0, MM_TAG_IOM, (void **)&pRange);
    if (VBOX_SUCCESS(rc))
    {
        pRange->Core.Key        = PortStart;
        pRange->Core.KeyLast    = PortLast;
        pRange->Port            = PortStart;
        pRange->cPorts          = cPorts;
        pRange->pvUser          = pvUser;
        pRange->pfnOutCallback  = pfnOutCallback;
        pRange->pfnInCallback   = pfnInCallback;
        pRange->pfnOutStrCallback = pfnOutStrCallback;
        pRange->pfnInStrCallback = pfnInStrCallback;
#ifdef IN_GC
        pRange->pDevIns         = MMHyperGCToR0(pVM, pDevIns);
        pRange->pszDesc         = MMHyperGCToR3(pVM, (void *)pszDesc);
#elif defined(IN_RING3)
        pRange->pDevIns         = MMHyperR3ToR0(pVM, pDevIns);
        pRange->pszDesc         = pszDesc;
#else
        pRange->pDevIns         = pDevIns;
        pRange->pszDesc         = MMHyperR0ToR3(pVM, (RTR0PTR)pszDesc);
#endif

        /*
         * Insert it.
         */
        if (RTAvlroIOPortInsert(&pVM->iom.s.CTXSUFF(pTrees)->IOPortTreeR0, &pRange->Core))
            return VINF_SUCCESS;

        /* conflict. */
        AssertMsgFailed(("Port range %#x-%#x (%s) conflicts with existing range(s)!\n", PortStart, (unsigned)PortStart + cPorts - 1, pszDesc));
        MMHyperFree(pVM, pRange);
        rc = VERR_IOM_IOPORT_RANGE_CONFLICT;
    }

    return rc;
}


/**
 * Registers a Memory Mapped I/O R0 handler range.
 *
 * This API is called by PDM on behalf of a device. Devices must first register ring-3 ranges
 * using IOMMR3MIORegisterHC() before calling this function.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM                 VM handle.
 * @param   pDevIns             PDM device instance owning the MMIO range.
 * @param   GCPhysStart         First physical address in the range.
 * @param   cbRange             The size of the range (in bytes).
 * @param   pvUser              User argument for the callbacks.
 * @param   pfnWriteCallback    Pointer to function which is gonna handle Write operations.
 * @param   pfnReadCallback     Pointer to function which is gonna handle Read operations.
 * @param   pfnFillCallback     Pointer to function which is gonna handle Fill/memset operations.
 * @param   pszDesc             Pointer to description string. This must not be freed.
 */
IOMDECL(int)  IOMMMIORegisterR0(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTR0PTR pvUser,
                                R0PTRTYPE(PFNIOMMMIOWRITE) pfnWriteCallback, R0PTRTYPE(PFNIOMMMIOREAD) pfnReadCallback,
                                R0PTRTYPE(PFNIOMMMIOFILL) pfnFillCallback, const char *pszDesc)
{
    LogFlow(("IOMMMIORegisterR0: pDevIns=%p GCPhysStart=%VGp cbRange=%#x pvUser=%VHv pfnWriteCallback=%#x pfnReadCallback=%#x pfnFillCallback=%#x pszDesc=%s\n",
             pDevIns, GCPhysStart, cbRange, pvUser, pfnWriteCallback, pfnReadCallback, pfnFillCallback, pszDesc));

    /*
     * Validate input.
     */
    if (!pfnWriteCallback && !pfnReadCallback)
    {
        AssertMsgFailed(("No callbacks! %VGp LB%#x %s\n", GCPhysStart, cbRange, pszDesc));
        return VERR_INVALID_PARAMETER;
    }
    RTGCPHYS GCPhysLast = GCPhysStart + (cbRange - 1);
    if (GCPhysLast < GCPhysStart)
    {
        AssertMsgFailed(("Wrapped! %VGp LB%#x %s\n", GCPhysStart, cbRange, pszDesc));
        return VERR_IOM_INVALID_MMIO_RANGE;
    }

    /*
     * Check that a ring-3 MMIO range exists.
     */
    RTGCPHYS GCPhys = GCPhysStart;
    while (GCPhys <= GCPhysLast && GCPhys >= GCPhysStart)
    {
        PIOMMMIORANGER3 pRange = (PIOMMMIORANGER3)RTAvlroGCPhysRangeGet(&pVM->iom.s.CTXSUFF(pTrees)->MMIOTreeR3, GCPhys);
        if (!pRange)
        {
            AssertMsgFailed(("No R3 range! GCPhys=%VGp %VGp LB%#x %s\n", GCPhys, GCPhysStart, cbRange, pszDesc));
            return VERR_IOM_NO_HC_MMIO_RANGE;
        }
#ifndef IOM_NO_PDMINS_CHECKS
# ifndef IN_GC
        if (pRange->pDevIns != pDevIns)
# else
        if (pRange->pDevIns != MMHyperGC2HC(pVM, pDevIns))
# endif
        {
            AssertMsgFailed(("Not owner! GCPhys=%VGp %VGp LB%#x %s / %#x-%#x %s\n", GCPhys, GCPhysStart, cbRange, pszDesc,
                             pRange->Core.Key, pRange->Core.KeyLast, MMHyper2HC(pVM, (uintptr_t)pRange->pszDesc)));
            return VERR_IOM_NOT_MMIO_RANGE_OWNER;
        }
#endif /* !IOM_NO_PDMINS_CHECKS */
        /* next */
        Assert(GCPhys <= pRange->Core.KeyLast);
        GCPhys = pRange->Core.KeyLast + 1;
    }


    /*
     * Allocate new range record and initialize it.
     */
    PIOMMMIORANGER0 pRange;
    int rc = MMHyperAlloc(pVM, sizeof(*pRange), 0, MM_TAG_IOM, (void **)&pRange);
    if (VBOX_SUCCESS(rc))
    {
        pRange->Core.Key        = GCPhysStart;
        pRange->Core.KeyLast    = GCPhysStart + (cbRange - 1);
        pRange->GCPhys          = GCPhysStart;
        pRange->cbSize          = cbRange;
        pRange->pvUser          = pvUser;
        pRange->pfnReadCallback = pfnReadCallback;
        pRange->pfnWriteCallback= pfnWriteCallback;
        pRange->pfnFillCallback = pfnFillCallback;
#ifdef IN_GC
        pRange->pDevIns         = MMHyperGCToR0(pVM, pDevIns);
        pRange->pszDesc         = MMHyperGCToR3(pVM, (void *)pszDesc);
#elif defined(IN_RING3)
        pRange->pDevIns         = MMHyperR3ToR0(pVM, pDevIns);
        pRange->pszDesc         = pszDesc;
#else
        pRange->pDevIns         = pDevIns;
        pRange->pszDesc         = MMHyperR0ToR3(pVM, (RTR0PTR)pszDesc);
#endif

        /*
         * Try insert it.
         */
        if (RTAvlroGCPhysInsert(&pVM->iom.s.CTXSUFF(pTrees)->MMIOTreeR0, &pRange->Core))
            return VINF_SUCCESS;

        AssertMsgFailed(("Conflict! %VGp LB%#x %s\n", GCPhysStart, cbRange, pszDesc));
        MMHyperFree(pVM, pRange);
        rc = VERR_IOM_MMIO_RANGE_CONFLICT;
    }

    return rc;
}


/**
 * Flushes the IOM port & statistics lookup cache
 *
 * @param   pVM     The VM.
 */
IOMDECL(void) IOMFlushCache(PVM pVM)
{
    /*
     * Caching of port and statistics (saves some time in rep outs/ins instruction emulation)
     */
    pVM->iom.s.pRangeLastReadGC  = 0;
    pVM->iom.s.pRangeLastWriteGC = 0;
    pVM->iom.s.pStatsLastReadGC  = 0;
    pVM->iom.s.pStatsLastWriteGC = 0;

    pVM->iom.s.pRangeLastReadR3  = 0;
    pVM->iom.s.pRangeLastWriteR3 = 0;
    pVM->iom.s.pStatsLastReadR3  = 0;
    pVM->iom.s.pStatsLastWriteR3 = 0;

    pVM->iom.s.pRangeLastReadR0  = 0;
    pVM->iom.s.pRangeLastWriteR0 = 0;
    pVM->iom.s.pStatsLastReadR0  = 0;
    pVM->iom.s.pStatsLastWriteR0 = 0;
}


//#undef LOG_GROUP
//#define LOG_GROUP LOG_GROUP_IOM_IOPORT

/**
 * Reads an I/O port register.
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented 
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the 
 *                                      status code must be passed on to EM.
 * @retval  VINF_IOM_HC_IOPORT_READ     Defer the read to ring-3. (R0/GC only)
 *
 * @param   pVM         VM handle.
 * @param   Port        The port to read.
 * @param   pu32Value   Where to store the value read.
 * @param   cbValue     The size of the register to read in bytes. 1, 2 or 4 bytes.
 */
IOMDECL(int) IOMIOPortRead(PVM pVM, RTIOPORT Port, uint32_t *pu32Value, size_t cbValue)
{
#ifdef VBOX_WITH_STATISTICS
    /*
     * Get the statistics record.
     */
    PIOMIOPORTSTATS  pStats = CTXALLSUFF(pVM->iom.s.pStatsLastRead);
    if (!pStats || pStats->Core.Key != Port)
    {
        pStats = (PIOMIOPORTSTATS)RTAvloIOPortGet(&pVM->iom.s.CTXSUFF(pTrees)->IOPortStatTree, Port);
        if (pStats)
            CTXALLSUFF(pVM->iom.s.pStatsLastRead) = pStats;
    }
#endif

    /*
     * Get handler for current context.
     */
    CTXALLSUFF(PIOMIOPORTRANGE) pRange = CTXALLSUFF(pVM->iom.s.pRangeLastRead);
    if (    !pRange
        ||   (unsigned)Port - (unsigned)pRange->Port >= (unsigned)pRange->cPorts)
    {
        pRange = iomIOPortGetRange(&pVM->iom.s, Port);
        if (pRange)
            CTXALLSUFF(pVM->iom.s.pRangeLastRead) = pRange;
    }
#ifdef IN_GC
    Assert(!pRange || MMHyperIsInsideArea(pVM, pRange)); /** @todo r=bird: there is a macro for this which skips the #if'ing. */
#endif

    if (pRange)
    {
        /*
         * Found a range.
         */
#ifndef IN_RING3
        if (!pRange->pfnInCallback)
        {
# ifdef VBOX_WITH_STATISTICS
            if (pStats)
                STAM_COUNTER_INC(&pStats->CTXALLMID(In,ToR3));
# endif
            return VINF_IOM_HC_IOPORT_READ;
        }
#endif
        /* call the device. */
#ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_PROFILE_ADV_START(&pStats->CTXALLSUFF(ProfIn), a);
#endif
        int rc = pRange->pfnInCallback(pRange->pDevIns, pRange->pvUser, Port, pu32Value, cbValue);
#ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_PROFILE_ADV_STOP(&pStats->CTXALLSUFF(ProfIn), a);
        if (rc == VINF_SUCCESS && pStats)
            STAM_COUNTER_INC(&pStats->CTXALLSUFF(In));
# ifndef IN_RING3
        else if (rc == VINF_IOM_HC_IOPORT_READ && pStats)
            STAM_COUNTER_INC(&pStats->CTXALLMID(In,ToR3));
# endif
#endif
        if (rc == VERR_IOM_IOPORT_UNUSED)
        {
            /* make return value */
            rc = VINF_SUCCESS;
            switch (cbValue)
            {
                case 1: *(uint8_t  *)pu32Value = 0xff; break;
                case 2: *(uint16_t *)pu32Value = 0xffff; break;
                case 4: *(uint32_t *)pu32Value = 0xffffffff; break;
                default:
                    AssertMsgFailed(("Invalid I/O port size %d. Port=%d\n", cbValue, Port));
                    return VERR_IOM_INVALID_IOPORT_SIZE;
            }
        }
        Log3(("IOMIOPortRead: Port=%RTiop *pu32=%08RX32 cb=%d rc=%Vrc\n", Port, *pu32Value, cbValue, rc));
        return rc;
    }

#ifndef IN_RING3
    /*
     * Handler in ring-3?
     */
    PIOMIOPORTRANGER3 pRangeR3 = iomIOPortGetRangeHC(&pVM->iom.s, Port);
    if (pRangeR3)
    {
# ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_COUNTER_INC(&pStats->CTXALLMID(In,ToR3));
# endif
        return VINF_IOM_HC_IOPORT_READ;
    }
#endif

    /*
     * Ok, no handler for this port.
     */
#ifdef VBOX_WITH_STATISTICS
    if (pStats)
        STAM_COUNTER_INC(&pStats->CTXALLSUFF(In));
    else
    {
# ifndef IN_RING3
        /* Ring-3 will have to create the statistics record. */
        return VINF_IOM_HC_IOPORT_READ;
# else
        pStats = iomr3IOPortStatsCreate(pVM, Port, NULL);
        if (pStats)
            STAM_COUNTER_INC(&pStats->CTXALLSUFF(In));
# endif
    }
#endif

    /* make return value */
    switch (cbValue)
    {
        case 1: *(uint8_t  *)pu32Value = 0xff; break;
        case 2: *(uint16_t *)pu32Value = 0xffff; break;
        case 4: *(uint32_t *)pu32Value = 0xffffffff; break;
        default:
            AssertMsgFailed(("Invalid I/O port size %d. Port=%d\n", cbValue, Port));
            return VERR_IOM_INVALID_IOPORT_SIZE;
    }
    Log3(("IOMIOPortRead: Port=%RTiop *pu32=%08RX32 cb=%d rc=VINF_SUCCESS\n", Port, *pu32Value, cbValue));
    return VINF_SUCCESS;
}


/**
 * Reads the string buffer of an I/O port register.
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented 
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the 
 *                                      status code must be passed on to EM.
 * @retval  VINF_IOM_HC_IOPORT_READ     Defer the read to ring-3. (R0/GC only)
 *
 * @param   pVM         VM handle.
 * @param   Port        The port to read.
 * @param   pGCPtrDst   Pointer to the destination buffer (GC, incremented appropriately).
 * @param   pcTransfers Pointer to the number of transfer units to read, on return remaining transfer units.
 * @param   cb          Size of the transfer unit (1, 2 or 4 bytes).
 *   */
IOMDECL(int) IOMIOPortReadString(PVM pVM, RTIOPORT Port, PRTGCPTR pGCPtrDst, PRTGCUINTREG pcTransfers, unsigned cb)
{
#ifdef LOG_ENABLED
    const RTGCUINTREG cTransfers = *pcTransfers;
#endif 
#ifdef VBOX_WITH_STATISTICS
    /*
     * Get the statistics record.
     */
    PIOMIOPORTSTATS pStats = CTXALLSUFF(pVM->iom.s.pStatsLastRead);
    if (!pStats || pStats->Core.Key != Port)
    {
        pStats = (PIOMIOPORTSTATS)RTAvloIOPortGet(&pVM->iom.s.CTXSUFF(pTrees)->IOPortStatTree, Port);
        if (pStats)
            CTXALLSUFF(pVM->iom.s.pStatsLastRead) = pStats;
    }
#endif

    /*
     * Get handler for current context.
     */
    CTXALLSUFF(PIOMIOPORTRANGE) pRange = CTXALLSUFF(pVM->iom.s.pRangeLastRead);
    if (    !pRange
        ||   (unsigned)Port - (unsigned)pRange->Port >= (unsigned)pRange->cPorts)
    {
        pRange = iomIOPortGetRange(&pVM->iom.s, Port);
        if (pRange)
            CTXALLSUFF(pVM->iom.s.pRangeLastRead) = pRange;
    }
#ifdef IN_GC
    Assert(!pRange || MMHyperIsInsideArea(pVM, pRange)); /** @todo r=bird: there is a macro for this which skips the #if'ing. */
#endif

    if (pRange)
    {
        /*
         * Found a range.
         */
#ifndef IN_RING3
        if (!pRange->pfnInStrCallback)
        {
# ifdef VBOX_WITH_STATISTICS
            if (pStats)
                STAM_COUNTER_INC(&pStats->CTXALLMID(In,ToR3));
# endif
            return VINF_IOM_HC_IOPORT_READ;
        }
#endif
        /* call the device. */
#ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_PROFILE_ADV_START(&pStats->CTXALLSUFF(ProfIn), a);
#endif

        int rc = pRange->pfnInStrCallback(pRange->pDevIns, pRange->pvUser, Port, pGCPtrDst, pcTransfers, cb);
#ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_PROFILE_ADV_STOP(&pStats->CTXALLSUFF(ProfIn), a);
        if (rc == VINF_SUCCESS && pStats)
            STAM_COUNTER_INC(&pStats->CTXALLSUFF(In));
# ifndef IN_RING3
        else if (rc == VINF_IOM_HC_IOPORT_READ && pStats)
            STAM_COUNTER_INC(&pStats->CTXALLMID(In, ToR3));
# endif
#endif
        Log3(("IOMIOPortReadStr: Port=%RTiop pGCPtrDst=%p pcTransfer=%p:{%#x->%#x} cb=%d rc=%Vrc\n",
              Port, pGCPtrDst, pcTransfers, cTransfers, *pcTransfers, cb, rc));
        return rc;
    }

#ifndef IN_RING3
    /*
     * Handler in ring-3?
     */
    PIOMIOPORTRANGER3 pRangeR3 = iomIOPortGetRangeHC(&pVM->iom.s, Port);
    if (pRangeR3)
    {
# ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_COUNTER_INC(&pStats->CTXALLMID(In,ToR3));
# endif
        return VINF_IOM_HC_IOPORT_READ;
    }
#endif

    /*
     * Ok, no handler for this port.
     */
#ifdef VBOX_WITH_STATISTICS
    if (pStats)
        STAM_COUNTER_INC(&pStats->CTXALLSUFF(In));
    else
    {
# ifndef IN_RING3
        /* Ring-3 will have to create the statistics record. */
        return VINF_IOM_HC_IOPORT_READ;
# else
        pStats = iomr3IOPortStatsCreate(pVM, Port, NULL);
        if (pStats)
            STAM_COUNTER_INC(&pStats->CTXALLSUFF(In));
# endif
    }
#endif

    Log3(("IOMIOPortReadStr: Port=%RTiop pGCPtrDst=%p pcTransfer=%p:{%#x->%#x} cb=%d rc=VINF_SUCCESS\n",
          Port, pGCPtrDst, pcTransfers, cTransfers, *pcTransfers, cb));
    return VINF_SUCCESS;
}


/**
 * Writes to an I/O port register.
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented 
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the 
 *                                      status code must be passed on to EM.
 * @retval  VINF_IOM_HC_IOPORT_WRITE    Defer the write to ring-3. (R0/GC only)
 *
 * @param   pVM         VM handle.
 * @param   Port        The port to write to.
 * @param   u32Value    The value to write.
 * @param   cbValue     The size of the register to read in bytes. 1, 2 or 4 bytes.
 */
IOMDECL(int) IOMIOPortWrite(PVM pVM, RTIOPORT Port, uint32_t u32Value, size_t cbValue)
{
/** @todo bird: When I get time, I'll remove the GC tree and link the GC entries to the ring-3 node. */
#ifdef VBOX_WITH_STATISTICS
    /*
     * Find the statistics record.
     */
    PIOMIOPORTSTATS pStats = CTXALLSUFF(pVM->iom.s.pStatsLastWrite);
    if (!pStats || pStats->Core.Key != Port)
    {
        pStats = (PIOMIOPORTSTATS)RTAvloIOPortGet(&pVM->iom.s.CTXSUFF(pTrees)->IOPortStatTree, Port);
        if (pStats)
            CTXALLSUFF(pVM->iom.s.pStatsLastWrite) = pStats;
    }
#endif

    /*
     * Get handler for current context.
     */
    CTXALLSUFF(PIOMIOPORTRANGE) pRange = CTXALLSUFF(pVM->iom.s.pRangeLastWrite);
    if (    !pRange
        ||   (unsigned)Port - (unsigned)pRange->Port >= (unsigned)pRange->cPorts)
    {
        pRange = iomIOPortGetRange(&pVM->iom.s, Port);
        if (pRange)
            CTXALLSUFF(pVM->iom.s.pRangeLastWrite) = pRange;
    }
#ifdef IN_GC
    Assert(!pRange || MMHyperIsInsideArea(pVM, pRange));
#endif

    if (pRange)
    {
        /*
         * Found a range.
         */
#ifndef IN_RING3
        if (!pRange->pfnOutCallback)
        {
# ifdef VBOX_WITH_STATISTICS
            if (pStats)
                STAM_COUNTER_INC(&pStats->CTXALLMID(Out,ToR3));
# endif
            return VINF_IOM_HC_IOPORT_WRITE;
        }
#endif
        /* call the device. */
#ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_PROFILE_ADV_START(&pStats->CTXALLSUFF(ProfOut), a);
#endif
        int rc = pRange->pfnOutCallback(pRange->pDevIns, pRange->pvUser, Port, u32Value, cbValue);

#ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_PROFILE_ADV_STOP(&pStats->CTXALLSUFF(ProfOut), a);
        if (rc == VINF_SUCCESS && pStats)
            STAM_COUNTER_INC(&pStats->CTXALLSUFF(Out));
# ifndef IN_RING3
        else if (rc == VINF_IOM_HC_IOPORT_WRITE && pStats)
            STAM_COUNTER_INC(&pStats->CTXALLMID(Out, ToR3));
# endif
#endif
        Log3(("IOMIOPortWrite: Port=%RTiop u32=%08RX32 cb=%d rc=%Vrc\n", Port, u32Value, cbValue, rc));
        return rc;
    }

#ifndef IN_RING3
    /*
     * Handler in ring-3?
     */
    PIOMIOPORTRANGER3 pRangeR3 = iomIOPortGetRangeHC(&pVM->iom.s, Port);
    if (pRangeR3)
    {
# ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_COUNTER_INC(&pStats->CTXALLMID(Out,ToR3));
# endif
        return VINF_IOM_HC_IOPORT_WRITE;
    }
#endif

    /*
     * Ok, no handler for that port.
     */
#ifdef VBOX_WITH_STATISTICS
    /* statistics. */
    if (pStats)
        STAM_COUNTER_INC(&pStats->CTXALLSUFF(Out));
    else
    {
# ifndef IN_RING3
        /* R3 will have to create the statistics record. */
        return VINF_IOM_HC_IOPORT_WRITE;
# else
        pStats = iomr3IOPortStatsCreate(pVM, Port, NULL);
        if (pStats)
            STAM_COUNTER_INC(&pStats->CTXALLSUFF(Out));
# endif
    }
#endif
    Log3(("IOMIOPortWrite: Port=%RTiop u32=%08RX32 cb=%d nop\n", Port, u32Value, cbValue));
    return VINF_SUCCESS;
}


/**
 * Writes the string buffer of an I/O port register.
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented 
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the 
 *                                      status code must be passed on to EM.
 * @retval  VINF_IOM_HC_IOPORT_WRITE    Defer the write to ring-3. (R0/GC only)
 *
 * @param   pVM         VM handle.
 * @param   Port        The port to write.
 * @param   pGCPtrSrc   Pointer to the source buffer (GC, incremented appropriately).
 * @param   pcTransfers Pointer to the number of transfer units to write, on return remaining transfer units.
 * @param   cb          Size of the transfer unit (1, 2 or 4 bytes).
 *   */
IOMDECL(int) IOMIOPortWriteString(PVM pVM, RTIOPORT Port, PRTGCPTR pGCPtrSrc, PRTGCUINTREG pcTransfers, unsigned cb)
{
#ifdef LOG_ENABLED
    const RTGCUINTREG cTransfers = *pcTransfers;
#endif 
#ifdef VBOX_WITH_STATISTICS
    /*
     * Get the statistics record.
     */
    PIOMIOPORTSTATS     pStats = CTXALLSUFF(pVM->iom.s.pStatsLastWrite);
    if (!pStats || pStats->Core.Key != Port)
    {
        pStats = (PIOMIOPORTSTATS)RTAvloIOPortGet(&pVM->iom.s.CTXSUFF(pTrees)->IOPortStatTree, Port);
        if (pStats)
            CTXALLSUFF(pVM->iom.s.pStatsLastWrite) = pStats;
    }
#endif

    /*
     * Get handler for current context.
     */
    CTXALLSUFF(PIOMIOPORTRANGE) pRange = CTXALLSUFF(pVM->iom.s.pRangeLastWrite);
    if (    !pRange
        ||   (unsigned)Port - (unsigned)pRange->Port >= (unsigned)pRange->cPorts)
    {
        pRange = iomIOPortGetRange(&pVM->iom.s, Port);
        if (pRange)
            CTXALLSUFF(pVM->iom.s.pRangeLastWrite) = pRange;
    }
#ifdef IN_GC
    Assert(!pRange || MMHyperIsInsideArea(pVM, pRange)); /** @todo r=bird: there is a macro for this which skips the #if'ing. */
#endif

    if (pRange)
    {
        /*
         * Found a range.
         */
#ifndef IN_RING3
        if (!pRange->pfnOutStrCallback)
        {
# ifdef VBOX_WITH_STATISTICS
            if (pStats)
                STAM_COUNTER_INC(&pStats->CTXALLMID(Out,ToR3));
# endif
            return VINF_IOM_HC_IOPORT_WRITE;
        }
#endif
        /* call the device. */
#ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_PROFILE_ADV_START(&pStats->CTXALLSUFF(ProfOut), a);
#endif
        int rc = pRange->pfnOutStrCallback(pRange->pDevIns, pRange->pvUser, Port, pGCPtrSrc, pcTransfers, cb);
#ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_PROFILE_ADV_STOP(&pStats->CTXALLSUFF(ProfOut), a);
        if (rc == VINF_SUCCESS && pStats)
            STAM_COUNTER_INC(&pStats->CTXALLSUFF(Out));
# ifndef IN_RING3
        else if (rc == VINF_IOM_HC_IOPORT_WRITE && pStats)
            STAM_COUNTER_INC(&pStats->CTXALLMID(Out, ToR3));
# endif
#endif
        Log3(("IOMIOPortWriteStr: Port=%RTiop pGCPtrSrc=%p pcTransfer=%p:{%#x->%#x} cb=%d rc=%Vrc\n",
              Port, pGCPtrSrc, pcTransfers, cTransfers, *pcTransfers, cb, rc));
        return rc;
    }

#ifndef IN_RING3
    /*
     * Handler in ring-3?
     */
    PIOMIOPORTRANGER3 pRangeR3 = iomIOPortGetRangeHC(&pVM->iom.s, Port);
    if (pRangeR3)
    {
# ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_COUNTER_INC(&pStats->CTXALLMID(Out,ToR3));
# endif
        return VINF_IOM_HC_IOPORT_WRITE;
    }
#endif

    /*
     * Ok, no handler for this port.
     */
#ifdef VBOX_WITH_STATISTICS
    if (pStats)
        STAM_COUNTER_INC(&pStats->CTXALLSUFF(Out));
    else
    {
# ifndef IN_RING3
        /* Ring-3 will have to create the statistics record. */
        return VINF_IOM_HC_IOPORT_WRITE;
# else
        pStats = iomr3IOPortStatsCreate(pVM, Port, NULL);
        if (pStats)
            STAM_COUNTER_INC(&pStats->CTXALLSUFF(Out));
# endif
    }
#endif

    Log3(("IOMIOPortWriteStr: Port=%RTiop pGCPtrSrc=%p pcTransfer=%p:{%#x->%#x} cb=%d rc=VINF_SUCCESS\n",
          Port, pGCPtrSrc, pcTransfers, cTransfers, *pcTransfers, cb));
    return VINF_SUCCESS;
}


/**
 * Checks that the operation is allowed according to the IOPL
 * level and I/O bitmap.
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented 
 *          here are to be treated as internal failure.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_RAW_GUEST_TRAP      The exception was left pending. (TRPMRaiseXcptErr)
 * @retval  VINF_TRPM_XCPT_DISPATCHED   The exception was raised and dispatched for raw-mode execution. (TRPMRaiseXcptErr)
 * @retval  VINF_EM_RESCHEDULE_REM      The exception was dispatched and cannot be executed in raw-mode. (TRPMRaiseXcptErr)
 *
 * @param   pVM         VM handle.
 * @param   pCtxCore    Pointer to register frame.
 * @param   Port        The I/O port number.
 * @param   cb          The access size.
 */
IOMDECL(int) IOMInterpretCheckPortIOAccess(PVM pVM, PCPUMCTXCORE pCtxCore, RTIOPORT Port, unsigned cb)
{
    /*
     * If this isn't ring-0, we have to check for I/O privileges.
     */
    uint32_t efl = CPUMRawGetEFlags(pVM, pCtxCore);
    uint32_t cpl = CPUMGetGuestCPL(pVM, pCtxCore);

    if (    (    cpl > 0
             &&  X86_EFL_GET_IOPL(efl) < cpl)
        ||  pCtxCore->eflags.Bits.u1VM      /* IOPL is ignored in V86 mode; always check TSS bitmap */
       )
    {
        /*
         * Get TSS location and check if there can be a I/O bitmap.
         */
        RTGCUINTPTR GCPtrTss;
        RTGCUINTPTR cbTss;
        bool        fCanHaveIOBitmap;
        int rc = SELMGetTSSInfo(pVM, &GCPtrTss, &cbTss, &fCanHaveIOBitmap);
        if (VBOX_FAILURE(rc))
        {
            Log(("iomInterpretCheckPortIOAccess: Port=%RTiop cb=%d %Vrc -> #GP(0)\n", Port, cb, rc));
            return TRPMRaiseXcptErr(pVM, pCtxCore, X86_XCPT_GP, 0);
        }

        if (    !fCanHaveIOBitmap
            ||  cbTss <= sizeof(VBOXTSS))
        {
            Log(("iomInterpretCheckPortIOAccess: Port=%RTiop cb=%d cbTss=%#x fCanHaveIOBitmap=%RTbool -> #GP(0)\n",
                 Port, cb, cbTss, fCanHaveIOBitmap));
            return TRPMRaiseXcptErr(pVM, pCtxCore, X86_XCPT_GP, 0);
        }

        /*
         * Fetch the I/O bitmap offset.
         */
        uint16_t offIOPB;
        rc = PGMPhysInterpretedRead(pVM, pCtxCore, &offIOPB, GCPtrTss + RT_OFFSETOF(VBOXTSS, offIoBitmap), sizeof(offIOPB));
        if (rc != VINF_SUCCESS)
        {
            Log(("iomInterpretCheckPortIOAccess: Port=%RTiop cb=%d GCPtrTss=%VGv %Vrc\n",
                 Port, cb, GCPtrTss, rc));
            return rc;
        }

        /*
         * Check the limit and read the two bitmap bytes.
         */
        uint32_t offTss = offIOPB + (Port >> 3);
        if (offTss + 1 >= cbTss)
        {
            Log(("iomInterpretCheckPortIOAccess: Port=%RTiop cb=%d offTss=%#x cbTss=%#x -> #GP(0)\n",
                 Port, cb, offTss, cbTss));
            return TRPMRaiseXcptErr(pVM, pCtxCore, X86_XCPT_GP, 0);
        }
        uint16_t u16;
        rc = PGMPhysInterpretedRead(pVM, pCtxCore, &u16, GCPtrTss + offTss, sizeof(u16));
        if (rc != VINF_SUCCESS)
        {
            Log(("iomInterpretCheckPortIOAccess: Port=%RTiop cb=%d GCPtrTss=%VGv offTss=%#x -> %Vrc\n",
                 Port, cb, GCPtrTss, offTss, rc));
            return rc;
        }

        /*
         * All the bits must be clear.
         */
        if ((u16 >> (Port & 7)) & ((1 << cb) - 1))
        {
            Log(("iomInterpretCheckPortIOAccess: Port=%RTiop cb=%d u16=%#x -> #GP(0)\n",
                 Port, cb, u16, offTss));
            return TRPMRaiseXcptErr(pVM, pCtxCore, X86_XCPT_GP, 0);
        }
        LogFlow(("iomInterpretCheckPortIOAccess: Port=%RTiop cb=%d offTss=%#x cbTss=%#x u16=%#x -> OK\n",
                 Port, cb, u16, offTss, cbTss));
    }
    return VINF_SUCCESS;
}

/**
 * IN <AL|AX|EAX>, <DX|imm16>
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented 
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the 
 *                                      status code must be passed on to EM.
 * @retval  VINF_IOM_HC_IOPORT_READ     Defer the read to ring-3. (R0/GC only)
 * @retval  VINF_EM_RAW_GUEST_TRAP      The exception was left pending. (TRPMRaiseXcptErr)
 * @retval  VINF_TRPM_XCPT_DISPATCHED   The exception was raised and dispatched for raw-mode execution. (TRPMRaiseXcptErr)
 * @retval  VINF_EM_RESCHEDULE_REM      The exception was dispatched and cannot be executed in raw-mode. (TRPMRaiseXcptErr)
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Pointer to CPUMCTXCORE guest registers structure.
 * @param   pCpu        Disassembler CPU state.
 */
IOMDECL(int) IOMInterpretIN(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu)
{
#ifdef IN_GC
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
        if (IOM_SUCCESS(rc))
        {
            /*
             * Store the result in the AL|AX|EAX register.
             */
            fRc = iomGCSaveDataToReg(pCpu, &pCpu->param1, pRegFrame, u32Data);
            AssertMsg(fRc, ("Failed to store register value!\n")); NOREF(fRc);
        }
        else
            AssertMsg(rc == VINF_IOM_HC_IOPORT_READ || VBOX_FAILURE(rc), ("%Vrc\n", rc));
    }
    else
        AssertMsg(rc == VINF_EM_RAW_GUEST_TRAP || rc == VINF_TRPM_XCPT_DISPATCHED || rc == VINF_TRPM_XCPT_DISPATCHED || VBOX_FAILURE(rc), ("%Vrc\n", rc));
    return rc;
}


/**
 * OUT <DX|imm16>, <AL|AX|EAX>
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
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Pointer to CPUMCTXCORE guest registers structure.
 * @param   pCpu        Disassembler CPU state.
 */
IOMDECL(int) IOMInterpretOUT(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu)
{
#ifdef IN_GC
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
         * Attempt to write to the port.
         */
        rc = IOMIOPortWrite(pVM, uPort, u32Data, cbSize);
        AssertMsg(rc == VINF_SUCCESS || rc == VINF_IOM_HC_IOPORT_WRITE || (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST) || VBOX_FAILURE(rc), ("%Vrc\n", rc));
    }
    else
        AssertMsg(rc == VINF_EM_RAW_GUEST_TRAP || rc == VINF_TRPM_XCPT_DISPATCHED || rc == VINF_TRPM_XCPT_DISPATCHED || VBOX_FAILURE(rc), ("%Vrc\n", rc));
    return rc;
}
