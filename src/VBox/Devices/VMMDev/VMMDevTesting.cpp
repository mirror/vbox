/* $Id$ */
/** @file
 * VMMDev - Testing Extensions.
 *
 * To enable: VBoxManage setextradata vmname VBoxInternal/Devices/VMMDev/0/Config/TestingEnabled  1
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DEV_VMM
#include <VBox/VMMDev.h>
#include <VBox/vmm/vmapi.h>
#include <VBox/log.h>
#include <VBox/err.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/time.h>
#ifdef IN_RING3
# include <iprt/stream.h>
#endif

#include "VMMDevState.h"
#include "VMMDevTesting.h"


#ifndef VBOX_WITHOUT_TESTING_FEATURES

#define VMMDEV_TESTING_OUTPUT(a) \
    do \
    { \
        LogAlways(a);\
        LogRel(a);\
        RTPrintf a; \
    } while (0)

/**
 * @callback_method_impl{FNIOMMMIOWRITE}
 */
PDMBOTHCBDECL(int) vmmdevTestingMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb)
{
    switch (GCPhysAddr)
    {
        case VMMDEV_TESTING_MMIO_NOP:
            switch (cb)
            {
                case 8:
                case 4:
                case 2:
                case 1:
                    break;
                default:
                    AssertFailed();
                    return VERR_INTERNAL_ERROR_5;
            }
            return VINF_SUCCESS;

        default:
            break;
    }
    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNIOMMMIOREAD}
 */
PDMBOTHCBDECL(int) vmmdevTestingMmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    switch (GCPhysAddr)
    {
        case VMMDEV_TESTING_MMIO_NOP:
            switch (cb)
            {
                case 8:
                    *(uint64_t *)pv = VMMDEV_TESTING_NOP_RET | ((uint64_t)VMMDEV_TESTING_NOP_RET << 32);
                    break;
                case 4:
                    *(uint32_t *)pv = VMMDEV_TESTING_NOP_RET;
                    break;
                case 2:
                    *(uint16_t *)pv = (uint16_t)VMMDEV_TESTING_NOP_RET;
                    break;
                case 1:
                    *(uint8_t *)pv  = (uint8_t)VMMDEV_TESTING_NOP_RET;
                    break;
                default:
                    AssertFailed();
                    return VERR_INTERNAL_ERROR_5;
            }
            return VINF_SUCCESS;


        default:
            break;
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
static void vmmdevTestingCmdExec_ValueReg(PPDMDEVINS pDevIns, VMMDevState *pThis)
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
        PVM         pVM = PDMDevHlpGetVM(pDevIns);
        VMCPUID     idCpu = VMMGetCpuId(pVM);
        uint64_t    u64Value;
        int rc2 = DBGFR3RegNmQueryU64(pVM, idCpu, pszRegNm, &u64Value);
        if (RT_SUCCESS(rc2))
        {
            const char *pszWarn = rc2 == VINF_DBGF_TRUNCATED_REGISTER ? " truncated" : "";
#if 1 /*!RTTestValue format*/
            char szFormat[128], szValue[128];
            RTStrPrintf(szFormat, sizeof(szFormat), "%%VR{%s}", pszRegNm);
            int rc2 = DBGFR3RegPrintf(pVM, idCpu, szValue, sizeof(szValue), szFormat);
            if (RT_SUCCESS(rc2))
                VMMDEV_TESTING_OUTPUT(("testing: VALUE '%s'%*s: %16s {reg=%s}%s\n",
                                       pszValueNm,
                                       (ssize_t)cchValueNm - 12 > 48 ? 0 : 48 - ((ssize_t)cchValueNm - 12), "",
                                       szValue, pszRegNm));
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
 * @callback_method_impl{FNIOMIOPORTOUT}
 */
PDMBOTHCBDECL(int) vmmdevTestingIoWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    VMMDevState *pThis = PDMINS_2_DATA(pDevIns, VMMDevState *);

    switch (Port)
    {
        /*
         * The NOP I/O ports are used for performance measurements.
         */
        case VMMDEV_TESTING_IOPORT_NOP:
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

        /* The timestamp I/O ports are read-only. */
        case VMMDEV_TESTING_IOPORT_TS_LOW:
        case VMMDEV_TESTING_IOPORT_TS_HIGH:
            break;

        /*
         * The command port (DWORD write only).
         */
        case VMMDEV_TESTING_IOPORT_CMD:
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
        case VMMDEV_TESTING_IOPORT_DATA:
        {
            uint32_t uCmd = pThis->u32TestingCmd;
            uint32_t off  = pThis->offTestingData;
            switch (uCmd)
            {
                case VMMDEV_TESTING_CMD_INIT:
                case VMMDEV_TESTING_CMD_SUB_NEW:
                case VMMDEV_TESTING_CMD_FAILED:
                case VMMDEV_TESTING_CMD_SKIPPED:
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
                            switch (uCmd)
                            {
                                case VMMDEV_TESTING_CMD_INIT:
                                    VMMDEV_TESTING_OUTPUT(("testing: INIT '%.*s'\n",
                                                           sizeof(pThis->TestingData.String.sz) - 1, pThis->TestingData.String.sz));
                                    break;
                                case VMMDEV_TESTING_CMD_SUB_NEW:
                                    VMMDEV_TESTING_OUTPUT(("testing: SUB_NEW  '%.*s'\n",
                                                           sizeof(pThis->TestingData.String.sz) - 1, pThis->TestingData.String.sz));
                                    break;
                                case VMMDEV_TESTING_CMD_FAILED:
                                    VMMDEV_TESTING_OUTPUT(("testing: FAILED '%.*s'\n",
                                                           sizeof(pThis->TestingData.String.sz) - 1, pThis->TestingData.String.sz));
                                    break;
                                case VMMDEV_TESTING_CMD_SKIPPED:
                                    VMMDEV_TESTING_OUTPUT(("testing: SKIPPED '%.*s'\n",
                                                           sizeof(pThis->TestingData.String.sz) - 1, pThis->TestingData.String.sz));
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
                    if (   off == 0
                        && cb  == 4)
                    {
#ifdef IN_RING3
                        pThis->TestingData.Error.c = u32;
                        if (uCmd == VMMDEV_TESTING_CMD_TERM)
                            VMMDEV_TESTING_OUTPUT(("testing: TERM - %u errors\n", u32));
                        else
                            VMMDEV_TESTING_OUTPUT(("testing: SUB_DONE - %u errors\n", u32));
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
                    if (   off >= 12
                        && cb  == 1
                        && off < sizeof(pThis->TestingData.Value.szName) - 1 - 12)
                    {
                        if (u32)
                        {
                            pThis->TestingData.Value.szName[off - 12] = u32;
                            pThis->offTestingData = off + 1;
                        }
                        else
                        {
#ifdef IN_RING3
                            VMMDEV_TESTING_OUTPUT(("testing: VALUE '%.*s'%*s: %'9llu (%#llx) [%u]\n",
                                                   sizeof(pThis->TestingData.Value.szName) - 1, pThis->TestingData.Value.szName,
                                                   off - 12 > 48 ? 0 : 48 - (off - 12), "",
                                                   pThis->TestingData.Value.u64Value.u, pThis->TestingData.Value.u64Value.u,
                                                   pThis->TestingData.Value.u32Unit));
#else
                            return VINF_IOM_R3_IOPORT_WRITE;
#endif
                        }
                        return VINF_SUCCESS;

#ifdef IN_RING3
                        pThis->TestingData.Error.c = u32;
                        if (uCmd == VMMDEV_TESTING_CMD_TERM)
                            VMMDEV_TESTING_OUTPUT(("testing: TERM - %u errors\n", u32));
                        else
                            VMMDEV_TESTING_OUTPUT(("testing: SUB_DONE - %u errors\n", u32));
                        return VINF_SUCCESS;
#else
                        return VINF_IOM_R3_IOPORT_WRITE;
#endif
                    }
                    break;


                /*
                 * RTTestValue with the return from DBGFR3RegNmQuery.
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
 * @callback_method_impl{FNIOMIOPORTIN}
 */
PDMBOTHCBDECL(int) vmmdevTestingIoRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    VMMDevState *pThis = PDMINS_2_DATA(pDevIns, VMMDevState *);

    switch (Port)
    {
        /*
         * The NOP I/O ports are used for performance measurements.
         */
        case VMMDEV_TESTING_IOPORT_NOP:
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

        /*
         * The timestamp I/O ports are obviously used for getting a good fix
         * on the current time (as seen by the host?).
         *
         * The high word is latched when reading the low, so reading low + high
         * gives you a 64-bit timestamp value.
         */
        case VMMDEV_TESTING_IOPORT_TS_LOW:
            if (cb == 4)
            {
                uint64_t NowTS = RTTimeNanoTS();
                *pu32 = (uint32_t)NowTS;
                pThis->u32TestingHighTimestamp = (uint32_t)(NowTS >> 32);
                return VINF_SUCCESS;
            }
            break;

        case VMMDEV_TESTING_IOPORT_TS_HIGH:
            if (cb == 4)
            {
                *pu32 = pThis->u32TestingHighTimestamp;
                return VINF_SUCCESS;
            }
            break;

        /*
         * The command and data registers are write-only.
         */
        case VMMDEV_TESTING_IOPORT_CMD:
        case VMMDEV_TESTING_IOPORT_DATA:
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
int vmmdevTestingInitialize(PPDMDEVINS pDevIns)
{
    VMMDevState *pThis = PDMINS_2_DATA(pDevIns, VMMDevState *);
    if (!pThis->fTestingEnabled)
        return VINF_SUCCESS;

    /*
     * Register a chunk of MMIO memory that we'll use for various
     * tests interfaces.
     */
    int rc = PDMDevHlpMMIORegister(pDevIns, VMMDEV_TESTING_MMIO_BASE, VMMDEV_TESTING_MMIO_SIZE, NULL /*pvUser*/,
                                   IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU,
                                   vmmdevTestingMmioWrite, vmmdevTestingMmioRead, "VMMDev Testing");
    AssertRCReturn(rc, rc);
    if (pThis->fRZEnabled)
    {
        rc = PDMDevHlpMMIORegisterR0(pDevIns, VMMDEV_TESTING_MMIO_BASE, VMMDEV_TESTING_MMIO_SIZE, NIL_RTR0PTR /*pvUser*/,
                                     "vmmdevTestingMmioWrite", "vmmdevTestingMmioRead");
        AssertRCReturn(rc, rc);
        rc = PDMDevHlpMMIORegisterRC(pDevIns, VMMDEV_TESTING_MMIO_BASE, VMMDEV_TESTING_MMIO_SIZE, NIL_RTRCPTR /*pvUser*/,
                                     "vmmdevTestingMmioWrite", "vmmdevTestingMmioRead");
        AssertRCReturn(rc, rc);
    }


    /*
     * Register the I/O ports used for testing.
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, VMMDEV_TESTING_IOPORT_BASE, VMMDEV_TESTING_IOPORT_COUNT, NULL,
                                 vmmdevTestingIoWrite,
                                 vmmdevTestingIoRead,
                                 NULL /*pfnOutStr*/,
                                 NULL /*pfnInStr*/,
                                 "VMMDev Testing");
    AssertRCReturn(rc, rc);
    if (pThis->fRZEnabled)
    {
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, VMMDEV_TESTING_IOPORT_BASE, VMMDEV_TESTING_IOPORT_COUNT, NIL_RTR0PTR /*pvUser*/,
                                       "vmmdevTestingIoWrite",
                                       "vmmdevTestingIoRead",
                                       NULL /*pszOutStr*/,
                                       NULL /*pszInStr*/,
                                       "VMMDev Testing");
        AssertRCReturn(rc, rc);
        rc = PDMDevHlpIOPortRegisterRC(pDevIns, VMMDEV_TESTING_IOPORT_BASE, VMMDEV_TESTING_IOPORT_COUNT, NIL_RTRCPTR /*pvUser*/,
                                       "vmmdevTestingIoWrite",
                                       "vmmdevTestingIoRead",
                                       NULL /*pszOutStr*/,
                                       NULL /*pszInStr*/,
                                       "VMMDev Testing");
        AssertRCReturn(rc, rc);
    }

    return VINF_SUCCESS;
}

#endif /* IN_RING3 */
#endif /* !VBOX_WITHOUT_TESTING_FEATURES */

