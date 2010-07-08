/* $Id$ */
/** @file
 * VMMDev - Testing Extensions.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
#include <VBox/log.h>
#include <VBox/err.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/time.h>

#include "VMMDevState.h"
#include "VMMDevTesting.h"


#ifndef VBOX_WITHOUT_TESTING_FEATURES

/**
 * @callback_method_impl{FNIOMMMIOWRITE}
 */
PDMBOTHCBDECL(int) vmmdevTestingMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
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


/**
 * @callback_method_impl{FNIOMIOPORTOUT}
 */
PDMBOTHCBDECL(int) vmmdevTestingIoWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    switch (Port)
    {
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

        case VMMDEV_TESTING_IOPORT_TS_LOW:
            break;

        case VMMDEV_TESTING_IOPORT_TS_HIGH:
            break;

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
                                   vmmdevTestingMmioWrite,
                                   vmmdevTestingMmioRead,
                                   NULL /*pfnFill*/,
                                   "VMMDev Testing");
    AssertRCReturn(rc, rc);
    if (pThis->fRZEnabled)
    {
        rc = PDMDevHlpMMIORegisterR0(pDevIns, VMMDEV_TESTING_MMIO_BASE, VMMDEV_TESTING_MMIO_SIZE, NIL_RTR0PTR /*pvUser*/,
                                     "vmmdevTestingMmioWrite",
                                     "vmmdevTestingMmioRead",
                                     NULL /*pszFill*/);
        AssertRCReturn(rc, rc);
        rc = PDMDevHlpMMIORegisterRC(pDevIns, VMMDEV_TESTING_MMIO_BASE, VMMDEV_TESTING_MMIO_SIZE, NIL_RTRCPTR /*pvUser*/,
                                     "vmmdevTestingMmioWrite",
                                     "vmmdevTestingMmioRead",
                                     NULL /*pszFill*/);
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

