#ifdef VBOX
/** @file
 *
 * VBox network devices:
 * NE2000 ethernet controller
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
 * QEMU NE2000 emulation
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
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_NE2000
#include <VBox/pdm.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/string.h>
#include <iprt/alloc.h>

#include "Builtins.h"
#include "vl_vbox.h"

#endif /* VBOX */

#ifndef VBOX
#include "vl.h"
#endif

#define ETHER_ADDR_LEN ETH_ALEN
#define ETH_ALEN 6
#undef bcmp
#define bcmp(b1,b2,len) memcmp((b1), (b2), (size_t)(len))
#pragma pack(1)
struct ether_header
{
    uint8_t  ether_dhost[ETH_ALEN]; /* destination ethernet address */
    uint8_t  ether_shost[ETH_ALEN]; /* source ethernet address */
    uint16_t ether_type;            /* packet type ID field */
};
#pragma pack()
#undef htonl
#define htonl(x) ( (((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >> 8) | \
                   (((x) & 0x0000ff00) << 8)  | (((x) & 0x000000ff) << 24) )
#undef htons
#define htons(x) ( (((x) & 0xff00) >> 8) | (((x) & 0x00ff) << 8) )

/* debug NE2000 card */
#define DEBUG_NE2000

#define MAX_ETH_FRAME_SIZE 1514

#define E8390_CMD	0x00  /* The command register (for all pages) */
/* Page 0 register offsets. */
#define EN0_CLDALO	0x01	/* Low byte of current local dma addr  RD */
#define EN0_STARTPG	0x01	/* Starting page of ring bfr WR */
#define EN0_CLDAHI	0x02	/* High byte of current local dma addr  RD */
#define EN0_STOPPG	0x02	/* Ending page +1 of ring bfr WR */
#define EN0_BOUNDARY	0x03	/* Boundary page of ring bfr RD WR */
#define EN0_TSR		0x04	/* Transmit status reg RD */
#define EN0_TPSR	0x04	/* Transmit starting page WR */
#define EN0_NCR		0x05	/* Number of collision reg RD */
#define EN0_TCNTLO	0x05	/* Low  byte of tx byte count WR */
#define EN0_FIFO	0x06	/* FIFO RD */
#define EN0_TCNTHI	0x06	/* High byte of tx byte count WR */
#define EN0_ISR		0x07	/* Interrupt status reg RD WR */
#define EN0_CRDALO	0x08	/* low byte of current remote dma address RD */
#define EN0_RSARLO	0x08	/* Remote start address reg 0 */
#define EN0_CRDAHI	0x09	/* high byte, current remote dma address RD */
#define EN0_RSARHI	0x09	/* Remote start address reg 1 */
#define EN0_RCNTLO	0x0a	/* Remote byte count reg WR */
#define EN0_RCNTHI	0x0b	/* Remote byte count reg WR */
#define EN0_RSR		0x0c	/* rx status reg RD */
#define EN0_RXCR	0x0c	/* RX configuration reg WR */
#define EN0_TXCR	0x0d	/* TX configuration reg WR */
#define EN0_COUNTER0	0x0d	/* Rcv alignment error counter RD */
#define EN0_DCFG	0x0e	/* Data configuration reg WR */
#define EN0_COUNTER1	0x0e	/* Rcv CRC error counter RD */
#define EN0_IMR		0x0f	/* Interrupt mask reg WR */
#define EN0_COUNTER2	0x0f	/* Rcv missed frame error counter RD */

#define EN1_PHYS        0x11
#define EN1_CURPAG      0x17
#define EN1_MULT        0x18

#define EN2_STARTPG	0x21	/* Starting page of ring bfr RD */
#define EN2_STOPPG	0x22	/* Ending page +1 of ring bfr RD */

/*  Register accessed at EN_CMD, the 8390 base addr.  */
#define E8390_STOP	0x01	/* Stop and reset the chip */
#define E8390_START	0x02	/* Start the chip, clear reset */
#define E8390_TRANS	0x04	/* Transmit a frame */
#define E8390_RREAD	0x08	/* Remote read */
#define E8390_RWRITE	0x10	/* Remote write  */
#define E8390_NODMA	0x20	/* Remote DMA */
#define E8390_PAGE0	0x00	/* Select page chip registers */
#define E8390_PAGE1	0x40	/* using the two high-order bits */
#define E8390_PAGE2	0x80	/* Page 3 is invalid. */

/* Bits in EN0_ISR - Interrupt status register */
#define ENISR_RX	0x01	/* Receiver, no error */
#define ENISR_TX	0x02	/* Transmitter, no error */
#define ENISR_RX_ERR	0x04	/* Receiver, with error */
#define ENISR_TX_ERR	0x08	/* Transmitter, with error */
#define ENISR_OVER	0x10	/* Receiver overwrote the ring */
#define ENISR_COUNTERS	0x20	/* Counters need emptying */
#define ENISR_RDC	0x40	/* remote dma complete */
#define ENISR_RESET	0x80	/* Reset completed */
#define ENISR_ALL	0x3f	/* Interrupts we will enable */

/* Bits in received packet status byte and EN0_RSR*/
#define ENRSR_RXOK	0x01	/* Received a good packet */
#define ENRSR_CRC	0x02	/* CRC error */
#define ENRSR_FAE	0x04	/* frame alignment error */
#define ENRSR_FO	0x08	/* FIFO overrun */
#define ENRSR_MPA	0x10	/* missed pkt */
#define ENRSR_PHY	0x20	/* physical/multicast address */
#define ENRSR_DIS	0x40	/* receiver disable. set in monitor mode */
#define ENRSR_DEF	0x80	/* deferring */

/* Transmitted packet status, EN0_TSR. */
#define ENTSR_PTX 0x01	/* Packet transmitted without error */
#define ENTSR_ND  0x02	/* The transmit wasn't deferred. */
#define ENTSR_COL 0x04	/* The transmit collided at least once. */
#define ENTSR_ABT 0x08  /* The transmit collided 16 times, and was deferred. */
#define ENTSR_CRS 0x10	/* The carrier sense was lost. */
#define ENTSR_FU  0x20  /* A "FIFO underrun" occurred during transmit. */
#define ENTSR_CDH 0x40	/* The collision detect "heartbeat" signal was lost. */
#define ENTSR_OWC 0x80  /* There was an out-of-window collision. */

#define NE2000_PMEM_SIZE    (32*1024)
#define NE2000_PMEM_START   (16*1024)
#define NE2000_PMEM_END     (NE2000_PMEM_SIZE+NE2000_PMEM_START)
#define NE2000_MEM_SIZE     NE2000_PMEM_END

typedef struct NE2000State {
#ifdef VBOX
    PCIDevice dev;
#endif
    uint8_t cmd;
    uint32_t start;
    uint32_t stop;
    uint8_t boundary;
    uint8_t tsr;
    uint8_t tpsr;
    uint16_t tcnt;
    uint16_t rcnt;
    uint32_t rsar;
    uint8_t rsr;
    uint8_t isr;
    uint8_t dcfg;
    uint8_t imr;
    uint8_t phys[6]; /* mac address */
    uint8_t curpag;
    uint8_t mult[8]; /* multicast mask array */
    int irq;
#ifndef VBOX
    PCIDevice *pci_dev;

    NetDriverState *nd;
#else
    /** Restore timer.
     * This is used to disconnect and reconnect the link after a restore. */
    PTMTIMER                pTimerRestore;
    /** Pointer to the device instance. */
    PPDMDEVINS              pDevIns;
    /** Pointer to the connector of the attached network driver. */
    PPDMINETWORKCONNECTOR   pDrv;
    /** Pointer to the attached network driver. */
    PPDMIBASE               pDrvBase;
    /** The base interface. */
    PDMIBASE                IBase;
    /** The network port interface. */
    PDMINETWORKPORT         INetworkPort;
    /** The network config port interface. */
    PDMINETWORKCONFIG       INetworkConfig;
    /** Base port of the I/O space region. */
    RTIOPORT                IOPortBase;
    /** If set the link is temporarily down because of a saved state load. */
    bool                    fLinkTempDown;
    /** Number of times we've reported the link down. */
    RTUINT                  cLinkDownReported;
    /** The configured MAC address. */
    PDMMAC                  MacConfigured;

    /** The LED. */
    PDMLED                  Led;
    /** The LED ports. */
    PDMILEDPORTS            ILeds;
    /** Partner of ILeds. */
    PPDMILEDCONNECTORS      pLedsConnector;
#endif

    uint8_t mem[NE2000_MEM_SIZE];
} NE2000State;


#ifdef VBOX
#define NE2000STATE_2_DEVINS(pNE2000)           ( (pNE2000)->pDevIns )
#define PCIDEV_2_NE2000STATE(pPciDev)           ( (NE2000State *)(pPciDev) )
#endif /* VBOX */

static void ne2000_reset(NE2000State *s)
{
    int i;

    s->isr = ENISR_RESET;
#ifndef VBOX
    memcpy(s->mem, s->nd->macaddr, 6);
#else /* VBOX */
    Assert(sizeof(s->MacConfigured) == 6);
    memcpy(s->mem, &s->MacConfigured, sizeof(s->MacConfigured));
#endif /* VBOX */
    s->mem[14] = 0x57;
    s->mem[15] = 0x57;

    /* duplicate prom data */
    for(i = 15;i >= 0; i--) {
        s->mem[2 * i] = s->mem[i];
        s->mem[2 * i + 1] = s->mem[i];
    }
}

static void ne2000_update_irq(NE2000State *s)
{
#ifdef VBOX
    PPDMDEVINS pDevIns = NE2000STATE_2_DEVINS(s);
#endif /* VBOX */
    int isr;
    isr = (s->isr & s->imr) & 0x7f;
#if defined(DEBUG_NE2000)
    Log(("NE2000: Set IRQ line %d to %d (%02x %02x)\n",
	   s->irq, isr ? 1 : 0, s->isr, s->imr));
#endif
    if (s->irq == 16) {
        /* PCI irq */
#ifndef VBOX
        pci_set_irq(s->pci_dev, 0, (isr != 0));
#else /* VBOX */
        pDevIns->pDevHlp->pfnPCISetIrq(pDevIns, 0, (isr != 0));
#endif /* VBOX */
    } else {
        /* ISA irq */
#ifndef VBOX
        pic_set_irq(s->irq, (isr != 0));
#else /* VBOX */
        pDevIns->pDevHlp->pfnISASetIrq(pDevIns, 0, (isr != 0));
#endif /* VBOX */
    }
}

/* return the max buffer size if the NE2000 can receive more data */
static int ne2000_can_receive(void *opaque)
{
    NE2000State *s = opaque;
    int avail, index, boundary;

    if (s->cmd & E8390_STOP)
        return 0;
    index = s->curpag << 8;
    boundary = s->boundary << 8;
    if (index < boundary)
        avail = boundary - index;
    else
        avail = (s->stop - s->start) - (index - boundary);
    if (avail < (MAX_ETH_FRAME_SIZE + 4))
        return 0;
    return MAX_ETH_FRAME_SIZE;
}

#define MIN_BUF_SIZE 60

static void ne2000_receive(void *opaque, const uint8_t *buf, int size)
{
    NE2000State *s = opaque;
    uint8_t *p;
    int total_len, next, avail, len, index;
    uint8_t buf1[60];

#ifdef DEBUG_NE2000
    Log(("NE2000: received len=%d\n", size));
#endif
    /* if too small buffer, then expand it */
    if (size < MIN_BUF_SIZE) {
        memcpy(buf1, buf, size);
        memset(buf1 + size, 0, MIN_BUF_SIZE - size);
        buf = buf1;
        size = MIN_BUF_SIZE;
    }

    index = s->curpag << 8;
    /* 4 bytes for header */
    total_len = size + 4;
    /* address for next packet (4 bytes for CRC) */
    next = index + ((total_len + 4 + 255) & ~0xff);
    if (next >= s->stop)
        next -= (s->stop - s->start);
    /* prepare packet header */
    p = s->mem + index;
    s->rsr = ENRSR_RXOK; /* receive status */
    /* XXX: check this */
    if (buf[0] & 0x01)
        s->rsr |= ENRSR_PHY;
    p[0] = s->rsr;
    p[1] = next >> 8;
    p[2] = total_len;
    p[3] = total_len >> 8;
    index += 4;

    /* write packet data */
    while (size > 0) {
        avail = s->stop - index;
        len = size;
        if (len > avail)
            len = avail;
        memcpy(s->mem + index, buf, len);
        buf += len;
        index += len;
        if (index == s->stop)
            index = s->start;
        size -= len;
    }
    s->curpag = next >> 8;

    /* now we can signal we have receive something */
    s->isr |= ENISR_RX;
    ne2000_update_irq(s);
}

static void ne2000_ioport_write(void *opaque, uint32_t addr, uint32_t val)
{
    NE2000State *s = opaque;
    int offset, page, index;

    addr &= 0xf;
#ifdef DEBUG_NE2000
    Log(("NE2000: write addr=0x%x val=0x%02x\n", addr, val));
#endif
    if (addr == E8390_CMD) {
        /* control register */
        s->cmd = val;
        if (!(val & E8390_STOP)) { /* START bit makes no sense on RTL8029... */
            s->isr &= ~ENISR_RESET;
            /* test specific case: zero length transfert */
            if ((val & (E8390_RREAD | E8390_RWRITE)) &&
                s->rcnt == 0) {
                s->isr |= ENISR_RDC;
                ne2000_update_irq(s);
            }
            if (val & E8390_TRANS) {
                index = (s->tpsr << 8);
                /* XXX: next 2 lines are a hack to make netware 3.11 work */
                Assert(index < NE2000_PMEM_END);
                if (index >= NE2000_PMEM_END)
                    index -= NE2000_PMEM_SIZE;
                /* fail safe: check range on the transmitted length  */
                Assert(index + s->tcnt <= NE2000_PMEM_END);
                if (index + s->tcnt <= NE2000_PMEM_END) {
#ifndef VBOX
                    qemu_send_packet(s->nd, s->mem + index, s->tcnt);
#else
                    if (s->tcnt > 70) /* unqualified guess */
                        s->Led.Asserted.s.fWriting = s->Led.Actual.s.fWriting = 1;
                    s->pDrv->pfnSend(s->pDrv, s->mem + index, s->tcnt);
                    s->Led.Actual.s.fWriting = 0;
#endif
                }
                /* signal end of transfert */
                s->tsr = ENTSR_PTX;
                s->isr |= ENISR_TX;
                s->cmd &= ~E8390_TRANS;
                ne2000_update_irq(s);
            }
        }
    } else {
        page = s->cmd >> 6;
        offset = addr | (page << 4);
        switch(offset) {
        case EN0_STARTPG:
            s->start = val << 8;
            break;
        case EN0_STOPPG:
            s->stop = val << 8;
            break;
        case EN0_BOUNDARY:
            s->boundary = val;
            break;
        case EN0_IMR:
            s->imr = val;
            ne2000_update_irq(s);
            break;
        case EN0_TPSR:
            s->tpsr = val;
            break;
        case EN0_TCNTLO:
            s->tcnt = (s->tcnt & 0xff00) | val;
            break;
        case EN0_TCNTHI:
            s->tcnt = (s->tcnt & 0x00ff) | (val << 8);
            break;
        case EN0_RSARLO:
            s->rsar = (s->rsar & 0xff00) | val;
            break;
        case EN0_RSARHI:
            s->rsar = (s->rsar & 0x00ff) | (val << 8);
            break;
        case EN0_RCNTLO:
            s->rcnt = (s->rcnt & 0xff00) | val;
            break;
        case EN0_RCNTHI:
            s->rcnt = (s->rcnt & 0x00ff) | (val << 8);
            break;
        case EN0_DCFG:
            s->dcfg = val;
            break;
        case EN0_ISR:
            s->isr &= ~(val & 0x7f);
            ne2000_update_irq(s);
            break;
#ifdef VBOX
        case EN1_PHYS:
        case EN1_PHYS + 1:
        case EN1_PHYS + 2:
        case EN1_PHYS + 3:
        case EN1_PHYS + 4:
        case EN1_PHYS + 5:
#else
        case EN1_PHYS ... EN1_PHYS + 5:
#endif
            s->phys[offset - EN1_PHYS] = val;
            break;
        case EN1_CURPAG:
            s->curpag = val;
            break;
#ifdef VBOX
        case EN1_MULT:
        case EN1_MULT + 1:
        case EN1_MULT + 2:
        case EN1_MULT + 3:
        case EN1_MULT + 4:
        case EN1_MULT + 5:
        case EN1_MULT + 6:
        case EN1_MULT + 7:
#else
        case EN1_MULT ... EN1_MULT + 7:
#endif
            s->mult[offset - EN1_MULT] = val;
            break;
        }
    }
}

static uint32_t ne2000_ioport_read(void *opaque, uint32_t addr)
{
    NE2000State *s = opaque;
    int offset, page, ret;

    addr &= 0xf;
    if (addr == E8390_CMD) {
        ret = s->cmd;
    } else {
        page = s->cmd >> 6;
        offset = addr | (page << 4);
        switch(offset) {
        case EN0_TSR:
            ret = s->tsr;
            break;
        case EN0_BOUNDARY:
            ret = s->boundary;
            break;
        case EN0_ISR:
            ret = s->isr;
            break;
    	case EN0_RSARLO:
	        ret = s->rsar & 0x00ff;
	        break;
	    case EN0_RSARHI:
    	    ret = s->rsar >> 8;
	        break;
#ifdef VBOX
        case EN1_PHYS:
        case EN1_PHYS + 1:
        case EN1_PHYS + 2:
        case EN1_PHYS + 3:
        case EN1_PHYS + 4:
        case EN1_PHYS + 5:
#else
        case EN1_PHYS ... EN1_PHYS + 5:
#endif
            ret = s->phys[offset - EN1_PHYS];
            break;
        case EN1_CURPAG:
            ret = s->curpag;
            break;
#ifdef VBOX
        case EN1_MULT:
        case EN1_MULT + 1:
        case EN1_MULT + 2:
        case EN1_MULT + 3:
        case EN1_MULT + 4:
        case EN1_MULT + 5:
        case EN1_MULT + 6:
        case EN1_MULT + 7:
#else
        case EN1_MULT ... EN1_MULT + 7:
#endif
            ret = s->mult[offset - EN1_MULT];
            break;
        case EN0_RSR:
            ret = s->rsr;
            break;
        case EN2_STARTPG:
            ret = s->start >> 8;
            break;
        case EN2_STOPPG:
            ret = s->stop >> 8;
            break;
        default:
            ret = 0x00;
            break;
        }
    }
#ifdef DEBUG_NE2000
    Log(("NE2000: read addr=0x%x val=%02x\n", addr, ret));
#endif
    return ret;
}

static inline void ne2000_mem_writeb(NE2000State *s, uint32_t addr,
                                     uint32_t val)
{
    if (addr < 32 ||
        (addr >= NE2000_PMEM_START && addr < NE2000_MEM_SIZE)) {
        s->mem[addr] = val;
    }
}

static inline void ne2000_mem_writew(NE2000State *s, uint32_t addr,
                                     uint32_t val)
{
    addr &= ~1; /* XXX: check exact behaviour if not even */
    if (addr < 32 ||
        (addr >= NE2000_PMEM_START && addr < NE2000_MEM_SIZE)) {
        *(uint16_t *)(s->mem + addr) = cpu_to_le16(val);
    }
}

static inline void ne2000_mem_writel(NE2000State *s, uint32_t addr,
                                     uint32_t val)
{
    addr &= ~1; /* XXX: check exact behaviour if not even */
    if (addr < 32 ||
        (addr >= NE2000_PMEM_START && addr < NE2000_MEM_SIZE)) {
        cpu_to_le32wu((uint32_t *)(s->mem + addr), val);
    }
}

static inline uint32_t ne2000_mem_readb(NE2000State *s, uint32_t addr)
{
    if (addr < 32 ||
        (addr >= NE2000_PMEM_START && addr < NE2000_MEM_SIZE)) {
        return s->mem[addr];
    } else {
        return 0xff;
    }
}

static inline uint32_t ne2000_mem_readw(NE2000State *s, uint32_t addr)
{
    addr &= ~1; /* XXX: check exact behaviour if not even */
    if (addr < 32 ||
        (addr >= NE2000_PMEM_START && addr < NE2000_MEM_SIZE)) {
        return le16_to_cpu(*(uint16_t *)(s->mem + addr));
    } else {
        return 0xffff;
    }
}

static inline uint32_t ne2000_mem_readl(NE2000State *s, uint32_t addr)
{
    addr &= ~1; /* XXX: check exact behaviour if not even */
    if (addr < 32 ||
        (addr >= NE2000_PMEM_START && addr < NE2000_MEM_SIZE)) {
        return le32_to_cpupu((uint32_t *)(s->mem + addr));
    } else {
        return 0xffffffff;
    }
}

static inline void ne2000_dma_update(NE2000State *s, int len)
{
    s->rsar += len;
    /* wrap */
    /* XXX: check what to do if rsar > stop */
    if (s->rsar == s->stop)
        s->rsar = s->start;

    if (s->rcnt <= len) {
        s->rcnt = 0;
        /* signal end of transfert */
        s->isr |= ENISR_RDC;
        ne2000_update_irq(s);
    } else {
        s->rcnt -= len;
    }
}

static void ne2000_asic_ioport_write(void *opaque, uint32_t addr, uint32_t val)
{
    NE2000State *s = opaque;

#ifdef DEBUG_NE2000
    Log(("NE2000: asic write val=0x%04x\n", val));
#endif
    if (s->rcnt == 0)
        return;
    if (s->dcfg & 0x01) {
        /* 16 bit access */
        ne2000_mem_writew(s, s->rsar, val);
        ne2000_dma_update(s, 2);
    } else {
        /* 8 bit access */
        ne2000_mem_writeb(s, s->rsar, val);
        ne2000_dma_update(s, 1);
    }
}

static uint32_t ne2000_asic_ioport_read(void *opaque, uint32_t addr)
{
    NE2000State *s = opaque;
    int ret;

    if (s->dcfg & 0x01) {
        /* 16 bit access */
        ret = ne2000_mem_readw(s, s->rsar);
        ne2000_dma_update(s, 2);
    } else {
        /* 8 bit access */
        ret = ne2000_mem_readb(s, s->rsar);
        ne2000_dma_update(s, 1);
    }
#ifdef DEBUG_NE2000
    Log(("NE2000: asic read val=0x%04x\n", ret));
#endif
    return ret;
}

static void ne2000_asic_ioport_writel(void *opaque, uint32_t addr, uint32_t val)
{
    NE2000State *s = opaque;

#ifdef DEBUG_NE2000
    Log(("NE2000: asic writel val=0x%04x\n", val));
#endif
    if (s->rcnt == 0)
        return;
    /* 32 bit access */
    ne2000_mem_writel(s, s->rsar, val);
    ne2000_dma_update(s, 4);
}

static uint32_t ne2000_asic_ioport_readl(void *opaque, uint32_t addr)
{
    NE2000State *s = opaque;
    int ret;

    /* 32 bit access */
    ret = ne2000_mem_readl(s, s->rsar);
    ne2000_dma_update(s, 4);
#ifdef DEBUG_NE2000
    Log(("NE2000: asic readl val=0x%04x\n", ret));
#endif
    return ret;
}

static void ne2000_reset_ioport_write(void *opaque, uint32_t addr, uint32_t val)
{
    /* nothing to do (end of reset pulse) */
}

static uint32_t ne2000_reset_ioport_read(void *opaque, uint32_t addr)
{
    NE2000State *s = opaque;
    ne2000_reset(s);
    return 0;
}

static void ne2000_save(QEMUFile* f,void* opaque)
{
	NE2000State* s=(NE2000State*)opaque;

	qemu_put_8s(f, &s->cmd);
	qemu_put_be32s(f, &s->start);
	qemu_put_be32s(f, &s->stop);
	qemu_put_8s(f, &s->boundary);
	qemu_put_8s(f, &s->tsr);
	qemu_put_8s(f, &s->tpsr);
	qemu_put_be16s(f, &s->tcnt);
	qemu_put_be16s(f, &s->rcnt);
	qemu_put_be32s(f, &s->rsar);
	qemu_put_8s(f, &s->rsr);
	qemu_put_8s(f, &s->isr);
	qemu_put_8s(f, &s->dcfg);
	qemu_put_8s(f, &s->imr);
	qemu_put_buffer(f, s->phys, 6);
	qemu_put_8s(f, &s->curpag);
	qemu_put_buffer(f, s->mult, 8);
	qemu_put_be32s(f, &s->irq);
	qemu_put_buffer(f, s->mem, NE2000_MEM_SIZE);
}

static int ne2000_load(QEMUFile* f,void* opaque,int version_id)
{
	NE2000State* s=(NE2000State*)opaque;

	if (version_id != 1)
#ifdef VBOX
        return -1;
#else
            return -EINVAL;
#endif
	qemu_get_8s(f, &s->cmd);
	qemu_get_be32s(f, &s->start);
	qemu_get_be32s(f, &s->stop);
	qemu_get_8s(f, &s->boundary);
	qemu_get_8s(f, &s->tsr);
	qemu_get_8s(f, &s->tpsr);
	qemu_get_be16s(f, &s->tcnt);
	qemu_get_be16s(f, &s->rcnt);
	qemu_get_be32s(f, &s->rsar);
	qemu_get_8s(f, &s->rsr);
	qemu_get_8s(f, &s->isr);
	qemu_get_8s(f, &s->dcfg);
	qemu_get_8s(f, &s->imr);
	qemu_get_buffer(f, s->phys, 6);
	qemu_get_8s(f, &s->curpag);
	qemu_get_buffer(f, s->mult, 8);
	qemu_get_be32s(f, &s->irq);
	qemu_get_buffer(f, s->mem, NE2000_MEM_SIZE);

	return 0;
}

#ifndef VBOX

void isa_ne2000_init(int base, int irq, NetDriverState *nd)
{
    NE2000State *s;

    s = qemu_mallocz(sizeof(NE2000State));
    if (!s)
        return;

    register_ioport_write(base, 16, 1, ne2000_ioport_write, s);
    register_ioport_read(base, 16, 1, ne2000_ioport_read, s);

    register_ioport_write(base + 0x10, 1, 1, ne2000_asic_ioport_write, s);
    register_ioport_read(base + 0x10, 1, 1, ne2000_asic_ioport_read, s);
    register_ioport_write(base + 0x10, 2, 2, ne2000_asic_ioport_write, s);
    register_ioport_read(base + 0x10, 2, 2, ne2000_asic_ioport_read, s);

    register_ioport_write(base + 0x1f, 1, 1, ne2000_reset_ioport_write, s);
    register_ioport_read(base + 0x1f, 1, 1, ne2000_reset_ioport_read, s);
    s->irq = irq;
    s->nd = nd;

    ne2000_reset(s);

    qemu_add_read_packet(nd, ne2000_can_receive, ne2000_receive, s);

    register_savevm("ne2000", 0, 1, ne2000_save, ne2000_load, s);

}

/***********************************************************/
/* PCI NE2000 definitions */

typedef struct PCINE2000State {
    PCIDevice dev;
    NE2000State ne2000;
} PCINE2000State;

static void ne2000_map(PCIDevice *pci_dev, int region_num,
                       uint32_t addr, uint32_t size, int type)
{
    PCINE2000State *d = (PCINE2000State *)pci_dev;
    NE2000State *s = &d->ne2000;

    register_ioport_write(addr, 16, 1, ne2000_ioport_write, s);
    register_ioport_read(addr, 16, 1, ne2000_ioport_read, s);

    register_ioport_write(addr + 0x10, 1, 1, ne2000_asic_ioport_write, s);
    register_ioport_read(addr + 0x10, 1, 1, ne2000_asic_ioport_read, s);
    register_ioport_write(addr + 0x10, 2, 2, ne2000_asic_ioport_write, s);
    register_ioport_read(addr + 0x10, 2, 2, ne2000_asic_ioport_read, s);
    register_ioport_write(addr + 0x10, 4, 4, ne2000_asic_ioport_writel, s);
    register_ioport_read(addr + 0x10, 4, 4, ne2000_asic_ioport_readl, s);

    register_ioport_write(addr + 0x1f, 1, 1, ne2000_reset_ioport_write, s);
    register_ioport_read(addr + 0x1f, 1, 1, ne2000_reset_ioport_read, s);
}

void pci_ne2000_init(PCIBus *bus, NetDriverState *nd)
{
    PCINE2000State *d;
    NE2000State *s;
    uint8_t *pci_conf;

    d = (PCINE2000State *)pci_register_device(bus,
                                              "NE2000", sizeof(PCINE2000State),
                                              -1,
                                              NULL, NULL);
    pci_conf = d->dev.config;
    pci_conf[0x00] = 0xec; // Realtek 8029
    pci_conf[0x01] = 0x10;
    pci_conf[0x02] = 0x29;
    pci_conf[0x03] = 0x80;
    pci_conf[0x0a] = 0x00; // ethernet network controller
    pci_conf[0x0b] = 0x02;
    pci_conf[0x0e] = 0x00; // header_type
    pci_conf[0x3d] = 1; // interrupt pin 0

    pci_register_io_region(&d->dev, 0, 0x100,
                           PCI_ADDRESS_SPACE_IO, ne2000_map);
    s = &d->ne2000;
    s->irq = 16; // PCI interrupt
    s->pci_dev = (PCIDevice *)d;
    s->nd = nd;
    ne2000_reset(s);
    qemu_add_read_packet(nd, ne2000_can_receive, ne2000_receive, s);

    /* XXX: instance number ? */
    register_savevm("ne2000", 0, 1, ne2000_save, ne2000_load, s);
    register_savevm("ne2000_pci", 0, 1, generic_pci_save, generic_pci_load,
                    &d->dev);
}

#else

/**
 * Port I/O Handler for IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   uPort       Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
static DECLCALLBACK(int) ne2000IOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    switch (cb)
    {
        case 1: *pu32 = ne2000_ioport_read(pvUser, Port); break;
        default:
            return VERR_IOM_IOPORT_UNUSED;
    }
    return VINF_SUCCESS;
}


/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   uPort       Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
static DECLCALLBACK(int) ne2000IOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    switch (cb)
    {
        case 1: ne2000_ioport_write(pvUser, Port, u32); break;
        default:
            return VERR_IOM_IOPORT_UNUSED;
    }
    return VINF_SUCCESS;
}

/**
 * Port I/O Handler for IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   uPort       Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
static DECLCALLBACK(int) ne2000ASICIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    switch (cb)
    {
        case 1:
        case 2: *pu32 = ne2000_asic_ioport_read(pvUser, Port);  break;
        case 4: *pu32 = ne2000_asic_ioport_readl(pvUser, Port); break;
        default:
            return VERR_IOM_IOPORT_UNUSED;
    }
    return VINF_SUCCESS;
}


/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   uPort       Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
static DECLCALLBACK(int) ne2000ASICIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    switch (cb)
    {
        case 1:
        case 2: ne2000_asic_ioport_write(pvUser, Port, u32);  break;
        case 4: ne2000_asic_ioport_writel(pvUser, Port, u32); break;
        default:
            return VERR_IOM_IOPORT_UNUSED;
    }
    return VINF_SUCCESS;
}


/**
 * Port I/O Handler for IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   uPort       Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
static DECLCALLBACK(int) ne2000ResetIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    switch (cb)
    {
        case 1: *pu32 = ne2000_reset_ioport_read(pvUser, Port); break;
        default:
            return VERR_IOM_IOPORT_UNUSED;
    }
    return VINF_SUCCESS;
}


/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   uPort       Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
static DECLCALLBACK(int) ne2000ResetIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    switch (cb)
    {
        case 1: ne2000_reset_ioport_write(pvUser, Port, u32); break;
        default:
            return VERR_IOM_IOPORT_UNUSED;
    }
    return VINF_SUCCESS;
}

/**
 * Callback function for mapping an PCI I/O region.
 *
 * @return VBox status code.
 * @param   pPciDev         Pointer to PCI device. Use pPciDev->pDevIns to get the device instance.
 * @param   iRegion         The region number.
 * @param   GCPhysAddress   Physical address of the region. If iType is PCI_ADDRESS_SPACE_IO, this is an
 *                          I/O port, else it's a physical address.
 *                          This address is *NOT* relative to pci_mem_base like earlier!
 * @param   enmType         One of the PCI_ADDRESS_SPACE_* values.
 */
static DECLCALLBACK(int) ne2000IOPortMap(PPCIDEVICE pPciDev, /*unsigned*/ int iRegion, RTGCPHYS GCPhysAddress, uint32_t cb, PCIADDRESSSPACE enmType)
{
    int         rc;
    PPDMDEVINS  pDevIns = pPciDev->pDevIns;
    RTIOPORT  Port = (RTIOPORT)GCPhysAddress;
    NE2000State *pData = PCIDEV_2_NE2000STATE(pPciDev);

    Assert(enmType == PCI_ADDRESS_SPACE_IO);
    Assert(cb >= 0x20);

    rc = pDevIns->pDevHlp->pfnIOPortRegister(pDevIns, Port, 0x10, pData, ne2000IOPortWrite, ne2000IOPortRead, NULL, NULL,  "ne2000");
    if (VBOX_FAILURE(rc))
        return rc;

    rc = pDevIns->pDevHlp->pfnIOPortRegister(pDevIns, Port + 0x10, 0x04, pData, ne2000ASICIOPortWrite, ne2000ASICIOPortRead, NULL, NULL,  "ne2000 ASIC");
    if (VBOX_FAILURE(rc))
        return rc;

    rc = pDevIns->pDevHlp->pfnIOPortRegister(pDevIns, Port + 0x1f, 0x01, pData, ne2000ResetIOPortWrite, ne2000ResetIOPortRead, NULL, NULL,  "ne2000 Reset");
    if (VBOX_FAILURE(rc))
        return rc;

    pData->IOPortBase = Port;
    return VINF_SUCCESS;
}

/**
 * Restore timer callback.
 *
 * This is only called when've restored a saved state and temporarily
 * disconnected the network link to inform the guest that network connections
 * should be considered lost.
 *
 * @param   pDevIns         Device instance of the device which registered the timer.
 * @param   pTimer          The timer handle.
 */
static DECLCALLBACK(void) ne2000TimerRestore(PPDMDEVINS pDevIns, PTMTIMER pTimer)
{
    NE2000State *pData = PDMINS2DATA(pDevIns, NE2000State *);

    int rc = VERR_GENERAL_FAILURE;
    if (pData->cLinkDownReported <= 3)
        rc = TMTimerSetMillies(pData->pTimerRestore, 1500);
    if (VBOX_FAILURE(rc))
    {
        Log(("ne2000TimerRestore: Clearing ERR and CERR after load. cLinkDownReported=%d\n", pData->cLinkDownReported));
        pData->fLinkTempDown = false;
        AssertFailed();
////        pData->csr[0] &= ~(BIT(15) | BIT(13)); /* ERR | CERR - probably not 100% correct either... */
        pData->Led.Actual.s.fError = 0;
    }
    else
        Log(("ne2000TimerRestore: cLinkDownReported=%d, wait another 1500ms...\n", pData->cLinkDownReported));
}


/**
 * Saves a state of the PC-Net II device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to save the state to.
 */
static DECLCALLBACK(int) ne2000SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle)
{
    NE2000State *pData = PDMINS2DATA(pDevIns, NE2000State *);

    ne2000_save(pSSMHandle, pData);

    SSMR3PutMem(pSSMHandle, &pData->MacConfigured, sizeof(pData->MacConfigured));

    return VINF_SUCCESS;
}


/**
 * Loads a saved PC-Net II device state.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to the saved state.
 * @param   u32Version  The data unit version number.
 */
static DECLCALLBACK(int) ne2000LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle, uint32_t u32Version)
{
    NE2000State *pData = PDMINS2DATA(pDevIns, NE2000State *);
    PDMMAC      Mac;
    if (u32Version != 1)
    {
        AssertFailed();
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    /* restore data */
    ne2000_load(pSSMHandle, pData, u32Version);

    SSMR3GetMem(pSSMHandle, &Mac, sizeof(Mac));
    Assert(!memcmp(&Mac, &pData->MacConfigured, sizeof(Mac)));

    /* Indicate link down to the guest OS that all network connections have been lost. */
    pData->fLinkTempDown = true;
    pData->cLinkDownReported = 0;
    AssertFailed();
    /*
     * todo !!!!!
     */
    //////pData->csr[0] |= BIT(15) | BIT(13); /* ERR | CERR (this is probably wrong) */

    pData->Led.Asserted.s.fError = pData->Led.Actual.s.fError = 1;
    return TMTimerSetMillies(pData->pTimerRestore, 5000);
}


/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 * @thread  Any thread.
 */
static DECLCALLBACK(void *) ne2000QueryInterface(struct PDMIBASE *pInterface, PDMINTERFACE enmInterface)
{
    NE2000State *pData = (NE2000State *)((uintptr_t)pInterface - RT_OFFSETOF(NE2000State, IBase));
    Assert(&pData->IBase == pInterface);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pData->IBase;
        case PDMINTERFACE_NETWORK_PORT:
            return &pData->INetworkPort;
        case PDMINTERFACE_NETWORK_CONFIG:
            return &pData->INetworkConfig;
        case PDMINTERFACE_LED_PORTS:
            return &pData->ILeds;
        default:
            return NULL;
    }
}

/** Converts a pointer to NE2000State::INetworkPort to a NE2000State pointer. */
#define INETWORKPORT_2_DATA(pInterface)  ( (NE2000State *)((uintptr_t)pInterface - RT_OFFSETOF(NE2000State, INetworkPort)) )

/**
 * Check if the device/driver can receive data now.
 * This must be called before the pfnRecieve() method is called.
 *
 * @returns Number of bytes the driver can receive.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @thread  EMT
 */
static DECLCALLBACK(size_t) ne2000CanReceive(PPDMINETWORKPORT pInterface)
{
    NE2000State *pData = INETWORKPORT_2_DATA(pInterface);
    return ne2000_can_receive(pData);
}


/**
 * Receive data from the network.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   pvBuf           The available data.
 * @param   cb              Number of bytes available in the buffer.
 * @thread  EMT
 */
static DECLCALLBACK(int) ne2000Receive(PPDMINETWORKPORT pInterface, const void *pvBuf, size_t cb)
{
    NE2000State *pData = INETWORKPORT_2_DATA(pInterface);

    if (cb > 70) /* unqualified guess */
        pData->Led.Asserted.s.fReading = pData->Led.Actual.s.fReading = 1;
    ne2000_receive(pData, pvBuf, cb);
    pData->Led.Actual.s.fReading = 0;

    return VINF_SUCCESS;
}

/** Converts a pointer to NE2000State::INetworkConfig to a NE2000State pointer. */
#define INETWORKCONFIG_2_DATA(pInterface)  ( (NE2000State *)((uintptr_t)pInterface - RT_OFFSETOF(NE2000State, INetworkConfig)) )


/**
 * Gets the current Media Access Control (MAC) address.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   pMac            Where to store the MAC address.
 * @thread  EMT
 */
static DECLCALLBACK(int) ne2000GetMac(PPDMINETWORKCONFIG pInterface, PPDMMAC *pMac)
{
    NE2000State *pData = INETWORKCONFIG_2_DATA(pInterface);
    memcpy(pMac, pData->mem, sizeof(*pMac));
    return VINF_SUCCESS;
}


/**
 * Gets the pointer to the status LED of a unit.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   iLUN            The unit which status LED we desire.
 * @param   ppLed           Where to store the LED pointer.
 */
static DECLCALLBACK(int) ne2000QueryStatusLed(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed)
{
    NE2000State *pData = (NE2000State *)( (uintptr_t)pInterface - RT_OFFSETOF(NE2000State, ILeds) );
    if (iLUN == 0)
    {
        *ppLed = &pData->Led;
        return VINF_SUCCESS;
    }
    return VERR_PDM_LUN_NOT_FOUND;
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
static DECLCALLBACK(int) ne2000Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    NE2000State     *pData = PDMINS2DATA(pDevIns, NE2000State *);
    PPDMIBASE       pBase;
    int             rc;
    Assert(iInstance == 0);

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "MAC\0CableConnected\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    /*
     * Read the configuration.
     */
    rc = CFGMR3QueryBytes(pCfgHandle, "MAC", &pData->MacConfigured, sizeof(pData->MacConfigured));
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Failed to get the \"MAC\" value, rc=%Vrc\n", rc));
        return rc;
    }

    /*
     * Initialize data (most of it anyway).
     */
    pData->pDevIns                      = pDevIns;
    pData->Led.u32Magic                 = PDMLED_MAGIC;
    /* IBase */
    pData->IBase.pfnQueryInterface      = ne2000QueryInterface;
    /* INeworkPort */
    pData->INetworkPort.pfnCanReceive   = ne2000CanReceive;
    pData->INetworkPort.pfnReceive      = ne2000Receive;
    /* INetworkConfig */
    pData->INetworkConfig.pfnGetMac = ne2000GetMac;
    /** @todo link state */
    /* ILeds */
    pData->ILeds.pfnQueryStatusLed      = ne2000QueryStatusLed;


    /* PCI Device */
    pData->dev.config[0x00] = 0xec; // Realtek 8029
    pData->dev.config[0x01] = 0x10;
    pData->dev.config[0x02] = 0x29;
    pData->dev.config[0x03] = 0x80;
    pData->dev.config[0x0a] = 0x00; // ethernet network controller
    pData->dev.config[0x0b] = 0x02;
    pData->dev.config[0x0e] = 0x00; // header_type
    pData->dev.config[0x3d] = 1; // interrupt pin 0

    /*
     * Register the PCI device, its I/O regions, the timer and the saved state item.
     */
    rc = pDevIns->pDevHlp->pfnPCIRegister(pDevIns, &pData->dev);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pDevIns->pDevHlp->pfnPCIIORegionRegister(pDevIns, 0, 0x100, PCI_ADDRESS_SPACE_IO, ne2000IOPortMap);
    if (VBOX_FAILURE(rc))
        return rc;

    rc = pDevIns->pDevHlp->pfnTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL, ne2000TimerRestore, "NE2000 Restore Timer", &pData->pTimerRestore);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("pfnTMTimerCreate -> %Vrc\n", rc));
        return rc;
    }

    rc = pDevIns->pDevHlp->pfnSSMRegister(pDevIns, pDevIns->pDevReg->szDeviceName, iInstance, 1 /* version */, sizeof(*pData),
                                          NULL, ne2000SaveExec, NULL,
                                          NULL, ne2000LoadExec, NULL);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Attach status driver (optional).
     */
    rc = PDMDevHlpDriverAttach(pDevIns, PDM_STATUS_LUN, &pData->IBase, &pBase, "Status Port");
    if (VBOX_SUCCESS(rc))
        pData->pLedsConnector = pBase->pfnQueryInterface(pBase, PDMINTERFACE_LED_CONNECTORS);
    else if (rc != VERR_PDM_NO_ATTACHED_DRIVER)
    {
        AssertMsgFailed(("Failed to attach to status driver. rc=%Vrc\n", rc));
        return rc;
    }

    /*
     * Attach driver.
     */
    rc = PDMDevHlpDriverAttach(pDevIns, 0, &pData->IBase, &pData->pDrvBase, "Network Port");
    if (VBOX_SUCCESS(rc))
    {
        pData->pDrv = (PPDMINETWORKCONNECTOR)pData->pDrvBase->pfnQueryInterface(pData->pDrvBase, PDMINTERFACE_NETWORK_CONNECTOR);
        if (!pData->pDrv)
        {
            AssertMsgFailed(("Failed to obtain the PDMINTERFACE_NETWORK_CONNECTOR interface!\n"));
            return VERR_PDM_MISSING_INTERFACE_BELOW;
        }
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        Log(("ne2000: No attached driver!\n"));
    else
        return rc;

    pData->irq = 16; // PCI interrupt

    /*
     * Reset the device state (will need pDrv later).
     */
    ne2000_reset(pData);

    return VINF_SUCCESS;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceNE2000 =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szDeviceName */
    "ne2000",
    /* szGCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "NE2000 Ethernet controller.\n",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_DEFAULT,
    /* fClass */
    PDM_DEVREG_CLASS_NETWORK,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(NE2000State),
    /* pfnConstruct */
    ne2000Construct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL, /** @todo check if this card is actually reset in anyway... */
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

#endif /* VBOX */
