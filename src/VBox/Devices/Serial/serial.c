#ifdef VBOX
/** @file
 *
 * VBox serial device:
 * Serial communication port driver
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 *
 * --------------------------------------------------------------------
 *
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
#include <VBox/pdm.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/string.h>

#include "Builtins.h"
#include "../vl_vbox.h"

#endif /* VBOX */

#ifndef VBOX
#include "vl.h"
#endif

//#define DEBUG_SERIAL

#define UART_LCR_DLAB	0x80	/* Divisor latch access bit */

#define UART_IER_MSI	0x08	/* Enable Modem status interrupt */
#define UART_IER_RLSI	0x04	/* Enable receiver line status interrupt */
#define UART_IER_THRI	0x02	/* Enable Transmitter holding register int. */
#define UART_IER_RDI	0x01	/* Enable receiver data interrupt */

#define UART_IIR_NO_INT	0x01	/* No interrupts pending */
#define UART_IIR_ID	0x06	/* Mask for the interrupt ID */

#define UART_IIR_MSI	0x00	/* Modem status interrupt */
#define UART_IIR_THRI	0x02	/* Transmitter holding register empty */
#define UART_IIR_RDI	0x04	/* Receiver data interrupt */
#define UART_IIR_RLSI	0x06	/* Receiver line status interrupt */

/*
 * These are the definitions for the Modem Control Register
 */
#define UART_MCR_LOOP	0x10	/* Enable loopback test mode */
#define UART_MCR_OUT2	0x08	/* Out2 complement */
#define UART_MCR_OUT1	0x04	/* Out1 complement */
#define UART_MCR_RTS	0x02	/* RTS complement */
#define UART_MCR_DTR	0x01	/* DTR complement */

/*
 * These are the definitions for the Modem Status Register
 */
#define UART_MSR_DCD	0x80	/* Data Carrier Detect */
#define UART_MSR_RI	0x40	/* Ring Indicator */
#define UART_MSR_DSR	0x20	/* Data Set Ready */
#define UART_MSR_CTS	0x10	/* Clear to Send */
#define UART_MSR_DDCD	0x08	/* Delta DCD */
#define UART_MSR_TERI	0x04	/* Trailing edge ring indicator */
#define UART_MSR_DDSR	0x02	/* Delta DSR */
#define UART_MSR_DCTS	0x01	/* Delta CTS */
#define UART_MSR_ANY_DELTA 0x0F	/* Any of the delta bits! */

#define UART_LSR_TEMT	0x40	/* Transmitter empty */
#define UART_LSR_THRE	0x20	/* Transmit-hold-register empty */
#define UART_LSR_BI	0x10	/* Break interrupt indicator */
#define UART_LSR_FE	0x08	/* Frame error indicator */
#define UART_LSR_PE	0x04	/* Parity error indicator */
#define UART_LSR_OE	0x02	/* Overrun error indicator */
#define UART_LSR_DR	0x01	/* Receiver data ready */

struct SerialState {
    uint16_t divider;
    uint8_t rbr; /* receive register */
    uint8_t ier;
    uint8_t iir; /* read only */
    uint8_t lcr;
    uint8_t mcr;
    uint8_t lsr; /* read only */
    uint8_t msr; /* read only */
    uint8_t scr;
    /* NOTE: this hidden state is necessary for tx irq generation as
       it can be reset while reading iir */
    int thr_ipending;
    SetIRQFunc *set_irq;
    void *irq_opaque;
    int irq;
#ifdef VBOX
    /* Be careful with pointers in the structure; load just gets the whole structure from the saved state */
    PPDMDEVINS pDevIns;
#else
    CharDriverState *chr;
#endif
    int last_break_enable;
    target_ulong base;
    int it_shift;
};

static void serial_update_irq(SerialState *s)
{
    if ((s->lsr & UART_LSR_DR) && (s->ier & UART_IER_RDI)) {
        s->iir = UART_IIR_RDI;
    } else if (s->thr_ipending && (s->ier & UART_IER_THRI)) {
        s->iir = UART_IIR_THRI;
    } else {
        s->iir = UART_IIR_NO_INT;
    }
    if (s->iir != UART_IIR_NO_INT) {
#ifdef VBOX
        s->pDevIns->pDevHlp->pfnISASetIrq (s->pDevIns, s->irq, 1);
#else
        s->set_irq(s->irq_opaque, s->irq, 1);
#endif
    } else {
#ifdef VBOX
        s->pDevIns->pDevHlp->pfnISASetIrq (s->pDevIns, s->irq, 0);
#else
        s->set_irq(s->irq_opaque, s->irq, 0);
#endif
    }
}

static void serial_update_parameters(SerialState *s)
{
    int speed, parity, data_bits, stop_bits;
    QEMUSerialSetParams ssp;

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
    ssp.speed = speed;
    ssp.parity = parity;
    ssp.data_bits = data_bits;
    ssp.stop_bits = stop_bits;
#ifndef VBOX
    qemu_chr_ioctl(s->chr, CHR_IOCTL_SERIAL_SET_PARAMS, &ssp);
#endif
#if 0
    printf("speed=%d parity=%c data=%d stop=%d\n",
           speed, parity, data_bits, stop_bits);
#endif
}

static void serial_ioport_write(void *opaque, uint32_t addr, uint32_t val)
{
    SerialState *s = opaque;
    unsigned char ch;

    addr &= 7;
#ifdef DEBUG_SERIAL
    printf("serial: write addr=0x%02x val=0x%02x\n", addr, val);
#endif
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
#ifndef VBOX
            qemu_chr_write(s->chr, &ch, 1);
#endif
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
            s->lcr = val;
            serial_update_parameters(s);
            break_enable = (val >> 6) & 1;
            if (break_enable != s->last_break_enable) {
                s->last_break_enable = break_enable;
#ifndef VBOX
                qemu_chr_ioctl(s->chr, CHR_IOCTL_SERIAL_SET_BREAK,
                               &break_enable);
#endif
            }
        }
        break;
    case 4:
        s->mcr = val & 0x1f;
        break;
    case 5:
        break;
    case 6:
        break;
    case 7:
        s->scr = val;
        break;
    }
}

static uint32_t serial_ioport_read(void *opaque, uint32_t addr)
{
    SerialState *s = opaque;
    uint32_t ret;

    addr &= 7;
    switch(addr) {
    default:
    case 0:
        if (s->lcr & UART_LCR_DLAB) {
            ret = s->divider & 0xff;
        } else {
            ret = s->rbr;
            s->lsr &= ~(UART_LSR_DR | UART_LSR_BI);
            serial_update_irq(s);
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
        ret = s->iir;
        /* reset THR pending bit */
        if ((ret & 0x7) == UART_IIR_THRI)
            s->thr_ipending = 0;
        serial_update_irq(s);
        break;
    case 3:
        ret = s->lcr;
        break;
    case 4:
        ret = s->mcr;
        break;
    case 5:
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
        }
        break;
    case 7:
        ret = s->scr;
        break;
    }
#ifdef DEBUG_SERIAL
    printf("serial: read addr=0x%02x val=0x%02x\n", addr, ret);
#endif
    return ret;
}

#ifndef VBOX
static int serial_can_receive(SerialState *s)
{
    return !(s->lsr & UART_LSR_DR);
}

static void serial_receive_byte(SerialState *s, int ch)
{
    s->rbr = ch;
    s->lsr |= UART_LSR_DR;
    serial_update_irq(s);
}

static void serial_receive_break(SerialState *s)
{
    s->rbr = 0;
    s->lsr |= UART_LSR_BI | UART_LSR_DR;
    serial_update_irq(s);
}

static int serial_can_receive1(void *opaque)
{
    SerialState *s = opaque;
    return serial_can_receive(s);
}

static void serial_receive1(void *opaque, const uint8_t *buf, int size)
{
    SerialState *s = opaque;
    serial_receive_byte(s, buf[0]);
}

static void serial_event(void *opaque, int event)
{
    SerialState *s = opaque;
    if (event == CHR_EVENT_BREAK)
        serial_receive_break(s);
}

static void serial_save(QEMUFile *f, void *opaque)
{
    SerialState *s = opaque;

    qemu_put_be16s(f,&s->divider);
    qemu_put_8s(f,&s->rbr);
    qemu_put_8s(f,&s->ier);
    qemu_put_8s(f,&s->iir);
    qemu_put_8s(f,&s->lcr);
    qemu_put_8s(f,&s->mcr);
    qemu_put_8s(f,&s->lsr);
    qemu_put_8s(f,&s->msr);
    qemu_put_8s(f,&s->scr);
}

static int serial_load(QEMUFile *f, void *opaque, int version_id)
{
    SerialState *s = opaque;

    if(version_id > 2)
        return -EINVAL;

    if (version_id >= 2)
        qemu_get_be16s(f, &s->divider);
    else
        s->divider = qemu_get_byte(f);
    qemu_get_8s(f,&s->rbr);
    qemu_get_8s(f,&s->ier);
    qemu_get_8s(f,&s->iir);
    qemu_get_8s(f,&s->lcr);
    qemu_get_8s(f,&s->mcr);
    qemu_get_8s(f,&s->lsr);
    qemu_get_8s(f,&s->msr);
    qemu_get_8s(f,&s->scr);

    return 0;
}

/* If fd is zero, it means that the serial device uses the console */
SerialState *serial_init(SetIRQFunc *set_irq, void *opaque,
                         int base, int irq, CharDriverState *chr)
{
    SerialState *s;

    s = qemu_mallocz(sizeof(SerialState));
    if (!s)
        return NULL;
    s->set_irq = set_irq;
    s->irq_opaque = opaque;
    s->irq = irq;
    s->lsr = UART_LSR_TEMT | UART_LSR_THRE;
    s->iir = UART_IIR_NO_INT;
    s->msr = UART_MSR_DCD | UART_MSR_DSR | UART_MSR_CTS;

    register_savevm("serial", base, 2, serial_save, serial_load, s);

    register_ioport_write(base, 8, 1, serial_ioport_write, s);
    register_ioport_read(base, 8, 1, serial_ioport_read, s);
    s->chr = chr;
    qemu_chr_add_read_handler(chr, serial_can_receive1, serial_receive1, s);
    qemu_chr_add_event_handler(chr, serial_event);
    return s;
}

/* Memory mapped interface */
static uint32_t serial_mm_readb (void *opaque, target_phys_addr_t addr)
{
    SerialState *s = opaque;

    return serial_ioport_read(s, (addr - s->base) >> s->it_shift) & 0xFF;
}

static void serial_mm_writeb (void *opaque,
                              target_phys_addr_t addr, uint32_t value)
{
    SerialState *s = opaque;

    serial_ioport_write(s, (addr - s->base) >> s->it_shift, value & 0xFF);
}

static uint32_t serial_mm_readw (void *opaque, target_phys_addr_t addr)
{
    SerialState *s = opaque;

    return serial_ioport_read(s, (addr - s->base) >> s->it_shift) & 0xFFFF;
}

static void serial_mm_writew (void *opaque,
                              target_phys_addr_t addr, uint32_t value)
{
    SerialState *s = opaque;

    serial_ioport_write(s, (addr - s->base) >> s->it_shift, value & 0xFFFF);
}

static uint32_t serial_mm_readl (void *opaque, target_phys_addr_t addr)
{
    SerialState *s = opaque;

    return serial_ioport_read(s, (addr - s->base) >> s->it_shift);
}

static void serial_mm_writel (void *opaque,
                              target_phys_addr_t addr, uint32_t value)
{
    SerialState *s = opaque;

    serial_ioport_write(s, (addr - s->base) >> s->it_shift, value);
}

static CPUReadMemoryFunc *serial_mm_read[] = {
    &serial_mm_readb,
    &serial_mm_readw,
    &serial_mm_readl,
};

static CPUWriteMemoryFunc *serial_mm_write[] = {
    &serial_mm_writeb,
    &serial_mm_writew,
    &serial_mm_writel,
};

SerialState *serial_mm_init (SetIRQFunc *set_irq, void *opaque,
                             target_ulong base, int it_shift,
                             int irq, CharDriverState *chr)
{
    SerialState *s;
    int s_io_memory;

    s = qemu_mallocz(sizeof(SerialState));
    if (!s)
        return NULL;
    s->set_irq = set_irq;
    s->irq_opaque = opaque;
    s->irq = irq;
    s->lsr = UART_LSR_TEMT | UART_LSR_THRE;
    s->iir = UART_IIR_NO_INT;
    s->msr = UART_MSR_DCD | UART_MSR_DSR | UART_MSR_CTS;
    s->base = base;
    s->it_shift = it_shift;

    register_savevm("serial", base, 2, serial_save, serial_load, s);

    s_io_memory = cpu_register_io_memory(0, serial_mm_read,
                                         serial_mm_write, s);
    cpu_register_physical_memory(base, 8 << it_shift, s_io_memory);
    s->chr = chr;
    qemu_chr_add_read_handler(chr, serial_can_receive1, serial_receive1, s);
    qemu_chr_add_event_handler(chr, serial_event);
    return s;
}

#else

static DECLCALLBACK(int) serial_io_write (PPDMDEVINS pDevIns,
                                       void *pvUser,
                                       RTIOPORT Port,
                                       uint32_t u32,
                                       unsigned cb)
{
    if (cb == 1) {
        serial_ioport_write (pvUser, Port, u32);
    }
    else {
        AssertMsgFailed(("Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
    }
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) serial_io_read (PPDMDEVINS pDevIns,
                                      void *pvUser,
                                      RTIOPORT Port,
                                      uint32_t *pu32,
                                      unsigned cb)
{
    if (cb == 1) {
        *pu32 = serial_ioport_read (pvUser, Port);
        return VINF_SUCCESS;
    }
    else {
        return VERR_IOM_IOPORT_UNUSED;
    }
}

static DECLCALLBACK(int) SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle)
{
    SerialState *s = PDMINS2DATA (pDevIns, SerialState *);
    SSMR3PutMem(pSSMHandle, s, sizeof(*s));
    return SSMR3PutU32(pSSMHandle, ~0); /* sanity/terminator */
}

static DECLCALLBACK(int) LoadExec (PPDMDEVINS pDevIns,
                                   PSSMHANDLE pSSMHandle,
                                   uint32_t u32Version)
{
    int rc;
    uint32_t u32;
    SerialState *s = PDMINS2DATA (pDevIns, SerialState *);

    if (u32Version != 2) {
        AssertMsgFailed(("u32Version=%d\n", u32Version));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    rc = SSMR3GetMem(pSSMHandle, s, sizeof(*s));
    if (VBOX_FAILURE(rc))
        return rc;

    rc = SSMR3GetU32(pSSMHandle, &u32);
    if (VBOX_FAILURE(rc))
        return rc;

    if (u32 != ~0U) {
        AssertMsgFailed(("u32=%#x expected ~0\n", u32));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }
    /* Be careful with pointers in the structure; load just gets the whole structure from the saved state */
    s->pDevIns = pDevIns;
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
    SerialState   *s = PDMINS2DATA(pDevIns, SerialState*);
    uint16_t       io_base;
    uint8_t        irq_lvl;

    Assert(iInstance < 2);

    s->pDevIns = pDevIns;
    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "IRQ\0IOBase\0")) {
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;
    }


/** @todo r=bird: Check for VERR_CFGM_VALUE_NOT_FOUND and provide sensible defaults.
 * Also do AssertMsgFailed(("Configuration error:....)) in the failure cases of CFGMR3Query*()
 * and CFGR3AreValuesValid() like we're doing in the other devices.  */
    rc = CFGMR3QueryU8 (pCfgHandle, "IRQ", &irq_lvl);
    if (VBOX_FAILURE (rc)) {
        return rc;
    }

    rc = CFGMR3QueryU16 (pCfgHandle, "IOBase", &io_base);
    if (VBOX_FAILURE (rc)) {
        return rc;
    }

    Log(("serialConstruct instance %d iobase=%04x irq=%d\n", iInstance, io_base, irq_lvl));

    s->irq = irq_lvl;
    s->lsr = UART_LSR_TEMT | UART_LSR_THRE;
    s->iir = UART_IIR_NO_INT;

    rc = pDevIns->pDevHlp->pfnIOPortRegister (
        pDevIns,
        io_base,
        8,
        s,
        serial_io_write,
        serial_io_read,
        NULL, NULL,
        "SERIAL"
        );
    if (VBOX_FAILURE (rc)) {
        return rc;
    }

    rc = pDevIns->pDevHlp->pfnSSMRegister (
        pDevIns,                /* pDevIns */
        pDevIns->pDevReg->szDeviceName, /* pszName */
        iInstance,              /* u32Instance */
        2                       /* u32Version */,
        sizeof (*s),            /* cbGuess */
        NULL,                   /* pfnSavePrep */
        SaveExec,               /* pfnSaveExec */
        NULL,                   /* pfnSaveDone */
        NULL,                   /* pfnLoadPrep */
        LoadExec,               /* pfnLoadExec */
        NULL                    /* pfnLoadDone */
        );
    if (VBOX_FAILURE(rc))
        return rc;

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
    /* szGCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Serial Communication Port",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_DEFAULT,
    /* fClass */
    PDM_DEVREG_CLASS_SERIAL_PORT,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(SerialState),
    /* pfnConstruct */
    serialConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    NULL,
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
#endif
