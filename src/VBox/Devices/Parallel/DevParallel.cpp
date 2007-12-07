/* $Id$ */
/** @file
 * VirtualBox Parallel Device Emulation.
 *
 * Contributed by: Alexander Eichner
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

/* based on DevSerial.cpp */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_PARALLEL
#include <VBox/pdmdev.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/critsect.h>

#include "Builtins.h"
#include "ParallelIOCtlCmd.h"

#define PARALLEL_SAVED_STATE_VERSION 1

/* defines for accessing the register bits */
#define LPT_STATUS_BUSY                0x80
#define LPT_STATUS_ACK                 0x40
#define LPT_STATUS_PAPER_OUT           0x20
#define LPT_STATUS_SELECT_IN           0x10
#define LPT_STATUS_ERROR               0x08
#define LPT_STATUS_IRQ                 0x04
#define LPT_STATUS_BIT1                0x02 /* reserved (only for completeness) */
#define LPT_STATUS_BIT0                0x01 /* reserved (only for completeness) */

#define LPT_CONTROL_BIT7               0x80 /* reserved (only for completeness) */
#define LPT_CONTROL_BIT6               0x40 /* reserved (only for completeness) */
#define LPT_CONTROL_ENABLE_BIDIRECT    0x20
#define LPT_CONTROL_ENABLE_IRQ_VIA_ACK 0x10
#define LPT_CONTROL_SELECT_PRINTER     0x08
#define LPT_CONTROL_RESET              0x04
#define LPT_CONTROL_AUTO_LINEFEED      0x02
#define LPT_CONTROL_STROBE             0x01

typedef struct ParallelState
{
    /** Access critical section. */
    PDMCRITSECT                         CritSect;

    /** Pointer to the device instance. */
    R3R0PTRTYPE(PPDMDEVINS)             pDevInsHC;
    /** Pointer to the device instance. */
    GCPTRTYPE(PPDMDEVINS)               pDevInsGC;
#if HC_ARCH_BITS == 64 && GC_ARCH_BITS != 64
    RTGCPTR                             Alignment0;
#endif
    /** The base interface. */
    R3PTRTYPE(PDMIBASE)                 IBase;
    /** The host device port interface. */
    R3PTRTYPE(PDMIHOSTDEVICEPORT)       IHostDevicePort;
    /** Pointer to the attached base driver. */
    R3PTRTYPE(PPDMIBASE)                pDrvBase;
    /** Pointer to the attached host device. */
    R3PTRTYPE(PPDMIHOSTDEVICECONNECTOR) pDrvHostDeviceConnector;

    uint8_t                             reg_data;
    uint8_t                             reg_status;
    uint8_t                             reg_control;

    int                                 irq;

    bool                                fGCEnabled;
    bool                                fR0Enabled;
    bool                                afAlignment[6];

    RTSEMEVENT                          ReceiveSem;
    uint32_t                            base;

} DEVPARALLELSTATE, *PDEVPARALLELSTATE;
typedef DEVPARALLELSTATE ParallelState;

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

#define PDMIBASE_2_PARALLELSTATE(pInstance)       ( (ParallelState *)((uintptr_t)(pInterface) - RT_OFFSETOF(ParallelState, IBase)) )
#define PDMIHOSTDEVICEPORT_2_PARALLELSTATE(pInstance)   ( (ParallelState *)((uintptr_t)(pInterface) - RT_OFFSETOF(ParallelState, IHostDevicePort)) )


__BEGIN_DECLS
PDMBOTHCBDECL(int) parallelIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) parallelIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
__END_DECLS


static void parallel_update_irq(ParallelState *s)
{
    if (s->reg_control & LPT_CONTROL_ENABLE_IRQ_VIA_ACK) {
        Log(("parallel_update_irq %d 1\n", s->irq));
        PDMDevHlpISASetIrqNoWait(CTXSUFF(s->pDevIns), s->irq, 1);
    } else {
        Log(("parallel_update_irq %d 0\n", s->irq));
        PDMDevHlpISASetIrqNoWait(CTXSUFF(s->pDevIns), s->irq, 0);
    }
}

static int parallel_ioport_write(void *opaque, uint32_t addr, uint32_t val)
{
    ParallelState *s = (ParallelState *)opaque;
    unsigned char ch;

    addr &= 7;
    LogFlow(("parallel: write addr=0x%02x val=0x%02x\n", addr, val));
    switch(addr) {
    default:
    case 0:
#ifndef IN_RING3
        NOREF(ch);
        return VINF_IOM_HC_IOPORT_WRITE;
#else
        ch = val;
        s->reg_data = ch;
        if (RT_LIKELY(s->pDrvHostDeviceConnector))
        {
            Log(("parallel_io_port_write: write 0x%X\n", ch));
            size_t cbWrite = 1;
            int rc = s->pDrvHostDeviceConnector->pfnWrite(s->pDrvHostDeviceConnector, &ch, &cbWrite);
            AssertRC(rc);
        }
#endif
        break;
    case 1:
        break;
    case 2:
        s->reg_control = val;
        parallel_update_irq(s);
        break;
    case 3:
        break;
    case 4:
        break;
    case 5:
        break;
    case 6:
        break;
    case 7:
        break;
    }
    return VINF_SUCCESS;
}

static uint32_t parallel_ioport_read(void *opaque, uint32_t addr, int *pRC)
{
    ParallelState *s = (ParallelState *)opaque;
    uint32_t ret = ~0U;

    *pRC = VINF_SUCCESS;

    addr &= 7;
    switch(addr) {
    default:
    case 0:
#ifndef IN_RING3
            *pRC = VINF_IOM_HC_IOPORT_READ;
#else
            if (RT_LIKELY(s->pDrvHostDeviceConnector))
            {
                size_t cbRead;
                int rc = s->pDrvHostDeviceConnector->pfnRead(s->pDrvHostDeviceConnector, &s->reg_data, &cbRead);
                Log(("parallel_io_port_read: read 0x%X\n", s->reg_data));
                AssertRC(rc);
            }
            ret = s->reg_data;
#endif
        break;
    case 1:
        ret = s->reg_status;
        break;
    case 2:
        ret = s->reg_control;
        break;
    case 3:
        break;
    case 4:
        break;
    case 5:
        break;
    case 6:
        break;
    case 7:
        break;
    }
    LogFlow(("parallel: read addr=0x%02x val=0x%02x\n", addr, ret));
    return ret;
}

#ifdef IN_RING3
static DECLCALLBACK(int) parallelNotifyRead(PPDMICHARPORT pInterface, const void *pvBuf, size_t *pcbRead)
{
    ParallelState *pData = PDMIHOSTDEVICEPORT_2_PARALLELSTATE(pInterface);
    int rc;

    NOREF(pvBuf); NOREF(pcbRead); NOREF(pData); NOREF(rc);
    return VINF_SUCCESS;
#if 0
    Assert(*pcbRead != 0);

    PDMCritSectEnter(&pData->CritSect, VERR_PERMISSION_DENIED);
    if (pData->lsr & UART_LSR_DR)
    {
        /* If a character is still in the read queue, then wait for it to be emptied. */
        PDMCritSectLeave(&pData->CritSect);
        rc = RTSemEventWait(pData->ReceiveSem, 250);
        if (VBOX_FAILURE(rc))
            return rc;

        PDMCritSectEnter(&pData->CritSect, VERR_PERMISSION_DENIED);
    }

    if (!(pData->lsr & UART_LSR_DR))
    {
        pData->rbr = *(const char *)pvBuf;
        pData->lsr |= UART_LSR_DR;
        serial_update_irq(pData);
        *pcbRead = 1;
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_TIMEOUT;

    PDMCritSectLeave(&pData->CritSect);

    return rc;
#endif
}
#endif /* IN_RING3 */

/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   Port        Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) parallelIOPortWrite(PPDMDEVINS pDevIns, void *pvUser,
                                       RTIOPORT Port, uint32_t u32, unsigned cb)
{
    ParallelState *pData = PDMINS2DATA(pDevIns, ParallelState *);
    int          rc = VINF_SUCCESS;

    if (cb == 1)
    {
        rc = PDMCritSectEnter(&pData->CritSect, VINF_IOM_HC_IOPORT_WRITE);
        if (rc == VINF_SUCCESS)
        {
            Log2(("%s: port %#06x val %#04x\n", __FUNCTION__, Port, u32));
            rc = parallel_ioport_write (pData, Port, u32);
            PDMCritSectLeave(&pData->CritSect);
        }
    }
    else
        AssertMsgFailed(("Port=%#x cb=%d u32=%#x\n", Port, cb, u32));

    return rc;
}

/**
 * Port I/O Handler for IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   Port        Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) parallelIOPortRead(PPDMDEVINS pDevIns, void *pvUser,
                                      RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    ParallelState *pData = PDMINS2DATA(pDevIns, ParallelState *);
    int          rc = VINF_SUCCESS;

    if (cb == 1)
    {
        rc = PDMCritSectEnter(&pData->CritSect, VINF_IOM_HC_IOPORT_READ);
        if (rc == VINF_SUCCESS)
        {
            *pu32 = parallel_ioport_read (pData, Port, &rc);
            Log2(("%s: port %#06x val %#04x\n", __FUNCTION__, Port, *pu32));
            PDMCritSectLeave(&pData->CritSect);
        }
    }
    else
        rc = VERR_IOM_IOPORT_UNUSED;

    return rc;
}

#ifdef IN_RING3
/**
 * Saves a state of the serial port device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to save the state to.
 */
