/* $Id$ */
/** @file
 * DMA Controller Device.
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
 * --------------------------------------------------------------------
 *
 * This code is based on:
 *
 * QEMU DMA emulation
 *
 * Copyright (c) 2003 Vassili Karpov (malc)
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
 */

#ifdef VBOX

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/pdmdev.h>
#include <VBox/err.h>

#define LOG_GROUP LOG_GROUP_DEFAULT ///@todo LOG_GROUP_DEV_DMA
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/string.h>

#include <stdio.h>
#include <stdlib.h>

#include "Builtins.h"
#include "../vl_vbox.h"
typedef PFNDMATRANSFERHANDLER DMA_transfer_handler;

#else  /* !VBOX */
#include "vl.h"
#endif

/* #define DEBUG_DMA */

#ifndef VBOX
#ifndef __WIN32__
#define dolog(...) fprintf (stderr, "dma: " __VA_ARGS__)
#ifdef DEBUG_DMA
#define lwarn(...) fprintf (stderr, "dma: " __VA_ARGS__)
#define linfo(...) fprintf (stderr, "dma: " __VA_ARGS__)
#define ldebug(...) fprintf (stderr, "dma: " __VA_ARGS__)
#else
#define lwarn(...)
#define linfo(...)
#define ldebug(...)
#endif
#else
#define dolog()
#define lwarn()
#define linfo()
#define ldebug()
#endif
#else /* VBOX */

#ifdef LOG_ENABLED
#endif
# ifdef LOG_ENABLED
#   define DEBUG_DMA
    static void DMA_DPRINTF (const char *fmt, ...)
    {
        if (LogIsEnabled ()) {
            va_list args;
            va_start (args, fmt);
            RTLogLogger (NULL, NULL, "dma: %N", fmt, &args); /* %N - nested va_list * type formatting call. */
            va_end (args);
        }
    }
# else
  DECLINLINE(void) DMA_DPRINTF(const char *pszFmt, ...) {}
# endif

#define dolog DMA_DPRINTF
#define lwarn DMA_DPRINTF
#define linfo DMA_DPRINTF
#define ldebug DMA_DPRINTF

#endif /* VBOX */

#define LENOFA(a) ((int) (sizeof(a)/sizeof(a[0])))

struct dma_regs {
    unsigned int now[2];
    uint16_t base[2];
    uint8_t mode;
    uint8_t page;
    uint8_t pageh;
    uint8_t dack;
    uint8_t eop;
    DMA_transfer_handler transfer_handler;
    void *opaque;
};

#define ADDR 0
#define COUNT 1

struct dma_cont {
    uint8_t status;
    uint8_t command;
    uint8_t mask;
    uint8_t flip_flop;
    unsigned int dshift;
    struct dma_regs regs[4];
};

typedef struct {
    PPDMDEVINS pDevIns;
    PCPDMDMACHLP pHlp;
    struct dma_cont dma_controllers[2];
} DMAState;

enum {
    CMD_MEMORY_TO_MEMORY = 0x01,
    CMD_FIXED_ADDRESS    = 0x02,
    CMD_BLOCK_CONTROLLER = 0x04,
    CMD_COMPRESSED_TIME  = 0x08,
    CMD_CYCLIC_PRIORITY  = 0x10,
    CMD_EXTENDED_WRITE   = 0x20,
    CMD_LOW_DREQ         = 0x40,
    CMD_LOW_DACK         = 0x80,
    CMD_NOT_SUPPORTED    = CMD_MEMORY_TO_MEMORY | CMD_FIXED_ADDRESS
    | CMD_COMPRESSED_TIME | CMD_CYCLIC_PRIORITY | CMD_EXTENDED_WRITE
    | CMD_LOW_DREQ | CMD_LOW_DACK

};

static int channels[8] = {-1, 2, 3, 1, -1, -1, -1, 0};

