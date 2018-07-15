/* $Id$ */
/** @file
 * UartCore - UART  (16550A up to 16950) emulation.
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
#include <iprt/uuid.h>
#include <iprt/assert.h>

#include "VBoxDD.h"
#include "UartCore.h"


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
# define UART_REG_IIR_ID_SET(a_Val)          (((a_Val) << 1) & UART_REG_IIR_ID_MASK)
/** Receiver Line Status interrupt. */
#  define UART_REG_IIR_ID_RCL                0x3
/** Received Data Available interrupt. */
#  define UART_REG_IIR_ID_RDA                0x2
/** Character Timeou Indicator interrupt. */
#  define UART_REG_IIR_ID_CTI                0x6
/** Transmitter Holding Register Empty interrupt. */
#  define UART_REG_IIR_ID_THRE               0x1
/** Modem Status interrupt. */
#  define UART_REG_IIR_ID_MS                 0x0
/** FIFOs enabled. */
# define UART_REG_IIR_FIFOS_EN               0xc0
/** Bits relevant for checking whether the interrupt status has changed. */
# define UART_REG_IIR_CHANGED_MASK           0x0f

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
/** RCV Interrupt trigger level - 1 byte. */
# define UART_REG_FCR_RCV_LVL_IRQ_1          0x0
/** RCV Interrupt trigger level - 4 bytes. */
# define UART_REG_FCR_RCV_LVL_IRQ_4          0x1
/** RCV Interrupt trigger level - 8 bytes. */
# define UART_REG_FCR_RCV_LVL_IRQ_8          0x2
/** RCV Interrupt trigger level - 14 bytes. */
# define UART_REG_FCR_RCV_LVL_IRQ_14         0x3
/** Mask of writeable bits. */
# define UART_REG_FCR_MASK_WR                0xcf
/** Mask of sticky bits. */
# define UART_REG_FCR_MASK_STICKY            0xc9

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
/** The bits to check in this register when checking for the RCL interrupt. */
# define UART_REG_LSR_BITS_IIR_RCL           0x1e

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
# define UART_REG_MSR_DCD                    RT_BIT(7)
/** The bits to check in this register when checking for the MS interrupt. */
# define UART_REG_MSR_BITS_IIR_MS            0x0f

/** The SCR register index (from the base of the port range). */
#define UART_REG_SCR_INDEX                   7

/** Set the specified bits in the given register. */
#define UART_REG_SET(a_Reg, a_Set)           ((a_Reg) |= (a_Set))
/** Clear the specified bits in the given register. */
#define UART_REG_CLR(a_Reg, a_Clr)           ((a_Reg) &= ~(a_Clr))


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

#ifndef VBOX_DEVICE_STRUCT_TESTCASE


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#ifdef IN_RING3
/**
 * String versions of the parity enum.
 */
static const char *s_aszParity[] =
{
    "INVALID",
    "NONE",
    "EVEN",
    "ODD",
    "MARK",
    "SPACE",
    "INVALID"
};


/**
 * String versions of the stop bits enum.
 */
static const char *s_aszStopBits[] =
{
    "INVALID",
    "1",
    "1.5",
    "2",
    "INVALID"
};
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/


/**
 * Updates the IRQ state based on the current device state.
 *
 * @returns nothing.
 * @param   pThis               The UART core instance.
 */
static void uartIrqUpdate(PUARTCORE pThis)
{
    LogFlowFunc(("pThis=%#p\n", pThis));

    /*
     * The interrupt uses a priority scheme, only the interrupt with the
     * highest priority is indicated in the interrupt identification register.
     *
     * The priorities are as follows (high to low):
     *      * Receiver line status
     *      * Received data available
     *      * Character timeout indication (only in FIFO mode).
     *      * Transmitter holding register empty
     *      * Modem status change.
     */
    uint8_t uRegIirNew = UART_REG_IIR_IP_NO_INT;
    if (   (pThis->uRegLsr & UART_REG_LSR_BITS_IIR_RCL)
        && (pThis->uRegIer & UART_REG_IER_ELSI))
        uRegIirNew = UART_REG_IIR_ID_SET(UART_REG_IIR_ID_RCL);
    else if (   (pThis->uRegLsr & UART_REG_LSR_DR)
             && (pThis->uRegIer & UART_REG_IER_ERBFI)
             && (   !(pThis->uRegFcr & UART_REG_FCR_FIFO_EN)
                 || pThis->FifoRecv.cbUsed >= pThis->FifoRecv.cbItl))
        uRegIirNew = UART_REG_IIR_ID_SET(UART_REG_IIR_ID_RDA);
    else if (   (pThis->uRegLsr & UART_REG_LSR_THRE)
             && (pThis->uRegIer & UART_REG_IER_ETBEI))
        uRegIirNew = UART_REG_IIR_ID_SET(UART_REG_IIR_ID_THRE);
    else if (   (pThis->uRegMsr & UART_REG_MSR_BITS_IIR_MS)
             && (pThis->uRegIer & UART_REG_IER_EDSSI))
        uRegIirNew = UART_REG_IIR_ID_SET(UART_REG_IIR_ID_MS);

    /** @todo Character timeout indication for FIFO mode. */

    LogFlowFunc(("    uRegIirNew=%#x uRegIir=%#x\n", uRegIirNew, pThis->uRegIir));

    /* Change interrupt only if the interrupt status really changed from the previous value. */
    if (uRegIirNew != (pThis->uRegIir & UART_REG_IIR_CHANGED_MASK))
    {
        LogFlow(("    Interrupt source changed from %#x -> %#x (IRQ %d -> %d)\n",
                 pThis->uRegIir, uRegIirNew,
                 pThis->uRegIir == UART_REG_IIR_IP_NO_INT ? 0 : 1,
                 uRegIirNew == UART_REG_IIR_IP_NO_INT ? 0 : 1));
        if (uRegIirNew == UART_REG_IIR_IP_NO_INT)
            pThis->CTX_SUFF(pfnUartIrqReq)(pThis->CTX_SUFF(pDevIns), pThis, pThis->iLUN, 0);
        else
            pThis->CTX_SUFF(pfnUartIrqReq)(pThis->CTX_SUFF(pDevIns), pThis, pThis->iLUN, 1);
    }
    else
        LogFlow(("    No change in interrupt source\n"));

    if (pThis->uRegFcr & UART_REG_FCR_FIFO_EN)
        uRegIirNew |= UART_REG_IIR_FIFOS_EN;

    pThis->uRegIir = uRegIirNew;
}