static DECLCALLBACK(int) parallelSaveExec(PPDMDEVINS pDevIns,
                                          PSSMHANDLE pSSMHandle)
{
    ParallelState *pData = PDMINS2DATA(pDevIns, ParallelState *);

    SSMR3PutU8(pSSMHandle, pData->reg_data);
    SSMR3PutU8(pSSMHandle, pData->reg_status);
    SSMR3PutU8(pSSMHandle, pData->reg_control);
    SSMR3PutS32(pSSMHandle, pData->irq);
    SSMR3PutU32(pSSMHandle, pData->base);

    return SSMR3PutU32(pSSMHandle, ~0); /* sanity/terminator */
}

/**
 * Loads a saved serial port device state.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to the saved state.
 * @param   u32Version  The data unit version number.
 */
static DECLCALLBACK(int) parallelLoadExec(PPDMDEVINS pDevIns,
                                          PSSMHANDLE pSSMHandle,
                                          uint32_t u32Version)
{
    int          rc;
    uint32_t     u32;
    ParallelState *pData = PDMINS2DATA(pDevIns, ParallelState *);

    if (u32Version != PARALLEL_SAVED_STATE_VERSION)
    {
        AssertMsgFailed(("u32Version=%d\n", u32Version));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    SSMR3GetU8(pSSMHandle, &pData->reg_data);
    SSMR3GetU8(pSSMHandle, &pData->reg_status);
    SSMR3GetU8(pSSMHandle, &pData->reg_control);
    SSMR3GetS32(pSSMHandle, &pData->irq);
    SSMR3GetU32(pSSMHandle, &pData->base);

    rc = SSMR3GetU32(pSSMHandle, &u32);
    if (VBOX_FAILURE(rc))
        return rc;

    if (u32 != ~0U)
    {
        AssertMsgFailed(("u32=%#x expected ~0\n", u32));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }

    pData->pDevInsHC = pDevIns;
    pData->pDevInsGC = PDMDEVINS_2_GCPTR(pDevIns);
    return VINF_SUCCESS;
}


/**
 * @copydoc FNPDMDEVRELOCATE
 */
static DECLCALLBACK(void) parallelRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    ParallelState *pData = PDMINS2DATA(pDevIns, ParallelState *);
    pData->pDevInsGC   = PDMDEVINS_2_GCPTR(pDevIns);
}

/** @copyfrom PIBASE::pfnqueryInterface */
static DECLCALLBACK(void *) parallelQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    ParallelState *pData = PDMIBASE_2_PARALLELSTATE(pInterface);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pData->IBase;
        case PDMINTERFACE_HOST_DEVICE_PORT:
            return &pData->IHostDevicePort;
        default:
            return NULL;
    }
}

/**
 * Destruct a device instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(int) parallelDestruct(PPDMDEVINS pDevIns)
{
    ParallelState *pData = PDMINS2DATA(pDevIns, ParallelState *);

    RTSemEventDestroy(pData->ReceiveSem);
    pData->ReceiveSem = NIL_RTSEMEVENT;

    PDMR3CritSectDelete(&pData->CritSect);
    return VINF_SUCCESS;
}


/**
 * Construct a device instance for a VM.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 *                      If the registration structure is needed, pDevIns->pDevReg points to it.
 * @param   iInstance   Instance number. Use this to figure out which registers and such to use.
 *                      The device number is also found in pDevIns->iInstance, but since it's
 *                      likely to be freqently used PDM passes it as parameter.
 * @param   pCfgHandle  Configuration node handle for the device. Use this to obtain the configuration
 *                      of the device instance. It's also found in pDevIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) parallelConstruct(PPDMDEVINS pDevIns,
                                           int iInstance,
                                           PCFGMNODE pCfgHandle)
{
    int            rc;
    ParallelState   *pData = PDMINS2DATA(pDevIns, ParallelState*);
    uint16_t       io_base;
    uint8_t        irq_lvl;

    Assert(iInstance < 4);

    pData->pDevInsHC = pDevIns;
    pData->pDevInsGC = PDMDEVINS_2_GCPTR(pDevIns);

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "IRQ\0IOBase\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    rc = CFGMR3QueryBool(pCfgHandle, "GCEnabled", &pData->fGCEnabled);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->fGCEnabled = true;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"GCEnabled\" value"));

    rc = CFGMR3QueryBool(pCfgHandle, "R0Enabled", &pData->fR0Enabled);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->fR0Enabled = true;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"R0Enabled\" value"));

    /* IBase */
    pData->IBase.pfnQueryInterface = parallelQueryInterface;

    /* ICharPort */
    /* pData->ICharPort.pfnNotifyRead = parallelNotifyRead; */

    rc = RTSemEventCreate(&pData->ReceiveSem);
    AssertRC(rc);

    /*
     * Initialize critical section.
     * This must of course be done before attaching drivers or anything else which can call us back..
     */
    char szName[24];
    RTStrPrintf(szName, sizeof(szName), "Parallel#%d", iInstance);
    rc = PDMDevHlpCritSectInit(pDevIns, &pData->CritSect, szName);
    if (VBOX_FAILURE(rc))
        return rc;

/** @todo r=bird: Check for VERR_CFGM_VALUE_NOT_FOUND and provide sensible defaults.
 * Also do AssertMsgFailed(("Configuration error:....)) in the failure cases of CFGMR3Query*()
 * and CFGR3AreValuesValid() like we're doing in the other devices.  */
    rc = CFGMR3QueryU8(pCfgHandle, "IRQ", &irq_lvl);
    if (VBOX_FAILURE(rc))
        return rc;

    rc = CFGMR3QueryU16(pCfgHandle, "IOBase", &io_base);
    if (VBOX_FAILURE(rc))
        return rc;

    Log(("parallelConstruct instance %d iobase=%04x irq=%d\n", iInstance, io_base, irq_lvl));

    pData->irq = irq_lvl;
    pData->reg_status = LPT_STATUS_BUSY | LPT_STATUS_IRQ;
    pData->reg_control = LPT_CONTROL_STROBE | LPT_CONTROL_AUTO_LINEFEED | LPT_CONTROL_SELECT_PRINTER;
    pData->base = io_base;
    rc = PDMDevHlpIOPortRegister(pDevIns, io_base, 8, 0,
                                 parallelIOPortWrite, parallelIOPortRead,
                                 NULL, NULL, "PARALLEL");
    if (VBOX_FAILURE (rc))
        return rc;

    if (pData->fGCEnabled)
        rc = PDMDevHlpIOPortRegisterGC(pDevIns, io_base, 8, 0, "parallelIOPortWrite",
                                      "parallelIOPortRead", NULL, NULL, "Parallel");

    if (pData->fR0Enabled)
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, io_base, 8, 0, "parallelIOPortWrite",
                                      "parallelIOPortRead", NULL, NULL, "Parallel");

    /* Attach the char driver and get the interfaces. For now no run-time
     * changes are supported. */
    rc = PDMDevHlpDriverAttach(pDevIns, 0, &pData->IBase, &pData->pDrvBase, "Parallel Host");
    if (VBOX_SUCCESS(rc))
    {
        pData->pDrvHostDeviceConnector = (PDMIHOSTDEVICECONNECTOR *)pData->pDrvBase->pfnQueryInterface(pData->pDrvBase, PDMINTERFACE_HOST_DEVICE_CONNECTOR);
        if (!pData->pDrvHostDeviceConnector)
        {
            AssertMsgFailed(("Configuration error: instance %d has no char interface!\n", iInstance));
            return VERR_PDM_MISSING_INTERFACE;
        }
        /** @todo provide read notification interface!!!! */
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
    {
        pData->pDrvBase = NULL;
        pData->pDrvHostDeviceConnector = NULL;
        LogRel(("Parallel%d: no unit\n", iInstance));
    }
    else
    {
        AssertMsgFailed(("Parallel%d: Failed to attach to host driver. rc=%Vrc\n", iInstance, rc));
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("Parallel device %d cannot attach to host driver\n"), iInstance);
    }

    rc = PDMDevHlpSSMRegister(
        pDevIns,                        /* pDevIns */
        pDevIns->pDevReg->szDeviceName, /* pszName */
        iInstance,                      /* u32Instance */
        PARALLEL_SAVED_STATE_VERSION,   /* u32Version */
        sizeof (*pData),                /* cbGuess */
        NULL,                           /* pfnSavePrep */
        parallelSaveExec,               /* pfnSaveExec */
        NULL,                           /* pfnSaveDone */
        NULL,                           /* pfnLoadPrep */
        parallelLoadExec,               /* pfnLoadExec */
        NULL                            /* pfnLoadDone */
        );
    if (VBOX_FAILURE(rc))
        return rc;

    return VINF_SUCCESS;
}

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceParallelPort =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szDeviceName */
    "parallel",
    /* szGCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "Parallel Communication Port",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_PARALLEL,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(ParallelState),
    /* pfnConstruct */
    parallelConstruct,
    /* pfnDestruct */
    parallelDestruct,
    /* pfnRelocate */
    parallelRelocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnQueryInterface. */
    NULL
};
#endif /* IN_RING3 */


#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */
