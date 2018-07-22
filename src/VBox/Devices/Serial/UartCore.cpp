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
#include <VBox/vmm/tm.h>
#include <iprt/log.h>
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
/** Sleep mode enable. */
# define UART_REG_IER_SLEEP_MODE_EN          RT_BIT(4)
/** Low power mode enable. */
# define UART_REG_IER_LP_MODE_EN             RT_BIT(5)
/** Mask of writeable bits. */
# define UART_REG_IER_MASK_WR                0x0f
/** Mask of writeable bits for 16750+. */
# define UART_REG_IER_MASK_WR_16750          0x3f

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
/** 64 byte FIFOs enabled (15750+ only). */
# define UART_REG_IIR_64BYTE_FIFOS_EN        RT_BIT(5)
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
/** 64 Byte FIFO enable (15750+ only). */
# define UART_REG_FCR_64BYTE_FIFO_EN         RT_BIT(5)
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
# define UART_REG_FCR_MASK_STICKY            0xe9

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
/** Flow Control Enable (15750+ only). */
# define UART_REG_MCR_AFE                    RT_BIT(5)
/** Mask of writeable bits (15450 and 15550A). */
# define UART_REG_MCR_MASK_WR                0x1f
/** Mask of writeable bits (15750+). */
# define UART_REG_MCR_MASK_WR_15750          0x3f

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
 * FIFO ITL levels.
 */
static struct
{
    /** ITL level for a 16byte FIFO. */
    uint8_t                     cbItl16;
    /** ITL level for a 64byte FIFO. */
    uint8_t                     cbItl64;
} s_aFifoItl[] =
{
    /* cbItl16     cbItl64 */
    {     1,          1    },
    {     4,         16    },
    {     8,         32    },
    {    14,         56    }
};


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
    else if (   (pThis->uRegIer & UART_REG_IER_ERBFI)
             && pThis->fIrqCtiPending)
        uRegIirNew = UART_REG_IIR_ID_SET(UART_REG_IIR_ID_CTI);
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
    if (pThis->uRegFcr & UART_REG_FCR_64BYTE_FIFO_EN)
        uRegIirNew |= UART_REG_IIR_64BYTE_FIFOS_EN;

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
    return pFifo->cbMax - pFifo->cbUsed;
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
    if (fOvrWr || pFifo->cbUsed < pFifo->cbMax)
    {
        pFifo->abBuf[pFifo->offWrite] = bData;
        pFifo->offWrite = (pFifo->offWrite + 1) % pFifo->cbMax;
    }

    bool fOverFlow = false;
    if (pFifo->cbUsed < pFifo->cbMax)
        pFifo->cbUsed++;
    else
    {
        fOverFlow = true;
        if (fOvrWr) /* Advance the read position to account for the lost character. */
           pFifo->offRead = (pFifo->offRead + 1) % pFifo->cbMax;
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
        pFifo->offRead = (pFifo->offRead + 1) % pFifo->cbMax;
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
        uint8_t cbThisCopy = (uint8_t)RT_MIN(cbCopy, (uint8_t)(pFifo->cbMax - pFifo->offRead));
        memcpy(pbDst, &pFifo->abBuf[pFifo->offRead], cbThisCopy);

        pFifo->offRead = (pFifo->offRead + cbThisCopy) % pFifo->cbMax;
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
        uint8_t cbThisCopy = (uint8_t)RT_MIN(cbCopy, (uint8_t)(pFifo->cbMax - pFifo->offWrite));
        memcpy(&pFifo->abBuf[pFifo->offWrite], pbSrc, cbThisCopy);

        pFifo->offWrite = (pFifo->offWrite + cbThisCopy) % pFifo->cbMax;
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
        uint32_t cFrameBits = cDataBits;
        PDMSERIALSTOPBITS enmStopBits = PDMSERIALSTOPBITS_ONE;
        PDMSERIALPARITY enmParity = PDMSERIALPARITY_NONE;

        if (pThis->uRegLcr & UART_REG_LCR_STB)
        {
            enmStopBits = cDataBits == 5 ? PDMSERIALSTOPBITS_ONEPOINTFIVE : PDMSERIALSTOPBITS_TWO;
            cFrameBits += 2;
        }
        else
            cFrameBits++;

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

            cFrameBits++;
        }

        uint64_t uTimerFreq = TMTimerGetFreq(pThis->CTX_SUFF(pTimerRcvFifoTimeout));
        pThis->cSymbolXferTicks = (uTimerFreq / uBps) * cFrameBits;

        LogFlowFunc(("Changing parameters to: %u,%s,%u,%s\n",
                     uBps, s_aszParity[enmParity], cDataBits, s_aszStopBits[enmStopBits]));

        int rc = pThis->pDrvSerial->pfnChgParams(pThis->pDrvSerial, uBps, enmParity, cDataBits, enmStopBits);
        if (RT_FAILURE(rc))
            LogRelMax(10, ("Serial#%d: Failed to change parameters to %u,%s,%u,%s -> %Rrc\n",
                           pThis->pDevInsR3->iInstance, uBps, s_aszParity[enmParity], cDataBits, s_aszStopBits[enmStopBits], rc));

        /* Changed parameters will flush all receive queues, so there won't be any data to read even if indicated. */
        ASMAtomicWriteU32(&pThis->cbAvailRdr, 0);
        UART_REG_CLR(pThis->uRegLsr, UART_REG_LSR_DR);
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
        size_t cbThisRead = RT_MIN(cbFill - cbFilled, (uint8_t)(pFifo->cbMax - pFifo->offWrite));
        size_t cbRead = 0;
        int rc = pThis->pDrvSerial->pfnReadRdr(pThis->pDrvSerial, &pFifo->abBuf[pFifo->offWrite], cbThisRead, &cbRead);
        AssertRC(rc); Assert(cbRead <= UINT8_MAX); RT_NOREF(rc);

        pFifo->offWrite = (pFifo->offWrite + (uint8_t)cbRead) % pFifo->cbMax;
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

    Assert(cbFilled <= (size_t)pThis->cbAvailRdr);
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
        AssertMsg(RT_SUCCESS(rc2) && cbRead == 1, ("This shouldn't fail and always return one byte!\n")); RT_NOREF(rc2);
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
    if (pThis->uRegFcr & UART_REG_FCR_FIFO_EN)
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
        if (pThis->enmType < UARTTYPE_16750)
            pThis->uRegIer = uVal & UART_REG_IER_MASK_WR;
        else
            pThis->uRegIer = uVal & UART_REG_IER_MASK_WR_16750;
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
#ifndef IN_RING3
    RT_NOREF(pThis, uVal);
    return VINF_IOM_R3_IOPORT_WRITE;
#else
    int rc = VINF_SUCCESS;
    if (   pThis->enmType >= UARTTYPE_16550A
        && uVal != pThis->uRegFcr)
    {
        /* A change in the FIFO enable bit clears both FIFOs automatically. */
        if ((uVal ^ pThis->uRegFcr) & UART_REG_FCR_FIFO_EN)
        {
            uartFifoClear(&pThis->FifoXmit);
            uartFifoClear(&pThis->FifoRecv);

            /*
             * If the FIFO is about to be enabled and the DR bit is ready we have an unacknowledged
             * byte in the RBR register which will be lost so we have to adjust the available bytes.
             */
            if (   ASMAtomicReadU32(&pThis->cbAvailRdr) > 0
                && (pThis->uRegFcr & UART_REG_FCR_FIFO_EN))
                ASMAtomicDecU32(&pThis->cbAvailRdr);

            /* Clear the DR bit too. */
            UART_REG_CLR(pThis->uRegLsr, UART_REG_LSR_DR);
        }

        if (rc == VINF_SUCCESS)
        {
            if (uVal & UART_REG_FCR_RCV_FIFO_RST)
            {
                TMTimerStop(pThis->CTX_SUFF(pTimerRcvFifoTimeout));
                pThis->fIrqCtiPending = false;
                uartFifoClear(&pThis->FifoRecv);
            }
            if (uVal & UART_REG_FCR_XMIT_FIFO_RST)
                uartFifoClear(&pThis->FifoXmit);

            /*
             * The 64byte FIFO enable bit is only changeable for 16750
             * and if the DLAB bit in LCR is set.
             */
            if (   pThis->enmType < UARTTYPE_16750
                || !(pThis->uRegLcr & UART_REG_LCR_DLAB))
                uVal &= ~UART_REG_FCR_64BYTE_FIFO_EN;
            else /* Use previous value. */
                uVal |= pThis->uRegFcr & UART_REG_FCR_64BYTE_FIFO_EN;

            if (uVal & UART_REG_FCR_64BYTE_FIFO_EN)
            {
                pThis->FifoRecv.cbMax = 64;
                pThis->FifoXmit.cbMax = 64;
            }
            else
            {
                pThis->FifoRecv.cbMax = 16;
                pThis->FifoXmit.cbMax = 16;
            }

            if (uVal & UART_REG_FCR_FIFO_EN)
            {
                uint8_t idxItl = UART_REG_FCR_RCV_LVL_IRQ_GET(uVal);
                if (uVal & UART_REG_FCR_64BYTE_FIFO_EN)
                    pThis->FifoRecv.cbItl = s_aFifoItl[idxItl].cbItl64;
                else
                    pThis->FifoRecv.cbItl = s_aFifoItl[idxItl].cbItl16;
            }

            /* The FIFO reset bits are self clearing. */
            pThis->uRegFcr = uVal & UART_REG_FCR_MASK_STICKY;
            uartIrqUpdate(pThis);
        }

        /* Fill in the next data. */
        if (ASMAtomicReadU32(&pThis->cbAvailRdr))
            uartR3DataFetch(pThis);
    }

    return rc;
#endif
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

    if (pThis->enmType < UARTTYPE_16750)
        uVal &= UART_REG_MCR_MASK_WR;
    else
        uVal &= UART_REG_MCR_MASK_WR_15750;
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
                pThis->fIrqCtiPending = false;
                if (!pThis->FifoRecv.cbUsed)
                {
                    TMTimerStop(pThis->CTX_SUFF(pTimerRcvFifoTimeout));
                    UART_REG_CLR(pThis->uRegLsr, UART_REG_LSR_DR);
                }
                else
                {
                    uint64_t tsCtiFire = TMTimerGet(pThis->CTX_SUFF(pTimerRcvFifoTimeout)) + pThis->cSymbolXferTicks * 4;
                    TMTimerSet(pThis->CTX_SUFF(pTimerRcvFifoTimeout), tsCtiFire);
                }
                uartIrqUpdate(pThis);
            }
        }
        else
        {
            *puVal = pThis->uRegRbr;

            if (pThis->uRegLsr & UART_REG_LSR_DR)
            {
                Assert(pThis->cbAvailRdr);
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
    AssertMsgReturn(cb == 1, ("uReg=%#x cb=%d u32=%#x\n", uReg, cb, u32), VINF_SUCCESS);

    int rc = PDMCritSectEnter(&pThis->CritSect, VINF_IOM_R3_IOPORT_WRITE);
    if (rc != VINF_SUCCESS)
        return rc;

    uint8_t idxReg = uReg & 0x7;
    LogFlowFunc(("pThis=%#p uReg=%u{%s} u32=%#x cb=%u\n",
                 pThis, uReg, uartRegIdx2Str(pThis, idxReg, true /*fWrite*/), u32, cb));

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

/* -=-=-=-=-=-=-=-=- Timer callbacks -=-=-=-=-=-=-=-=- */

/**
 * @callback_method_impl{FNTMTIMERDEV, Fifo timer function.}
 */
static DECLCALLBACK(void) uartR3RcvFifoTimeoutTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    RT_NOREF(pDevIns, pTimer);
    PUARTCORE pThis = (PUARTCORE)pvUser;
    PDMCritSectEnter(&pThis->CritSect, VERR_IGNORED);
    if (pThis->FifoRecv.cbUsed)
    {
        pThis->fIrqCtiPending = true;
        uartIrqUpdate(pThis);
    }
    PDMCritSectLeave(&pThis->CritSect);
}


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
        AssertMsg(RT_SUCCESS(rc) && cbRead == 1, ("This shouldn't fail and always return one byte!\n")); RT_NOREF(rc);
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
        /*
         * This can happen if there was data in the FIFO when the connection was closed,
         * idicate this condition to the lower driver by returning 0 bytes.
         */
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


DECLHIDDEN(int) uartR3SaveExec(PUARTCORE pThis, PSSMHANDLE pSSM)
{
    SSMR3PutU16(pSSM,  pThis->uRegDivisor);
    SSMR3PutU8(pSSM,   pThis->uRegRbr);
    SSMR3PutU8(pSSM,   pThis->uRegThr);
    SSMR3PutU8(pSSM,   pThis->uRegIer);
    SSMR3PutU8(pSSM,   pThis->uRegIir);
    SSMR3PutU8(pSSM,   pThis->uRegFcr);
    SSMR3PutU8(pSSM,   pThis->uRegLcr);
    SSMR3PutU8(pSSM,   pThis->uRegMcr);
    SSMR3PutU8(pSSM,   pThis->uRegLsr);
    SSMR3PutU8(pSSM,   pThis->uRegMsr);
    SSMR3PutU8(pSSM,   pThis->uRegScr);
    SSMR3PutBool(pSSM, pThis->fIrqCtiPending);
    SSMR3PutU8(pSSM,   pThis->FifoXmit.cbMax);
    SSMR3PutU8(pSSM,   pThis->FifoXmit.cbItl);
    SSMR3PutU8(pSSM,   pThis->FifoRecv.cbMax);
    SSMR3PutU8(pSSM,   pThis->FifoRecv.cbItl);

    return TMR3TimerSave(pThis->pTimerRcvFifoTimeoutR3, pSSM);
}


DECLHIDDEN(int) uartR3LoadExec(PUARTCORE pThis, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass,
                               uint8_t *puIrq, RTIOPORT *pPortBase)
{
    RT_NOREF(uPass);
    int rc = VINF_SUCCESS;

    if (uVersion > UART_SAVED_STATE_VERSION_LEGACY_CODE)
    {
        SSMR3GetU16(pSSM,  &pThis->uRegDivisor);
        SSMR3GetU8(pSSM,   &pThis->uRegRbr);
        SSMR3GetU8(pSSM,   &pThis->uRegThr);
        SSMR3GetU8(pSSM,   &pThis->uRegIer);
        SSMR3GetU8(pSSM,   &pThis->uRegIir);
        SSMR3GetU8(pSSM,   &pThis->uRegFcr);
        SSMR3GetU8(pSSM,   &pThis->uRegLcr);
        SSMR3GetU8(pSSM,   &pThis->uRegMcr);
        SSMR3GetU8(pSSM,   &pThis->uRegLsr);
        SSMR3GetU8(pSSM,   &pThis->uRegMsr);
        SSMR3GetU8(pSSM,   &pThis->uRegScr);
        SSMR3GetBool(pSSM, &pThis->fIrqCtiPending);
        SSMR3GetU8(pSSM,   &pThis->FifoXmit.cbMax);
        SSMR3GetU8(pSSM,   &pThis->FifoXmit.cbItl);
        SSMR3GetU8(pSSM,   &pThis->FifoRecv.cbMax);
        SSMR3GetU8(pSSM,   &pThis->FifoRecv.cbItl);

        TMR3TimerLoad(pThis->pTimerRcvFifoTimeoutR3, pSSM);
    }
    else
    {
        if (uVersion == UART_SAVED_STATE_VERSION_16450)
        {
            pThis->enmType = UARTTYPE_16450;
            LogRel(("Serial#%d: falling back to 16450 mode from load state\n", pThis->pDevInsR3->iInstance));
        }

        int      uIrq;
        uint32_t PortBase;

        SSMR3GetU16(pSSM, &pThis->uRegDivisor);
        SSMR3GetU8(pSSM, &pThis->uRegRbr);
        SSMR3GetU8(pSSM, &pThis->uRegIer);
        SSMR3GetU8(pSSM, &pThis->uRegLcr);
        SSMR3GetU8(pSSM, &pThis->uRegMcr);
        SSMR3GetU8(pSSM, &pThis->uRegLsr);
        SSMR3GetU8(pSSM, &pThis->uRegMsr);
        SSMR3GetU8(pSSM, &pThis->uRegScr);
        if (uVersion > UART_SAVED_STATE_VERSION_16450)
            SSMR3GetU8(pSSM, &pThis->uRegFcr);
        SSMR3Skip(pSSM, sizeof(int32_t));
        SSMR3GetS32(pSSM, &uIrq);
        SSMR3Skip(pSSM, sizeof(int32_t));
        SSMR3GetU32(pSSM, &PortBase);
        rc = SSMR3Skip(pSSM, sizeof(int32_t));

        if (   RT_SUCCESS(rc)
            && uVersion > UART_SAVED_STATE_VERSION_MISSING_BITS)
        {
            SSMR3GetU8(pSSM, &pThis->uRegThr);
            SSMR3Skip(pSSM, sizeof(uint8_t)); /* The old transmit shift register, not used anymore. */
            SSMR3GetU8(pSSM, &pThis->uRegIir);

            int iTimeoutPending = 0;
            SSMR3GetS32(pSSM, &iTimeoutPending);
            pThis->fIrqCtiPending = RT_BOOL(iTimeoutPending);

            TMR3TimerLoad(pThis->pTimerRcvFifoTimeoutR3, pSSM);
            TMR3TimerSkip(pSSM, NULL);
            SSMR3GetU8(pSSM, &pThis->FifoRecv.cbItl);
            rc = SSMR3GetU8(pSSM, &pThis->FifoRecv.cbItl);
        }

        if (RT_SUCCESS(rc))
        {
            AssertPtr(puIrq);
            AssertPtr(pPortBase);
            *puIrq     = (uint8_t)uIrq;
            *pPortBase = (RTIOPORT)PortBase;
        }
    }

    return rc;
}


DECLHIDDEN(int) uartR3LoadDone(PUARTCORE pThis, PSSMHANDLE pSSM)
{
    RT_NOREF(pSSM);

    uartR3ParamsUpdate(pThis);
    uartIrqUpdate(pThis);

    if (pThis->pDrvSerial)
    {
        /* Set the modem lines to reflect the current state. */
        int rc = pThis->pDrvSerial->pfnChgModemLines(pThis->pDrvSerial,
                                                     RT_BOOL(pThis->uRegMcr & UART_REG_MCR_RTS),
                                                     RT_BOOL(pThis->uRegMcr & UART_REG_MCR_DTR));
        if (RT_FAILURE(rc))
            LogRel(("Serial#%d: Failed to set modem lines with %Rrc during saved state load\n",
                    pThis->pDevInsR3->iInstance, rc));

        uint32_t fStsLines = 0;
        rc = pThis->pDrvSerial->pfnQueryStsLines(pThis->pDrvSerial, &fStsLines);
        if (RT_SUCCESS(rc))
            uartR3StsLinesUpdate(pThis, fStsLines);
        else
            LogRel(("Serial#%d: Failed to query status line status with %Rrc during reset\n",
                    pThis->pDevInsR3->iInstance, rc));
    }

    return VINF_SUCCESS;
}


DECLHIDDEN(void) uartR3Relocate(PUARTCORE pThis, RTGCINTPTR offDelta)
{
    RT_NOREF(offDelta);
    pThis->pDevInsRC              = PDMDEVINS_2_RCPTR(pThis->pDevInsR3);
    pThis->pTimerRcvFifoTimeoutRC = TMTimerRCPtr(pThis->pTimerRcvFifoTimeoutR3);
}


DECLHIDDEN(void) uartR3Reset(PUARTCORE pThis)
{
    pThis->uRegDivisor    = 0x0c; /* Default to 9600 Baud. */
    pThis->uRegRbr        = 0;
    pThis->uRegThr        = 0;
    pThis->uRegIer        = 0;
    pThis->uRegIir        = UART_REG_IIR_IP_NO_INT;
    pThis->uRegFcr        = 0;
    pThis->uRegLcr        = 0; /* 5 data bits, no parity, 1 stop bit. */
    pThis->uRegMcr        = 0;
    pThis->uRegLsr        = UART_REG_LSR_THRE | UART_REG_LSR_TEMT;
    pThis->uRegMsr        = 0; /* Updated below. */
    pThis->uRegScr        = 0;
    pThis->fIrqCtiPending = false;

    /* Standard FIFO size for 15550A. */
    pThis->FifoXmit.cbMax = 16;
    pThis->FifoRecv.cbMax = 16;
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

    rc = PDMDevHlpCritSectInit(pDevInsR3, &pThis->CritSect, RT_SRC_POS, "Uart{%s#%d}#%d",
                               pDevInsR3->pReg->szName, pDevInsR3->iInstance, iLUN);
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

    /*
     * Create the receive FIFO character timeout indicator timer.
     */
    rc = PDMDevHlpTMTimerCreate(pDevInsR3, TMCLOCK_VIRTUAL, uartR3RcvFifoTimeoutTimer, pThis,
                                TMTIMER_FLAGS_NO_CRIT_SECT, "UART Rcv FIFO Timer",
                                &pThis->pTimerRcvFifoTimeoutR3);
    AssertRCReturn(rc, rc);

    rc = TMR3TimerSetCritSect(pThis->pTimerRcvFifoTimeoutR3, &pThis->CritSect);
    AssertRCReturn(rc, rc);

    pThis->pTimerRcvFifoTimeoutR0 = TMTimerR0Ptr(pThis->pTimerRcvFifoTimeoutR3);
    pThis->pTimerRcvFifoTimeoutRC = TMTimerRCPtr(pThis->pTimerRcvFifoTimeoutR3);

    uartR3Reset(pThis);
    return VINF_SUCCESS;
}

#endif /* IN_RING3 */

#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */
