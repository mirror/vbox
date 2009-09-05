/* $Id$ */
/** @file
 * DevSerial - 16450 UART emulation.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*
 * This code is based on:
 *
 * QEMU 16450 UART emulation
 *
 * Copyright (c) 2003-2004 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_SERIAL
#include <VBox/pdmdev.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/critsect.h>

#include "../Builtins.h"

#undef VBOX_SERIAL_PCI /* The PCI variant has lots of problems: wrong IRQ line and wrong IO base assigned. */

#ifdef VBOX_SERIAL_PCI
# include <VBox/pci.h>
#endif /* VBOX_SERIAL_PCI */


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define SERIAL_SAVED_STATE_VERSION  3

#define UART_LCR_DLAB               0x80        /* Divisor latch access bit */

#define UART_IER_MSI                0x08        /* Enable Modem status interrupt */
#define UART_IER_RLSI               0x04        /* Enable receiver line status interrupt */
#define UART_IER_THRI               0x02        /* Enable Transmitter holding register int. */
#define UART_IER_RDI                0x01        /* Enable receiver data interrupt */

#define UART_IIR_NO_INT             0x01        /* No interrupts pending */
#define UART_IIR_ID                     0x06    /* Mask for the interrupt ID */

#define UART_IIR_MSI                0x00        /* Modem status interrupt */
#define UART_IIR_THRI               0x02        /* Transmitter holding register empty */
#define UART_IIR_RDI                0x04        /* Receiver data interrupt */
#define UART_IIR_RLSI               0x06        /* Receiver line status interrupt */

/*
 * These are the definitions for the Modem Control Register
 */
#define UART_MCR_LOOP               0x10        /* Enable loopback test mode */
#define UART_MCR_OUT2               0x08        /* Out2 complement */
#define UART_MCR_OUT1               0x04        /* Out1 complement */
#define UART_MCR_RTS                0x02        /* RTS complement */
#define UART_MCR_DTR                0x01        /* DTR complement */

/*
 * These are the definitions for the Modem Status Register
 */
#define UART_MSR_DCD                0x80        /* Data Carrier Detect */
#define UART_MSR_RI                     0x40    /* Ring Indicator */
#define UART_MSR_DSR                0x20        /* Data Set Ready */
#define UART_MSR_CTS                0x10        /* Clear to Send */
#define UART_MSR_DDCD               0x08        /* Delta DCD */
#define UART_MSR_TERI               0x04        /* Trailing edge ring indicator */
#define UART_MSR_DDSR               0x02        /* Delta DSR */
#define UART_MSR_DCTS               0x01        /* Delta CTS */
#define UART_MSR_ANY_DELTA          0x0F        /* Any of the delta bits! */

#define UART_LSR_TEMT               0x40        /* Transmitter empty */
#define UART_LSR_THRE               0x20        /* Transmit-hold-register empty */
#define UART_LSR_BI                     0x10    /* Break interrupt indicator */
#define UART_LSR_FE                     0x08    /* Frame error indicator */
#define UART_LSR_PE                     0x04    /* Parity error indicator */
#define UART_LSR_OE                     0x02    /* Overrun error indicator */
#define UART_LSR_DR                     0x01    /* Receiver data ready */


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
struct SerialState
{
    /** Access critical section. */
    PDMCRITSECT                     CritSect;

    /** Pointer to the device instance - R3 Ptr. */
    PPDMDEVINSR3                    pDevInsR3;
    /** Pointer to the device instance - R0 Ptr. */
    PPDMDEVINSR0                    pDevInsR0;
    /** Pointer to the device instance - RC Ptr. */
    PPDMDEVINSRC                    pDevInsRC;
    RTRCPTR                         Alignment0; /**< Alignment. */
    /** The base interface. */
    PDMIBASE                        IBase;
    /** The character port interface. */
    PDMICHARPORT                    ICharPort;
    /** Pointer to the attached base driver. */
    R3PTRTYPE(PPDMIBASE)            pDrvBase;
    /** Pointer to the attached character driver. */
    R3PTRTYPE(PPDMICHAR)            pDrvChar;