static void write_page (void *opaque, uint32_t nport, uint32_t data)
{
    struct dma_cont *d = (struct dma_cont*)opaque;
    int ichan;

    ichan = channels[nport & 7];
    if (-1 == ichan) {
        dolog ("invalid channel %#x %#x\n", nport, data);
        return;
    }
    d->regs[ichan].page = data;
}

static void write_pageh (void *opaque, uint32_t nport, uint32_t data)
{
    struct dma_cont *d = (struct dma_cont*)opaque;
    int ichan;

    ichan = channels[nport & 7];
    if (-1 == ichan) {
        dolog ("invalid channel %#x %#x\n", nport, data);
        return;
    }
    d->regs[ichan].pageh = data;
}

static uint32_t read_page (void *opaque, uint32_t nport)
{
    struct dma_cont *d = (struct dma_cont*)opaque;
    int ichan;

    ichan = channels[nport & 7];
    if (-1 == ichan) {
        dolog ("invalid channel read %#x\n", nport);
        return 0;
    }
    return d->regs[ichan].page;
}

static uint32_t read_pageh (void *opaque, uint32_t nport)
{
    struct dma_cont *d = (struct dma_cont*)opaque;
    int ichan;

    ichan = channels[nport & 7];
    if (-1 == ichan) {
        dolog ("invalid channel read %#x\n", nport);
        return 0;
    }
    return d->regs[ichan].pageh;
}

static inline void init_chan (struct dma_cont *d, int ichan)
{
    struct dma_regs *r;

    r = d->regs + ichan;
    r->now[ADDR] = r->base[ADDR] << d->dshift;
    r->now[COUNT] = 0;
}

static inline int getff (struct dma_cont *d)
{
    int ff;

    ff = d->flip_flop;
    d->flip_flop = !ff;
    return ff;
}

static uint32_t read_chan (void *opaque, uint32_t nport)
{
    struct dma_cont *d = (struct dma_cont*)opaque;
    int ichan, nreg, iport, ff, val, dir;
    struct dma_regs *r;

    iport = (nport >> d->dshift) & 0x0f;
    ichan = iport >> 1;
    nreg = iport & 1;
    r = d->regs + ichan;

    dir = ((r->mode >> 5) & 1) ? -1 : 1;
    ff = getff (d);
    if (nreg)
        val = (r->base[COUNT] << d->dshift) - r->now[COUNT];
    else
        val = r->now[ADDR] + r->now[COUNT] * dir;

    ldebug ("read_chan %#x -> %d\n", iport, val);
    return (val >> (d->dshift + (ff << 3))) & 0xff;
}

static void write_chan (void *opaque, uint32_t nport, uint32_t data)
{
    struct dma_cont *d = (struct dma_cont*)opaque;
    int iport, ichan, nreg;
    struct dma_regs *r;

    iport = (nport >> d->dshift) & 0x0f;
    ichan = iport >> 1;
    nreg = iport & 1;
    r = d->regs + ichan;
    if (getff (d)) {
        r->base[nreg] = (r->base[nreg] & 0xff) | ((data << 8) & 0xff00);
        init_chan (d, ichan);
    } else {
        r->base[nreg] = (r->base[nreg] & 0xff00) | (data & 0xff);
    }
}

static void write_cont (void *opaque, uint32_t nport, uint32_t data)
{
    struct dma_cont *d = (struct dma_cont*)opaque;
    int iport, ichan = 0;

    iport = (nport >> d->dshift) & 0x0f;
    switch (iport) {
    case 0x08:                  /* command */
        if ((data != 0) && (data & CMD_NOT_SUPPORTED)) {
            dolog ("command %#x not supported\n", data);
            return;
        }
        d->command = data;
        break;

    case 0x09:
        ichan = data & 3;
        if (data & 4) {
            d->status |= 1 << (ichan + 4);
        }
        else {
            d->status &= ~(1 << (ichan + 4));
        }
        d->status &= ~(1 << ichan);
        break;

    case 0x0a:                  /* single mask */
        if (data & 4)
            d->mask |= 1 << (data & 3);
        else
            d->mask &= ~(1 << (data & 3));
        break;

    case 0x0b:                  /* mode */
        {
            ichan = data & 3;
#ifdef DEBUG_DMA
            {
                int op, ai, dir, opmode;
                op = (data >> 2) & 3;
                ai = (data >> 4) & 1;
                dir = (data >> 5) & 1;
                opmode = (data >> 6) & 3;

                linfo ("ichan %d, op %d, ai %d, dir %d, opmode %d\n",
                       ichan, op, ai, dir, opmode);
            }
#endif
            d->regs[ichan].mode = data;
            break;
        }

    case 0x0c:                  /* clear flip flop */
        d->flip_flop = 0;
        break;

    case 0x0d:                  /* reset */
        d->flip_flop = 0;
        d->mask = ~0;
        d->status = 0;
        d->command = 0;
        break;

    case 0x0e:                  /* clear mask for all channels */
        d->mask = 0;
        break;

    case 0x0f:                  /* write mask for all channels */
        d->mask = data;
        break;

    default:
        dolog ("unknown iport %#x\n", iport);
        break;
    }

#ifdef DEBUG_DMA
    if (0xc != iport) {
        linfo ("write_cont: nport %#06x, ichan % 2d, val %#06x\n",
               nport, ichan, data);
    }
#endif
}

static uint32_t read_cont (void *opaque, uint32_t nport)
{
    struct dma_cont *d = (struct dma_cont*)opaque;
    int iport, val;

    iport = (nport >> d->dshift) & 0x0f;
    switch (iport) {
    case 0x08:                  /* status */
        val = d->status;
        d->status &= 0xf0;
        break;
    case 0x0f:                  /* mask */
        val = d->mask;
        break;
    default:
        val = 0;
        break;
    }

    ldebug ("read_cont: nport %#06x, iport %#04x val %#x\n", nport, iport, val);
    return val;
}

static uint8_t DMA_get_channel_mode (DMAState *s, int nchan)
{
    return s->dma_controllers[nchan > 3].regs[nchan & 3].mode;
}

static void DMA_hold_DREQ (DMAState *s, int nchan)
{
    int ncont, ichan;

    ncont = nchan > 3;
    ichan = nchan & 3;
    linfo ("held cont=%d chan=%d\n", ncont, ichan);
    s->dma_controllers[ncont].status |= 1 << (ichan + 4);
}

static void DMA_release_DREQ (DMAState *s, int nchan)
{
    int ncont, ichan;

    ncont = nchan > 3;
    ichan = nchan & 3;
    linfo ("released cont=%d chan=%d\n", ncont, ichan);
    s->dma_controllers[ncont].status &= ~(1 << (ichan + 4));
}

static void channel_run (DMAState *s, int ncont, int ichan)
{
    int n;
    struct dma_regs *r = &s->dma_controllers[ncont].regs[ichan];
#ifdef DEBUG_DMA
    int dir, opmode;

    dir = (r->mode >> 5) & 1;
    opmode = (r->mode >> 6) & 3;

    if (dir) {
        dolog ("DMA in address decrement mode\n");
    }
    if (opmode != 1) {
        dolog ("DMA not in single mode select %#x\n", opmode);
    }
#endif

    r = s->dma_controllers[ncont].regs + ichan;
    n = r->transfer_handler (s->pDevIns, r->opaque, ichan + (ncont << 2),
                             r->now[COUNT], (r->base[COUNT] + 1) << ncont);
    r->now[COUNT] = n;
    ldebug ("dma_pos %d size %d\n", n, (r->base[COUNT] + 1) << ncont);
}

static void DMA_run (DMAState *s)
{
    struct dma_cont *d;
    int icont, ichan;

    d = s->dma_controllers;

    for (icont = 0; icont < 2; icont++, d++) {
        for (ichan = 0; ichan < 4; ichan++) {
            int mask;

            mask = 1 << ichan;

            if ((0 == (d->mask & mask)) && (0 != (d->status & (mask << 4))))
                channel_run (s, icont, ichan);
        }
    }
}

static void DMA_register_channel (DMAState *s, unsigned nchan,
                                  DMA_transfer_handler transfer_handler,
                                  void *opaque)
{
    struct dma_regs *r;
    int ichan, ncont;
    LogFlow (("DMA_register_channel: s=%p nchan=%d transfer_handler=%p opaque=%p\n",
              s, nchan, transfer_handler, opaque));

    ncont = nchan > 3;
    ichan = nchan & 3;

    r = s->dma_controllers[ncont].regs + ichan;
    r->transfer_handler = transfer_handler;
    r->opaque = opaque;
}

static uint32_t DMA_read_memory (DMAState *s,
                                 unsigned nchan,
                                 void *buf,
                                 uint32_t pos,
                                 uint32_t len)
{
    struct dma_regs *r = &s->dma_controllers[nchan > 3].regs[nchan & 3];
    uint32_t addr = ((r->pageh & 0x7f) << 24) | (r->page << 16) | r->now[ADDR];

    if (r->mode & 0x20) {
        unsigned i;
        uint8_t *p = (uint8_t*)buf;

#ifdef VBOX
        PDMDevHlpPhysRead (s->pDevIns, addr - pos - len, buf, len);
#else
        cpu_physical_memory_read (addr - pos - len, buf, len);
#endif
        /* What about 16bit transfers? */
        for (i = 0; i < len >> 1; i++) {
            uint8_t b = p[len - i - 1];
            p[i] = b;
        }
    }
    else
#ifdef VBOX
        PDMDevHlpPhysRead (s->pDevIns, addr + pos, buf, len);
#else
        cpu_physical_memory_read (addr + pos, buf, len);
#endif
    return len;
}

static uint32_t DMA_write_memory (DMAState *s,
                                  unsigned nchan,
                                  const void *buf,
                                  uint32_t pos,
                                  uint32_t len)
{
    struct dma_regs *r = &s->dma_controllers[nchan > 3].regs[nchan & 3];
    uint32_t addr = ((r->pageh & 0x7f) << 24) | (r->page << 16) | r->now[ADDR];

    if (r->mode & 0x20) {
        unsigned i;
        uint8_t *p = (uint8_t *) buf;

#ifdef VBOX
        PDMDevHlpPhysWrite (s->pDevIns, addr - pos - len, buf, len);
#else
        cpu_physical_memory_write (addr - pos - len, buf, len);
#endif
        /* What about 16bit transfers? */
        for (i = 0; i < len; i++) {
            uint8_t b = p[len - i - 1];
            p[i] = b;
        }
    }
    else
#ifdef VBOX
        PDMDevHlpPhysWrite (s->pDevIns, addr + pos, buf, len);
#else
        cpu_physical_memory_write (addr + pos, buf, len);
#endif

    return len;
}


#ifndef VBOX
/* request the emulator to transfer a new DMA memory block ASAP */
void DMA_schedule(int nchan)
{
    cpu_interrupt(cpu_single_env, CPU_INTERRUPT_EXIT);
}
#endif

static void dma_reset(void *opaque)
{
    struct dma_cont *d = (struct dma_cont*)opaque;
    write_cont (d, (0x0d << d->dshift), 0);
}

#ifdef VBOX
#define IO_READ_PROTO(n)                                        \
static DECLCALLBACK(int) io_read_##n (PPDMDEVINS pDevIns,       \
                                      void *pvUser,             \
                                      RTIOPORT Port,          \
                                      uint32_t *pu32,           \
                                      unsigned cb)


#define IO_WRITE_PROTO(n)                                       \
static DECLCALLBACK(int) io_write_##n (PPDMDEVINS pDevIns,      \
                                       void *pvUser,            \
                                       RTIOPORT Port,         \
                                       uint32_t u32,            \
                                       unsigned cb)

IO_WRITE_PROTO (chan)
{
    if (cb == 1) {
        write_chan (pvUser, Port, u32);
    }
#ifdef PARANOID
    else {
        Log (("Unknown write to %#x of size %d, value %#x\n",
              Port, cb, u32));
    }
#endif
    return VINF_SUCCESS;
}

IO_WRITE_PROTO (page)
{
    if (cb == 1) {
        write_page (pvUser, Port, u32);
    }
#ifdef PARANOID
    else {
        Log (("Unknown write to %#x of size %d, value %#x\n",
              Port, cb, u32));
    }
#endif
    return VINF_SUCCESS;
}

IO_WRITE_PROTO (pageh)
{
    if (cb == 1) {
        write_pageh (pvUser, Port, u32);
    }
#ifdef PARANOID
    else {
        Log (("Unknown write to %#x of size %d, value %#x\n",
              Port, cb, u32));
    }
#endif
    return VINF_SUCCESS;
}

IO_WRITE_PROTO (cont)
{
    if (cb == 1) {
        write_cont (pvUser, Port, u32);
    }
#ifdef PARANOID
    else {
        Log (("Unknown write to %#x of size %d, value %#x\n",
              Port, cb, u32));
    }
#endif
    return VINF_SUCCESS;
}

IO_READ_PROTO (chan)
{
    if (cb == 1) {
        *pu32 = read_chan (pvUser, Port);
        return VINF_SUCCESS;
    }
    else {
        return VERR_IOM_IOPORT_UNUSED;
    }
}

IO_READ_PROTO (page)
{
    if (cb == 1) {
        *pu32 = read_page (pvUser, Port);
        return VINF_SUCCESS;
    }
    else {
        return VERR_IOM_IOPORT_UNUSED;
    }
}

IO_READ_PROTO (pageh)
{
    if (cb == 1) {
        *pu32 = read_pageh (pvUser, Port);
        return VINF_SUCCESS;
    }
    else {
        return VERR_IOM_IOPORT_UNUSED;
    }
}

IO_READ_PROTO (cont)
{
    if (cb == 1) {
        *pu32 = read_cont (pvUser, Port);
        return VINF_SUCCESS;
    }
    else {
        return VERR_IOM_IOPORT_UNUSED;
    }
}
#endif

/* dshift = 0: 8 bit DMA, 1 = 16 bit DMA */
static void dma_init2(DMAState *s, struct dma_cont *d, int base, int dshift,
                      int page_base, int pageh_base)
{
    const static int page_port_list[] = { 0x1, 0x2, 0x3, 0x7 };
    int i;

    d->dshift = dshift;
    for (i = 0; i < 8; i++) {
#ifdef VBOX
        PDMDevHlpIOPortRegister (s->pDevIns, base + (i << dshift), 1, d,
                                 io_write_chan, io_read_chan, NULL, NULL, "DMA");
#else
        register_ioport_write (base + (i << dshift), 1, 1, write_chan, d);
        register_ioport_read (base + (i << dshift), 1, 1, read_chan, d);
#endif
    }
    for (i = 0; i < LENOFA (page_port_list); i++) {
#ifdef VBOX
        PDMDevHlpIOPortRegister (s->pDevIns, page_base + page_port_list[i], 1, d,
                                 io_write_page, io_read_page, NULL, NULL, "DMA Page");
#else
        register_ioport_write (page_base + page_port_list[i], 1, 1,
                               write_page, d);
        register_ioport_read (page_base + page_port_list[i], 1, 1,
                              read_page, d);
#endif
        if (pageh_base >= 0) {
#ifdef VBOX
            PDMDevHlpIOPortRegister (s->pDevIns, pageh_base + page_port_list[i], 1, d,
                                     io_write_pageh, io_read_pageh, NULL, NULL, "DMA Page High");
#else
            register_ioport_write (pageh_base + page_port_list[i], 1, 1,
                                   write_pageh, d);
            register_ioport_read (pageh_base + page_port_list[i], 1, 1,
                                  read_pageh, d);
#endif
        }
    }
    for (i = 0; i < 8; i++) {
#ifdef VBOX
        PDMDevHlpIOPortRegister (s->pDevIns, base + ((i + 8) << dshift), 1, d,
                                 io_write_cont, io_read_cont, NULL, NULL, "DMA cont");
#else
        register_ioport_write (base + ((i + 8) << dshift), 1, 1,
                               write_cont, d);
        register_ioport_read (base + ((i + 8) << dshift), 1, 1,
                              read_cont, d);
#endif
    }
#ifndef VBOX
    qemu_register_reset(dma_reset, d);
#endif
    dma_reset(d);
}

