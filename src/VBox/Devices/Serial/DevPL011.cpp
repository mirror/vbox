/* $Id$ */
/** @file
 * DevSerialPL011 - ARM PL011 PrimeCell UART.
 *
 * The documentation for this device was taken from
 * https://developer.arm.com/documentation/ddi0183/g/programmers-model/summary-of-registers (2023-03-21).
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
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

/** The current serial code saved state version. */
#define PL011_SAVED_STATE_VERSION               1

/** PL011 MMIO region size in bytes. */
#define PL011_MMIO_SIZE                         _4K

/** The offset of the UARTDR register from the beginning of the region. */
#define PL011_REG_UARTDR_INDEX                  0x0
/** Framing error. */
# define PL011_REG_UARTDR_FE                    RT_BIT(8)
/** Parity error. */
# define PL011_REG_UARTDR_PE                    RT_BIT(9)
/** Break error. */
# define PL011_REG_UARTDR_BE                    RT_BIT(10)
/** Overrun error. */
# define PL011_REG_UARTDR_OE                    RT_BIT(11)

/** The offset of the UARTRSR/UARTECR register from the beginning of the region. */
#define PL011_REG_UARTRSR_ECR_INDEX             0x4
/** Framing error. */
# define PL011_REG_UARTRSR_ECR_FE               RT_BIT(0)
/** Parity error. */
# define PL011_REG_UARTRSR_ECR_PE               RT_BIT(1)
/** Break error. */
# define PL011_REG_UARTRSR_ECR_BE               RT_BIT(2)
/** Overrun error. */
# define PL011_REG_UARTRSR_ECR_OE               RT_BIT(3)

/** The offset of the UARTFR register from the beginning of the region. */
#define PL011_REG_UARTFR_INDEX                  0x18
/** Clear to send. */
# define PL011_REG_UARTFR_CTS                   RT_BIT(0)
/** Data set ready. */
# define PL011_REG_UARTFR_DSR                   RT_BIT(1)
/** Data carrier detect. */
# define PL011_REG_UARTFR_DCD                   RT_BIT(2)
/** UART busy. */
# define PL011_REG_UARTFR_BUSY                  RT_BIT(3)
/** Receive FIFO empty. */
# define PL011_REG_UARTFR_RXFE                  RT_BIT(4)
/** Transmit FIFO full. */
# define PL011_REG_UARTFR_TXFF                  RT_BIT(5)
/** Receive FIFO full. */
# define PL011_REG_UARTFR_RXFF                  RT_BIT(6)
/** Transmit FIFO empty. */
# define PL011_REG_UARTFR_TXFE                  RT_BIT(7)
/** Ring indicator. */
# define PL011_REG_UARTFR_RI                    RT_BIT(8)

/** The offset of the UARTILPR register from the beginning of the region. */
#define PL011_REG_UARTILPR_INDEX                0x20

/** The offset of the UARTIBRD register from the beginning of the region. */
#define PL011_REG_UARTIBRD_INDEX                0x24

/** The offset of the UARTFBRD register from the beginning of the region. */
#define PL011_REG_UARTFBRD_INDEX                0x28

/** The offset of the UARTLCR_H register from the beginning of the region. */
#define PL011_REG_UARTLCR_H_INDEX               0x2c
/** Send break. */
# define PL011_REG_UARTLCR_H_BRK                RT_BIT(0)
/** Parity enable. */
# define PL011_REG_UARTLCR_H_PEN                RT_BIT(1)
/** Even parity select. */
# define PL011_REG_UARTLCR_H_EPS                RT_BIT(2)
/** Two stop bits select. */
# define PL011_REG_UARTLCR_H_STP2               RT_BIT(3)
/** Enable FIFOs. */
# define PL011_REG_UARTLCR_H_FEN                RT_BIT(4)
/** Word length. */
# define PL011_REG_UARTLCR_H_WLEN               (RT_BIT(5) | RT_BIT(6))
# define PL011_REG_UARTLCR_H_WLEN_GET(a_Lcr)    (((a_Lcr) & PL011_REG_UARTLCR_H_WLEN) >> 5)
# define PL011_REG_UARTLCR_H_WLEN_SET(a_Wlen)   (((a_Wlen) << 5) & PL011_REG_UARTLCR_H_WLEN)
/** 5 bits word length. */
#  define PL011_REG_UARTLCR_H_WLEN_5BITS        0
/** 6 bits word length. */
#  define PL011_REG_UARTLCR_H_WLEN_6BITS        1
/** 7 bits word length. */
#  define PL011_REG_UARTLCR_H_WLEN_7BITS        2
/** 8 bits word length. */
#  define PL011_REG_UARTLCR_H_WLEN_8BITS        3
/** Stick parity select. */
# define PL011_REG_UARTLCR_H_SPS                RT_BIT(7)