    uint16_t                        divider;
    uint16_t                        auAlignment[3];
    uint8_t                         rbr; /* receive register */
    uint8_t                         ier;
    uint8_t                         iir; /* read only */
    uint8_t                         lcr;
    uint8_t                         mcr;
    uint8_t                         lsr; /* read only */
    uint8_t                         msr; /* read only */
    uint8_t                         scr;
    /* NOTE: this hidden state is necessary for tx irq generation as
       it can be reset while reading iir */
    int                             thr_ipending;
    int                             irq;
    bool                            msr_changed;

    bool                            fGCEnabled;
    bool                            fR0Enabled;
    bool                            fYieldOnLSRRead;
    bool                            afAlignment[4];

    RTSEMEVENT                      ReceiveSem;
    int                             last_break_enable;
    uint32_t                        base;

#ifdef VBOX_SERIAL_PCI
    PCIDEVICE                       dev;
#endif /* VBOX_SERIAL_PCI */
};

#ifndef VBOX_DEVICE_STRUCT_TESTCASE


#ifdef VBOX_SERIAL_PCI
#define PCIDEV_2_SERIALSTATE(pPciDev)           ( (SerialState *)((uintptr_t)(pPciDev) - RT_OFFSETOF(SerialState, dev)) )
#endif /* VBOX_SERIAL_PCI */
#define PDMIBASE_2_SERIALSTATE(pInstance)       ( (SerialState *)((uintptr_t)(pInterface) - RT_OFFSETOF(SerialState, IBase)) )
#define PDMICHARPORT_2_SERIALSTATE(pInstance)   ( (SerialState *)((uintptr_t)(pInterface) - RT_OFFSETOF(SerialState, ICharPort)) )


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
RT_C_DECLS_BEGIN
PDMBOTHCBDECL(int) serialIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) serialIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
RT_C_DECLS_END

#ifdef IN_RING3

static void serial_update_irq(SerialState *s)
{
    if ((s->lsr & UART_LSR_DR) && (s->ier & UART_IER_RDI)) {
        s->iir = UART_IIR_RDI;
    } else if (s->thr_ipending && (s->ier & UART_IER_THRI)) {
        s->iir = UART_IIR_THRI;
    } else if (s->msr_changed && (s->ier & UART_IER_RLSI)) {
        s->iir = UART_IIR_RLSI;
    } else {
        s->iir = UART_IIR_NO_INT;
    }
    if (s->iir != UART_IIR_NO_INT) {
        Log(("serial_update_irq %d 1\n", s->irq));
# ifdef VBOX_SERIAL_PCI
        PDMDevHlpPCISetIrqNoWait(s->CTX_SUFF(pDevIns), 0, 1);
# else /* !VBOX_SERIAL_PCI */
        PDMDevHlpISASetIrqNoWait(s->CTX_SUFF(pDevIns), s->irq, 1);
# endif /* !VBOX_SERIAL_PCI */
    } else {
        Log(("serial_update_irq %d 0\n", s->irq));
# ifdef VBOX_SERIAL_PCI
        PDMDevHlpPCISetIrqNoWait(s->CTX_SUFF(pDevIns), 0, 0);
# else /* !VBOX_SERIAL_PCI */
        PDMDevHlpISASetIrqNoWait(s->CTX_SUFF(pDevIns), s->irq, 0);
# endif /* !VBOX_SERIAL_PCI */
    }
}

static void serial_update_parameters(SerialState *s)
{
    int speed, parity, data_bits, stop_bits;

    if (s->lcr & 0x08) {
        if (s->lcr & 0x10)
            parity = 'E';
        else
            parity = 'O';
    } else {
            parity = 'N';
    }
    if (s->lcr & 0x04)
        stop_bits = 2;
    else
        stop_bits = 1;
    data_bits = (s->lcr & 0x03) + 5;
    if (s->divider == 0)
        return;
    speed = 115200 / s->divider;
    Log(("speed=%d parity=%c data=%d stop=%d\n", speed, parity, data_bits, stop_bits));
    if (RT_LIKELY(s->pDrvChar))
        s->pDrvChar->pfnSetParameters(s->pDrvChar, speed, parity, data_bits, stop_bits);
}

#endif /* IN_RING3 */

static int serial_ioport_write(void *opaque, uint32_t addr, uint32_t val)
{
    SerialState *s = (SerialState *)opaque;
    unsigned char ch;

    addr &= 7;
    LogFlow(("serial: write addr=0x%02x val=0x%02x\n", addr, val));

#ifndef IN_RING3
    NOREF(ch);
    NOREF(s);
    return VINF_IOM_HC_IOPORT_WRITE;
#else
    switch(addr) {
    default:
    case 0:
        if (s->lcr & UART_LCR_DLAB) {
            s->divider = (s->divider & 0xff00) | val;
            serial_update_parameters(s);
        } else {
            s->thr_ipending = 0;
            s->lsr &= ~UART_LSR_THRE;
            serial_update_irq(s);
            ch = val;
            if (RT_LIKELY(s->pDrvChar))
            {
                Log(("serial_io_port_write: write 0x%X\n", ch));
                int rc = s->pDrvChar->pfnWrite(s->pDrvChar, &ch, 1);
                AssertRC(rc);
            }
            s->thr_ipending = 1;
            s->lsr |= UART_LSR_THRE;
            s->lsr |= UART_LSR_TEMT;
            serial_update_irq(s);
        }
        break;
    case 1:
        if (s->lcr & UART_LCR_DLAB) {
            s->divider = (s->divider & 0x00ff) | (val << 8);
            serial_update_parameters(s);
        } else {
            s->ier = val & 0x0f;
            if (s->lsr & UART_LSR_THRE) {
                s->thr_ipending = 1;
            }
            serial_update_irq(s);
        }
        break;
    case 2:
        break;
    case 3:
        {
            int break_enable;
            if (s->lcr != val)
            {
                s->lcr = val;
                serial_update_parameters(s);
            }
            break_enable = (val >> 6) & 1;
            if (break_enable != s->last_break_enable) {
                s->last_break_enable = break_enable;
            }
        }
        break;
    case 4:
        s->mcr = val & 0x1f;
        if (RT_LIKELY(s->pDrvChar))
        {
            int rc = s->pDrvChar->pfnSetModemLines(s->pDrvChar, !!(s->mcr & UART_MCR_RTS), !!(s->mcr & UART_MCR_DTR));
            AssertRC(rc);
        }
        break;
    case 5:
        break;
    case 6:
        break;
    case 7:
        s->scr = val;
        break;
    }
    return VINF_SUCCESS;
#endif
}

static uint32_t serial_ioport_read(void *opaque, uint32_t addr, int *pRC)
{
    SerialState *s = (SerialState *)opaque;
    uint32_t ret = ~0U;

    *pRC = VINF_SUCCESS;

    addr &= 7;
    switch(addr) {
    default:
    case 0:
        if (s->lcr & UART_LCR_DLAB) {
            ret = s->divider & 0xff;
        } else {
#ifndef IN_RING3
            *pRC = VINF_IOM_HC_IOPORT_READ;
#else
            Log(("serial_io_port_read: read 0x%X\n", s->rbr));
            ret = s->rbr;
            s->lsr &= ~(UART_LSR_DR | UART_LSR_BI);
            serial_update_irq(s);
            {
                int rc = RTSemEventSignal(s->ReceiveSem);
                AssertRC(rc);
            }
#endif
        }
        break;
    case 1:
        if (s->lcr & UART_LCR_DLAB) {
            ret = (s->divider >> 8) & 0xff;
        } else {
            ret = s->ier;
        }
        break;
    case 2:
#ifndef IN_RING3
        *pRC = VINF_IOM_HC_IOPORT_READ;
#else
        ret = s->iir;
        /* reset THR pending bit */
        if ((ret & 0x7) == UART_IIR_THRI)
            s->thr_ipending = 0;
        /* reset msr changed bit */
        s->msr_changed = false;
        serial_update_irq(s);
#endif
        break;
    case 3:
        ret = s->lcr;
        break;
    case 4:
        ret = s->mcr;
        break;
    case 5:
        if ((s->lsr & UART_LSR_DR) == 0 && s->fYieldOnLSRRead)
        {
            /* No data available and yielding is enabled, so yield in ring3. */
#ifndef IN_RING3
            *pRC = VINF_IOM_HC_IOPORT_READ;
            break;
#else
            RTThreadYield ();
#endif
        }
        ret = s->lsr;
        break;
    case 6:
        if (s->mcr & UART_MCR_LOOP) {
            /* in loopback, the modem output pins are connected to the
               inputs */
            ret = (s->mcr & 0x0c) << 4;
            ret |= (s->mcr & 0x02) << 3;
            ret |= (s->mcr & 0x01) << 5;
        } else {
            ret = s->msr;
            /* Reset delta bits. */
            s->msr &= ~UART_MSR_ANY_DELTA;
        }
        break;
    case 7:
        ret = s->scr;
        break;
    }
    LogFlow(("serial: read addr=0x%02x val=0x%02x\n", addr, ret));
    return ret;
}