static void dma_save (QEMUFile *f, void *opaque)
{
    struct dma_cont *d = (struct dma_cont*)opaque;
    int i;

    /* qemu_put_8s (f, &d->status); */
    qemu_put_8s (f, &d->command);
    qemu_put_8s (f, &d->mask);
    qemu_put_8s (f, &d->flip_flop);
    qemu_put_be32s (f, &d->dshift);

    for (i = 0; i < 4; ++i) {
        struct dma_regs *r = &d->regs[i];
        qemu_put_be32s (f, &r->now[0]);
        qemu_put_be32s (f, &r->now[1]);
        qemu_put_be16s (f, &r->base[0]);
        qemu_put_be16s (f, &r->base[1]);
        qemu_put_8s (f, &r->mode);
        qemu_put_8s (f, &r->page);
        qemu_put_8s (f, &r->pageh);
        qemu_put_8s (f, &r->dack);
        qemu_put_8s (f, &r->eop);
    }
}

static int dma_load (QEMUFile *f, void *opaque, int version_id)
{
    struct dma_cont *d = (struct dma_cont*)opaque;
    int i;

    if (version_id != 1)
#ifdef VBOX
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
#else
        return -EINVAL;
#endif

    /* qemu_get_8s (f, &d->status); */
    qemu_get_8s (f, &d->command);
    qemu_get_8s (f, &d->mask);
    qemu_get_8s (f, &d->flip_flop);
    qemu_get_be32s (f, &d->dshift);

    for (i = 0; i < 4; ++i) {
        struct dma_regs *r = &d->regs[i];
        qemu_get_be32s (f, &r->now[0]);
        qemu_get_be32s (f, &r->now[1]);
        qemu_get_be16s (f, &r->base[0]);
        qemu_get_be16s (f, &r->base[1]);
        qemu_get_8s (f, &r->mode);
        qemu_get_8s (f, &r->page);
        qemu_get_8s (f, &r->pageh);
        qemu_get_8s (f, &r->dack);
        qemu_get_8s (f, &r->eop);
    }
    return 0;
}

#ifndef VBOX
void DMA_init (int high_page_enable)
{
    dma_init2(&dma_controllers[0], 0x00, 0, 0x80,
              high_page_enable ? 0x480 : -1);
    dma_init2(&dma_controllers[1], 0xc0, 1, 0x88,
              high_page_enable ? 0x488 : -1);
    register_savevm ("dma", 0, 1, dma_save, dma_load, &dma_controllers[0]);
    register_savevm ("dma", 1, 1, dma_save, dma_load, &dma_controllers[1]);
}
#endif

#ifdef VBOX
static bool run_wrapper (PPDMDEVINS pDevIns)
{
    DMA_run (PDMINS2DATA (pDevIns, DMAState *));
    return 0;
}

static void register_channel_wrapper (PPDMDEVINS pDevIns,
                                      unsigned nchan,
                                      PFNDMATRANSFERHANDLER f,
                                      void *opaque)
{
    DMAState *s = PDMINS2DATA (pDevIns, DMAState *);
    DMA_register_channel (s, nchan, f, opaque);
}

static uint32_t rd_mem_wrapper (PPDMDEVINS pDevIns,
                                unsigned nchan,
                                void *buf,
                                uint32_t pos,
                                uint32_t len)
{
    DMAState *s = PDMINS2DATA (pDevIns, DMAState *);
    return DMA_read_memory (s, nchan, buf, pos, len);
}

static uint32_t wr_mem_wrapper (PPDMDEVINS pDevIns,
                                unsigned nchan,
                                const void *buf,
                                uint32_t pos,
                                uint32_t len)
{
    DMAState *s = PDMINS2DATA (pDevIns, DMAState *);
    return DMA_write_memory (s, nchan, buf, pos, len);
}

static void set_DREQ_wrapper (PPDMDEVINS pDevIns,
                              unsigned nchan,
                              unsigned level)
{
    DMAState *s = PDMINS2DATA (pDevIns, DMAState *);
    if (level) {
        DMA_hold_DREQ (s, nchan);
    }
    else {
        DMA_release_DREQ (s, nchan);
    }
}

static uint8_t get_mode_wrapper (PPDMDEVINS pDevIns, unsigned nchan)
{
    DMAState *s = PDMINS2DATA (pDevIns, DMAState *);
    return DMA_get_channel_mode (s, nchan);
}

static void DMAReset (PPDMDEVINS pDevIns)
{
    DMAState *s = PDMINS2DATA (pDevIns, DMAState *);
    dma_reset (&s->dma_controllers[0]);
    dma_reset (&s->dma_controllers[1]);
}

static DECLCALLBACK(int) SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle)
{
    DMAState *s = PDMINS2DATA (pDevIns, DMAState *);
    dma_save (pSSMHandle, &s->dma_controllers[0]);
    dma_save (pSSMHandle, &s->dma_controllers[1]);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) LoadExec (PPDMDEVINS pDevIns,
                                   PSSMHANDLE pSSMHandle,
                                   uint32_t u32Version)
{
    DMAState *s = PDMINS2DATA (pDevIns, DMAState *);

    if (u32Version != 1) {
        AssertFailed ();
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    dma_load (pSSMHandle, &s->dma_controllers[0], u32Version);
    return dma_load (pSSMHandle, &s->dma_controllers[1], u32Version);
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
static DECLCALLBACK(int) DMAConstruct(PPDMDEVINS pDevIns,
                                      int iInstance,
                                      PCFGMNODE pCfgHandle)
{
    DMAState *s = PDMINS2DATA (pDevIns, DMAState *);
    bool high_page_enable = 0;
    PDMDMACREG reg;
    int rc;

    s->pDevIns = pDevIns;

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "\0")) /* "HighPageEnable\0")) */
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

#if 0
    rc = CFGMR3QueryBool (pCfgHandle, "HighPageEnable", &high_page_enable);
    if (VBOX_FAILURE (rc)) {
        return rc;
    }
#endif

    dma_init2(s, &s->dma_controllers[0], 0x00, 0, 0x80,
              high_page_enable ? 0x480 : -1);
    dma_init2(s, &s->dma_controllers[1], 0xc0, 1, 0x88,
              high_page_enable ? 0x488 : -1);

    reg.u32Version        = PDM_DMACREG_VERSION;
    reg.pfnRun            = run_wrapper;
    reg.pfnRegister       = register_channel_wrapper;
    reg.pfnReadMemory     = rd_mem_wrapper;
    reg.pfnWriteMemory    = wr_mem_wrapper;
    reg.pfnSetDREQ        = set_DREQ_wrapper;
    reg.pfnGetChannelMode = get_mode_wrapper;

    Assert(pDevIns->pDevHlp->pfnDMARegister);
    rc = pDevIns->pDevHlp->pfnDMACRegister (pDevIns, &reg, &s->pHlp);
    if (VBOX_FAILURE (rc)) {
        return rc;
    }

    rc = PDMDevHlpSSMRegister (pDevIns, pDevIns->pDevReg->szDeviceName, iInstance, 1, sizeof (*s),
                               NULL, SaveExec, NULL, NULL, LoadExec, NULL);
    if (VBOX_FAILURE(rc))
        return rc;

    return VINF_SUCCESS;
}

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceDMA =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szDeviceName */
    "8237A",
    /* szGCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "DMA Controller Device",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_DEFAULT,
    /* fClass */
    PDM_DEVREG_CLASS_DMA,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(DMAState),
    /* pfnConstruct */
    DMAConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    DMAReset,
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
    NULL
};
#endif /* VBOX */