/**
 * Clears the given FIFO.
 *
 * @returns nothing.
 * @param   pFifo               The FIFO to clear.
 */
DECLINLINE(void) uartFifoClear(PUARTFIFO pFifo)
{
    memset(&pFifo->abBuf[0], 0, sizeof(pFifo->abBuf));
    pFifo->cbUsed   = 0;
    pFifo->offWrite = 0;
    pFifo->offRead  = 0;
}


/**
 * Returns the amount of free bytes in the given FIFO.
 *
 * @returns The amount of bytes free in the given FIFO.
 * @param   pFifo               The FIFO.
 */
DECLINLINE(size_t) uartFifoFreeGet(PUARTFIFO pFifo)
{
    return UART_FIFO_LENGTH - pFifo->cbUsed;
}


/**
 * Puts a new character into the given FIFO.
 *
 * @returns Flag whether the FIFO overflowed.
 * @param   pFifo               The FIFO to put the data into.
 * @param   fOvrWr              Flag whether to overwrite data if the FIFO is full.
 * @param   bData               The data to add.
 */
DECLINLINE(bool) uartFifoPut(PUARTFIFO pFifo, bool fOvrWr, uint8_t bData)
{
    if (fOvrWr || pFifo->cbUsed < UART_FIFO_LENGTH)
    {
        pFifo->abBuf[pFifo->offWrite] = bData;
        pFifo->offWrite = (pFifo->offWrite + 1) % UART_FIFO_LENGTH;
    }

    bool fOverFlow = false;
    if (pFifo->cbUsed < UART_FIFO_LENGTH)
        pFifo->cbUsed++;
    else
    {
        fOverFlow = true;
        if (fOvrWr) /* Advance the read position to account for the lost character. */
           pFifo->offRead = (pFifo->offRead + 1) % UART_FIFO_LENGTH;
    }

    return fOverFlow;
}


/**
 * Returns the next character in the FIFO.
 *
 * @return Next byte in the FIFO.
 * @param   pFifo               The FIFO to get data from.
 */
DECLINLINE(uint8_t) uartFifoGet(PUARTFIFO pFifo)
{
    uint8_t bRet = 0;

    if (pFifo->cbUsed)
    {
        bRet = pFifo->abBuf[pFifo->offRead];
        pFifo->offRead = (pFifo->offRead + 1) % UART_FIFO_LENGTH;
        pFifo->cbUsed--;
    }

    return bRet;
}


/**
 * Tries to copy the requested amount of data from the given FIFO into the provided buffer.
 *
 * @returns Amount of bytes actually copied.
 * @param   pFifo               The FIFO to copy data from.
 * @param   pvDst               Where to copy the data to.
 * @param   cbCopy              How much to copy.
 */
DECLINLINE(size_t) uartFifoCopyTo(PUARTFIFO pFifo, void *pvDst, size_t cbCopy)
{
    size_t cbCopied = 0;
    uint8_t *pbDst = (uint8_t *)pvDst;

    cbCopy = RT_MIN(cbCopy, pFifo->cbUsed);
    while (cbCopy)
    {
        size_t cbThisCopy = RT_MIN(cbCopy, (uint8_t)(UART_FIFO_LENGTH - pFifo->offRead));
        memcpy(pbDst, &pFifo->abBuf[pFifo->offRead], cbThisCopy);

        pFifo->offRead = (pFifo->offRead + cbThisCopy) % UART_FIFO_LENGTH;
        pFifo->cbUsed -= cbThisCopy;
        pbDst    += cbThisCopy;
        cbCopied += cbThisCopy;
        cbCopy   -= cbThisCopy;
    }

    return cbCopied;
}


/**
 * Tries to copy the requested amount of data from the provided buffer into the given FIFO.
 *
 * @returns Amount of bytes actually copied.
 * @param   pFifo               The FIFO to copy data to.
 * @param   pvSrc               Where to copy the data from.
 * @param   cbCopy              How much to copy.
 */
DECLINLINE(size_t) uartFifoCopyFrom(PUARTFIFO pFifo, void *pvSrc, size_t cbCopy)
{
    size_t cbCopied = 0;
    uint8_t *pbSrc = (uint8_t *)pvSrc;

    cbCopy = RT_MIN(cbCopy, uartFifoFreeGet(pFifo));
    while (cbCopy)
    {
        size_t cbThisCopy = RT_MIN(cbCopy, (uint8_t)(UART_FIFO_LENGTH - pFifo->offWrite));
        memcpy(&pFifo->abBuf[pFifo->offWrite], pbSrc, cbThisCopy);

        pFifo->offWrite = (pFifo->offWrite + cbThisCopy) % UART_FIFO_LENGTH;
        pFifo->cbUsed += cbThisCopy;
        pbSrc    += cbThisCopy;
        cbCopied += cbThisCopy;
        cbCopy   -= cbThisCopy;
    }

    return cbCopied;
}


#ifdef IN_RING3
/**
 * Updates the delta bits for the given MSR register value which has the status line
 * bits set.
 *
 * @returns nothing.
 * @param   pThis               The serial port instance.
 * @param   uMsrSts             MSR value with the appropriate status bits set.
 */
static void uartR3MsrUpdate(PUARTCORE pThis, uint8_t uMsrSts)
{
    /* Compare current and new states and set remaining bits accordingly. */
    if ((uMsrSts & UART_REG_MSR_CTS) != (pThis->uRegMsr & UART_REG_MSR_CTS))
        uMsrSts |= UART_REG_MSR_DCTS;
    if ((uMsrSts & UART_REG_MSR_DSR) != (pThis->uRegMsr & UART_REG_MSR_DSR))
        uMsrSts |= UART_REG_MSR_DDSR;
    if ((uMsrSts & UART_REG_MSR_RI) != 0 && (pThis->uRegMsr & UART_REG_MSR_RI) == 0)
        uMsrSts |= UART_REG_MSR_TERI;
    if ((uMsrSts & UART_REG_MSR_DCD) != (pThis->uRegMsr & UART_REG_MSR_DCD))
        uMsrSts |= UART_REG_MSR_DDCD;

    pThis->uRegMsr = uMsrSts;

    uartIrqUpdate(pThis);
}


/**
 * Updates the serial port parameters of the attached driver with the current configuration.
 *
 * @returns nothing.
 * @param   pThis               The serial port instance.
 */
