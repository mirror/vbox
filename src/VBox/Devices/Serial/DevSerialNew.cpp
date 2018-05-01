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

/** The RBR/DLL register index (from the base of the port range). */
#define UART_REG_RBR_DLL_INDEX               0

/** The THR/DLL register index (from the base of the port range). */
#define UART_REG_THR_DLL_INDEX               0

/** The IER/DLM register index (from the base of the port range). */
#define UART_REG_IER_DLM_INDEX               1
/** Enable received data available interrupt */
# define UART_REG_IER_ERBFI                  RT_BIT(0)
/** Enable transmitter holding register empty interrupt */
# define UART_REG_IER_ETBEI                  RT_BIT(1)
/** Enable receiver line status interrupt */
# define UART_REG_IER_ELSI                   RT_BIT(2)
/** Enable modem status interrupt. */
# define UART_REG_IER_EDSSI                  RT_BIT(3)
/** Mask of writeable bits. */
# define UART_REG_IER_MASK_WR                0x0f

/** The IIR register index (from the base of the port range). */
#define UART_REG_IIR_INDEX                   2
/** Interrupt Pending - high means no interrupt pending. */
# define UART_REG_IIR_IP_NO_INT              RT_BIT(0)
/** Interrupt identification mask. */
# define UART_REG_IIR_ID_MASK                0x0e
/** Sets the interrupt identification to the given value. */
# define UART_REG_IIR_ID_SET(a_Val)          (((a_Val) & UART_REG_IIR_ID_MASK) << 1)
/** Receiver Line Status interrupt. */
#  define UART_REG_IIR_ID_RCL                0x3
/** Received Data Avalable interrupt. */
#  define UART_REG_IIR_ID_RDA                0x2
/** Character Timeou Indicator interrupt. */
#  define UART_REG_IIR_ID_CTI                0x6
/** Transmitter Holding Register Empty interrupt. */
#  define UART_REG_IIR_ID_THRE               0x1
/** Modem Status interrupt. */
#  define UART_REG_IIR_ID_MS                 0x0
/** FIFOs enabled. */
# define UART_REG_IIR_FIFOS_EN               0xc0

/** The FCR register index (from the base of the port range). */
#define UART_REG_FCR_INDEX                   2
/** Enable the TX/RX FIFOs. */
# define UART_REG_FCR_FIFO_EN                RT_BIT(0)
/** Reset the receive FIFO. */
# define UART_REG_FCR_RCV_FIFO_RST           RT_BIT(1)
/** Reset the transmit FIFO. */
# define UART_REG_FCR_XMIT_FIFO_RST          RT_BIT(2)
/** DMA Mode Select. */
# define UART_REG_FCR_DMA_MODE_SEL           RT_BIT(3)
/** Receiver level interrupt trigger. */
# define UART_REG_FCR_RCV_LVL_IRQ_MASK       0xc0
/** Returns the receive level trigger value from the given FCR register. */
# define UART_REG_FCR_RCV_LVL_IRQ_GET(a_Fcr) (((a_Fcr) & UART_REG_FCR_RCV_LVL_IRQ_MASK) >> 6)
/** Mask of writeable bits. */
# define UART_REG_FCR_MASK_WR                0xcf

/** The LCR register index (from the base of the port range). */
#define UART_REG_LCR_INDEX                   3
/** Word Length Select Mask. */
# define UART_REG_LCR_WLS_MASK               0x3
/** Returns the WLS value form the given LCR register value. */
# define UART_REG_LCR_WLS_GET(a_Lcr)         ((a_Lcr) & UART_REG_LCR_WLS_MASK)
/** Number of stop bits. */
# define UART_REG_LCR_STB                    RT_BIT(2)
/** Parity Enable. */
# define UART_REG_LCR_PEN                    RT_BIT(3)
/** Even Parity. */
# define UART_REG_LCR_EPS                    RT_BIT(4)
/** Stick parity. */
# define UART_REG_LCR_PAR_STICK              RT_BIT(5)
/** Set Break. */
# define UART_REG_LCR_BRK_SET                RT_BIT(6)
/** Divisor Latch Access Bit. */
# define UART_REG_LCR_DLAB                   RT_BIT(7)

/** The MCR register index (from the base of the port range). */
#define UART_REG_MCR_INDEX                   4
/** Data Terminal Ready. */
# define UART_REG_MCR_DTR                    RT_BIT(0)
/** Request To Send. */
# define UART_REG_MCR_RTS                    RT_BIT(1)
/** Out1. */
# define UART_REG_MCR_OUT1                   RT_BIT(2)
/** Out2. */
# define UART_REG_MCR_OUT2                   RT_BIT(3)
/** Loopback connection. */
# define UART_REG_MCR_LOOP                   RT_BIT(4)
/** Mask of writeable bits. */
# define UART_REG_MCR_MASK_WR                0x1f

/** The LSR register index (from the base of the port range). */
#define UART_REG_LSR_INDEX                   5
/** Data Ready. */
# define UART_REG_LSR_DR                     RT_BIT(0)
/** Overrun Error. */
# define UART_REG_LSR_OE                     RT_BIT(1)
/** Parity Error. */
# define UART_REG_LSR_PE                     RT_BIT(2)
/** Framing Error. */
# define UART_REG_LSR_FE                     RT_BIT(3)
/** Break Interrupt. */
# define UART_REG_LSR_BI                     RT_BIT(4)
/** Transmitter Holding Register. */
# define UART_REG_LSR_THRE                   RT_BIT(5)
/** Transmitter Empty. */
# define UART_REG_LSR_TEMT                   RT_BIT(6)
/** Error in receiver FIFO. */
# define UART_REG_LSR_RCV_FIFO_ERR           RT_BIT(7)

/** The MSR register index (from the base of the port range). */
#define UART_REG_MSR_INDEX                   6
/** Delta Clear to Send. */
# define UART_REG_MSR_DCTS                   RT_BIT(0)
/** Delta Data Set Ready. */
# define UART_REG_MSR_DDSR                   RT_BIT(1)
/** Trailing Edge Ring Indicator. */
# define UART_REG_MSR_TERI                   RT_BIT(2)
/** Delta Data Carrier Detect. */
# define UART_REG_MSR_DDCD                   RT_BIT(3)
/** Clear to Send. */
# define UART_REG_MSR_CTS                    RT_BIT(4)
/** Data Set Ready. */
# define UART_REG_MSR_DSR                    RT_BIT(5)
/** Ring Indicator. */
# define UART_REG_MSR_RI                     RT_BIT(6)
/** Data Carrier Detect. */
# define UART_REG_MSSR_DCD                   RT_BIT(7)

/** The SCR register index (from the base of the port range). */
#define UART_REG_SCR_INDEX                   7


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

    /** The divisor register (DLAB = 1). */
    uint16_t                        uRegDivisor;
    /** The Receiver Buffer Register (RBR, DLAB = 0). */
    uint8_t                         uRegRbr;
    /** The Transmitter Holding Register (THR, DLAB = 0). */
    uint8_t                         uRegThr;
    /** The Interrupt Enable Register (IER). */
    uint8_t                         uRegIer;
    /** The Interrupt Identification Register (IIR). */
    uint8_t                         uRegIir;
    /** The FIFO Control Register (FCR). */
    uint8_t                         uRegFcr;
    /** The Line Control Register (LCR). */
    uint8_t                         uRegLcr;
    /** The Modem Control Register (MCR). */
    uint8_t                         uRegMcr;
    /** The Line Status Register (LSR). */
    uint8_t                         uRegLsr;
    /** The Modem Status Register (MSR). */
    uint8_t                         uRegMsr;
    /** The Scratch Register (SCR). */
    uint8_t                         uRegScr;

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

    AssertMsgReturn(cb == 1, ("uPort=%#x cb=%d u32=%#x\n", uPort, cb, u32), VINF_SUCCESS);

    int rc = VINF_SUCCESS;
    switch (uPort & 0x7)
    {
        case UART_REG_THR_DLL_INDEX:
            break;
        case UART_REG_IER_DLM_INDEX:
            break;
        case UART_REG_FCR_INDEX:
            break;
        case UART_REG_LCR_INDEX:
            break;
        case UART_REG_MCR_INDEX:
            break;
        case UART_REG_SCR_INDEX:
            pThis->uRegScr = u32;
            break;
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

    if (cb != 1)
        return VERR_IOM_IOPORT_UNUSED;

    int rc = VINF_SUCCESS;
    switch (uPort & 0x7)
    {
        case UART_REG_RBR_DLL_INDEX:
            break;
        case UART_REG_IER_DLM_INDEX:
            break;
        case UART_REG_IIR_INDEX:
            break;
        case UART_REG_LCR_INDEX:
            break;
        case UART_REG_MCR_INDEX:
            break;
        case UART_REG_LSR_INDEX:
            break;
        case UART_REG_MSR_INDEX:
            break;
        case UART_REG_SCR_INDEX:
            *pu32 = pThis->uRegScr;
            break;
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

#if 0 /** @todo Later */
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
