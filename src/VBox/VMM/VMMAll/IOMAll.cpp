/* $Id$ */
/** @file
 * IOM - Input / Output Monitor - Any Context.
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


/**
 * Calculates the size of register parameter.
 *
 * @returns 1, 2, 4 on success.
 * @returns 0 if non-register parameter.
 * @param   pCpu                Pointer to current disassembler context.
 * @param   pParam              Pointer to parameter of instruction to proccess.
 */
static unsigned iomGetRegSize(PDISCPUSTATE pCpu, PCOP_PARAMETER pParam)
{
    if (pParam->flags & (USE_BASE | USE_INDEX | USE_SCALE | USE_DISPLACEMENT8 | USE_DISPLACEMENT16 | USE_DISPLACEMENT32 | USE_IMMEDIATE8 | USE_IMMEDIATE16 | USE_IMMEDIATE32 | USE_IMMEDIATE16_SX8 | USE_IMMEDIATE32_SX8))
        return 0;

    if (pParam->flags & USE_REG_GEN32)
        return 4;

    if (pParam->flags & USE_REG_GEN16)
        return 2;

    if (pParam->flags & USE_REG_GEN8)
        return 1;

    if (pParam->flags & USE_REG_GEN64)
        return 8;

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
bool iomGetRegImmData(PDISCPUSTATE pCpu, PCOP_PARAMETER pParam, PCPUMCTXCORE pRegFrame, uint32_t *pu32Data, unsigned *pcbSize)
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
        DISFetchReg32(pRegFrame, pParam->base.reg_gen, pu32Data);
        return true;
    }

    if (pParam->flags & USE_REG_GEN16)
    {
        *pcbSize  = 2;
        DISFetchReg16(pRegFrame, pParam->base.reg_gen, (uint16_t *)pu32Data);
        return true;
    }

    if (pParam->flags & USE_REG_GEN8)
    {
        *pcbSize  = 1;
        DISFetchReg8(pRegFrame, pParam->base.reg_gen, (uint8_t *)pu32Data);
        return true;
    }

    if (pParam->flags & USE_REG_GEN64)
    {
        AssertFailed();
        *pcbSize  = 8;
        ///DISFetchReg64(pRegFrame, pParam->base.reg_gen, pu32Data);
        return true;
    }

    if (pParam->flags & (USE_IMMEDIATE64))
    {
        AssertFailed();
        *pcbSize  = 8;
        *pu32Data = (uint32_t)pParam->parval;
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

    AssertFailed();
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
bool iomSaveDataToReg(PDISCPUSTATE pCpu, PCOP_PARAMETER pParam, PCPUMCTXCORE pRegFrame, unsigned u32Data)
{
    if (pParam->flags & (USE_BASE | USE_INDEX | USE_SCALE | USE_DISPLACEMENT8 | USE_DISPLACEMENT16 | USE_DISPLACEMENT32 | USE_IMMEDIATE8 | USE_IMMEDIATE16 | USE_IMMEDIATE32 | USE_IMMEDIATE32_SX8 | USE_IMMEDIATE16_SX8))
    {
        return false;
    }

    if (pParam->flags & USE_REG_GEN32)
    {
        DISWriteReg32(pRegFrame, pParam->base.reg_gen, u32Data);
        return true;
    }

    if (pParam->flags & USE_REG_GEN16)
    {
        DISWriteReg16(pRegFrame, pParam->base.reg_gen, (uint16_t)u32Data);
        return true;
    }

    if (pParam->flags & USE_REG_GEN8)
    {
        DISWriteReg8(pRegFrame, pParam->base.reg_gen, (uint8_t)u32Data);
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
DECLINLINE(void) iomMMIOStatLength(PVM pVM, unsigned cb)
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
    Assert(!pRange || MMHyperIsInsideArea(pVM, (RTGCPTR)pRange)); /** @todo r=bird: there is a macro for this which skips the #if'ing. */
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
    Assert(!pRange || MMHyperIsInsideArea(pVM, (RTGCPTR)pRange)); /** @todo r=bird: there is a macro for this which skips the #if'ing. */
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
    Assert(!pRange || MMHyperIsInsideArea(pVM, (RTGCPTR)pRange));
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
    Assert(!pRange || MMHyperIsInsideArea(pVM, (RTGCPTR)pRange)); /** @todo r=bird: there is a macro for this which skips the #if'ing. */
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
    bool fRc = iomGetRegImmData(pCpu, &pCpu->param2, pRegFrame, &uPort, &cbSize);
    AssertMsg(fRc, ("Failed to get reg/imm port number!\n")); NOREF(fRc);

    cbSize = iomGetRegSize(pCpu, &pCpu->param1);
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
            fRc = iomSaveDataToReg(pCpu, &pCpu->param1, pRegFrame, u32Data);
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
    bool fRc = iomGetRegImmData(pCpu, &pCpu->param1, pRegFrame, &uPort, &cbSize);
    AssertMsg(fRc, ("Failed to get reg/imm port number!\n")); NOREF(fRc);

    int rc = IOMInterpretCheckPortIOAccess(pVM, pRegFrame, uPort, cbSize);
    if (rc == VINF_SUCCESS)
    {
        uint32_t    u32Data = 0;
        fRc = iomGetRegImmData(pCpu, &pCpu->param2, pRegFrame, &u32Data, &cbSize);
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