static void uartR3ParamsUpdate(PUARTCORE pThis)
{
    if (   pThis->uRegDivisor != 0
        && pThis->pDrvSerial)
    {
        uint32_t uBps = 115200 / pThis->uRegDivisor; /* This is for PC compatible serial port with a 1.8432 MHz crystal. */
        unsigned cDataBits = UART_REG_LCR_WLS_GET(pThis->uRegLcr) + 5;
        PDMSERIALSTOPBITS enmStopBits = PDMSERIALSTOPBITS_ONE;
        PDMSERIALPARITY enmParity = PDMSERIALPARITY_NONE;

        if (pThis->uRegLcr & UART_REG_LCR_STB)
        {
            enmStopBits = cDataBits == 5 ? PDMSERIALSTOPBITS_ONEPOINTFIVE : PDMSERIALSTOPBITS_TWO;
        }

        if (pThis->uRegLcr & UART_REG_LCR_PEN)
        {
            /* Select the correct parity mode based on the even and stick parity bits. */
            switch (pThis->uRegLcr & (UART_REG_LCR_EPS | UART_REG_LCR_PAR_STICK))
            {
                case 0:
                    enmParity = PDMSERIALPARITY_ODD;
                    break;
                case UART_REG_LCR_EPS:
                    enmParity = PDMSERIALPARITY_EVEN;
                    break;
                case UART_REG_LCR_EPS | UART_REG_LCR_PAR_STICK:
                    enmParity = PDMSERIALPARITY_SPACE;
                    break;
                case UART_REG_LCR_PAR_STICK:
                    enmParity = PDMSERIALPARITY_MARK;
                    break;
                default:
                    /* We should never get here as all cases where caught earlier. */
                    AssertMsgFailed(("This shouldn't happen at all: %#x\n",
                                     pThis->uRegLcr & (UART_REG_LCR_EPS | UART_REG_LCR_PAR_STICK)));
            }
        }

        LogFlowFunc(("Changing parameters to: %u,%s,%u,%s\n",
                     uBps, s_aszParity[enmParity], cDataBits, s_aszStopBits[enmStopBits]));

        int rc = pThis->pDrvSerial->pfnChgParams(pThis->pDrvSerial, uBps, enmParity, cDataBits, enmStopBits);
        if (RT_FAILURE(rc))
            LogRelMax(10, ("Serial#%d: Failed to change parameters to %u,%s,%u,%s -> %Rrc\n",
                           pThis->pDevInsR3->iInstance, uBps, s_aszParity[enmParity], cDataBits, s_aszStopBits[enmStopBits], rc));
    }
}


/**
 * Updates the internal device state with the given PDM status line states.
 *
 * @returns nothing.
 * @param   pThis               The serial port instance.
 * @param   fStsLines           The PDM status line states.
 */
static void uartR3StsLinesUpdate(PUARTCORE pThis, uint32_t fStsLines)
{
    uint8_t uRegMsrNew = 0; /* The new MSR value. */

    if (fStsLines & PDMISERIALPORT_STS_LINE_DCD)
        uRegMsrNew |= UART_REG_MSR_DCD;
    if (fStsLines & PDMISERIALPORT_STS_LINE_RI)
        uRegMsrNew |= UART_REG_MSR_RI;
    if (fStsLines & PDMISERIALPORT_STS_LINE_DSR)
        uRegMsrNew |= UART_REG_MSR_DSR;
    if (fStsLines & PDMISERIALPORT_STS_LINE_CTS)
        uRegMsrNew |= UART_REG_MSR_CTS;

    uartR3MsrUpdate(pThis, uRegMsrNew);
}


/**
 * Fills up the receive FIFO with as much data as possible.
 *
 * @returns nothing.
 * @param   pThis               The serial port instance.
 */
static void uartR3RecvFifoFill(PUARTCORE pThis)
{
    LogFlowFunc(("pThis=%#p\n", pThis));

    PUARTFIFO pFifo = &pThis->FifoRecv;
    size_t cbFill = RT_MIN(uartFifoFreeGet(pFifo),
                           ASMAtomicReadU32(&pThis->cbAvailRdr));
    size_t cbFilled = 0;

    while (cbFilled < cbFill)
    {
        size_t cbThisRead = RT_MIN(cbFill, (uint8_t)(UART_FIFO_LENGTH - pFifo->offWrite));
        size_t cbRead = 0;
        int rc = pThis->pDrvSerial->pfnReadRdr(pThis->pDrvSerial, &pFifo->abBuf[pFifo->offWrite], cbThisRead, &cbRead);
        /*Assert(RT_SUCCESS(rc) && cbRead == cbThisRead);*/ RT_NOREF(rc);

        pFifo->offWrite = (pFifo->offWrite + cbRead) % UART_FIFO_LENGTH;
        pFifo->cbUsed   += cbRead;
        cbFilled        += cbRead;

        if (cbRead < cbThisRead)
            break;
    }

    if (cbFilled)
    {
        UART_REG_SET(pThis->uRegLsr, UART_REG_LSR_DR);
        uartIrqUpdate(pThis);
    }

    ASMAtomicSubU32(&pThis->cbAvailRdr, cbFilled);
}


/**
 * Fetches a single byte and writes it to RBR.
 *
 * @returns nothing.
 * @param   pThis               The serial port instance.
 */
static void uartR3ByteFetch(PUARTCORE pThis)
{
    if (ASMAtomicReadU32(&pThis->cbAvailRdr))
    {
        AssertPtr(pThis->pDrvSerial);
        size_t cbRead = 0;
        int rc2 = pThis->pDrvSerial->pfnReadRdr(pThis->pDrvSerial, &pThis->uRegRbr, 1, &cbRead);
        AssertMsg(RT_SUCCESS(rc2) && cbRead == 1, ("This shouldn't fail and always return one byte!\n"));
        UART_REG_SET(pThis->uRegLsr, UART_REG_LSR_DR);
        uartIrqUpdate(pThis);
    }
}


/**
 * Fetches a ready data based on the FIFO setting.
 *
 * @returns nothing.
 * @param   pThis               The serial port instance.
 */
static void uartR3DataFetch(PUARTCORE pThis)
{
    if (pThis->uRegFcr % UART_REG_FCR_FIFO_EN)
        uartR3RecvFifoFill(pThis);
    else
        uartR3ByteFetch(pThis);
}
#endif


/**
 * Write handler for the THR/DLL register (depending on the DLAB bit in LCR).
 *
 * @returns VBox status code.
 * @param   pThis               The serial port instance.
 * @param   uVal                The value to write.
 */
DECLINLINE(int) uartRegThrDllWrite(PUARTCORE pThis, uint8_t uVal)
{
    int rc = VINF_SUCCESS;

    /* A set DLAB causes a write to the lower 8bits of the divisor latch. */
    if (pThis->uRegLcr & UART_REG_LCR_DLAB)
    {
        if (uVal != (pThis->uRegDivisor & 0xff))
        {
#ifndef IN_RING3
            rc = VINF_IOM_R3_IOPORT_WRITE;
#else
            pThis->uRegDivisor = (pThis->uRegDivisor & 0xff00) | uVal;
            uartR3ParamsUpdate(pThis);
#endif
        }
    }
    else
    {
        if (pThis->uRegFcr & UART_REG_FCR_FIFO_EN)
        {
#ifndef IN_RING3
            rc = VINF_IOM_R3_IOPORT_WRITE;
#else
            uartFifoPut(&pThis->FifoXmit, true /*fOvrWr*/, uVal);
            UART_REG_CLR(pThis->uRegLsr, UART_REG_LSR_THRE | UART_REG_LSR_TEMT);
            uartIrqUpdate(pThis);
            if (pThis->pDrvSerial)
            {
                int rc2 = pThis->pDrvSerial->pfnDataAvailWrNotify(pThis->pDrvSerial, 1);
                if (RT_FAILURE(rc2))
                    LogRelMax(10, ("Serial#%d: Failed to send data with %Rrc\n", rc2));
            }
#endif
        }
        else
        {
            /* Notify the lower driver about available data only if the register was empty before. */
            if (pThis->uRegLsr & UART_REG_LSR_THRE)
            {
#ifndef IN_RING3
                rc = VINF_IOM_R3_IOPORT_WRITE;
#else
                pThis->uRegThr = uVal;
                UART_REG_CLR(pThis->uRegLsr, UART_REG_LSR_THRE | UART_REG_LSR_TEMT);
                uartIrqUpdate(pThis);
                if (pThis->pDrvSerial)
                {
                    int rc2 = pThis->pDrvSerial->pfnDataAvailWrNotify(pThis->pDrvSerial, 1);
                    if (RT_FAILURE(rc2))
                        LogRelMax(10, ("Serial#%d: Failed to send data with %Rrc\n", rc2));
                }
#endif
            }
            else
                pThis->uRegThr = uVal;
        }
    }

    return rc;
}


/**
 * Write handler for the IER/DLM register (depending on the DLAB bit in LCR).
 *
 * @returns VBox status code.
 * @param   pThis               The serial port instance.
 * @param   uVal                The value to write.
 */
DECLINLINE(int) uartRegIerDlmWrite(PUARTCORE pThis, uint8_t uVal)
{
    int rc = VINF_SUCCESS;

    /* A set DLAB causes a write to the higher 8bits of the divisor latch. */
    if (pThis->uRegLcr & UART_REG_LCR_DLAB)
    {
        if (uVal != (pThis->uRegDivisor & 0xff00) >> 8)
        {
#ifndef IN_RING3
            rc = VINF_IOM_R3_IOPORT_WRITE;
#else
            pThis->uRegDivisor = (pThis->uRegDivisor & 0xff) | (uVal << 8);
            uartR3ParamsUpdate(pThis);
#endif
        }
    }
    else
    {
        pThis->uRegIer = uVal & UART_REG_IER_MASK_WR;
        uartIrqUpdate(pThis);
    }

    return rc;
}


/**
 * Write handler for the FCR register.
 *
 * @returns VBox status code.
 * @param   pThis               The serial port instance.
 * @param   uVal                The value to write.
 */
DECLINLINE(int) uartRegFcrWrite(PUARTCORE pThis, uint8_t uVal)
{
    int rc = VINF_SUCCESS;

    if (   pThis->enmType >= UARTTYPE_16550A
        && uVal != pThis->uRegFcr)
    {
        /* A change in the FIFO enable bit clears both FIFOs automatically. */
        if ((uVal ^ pThis->uRegFcr) & UART_REG_FCR_FIFO_EN)
        {
            uartFifoClear(&pThis->FifoXmit);
            uartFifoClear(&pThis->FifoRecv);

            /* Fill in the next data. */
            if (ASMAtomicReadU32(&pThis->cbAvailRdr))
            {
#ifndef IN_RING3
                rc = VINF_IOM_R3_IOPORT_WRITE;
#else
                uartR3DataFetch(pThis);
#endif
            }
        }

        if (rc == VINF_SUCCESS)
        {
            if (uVal & UART_REG_FCR_RCV_FIFO_RST)
                uartFifoClear(&pThis->FifoRecv);
            if (uVal & UART_REG_FCR_XMIT_FIFO_RST)
                uartFifoClear(&pThis->FifoXmit);

            if (uVal & UART_REG_FCR_FIFO_EN)
            {
                switch (UART_REG_FCR_RCV_LVL_IRQ_GET(uVal))
                {
                    case UART_REG_FCR_RCV_LVL_IRQ_1:
                        pThis->FifoRecv.cbItl = 1;
                        break;
                    case UART_REG_FCR_RCV_LVL_IRQ_4:
                        pThis->FifoRecv.cbItl = 4;
                        break;
                    case UART_REG_FCR_RCV_LVL_IRQ_8:
                        pThis->FifoRecv.cbItl = 8;
                        break;
                    case UART_REG_FCR_RCV_LVL_IRQ_14:
                        pThis->FifoRecv.cbItl = 14;
                        break;
                    default:
                        /* Should never occur as all cases are handled earlier. */
                        AssertMsgFailed(("Impossible to hit!\n"));
                }
            }

            /* The FIFO reset bits are self clearing. */
            pThis->uRegFcr = uVal & UART_REG_FCR_MASK_STICKY;
            uartIrqUpdate(pThis);
        }
    }

    return rc;
}


/**
 * Write handler for the LCR register.
 *
 * @returns VBox status code.
 * @param   pThis               The serial port instance.
 * @param   uVal                The value to write.
 */
