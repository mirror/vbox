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
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/critsect.h>

#include "VBoxDD.h"

/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * Serial device.
 *
 * @implements  PDMIBASE
 * @implements  PDMISERIALPORT
 */
typedef struct DEVSERIAL
{
    /** Access critical section. */
    PDMCRITSECT                     CritSect;
    /** Pointer to the device instance - R3 Ptr. */
    PPDMDEVINSR3                    pDevInsR3;
    /** Pointer to the device instance - R0 Ptr. */
    PPDMDEVINSR0                    pDevInsR0;
    /** Pointer to the device instance - RC Ptr. */
    PPDMDEVINSRC                    pDevInsRC;
    /** Alignment. */
    RTRCPTR                         Alignment0;
    /** LUN\#0: The base interface. */
    PDMIBASE                        IBase;
    /** LUN\#0: The serial port interface. */
    PDMISERIALPORT                  ISerialPort;
    /** Pointer to the attached base driver. */
    R3PTRTYPE(PPDMIBASE)            pDrvBase;
    /** Pointer to the attached serial driver. */
    R3PTRTYPE(PPDMISERIALCONNECTOR) pDrvSerial;
    /** Flag whether the R0 portion of this device is enabled. */
    bool                            fR0Enabled;
    /** Flag whether the RC portion of this device is enabled. */
    bool                            fRCEnabled;
    /** Flag whether an 16550A (with FIFO) or a plain 16450 is emulated. */
    bool                            f16550AEnabled;
    /** Flag whether to yield on an guest LSR read. */
    bool                            fYieldOnLSRRead;
    /** The IRQ value. */
    uint8_t                         uIrq;
    /** The base I/O port the device is registered at. */
    RTIOPORT                        PortBase;

} DEVSERIAL;
/** Pointer to the serial device state. */
typedef DEVSERIAL *PDEVSERIAL;

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

/* -=-=-=-=-=-=-=-=- I/O Port Access Handlers -=-=-=-=-=-=-=-=- */

/**
 * @callback_method_impl{FNIOMIOPORTOUT}
 */
PDMBOTHCBDECL(int) serialIoPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT uPort, uint32_t u32, unsigned cb)
{
    PDEVSERIAL pThis = PDMINS_2_DATA(pDevIns, PDEVSERIAL);
    Assert(PDMCritSectIsOwner(&pThis->CritSect));
    RT_NOREF_PV(pvUser);
    RT_NOREF(u32);

    AssertMsgReturn(cb == 1, ("uPort=%#x cb=%d u32=%#x\n", uPort, cb, u32), VINF_SUCCESS);

    int rc = VINF_SUCCESS;
    switch (uPort & 0x7)
    {
        default:
            break;
    }

    return rc;
}


/**
 * @callback_method_impl{FNIOMIOPORTIN}
 */
PDMBOTHCBDECL(int) serialIoPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT uPort, uint32_t *pu32, unsigned cb)
{
    PDEVSERIAL pThis = PDMINS_2_DATA(pDevIns, PDEVSERIAL);
    Assert(PDMCritSectIsOwner(&pThis->CritSect));
    RT_NOREF_PV(pvUser);
    RT_NOREF(pu32);

    if (cb != 1)
        return VERR_IOM_IOPORT_UNUSED;

    int rc = VINF_SUCCESS;
    switch (uPort & 0x7)
    {
        default:
            rc = VERR_IOM_IOPORT_UNUSED;
    }

    return rc;
}


#ifdef IN_RING3

/* -=-=-=-=-=-=-=-=- PDMISERIALPORT on LUN#0 -=-=-=-=-=-=-=-=- */

static DECLCALLBACK(int) serialDataAvailRdrNotify(PPDMISERIALPORT pInterface, size_t cbAvail)
{
    PDEVSERIAL pThis = RT_FROM_MEMBER(pInterface, DEVSERIAL, ISerialPort);

    PDMCritSectEnter(&pThis->CritSect, VERR_IGNORED);
    RT_NOREF(cbAvail);
    PDMCritSectLeave(&pThis->CritSect);
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) serialReadWr(PPDMISERIALPORT pInterface, void *pvBuf, size_t cbRead, size_t *pcbRead)
{
    PDEVSERIAL pThis = RT_FROM_MEMBER(pInterface, DEVSERIAL, ISerialPort);

    PDMCritSectEnter(&pThis->CritSect, VERR_IGNORED);
    /** @todo */
    RT_NOREF(pvBuf, cbRead, pcbRead);
    PDMCritSectLeave(&pThis->CritSect);
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) serialNotifyStsLinesChanged(PPDMISERIALPORT pInterface, uint32_t fNewStatusLines)
{
    PDEVSERIAL pThis = RT_FROM_MEMBER(pInterface, DEVSERIAL, ISerialPort);

    PDMCritSectEnter(&pThis->CritSect, VERR_IGNORED);
    /** @todo */
    RT_NOREF(fNewStatusLines);
    PDMCritSectLeave(&pThis->CritSect);
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) serialNotifyBrk(PPDMISERIALPORT pInterface)
{
    PDEVSERIAL pThis = RT_FROM_MEMBER(pInterface, DEVSERIAL, ISerialPort);

    PDMCritSectEnter(&pThis->CritSect, VERR_IGNORED);
    /** @todo */
    PDMCritSectLeave(&pThis->CritSect);
    return VERR_NOT_IMPLEMENTED;
}


/* -=-=-=-=-=-=-=-=- PDMIBASE on LUN#0 -=-=-=-=-=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) serialR3QueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PDEVSERIAL pThis = RT_FROM_MEMBER(pInterface, DEVSERIAL, IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMISERIALPORT, &pThis->ISerialPort);
    return NULL;
}


/* -=-=-=-=-=-=-=-=- PDMDEVREG -=-=-=-=-=-=-=-=- */

/**
 * @interface_method_impl{PDMDEVREG,pfnRelocate}
 */
static DECLCALLBACK(void) serialR3Relocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    RT_NOREF(offDelta);
    PDEVSERIAL pThis = PDMINS_2_DATA(pDevIns, PDEVSERIAL);

    pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) serialR3Reset(PPDMDEVINS pDevIns)
{
    PDEVSERIAL pThis = PDMINS_2_DATA(pDevIns, PDEVSERIAL);
    RT_NOREF(pThis);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnAttach}
 */
static DECLCALLBACK(int) serialR3Attach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    RT_NOREF(iLUN, fFlags);
    PDEVSERIAL pThis = PDMINS_2_DATA(pDevIns, PDEVSERIAL);

    int rc = PDMDevHlpDriverAttach(pDevIns, 0, &pThis->IBase, &pThis->pDrvBase, "Serial Char");
    if (RT_SUCCESS(rc))
    {
        pThis->pDrvSerial = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMISERIALCONNECTOR);
        if (!pThis->pDrvSerial)
        {
            AssertLogRelMsgFailed(("Configuration error: instance %d has no serial interface!\n", pDevIns->iInstance));
            return VERR_PDM_MISSING_INTERFACE;
        }
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
    {
        pThis->pDrvBase = NULL;
        pThis->pDrvSerial = NULL;
        rc = VINF_SUCCESS;
        LogRel(("Serial%d: no unit\n", pDevIns->iInstance));
    }
    else /* Don't call VMSetError here as we assume that the driver already set an appropriate error */
        LogRel(("Serial%d: Failed to attach to serial driver. rc=%Rrc\n", pDevIns->iInstance, rc));

   return rc;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDetach}
 */
static DECLCALLBACK(void) serialR3Detach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    RT_NOREF(iLUN, fFlags);
    PDEVSERIAL pThis = PDMINS_2_DATA(pDevIns, PDEVSERIAL);

    /* Zero out important members. */
    pThis->pDrvBase   = NULL;
    pThis->pDrvSerial = NULL;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) serialR3Destruct(PPDMDEVINS pDevIns)
{
    PDEVSERIAL pThis = PDMINS_2_DATA(pDevIns, PDEVSERIAL);
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);

    PDMR3CritSectDelete(&pThis->CritSect);
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

    /* IBase */
    pThis->IBase.pfnQueryInterface = serialR3QueryInterface;

    /* ISerialPort */
    pThis->ISerialPort.pfnDataAvailRdrNotify    = serialDataAvailRdrNotify;
    pThis->ISerialPort.pfnReadWr                = serialReadWr;
    pThis->ISerialPort.pfnNotifyStsLinesChanged = serialNotifyStsLinesChanged;
    pThis->ISerialPort.pfnNotifyBrk             = serialNotifyBrk;

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

    rc = CFGMR3QueryBoolDef(pCfg, "YieldOnLSRRead", &pThis->fYieldOnLSRRead, false);
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
            AssertReleaseFailed(); /* io_base is undefined */
    }
    else if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"IOBase\" value"));

    rc = CFGMR3QueryBoolDef(pCfg, "Enable16550A", &pThis->f16550AEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"Enable16550A\" value"));

    pThis->uIrq     = uIrq;
    pThis->PortBase = uIoBase;

    LogRel(("Serial#%d: emulating %s (IOBase: %04x IRQ: %u)\n",
            pDevIns->iInstance, pThis->f16550AEnabled ? "16550A" : "16450", uIoBase, uIrq));

    /*
     * Initialize critical section and the semaphore.  Change the default
     * critical section to ours so that TM and IOM will enter it before
     * calling us.
     *
     * Note! This must of be done BEFORE creating timers, registering I/O ports
     *       and other things which might pick up the default CS or end up
     *       calling back into the device.
     */
    rc = PDMDevHlpCritSectInit(pDevIns, &pThis->CritSect, RT_SRC_POS, "Serial#%u", iInstance);
    AssertRCReturn(rc, rc);

    rc = PDMDevHlpSetDeviceCritSect(pDevIns, &pThis->CritSect);
    AssertRCReturn(rc, rc);

    /*
     * Register the I/O ports.
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, uIoBase, 8, 0,
                                 serialIoPortWrite, serialIoPortRead,
                                 NULL, NULL, "SERIAL");
    if (RT_FAILURE(rc))
        return rc;

    if (pThis->fRCEnabled)
    {
        rc = PDMDevHlpIOPortRegisterRC(pDevIns, uIoBase, 8, 0, "serialIoPortWrite",
                                      "serialIoPortRead", NULL, NULL, "SERIAL");
        if (RT_FAILURE(rc))
            return rc;
    }

    if (pThis->fR0Enabled)
    {
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, uIoBase, 8, 0, "serialIoPortWrite",
                                      "serialIoPortRead", NULL, NULL, "SERIAL");
        if (RT_FAILURE(rc))
            return rc;
    }

#if 0 /** @todo: Later */
    /*
     * Saved state.
     */
    rc = PDMDevHlpSSMRegister3(pDevIns, SERIAL_SAVED_STATE_VERSION, sizeof (*pThis),
                               serialR3LiveExec, serialR3SaveExec, serialR3LoadExec);
    if (RT_FAILURE(rc))
        return rc;
#endif

    /*
     * Attach the char driver and get the interfaces.
     */
    rc = PDMDevHlpDriverAttach(pDevIns, 0, &pThis->IBase, &pThis->pDrvBase, "Serial");
    if (RT_SUCCESS(rc))
    {
        pThis->pDrvSerial = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMISERIALCONNECTOR);
        if (!pThis->pDrvSerial)
        {
            AssertLogRelMsgFailed(("Configuration error: instance %d has no serial interface!\n", iInstance));
            return VERR_PDM_MISSING_INTERFACE;
        }
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
    {
        pThis->pDrvBase   = NULL;
        pThis->pDrvSerial = NULL;
        LogRel(("Serial%d: no unit\n", iInstance));
    }
    else
    {
        AssertLogRelMsgFailed(("Serial%d: Failed to attach to char driver. rc=%Rrc\n", iInstance, rc));
        /* Don't call VMSetError here as we assume that the driver already set an appropriate error */
        return rc;
    }

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
