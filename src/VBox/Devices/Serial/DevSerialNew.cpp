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
                                    "Enable16550A\0"
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

    bool f16550AEnabled = true;
    rc = CFGMR3QueryBoolDef(pCfg, "Enable16550A", &f16550AEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"Enable16550A\" value"));

    pThis->uIrq     = uIrq;
    pThis->PortBase = uIoBase;

    LogRel(("Serial#%d: emulating %s (IOBase: %04x IRQ: %u)\n",
            pDevIns->iInstance, f16550AEnabled ? "16550A" : "16450", uIoBase, uIrq));

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

#if 0 /** @todo Later */
    /*
     * Saved state.
     */
    rc = PDMDevHlpSSMRegister3(pDevIns, SERIAL_SAVED_STATE_VERSION, sizeof (*pThis),
                               serialR3LiveExec, serialR3SaveExec, serialR3LoadExec);
    if (RT_FAILURE(rc))
        return rc;
#endif


    /* Init the UART core structure. */
    rc = uartR3Init(&pThis->UartCore, pDevIns, f16550AEnabled ? UARTTYPE_16550A : UARTTYPE_16450, 0,
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