/** The offset of the UARTCR register from the beginning of the region. */
#define PL011_REG_UARTCR_INDEX                  0x30
/** UART transmit enable flag. */
# define PL011_REG_UARTCR_TXE                   RT_BIT(8)
/** UART receive enable flag. */
# define PL011_REG_UARTCR_RXE                   RT_BIT(9)

/** The offset of the UARTIFLS register from the beginning of the region. */
#define PL011_REG_UARTIFLS_INDEX                0x34

/** The offset of the UARTIMSC register from the beginning of the region. */
#define PL011_REG_UARTIMSC_INDEX                0x38

/** The offset of the UARTRIS register from the beginning of the region. */
#define PL011_REG_UARTRIS_INDEX                 0x3c

/** The offset of the UARTMIS register from the beginning of the region. */
#define PL011_REG_UARTMIS_INDEX                 0x40

/** The offset of the UARTICR register from the beginning of the region. */
#define PL011_REG_UARTICR_INDEX                 0x44

/** The offset of the UARTDMACR register from the beginning of the region. */
#define PL011_REG_UARTDMACR_INDEX               0x48

/** The offset of the UARTPeriphID0 register from the beginning of the region. */
#define PL011_REG_UART_PERIPH_ID0_INDEX         0xfe0
/** The offset of the UARTPeriphID1 register from the beginning of the region. */
#define PL011_REG_UART_PERIPH_ID1_INDEX         0xfe4
/** The offset of the UARTPeriphID2 register from the beginning of the region. */
#define PL011_REG_UART_PERIPH_ID2_INDEX         0xfe8
/** The offset of the UARTPeriphID3 register from the beginning of the region. */
#define PL011_REG_UART_PERIPH_ID3_INDEX         0xfec
/** The offset of the UARTPCellID0 register from the beginning of the region. */
#define PL011_REG_UART_PCELL_ID0_INDEX          0xff0
/** The offset of the UARTPCellID1 register from the beginning of the region. */
#define PL011_REG_UART_PCELL_ID1_INDEX          0xff4
/** The offset of the UARTPCellID2 register from the beginning of the region. */
#define PL011_REG_UART_PCELL_ID2_INDEX          0xff8
/** The offset of the UARTPCellID3 register from the beginning of the region. */
#define PL011_REG_UART_PCELL_ID3_INDEX          0xffc


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * Shared serial device state.
 */
typedef struct DEVPL011
{
    /** The MMIO handle. */
    IOMMMIOHANDLE                   hMmio;
    /** The base MMIO address the device is registered at. */
    RTGCPHYS                        GCPhysMmioBase;
    /** The IRQ value. */
    uint16_t                        u16Irq;

    /** @name Registers.
     * @{ */
    /** UART control register. */
    uint32_t                        uRegCr;
    /** UART flag register. */
    uint32_t                        uRegFr;
    /** @} */
} DEVPL011;
/** Pointer to the shared serial device state. */
typedef DEVPL011 *PDEVPL011;


/**
 * Serial device state for ring-3.
 */
typedef struct DEVPL011R3
{
    /** LUN\#0: The base interface. */
    PDMIBASE                        IBase;
    /** LUN\#0: The serial port interface. */
    PDMISERIALPORT                  ISerialPort;
    /** Pointer to the attached base driver. */
    R3PTRTYPE(PPDMIBASE)            pDrvBase;
    /** Pointer to the attached serial driver. */
    R3PTRTYPE(PPDMISERIALCONNECTOR) pDrvSerial;
    /** Pointer to the device instance - only for getting our bearings in
     *  interface methods. */
    PPDMDEVINS                      pDevIns;
} DEVPL011R3;
/** Pointer to the serial device state for ring-3. */
typedef DEVPL011R3 *PDEVPL011R3;


/**
 * Serial device state for ring-0.
 */
typedef struct DEVPL011R0
{
    /** Dummy .*/
    uint8_t                         bDummy;
} DEVPL011R0;
/** Pointer to the serial device state for ring-0. */
typedef DEVPL011R0 *PDEVPL011R0;


/**
 * Serial device state for raw-mode.
 */
typedef struct DEVPL011RC
{
    /** Dummy .*/
    uint8_t                         bDummy;
} DEVPL011RC;
/** Pointer to the serial device state for raw-mode. */
typedef DEVPL011RC *PDEVPL011RC;

/** The serial device state for the current context. */
typedef CTX_SUFF(DEVPL011) DEVPL011CC;
/** Pointer to the serial device state for the current context. */
typedef CTX_SUFF(PDEVPL011) PDEVPL011CC;


#ifndef VBOX_DEVICE_STRUCT_TESTCASE



/* -=-=-=-=-=- MMIO callbacks -=-=-=-=-=- */


/**
 * @callback_method_impl{FNIOMMMIONEWREAD}
 */
static DECLCALLBACK(VBOXSTRICTRC) pl011MmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void *pv, unsigned cb)
{
    PDEVPL011 pThis = PDMDEVINS_2_DATA(pDevIns, PDEVPL011);
    NOREF(pvUser);
    Assert(cb == 4 || cb == 8);
    Assert(!(off & (cb - 1)));

    LogFlowFunc(("%RGp cb=%u\n", off, cb));

    uint32_t u32Val = 0;
    VBOXSTRICTRC rc = VINF_SUCCESS;
    switch (off)
    {
        case PL011_REG_UARTFR_INDEX:
            u32Val = pThis->uRegFr;
            break;
        case PL011_REG_UARTCR_INDEX:
            u32Val = pThis->uRegCr;
            break;
        case PL011_REG_UART_PERIPH_ID0_INDEX:
            u32Val = 0x11;
            break;
        case PL011_REG_UART_PERIPH_ID1_INDEX:
            u32Val = 0x10;
            break;
        case PL011_REG_UART_PERIPH_ID2_INDEX:
            u32Val = 0x34; /* r1p5 */
            break;
        case PL011_REG_UART_PERIPH_ID3_INDEX:
            u32Val = 0x00;
            break;
        case PL011_REG_UART_PCELL_ID0_INDEX:
            u32Val = 0x0d;
            break;
        case PL011_REG_UART_PCELL_ID1_INDEX:
            u32Val = 0xf0;
            break;
        case PL011_REG_UART_PCELL_ID2_INDEX:
            u32Val = 0x05;
            break;
        case PL011_REG_UART_PCELL_ID3_INDEX:
            u32Val = 0xb1;
            break;
        default:
            break;
    }

    if (rc == VINF_SUCCESS)
        *(uint32_t *)pv = u32Val;

    return rc;
}


/**
 * @callback_method_impl{FNIOMMMIONEWWRITE}
 */
static DECLCALLBACK(VBOXSTRICTRC) pl011MmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb)
{
    PDEVPL011 pThis = PDMDEVINS_2_DATA(pDevIns, PDEVPL011);
    LogFlowFunc(("cb=%u reg=%RGp val=%llx\n", cb, off, cb == 4 ? *(uint32_t *)pv : cb == 8 ? *(uint64_t *)pv : 0xdeadbeef));
    RT_NOREF(pvUser);
    Assert(cb == 4 || cb == 8);
    Assert(!(off & (cb - 1)));

    int rc = VINF_SUCCESS;
    return rc;
}


#ifdef IN_RING3

/* -=-=-=-=-=-=-=-=- PDMISERIALPORT on LUN#0 -=-=-=-=-=-=-=-=- */


/**
 * @interface_method_impl{PDMISERIALPORT,pfnDataAvailRdrNotify}
 */
static DECLCALLBACK(int) pl011R3DataAvailRdrNotify(PPDMISERIALPORT pInterface, size_t cbAvail)
{
    LogFlowFunc(("pInterface=%#p cbAvail=%zu\n", pInterface, cbAvail));
    PDEVPL011CC pThisCC = RT_FROM_MEMBER(pInterface, DEVPL011CC, ISerialPort);
    PPDMDEVINS  pDevIns = pThisCC->pDevIns;
    PDEVPL011   pThis = PDMDEVINS_2_DATA(pDevIns, PDEVPL011);

    AssertMsg((uint32_t)cbAvail == cbAvail, ("Too much data available\n"));

    int const rcLock = PDMDevHlpCritSectEnter(pDevIns, pDevIns->pCritSectRoR3, VERR_IGNORED);
    PDM_CRITSECT_RELEASE_ASSERT_RC_DEV(pDevIns, pDevIns->pCritSectRoR3, rcLock);

    /** @todo */
    RT_NOREF(pThis);

    PDMDevHlpCritSectLeave(pDevIns, pDevIns->pCritSectRoR3);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMISERIALPORT,pfnDataSentNotify}
 */