DECLINLINE(int) uartRegLcrWrite(PUARTCORE pThis, uint8_t uVal)
{
    int rc = VINF_SUCCESS;

    /* Any change except the DLAB bit causes a switch to R3. */
    if ((pThis->uRegLcr & ~UART_REG_LCR_DLAB) != (uVal & ~UART_REG_LCR_DLAB))
    {
#ifndef IN_RING3
        rc = VINF_IOM_R3_IOPORT_WRITE;
#else
        /* Check whether the BREAK bit changed before updating the LCR value. */
        bool fBrkEn = RT_BOOL(uVal & UART_REG_LCR_BRK_SET);
        bool fBrkChg = fBrkEn != RT_BOOL(pThis->uRegLcr & UART_REG_LCR_BRK_SET);
        pThis->uRegLcr = uVal;
        uartR3ParamsUpdate(pThis);

        if (   fBrkChg
            && pThis->pDrvSerial)
            pThis->pDrvSerial->pfnChgBrk(pThis->pDrvSerial, fBrkEn);
#endif
    }
    else
        pThis->uRegLcr = uVal;

    return rc;
}


/**
 * Write handler for the MCR register.
 *
 * @returns VBox status code.
 * @param   pThis               The serial port instance.
 * @param   uVal                The value to write.
 */
DECLINLINE(int) uartRegMcrWrite(PUARTCORE pThis, uint8_t uVal)
{
    int rc = VINF_SUCCESS;

    uVal &= UART_REG_MCR_MASK_WR;
    if (pThis->uRegMcr != uVal)
    {
#ifndef IN_RING3
        rc = VINF_IOM_R3_IOPORT_WRITE;
#else
        /*
         * When loopback mode is activated the RTS, DTR, OUT1 and OUT2 lines are
         * disconnected and looped back to MSR.
         */
        if (   (uVal & UART_REG_MCR_LOOP)
            && !(pThis->uRegMcr & UART_REG_MCR_LOOP)
            && pThis->pDrvSerial)
            pThis->pDrvSerial->pfnChgModemLines(pThis->pDrvSerial, false /*fRts*/, false /*fDtr*/);

        pThis->uRegMcr = uVal;
        if (uVal & UART_REG_MCR_LOOP)
        {
            uint8_t uRegMsrSts = 0;

            if (uVal & UART_REG_MCR_RTS)
                uRegMsrSts |= UART_REG_MSR_CTS;
            if (uVal & UART_REG_MCR_DTR)
                uRegMsrSts |= UART_REG_MSR_DSR;
            if (uVal & UART_REG_MCR_OUT1)
                uRegMsrSts |= UART_REG_MSR_RI;
            if (uVal & UART_REG_MCR_OUT2)
                uRegMsrSts |= UART_REG_MSR_DCD;
            uartR3MsrUpdate(pThis, uRegMsrSts);
        }
        else if (pThis->pDrvSerial)
            pThis->pDrvSerial->pfnChgModemLines(pThis->pDrvSerial,
                                                RT_BOOL(uVal & UART_REG_MCR_RTS),
                                                RT_BOOL(uVal & UART_REG_MCR_DTR));
#endif
    }

    return rc;
}


/**
 * Read handler for the RBR/DLL register (depending on the DLAB bit in LCR).
 *
 * @returns VBox status code.
 * @param   pThis               The serial port instance.
 * @param   puVal               Where to store the read value on success.
 */
DECLINLINE(int) uartRegRbrDllRead(PUARTCORE pThis, uint32_t *puVal)
{
    int rc = VINF_SUCCESS;

    /* A set DLAB causes a read from the lower 8bits of the divisor latch. */
    if (pThis->uRegLcr & UART_REG_LCR_DLAB)
        *puVal = pThis->uRegDivisor & 0xff;
    else
    {
        if (pThis->uRegFcr & UART_REG_FCR_FIFO_EN)
        {
            /*
             * Only go back to R3 if there is new data available for the FIFO
             * and we would clear the interrupt to fill it up again.
             */
            if (   pThis->FifoRecv.cbUsed <= pThis->FifoRecv.cbItl
                && ASMAtomicReadU32(&pThis->cbAvailRdr) > 0)
            {
#ifndef IN_RING3
                rc = VINF_IOM_R3_IOPORT_READ;
#else
                uartR3RecvFifoFill(pThis);
#endif
            }

            if (rc == VINF_SUCCESS)
            {
                *puVal = uartFifoGet(&pThis->FifoRecv);
                if (!pThis->FifoRecv.cbUsed)
                    UART_REG_CLR(pThis->uRegLsr, UART_REG_LSR_DR);
                uartIrqUpdate(pThis);
            }
        }
        else
        {
            *puVal = pThis->uRegRbr;

            if (pThis->uRegLsr & UART_REG_LSR_DR)
            {
                uint32_t cbAvail = ASMAtomicDecU32(&pThis->cbAvailRdr);
                if (!cbAvail)
                {
                    UART_REG_CLR(pThis->uRegLsr, UART_REG_LSR_DR);
                    uartIrqUpdate(pThis);
                }
                else
                {
#ifndef IN_RING3
                    /* Restore state and go back to R3. */
                    ASMAtomicIncU32(&pThis->cbAvailRdr);
                    rc = VINF_IOM_R3_IOPORT_READ;
#else
                    /* Fetch new data and keep the DR bit set. */
                    uartR3DataFetch(pThis);
#endif
                }
            }
        }
    }

    return rc;
}


/**
 * Read handler for the IER/DLM register (depending on the DLAB bit in LCR).
 *
 * @returns VBox status code.
 * @param   pThis               The serial port instance.
 * @param   puVal               Where to store the read value on success.
 */
DECLINLINE(int) uartRegIerDlmRead(PUARTCORE pThis, uint32_t *puVal)
{
    int rc = VINF_SUCCESS;

    /* A set DLAB causes a read from the upper 8bits of the divisor latch. */
    if (pThis->uRegLcr & UART_REG_LCR_DLAB)
        *puVal = (pThis->uRegDivisor & 0xff00) >> 8;
    else
        *puVal = pThis->uRegIer;

    return rc;
}


/**
 * Read handler for the IIR register.
 *
 * @returns VBox status code.
 * @param   pThis               The serial port instance.
 * @param   puVal               Where to store the read value on success.
 */
DECLINLINE(int) uartRegIirRead(PUARTCORE pThis, uint32_t *puVal)
{
    *puVal = pThis->uRegIir;
    return VINF_SUCCESS;
}


/**
 * Read handler for the LSR register.
 *
 * @returns VBox status code.
 * @param   pThis               The serial port instance.
 * @param   puVal               Where to store the read value on success.
 */
