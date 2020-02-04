/* $Id$ */
/** @file
 * VMMDev - Testing Extensions.
 *
 * To enable: VBoxManage setextradata vmname VBoxInternal/Devices/VMMDev/0/Config/TestingEnabled  1
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DEV_VMM
#include <VBox/VMMDev.h>
#include <VBox/vmm/vmapi.h>
#include <VBox/log.h>
#include <VBox/err.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/test.h>

#include "VMMDevState.h"
#include "VMMDevTesting.h"


#ifndef VBOX_WITHOUT_TESTING_FEATURES

#define VMMDEV_TESTING_OUTPUT(a) \
    do \
    { \
        LogAlways(a);\
        LogRel(a);\
    } while (0)

/**
 * @callback_method_impl{FNIOMMMIONEWWRITE}
 */
static DECLCALLBACK(VBOXSTRICTRC) vmmdevTestingMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb)
{
    RT_NOREF_PV(pvUser);

    switch (off)
    {
        case VMMDEV_TESTING_MMIO_OFF_NOP_R3:
#ifndef IN_RING3
            return VINF_IOM_R3_MMIO_WRITE;
#endif
        case VMMDEV_TESTING_MMIO_OFF_NOP:
            return VINF_SUCCESS;

        default:
        {
            /*
             * Readback register (64 bytes wide).
             */
            if (   (   off      >= VMMDEV_TESTING_MMIO_OFF_READBACK
                    && off + cb <= VMMDEV_TESTING_MMIO_OFF_READBACK + VMMDEV_TESTING_READBACK_SIZE)
#ifndef IN_RING3
                || (   off      >= VMMDEV_TESTING_MMIO_OFF_READBACK_R3
                    && off + cb <= VMMDEV_TESTING_MMIO_OFF_READBACK_R3 + VMMDEV_TESTING_READBACK_SIZE)
#endif
                    )
            {
                PVMMDEV pThis = PDMDEVINS_2_DATA(pDevIns, PVMMDEV);
                off &= VMMDEV_TESTING_READBACK_SIZE - 1;
                switch (cb)
                {
                    case 8: *(uint64_t *)&pThis->TestingData.abReadBack[off] = *(uint64_t const *)pv; break;
                    case 4: *(uint32_t *)&pThis->TestingData.abReadBack[off] = *(uint32_t const *)pv; break;
                    case 2: *(uint16_t *)&pThis->TestingData.abReadBack[off] = *(uint16_t const *)pv; break;
                    case 1: *(uint8_t  *)&pThis->TestingData.abReadBack[off] = *(uint8_t  const *)pv; break;
                    default: memcpy(&pThis->TestingData.abReadBack[off], pv, cb); break;
                }
                return VINF_SUCCESS;
            }
#ifndef IN_RING3
            if (   off      >= VMMDEV_TESTING_MMIO_OFF_READBACK_R3
                && off + cb <= VMMDEV_TESTING_MMIO_OFF_READBACK_R3 + 64)
                return VINF_IOM_R3_MMIO_WRITE;
#endif

            break;
        }

        /*
         * Odd NOP accesses.
         */
        case VMMDEV_TESTING_MMIO_OFF_NOP_R3 + 1:
        case VMMDEV_TESTING_MMIO_OFF_NOP_R3 + 2:
        case VMMDEV_TESTING_MMIO_OFF_NOP_R3 + 3:
        case VMMDEV_TESTING_MMIO_OFF_NOP_R3 + 4:
        case VMMDEV_TESTING_MMIO_OFF_NOP_R3 + 5:
        case VMMDEV_TESTING_MMIO_OFF_NOP_R3 + 6:
        case VMMDEV_TESTING_MMIO_OFF_NOP_R3 + 7:
#ifndef IN_RING3
            return VINF_IOM_R3_MMIO_WRITE;
#endif
        case VMMDEV_TESTING_MMIO_OFF_NOP    + 1:
        case VMMDEV_TESTING_MMIO_OFF_NOP    + 2:
        case VMMDEV_TESTING_MMIO_OFF_NOP    + 3:
        case VMMDEV_TESTING_MMIO_OFF_NOP    + 4:
        case VMMDEV_TESTING_MMIO_OFF_NOP    + 5:
        case VMMDEV_TESTING_MMIO_OFF_NOP    + 6:
        case VMMDEV_TESTING_MMIO_OFF_NOP    + 7:
            return VINF_SUCCESS;
    }
    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNIOMMMIONEWREAD}
 */