#ifdef IN_RING3

static DECLCALLBACK(int) serialNotifyRead(PPDMICHARPORT pInterface, const void *pvBuf, size_t *pcbRead)
{
    SerialState *pThis = PDMICHARPORT_2_SERIALSTATE(pInterface);
    int rc;

    Assert(*pcbRead != 0);

    PDMCritSectEnter(&pThis->CritSect, VERR_PERMISSION_DENIED);
    if (pThis->lsr & UART_LSR_DR)
    {
        /* If a character is still in the read queue, then wait for it to be emptied. */
        PDMCritSectLeave(&pThis->CritSect);
        rc = RTSemEventWait(pThis->ReceiveSem, 250);
        if (RT_FAILURE(rc))
            return rc;

        PDMCritSectEnter(&pThis->CritSect, VERR_PERMISSION_DENIED);
    }

    if (!(pThis->lsr & UART_LSR_DR))
    {
        pThis->rbr = *(const char *)pvBuf;
        pThis->lsr |= UART_LSR_DR;
        serial_update_irq(pThis);
        *pcbRead = 1;
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_TIMEOUT;

    PDMCritSectLeave(&pThis->CritSect);

    return rc;
}

static DECLCALLBACK(int) serialNotifyStatusLinesChanged(PPDMICHARPORT pInterface, uint32_t newStatusLines)
{
    SerialState *pThis = PDMICHARPORT_2_SERIALSTATE(pInterface);
    uint8_t newMsr = 0;

    Log(("%s: pInterface=%p newStatusLines=%u\n", __FUNCTION__, pInterface, newStatusLines));

    PDMCritSectEnter(&pThis->CritSect, VERR_PERMISSION_DENIED);

    /* Set new states. */
    if (newStatusLines & PDM_ICHAR_STATUS_LINES_DCD)
        newMsr |= UART_MSR_DCD;
    if (newStatusLines & PDM_ICHAR_STATUS_LINES_RI)
        newMsr |= UART_MSR_RI;
    if (newStatusLines & PDM_ICHAR_STATUS_LINES_DSR)
        newMsr |= UART_MSR_DSR;
    if (newStatusLines & PDM_ICHAR_STATUS_LINES_CTS)
        newMsr |= UART_MSR_CTS;

    /* Compare the old and the new states and set the delta bits accordingly. */
    if ((newMsr & UART_MSR_DCD) != (pThis->msr & UART_MSR_DCD))
        newMsr |= UART_MSR_DDCD;
    if ((newMsr & UART_MSR_RI) == 1 && (pThis->msr & UART_MSR_RI) == 0)
        newMsr |= UART_MSR_TERI;
    if ((newMsr & UART_MSR_DSR) != (pThis->msr & UART_MSR_DSR))
        newMsr |= UART_MSR_DDSR;
    if ((newMsr & UART_MSR_CTS) != (pThis->msr & UART_MSR_CTS))
        newMsr |= UART_MSR_DCTS;

    pThis->msr = newMsr;
    pThis->msr_changed = true;
    serial_update_irq(pThis);

    PDMCritSectLeave(&pThis->CritSect);

    return VINF_SUCCESS;
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
PDMBOTHCBDECL(int) serialIOPortWrite(PPDMDEVINS pDevIns, void *pvUser,
                                     RTIOPORT Port, uint32_t u32, unsigned cb)
{
    SerialState *pThis = PDMINS_2_DATA(pDevIns, SerialState *);
    int          rc = VINF_SUCCESS;

    if (cb == 1)
    {
        rc = PDMCritSectEnter(&pThis->CritSect, VINF_IOM_HC_IOPORT_WRITE);
        if (rc == VINF_SUCCESS)
        {
            Log2(("%s: port %#06x val %#04x\n", __FUNCTION__, Port, u32));
            rc = serial_ioport_write(pThis, Port, u32);
            PDMCritSectLeave(&pThis->CritSect);
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
PDMBOTHCBDECL(int) serialIOPortRead(PPDMDEVINS pDevIns, void *pvUser,
                                    RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    SerialState *pThis = PDMINS_2_DATA(pDevIns, SerialState *);
    int          rc = VINF_SUCCESS;

    if (cb == 1)
    {
        rc = PDMCritSectEnter(&pThis->CritSect, VINF_IOM_HC_IOPORT_READ);
        if (rc == VINF_SUCCESS)
        {
            *pu32 = serial_ioport_read(pThis, Port, &rc);
            Log2(("%s: port %#06x val %#04x\n", __FUNCTION__, Port, *pu32));
            PDMCritSectLeave(&pThis->CritSect);
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
static DECLCALLBACK(int) serialSaveExec(PPDMDEVINS pDevIns,
                                        PSSMHANDLE pSSMHandle)
{
    SerialState *pThis = PDMINS_2_DATA(pDevIns, SerialState *);

    SSMR3PutU16(pSSMHandle, pThis->divider);
    SSMR3PutU8(pSSMHandle, pThis->rbr);
    SSMR3PutU8(pSSMHandle, pThis->ier);
    SSMR3PutU8(pSSMHandle, pThis->lcr);
    SSMR3PutU8(pSSMHandle, pThis->mcr);
    SSMR3PutU8(pSSMHandle, pThis->lsr);
    SSMR3PutU8(pSSMHandle, pThis->msr);
    SSMR3PutU8(pSSMHandle, pThis->scr);
    SSMR3PutS32(pSSMHandle, pThis->thr_ipending);
    SSMR3PutS32(pSSMHandle, pThis->irq);
    SSMR3PutS32(pSSMHandle, pThis->last_break_enable);
    SSMR3PutU32(pSSMHandle, pThis->base);
    SSMR3PutBool(pSSMHandle, pThis->msr_changed);
    return SSMR3PutU32(pSSMHandle, ~0); /* sanity/terminator */
}

/**
 * Loads a saved serial port device state.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to the saved state.
 * @param   uVersion    The data unit version number.
 * @param   uPass       The data pass.
 */
static DECLCALLBACK(int) serialLoadExec(PPDMDEVINS pDevIns,
                                        PSSMHANDLE pSSMHandle,
                                        uint32_t uVersion,
                                        uint32_t uPass)
{
    SerialState *pThis = PDMINS_2_DATA(pDevIns, SerialState *);

    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);
    AssertMsgReturn(uVersion == SERIAL_SAVED_STATE_VERSION, ("%d\n", uVersion), VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION);

    SSMR3GetU16(pSSMHandle, &pThis->divider);
    SSMR3GetU8(pSSMHandle, &pThis->rbr);
    SSMR3GetU8(pSSMHandle, &pThis->ier);
    SSMR3GetU8(pSSMHandle, &pThis->lcr);
    SSMR3GetU8(pSSMHandle, &pThis->mcr);
    SSMR3GetU8(pSSMHandle, &pThis->lsr);
    SSMR3GetU8(pSSMHandle, &pThis->msr);
    SSMR3GetU8(pSSMHandle, &pThis->scr);
    SSMR3GetS32(pSSMHandle, &pThis->thr_ipending);
    SSMR3GetS32(pSSMHandle, &pThis->irq);
    SSMR3GetS32(pSSMHandle, &pThis->last_break_enable);
    SSMR3GetU32(pSSMHandle, &pThis->base);
    SSMR3GetBool(pSSMHandle, &pThis->msr_changed);

    uint32_t u32;
    int rc = SSMR3GetU32(pSSMHandle, &u32);
    if (RT_FAILURE(rc))
        return rc;
    AssertMsgReturn(u32 == ~0U, ("%#x\n", u32), VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

    if (pThis->lsr & UART_LSR_DR)
    {
        int rc = RTSemEventSignal(pThis->ReceiveSem);
        AssertRC(rc);
    }

    /* this isn't strictly necessary but cannot hurt... */
    pThis->pDevInsR3 = pDevIns;
    pThis->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    return VINF_SUCCESS;
}


/**
 * @copydoc FNPDMDEVRELOCATE
 */
static DECLCALLBACK(void) serialRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    SerialState *pThis = PDMINS_2_DATA(pDevIns, SerialState *);
    pThis->pDevInsRC   = PDMDEVINS_2_RCPTR(pDevIns);
}

#ifdef VBOX_SERIAL_PCI

static DECLCALLBACK(int) serialIOPortRegionMap(PPCIDEVICE pPciDev, /* unsigned */ int iRegion, RTGCPHYS GCPhysAddress, uint32_t cb, PCIADDRESSSPACE enmType)
{
    SerialState *pThis = PCIDEV_2_SERIALSTATE(pPciDev);
    int rc = VINF_SUCCESS;

    Assert(enmType == PCI_ADDRESS_SPACE_IO);
    Assert(iRegion == 0);
    Assert(cb == 8);
    AssertMsg(RT_ALIGN(GCPhysAddress, 8) == GCPhysAddress, ("Expected 8 byte alignment. GCPhysAddress=%#x\n", GCPhysAddress));

    pThis->base = (RTIOPORT)GCPhysAddress;
    LogRel(("Serial#%d: mapping I/O at %#06x\n", pThis->pDevIns->iInstance, pThis->base));

    /*
     * Register our port IO handlers.
     */
    rc = PDMDevHlpIOPortRegister(pPciDev->pDevIns, (RTIOPORT)GCPhysAddress, 8, (void *)pThis,
                                 serial_io_write, serial_io_read, NULL, NULL, "SERIAL");
    AssertRC(rc);
    return rc;
}

#endif /* VBOX_SERIAL_PCI */


/** @copyfrom PIBASE::pfnqueryInterface */
static DECLCALLBACK(void *) serialQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    SerialState *pThis = PDMIBASE_2_SERIALSTATE(pInterface);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pThis->IBase;
        case PDMINTERFACE_CHAR_PORT:
            return &pThis->ICharPort;
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
static DECLCALLBACK(int) serialDestruct(PPDMDEVINS pDevIns)
{
    SerialState *pThis = PDMINS_2_DATA(pDevIns, SerialState *);

    RTSemEventDestroy(pThis->ReceiveSem);
    pThis->ReceiveSem = NIL_RTSEMEVENT;

    PDMR3CritSectDelete(&pThis->CritSect);
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
static DECLCALLBACK(int) serialConstruct(PPDMDEVINS pDevIns,
                                         int iInstance,
                                         PCFGMNODE pCfgHandle)
{
    int            rc;
    SerialState   *pThis = PDMINS_2_DATA(pDevIns, SerialState*);
    uint16_t       io_base;
    uint8_t        irq_lvl;

    Assert(iInstance < 4);

    /*
     * Initialize the instance data.
     * (Do this early or the destructor might choke on something!)
     */
    pThis->pDevInsR3 = pDevIns;
    pThis->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

    pThis->lsr = UART_LSR_TEMT | UART_LSR_THRE;
    pThis->iir = UART_IIR_NO_INT;
    pThis->msr = UART_MSR_DCD | UART_MSR_DSR | UART_MSR_CTS;

    /* IBase */
    pThis->IBase.pfnQueryInterface = serialQueryInterface;

    /* ICharPort */
    pThis->ICharPort.pfnNotifyRead = serialNotifyRead;
    pThis->ICharPort.pfnNotifyStatusLinesChanged = serialNotifyStatusLinesChanged;

#ifdef VBOX_SERIAL_PCI
    /* the PCI device */
    pThis->dev.config[0x00] = 0xee; /* Vendor: ??? */
    pThis->dev.config[0x01] = 0x80;
    pThis->dev.config[0x02] = 0x01; /* Device: ??? */
    pThis->dev.config[0x03] = 0x01;
    pThis->dev.config[0x04] = PCI_COMMAND_IOACCESS;
    pThis->dev.config[0x09] = 0x01; /* Programming interface: 16450 */
    pThis->dev.config[0x0a] = 0x00; /* Subclass: Serial controller */
    pThis->dev.config[0x0b] = 0x07; /* Class: Communication controller */
    pThis->dev.config[0x0e] = 0x00; /* Header type: standard */
    pThis->dev.config[0x3c] = irq_lvl; /* preconfigure IRQ number (0 = autoconfig)*/
    pThis->dev.config[0x3d] = 1;    /* interrupt pin 0 */
#endif /* VBOX_SERIAL_PCI */

    /*
     * Validate and read the configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "IRQ\0" "IOBase\0" "GCEnabled\0" "R0Enabled\0" "YieldOnLSRRead\0"))
    {
        AssertMsgFailed(("serialConstruct Invalid configuration values\n"));
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;
    }

    rc = CFGMR3QueryBoolDef(pCfgHandle, "GCEnabled", &pThis->fGCEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"GCEnabled\" value"));

    rc = CFGMR3QueryBoolDef(pCfgHandle, "R0Enabled", &pThis->fR0Enabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"R0Enabled\" value"));

    rc = CFGMR3QueryBoolDef(pCfgHandle, "YieldOnLSRRead", &pThis->fYieldOnLSRRead, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"YieldOnLSRRead\" value"));

    rc = CFGMR3QueryU8(pCfgHandle, "IRQ", &irq_lvl);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
    {
        /* Provide sensible defaults. */
        if (iInstance == 0)
            irq_lvl = 4;
        else if (iInstance == 1)
            irq_lvl = 3;
        else
            AssertReleaseFailed(); /* irq_lvl is undefined. */
    }
    else if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"IRQ\" value"));

    rc = CFGMR3QueryU16(pCfgHandle, "IOBase", &io_base);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
    {
        if (iInstance == 0)
            io_base = 0x3f8;
        else if (iInstance == 1)
            io_base = 0x2f8;
        else
            AssertReleaseFailed(); /* io_base is undefined */
    }
    else if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"IOBase\" value"));

    Log(("DevSerial: instance %d iobase=%04x irq=%d\n", iInstance, io_base, irq_lvl));

    pThis->irq = irq_lvl;
#ifdef VBOX_SERIAL_PCI
    pThis->base = -1;
#else
    pThis->base = io_base;
#endif

    /*
     * Initialize critical section and the semaphore.
     * This must of course be done before attaching drivers or anything else which can call us back..
     */
    char szName[24];
    RTStrPrintf(szName, sizeof(szName), "Serial#%d", iInstance);
    rc = PDMDevHlpCritSectInit(pDevIns, &pThis->CritSect, szName);
    if (RT_FAILURE(rc))
        return rc;

    rc = RTSemEventCreate(&pThis->ReceiveSem);
    AssertRC(rc);

#ifdef VBOX_SERIAL_PCI
    /*
     * Register the PCI Device and region.
     */
    rc = PDMDevHlpPCIRegister(pDevIns, &pThis->dev);
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 0, 8, PCI_ADDRESS_SPACE_IO, serialIOPortRegionMap);
    if (RT_FAILURE(rc))
        return rc;

#else /* !VBOX_SERIAL_PCI */
    /*
     * Register the I/O ports.
     */
    pThis->base = io_base;
    rc = PDMDevHlpIOPortRegister(pDevIns, io_base, 8, 0,
                                 serialIOPortWrite, serialIOPortRead,
                                 NULL, NULL, "SERIAL");
    if (RT_FAILURE(rc))
        return rc;

    if (pThis->fGCEnabled)
        rc = PDMDevHlpIOPortRegisterGC(pDevIns, io_base, 8, 0, "serialIOPortWrite",
                                      "serialIOPortRead", NULL, NULL, "Serial");

    if (pThis->fR0Enabled)
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, io_base, 8, 0, "serialIOPortWrite",
                                      "serialIOPortRead", NULL, NULL, "Serial");
#endif /* !VBOX_SERIAL_PCI */

    /*
     * Saved state.
     */
    rc = PDMDevHlpSSMRegister(pDevIns, SERIAL_SAVED_STATE_VERSION, sizeof (*pThis), serialSaveExec, serialLoadExec);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Attach the char driver and get the interfaces.
     * For now no run-time changes are supported.
     */
    rc = PDMDevHlpDriverAttach(pDevIns, 0, &pThis->IBase, &pThis->pDrvBase, "Serial Char");
    if (RT_SUCCESS(rc))
    {
        pThis->pDrvChar = (PDMICHAR *)pThis->pDrvBase->pfnQueryInterface(pThis->pDrvBase, PDMINTERFACE_CHAR);
        if (!pThis->pDrvChar)
        {
            AssertLogRelMsgFailed(("Configuration error: instance %d has no char interface!\n", iInstance));
            return VERR_PDM_MISSING_INTERFACE;
        }
        /** @todo provide read notification interface!!!! */
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
    {
        pThis->pDrvBase = NULL;
        pThis->pDrvChar = NULL;
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
    /* szDeviceName */
    "serial",
    /* szRCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "Serial Communication Port",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_SERIAL,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(SerialState),
    /* pfnConstruct */
    serialConstruct,
    /* pfnDestruct */
    serialDestruct,
    /* pfnRelocate */
    serialRelocate,
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