DECLINLINE(int) uartRegLsrRead(PUARTCORE pThis, uint32_t *puVal)
{
    int rc = VINF_SUCCESS;

    /* Yield if configured and there is no data available. */
    if (   !(pThis->uRegLsr & UART_REG_LSR_DR)
        && (pThis->fFlags & UART_CORE_YIELD_ON_LSR_READ))
    {
#ifndef IN_RING3
        return VINF_IOM_R3_IOPORT_READ;
#else
        RTThreadYield();
#endif
    }

    *puVal = pThis->uRegLsr;
    /*
     * Reading this register clears the Overrun (OE), Parity (PE) and Framing (FE) error
     * as well as the Break Interrupt (BI).
     */
    UART_REG_CLR(pThis->uRegLsr, UART_REG_LSR_BITS_IIR_RCL);
    uartIrqUpdate(pThis);

    return rc;
}


/**
 * Read handler for the MSR register.
 *
 * @returns VBox status code.
 * @param   pThis               The serial port instance.
 * @param   puVal               Where to store the read value on success.
 */
DECLINLINE(int) uartRegMsrRead(PUARTCORE pThis, uint32_t *puVal)
{
    *puVal = pThis->uRegMsr;

    /* Clear any of the delta bits. */
    UART_REG_CLR(pThis->uRegMsr, UART_REG_MSR_BITS_IIR_MS);
    uartIrqUpdate(pThis);
    return VINF_SUCCESS;
}


#ifdef LOG_ENABLED
/**
 * Converts the register index into a sensible memnonic.
 *
 * @returns Register memnonic.
 * @param   pThis               The serial port instance.
 * @param   idxReg              Register index.
 * @param   fWrite              Flag whether the register gets written.
 */
DECLINLINE(const char *) uartRegIdx2Str(PUARTCORE pThis, uint8_t idxReg, bool fWrite)
{
    const char *psz = "INV";

    switch (idxReg)
    {
        /*case UART_REG_THR_DLL_INDEX:*/
        case UART_REG_RBR_DLL_INDEX:
            if (pThis->uRegLcr & UART_REG_LCR_DLAB)
                psz = "DLL";
            else if (fWrite)
                psz = "THR";
            else
                psz = "RBR";
            break;
        case UART_REG_IER_DLM_INDEX:
            if (pThis->uRegLcr & UART_REG_LCR_DLAB)
                psz = "DLM";
            else
                psz = "IER";
            break;
        /*case UART_REG_IIR_INDEX:*/
        case UART_REG_FCR_INDEX:
            if (fWrite)
                psz = "FCR";
            else
                psz = "IIR";
            break;
        case UART_REG_LCR_INDEX:
            psz = "LCR";
            break;
        case UART_REG_MCR_INDEX:
            psz = "MCR";
            break;
        case UART_REG_LSR_INDEX:
            psz = "LSR";
            break;
        case UART_REG_MSR_INDEX:
            psz = "MSR";
            break;
        case UART_REG_SCR_INDEX:
            psz = "SCR";
            break;
    }

    return psz;
}
#endif


DECLHIDDEN(int) uartRegWrite(PUARTCORE pThis, uint32_t uReg, uint32_t u32, size_t cb)
{
    int rc = PDMCritSectEnter(&pThis->CritSect, VINF_IOM_R3_IOPORT_WRITE);
    if (rc != VINF_SUCCESS)
        return rc;

    uint8_t idxReg = uReg & 0x7;
    LogFlowFunc(("pThis=%#p uReg=%u{%s} u32=%#x cb=%u\n",
                 pThis, uReg, uartRegIdx2Str(pThis, idxReg, true /*fWrite*/), u32, cb));

    AssertMsgReturn(cb == 1, ("uReg=%#x cb=%d u32=%#x\n", uReg, cb, u32), VINF_SUCCESS);

    uint8_t uVal = (uint8_t)u32;
    switch (idxReg)
    {
        case UART_REG_THR_DLL_INDEX:
            rc = uartRegThrDllWrite(pThis, uVal);
            break;
        case UART_REG_IER_DLM_INDEX:
            rc = uartRegIerDlmWrite(pThis, uVal);
            break;
        case UART_REG_FCR_INDEX:
            rc = uartRegFcrWrite(pThis, uVal);
            break;
        case UART_REG_LCR_INDEX:
            rc = uartRegLcrWrite(pThis, uVal);
            break;
        case UART_REG_MCR_INDEX:
            rc = uartRegMcrWrite(pThis, uVal);
            break;
        case UART_REG_SCR_INDEX:
            pThis->uRegScr = u32;
            break;
        default:
            break;
    }

    PDMCritSectLeave(&pThis->CritSect);
    LogFlowFunc(("-> %Rrc\n", rc));
    return rc;
}


DECLHIDDEN(int) uartRegRead(PUARTCORE pThis, uint32_t uReg, uint32_t *pu32, size_t cb)
{
    if (cb != 1)
        return VERR_IOM_IOPORT_UNUSED;

    int rc = PDMCritSectEnter(&pThis->CritSect, VINF_IOM_R3_IOPORT_READ);
    if (rc != VINF_SUCCESS)
        return rc;

    uint8_t idxReg = uReg & 0x7;
    switch (idxReg)
    {
        case UART_REG_RBR_DLL_INDEX:
            rc = uartRegRbrDllRead(pThis, pu32);
            break;
        case UART_REG_IER_DLM_INDEX:
            rc = uartRegIerDlmRead(pThis, pu32);
            break;
        case UART_REG_IIR_INDEX:
            rc = uartRegIirRead(pThis, pu32);
            break;
        case UART_REG_LCR_INDEX:
            *pu32 = pThis->uRegLcr;
            break;
        case UART_REG_MCR_INDEX:
            *pu32 = pThis->uRegMcr;
            break;
        case UART_REG_LSR_INDEX:
            rc = uartRegLsrRead(pThis, pu32);
            break;
        case UART_REG_MSR_INDEX:
            rc = uartRegMsrRead(pThis, pu32);
            break;
        case UART_REG_SCR_INDEX:
            *pu32 = pThis->uRegScr;
            break;
        default:
            rc = VERR_IOM_IOPORT_UNUSED;
    }

    PDMCritSectLeave(&pThis->CritSect);
    LogFlowFunc(("pThis=%#p uReg=%u{%s} u32=%#x cb=%u -> %Rrc\n",
                 pThis, uReg, uartRegIdx2Str(pThis, idxReg, false /*fWrite*/), *pu32, cb, rc));
    return rc;
}


#ifdef IN_RING3

/* -=-=-=-=-=-=-=-=- PDMISERIALPORT on LUN#0 -=-=-=-=-=-=-=-=- */


/**
 * @interface_method_impl{PDMISERIALPORT,pfnDataAvailRdrNotify}
 */
static DECLCALLBACK(int) uartR3DataAvailRdrNotify(PPDMISERIALPORT pInterface, size_t cbAvail)
{
    LogFlowFunc(("pInterface=%#p cbAvail=%zu\n", pInterface, cbAvail));
    PUARTCORE pThis = RT_FROM_MEMBER(pInterface, UARTCORE, ISerialPort);

    AssertMsg((uint32_t)cbAvail == cbAvail, ("Too much data available\n"));

    uint32_t cbAvailOld = ASMAtomicAddU32(&pThis->cbAvailRdr, (uint32_t)cbAvail);
    LogFlow(("    cbAvailRdr=%zu -> cbAvailRdr=%zu\n", cbAvailOld, cbAvail + cbAvailOld));
    PDMCritSectEnter(&pThis->CritSect, VERR_IGNORED);
    if (pThis->uRegFcr & UART_REG_FCR_FIFO_EN)
        uartR3RecvFifoFill(pThis);
    else if (!cbAvailOld)
    {
        size_t cbRead = 0;
        int rc = pThis->pDrvSerial->pfnReadRdr(pThis->pDrvSerial, &pThis->uRegRbr, 1, &cbRead);
        AssertMsg(RT_SUCCESS(rc) && cbRead == 1, ("This shouldn't fail and always return one byte!\n"));
        UART_REG_SET(pThis->uRegLsr, UART_REG_LSR_DR);
        uartIrqUpdate(pThis);
    }
    PDMCritSectLeave(&pThis->CritSect);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMISERIALPORT,pfnDataSentNotify}
 */
static DECLCALLBACK(int) uartR3DataSentNotify(PPDMISERIALPORT pInterface)
{
    LogFlowFunc(("pInterface=%#p\n", pInterface));
    PUARTCORE pThis = RT_FROM_MEMBER(pInterface, UARTCORE, ISerialPort);

    /* Set the transmitter empty bit because everything was sent. */
    PDMCritSectEnter(&pThis->CritSect, VERR_IGNORED);
    UART_REG_SET(pThis->uRegLsr, UART_REG_LSR_TEMT);
    uartIrqUpdate(pThis);
    PDMCritSectLeave(&pThis->CritSect);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMISERIALPORT,pfnReadWr}
 */
static DECLCALLBACK(int) uartR3ReadWr(PPDMISERIALPORT pInterface, void *pvBuf, size_t cbRead, size_t *pcbRead)
{
    LogFlowFunc(("pInterface=%#p pvBuf=%#p cbRead=%zu pcbRead=%#p\n", pInterface, pvBuf, cbRead, pcbRead));
    PUARTCORE pThis = RT_FROM_MEMBER(pInterface, UARTCORE, ISerialPort);

    AssertReturn(cbRead > 0, VERR_INVALID_PARAMETER);

    PDMCritSectEnter(&pThis->CritSect, VERR_IGNORED);
    if (pThis->uRegFcr & UART_REG_FCR_FIFO_EN)
    {
        *pcbRead = uartFifoCopyTo(&pThis->FifoXmit, pvBuf, cbRead);
        if (!pThis->FifoXmit.cbUsed)
            UART_REG_SET(pThis->uRegLsr, UART_REG_LSR_THRE);
        if (*pcbRead)
            UART_REG_CLR(pThis->uRegLsr, UART_REG_LSR_TEMT);
        uartIrqUpdate(pThis);
    }
    else if (!(pThis->uRegLsr & UART_REG_LSR_THRE))
    {
        *(uint8_t *)pvBuf = pThis->uRegThr;
        *pcbRead = 1;
        UART_REG_SET(pThis->uRegLsr, UART_REG_LSR_THRE);
        UART_REG_CLR(pThis->uRegLsr, UART_REG_LSR_TEMT);
        uartIrqUpdate(pThis);
    }
    else
    {
        AssertMsgFailed(("There is no data to read!\n"));
        *pcbRead = 0;
    }
    PDMCritSectLeave(&pThis->CritSect);

    LogFlowFunc(("-> VINF_SUCCESS{*pcbRead=%zu}\n", *pcbRead));
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMISERIALPORT,pfnNotifyStsLinesChanged}
 */
static DECLCALLBACK(int) uartR3NotifyStsLinesChanged(PPDMISERIALPORT pInterface, uint32_t fNewStatusLines)
{
    LogFlowFunc(("pInterface=%#p fNewStatusLines=%#x\n", pInterface, fNewStatusLines));
    PUARTCORE pThis = RT_FROM_MEMBER(pInterface, UARTCORE, ISerialPort);

    PDMCritSectEnter(&pThis->CritSect, VERR_IGNORED);
    uartR3StsLinesUpdate(pThis, fNewStatusLines);
    PDMCritSectLeave(&pThis->CritSect);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMISERIALPORT,pfnNotifyBrk}
 */
static DECLCALLBACK(int) uartR3NotifyBrk(PPDMISERIALPORT pInterface)
{
    LogFlowFunc(("pInterface=%#p\n", pInterface));
    PUARTCORE pThis = RT_FROM_MEMBER(pInterface, UARTCORE, ISerialPort);

    PDMCritSectEnter(&pThis->CritSect, VERR_IGNORED);
    UART_REG_SET(pThis->uRegLsr, UART_REG_LSR_BI);
    uartIrqUpdate(pThis);
    PDMCritSectLeave(&pThis->CritSect);
    return VINF_SUCCESS;
}


/* -=-=-=-=-=-=-=-=- PDMIBASE -=-=-=-=-=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) uartR3QueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PUARTCORE pThis = RT_FROM_MEMBER(pInterface, UARTCORE, IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMISERIALPORT, &pThis->ISerialPort);
    return NULL;
}


DECLHIDDEN(void) uartR3Relocate(PUARTCORE pThis, RTGCINTPTR offDelta)
{
    RT_NOREF(offDelta);
    pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pThis->pDevInsR3);
}


DECLHIDDEN(void) uartR3Reset(PUARTCORE pThis)
{
    pThis->uRegDivisor = 0x0c; /* Default to 9600 Baud. */
    pThis->uRegRbr     = 0;
    pThis->uRegThr     = 0;
    pThis->uRegIer     = 0;
    pThis->uRegIir     = UART_REG_IIR_IP_NO_INT;
    pThis->uRegFcr     = 0;
    pThis->uRegLcr     = 0; /* 5 data bits, no parity, 1 stop bit. */
    pThis->uRegMcr     = 0;
    pThis->uRegLsr     = UART_REG_LSR_THRE | UART_REG_LSR_TEMT;
    pThis->uRegMsr     = 0; /* Updated below. */
    pThis->uRegScr     = 0;

    uartFifoClear(&pThis->FifoXmit);
    uartFifoClear(&pThis->FifoRecv);
    pThis->FifoRecv.cbItl = 1;

    uartR3ParamsUpdate(pThis);
    uartIrqUpdate(pThis);

    if (pThis->pDrvSerial)
    {
        /* Set the modem lines to reflect the current state. */
        int rc = pThis->pDrvSerial->pfnChgModemLines(pThis->pDrvSerial, false /*fRts*/, false /*fDtr*/);
        if (RT_FAILURE(rc))
            LogRel(("Serial#%d: Failed to set modem lines with %Rrc during reset\n",
                    pThis->pDevInsR3->iInstance, rc));

        uint32_t fStsLines = 0;
        rc = pThis->pDrvSerial->pfnQueryStsLines(pThis->pDrvSerial, &fStsLines);
        if (RT_SUCCESS(rc))
            uartR3StsLinesUpdate(pThis, fStsLines);
        else
            LogRel(("Serial#%d: Failed to query status line status with %Rrc during reset\n",
                    pThis->pDevInsR3->iInstance, rc));
    }
}


DECLHIDDEN(int) uartR3Attach(PUARTCORE pThis, unsigned iLUN)
{
    int rc = PDMDevHlpDriverAttach(pThis->pDevInsR3, iLUN, &pThis->IBase, &pThis->pDrvBase, "Serial Char");
    if (RT_SUCCESS(rc))
    {
        pThis->pDrvSerial = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMISERIALCONNECTOR);
        if (!pThis->pDrvSerial)
        {
            AssertLogRelMsgFailed(("Configuration error: instance %d has no serial interface!\n", pThis->pDevInsR3->iInstance));
            return VERR_PDM_MISSING_INTERFACE;
        }
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
    {
        pThis->pDrvBase = NULL;
        pThis->pDrvSerial = NULL;
        rc = VINF_SUCCESS;
        LogRel(("Serial#%d: no unit\n", pThis->pDevInsR3->iInstance));
    }
    else /* Don't call VMSetError here as we assume that the driver already set an appropriate error */
        LogRel(("Serial#%d: Failed to attach to serial driver. rc=%Rrc\n", pThis->pDevInsR3->iInstance, rc));

   return rc;
}


DECLHIDDEN(void) uartR3Detach(PUARTCORE pThis)
{
    /* Zero out important members. */
    pThis->pDrvBase   = NULL;
    pThis->pDrvSerial = NULL;
}


DECLHIDDEN(void) uartR3Destruct(PUARTCORE pThis)
{
    PDMR3CritSectDelete(&pThis->CritSect);
}


DECLHIDDEN(int) uartR3Init(PUARTCORE pThis, PPDMDEVINS pDevInsR3, UARTTYPE enmType, unsigned iLUN, uint32_t fFlags,
                           R3PTRTYPE(PFNUARTCOREIRQREQ) pfnUartIrqReqR3, R0PTRTYPE(PFNUARTCOREIRQREQ) pfnUartIrqReqR0,
                           RCPTRTYPE(PFNUARTCOREIRQREQ) pfnUartIrqReqRC)
{
    int rc = VINF_SUCCESS;

    /*
     * Initialize the instance data.
     * (Do this early or the destructor might choke on something!)
     */
    pThis->pDevInsR3                            = pDevInsR3;
    pThis->pDevInsR0                            = PDMDEVINS_2_R0PTR(pDevInsR3);
    pThis->pDevInsRC                            = PDMDEVINS_2_RCPTR(pDevInsR3);
    pThis->iLUN                                 = iLUN;
    pThis->enmType                              = enmType;
    pThis->fFlags                               = fFlags;
    pThis->pfnUartIrqReqR3                      = pfnUartIrqReqR3;
    pThis->pfnUartIrqReqR0                      = pfnUartIrqReqR0;
    pThis->pfnUartIrqReqRC                      = pfnUartIrqReqRC;

    /* IBase */
    pThis->IBase.pfnQueryInterface              = uartR3QueryInterface;

    /* ISerialPort */
    pThis->ISerialPort.pfnDataAvailRdrNotify    = uartR3DataAvailRdrNotify;
    pThis->ISerialPort.pfnDataSentNotify        = uartR3DataSentNotify;
    pThis->ISerialPort.pfnReadWr                = uartR3ReadWr;
    pThis->ISerialPort.pfnNotifyStsLinesChanged = uartR3NotifyStsLinesChanged;
    pThis->ISerialPort.pfnNotifyBrk             = uartR3NotifyBrk;

    rc = PDMDevHlpCritSectInit(pDevInsR3, &pThis->CritSect, RT_SRC_POS, "Serial#%d", iLUN);
    AssertRCReturn(rc, rc);

    /*
     * Attach the char driver and get the interfaces.
     */
    rc = PDMDevHlpDriverAttach(pDevInsR3, iLUN, &pThis->IBase, &pThis->pDrvBase, "UART");
    if (RT_SUCCESS(rc))
    {
        pThis->pDrvSerial = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMISERIALCONNECTOR);
        if (!pThis->pDrvSerial)
        {
            AssertLogRelMsgFailed(("Configuration error: instance %d has no serial interface!\n", iLUN));
            return VERR_PDM_MISSING_INTERFACE;
        }
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
    {
        pThis->pDrvBase   = NULL;
        pThis->pDrvSerial = NULL;
        LogRel(("Serial#%d: no unit\n", iLUN));
    }
    else
    {
        AssertLogRelMsgFailed(("Serial#%d: Failed to attach to char driver. rc=%Rrc\n", iLUN, rc));
        /* Don't call VMSetError here as we assume that the driver already set an appropriate error */
        return rc;
    }

    uartR3Reset(pThis);
    return VINF_SUCCESS;
}

#endif /* IN_RING3 */

#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */
