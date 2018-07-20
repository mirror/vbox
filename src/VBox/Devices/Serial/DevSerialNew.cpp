/* $Id$ */
/** @file
 * DevSerial - 16550A UART emulation.
 *
 * The documentation for this device was taken from the PC16550D spec from TI.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DEV_SERIAL
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmserialifs.h>
#include <VBox/vmm/vm.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/critsect.h>

#include "VBoxDD.h"
#include "UartCore.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * Serial device.
 */
typedef struct DEVSERIAL
{
    /** Pointer to the device instance - R3 Ptr. */
    PPDMDEVINSR3                    pDevInsR3;
    /** Pointer to the device instance - R0 Ptr. */
    PPDMDEVINSR0                    pDevInsR0;
    /** Pointer to the device instance - RC Ptr. */
    PPDMDEVINSRC                    pDevInsRC;
    /** Alignment. */
    RTRCPTR                         Alignment0;
    /** Flag whether the R0 portion of this device is enabled. */
    bool                            fR0Enabled;
    /** Flag whether the RC portion of this device is enabled. */
    bool                            fRCEnabled;
    /** Alignment. */
    bool                            afAlignment1[2];
    /** The IRQ value. */
    uint8_t                         uIrq;
    /** The base I/O port the device is registered at. */
    RTIOPORT                        PortBase;

    /** The UART core. */
    UARTCORE                        UartCore;
} DEVSERIAL;
/** Pointer to the serial device state. */
typedef DEVSERIAL *PDEVSERIAL;

#ifndef VBOX_DEVICE_STRUCT_TESTCASE


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/


PDMBOTHCBDECL(void) serialIrqReq(PPDMDEVINS pDevIns, PUARTCORE pUart, unsigned iLUN, int iLvl)
{
    RT_NOREF(pUart, iLUN);
    PDEVSERIAL pThis = PDMINS_2_DATA(pDevIns, PDEVSERIAL);
    PDMDevHlpISASetIrqNoWait(pDevIns, pThis->uIrq, iLvl);
}


/* -=-=-=-=-=-=-=-=- I/O Port Access Handlers -=-=-=-=-=-=-=-=- */

/**
 * @callback_method_impl{FNIOMIOPORTOUT}
 */
PDMBOTHCBDECL(int) serialIoPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT uPort, uint32_t u32, unsigned cb)
{
    PDEVSERIAL pThis = PDMINS_2_DATA(pDevIns, PDEVSERIAL);
    RT_NOREF_PV(pvUser);

    return uartRegWrite(&pThis->UartCore, uPort - pThis->PortBase, u32, cb);
}


/**
 * @callback_method_impl{FNIOMIOPORTIN}
 */
PDMBOTHCBDECL(int) serialIoPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT uPort, uint32_t *pu32, unsigned cb)
{
    PDEVSERIAL pThis = PDMINS_2_DATA(pDevIns, PDEVSERIAL);
    RT_NOREF_PV(pvUser);

    return uartRegRead(&pThis->UartCore, uPort - pThis->PortBase, pu32, cb);
}


#ifdef IN_RING3


/**
 * Returns the matching UART type from the given string.
 *
 * @returns UART type based on the given string or UARTTYPE_INVALID if an invalid type was passed.
 * @param   pszUartType         The UART type.
 */
static UARTTYPE serialR3GetUartTypeFromString(const char *pszUartType)
{
    if (!RTStrCmp(pszUartType, "15450"))
        return UARTTYPE_16450;
    else if (!RTStrCmp(pszUartType, "15550A"))
        return UARTTYPE_16550A;
    else if (!RTStrCmp(pszUartType, "16750"))
        return UARTTYPE_16750;

    AssertLogRelMsgFailedReturn(("Unknown UART type \"%s\" specified", pszUartType),
                                UARTTYPE_INVALID);
}


/* -=-=-=-=-=-=-=-=- Saved State -=-=-=-=-=-=-=-=- */

/**
 * @callback_method_impl{FNSSMDEVLIVEEXEC}
 */
static DECLCALLBACK(int) serialR3LiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    RT_NOREF(uPass);
    PDEVSERIAL pThis = PDMINS_2_DATA(pDevIns, PDEVSERIAL);
    SSMR3PutU8(pSSM, pThis->uIrq);
    SSMR3PutIOPort(pSSM, pThis->PortBase);
    SSMR3PutU32(pSSM, pThis->UartCore.enmType);

    return VINF_SSM_DONT_CALL_AGAIN;
}


/**
 * @callback_method_impl{FNSSMDEVSAVEEXEC}
 */
static DECLCALLBACK(int) serialR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PDEVSERIAL pThis = PDMINS_2_DATA(pDevIns, PDEVSERIAL);

    SSMR3PutU8(    pSSM, pThis->uIrq);
    SSMR3PutIOPort(pSSM, pThis->PortBase);
    SSMR3PutU32(   pSSM, pThis->UartCore.enmType);

    uartR3SaveExec(&pThis->UartCore, pSSM);
    return SSMR3PutU32(pSSM, UINT32_MAX); /* sanity/terminator */
}


/**
 * @callback_method_impl{FNSSMDEVLOADEXEC}
 */
static DECLCALLBACK(int) serialR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PDEVSERIAL pThis = PDMINS_2_DATA(pDevIns, PDEVSERIAL);
    uint8_t    uIrq;
    RTIOPORT   PortBase;
    UARTTYPE   enmType;

    AssertMsgReturn(uVersion >= UART_SAVED_STATE_VERSION_16450, ("%d\n", uVersion), VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION);

    if (uPass != SSM_PASS_FINAL)
    {
        SSMR3GetU8(pSSM, &uIrq);
        SSMR3GetIOPort(pSSM, &PortBase);
        int rc = SSMR3GetU32(pSSM, (uint32_t *)&enmType);
        AssertRCReturn(rc, rc);
    }
    else
    {
        int rc = VINF_SUCCESS;

        if (uVersion > UART_SAVED_STATE_VERSION_LEGACY_CODE)
        {
            SSMR3GetU8(    pSSM, &uIrq);
            SSMR3GetIOPort(pSSM, &PortBase);
            SSMR3GetU32(   pSSM, (uint32_t *)&enmType);
            rc = uartR3LoadExec(&pThis->UartCore, pSSM, uVersion, uPass, NULL, NULL);
        }
        else
            rc = uartR3LoadExec(&pThis->UartCore, pSSM, uVersion, uPass, &uIrq, &PortBase);
        if (RT_SUCCESS(rc))
        {
            /* The marker. */
            uint32_t u32;
            rc = SSMR3GetU32(pSSM, &u32);
            if (RT_FAILURE(rc))
                return rc;
            AssertMsgReturn(u32 == UINT32_MAX, ("%#x\n", u32), VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
        }
    }

    /*
     * Check the config.
     */
    if (    pThis->uIrq     != uIrq
        ||  pThis->PortBase != PortBase
        ||  pThis->UartCore.enmType != enmType)
        return SSMR3SetCfgError(pSSM, RT_SRC_POS,
                                N_("Config mismatch - saved IRQ=%#x PortBase=%#x Type=%d; configured IRQ=%#x PortBase=%#x Type=%d"),
                                uIrq, PortBase, enmType, pThis->uIrq, pThis->PortBase, pThis->UartCore.enmType);

    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNSSMDEVLOADDONE}
 */
static DECLCALLBACK(int) serialR3LoadDone(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PDEVSERIAL  pThis = PDMINS_2_DATA(pDevIns, PDEVSERIAL);
    return uartR3LoadDone(&pThis->UartCore, pSSM);
}


/* -=-=-=-=-=-=-=-=- PDMDEVREG -=-=-=-=-=-=-=-=- */

/**
 * @interface_method_impl{PDMDEVREG,pfnRelocate}
 */
static DECLCALLBACK(void) serialR3Relocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    PDEVSERIAL pThis = PDMINS_2_DATA(pDevIns, PDEVSERIAL);
    uartR3Relocate(&pThis->UartCore, offDelta);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) serialR3Reset(PPDMDEVINS pDevIns)
{
    PDEVSERIAL pThis = PDMINS_2_DATA(pDevIns, PDEVSERIAL);
    uartR3Reset(&pThis->UartCore);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnAttach}
 */
static DECLCALLBACK(int) serialR3Attach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    RT_NOREF(fFlags);
    PDEVSERIAL pThis = PDMINS_2_DATA(pDevIns, PDEVSERIAL);
    return uartR3Attach(&pThis->UartCore, iLUN);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDetach}
 */
static DECLCALLBACK(void) serialR3Detach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    RT_NOREF(iLUN, fFlags);
    PDEVSERIAL pThis = PDMINS_2_DATA(pDevIns, PDEVSERIAL);
    uartR3Detach(&pThis->UartCore);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) serialR3Destruct(PPDMDEVINS pDevIns)
{
    PDEVSERIAL pThis = PDMINS_2_DATA(pDevIns, PDEVSERIAL);
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);

    uartR3Destruct(&pThis->UartCore);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) serialR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDEVSERIAL pThis = PDMINS_2_DATA(pDevIns, PDEVSERIAL);
    int        rc = VINF_SUCCESS;

    Assert(iInstance < 4);
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

    /*
     * Initialize the instance data.
     * (Do this early or the destructor might choke on something!)
     */
    pThis->pDevInsR3 = pDevIns;
    pThis->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

    /*
     * Validate and read the configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "IRQ\0"
                                    "IOBase\0"
                                    "GCEnabled\0"
                                    "R0Enabled\0"
                                    "YieldOnLSRRead\0"
                                    "UartType\0"
                                    ))
    {
        AssertMsgFailed(("serialConstruct Invalid configuration values\n"));
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;
    }

    rc = CFGMR3QueryBoolDef(pCfg, "GCEnabled", &pThis->fRCEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"GCEnabled\" value"));

    rc = CFGMR3QueryBoolDef(pCfg, "R0Enabled", &pThis->fR0Enabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"R0Enabled\" value"));

    bool fYieldOnLSRRead = false;
    rc = CFGMR3QueryBoolDef(pCfg, "YieldOnLSRRead", &fYieldOnLSRRead, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"YieldOnLSRRead\" value"));

    uint8_t uIrq = 0;
    rc = CFGMR3QueryU8(pCfg, "IRQ", &uIrq);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
    {
        /* Provide sensible defaults. */
        if (iInstance == 0)
            uIrq = 4;
        else if (iInstance == 1)
            uIrq = 3;
        else
            AssertReleaseFailed(); /* irq_lvl is undefined. */
    }
    else if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"IRQ\" value"));

    uint16_t uIoBase = 0;
    rc = CFGMR3QueryU16(pCfg, "IOBase", &uIoBase);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
    {
        if (iInstance == 0)
            uIoBase = 0x3f8;
        else if (iInstance == 1)
            uIoBase = 0x2f8;
        else
            AssertReleaseFailed(); /* uIoBase is undefined */
    }
    else if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"IOBase\" value"));

    char *pszUartType;
    rc = CFGMR3QueryStringAllocDef(pCfg, "UartType", &pszUartType, "16550A");
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: failed to read \"UartType\" as string"));

    UARTTYPE enmUartType = serialR3GetUartTypeFromString(pszUartType);

    if (enmUartType != UARTTYPE_INVALID)
        LogRel(("Serial#%d: emulating %s (IOBase: %04x IRQ: %u)\n",
                pDevIns->iInstance, pszUartType, uIoBase, uIrq));

    MMR3HeapFree(pszUartType);

    if (enmUartType == UARTTYPE_INVALID)
        return PDMDEV_SET_ERROR(pDevIns, VERR_INVALID_PARAMETER,
                                N_("Configuration error: \"UartType\" contains invalid type"));

    pThis->uIrq     = uIrq;
    pThis->PortBase = uIoBase;

    /*
     * Init locks, using explicit locking where necessary.
     */
    rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Register the I/O ports.
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, uIoBase, 8, 0,
                                 serialIoPortWrite, serialIoPortRead,
                                 NULL, NULL, "SERIAL");
    if (RT_FAILURE(rc))
        return rc;

    PVM pVM = PDMDevHlpGetVM(pDevIns);
    RTR0PTR pfnSerialIrqReqR0 = NIL_RTR0PTR;
    RTRCPTR pfnSerialIrqReqRC = NIL_RTRCPTR;

    if (pThis->fRCEnabled)
    {
        rc = PDMDevHlpIOPortRegisterRC(pDevIns, uIoBase, 8, 0, "serialIoPortWrite",
                                      "serialIoPortRead", NULL, NULL, "SERIAL");
        if (   RT_SUCCESS(rc)
            && VM_IS_RAW_MODE_ENABLED(pVM))
            rc = PDMR3LdrGetSymbolRC(pVM, pDevIns->pReg->szRCMod, "serialIrqReq", &pfnSerialIrqReqRC);

        if (RT_FAILURE(rc))
            return rc;
    }

    if (pThis->fR0Enabled)
    {
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, uIoBase, 8, 0, "serialIoPortWrite",
                                      "serialIoPortRead", NULL, NULL, "SERIAL");
        if (RT_SUCCESS(rc))
            rc = PDMR3LdrGetSymbolR0(pVM, pDevIns->pReg->szR0Mod, "serialIrqReq", &pfnSerialIrqReqR0);

        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Saved state.
     */
    rc = PDMDevHlpSSMRegisterEx(pDevIns, UART_SAVED_STATE_VERSION, sizeof(*pThis), NULL,
                                NULL, serialR3LiveExec, NULL,
                                NULL, serialR3SaveExec, NULL,
                                NULL, serialR3LoadExec, serialR3LoadDone);
    if (RT_FAILURE(rc))
        return rc;

    /* Init the UART core structure. */
    rc = uartR3Init(&pThis->UartCore, pDevIns, enmUartType, 0,
                    fYieldOnLSRRead ? UART_CORE_YIELD_ON_LSR_READ : 0, serialIrqReq, pfnSerialIrqReqR0, pfnSerialIrqReqRC);

    serialR3Reset(pDevIns);
    return VINF_SUCCESS;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceSerialPort =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "serial",
    /* szRCMod */
    "VBoxDDRC.rc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "Serial Communication Port",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_SERIAL,
    /* cMaxInstances */
    UINT32_MAX,
    /* cbInstance */
    sizeof(DEVSERIAL),
    /* pfnConstruct */
    serialR3Construct,
    /* pfnDestruct */
    serialR3Destruct,
    /* pfnRelocate */
    serialR3Relocate,
    /* pfnMemSetup */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    serialR3Reset,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    serialR3Attach,
    /* pfnDetach */
    serialR3Detach,
    /* pfnQueryInterface. */
    NULL,
    /* pfnInitComplete */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};
#endif /* IN_RING3 */

#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */
