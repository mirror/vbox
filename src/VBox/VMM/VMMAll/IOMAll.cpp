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
 * Returns the contents of register or immediate data of instruction's parameter.
 *
 * @returns true on success.
 *
 * @todo Get rid of this code. Use DISQueryParamVal instead
 *
 * @param   pCpu                Pointer to current disassembler context.
 * @param   pParam              Pointer to parameter of instruction to proccess.
 * @param   pRegFrame           Pointer to CPUMCTXCORE guest structure.
 * @param   pu64Data            Where to store retrieved data.
 * @param   pcbSize             Where to store the size of data (1, 2, 4, 8).
 */
bool iomGetRegImmData(PDISCPUSTATE pCpu, PCOP_PARAMETER pParam, PCPUMCTXCORE pRegFrame, uint64_t *pu64Data, unsigned *pcbSize)
{
    if (pParam->flags & (USE_BASE | USE_INDEX | USE_SCALE | USE_DISPLACEMENT8 | USE_DISPLACEMENT16 | USE_DISPLACEMENT32))
    {
        *pcbSize  = 0;
        *pu64Data = 0;
        return false;
    }

    /* divide and conquer */
    if (pParam->flags & (USE_REG_GEN64 | USE_REG_GEN32 | USE_REG_GEN16 | USE_REG_GEN8))
    {
        if (pParam->flags & USE_REG_GEN32)
        {
            *pcbSize  = 4;
            DISFetchReg32(pRegFrame, pParam->base.reg_gen, (uint32_t *)pu64Data);
            return true;
        }

        if (pParam->flags & USE_REG_GEN16)
        {
            *pcbSize  = 2;
            DISFetchReg16(pRegFrame, pParam->base.reg_gen, (uint16_t *)pu64Data);
            return true;
        }

        if (pParam->flags & USE_REG_GEN8)
        {
            *pcbSize  = 1;
            DISFetchReg8(pRegFrame, pParam->base.reg_gen, (uint8_t *)pu64Data);
            return true;
        }

        Assert(pParam->flags & USE_REG_GEN64);
        *pcbSize  = 8;
        DISFetchReg64(pRegFrame, pParam->base.reg_gen, pu64Data);
        return true;
    }
    else
    {
        if (pParam->flags & (USE_IMMEDIATE64 | USE_IMMEDIATE64_SX8))
        {
            *pcbSize  = 8;
            *pu64Data = pParam->parval;
            return true;
        }

        if (pParam->flags & (USE_IMMEDIATE32 | USE_IMMEDIATE32_SX8))
        {
            *pcbSize  = 4;
            *pu64Data = (uint32_t)pParam->parval;
            return true;
        }

        if (pParam->flags & (USE_IMMEDIATE16 | USE_IMMEDIATE16_SX8))
        {
            *pcbSize  = 2;
            *pu64Data = (uint16_t)pParam->parval;
            return true;
        }

        if (pParam->flags & USE_IMMEDIATE8)
        {
            *pcbSize  = 1;
            *pu64Data = (uint8_t)pParam->parval;
            return true;
        }

        if (pParam->flags & USE_REG_SEG)
        {
            *pcbSize  = 2;
            DISFetchRegSeg(pRegFrame, pParam->base.reg_seg, (RTSEL *)pu64Data);
            return true;
        } /* Else - error. */

        AssertFailed();
        *pcbSize  = 0;
        *pu64Data = 0;
        return false;
    }
}


/**
 * Saves data to 8/16/32 general purpose or segment register defined by
 * instruction's parameter.
 *
 * @returns true on success.
 * @param   pCpu                Pointer to current disassembler context.
 * @param   pParam              Pointer to parameter of instruction to proccess.
 * @param   pRegFrame           Pointer to CPUMCTXCORE guest structure.
 * @param   u64Data             8/16/32/64 bit data to store.
 */
bool iomSaveDataToReg(PDISCPUSTATE pCpu, PCOP_PARAMETER pParam, PCPUMCTXCORE pRegFrame, uint64_t u64Data)
{
    if (pParam->flags & (USE_BASE | USE_INDEX | USE_SCALE | USE_DISPLACEMENT8 | USE_DISPLACEMENT16 | USE_DISPLACEMENT32 | USE_DISPLACEMENT64 | USE_IMMEDIATE8 | USE_IMMEDIATE16 | USE_IMMEDIATE32 | USE_IMMEDIATE32_SX8 | USE_IMMEDIATE16_SX8))
    {
        return false;
    }

    if (pParam->flags & USE_REG_GEN32)
    {
        DISWriteReg32(pRegFrame, pParam->base.reg_gen, (uint32_t)u64Data);
        return true;
    }

    if (pParam->flags & USE_REG_GEN64)
    {
        DISWriteReg64(pRegFrame, pParam->base.reg_gen, u64Data);
        return true;
    }

    if (pParam->flags & USE_REG_GEN16)
    {
        DISWriteReg16(pRegFrame, pParam->base.reg_gen, (uint16_t)u64Data);
        return true;
    }

    if (pParam->flags & USE_REG_GEN8)
    {
        DISWriteReg8(pRegFrame, pParam->base.reg_gen, (uint8_t)u64Data);
        return true;
    }

    if (pParam->flags & USE_REG_SEG)
    {
        DISWriteRegSeg(pRegFrame, pParam->base.reg_seg, (RTSEL)u64Data);
        return true;
    }

    /* Else - error. */
    return false;
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
VMMDECL(int) IOMIOPortRead(PVM pVM, RTIOPORT Port, uint32_t *pu32Value, size_t cbValue)
{
#ifdef VBOX_WITH_STATISTICS
    /*
     * Get the statistics record.
     */
    PIOMIOPORTSTATS  pStats = pVM->iom.s.CTX_SUFF(pStatsLastRead);
    if (!pStats || pStats->Core.Key != Port)
    {
        pStats = (PIOMIOPORTSTATS)RTAvloIOPortGet(&pVM->iom.s.CTX_SUFF(pTrees)->IOPortStatTree, Port);
        if (pStats)
            pVM->iom.s.CTX_SUFF(pStatsLastRead) = pStats;
    }
#endif

    /*
     * Get handler for current context.
     */
    CTX_SUFF(PIOMIOPORTRANGE) pRange = pVM->iom.s.CTX_SUFF(pRangeLastRead);
    if (    !pRange
        ||   (unsigned)Port - (unsigned)pRange->Port >= (unsigned)pRange->cPorts)
    {
        pRange = iomIOPortGetRange(&pVM->iom.s, Port);
        if (pRange)
            pVM->iom.s.CTX_SUFF(pRangeLastRead) = pRange;
    }
    MMHYPER_RC_ASSERT_RCPTR(pVM, pRange);
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
                STAM_COUNTER_INC(&pStats->CTX_MID_Z(In,ToR3));
# endif
            return VINF_IOM_HC_IOPORT_READ;
        }
#endif
        /* call the device. */
#ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_PROFILE_ADV_START(&pStats->CTX_SUFF_Z(ProfIn), a);
#endif
        int rc = pRange->pfnInCallback(pRange->pDevIns, pRange->pvUser, Port, pu32Value, (unsigned)cbValue);
#ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_PROFILE_ADV_STOP(&pStats->CTX_SUFF_Z(ProfIn), a);
        if (rc == VINF_SUCCESS && pStats)
            STAM_COUNTER_INC(&pStats->CTX_SUFF_Z(In));
# ifndef IN_RING3
        else if (rc == VINF_IOM_HC_IOPORT_READ && pStats)
            STAM_COUNTER_INC(&pStats->CTX_MID_Z(In,ToR3));
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
                case 4: *(uint32_t *)pu32Value = UINT32_C(0xffffffff); break;
                default:
                    AssertMsgFailed(("Invalid I/O port size %d. Port=%d\n", cbValue, Port));
                    return VERR_IOM_INVALID_IOPORT_SIZE;
            }
        }
        Log3(("IOMIOPortRead: Port=%RTiop *pu32=%08RX32 cb=%d rc=%Rrc\n", Port, *pu32Value, cbValue, rc));
        return rc;
    }

#ifndef IN_RING3
    /*
     * Handler in ring-3?
     */
    PIOMIOPORTRANGER3 pRangeR3 = iomIOPortGetRangeR3(&pVM->iom.s, Port);
    if (pRangeR3)
    {
# ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_COUNTER_INC(&pStats->CTX_MID_Z(In,ToR3));
# endif
        return VINF_IOM_HC_IOPORT_READ;
    }
#endif

    /*
     * Ok, no handler for this port.
     */
#ifdef VBOX_WITH_STATISTICS
    if (pStats)
        STAM_COUNTER_INC(&pStats->CTX_SUFF_Z(In));
    else
    {
# ifndef IN_RING3
        /* Ring-3 will have to create the statistics record. */
        return VINF_IOM_HC_IOPORT_READ;
# else
        pStats = iomR3IOPortStatsCreate(pVM, Port, NULL);
        if (pStats)
            STAM_COUNTER_INC(&pStats->CTX_SUFF_Z(In));
# endif
    }
#endif

    /* make return value */
    switch (cbValue)
    {
        case 1: *(uint8_t  *)pu32Value = 0xff; break;
        case 2: *(uint16_t *)pu32Value = 0xffff; break;
        case 4: *(uint32_t *)pu32Value = UINT32_C(0xffffffff); break;
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
VMMDECL(int) IOMIOPortReadString(PVM pVM, RTIOPORT Port, PRTGCPTR pGCPtrDst, PRTGCUINTREG pcTransfers, unsigned cb)
{
#ifdef LOG_ENABLED
    const RTGCUINTREG cTransfers = *pcTransfers;
#endif
#ifdef VBOX_WITH_STATISTICS
    /*
     * Get the statistics record.
     */
    PIOMIOPORTSTATS pStats = pVM->iom.s.CTX_SUFF(pStatsLastRead);
    if (!pStats || pStats->Core.Key != Port)
    {
        pStats = (PIOMIOPORTSTATS)RTAvloIOPortGet(&pVM->iom.s.CTX_SUFF(pTrees)->IOPortStatTree, Port);
        if (pStats)
            pVM->iom.s.CTX_SUFF(pStatsLastRead) = pStats;
    }
#endif

    /*
     * Get handler for current context.
     */
    CTX_SUFF(PIOMIOPORTRANGE) pRange = pVM->iom.s.CTX_SUFF(pRangeLastRead);
    if (    !pRange
        ||   (unsigned)Port - (unsigned)pRange->Port >= (unsigned)pRange->cPorts)
    {
        pRange = iomIOPortGetRange(&pVM->iom.s, Port);
        if (pRange)
            pVM->iom.s.CTX_SUFF(pRangeLastRead) = pRange;
    }
    MMHYPER_RC_ASSERT_RCPTR(pVM, pRange);
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
                STAM_COUNTER_INC(&pStats->CTX_MID_Z(In,ToR3));
# endif
            return VINF_IOM_HC_IOPORT_READ;
        }
#endif
        /* call the device. */
#ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_PROFILE_ADV_START(&pStats->CTX_SUFF_Z(ProfIn), a);
#endif

        int rc = pRange->pfnInStrCallback(pRange->pDevIns, pRange->pvUser, Port, pGCPtrDst, pcTransfers, cb);
#ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_PROFILE_ADV_STOP(&pStats->CTX_SUFF_Z(ProfIn), a);
        if (rc == VINF_SUCCESS && pStats)
            STAM_COUNTER_INC(&pStats->CTX_SUFF_Z(In));
# ifndef IN_RING3
        else if (rc == VINF_IOM_HC_IOPORT_READ && pStats)
            STAM_COUNTER_INC(&pStats->CTX_MID_Z(In, ToR3));
# endif
#endif
        Log3(("IOMIOPortReadStr: Port=%RTiop pGCPtrDst=%p pcTransfer=%p:{%#x->%#x} cb=%d rc=%Rrc\n",
              Port, pGCPtrDst, pcTransfers, cTransfers, *pcTransfers, cb, rc));
        return rc;
    }

#ifndef IN_RING3
    /*
     * Handler in ring-3?
     */
    PIOMIOPORTRANGER3 pRangeR3 = iomIOPortGetRangeR3(&pVM->iom.s, Port);
    if (pRangeR3)
    {
# ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_COUNTER_INC(&pStats->CTX_MID_Z(In,ToR3));
# endif
        return VINF_IOM_HC_IOPORT_READ;
    }
#endif

    /*
     * Ok, no handler for this port.
     */
#ifdef VBOX_WITH_STATISTICS
    if (pStats)
        STAM_COUNTER_INC(&pStats->CTX_SUFF_Z(In));
    else
    {
# ifndef IN_RING3
        /* Ring-3 will have to create the statistics record. */
        return VINF_IOM_HC_IOPORT_READ;
# else
        pStats = iomR3IOPortStatsCreate(pVM, Port, NULL);
        if (pStats)
            STAM_COUNTER_INC(&pStats->CTX_SUFF_Z(In));
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
VMMDECL(int) IOMIOPortWrite(PVM pVM, RTIOPORT Port, uint32_t u32Value, size_t cbValue)
{
/** @todo bird: When I get time, I'll remove the GC tree and link the GC entries to the ring-3 node. */
#ifdef VBOX_WITH_STATISTICS
    /*
     * Find the statistics record.
     */
    PIOMIOPORTSTATS pStats = pVM->iom.s.CTX_SUFF(pStatsLastWrite);
    if (!pStats || pStats->Core.Key != Port)
    {
        pStats = (PIOMIOPORTSTATS)RTAvloIOPortGet(&pVM->iom.s.CTX_SUFF(pTrees)->IOPortStatTree, Port);
        if (pStats)
            pVM->iom.s.CTX_SUFF(pStatsLastWrite) = pStats;
    }
#endif

    /*
     * Get handler for current context.
     */
    CTX_SUFF(PIOMIOPORTRANGE) pRange = pVM->iom.s.CTX_SUFF(pRangeLastWrite);
    if (    !pRange
        ||   (unsigned)Port - (unsigned)pRange->Port >= (unsigned)pRange->cPorts)
    {
        pRange = iomIOPortGetRange(&pVM->iom.s, Port);
        if (pRange)
            pVM->iom.s.CTX_SUFF(pRangeLastWrite) = pRange;
    }
    MMHYPER_RC_ASSERT_RCPTR(pVM, pRange);
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
                STAM_COUNTER_INC(&pStats->CTX_MID_Z(Out,ToR3));
# endif
            return VINF_IOM_HC_IOPORT_WRITE;
        }
#endif
        /* call the device. */
#ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_PROFILE_ADV_START(&pStats->CTX_SUFF_Z(ProfOut), a);
#endif
        int rc = pRange->pfnOutCallback(pRange->pDevIns, pRange->pvUser, Port, u32Value, (unsigned)cbValue);

#ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_PROFILE_ADV_STOP(&pStats->CTX_SUFF_Z(ProfOut), a);
        if (rc == VINF_SUCCESS && pStats)
            STAM_COUNTER_INC(&pStats->CTX_SUFF_Z(Out));
# ifndef IN_RING3
        else if (rc == VINF_IOM_HC_IOPORT_WRITE && pStats)
            STAM_COUNTER_INC(&pStats->CTX_MID_Z(Out, ToR3));
# endif
#endif
        Log3(("IOMIOPortWrite: Port=%RTiop u32=%08RX32 cb=%d rc=%Rrc\n", Port, u32Value, cbValue, rc));
        return rc;
    }

#ifndef IN_RING3
    /*
     * Handler in ring-3?
     */
    PIOMIOPORTRANGER3 pRangeR3 = iomIOPortGetRangeR3(&pVM->iom.s, Port);
    if (pRangeR3)
    {
# ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_COUNTER_INC(&pStats->CTX_MID_Z(Out,ToR3));
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
        STAM_COUNTER_INC(&pStats->CTX_SUFF_Z(Out));
    else
    {
# ifndef IN_RING3
        /* R3 will have to create the statistics record. */
        return VINF_IOM_HC_IOPORT_WRITE;
# else
        pStats = iomR3IOPortStatsCreate(pVM, Port, NULL);
        if (pStats)
            STAM_COUNTER_INC(&pStats->CTX_SUFF_Z(Out));
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
VMMDECL(int) IOMIOPortWriteString(PVM pVM, RTIOPORT Port, PRTGCPTR pGCPtrSrc, PRTGCUINTREG pcTransfers, unsigned cb)
{
#ifdef LOG_ENABLED
    const RTGCUINTREG cTransfers = *pcTransfers;
#endif
#ifdef VBOX_WITH_STATISTICS
    /*
     * Get the statistics record.
     */
    PIOMIOPORTSTATS     pStats = pVM->iom.s.CTX_SUFF(pStatsLastWrite);
    if (!pStats || pStats->Core.Key != Port)
    {
        pStats = (PIOMIOPORTSTATS)RTAvloIOPortGet(&pVM->iom.s.CTX_SUFF(pTrees)->IOPortStatTree, Port);
        if (pStats)
            pVM->iom.s.CTX_SUFF(pStatsLastWrite) = pStats;
    }
#endif

    /*
     * Get handler for current context.
     */
    CTX_SUFF(PIOMIOPORTRANGE) pRange = pVM->iom.s.CTX_SUFF(pRangeLastWrite);
    if (    !pRange
        ||   (unsigned)Port - (unsigned)pRange->Port >= (unsigned)pRange->cPorts)
    {
        pRange = iomIOPortGetRange(&pVM->iom.s, Port);
        if (pRange)
            pVM->iom.s.CTX_SUFF(pRangeLastWrite) = pRange;
    }
    MMHYPER_RC_ASSERT_RCPTR(pVM, pRange);
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
                STAM_COUNTER_INC(&pStats->CTX_MID_Z(Out,ToR3));
# endif
            return VINF_IOM_HC_IOPORT_WRITE;
        }
#endif
        /* call the device. */
#ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_PROFILE_ADV_START(&pStats->CTX_SUFF_Z(ProfOut), a);
#endif
        int rc = pRange->pfnOutStrCallback(pRange->pDevIns, pRange->pvUser, Port, pGCPtrSrc, pcTransfers, cb);
#ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_PROFILE_ADV_STOP(&pStats->CTX_SUFF_Z(ProfOut), a);
        if (rc == VINF_SUCCESS && pStats)
            STAM_COUNTER_INC(&pStats->CTX_SUFF_Z(Out));
# ifndef IN_RING3
        else if (rc == VINF_IOM_HC_IOPORT_WRITE && pStats)
            STAM_COUNTER_INC(&pStats->CTX_MID_Z(Out, ToR3));
# endif
#endif
        Log3(("IOMIOPortWriteStr: Port=%RTiop pGCPtrSrc=%p pcTransfer=%p:{%#x->%#x} cb=%d rc=%Rrc\n",
              Port, pGCPtrSrc, pcTransfers, cTransfers, *pcTransfers, cb, rc));
        return rc;
    }

#ifndef IN_RING3
    /*
     * Handler in ring-3?
     */
    PIOMIOPORTRANGER3 pRangeR3 = iomIOPortGetRangeR3(&pVM->iom.s, Port);
    if (pRangeR3)
    {
# ifdef VBOX_WITH_STATISTICS
        if (pStats)
            STAM_COUNTER_INC(&pStats->CTX_MID_Z(Out,ToR3));
# endif
        return VINF_IOM_HC_IOPORT_WRITE;
    }
#endif

    /*
     * Ok, no handler for this port.
     */
#ifdef VBOX_WITH_STATISTICS
    if (pStats)
        STAM_COUNTER_INC(&pStats->CTX_SUFF_Z(Out));
    else
    {
# ifndef IN_RING3
        /* Ring-3 will have to create the statistics record. */
        return VINF_IOM_HC_IOPORT_WRITE;
# else
        pStats = iomR3IOPortStatsCreate(pVM, Port, NULL);
        if (pStats)
            STAM_COUNTER_INC(&pStats->CTX_SUFF_Z(Out));
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
VMMDECL(int) IOMInterpretCheckPortIOAccess(PVM pVM, PCPUMCTXCORE pCtxCore, RTIOPORT Port, unsigned cb)
{
    /* @todo SMP support */
    Assert(pVM->cCPUs == 1);
    PVMCPU pVCpu = &pVM->aCpus[0];

    /*
     * If this isn't ring-0, we have to check for I/O privileges.
     */
    uint32_t efl = CPUMRawGetEFlags(pVCpu, pCtxCore);
    uint32_t cpl = CPUMGetGuestCPL(pVCpu, pCtxCore);

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
        int rc = SELMGetTSSInfo(pVM, pVCpu, &GCPtrTss, &cbTss, &fCanHaveIOBitmap);
        if (RT_FAILURE(rc))
        {
            Log(("iomInterpretCheckPortIOAccess: Port=%RTiop cb=%d %Rrc -> #GP(0)\n", Port, cb, rc));
            return TRPMRaiseXcptErr(pVCpu, pCtxCore, X86_XCPT_GP, 0);
        }

        if (    !fCanHaveIOBitmap
            ||  cbTss <= sizeof(VBOXTSS)) /** @todo r=bird: Should this really include the interrupt redirection bitmap? */
        {
            Log(("iomInterpretCheckPortIOAccess: Port=%RTiop cb=%d cbTss=%#x fCanHaveIOBitmap=%RTbool -> #GP(0)\n",
                 Port, cb, cbTss, fCanHaveIOBitmap));
            return TRPMRaiseXcptErr(pVCpu, pCtxCore, X86_XCPT_GP, 0);
        }

        /*
         * Fetch the I/O bitmap offset.
         */
        uint16_t offIOPB;
        rc = PGMPhysInterpretedRead(pVCpu, pCtxCore, &offIOPB, GCPtrTss + RT_OFFSETOF(VBOXTSS, offIoBitmap), sizeof(offIOPB));
        if (rc != VINF_SUCCESS)
        {
            Log(("iomInterpretCheckPortIOAccess: Port=%RTiop cb=%d GCPtrTss=%RGv %Rrc\n",
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
            return TRPMRaiseXcptErr(pVCpu, pCtxCore, X86_XCPT_GP, 0);
        }
        uint16_t u16;
        rc = PGMPhysInterpretedRead(pVCpu, pCtxCore, &u16, GCPtrTss + offTss, sizeof(u16));
        if (rc != VINF_SUCCESS)
        {
            Log(("iomInterpretCheckPortIOAccess: Port=%RTiop cb=%d GCPtrTss=%RGv offTss=%#x -> %Rrc\n",
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
            return TRPMRaiseXcptErr(pVCpu, pCtxCore, X86_XCPT_GP, 0);
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
VMMDECL(int) IOMInterpretIN(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu)
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
    bool fRc = iomGetRegImmData(pCpu, &pCpu->param2, pRegFrame, &uPort, &cbSize);
    AssertMsg(fRc, ("Failed to get reg/imm port number!\n")); NOREF(fRc);

    cbSize = DISGetParamSize(pCpu, &pCpu->param1);
    Assert(cbSize > 0);
    int rc = IOMInterpretCheckPortIOAccess(pVM, pRegFrame, uPort, cbSize);
    if (rc == VINF_SUCCESS)
    {
        /*
         * Attemp to read the port.
         */
        uint32_t u32Data = UINT32_C(0xffffffff);
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
            AssertMsg(rc == VINF_IOM_HC_IOPORT_READ || RT_FAILURE(rc), ("%Rrc\n", rc));
    }
    else
        AssertMsg(rc == VINF_EM_RAW_GUEST_TRAP || rc == VINF_TRPM_XCPT_DISPATCHED || rc == VINF_TRPM_XCPT_DISPATCHED || RT_FAILURE(rc), ("%Rrc\n", rc));
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
VMMDECL(int) IOMInterpretOUT(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu)
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
    bool fRc = iomGetRegImmData(pCpu, &pCpu->param1, pRegFrame, &uPort, &cbSize);
    AssertMsg(fRc, ("Failed to get reg/imm port number!\n")); NOREF(fRc);

    int rc = IOMInterpretCheckPortIOAccess(pVM, pRegFrame, uPort, cbSize);
    if (rc == VINF_SUCCESS)
    {
        uint64_t u64Data = 0;
        fRc = iomGetRegImmData(pCpu, &pCpu->param2, pRegFrame, &u64Data, &cbSize);
        AssertMsg(fRc, ("Failed to get reg value!\n")); NOREF(fRc);

        /*
         * Attempt to write to the port.
         */
        rc = IOMIOPortWrite(pVM, uPort, u64Data, cbSize);
        AssertMsg(rc == VINF_SUCCESS || rc == VINF_IOM_HC_IOPORT_WRITE || (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST) || RT_FAILURE(rc), ("%Rrc\n", rc));
    }
    else
        AssertMsg(rc == VINF_EM_RAW_GUEST_TRAP || rc == VINF_TRPM_XCPT_DISPATCHED || rc == VINF_TRPM_XCPT_DISPATCHED || RT_FAILURE(rc), ("%Rrc\n", rc));
    return rc;
}