static DECLCALLBACK(int) pl011R3DataSentNotify(PPDMISERIALPORT pInterface)
{
    LogFlowFunc(("pInterface=%#p\n", pInterface));
    PDEVPL011CC pThisCC = RT_FROM_MEMBER(pInterface, DEVPL011CC, ISerialPort);
    PPDMDEVINS  pDevIns = pThisCC->pDevIns;
    PDEVPL011   pThis = PDMDEVINS_2_DATA(pDevIns, PDEVPL011);

    /* Set the transmitter empty bit because everything was sent. */
    int const rcLock = PDMDevHlpCritSectEnter(pDevIns, pDevIns->pCritSectRoR3, VERR_IGNORED);
    PDM_CRITSECT_RELEASE_ASSERT_RC_DEV(pDevIns, pDevIns->pCritSectRoR3, rcLock);

    /** @todo */
    RT_NOREF(pThis);

    PDMDevHlpCritSectLeave(pDevIns, pDevIns->pCritSectRoR3);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMISERIALPORT,pfnReadWr}
 */
static DECLCALLBACK(int) pl011R3ReadWr(PPDMISERIALPORT pInterface, void *pvBuf, size_t cbRead, size_t *pcbRead)
{
    LogFlowFunc(("pInterface=%#p pvBuf=%#p cbRead=%zu pcbRead=%#p\n", pInterface, pvBuf, cbRead, pcbRead));
    PDEVPL011CC pThisCC = RT_FROM_MEMBER(pInterface, DEVPL011CC, ISerialPort);
    PPDMDEVINS  pDevIns = pThisCC->pDevIns;
    PDEVPL011   pThis = PDMDEVINS_2_DATA(pDevIns, PDEVPL011);

    AssertReturn(cbRead > 0, VERR_INVALID_PARAMETER);

    int const rcLock = PDMDevHlpCritSectEnter(pDevIns, pDevIns->pCritSectRoR3, VERR_IGNORED);
    PDM_CRITSECT_RELEASE_ASSERT_RC_DEV(pDevIns, pDevIns->pCritSectRoR3, rcLock);

    /** @todo */
    RT_NOREF(pThis);

    PDMDevHlpCritSectLeave(pDevIns, pDevIns->pCritSectRoR3);
    LogFlowFunc(("-> VINF_SUCCESS{*pcbRead=%zu}\n", *pcbRead));
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMISERIALPORT,pfnNotifyStsLinesChanged}
 */
static DECLCALLBACK(int) pl011R3NotifyStsLinesChanged(PPDMISERIALPORT pInterface, uint32_t fNewStatusLines)
{
    LogFlowFunc(("pInterface=%#p fNewStatusLines=%#x\n", pInterface, fNewStatusLines));
    PDEVPL011CC pThisCC = RT_FROM_MEMBER(pInterface, DEVPL011CC, ISerialPort);
    PPDMDEVINS  pDevIns = pThisCC->pDevIns;
    PDEVPL011   pThis = PDMDEVINS_2_DATA(pDevIns, PDEVPL011);

    int const rcLock = PDMDevHlpCritSectEnter(pDevIns, pDevIns->pCritSectRoR3, VERR_IGNORED);
    PDM_CRITSECT_RELEASE_ASSERT_RC_DEV(pDevIns, pDevIns->pCritSectRoR3, rcLock);

    /** @todo */
    RT_NOREF(pThis);

    PDMDevHlpCritSectLeave(pDevIns, pDevIns->pCritSectRoR3);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMISERIALPORT,pfnNotifyBrk}
 */
static DECLCALLBACK(int) pl011R3NotifyBrk(PPDMISERIALPORT pInterface)
{
    LogFlowFunc(("pInterface=%#p\n", pInterface));
    PDEVPL011CC pThisCC = RT_FROM_MEMBER(pInterface, DEVPL011CC, ISerialPort);
    PPDMDEVINS  pDevIns = pThisCC->pDevIns;
    PDEVPL011   pThis = PDMDEVINS_2_DATA(pDevIns, PDEVPL011);

    int const rcLock = PDMDevHlpCritSectEnter(pDevIns, pDevIns->pCritSectRoR3, VERR_IGNORED);
    PDM_CRITSECT_RELEASE_ASSERT_RC_DEV(pDevIns, pDevIns->pCritSectRoR3, rcLock);

    /** @todo */
    RT_NOREF(pThis);

    PDMDevHlpCritSectLeave(pDevIns, pDevIns->pCritSectRoR3);
    return VINF_SUCCESS;
}


/* -=-=-=-=-=-=-=-=- PDMIBASE -=-=-=-=-=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) pl011R3QueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PDEVPL011CC pThisCC = RT_FROM_MEMBER(pInterface, DEVPL011CC, IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThisCC->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMISERIALPORT, &pThisCC->ISerialPort);
    return NULL;
}


/* -=-=-=-=-=-=-=-=- Saved State -=-=-=-=-=-=-=-=- */

/**
 * @callback_method_impl{FNSSMDEVLIVEEXEC}
 */
static DECLCALLBACK(int) pl011R3LiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    PDEVPL011       pThis = PDMDEVINS_2_DATA(pDevIns, PDEVPL011);
    PCPDMDEVHLPR3   pHlp  = pDevIns->pHlpR3;
    RT_NOREF(uPass);

    pHlp->pfnSSMPutU16(pSSM, pThis->u16Irq);
    pHlp->pfnSSMPutGCPhys(pSSM, pThis->GCPhysMmioBase);
    return VINF_SSM_DONT_CALL_AGAIN;
}


/**
 * @callback_method_impl{FNSSMDEVSAVEEXEC}
 */
static DECLCALLBACK(int) pl011R3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PDEVPL011       pThis = PDMDEVINS_2_DATA(pDevIns, PDEVPL011);
    PCPDMDEVHLPR3   pHlp  = pDevIns->pHlpR3;

    pHlp->pfnSSMPutU16(pSSM, pThis->u16Irq);
    pHlp->pfnSSMPutGCPhys(pSSM, pThis->GCPhysMmioBase);

    return pHlp->pfnSSMPutU32(pSSM, UINT32_MAX); /* sanity/terminator */
}


/**
 * @callback_method_impl{FNSSMDEVLOADEXEC}
 */
static DECLCALLBACK(int) pl011R3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PDEVPL011       pThis = PDMDEVINS_2_DATA(pDevIns, PDEVPL011);
    PCPDMDEVHLPR3   pHlp  = pDevIns->pHlpR3;
    uint16_t        u16Irq;
    RTGCPHYS        GCPhysMmioBase;
    int rc;

    RT_NOREF(uVersion);

    pHlp->pfnSSMGetU16(   pSSM, &u16Irq);
    pHlp->pfnSSMGetGCPhys(pSSM, &GCPhysMmioBase);
    if (uPass == SSM_PASS_FINAL)
    {
        rc = VERR_NOT_IMPLEMENTED;
        AssertRCReturn(rc, rc);
    }

    if (uPass == SSM_PASS_FINAL)
    {
        /* The marker. */
        uint32_t u32;
        rc = pHlp->pfnSSMGetU32(pSSM, &u32);
        AssertRCReturn(rc, rc);
        AssertMsgReturn(u32 == UINT32_MAX, ("%#x\n", u32), VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
    }

    /*
     * Check the config.
     */
    if (   pThis->u16Irq         != u16Irq
        || pThis->GCPhysMmioBase != GCPhysMmioBase)
        return pHlp->pfnSSMSetCfgError(pSSM, RT_SRC_POS,
                                       N_("Config mismatch - saved Irq=%#x GCPhysMmioBase=%#RGp; configured Irq=%#x GCPhysMmioBase=%#RGp"),
                                       u16Irq, GCPhysMmioBase, pThis->u16Irq, pThis->GCPhysMmioBase);

    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNSSMDEVLOADDONE}
 */
static DECLCALLBACK(int) pl011R3LoadDone(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PDEVPL011   pThis   = PDMDEVINS_2_DATA(pDevIns, PDEVPL011);
    PDEVPL011CC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PDEVPL011CC);

    RT_NOREF(pThis, pThisCC, pSSM);
    return VERR_NOT_IMPLEMENTED;
}


/* -=-=-=-=-=-=-=-=- PDMDEVREG -=-=-=-=-=-=-=-=- */

/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) pl011R3Reset(PPDMDEVINS pDevIns)
{
    PDEVPL011   pThis   = PDMDEVINS_2_DATA(pDevIns, PDEVPL011);
    PDEVPL011CC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PDEVPL011CC);

    pThis->uRegCr = PL011_REG_UARTCR_TXE | PL011_REG_UARTCR_RXE;
    pThis->uRegFr = PL011_REG_UARTFR_TXFE | PL011_REG_UARTFR_RXFE;
    /** @todo */
    RT_NOREF(pThisCC);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnAttach}
 */
static DECLCALLBACK(int) pl011R3Attach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PDEVPL011CC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PDEVPL011CC);
    RT_NOREF(fFlags);
    AssertReturn(iLUN == 0, VERR_PDM_LUN_NOT_FOUND);

    int rc = PDMDevHlpDriverAttach(pDevIns, iLUN, &pThisCC->IBase, &pThisCC->pDrvBase, "PL011 Char");
    if (RT_SUCCESS(rc))
    {
        pThisCC->pDrvSerial = PDMIBASE_QUERY_INTERFACE(pThisCC->pDrvBase, PDMISERIALCONNECTOR);
        if (!pThisCC->pDrvSerial)
        {
            AssertLogRelMsgFailed(("Configuration error: instance %d has no serial interface!\n", pDevIns->iInstance));
            return VERR_PDM_MISSING_INTERFACE;
        }
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
    {
        pThisCC->pDrvBase = NULL;
        pThisCC->pDrvSerial = NULL;
        LogRel(("PL011#%d: no unit\n", pDevIns->iInstance));
    }
    else /* Don't call VMSetError here as we assume that the driver already set an appropriate error */
        LogRel(("PL011#%d: Failed to attach to serial driver. rc=%Rrc\n", pDevIns->iInstance, rc));

   return rc;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDetach}
 */
static DECLCALLBACK(void) pl011R3Detach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PDEVPL011CC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PDEVPL011CC);
    RT_NOREF(fFlags);
    AssertReturnVoid(iLUN == 0);

    /* Zero out important members. */
    pThisCC->pDrvBase   = NULL;
    pThisCC->pDrvSerial = NULL;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) pl011R3Destruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);

    /* Nothing to do. */
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) pl011R3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PDEVPL011       pThis   = PDMDEVINS_2_DATA(pDevIns, PDEVPL011);
    PDEVPL011CC     pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PDEVPL011CC);
    PCPDMDEVHLPR3   pHlp    = pDevIns->pHlpR3;
    int             rc;

    Assert(iInstance < 4);

    pThisCC->pDevIns                                = pDevIns;

    /* IBase */
    pThisCC->IBase.pfnQueryInterface                = pl011R3QueryInterface;

    /* ISerialPort */
    pThisCC->ISerialPort.pfnDataAvailRdrNotify      = pl011R3DataAvailRdrNotify;
    pThisCC->ISerialPort.pfnDataSentNotify          = pl011R3DataSentNotify;
    pThisCC->ISerialPort.pfnReadWr                  = pl011R3ReadWr;
    pThisCC->ISerialPort.pfnNotifyStsLinesChanged   = pl011R3NotifyStsLinesChanged;
    pThisCC->ISerialPort.pfnNotifyBrk               = pl011R3NotifyBrk;

    /*
     * Validate and read the configuration.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "Irq|MmioBase|YieldOnLSRRead", "");

    bool fYieldOnLSRRead = false;
    rc = pHlp->pfnCFGMQueryBoolDef(pCfg, "YieldOnLSRRead", &fYieldOnLSRRead, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to get the \"YieldOnLSRRead\" value"));

    uint16_t u16Irq = 0;
    rc = pHlp->pfnCFGMQueryU16(pCfg, "Irq", &u16Irq);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to get the \"Irq\" value"));

    RTGCPHYS GCPhysMmioBase = 0;
    rc = pHlp->pfnCFGMQueryU64(pCfg, "MmioBase", &GCPhysMmioBase);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"IOBase\" value"));

    pThis->u16Irq         = u16Irq;
    pThis->GCPhysMmioBase = GCPhysMmioBase;

    /*
     * Register and map the MMIO region.
     */
    rc = PDMDevHlpMmioCreateAndMap(pDevIns, GCPhysMmioBase, PL011_MMIO_SIZE, pl011MmioWrite, pl011MmioRead,
                                   IOMMMIO_FLAGS_READ_DWORD | IOMMMIO_FLAGS_WRITE_DWORD_ZEROED, "PL011", &pThis->hMmio);
    AssertRCReturn(rc, rc);


    /*
     * Saved state.
     */
    rc = PDMDevHlpSSMRegisterEx(pDevIns, PL011_SAVED_STATE_VERSION, sizeof(*pThis), NULL,
                                NULL, pl011R3LiveExec, NULL,
                                NULL, pl011R3SaveExec, NULL,
                                NULL, pl011R3LoadExec, pl011R3LoadDone);
    AssertRCReturn(rc, rc);

    /*
     * Attach the char driver and get the interfaces.
     */
    rc = PDMDevHlpDriverAttach(pDevIns, 0 /*iLUN*/, &pThisCC->IBase, &pThisCC->pDrvBase, "UART");
    if (RT_SUCCESS(rc))
    {
        pThisCC->pDrvSerial = PDMIBASE_QUERY_INTERFACE(pThisCC->pDrvBase, PDMISERIALCONNECTOR);
        if (!pThisCC->pDrvSerial)
        {
            AssertLogRelMsgFailed(("Configuration error: instance %d has no serial interface!\n", iInstance));
            return VERR_PDM_MISSING_INTERFACE;
        }
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
    {
        pThisCC->pDrvBase   = NULL;
        pThisCC->pDrvSerial = NULL;
        LogRel(("PL011#%d: no unit\n", iInstance));
    }
    else
    {
        AssertLogRelMsgFailed(("PL011#%d: Failed to attach to char driver. rc=%Rrc\n", iInstance, rc));
        /* Don't call VMSetError here as we assume that the driver already set an appropriate error */
        return rc;
    }

    pl011R3Reset(pDevIns);
    return VINF_SUCCESS;
}

#else  /* !IN_RING3 */

/**
 * @callback_method_impl{PDMDEVREGR0,pfnConstruct}
 */
static DECLCALLBACK(int) pl011RZConstruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PDEVPL011   pThis   = PDMDEVINS_2_DATA(pDevIns, PDEVPL011);
    PDEVPL011CC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PDEVPL011CC);

    int rc = PDMDevHlpMmioSetUpContext(pDevIns, pThis->hMmio, pl011MmioWrite, pl011MmioRead, NULL /*pvUser*/);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}

#endif /* !IN_RING3 */

/**
 * The device registration structure.
 */
const PDMDEVREG g_DevicePl011 =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "arm-pl011",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RZ | PDM_DEVREG_FLAGS_NEW_STYLE,
    /* .fClass = */                 PDM_DEVREG_CLASS_SERIAL,
    /* .cMaxInstances = */          UINT32_MAX,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(DEVPL011),
    /* .cbInstanceCC = */           sizeof(DEVPL011CC),
    /* .cbInstanceRC = */           sizeof(DEVPL011RC),
    /* .cMaxPciDevices = */         0,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "ARM PL011 PrimeCell UART",
#if defined(IN_RING3)
    /* .pszRCMod = */               "VBoxDDRC.rc",
    /* .pszR0Mod = */               "VBoxDDR0.r0",
    /* .pfnConstruct = */           pl011R3Construct,
    /* .pfnDestruct = */            pl011R3Destruct,
    /* .pfnRelocate = */            NULL,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               pl011R3Reset,
    /* .pfnSuspend = */             NULL,
    /* .pfnResume = */              NULL,
    /* .pfnAttach = */              pl011R3Attach,
    /* .pfnDetach = */              pl011R3Detach,
    /* .pfnQueryInterface = */      NULL,
    /* .pfnInitComplete = */        NULL,
    /* .pfnPowerOff = */            NULL,
    /* .pfnSoftReset = */           NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RING0)
    /* .pfnEarlyConstruct = */      NULL,
    /* .pfnConstruct = */           pl011RZConstruct,
    /* .pfnDestruct = */            NULL,
    /* .pfnFinalDestruct = */       NULL,
    /* .pfnRequest = */             NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RC)
    /* .pfnConstruct = */           pl011RZConstruct,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#else
# error "Not in IN_RING3, IN_RING0 or IN_RC!"
#endif
    /* .u32VersionEnd = */          PDM_DEVREG_VERSION
};

#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