static DECLCALLBACK(VBOXSTRICTRC) vmmdevTestingMmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void *pv, unsigned cb)
{
    RT_NOREF_PV(pvUser);

    switch (off)
    {
        case VMMDEV_TESTING_MMIO_OFF_NOP_R3:
#ifndef IN_RING3
            return VINF_IOM_R3_MMIO_READ;
#endif
            /* fall thru. */
        case VMMDEV_TESTING_MMIO_OFF_NOP:
            switch (cb)
            {
                case 8:
                    *(uint64_t *)pv = VMMDEV_TESTING_NOP_RET | ((uint64_t)VMMDEV_TESTING_NOP_RET << 32);
                    break;
                case 4:
                    *(uint32_t *)pv = VMMDEV_TESTING_NOP_RET;
                    break;
                case 2:
                    *(uint16_t *)pv = RT_LO_U16(VMMDEV_TESTING_NOP_RET);
                    break;
                case 1:
                    *(uint8_t *)pv  = (uint8_t)(VMMDEV_TESTING_NOP_RET & UINT8_MAX);
                    break;
                default:
                    AssertFailed();
                    return VERR_INTERNAL_ERROR_5;
            }
            return VINF_SUCCESS;


        default:
        {
            /*
             * Readback register (64 bytes wide).
             */
            if (   (   off      >= VMMDEV_TESTING_MMIO_OFF_READBACK
                    && off + cb <= VMMDEV_TESTING_MMIO_OFF_READBACK + 64)
#ifndef IN_RING3
                || (   off      >= VMMDEV_TESTING_MMIO_OFF_READBACK_R3
                    && off + cb <= VMMDEV_TESTING_MMIO_OFF_READBACK_R3 + 64)
#endif
                    )
            {
                PVMMDEV pThis = PDMDEVINS_2_DATA(pDevIns, PVMMDEV);
                off &= 0x3f;
                switch (cb)
                {
                    case 8: *(uint64_t *)pv = *(uint64_t const *)&pThis->TestingData.abReadBack[off]; break;
                    case 4: *(uint32_t *)pv = *(uint32_t const *)&pThis->TestingData.abReadBack[off]; break;
                    case 2: *(uint16_t *)pv = *(uint16_t const *)&pThis->TestingData.abReadBack[off]; break;
                    case 1: *(uint8_t  *)pv = *(uint8_t  const *)&pThis->TestingData.abReadBack[off]; break;
                    default: memcpy(pv, &pThis->TestingData.abReadBack[off], cb); break;
                }
                return VINF_SUCCESS;
            }
#ifndef IN_RING3
            if (   off      >= VMMDEV_TESTING_MMIO_OFF_READBACK_R3
                && off + cb <= VMMDEV_TESTING_MMIO_OFF_READBACK_R3 + 64)
                return VINF_IOM_R3_MMIO_READ;
#endif
            break;
        }

        /*
         * Odd NOP accesses (for 16-bit code mainly).
         */
        case VMMDEV_TESTING_MMIO_OFF_NOP_R3 + 1:
        case VMMDEV_TESTING_MMIO_OFF_NOP_R3 + 2:
        case VMMDEV_TESTING_MMIO_OFF_NOP_R3 + 3:
        case VMMDEV_TESTING_MMIO_OFF_NOP_R3 + 4:
        case VMMDEV_TESTING_MMIO_OFF_NOP_R3 + 5:
        case VMMDEV_TESTING_MMIO_OFF_NOP_R3 + 6:
        case VMMDEV_TESTING_MMIO_OFF_NOP_R3 + 7:
#ifndef IN_RING3
            return VINF_IOM_R3_MMIO_READ;
#endif
        case VMMDEV_TESTING_MMIO_OFF_NOP    + 1:
        case VMMDEV_TESTING_MMIO_OFF_NOP    + 2:
        case VMMDEV_TESTING_MMIO_OFF_NOP    + 3:
        case VMMDEV_TESTING_MMIO_OFF_NOP    + 4:
        case VMMDEV_TESTING_MMIO_OFF_NOP    + 5:
        case VMMDEV_TESTING_MMIO_OFF_NOP    + 6:
        case VMMDEV_TESTING_MMIO_OFF_NOP    + 7:
        {
            static uint8_t const s_abNopValue[8] =
            {
                 VMMDEV_TESTING_NOP_RET        & 0xff,
                (VMMDEV_TESTING_NOP_RET >>  8) & 0xff,
                (VMMDEV_TESTING_NOP_RET >> 16) & 0xff,
                (VMMDEV_TESTING_NOP_RET >> 24) & 0xff,
                VMMDEV_TESTING_NOP_RET        & 0xff,
                (VMMDEV_TESTING_NOP_RET >>  8) & 0xff,
                (VMMDEV_TESTING_NOP_RET >> 16) & 0xff,
                (VMMDEV_TESTING_NOP_RET >> 24) & 0xff,
            };

            memset(pv, 0xff, cb);
            memcpy(pv, &s_abNopValue[off & 7], RT_MIN(8 - (off & 7), cb));
            return VINF_SUCCESS;
        }
    }

    return VINF_IOM_MMIO_UNUSED_FF;
}

#ifdef IN_RING3

/**
 * Executes the VMMDEV_TESTING_CMD_VALUE_REG command when the data is ready.
 *
 * @param   pDevIns             The PDM device instance.
 * @param   pThis               The instance VMMDev data.
 */
static void vmmdevTestingCmdExec_ValueReg(PPDMDEVINS pDevIns, PVMMDEV pThis)
{
    char *pszRegNm = strchr(pThis->TestingData.String.sz, ':');
    if (pszRegNm)
    {
        *pszRegNm++ = '\0';
        pszRegNm = RTStrStrip(pszRegNm);
    }
    char        *pszValueNm = RTStrStrip(pThis->TestingData.String.sz);
    size_t const cchValueNm = strlen(pszValueNm);
    if (cchValueNm && pszRegNm && *pszRegNm)
    {
        PUVM        pUVM  = PDMDevHlpGetUVM(pDevIns);
        PVM         pVM   = PDMDevHlpGetVM(pDevIns);
        VMCPUID     idCpu = VMMGetCpuId(pVM);
        uint64_t    u64Value;
        int rc2 = DBGFR3RegNmQueryU64(pUVM, idCpu, pszRegNm, &u64Value);
        if (RT_SUCCESS(rc2))
        {
            const char *pszWarn = rc2 == VINF_DBGF_TRUNCATED_REGISTER ? " truncated" : "";
#if 1 /*!RTTestValue format*/
            char szFormat[128], szValue[128];
            RTStrPrintf(szFormat, sizeof(szFormat), "%%VR{%s}", pszRegNm);
            rc2 = DBGFR3RegPrintf(pUVM, idCpu, szValue, sizeof(szValue), szFormat);
            if (RT_SUCCESS(rc2))
                VMMDEV_TESTING_OUTPUT(("testing: VALUE '%s'%*s: %16s {reg=%s}%s\n",
                                       pszValueNm,
                                       (ssize_t)cchValueNm - 12 > 48 ? 0 : 48 - ((ssize_t)cchValueNm - 12), "",
                                       szValue, pszRegNm, pszWarn));
            else
#endif
                VMMDEV_TESTING_OUTPUT(("testing: VALUE '%s'%*s: %'9llu (%#llx) [0] {reg=%s}%s\n",
                                       pszValueNm,
                                       (ssize_t)cchValueNm - 12 > 48 ? 0 : 48 - ((ssize_t)cchValueNm - 12), "",
                                       u64Value, u64Value, pszRegNm, pszWarn));
        }
        else
            VMMDEV_TESTING_OUTPUT(("testing: error querying register '%s' for value '%s': %Rrc\n",
                                   pszRegNm, pszValueNm, rc2));
    }
    else
        VMMDEV_TESTING_OUTPUT(("testing: malformed register value '%s'/'%s'\n", pszValueNm, pszRegNm));
}

#endif /* IN_RING3 */

/**
 * @callback_method_impl{FNIOMIOPORTNEWOUT}
 */
static DECLCALLBACK(VBOXSTRICTRC)
vmmdevTestingIoWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT offPort, uint32_t u32, unsigned cb)
{
    PVMMDEV pThis = PDMDEVINS_2_DATA(pDevIns, PVMMDEV);
#ifdef IN_RING3
    PVMMDEVCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVMMDEVCC);
#endif
    RT_NOREF_PV(pvUser);

    switch (offPort)
    {
        /*
         * The NOP I/O ports are used for performance measurements.
         */
        case VMMDEV_TESTING_IOPORT_NOP - VMMDEV_TESTING_IOPORT_BASE:
            switch (cb)
            {
                case 4:
                case 2:
                case 1:
                    break;
                default:
                    AssertFailed();
                    return VERR_INTERNAL_ERROR_2;
            }
            return VINF_SUCCESS;

        case VMMDEV_TESTING_IOPORT_NOP_R3 - VMMDEV_TESTING_IOPORT_BASE:
            switch (cb)
            {
                case 4:
                case 2:
                case 1:
#ifndef IN_RING3
                    return VINF_IOM_R3_IOPORT_WRITE;
#else
                    return VINF_SUCCESS;
#endif
                default:
                    AssertFailed();
                    return VERR_INTERNAL_ERROR_2;
            }

        /* The timestamp I/O ports are read-only. */
        case VMMDEV_TESTING_IOPORT_TS_LOW   - VMMDEV_TESTING_IOPORT_BASE:
        case VMMDEV_TESTING_IOPORT_TS_HIGH  - VMMDEV_TESTING_IOPORT_BASE:
            break;

        /*
         * The command port (DWORD and WORD write only).
         * (We have to allow WORD writes for 286, 186 and 8086 execution modes.)
         */
        case VMMDEV_TESTING_IOPORT_CMD - VMMDEV_TESTING_IOPORT_BASE:
            if (cb == 2)
            {
                u32 |= VMMDEV_TESTING_CMD_MAGIC_HI_WORD;
                cb = 4;
            }
            if (cb == 4)
            {
                pThis->u32TestingCmd  = u32;
                pThis->offTestingData = 0;
                RT_ZERO(pThis->TestingData);
                return VINF_SUCCESS;
            }
            break;

        /*
         * The data port.  Used of providing data for a command.
         */
        case VMMDEV_TESTING_IOPORT_DATA - VMMDEV_TESTING_IOPORT_BASE:
        {
            uint32_t uCmd = pThis->u32TestingCmd;
            uint32_t off  = pThis->offTestingData;
            switch (uCmd)
            {
                case VMMDEV_TESTING_CMD_INIT:
                case VMMDEV_TESTING_CMD_SUB_NEW:
                case VMMDEV_TESTING_CMD_FAILED:
                case VMMDEV_TESTING_CMD_SKIPPED:
                case VMMDEV_TESTING_CMD_PRINT:
                    if (   off < sizeof(pThis->TestingData.String.sz) - 1
                        && cb == 1)
                    {
                        if (u32)
                        {
                            pThis->TestingData.String.sz[off] = u32;
                            pThis->offTestingData = off + 1;
                        }
                        else
                        {
#ifdef IN_RING3
                            pThis->TestingData.String.sz[off] = '\0';
                            switch (uCmd)
                            {
                                case VMMDEV_TESTING_CMD_INIT:
                                    VMMDEV_TESTING_OUTPUT(("testing: INIT '%s'\n", pThis->TestingData.String.sz));
                                    if (pThisCC->hTestingTest != NIL_RTTEST)
                                    {
                                        RTTestChangeName(pThisCC->hTestingTest, pThis->TestingData.String.sz);
                                        RTTestBanner(pThisCC->hTestingTest);
                                    }
                                    break;
                                case VMMDEV_TESTING_CMD_SUB_NEW:
                                    VMMDEV_TESTING_OUTPUT(("testing: SUB_NEW  '%s'\n", pThis->TestingData.String.sz));
                                    if (pThisCC->hTestingTest != NIL_RTTEST)
                                        RTTestSub(pThisCC->hTestingTest, pThis->TestingData.String.sz);
                                    break;
                                case VMMDEV_TESTING_CMD_FAILED:
                                    if (pThisCC->hTestingTest != NIL_RTTEST)
                                        RTTestFailed(pThisCC->hTestingTest, "%s", pThis->TestingData.String.sz);
                                    VMMDEV_TESTING_OUTPUT(("testing: FAILED '%s'\n", pThis->TestingData.String.sz));
                                    break;
                                case VMMDEV_TESTING_CMD_SKIPPED:
                                    if (pThisCC->hTestingTest != NIL_RTTEST)
                                    {
                                        if (off)
                                            RTTestSkipped(pThisCC->hTestingTest, "%s", pThis->TestingData.String.sz);
                                        else
                                            RTTestSkipped(pThisCC->hTestingTest, NULL);
                                    }
                                    VMMDEV_TESTING_OUTPUT(("testing: SKIPPED '%s'\n", pThis->TestingData.String.sz));
                                    break;
                                case VMMDEV_TESTING_CMD_PRINT:
                                    if (pThisCC->hTestingTest != NIL_RTTEST && off)
                                        RTTestPrintf(pThisCC->hTestingTest, RTTESTLVL_ALWAYS, "%s", pThis->TestingData.String.sz);
                                    VMMDEV_TESTING_OUTPUT(("testing: '%s'\n", pThis->TestingData.String.sz));
                                    break;
                            }
#else
                            return VINF_IOM_R3_IOPORT_WRITE;
#endif
                        }
                        return VINF_SUCCESS;
                    }
                    break;

                case VMMDEV_TESTING_CMD_TERM:
                case VMMDEV_TESTING_CMD_SUB_DONE:
                    if (cb == 2)
                    {
                        if (off == 0)
                        {
                            pThis->TestingData.Error.c = u32;
                            pThis->offTestingData = 2;
                            break;
                        }
                        if (off == 2)
                        {
                            u32 <<= 16;
                            u32  |= pThis->TestingData.Error.c & UINT16_MAX;
                            cb    = 4;
                            off   = 0;
                        }
                        else
                            break;
                    }

                    if (   off == 0
                        && cb  == 4)
                    {
#ifdef IN_RING3
                        pThis->TestingData.Error.c = u32;
                        if (uCmd == VMMDEV_TESTING_CMD_TERM)
                        {
                            if (pThisCC->hTestingTest != NIL_RTTEST)
                            {
                                while (RTTestErrorCount(pThisCC->hTestingTest) < u32)
                                    RTTestErrorInc(pThisCC->hTestingTest); /* A bit stupid, but does the trick. */
                                RTTestSubDone(pThisCC->hTestingTest);
                                RTTestSummaryAndDestroy(pThisCC->hTestingTest);
                                pThisCC->hTestingTest = NIL_RTTEST;
                            }
                            VMMDEV_TESTING_OUTPUT(("testing: TERM - %u errors\n", u32));
                        }
                        else
                        {
                            if (pThisCC->hTestingTest != NIL_RTTEST)
                            {
                                while (RTTestSubErrorCount(pThisCC->hTestingTest) < u32)
                                    RTTestErrorInc(pThisCC->hTestingTest); /* A bit stupid, but does the trick. */
                                RTTestSubDone(pThisCC->hTestingTest);
                            }
                            VMMDEV_TESTING_OUTPUT(("testing: SUB_DONE - %u errors\n", u32));
                        }
                        return VINF_SUCCESS;
#else
                        return VINF_IOM_R3_IOPORT_WRITE;
#endif
                    }
                    break;

                case VMMDEV_TESTING_CMD_VALUE:
                    if (cb == 4)
                    {
                        if (off == 0)
                            pThis->TestingData.Value.u64Value.s.Lo = u32;
                        else if (off == 4)
                            pThis->TestingData.Value.u64Value.s.Hi = u32;
                        else if (off == 8)
                            pThis->TestingData.Value.u32Unit = u32;
                        else
                            break;
                        pThis->offTestingData = off + 4;
                        return VINF_SUCCESS;
                    }
                    if (cb == 2)
                    {
                        if (off == 0)
                            pThis->TestingData.Value.u64Value.Words.w0 = (uint16_t)u32;
                        else if (off == 2)
                            pThis->TestingData.Value.u64Value.Words.w1 = (uint16_t)u32;
                        else if (off == 4)
                            pThis->TestingData.Value.u64Value.Words.w2 = (uint16_t)u32;
                        else if (off == 6)
                            pThis->TestingData.Value.u64Value.Words.w3 = (uint16_t)u32;
                        else if (off == 8)
                            pThis->TestingData.Value.u32Unit = (uint16_t)u32;
                        else if (off == 10)
                            pThis->TestingData.Value.u32Unit = u32 << 16;
                        else
                            break;
                        pThis->offTestingData = off + 2;
                        return VINF_SUCCESS;
                    }

                    if (   off >= 12
                        && cb  == 1
                        && off - 12 < sizeof(pThis->TestingData.Value.szName) - 1)
                    {
                        if (u32)
                        {
                            pThis->TestingData.Value.szName[off - 12] = u32;
                            pThis->offTestingData = off + 1;
                        }
                        else
                        {
#ifdef IN_RING3
                            pThis->TestingData.Value.szName[off - 12] = '\0';

                            RTTESTUNIT enmUnit = (RTTESTUNIT)pThis->TestingData.Value.u32Unit;
                            if (enmUnit <= RTTESTUNIT_INVALID || enmUnit >= RTTESTUNIT_END)
                            {
                                VMMDEV_TESTING_OUTPUT(("Invalid log value unit %#x\n", pThis->TestingData.Value.u32Unit));
                                enmUnit = RTTESTUNIT_NONE;
                            }
                            if (pThisCC->hTestingTest != NIL_RTTEST)
                                RTTestValue(pThisCC->hTestingTest, pThis->TestingData.Value.szName,
                                            pThis->TestingData.Value.u64Value.u, enmUnit);

                            VMMDEV_TESTING_OUTPUT(("testing: VALUE '%s'%*s: %'9llu (%#llx) [%u]\n",
                                                   pThis->TestingData.Value.szName,
                                                   off - 12 > 48 ? 0 : 48 - (off - 12), "",
                                                   pThis->TestingData.Value.u64Value.u, pThis->TestingData.Value.u64Value.u,
                                                   pThis->TestingData.Value.u32Unit));
#else
                            return VINF_IOM_R3_IOPORT_WRITE;
#endif
                        }
                        return VINF_SUCCESS;
                    }
                    break;


                /*
                 * RTTestValue with the output from DBGFR3RegNmQuery.
                 */
                case VMMDEV_TESTING_CMD_VALUE_REG:
                {
                    if (   off < sizeof(pThis->TestingData.String.sz) - 1
                        && cb == 1)
                    {
                        pThis->TestingData.String.sz[off] = u32;
                        if (u32)
                            pThis->offTestingData = off + 1;
                        else
#ifdef IN_RING3
                            vmmdevTestingCmdExec_ValueReg(pDevIns, pThis);
#else
                            return VINF_IOM_R3_IOPORT_WRITE;
#endif
                        return VINF_SUCCESS;
                    }
                    break;
                }

                default:
                    break;
            }
            Log(("VMMDEV_TESTING_IOPORT_CMD: bad access; cmd=%#x off=%#x cb=%#x u32=%#x\n", uCmd, off, cb, u32));
            return VINF_SUCCESS;
        }

        default:
            break;
    }

    return VERR_IOM_IOPORT_UNUSED;
}


/**
 * @callback_method_impl{FNIOMIOPORTNEWIN}
 */
static DECLCALLBACK(VBOXSTRICTRC)
vmmdevTestingIoRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT offPort, uint32_t *pu32, unsigned cb)
{
    PVMMDEV pThis = PDMDEVINS_2_DATA(pDevIns, PVMMDEV);
    RT_NOREF_PV(pvUser);

    switch (offPort)
    {
        /*
         * The NOP I/O ports are used for performance measurements.
         */
        case VMMDEV_TESTING_IOPORT_NOP - VMMDEV_TESTING_IOPORT_BASE:
            switch (cb)
            {
                case 4:
                case 2:
                case 1:
                    break;
                default:
                    AssertFailed();
                    return VERR_INTERNAL_ERROR_2;
            }
            *pu32 = VMMDEV_TESTING_NOP_RET;
            return VINF_SUCCESS;

        case VMMDEV_TESTING_IOPORT_NOP_R3 - VMMDEV_TESTING_IOPORT_BASE:
            switch (cb)
            {
                case 4:
                case 2:
                case 1:
#ifndef IN_RING3
                    return VINF_IOM_R3_IOPORT_READ;
#else
                    *pu32 = VMMDEV_TESTING_NOP_RET;
                    return VINF_SUCCESS;
#endif
                default:
                    AssertFailed();
                    return VERR_INTERNAL_ERROR_2;
            }

        /*
         * The timestamp I/O ports are obviously used for getting a good fix
         * on the current time (as seen by the host?).
         *
         * The high word is latched when reading the low, so reading low + high
         * gives you a 64-bit timestamp value.
         */
        case VMMDEV_TESTING_IOPORT_TS_LOW - VMMDEV_TESTING_IOPORT_BASE:
            if (cb == 4)
            {
                uint64_t NowTS = RTTimeNanoTS();
                *pu32 = (uint32_t)NowTS;
                pThis->u32TestingHighTimestamp = (uint32_t)(NowTS >> 32);
                return VINF_SUCCESS;
            }
            break;

        case VMMDEV_TESTING_IOPORT_TS_HIGH - VMMDEV_TESTING_IOPORT_BASE:
            if (cb == 4)
            {
                *pu32 = pThis->u32TestingHighTimestamp;
                return VINF_SUCCESS;
            }
            break;

        /*
         * The command and data registers are write-only.
         */
        case VMMDEV_TESTING_IOPORT_CMD  - VMMDEV_TESTING_IOPORT_BASE:
        case VMMDEV_TESTING_IOPORT_DATA - VMMDEV_TESTING_IOPORT_BASE:
            break;

        default:
            break;
    }

    return VERR_IOM_IOPORT_UNUSED;
}


#ifdef IN_RING3

/**
 * Initializes the testing part of the VMMDev if enabled.
 *
 * @returns VBox status code.
 * @param   pDevIns             The VMMDev device instance.
 */
void vmmdevR3TestingTerminate(PPDMDEVINS pDevIns)
{
    PVMMDEV   pThis   = PDMDEVINS_2_DATA(pDevIns, PVMMDEV);
    PVMMDEVCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVMMDEVCC);
    if (!pThis->fTestingEnabled)
        return;

    if (pThisCC->hTestingTest != NIL_RTTEST)
    {
        RTTestFailed(pThisCC->hTestingTest, "Still open at vmmdev destruction.");
        RTTestSummaryAndDestroy(pThisCC->hTestingTest);
        pThisCC->hTestingTest = NIL_RTTEST;
    }
}


/**
 * Initializes the testing part of the VMMDev if enabled.
 *
 * @returns VBox status code.
 * @param   pDevIns             The VMMDev device instance.
 */
int vmmdevR3TestingInitialize(PPDMDEVINS pDevIns)
{
    PVMMDEV     pThis   = PDMDEVINS_2_DATA(pDevIns, PVMMDEV);
    PVMMDEVCC   pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVMMDEVCC);
    int         rc;

    if (!pThis->fTestingEnabled)
        return VINF_SUCCESS;

    if (pThis->fTestingMMIO)
    {
        /*
         * Register a chunk of MMIO memory that we'll use for various
         * tests interfaces.  Optional, needs to be explicitly enabled.
         */
        rc = PDMDevHlpMmioCreateAndMap(pDevIns, VMMDEV_TESTING_MMIO_BASE, VMMDEV_TESTING_MMIO_SIZE,
                                       vmmdevTestingMmioWrite, vmmdevTestingMmioRead,
                                       IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU,
                                       "VMMDev Testing", &pThis->hMmioTesting);
        AssertRCReturn(rc, rc);
    }


    /*
     * Register the I/O ports used for testing.
     */
    rc = PDMDevHlpIoPortCreateAndMap(pDevIns, VMMDEV_TESTING_IOPORT_BASE, VMMDEV_TESTING_IOPORT_COUNT,
                                     vmmdevTestingIoWrite, vmmdevTestingIoRead, "VMMDev Testing", NULL /*paExtDescs*/,
                                     &pThis->hIoPortTesting);
    AssertRCReturn(rc, rc);

    /*
     * Open the XML output file(/pipe/whatever) if specfied.
     */
    rc = RTTestCreateEx("VMMDevTesting", RTTEST_C_USE_ENV | RTTEST_C_NO_TLS | RTTEST_C_XML_DELAY_TOP_TEST,
                        RTTESTLVL_INVALID, -1 /*iNativeTestPipe*/, pThisCC->pszTestingXmlOutput, &pThisCC->hTestingTest);
    if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS, "Error creating testing instance");

    return VINF_SUCCESS;
}

#else  /* !IN_RING3 */

/**
 * Does the ring-0/raw-mode initialization of the testing part if enabled.
 *
 * @returns VBox status code.
 * @param   pDevIns             The VMMDev device instance.
 */
int vmmdevRZTestingInitialize(PPDMDEVINS pDevIns)
{
    PVMMDEV     pThis   = PDMDEVINS_2_DATA(pDevIns, PVMMDEV);
    int         rc;

    if (!pThis->fTestingEnabled)
        return VINF_SUCCESS;

    if (pThis->fTestingMMIO)
    {
        rc = PDMDevHlpMmioSetUpContext(pDevIns, pThis->hMmioTesting, vmmdevTestingMmioWrite, vmmdevTestingMmioRead, NULL);
        AssertRCReturn(rc, rc);
    }

    rc = PDMDevHlpIoPortSetUpContext(pDevIns, pThis->hIoPortTesting, vmmdevTestingIoWrite, vmmdevTestingIoRead, NULL);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}

#endif /* !IN_RING3 */
#endif /* !VBOX_WITHOUT_TESTING_FEATURES */

