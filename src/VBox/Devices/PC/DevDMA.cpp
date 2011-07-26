/* $Id$ */
/** @file
 * DevDMA - DMA Controller Device.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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
 * This code is loosely based on:
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_DMA
#include <VBox/vmm/pdmdev.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/string.h>

#include <stdio.h>
#include <stdlib.h>

#include "VBoxDD.h"


/* DMA Overview and notes
 *
 * Modern PCs typically emulate AT-compatible DMA. The IBM PC/AT used dual
 * cascaded 8237A DMA controllers, augmented with a 74LS612 memory mapper.
 * The 8237As are 8-bit parts, only capable of addressing up to 64KB; the
 * 74LS612 extends addressing to 24 bits. That leads to well known and
 * inconvenient DMA limitations:
 *  - DMA can only access physical memory under the 16MB line
 *  - DMA transfers must occur within a 64KB/128KB 'page'
 *
 * The 16-bit DMA controller added in the PC/AT shifts all 8237A addresses
 * left by one, including the control registers addresses. The DMA register
 * offsets (except for the page registers) are therefore "double spaced".
 *
 * Due to the address shifting, the DMA controller decodes more addresses
 * than are usually documented, with aliasing. See the ICH8 datasheet.
 *
 * In the IBM PC and PC/XT, DMA channel 0 was used for memory refresh, thus
 * preventing the use of memory-to-memory DMA transfers (which use channels
 * 0 and 1). In the PC/AT, memory-to-memory DMA was theoretically possible.
 * However, it would transfer a single byte at a time, while the CPU can
 * transfer two (on a 286) or four (on a 386+) bytes at a time. On many
 * compatibles, memory-to-memory DMA is not even implemented at all, and
 * therefore has no practical use.
 *
 * Auto-init mode is handled implicitly; a device's transfer handler may
 * return an end count lower than the start count.
 *
 * Naming convention: 'channel' refers to a system-wide DMA channel (0-7)
 * while 'chidx' refers to a DMA channel index within a controller (0-3).
 *
 * References:
 *  - IBM Personal Computer AT Technical Reference, 1984
 *  - Intel 8237A-5 Datasheet, 1993
 *  - Frank van Gilluwe, The Undocumented PC, 1994
 *  - OPTi 82C206 Data Book, 1996 (or Chips & Tech 82C206)
 *  - Intel ICH8 Datasheet, 2007
 */


/* Saved state versions. */
#define DMA_SAVESTATE_OLD       1   /* The original saved state. */
#define DMA_SAVESTATE_CURRENT   2   /* The new and improved saved state. */

/* State information for a single DMA channel. */
typedef struct {
    void                    *pvUser;        /* User specific context. */
    PFNDMATRANSFERHANDLER   pfnXferHandler; /* Transfer handler for channel. */
    uint16_t                u16BaseAddr;    /* Base address for transfers. */
    uint16_t                u16BaseCount;   /* Base count for transfers. */
    uint16_t                u16CurAddr;     /* Current address. */
    uint16_t                u16CurCount;    /* Current count. */
    uint8_t                 u8Mode;         /* Channel mode. */
} DMAChannel;

/* State information for a DMA controller (DMA8 or DMA16). */
typedef struct {
    DMAChannel  ChState[4];     /* Per-channel state. */
    uint8_t     au8Page[8];     /* Page registers (A16-A23). */
    uint8_t     au8PageHi[8];   /* High page registers (A24-A31). */
    uint8_t     u8Command;      /* Command register. */
    uint8_t     u8Status;       /* Status register. */
    uint8_t     u8Mask;         /* Mask register. */
    uint8_t     u8Temp;         /* Temporary (mem/mem) register. */
    uint8_t     u8ModeCtr;      /* Mode register counter for reads. */
    bool        bHiByte;        /* Byte pointer (T/F -> high/low). */
    uint32_t    is16bit;        /* True for 16-bit DMA. */
} DMAControl;

/* Complete DMA state information. */
typedef struct {
    PPDMDEVINS      pDevIns;    /* Device instance. */
    PCPDMDMACHLP    pHlp;       /* PDM DMA helpers. */
    DMAControl      DMAC[2];    /* Two DMA controllers. */
} DMAState;

/* DMA command register bits. */
enum {
    CMD_MEMTOMEM    = 0x01,     /* Enable mem-to-mem trasfers. */
    CMD_ADRHOLD     = 0x02,     /* Address hold for mem-to-mem. */
    CMD_DISABLE     = 0x04,     /* Disable controller. */
    CMD_COMPRTIME   = 0x08,     /* Compressed timing. */
    CMD_ROTPRIO     = 0x10,     /* Rotating priority. */
    CMD_EXTWR       = 0x20,     /* Extended write. */
    CMD_DREQHI      = 0x40,     /* DREQ is active high if set. */
    CMD_DACKHI      = 0x80,     /* DACK is active high if set. */
    CMD_UNSUPPORTED = CMD_MEMTOMEM | CMD_ADRHOLD | CMD_COMPRTIME
                    | CMD_EXTWR | CMD_DREQHI | CMD_DACKHI
};

/* DMA control register offsets for read accesses. */
enum {
    CTL_R_STAT,     /* Read status registers. */
    CTL_R_DMAREQ,   /* Read DRQ register. */
    CTL_R_CMD,      /* Read command register. */
    CTL_R_MODE,     /* Read mode register. */
    CTL_R_SETBPTR,  /* Set byte pointer flip-flop. */
    CTL_R_TEMP,     /* Read temporary register. */
    CTL_R_CLRMODE,  /* Clear mode register counter. */
    CTL_R_MASK      /* Read all DRQ mask bits. */
};

/* DMA control register offsets for read accesses. */
enum {
    CTL_W_CMD,      /* Write command register. */
    CTL_W_DMAREQ,   /* Write DRQ register. */
    CTL_W_MASKONE,  /* Write single DRQ mask bit. */
    CTL_W_MODE,     /* Write mode register. */
    CTL_W_CLRBPTR,  /* Clear byte pointer flip-flop. */
    CTL_W_MASTRCLR, /* Master clear. */
    CTL_W_CLRMASK,  /* Clear all DRQ mask bits. */
    CTL_W_MASK      /* Write all DRQ mask bits. */
};

/* Convert DMA channel number (0-7) to controller number (0-1). */
#define DMACH2C(c)      (c < 4 ? 0 : 1)

static int dmaChannelMap[8] = {-1, 2, 3, 1, -1, -1, -1, 0};
/* Map a DMA page register offset (0-7) to channel index (0-3). */
#define DMAPG2CX(c)     (dmaChannelMap[c])

static int dmaMapChannel[4] = {7, 3, 1, 2};
/* Map a channel index (0-3) to DMA page register offset (0-7). */
#define DMACX2PG(c)     (dmaMapChannel[c])
/* Map a channel number (0-7) to DMA page register offset (0-7). */
#define DMACH2PG(c)     (dmaMapChannel[c & 3])

/* Test the decrement bit of mode register. */
#define IS_MODE_DEC(c)  ((c) & 0x20)
/* Test the auto-init bit of mode register. */
#define IS_MODE_AI(c)   ((c) & 0x10)

/* Perform a master clear (reset) on a DMA controller. */
static void dmaClear(DMAControl *dc)
{
    dc->u8Command = 0;
    dc->u8Status  = 0;
    dc->u8Temp    = 0;
    dc->u8ModeCtr = 0;
    dc->bHiByte   = false;
    dc->u8Mask    = ~0;
}

/* Read the byte pointer and flip it. */
static inline bool dmaReadBytePtr(DMAControl *dc)
{
    bool    bHighByte;

    bHighByte = !!dc->bHiByte;
    dc->bHiByte ^= 1;
    return bHighByte;
}

/* DMA address registers writes and reads. */

static DECLCALLBACK(int) dmaWriteAddr(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT port,
                                      uint32_t u32, unsigned cb)
{
    if (cb == 1)
    {
        DMAControl  *dc = (DMAControl *)pvUser;
        DMAChannel  *ch;
        int         chidx, reg, is_count;

        Assert(!(u32 & ~0xff)); /* Check for garbage in high bits. */
        reg      = (port >> dc->is16bit) & 0x0f;
        chidx    = reg >> 1;
        is_count = reg & 1;
        ch       = &dc->ChState[chidx];
        if (dmaReadBytePtr(dc))
        {
            /* Write the high byte. */
            if (is_count)
                ch->u16BaseCount = RT_MAKE_U16(ch->u16BaseCount, u32);
            else
                ch->u16BaseAddr  = RT_MAKE_U16(ch->u16BaseAddr, u32);

            ch->u16CurCount = 0;
            ch->u16CurAddr  = ch->u16BaseAddr;
        }
        else
        {
            /* Write the low byte. */
            if (is_count)
                ch->u16BaseCount = RT_MAKE_U16(u32, RT_HIBYTE(ch->u16BaseCount));
            else
                ch->u16BaseAddr  = RT_MAKE_U16(u32, RT_HIBYTE(ch->u16BaseAddr));
        }
        Log2(("dmaWriteAddr: port %#06x, chidx %d, data %#02x\n",
              port, chidx, u32));
    }
    else
    {
        /* Likely a guest bug. */
        Log(("Bad size write to count register %#x (size %d, data %#x)\n",
             port, cb, u32));
    }
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) dmaReadAddr(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT port,
                                     uint32_t *pu32, unsigned cb)
{
    if (cb == 1)
    {
        DMAControl  *dc = (DMAControl *)pvUser;
        DMAChannel  *ch;
        int         chidx, reg, val, dir;
        int         bptr;

        reg   = (port >> dc->is16bit) & 0x0f;
        chidx = reg >> 1;
        ch    = &dc->ChState[chidx];

        dir = IS_MODE_DEC(ch->u8Mode) ? -1 : 1;
        if (reg & 1)
            val = ch->u16BaseCount - ch->u16CurCount;
        else
            val = ch->u16CurAddr + ch->u16CurCount * dir;

        bptr = dmaReadBytePtr(dc);
        *pu32 = RT_LOBYTE(val >> (bptr * 8));

        Log(("Count read: port %#06x, reg %#04x, data %#x\n", port, reg, val));
        return VINF_SUCCESS;
    }
    else
        return VERR_IOM_IOPORT_UNUSED;
}

/* DMA control registers writes and reads. */

static DECLCALLBACK(int) dmaWriteCtl(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT port,
                                     uint32_t u32, unsigned cb)
{
    if (cb == 1)
    {
        DMAControl  *dc = (DMAControl *)pvUser;
        int         chidx = 0;
        int         reg;

        reg = ((port >> dc->is16bit) & 0x0f) - 8;
        Assert((reg >= CTL_W_CMD && reg <= CTL_W_MASK));
        Assert(!(u32 & ~0xff)); /* Check for garbage in high bits. */

        switch (reg) {
        case CTL_W_CMD:
            /* Unsupported commands are entirely ignored. */
            if (u32 & CMD_UNSUPPORTED)
            {
                Log(("DMA command %#x is not supported, ignoring!\n", u32));
                break;
            }
            dc->u8Command = u32;
            break;
        case CTL_W_DMAREQ:
            chidx = u32 & 3;
            if (u32 & 4)
                dc->u8Status |= 1 << (chidx + 4);
            else
                dc->u8Status &= ~(1 << (chidx + 4));
            dc->u8Status &= ~(1 << chidx);  /* Clear TC for channel. */
            break;
        case CTL_W_MASKONE:
            chidx = u32 & 3;
            if (u32 & 4)
                dc->u8Mask |= 1 << chidx;
            else
                dc->u8Mask &= ~(1 << chidx);
            break;
        case CTL_W_MODE:
            {
                int op, opmode;

                chidx = u32 & 3;
                op = (u32 >> 2) & 3;
                opmode = (u32 >> 6) & 3;
                Log2(("chidx %d, op %d, %sauto-init, %screment, opmode %d\n",
                      chidx, op, IS_MODE_AI(u32) ? "" : "no ",
                      IS_MODE_DEC(u32) ? "de" : "in", opmode));

                dc->ChState[chidx].u8Mode = u32;
                break;
            }
        case CTL_W_CLRBPTR:
            dc->bHiByte = false;
            break;
        case CTL_W_MASTRCLR:
            dmaClear(dc);
            break;
        case CTL_W_CLRMASK:
            dc->u8Mask = 0;
            break;
        case CTL_W_MASK:
            dc->u8Mask = u32;
            break;
        default:
            Assert(0);
            break;
        }
        Log(("dmaWriteCtl: port %#06x, chidx %d, data %#02x\n",
              port, chidx, u32));
    }
    else
    {
        /* Likely a guest bug. */
        Log(("Bad size write to controller register %#x (size %d, data %#x)\n",
             port, cb, u32));
    }
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) dmaReadCtl(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT port,
                                    uint32_t *pu32, unsigned cb)
{
    if (cb == 1)
    {
        DMAControl  *dc = (DMAControl *)pvUser;
        uint8_t     val = 0;
        int         reg;

        reg = ((port >> dc->is16bit) & 0x0f) - 8;
        Assert((reg >= CTL_R_STAT && reg <= CTL_R_MASK));

        switch (reg) {
        case CTL_R_STAT:
            val = dc->u8Status;
            dc->u8Status &= 0xf0;   /* A read clears all TCs. */
            break;
        case CTL_R_DMAREQ:
            val = (dc->u8Status >> 4) | 0xf0;
            break;
        case CTL_R_CMD:
            val = dc->u8Command;
            break;
        case CTL_R_MODE:
            val = dc->ChState[dc->u8ModeCtr].u8Mode | 3;
            dc->u8ModeCtr = (dc->u8ModeCtr + 1) & 3;
        case CTL_R_SETBPTR:
            dc->bHiByte = true;
            break;
        case CTL_R_TEMP:
            val = dc->u8Temp;
            break;
        case CTL_R_CLRMODE:
            dc->u8ModeCtr = 0;
            break;
        case CTL_R_MASK:
            val = dc->u8Mask;
            break;
        default:
            Assert(0);
            break;
        }

        Log(("Ctrl read: port %#06x, reg %#04x, data %#x\n", port, reg, val));
        *pu32 = val;

        return VINF_SUCCESS;
    }
    else
        return VERR_IOM_IOPORT_UNUSED;
}

/* DMA page registers. There are 16 R/W page registers for compatibility with
 * the IBM PC/AT; only some of those registers are used for DMA. The page register
 * accessible via port 80h may be read to insert small delays or used as a scratch
 * register by a BIOS.
 */
static DECLCALLBACK(int) dmaReadPage(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT port,
                                     uint32_t *pu32, unsigned cb)
{
    DMAControl  *dc = (DMAControl *)pvUser;
    int         reg;

    if (cb == 1)
    {
        reg   = port & 7;
        *pu32 = dc->au8Page[reg];
        Log2(("Read %#x (byte) from page register %#x (channel %d)\n",
              *pu32, port, DMAPG2CX(reg)));
        return VINF_SUCCESS;
    } 
    else if (cb == 2) 
    {
        reg   = port & 7;
        *pu32 = dc->au8Page[reg] | (dc->au8Page[(reg + 1) & 7] << 8);
        Log2(("Read %#x (word) from page register %#x (channel %d)\n",
              *pu32, port, DMAPG2CX(reg)));
        return VINF_SUCCESS;
    }
    else
        return VERR_IOM_IOPORT_UNUSED;
}

static DECLCALLBACK(int) dmaWritePage(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT port,
                                      uint32_t u32, unsigned cb)
{
    DMAControl  *dc = (DMAControl *)pvUser;
    int         reg;

    Assert(!(u32 & ~0xff)); /* Check for garbage in high bits. */
    if (cb == 1)
    {
        reg = port & 7;
        dc->au8Page[reg]   = u32;
        dc->au8PageHi[reg] = 0;  /* Corresponding high page cleared. */
        Log2(("Wrote %#x to page register %#x (channel %d)\n",
              u32, port, DMAPG2CX(reg)));
    }
    else if (cb == 2) 
    {
        reg = port & 7;
        dc->au8Page[reg]   = u32;
        dc->au8PageHi[reg] = 0;  /* Corresponding high page cleared. */
        reg = (port + 1) & 7;
        dc->au8Page[reg]   = u32 >> 8;
        dc->au8PageHi[reg] = 0;  /* Corresponding high page cleared. */
    }
    else
    {
        /* Likely a guest bug. */
        Log(("Bad size write to page register %#x (size %d, data %#x)\n",
             port, cb, u32));
    }
    return VINF_SUCCESS;
}

/* EISA style high page registers, for extending the DMA addresses to cover
 * the entire 32-bit address space.
 */
static DECLCALLBACK(int) dmaReadHiPage(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT port,
                                       uint32_t *pu32, unsigned cb)
{
    if (cb == 1)
    {
        DMAControl  *dc = (DMAControl *)pvUser;
        int         reg;

        reg   = port & 7;
        *pu32 = dc->au8PageHi[reg];
        Log2(("Read %#x to from high page register %#x (channel %d)\n",
              *pu32, port, DMAPG2CX(reg)));
        return VINF_SUCCESS;
    }
    else
        return VERR_IOM_IOPORT_UNUSED;
}

static DECLCALLBACK(int) dmaWriteHiPage(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT port,
                                        uint32_t u32, unsigned cb)
{
    if (cb == 1)
    {
        DMAControl  *dc = (DMAControl *)pvUser;
        int         reg;

        Assert(!(u32 & ~0xff)); /* Check for garbage in high bits. */
        reg = port & 7;
        dc->au8PageHi[reg] = u32;
        Log2(("Wrote %#x to high page register %#x (channel %d)\n",
              u32, port, DMAPG2CX(reg)));
    }
    else
    {
        /* Likely a guest bug. */
        Log(("Bad size write to high page register %#x (size %d, data %#x)\n",
             port, cb, u32));
    }
    return VINF_SUCCESS;
}

/* Perform any pending transfers on a single DMA channel. */
static void dmaRunChannel(DMAState *s, int ctlidx, int chidx)
{
    DMAControl  *dc = &s->DMAC[ctlidx];
    DMAChannel  *ch = &dc->ChState[chidx];
    uint32_t    start_cnt, end_cnt;
    int         opmode;

    opmode = (ch->u8Mode >> 6) & 3;

    Log3(("DMA address %screment, mode %d\n",
          IS_MODE_DEC(ch->u8Mode) ? "de" : "in",
          ch->u8Mode >> 6));

    /* Addresses and counts are shifted for 16-bit channels. */
    start_cnt = ch->u16CurCount << dc->is16bit;
    end_cnt = ch->pfnXferHandler(s->pDevIns, ch->pvUser, (ctlidx * 4) + chidx,
                                 start_cnt, (ch->u16BaseCount + 1) << dc->is16bit);
    ch->u16CurCount = end_cnt >> dc->is16bit;
    Log3(("DMA position %d, size %d\n", end_cnt, (ch->u16BaseCount + 1) << dc->is16bit));
}

static bool dmaRun(PPDMDEVINS pDevIns)
{
    DMAState    *s = PDMINS_2_DATA(pDevIns, DMAState *);
    DMAControl  *dc;
    int         ctlidx, chidx, mask;

    /* Run all controllers and channels. */
    for (ctlidx = 0; ctlidx < 2; ++ctlidx)
    {
        dc = &s->DMAC[ctlidx];

        /* If controller is disabled, don't even bother. */
        if (dc->u8Command & CMD_DISABLE)
            continue;

        for (chidx = 0; chidx < 4; ++chidx)
        {
            mask = 1 << chidx;
            if (!(dc->u8Mask & mask) && (dc->u8Status & (mask << 4)))
                dmaRunChannel(s, ctlidx, chidx);
        }
    }
    return 0;
}

static void dmaRegister(PPDMDEVINS pDevIns, unsigned channel,
                        PFNDMATRANSFERHANDLER handler, void *pvUser)
{
    DMAState    *s = PDMINS_2_DATA(pDevIns, DMAState *);
    DMAChannel  *ch = &s->DMAC[DMACH2C(channel)].ChState[channel & 3];

    LogFlow(("dmaRegister: s=%p channel=%u XferHandler=%p pvUser=%p\n",
             s, channel, handler, pvUser));

    ch->pfnXferHandler = handler;
    ch->pvUser = pvUser;
}

/* Reverse the order of bytes in a memory buffer. */
static void dmaReverseBuf8(void *buf, unsigned len)
{
    uint8_t     *pBeg, *pEnd;
    uint8_t     temp;

    pBeg = (uint8_t *)buf;
    pEnd = pBeg + len - 1;
    for (len = len / 2; len; --len)
    {
        temp = *pBeg;
        *pBeg++ = *pEnd;
        *pEnd-- = temp;
    }
}

/* Reverse the order of words in a memory buffer. */
static void dmaReverseBuf16(void *buf, unsigned len)
{
    uint16_t    *pBeg, *pEnd;
    uint16_t    temp;

    Assert(!(len & 1));
    len /= 2;   /* Convert to word count. */
    pBeg = (uint16_t *)buf;
    pEnd = pBeg + len - 1;
    for (len = len / 2; len; --len)
    {
        temp = *pBeg;
        *pBeg++ = *pEnd;
        *pEnd-- = temp;
    }
}

static uint32_t dmaReadMemory(PPDMDEVINS pDevIns, unsigned channel,
                              void *buf, uint32_t pos, uint32_t len)
{
    DMAState    *s = PDMINS_2_DATA(pDevIns, DMAState *);
    DMAControl  *dc = &s->DMAC[DMACH2C(channel)];
    DMAChannel  *ch = &dc->ChState[channel & 3];
    uint32_t    page, pagehi;
    uint32_t    addr;

    LogFlow(("dmaReadMemory: s=%p channel=%u buf=%p pos=%u len=%u\n",
             s, channel, buf, pos, len));

    /* Build the address for this transfer. */
    page   = dc->au8Page[DMACH2PG(channel)] & ~dc->is16bit;
    pagehi = dc->au8PageHi[DMACH2PG(channel)];
    addr   = (pagehi << 24) | (page << 16) | (ch->u16CurAddr << dc->is16bit);

    if (IS_MODE_DEC(ch->u8Mode))
    {
        PDMDevHlpPhysRead(s->pDevIns, addr - pos - len, buf, len);
        if (dc->is16bit)
            dmaReverseBuf16(buf, len);
        else
            dmaReverseBuf8(buf, len);
    }
    else
        PDMDevHlpPhysRead(s->pDevIns, addr + pos, buf, len);

    return len;
}

static uint32_t dmaWriteMemory(PPDMDEVINS pDevIns, unsigned channel,
                               const void *buf, uint32_t pos, uint32_t len)
{
    DMAState    *s = PDMINS_2_DATA(pDevIns, DMAState *);
    DMAControl  *dc = &s->DMAC[DMACH2C(channel)];
    DMAChannel  *ch = &dc->ChState[channel & 3];
    uint32_t    page, pagehi;
    uint32_t    addr;

    LogFlow(("dmaWriteMemory: s=%p channel=%u buf=%p pos=%u len=%u\n",
             s, channel, buf, pos, len));

    /* Build the address for this transfer. */
    page   = dc->au8Page[DMACH2PG(channel)] & ~dc->is16bit;
    pagehi = dc->au8PageHi[DMACH2PG(channel)];
    addr   = (pagehi << 24) | (page << 16) | (ch->u16CurAddr << dc->is16bit);

    if (IS_MODE_DEC(ch->u8Mode))
    {
        //@todo: This would need a temporary buffer.
        Assert(0);
#if 0
        if (dc->is16bit)
            dmaReverseBuf16(buf, len);
        else
            dmaReverseBuf8(buf, len);
#endif
        PDMDevHlpPhysWrite(s->pDevIns, addr - pos - len, buf, len);
    }
    else
        PDMDevHlpPhysWrite(s->pDevIns, addr + pos, buf, len);

    return len;
}

static void dmaSetDREQ(PPDMDEVINS pDevIns, unsigned channel, unsigned level)
{
    DMAState    *s = PDMINS_2_DATA(pDevIns, DMAState *);
    DMAControl  *dc = &s->DMAC[DMACH2C(channel)];
    int         chidx;

    LogFlow(("dmaSetDREQ: s=%p channel=%u level=%u\n", s, channel, level));

    chidx  = channel & 3;
    if (level)
        dc->u8Status |= 1 << (chidx + 4);
    else
        dc->u8Status &= ~(1 << (chidx + 4));
}

static uint8_t dmaGetChannelMode(PPDMDEVINS pDevIns, unsigned channel)
{
    DMAState *s = PDMINS_2_DATA(pDevIns, DMAState *);

    LogFlow(("dmaGetChannelMode: s=%p channel=%u\n", s, channel));

    return s->DMAC[DMACH2C(channel)].ChState[channel & 3].u8Mode;
}

static void dmaReset(PPDMDEVINS pDevIns)
{
    DMAState *s = PDMINS_2_DATA(pDevIns, DMAState *);

    LogFlow(("dmaReset: s=%p\n", s));

    /* NB: The page and address registers are unaffected by a reset
     * and in an undefined state after power-up.
     */
    dmaClear(&s->DMAC[0]);
    dmaClear(&s->DMAC[1]);
}

/* Register DMA I/O port handlers. */
static void dmaIORegister(PPDMDEVINS pDevIns, bool bHighPage)
{
    DMAState    *s = PDMINS_2_DATA(pDevIns, DMAState *);
    DMAControl  *dc8;
    DMAControl  *dc16;

    dc8  = &s->DMAC[0];
    dc16 = &s->DMAC[1];

    dc8->is16bit  = false;
    dc16->is16bit = true;

    /* Base and current address for each channel. */
    PDMDevHlpIOPortRegister(s->pDevIns, 0x00, 8, dc8,
                            dmaWriteAddr, dmaReadAddr, NULL, NULL, "DMA8 Address");
    PDMDevHlpIOPortRegister(s->pDevIns, 0xC0, 16, dc16,
                            dmaWriteAddr, dmaReadAddr, NULL, NULL, "DMA16 Address");
    /* Control registers for both DMA controllers. */
    PDMDevHlpIOPortRegister(s->pDevIns, 0x08, 8, dc8,
                            dmaWriteCtl, dmaReadCtl, NULL, NULL, "DMA8 Control");
    PDMDevHlpIOPortRegister(s->pDevIns, 0xD0, 16, dc16,
                            dmaWriteCtl, dmaReadCtl, NULL, NULL, "DMA16 Control");
    /* Page registers for each channel (plus a few unused ones). */
    PDMDevHlpIOPortRegister(s->pDevIns, 0x80, 8, dc8,
                            dmaWritePage, dmaReadPage, NULL, NULL, "DMA8 Page");
    PDMDevHlpIOPortRegister(s->pDevIns, 0x88, 8, dc16,
                            dmaWritePage, dmaReadPage, NULL, NULL, "DMA16 Page");
    /* Optional EISA style high page registers (address bits 24-31). */
    if (bHighPage)
    {
        PDMDevHlpIOPortRegister(s->pDevIns, 0x480, 8, dc8,
                                dmaWriteHiPage, dmaReadHiPage, NULL, NULL, "DMA8 Page High");
        PDMDevHlpIOPortRegister(s->pDevIns, 0x488, 8, dc16,
                                dmaWriteHiPage, dmaReadHiPage, NULL, NULL, "DMA16 Page High");
    }
}

static void dmaSaveController(PSSMHANDLE pSSMHandle, DMAControl *dc)
{
    int         chidx;

    /* Save controller state... */
    SSMR3PutU8(pSSMHandle, dc->u8Command);
    SSMR3PutU8(pSSMHandle, dc->u8Mask);
    SSMR3PutU8(pSSMHandle, dc->bHiByte);
    SSMR3PutU32(pSSMHandle, dc->is16bit);
    SSMR3PutU8(pSSMHandle, dc->u8Status);
    SSMR3PutU8(pSSMHandle, dc->u8Temp);
    SSMR3PutU8(pSSMHandle, dc->u8ModeCtr);
    SSMR3PutMem(pSSMHandle, &dc->au8Page, sizeof(dc->au8Page));
    SSMR3PutMem(pSSMHandle, &dc->au8PageHi, sizeof(dc->au8PageHi));

    /* ...and all four of its channels. */
    for (chidx = 0; chidx < 4; ++chidx)
    {
        DMAChannel  *ch = &dc->ChState[chidx];

        SSMR3PutU16(pSSMHandle, ch->u16CurAddr);
        SSMR3PutU16(pSSMHandle, ch->u16CurCount);
        SSMR3PutU16(pSSMHandle, ch->u16BaseAddr);
        SSMR3PutU16(pSSMHandle, ch->u16BaseCount);
        SSMR3PutU8(pSSMHandle, ch->u8Mode);
    }
}

static int dmaLoadController(PSSMHANDLE pSSMHandle, DMAControl *dc, int version)
{
    uint8_t     u8val;
    uint32_t    u32val;
    int         chidx;

    SSMR3GetU8(pSSMHandle, &dc->u8Command);
    SSMR3GetU8(pSSMHandle, &dc->u8Mask);
    SSMR3GetU8(pSSMHandle, &u8val);
    dc->bHiByte = !!u8val;
    SSMR3GetU32(pSSMHandle, &dc->is16bit);
    if (version > DMA_SAVESTATE_OLD)
    {
        SSMR3GetU8(pSSMHandle, &dc->u8Status);
        SSMR3GetU8(pSSMHandle, &dc->u8Temp);
        SSMR3GetU8(pSSMHandle, &dc->u8ModeCtr);
        SSMR3GetMem(pSSMHandle, &dc->au8Page, sizeof(dc->au8Page));
        SSMR3GetMem(pSSMHandle, &dc->au8PageHi, sizeof(dc->au8PageHi));
    }

    for (chidx = 0; chidx < 4; ++chidx)
    {
        DMAChannel  *ch = &dc->ChState[chidx];

        if (version == DMA_SAVESTATE_OLD)
        {
            /* Convert from 17-bit to 16-bit format. */
            SSMR3GetU32(pSSMHandle, &u32val);
            ch->u16CurAddr = u32val >> dc->is16bit;
            SSMR3GetU32(pSSMHandle, &u32val);
            ch->u16CurCount = u32val >> dc->is16bit;
        }
        else
        {
            SSMR3GetU16(pSSMHandle, &ch->u16CurAddr);
            SSMR3GetU16(pSSMHandle, &ch->u16CurCount);
        }
        SSMR3GetU16(pSSMHandle, &ch->u16BaseAddr);
        SSMR3GetU16(pSSMHandle, &ch->u16BaseCount);
        SSMR3GetU8(pSSMHandle, &ch->u8Mode);
        /* Convert from old save state. */
        if (version == DMA_SAVESTATE_OLD)
        {
            /* Remap page register contents. */
            SSMR3GetU8(pSSMHandle, &u8val);
            dc->au8Page[DMACX2PG(chidx)] = u8val;
            SSMR3GetU8(pSSMHandle, &u8val);
            dc->au8PageHi[DMACX2PG(chidx)] = u8val;
            /* Throw away dack, eop. */
            SSMR3GetU8(pSSMHandle, &u8val);
            SSMR3GetU8(pSSMHandle, &u8val);
        }
    }
    return 0;
}

static DECLCALLBACK(int) dmaSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle)
{
    DMAState *s = PDMINS_2_DATA(pDevIns, DMAState *);

    dmaSaveController(pSSMHandle, &s->DMAC[0]);
    dmaSaveController(pSSMHandle, &s->DMAC[1]);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) dmaLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle,
                                     uint32_t uVersion, uint32_t uPass)
{
    DMAState *s = PDMINS_2_DATA(pDevIns, DMAState *);

    AssertMsgReturn(uVersion <= DMA_SAVESTATE_CURRENT, ("%d\n", uVersion), VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION);
    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    dmaLoadController(pSSMHandle, &s->DMAC[0], uVersion);
    return dmaLoadController(pSSMHandle, &s->DMAC[1], uVersion);
}

/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) dmaConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    DMAState    *s = PDMINS_2_DATA(pDevIns, DMAState *);
    bool        bHighPage = false;
    PDMDMACREG  reg;
    int         rc;

    s->pDevIns = pDevIns;

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "\0")) /* "HighPageEnable\0")) */
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

#if 0
    rc = CFGMR3QueryBool(pCfg, "HighPageEnable", &bHighPage);
    if (RT_FAILURE (rc))
        return rc;
#endif

    dmaIORegister(pDevIns, bHighPage);
    dmaReset(pDevIns);

    reg.u32Version        = PDM_DMACREG_VERSION;
    reg.pfnRun            = dmaRun;
    reg.pfnRegister       = dmaRegister;
    reg.pfnReadMemory     = dmaReadMemory;
    reg.pfnWriteMemory    = dmaWriteMemory;
    reg.pfnSetDREQ        = dmaSetDREQ;
    reg.pfnGetChannelMode = dmaGetChannelMode;

    rc = PDMDevHlpDMACRegister(pDevIns, &reg, &s->pHlp);
    if (RT_FAILURE (rc))
        return rc;

    rc = PDMDevHlpSSMRegister(pDevIns, DMA_SAVESTATE_CURRENT, sizeof(*s),
                              dmaSaveExec, dmaLoadExec);
    if (RT_FAILURE(rc))
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
    /* szName */
    "8237A",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "DMA Controller Device",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS,
    /* fClass */
    PDM_DEVREG_CLASS_DMA,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(DMAState),
    /* pfnConstruct */
    dmaConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    dmaReset,
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
