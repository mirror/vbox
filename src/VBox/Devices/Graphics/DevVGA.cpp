#ifdef VBOX
/** @file
 *
 * VBox VGA/VESA device
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
 * --------------------------------------------------------------------
 *
 * This code is based on:
 *
 * QEMU VGA Emulator.
 *
 * Copyright (c) 2003 Fabrice Bellard
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
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The default amount of VRAM. */
#define VGA_VRAM_DEFAULT    (_4M)
/** The maximum amount of VRAM. */
#define VGA_VRAM_MAX        (128 * _1M)
/** The minimum amount of VRAM. */
#define VGA_VRAM_MIN        (_1M)

/** The size of the VGA GC mapping.
 * This is supposed to be all the VGA memory accessible to the guest.
 * The initial value was 256KB but NTAllInOne.iso appears to access more
 * thus the limit was upped to 512KB.
 *
 * @todo Someone with some VGA knowhow should make a better guess at this value.
 */
#define VGA_MAPPING_SIZE    _512K

/** Converts a vga adaptor state pointer to a device instance pointer. */
#define VGASTATE2DEVINS(pVgaState)    ((pVgaState)->CTXSUFF(pDevIns))

/** Use VBE bytewise I/O */
#define VBE_BYTEWISE_IO

/** Use VBE new dynamic mode list.
 * If this is not defined, no checks are carried out to see if the modes all
 * fit into the framebuffer! See the VRAM_SIZE_FIX define. */
#define VBE_NEW_DYN_LIST

/** Check that the video modes fit into virtual video memory.
 * Only works when VBE_NEW_DYN_LIST is defined! */
#define VRAM_SIZE_FIX

/** Some fixes to ensure that logical scan-line lengths are not overwritten. */
#define KEEP_SCAN_LINE_LENGTH


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_VGA
#include <VBox/pdmdev.h>
#include <VBox/pgm.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>

#include <VBox/VBoxGuest.h>
#include <VBox/VBoxVideo.h>

#if defined(VBE_NEW_DYN_LIST) && defined(IN_RING3) && !defined(VBOX_DEVICE_STRUCT_TESTCASE)
# include "DevVGAModes.h"
# include <stdio.h> /* sscan */
#endif

#include "vl_vbox.h"
#include "DevVGA.h"
#include "Builtins.h"
#include "Builtins2.h"


#ifndef VBOX_DEVICE_STRUCT_TESTCASE
/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS

PDMBOTHCBDECL(int) vgaIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int) vgaIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) vgaIOPortWriteVBEIndex(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int) vgaIOPortWriteVBEData(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int) vgaIOPortReadVBEIndex(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) vgaIOPortReadVBEData(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) vgaMMIOFill(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, uint32_t u32Item, unsigned cbItem, unsigned cItems);
PDMBOTHCBDECL(int) vgaMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
PDMBOTHCBDECL(int) vgaMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
#ifdef IN_GC
PDMBOTHCBDECL(int) vgaGCLFBAccessHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPHYS GCPhysFault, void *pvUser);
#endif
#ifdef IN_RING0
PDMBOTHCBDECL(int) vgaR0LFBAccessHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPHYS GCPhysFault, void *pvUser);
#endif
#ifdef IN_RING3
# ifdef VBE_NEW_DYN_LIST
PDMBOTHCBDECL(int) vbeIOPortReadVBEExtra(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) vbeIOPortWriteVBEExtra(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
# endif
#endif /* IN_RING3 */


__END_DECLS


/**
 * Set a VRAM page dirty.
 *
 * @param   pData       VGA instance data.
 * @param   offVRAM     The VRAM offset of the page to set.
 */
DECLINLINE(void) vga_set_dirty(VGAState *pData, RTGCPHYS offVRAM)
{
    AssertMsg(offVRAM < pData->vram_size, ("offVRAM = %p, pData->vram_size = %p\n", offVRAM, pData->vram_size));
    ASMBitSet(&pData->au32DirtyBitmap[0], offVRAM >> PAGE_SHIFT);
    pData->fHaveDirtyBits = true;
}

/**
 * Tests if a VRAM page is dirty.
 *
 * @returns true if dirty.
 * @returns false if clean.
 * @param   pData       VGA instance data.
 * @param   offVRAM     The VRAM offset of the page to check.
 */
DECLINLINE(bool) vga_is_dirty(VGAState *pData, RTGCPHYS offVRAM)
{
    AssertMsg(offVRAM < pData->vram_size, ("offVRAM = %p, pData->vram_size = %p\n", offVRAM, pData->vram_size));
    return ASMBitTest(&pData->au32DirtyBitmap[0], offVRAM >> PAGE_SHIFT);
}

/**
 * Reset dirty flags in a give range.
 *
 * @param   pData           VGA instance data.
 * @param   offVRAMStart    Offset into the VRAM buffer of the first page.
 * @param   offVRAMEnd      Offset into the VRAM buffer of the last page - exclusive.
 */
DECLINLINE(void) vga_reset_dirty(VGAState *pData, RTGCPHYS offVRAMStart, RTGCPHYS offVRAMEnd)
{
    Assert(offVRAMStart < pData->vram_size);
    Assert(offVRAMEnd <= pData->vram_size);
    Assert(offVRAMStart < offVRAMEnd);
    ASMBitClearRange(&pData->au32DirtyBitmap[0], offVRAMStart >> PAGE_SHIFT, offVRAMEnd >> PAGE_SHIFT);
}

#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */
#endif /* VBOX */
#ifndef VBOX_DEVICE_STRUCT_TESTCASE

#ifndef VBOX
#include "vl.h"
#include "vga_int.h"
#endif /* !VBOX */

#ifdef LOG_ENABLED
//#define DEBUG_VGA
//#define DEBUG_VGA_MEM
//#define DEBUG_VGA_REG

#define DEBUG_BOCHS_VBE

#endif

/* force some bits to zero */
#ifdef VBOX
static
#endif /* VBOX */
const uint8_t sr_mask[8] = {
    (uint8_t)~0xfc,
    (uint8_t)~0xc2,
    (uint8_t)~0xf0,
    (uint8_t)~0xc0,
    (uint8_t)~0xf1,
    (uint8_t)~0xff,
    (uint8_t)~0xff,
    (uint8_t)~0x00,
};

#ifdef VBOX
static
#endif /* VBOX */
const uint8_t gr_mask[16] = {
    (uint8_t)~0xf0, /* 0x00 */
    (uint8_t)~0xf0, /* 0x01 */
    (uint8_t)~0xf0, /* 0x02 */
    (uint8_t)~0xe0, /* 0x03 */
    (uint8_t)~0xfc, /* 0x04 */
    (uint8_t)~0x84, /* 0x05 */
    (uint8_t)~0xf0, /* 0x06 */
    (uint8_t)~0xf0, /* 0x07 */
    (uint8_t)~0x00, /* 0x08 */
    (uint8_t)~0xff, /* 0x09 */
    (uint8_t)~0xff, /* 0x0a */
    (uint8_t)~0xff, /* 0x0b */
    (uint8_t)~0xff, /* 0x0c */
    (uint8_t)~0xff, /* 0x0d */
    (uint8_t)~0xff, /* 0x0e */
    (uint8_t)~0xff, /* 0x0f */
};

#define cbswap_32(__x) \
((uint32_t)( \
		(((uint32_t)(__x) & (uint32_t)0x000000ffUL) << 24) | \
		(((uint32_t)(__x) & (uint32_t)0x0000ff00UL) <<  8) | \
		(((uint32_t)(__x) & (uint32_t)0x00ff0000UL) >>  8) | \
		(((uint32_t)(__x) & (uint32_t)0xff000000UL) >> 24) ))

#ifdef WORDS_BIGENDIAN
#define PAT(x) cbswap_32(x)
#else
#define PAT(x) (x)
#endif

#ifdef WORDS_BIGENDIAN
#define BIG 1
#else
#define BIG 0
#endif

#ifdef WORDS_BIGENDIAN
#define GET_PLANE(data, p) (((data) >> (24 - (p) * 8)) & 0xff)
#else
#define GET_PLANE(data, p) (((data) >> ((p) * 8)) & 0xff)
#endif

static const uint32_t mask16[16] = {
    PAT(0x00000000),
    PAT(0x000000ff),
    PAT(0x0000ff00),
    PAT(0x0000ffff),
    PAT(0x00ff0000),
    PAT(0x00ff00ff),
    PAT(0x00ffff00),
    PAT(0x00ffffff),
    PAT(0xff000000),
    PAT(0xff0000ff),
    PAT(0xff00ff00),
    PAT(0xff00ffff),
    PAT(0xffff0000),
    PAT(0xffff00ff),
    PAT(0xffffff00),
    PAT(0xffffffff),
};

#undef PAT

#ifdef WORDS_BIGENDIAN
#define PAT(x) (x)
#else
#define PAT(x) cbswap_32(x)
#endif

static const uint32_t dmask16[16] = {
    PAT(0x00000000),
    PAT(0x000000ff),
    PAT(0x0000ff00),
    PAT(0x0000ffff),
    PAT(0x00ff0000),
    PAT(0x00ff00ff),
    PAT(0x00ffff00),
    PAT(0x00ffffff),
    PAT(0xff000000),
    PAT(0xff0000ff),
    PAT(0xff00ff00),
    PAT(0xff00ffff),
    PAT(0xffff0000),
    PAT(0xffff00ff),
    PAT(0xffffff00),
    PAT(0xffffffff),
};

static const uint32_t dmask4[4] = {
    PAT(0x00000000),
    PAT(0x0000ffff),
    PAT(0xffff0000),
    PAT(0xffffffff),
};

#if defined(VBOX) && defined(IN_RING3)
static uint32_t expand4[256];
static uint16_t expand2[256];
static uint8_t expand4to8[16];
#endif /* VBOX && IN_RING3 */

#ifndef VBOX
VGAState *vga_state;
int vga_io_memory;
#endif /* !VBOX */

static uint32_t vga_ioport_read(void *opaque, uint32_t addr)
{
    VGAState *s = (VGAState*)opaque;
    int val, index;

    /* check port range access depending on color/monochrome mode */
    if ((addr >= 0x3b0 && addr <= 0x3bf && (s->msr & MSR_COLOR_EMULATION)) ||
        (addr >= 0x3d0 && addr <= 0x3df && !(s->msr & MSR_COLOR_EMULATION))) {
        val = 0xff;
    } else {
        switch(addr) {
        case 0x3c0:
            if (s->ar_flip_flop == 0) {
                val = s->ar_index;
            } else {
                val = 0;
            }
            break;
        case 0x3c1:
            index = s->ar_index & 0x1f;
            if (index < 21)
                val = s->ar[index];
            else
                val = 0;
            break;
        case 0x3c2:
            val = s->st00;
            break;
        case 0x3c4:
            val = s->sr_index;
            break;
        case 0x3c5:
            val = s->sr[s->sr_index];
#ifdef DEBUG_VGA_REG
            Log(("vga: read SR%x = 0x%02x\n", s->sr_index, val));
#endif
            break;
        case 0x3c7:
            val = s->dac_state;
            break;
	case 0x3c8:
	    val = s->dac_write_index;
	    break;
        case 0x3c9:
            val = s->palette[s->dac_read_index * 3 + s->dac_sub_index];
            if (++s->dac_sub_index == 3) {
                s->dac_sub_index = 0;
                s->dac_read_index++;
            }
            break;
        case 0x3ca:
            val = s->fcr;
            break;
        case 0x3cc:
            val = s->msr;
            break;
        case 0x3ce:
            val = s->gr_index;
            break;
        case 0x3cf:
            val = s->gr[s->gr_index];
#ifdef DEBUG_VGA_REG
            Log(("vga: read GR%x = 0x%02x\n", s->gr_index, val));
#endif
            break;
        case 0x3b4:
        case 0x3d4:
            val = s->cr_index;
            break;
        case 0x3b5:
        case 0x3d5:
            val = s->cr[s->cr_index];
#ifdef DEBUG_VGA_REG
            Log(("vga: read CR%x = 0x%02x\n", s->cr_index, val));
#endif
            break;
        case 0x3ba:
        case 0x3da:
            /* just toggle to fool polling */
            s->st01 ^= ST01_V_RETRACE | ST01_DISP_ENABLE;
            val = s->st01;
            s->ar_flip_flop = 0;
            break;
        default:
            val = 0x00;
            break;
        }
    }
#if defined(DEBUG_VGA)
    Log(("VGA: read addr=0x%04x data=0x%02x\n", addr, val));
#endif
    return val;
}

static void vga_ioport_write(void *opaque, uint32_t addr, uint32_t val)
{
    VGAState *s = (VGAState*)opaque;
    int index;

    /* check port range access depending on color/monochrome mode */
    if ((addr >= 0x3b0 && addr <= 0x3bf && (s->msr & MSR_COLOR_EMULATION)) ||
        (addr >= 0x3d0 && addr <= 0x3df && !(s->msr & MSR_COLOR_EMULATION)))
        return;

#ifdef DEBUG_VGA
    Log(("VGA: write addr=0x%04x data=0x%02x\n", addr, val));
#endif

    switch(addr) {
    case 0x3c0:
        if (s->ar_flip_flop == 0) {
            val &= 0x3f;
            s->ar_index = val;
        } else {
            index = s->ar_index & 0x1f;
            switch(index) {
#ifndef VBOX
            case 0x00 ... 0x0f:
#else /* VBOX */
            case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
            case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
#endif /* VBOX */
                s->ar[index] = val & 0x3f;
                break;
            case 0x10:
                s->ar[index] = val & ~0x10;
                break;
            case 0x11:
                s->ar[index] = val;
                break;
            case 0x12:
                s->ar[index] = val & ~0xc0;
                break;
            case 0x13:
                s->ar[index] = val & ~0xf0;
                break;
            case 0x14:
                s->ar[index] = val & ~0xf0;
                break;
            default:
                break;
            }
        }
        s->ar_flip_flop ^= 1;
        break;
    case 0x3c2:
        s->msr = val & ~0x10;
        break;
    case 0x3c4:
        s->sr_index = val & 7;
        break;
    case 0x3c5:
#ifdef DEBUG_VGA_REG
        Log(("vga: write SR%x = 0x%02x\n", s->sr_index, val));
#endif
        s->sr[s->sr_index] = val & sr_mask[s->sr_index];
        break;
    case 0x3c7:
        s->dac_read_index = val;
        s->dac_sub_index = 0;
        s->dac_state = 3;
        break;
    case 0x3c8:
        s->dac_write_index = val;
        s->dac_sub_index = 0;
        s->dac_state = 0;
        break;
    case 0x3c9:
        s->dac_cache[s->dac_sub_index] = val;
        if (++s->dac_sub_index == 3) {
            memcpy(&s->palette[s->dac_write_index * 3], s->dac_cache, 3);
            s->dac_sub_index = 0;
            s->dac_write_index++;
        }
        break;
    case 0x3ce:
        s->gr_index = val & 0x0f;
        break;
    case 0x3cf:
#ifdef DEBUG_VGA_REG
        Log(("vga: write GR%x = 0x%02x\n", s->gr_index, val));
#endif
        s->gr[s->gr_index] = val & gr_mask[s->gr_index];
        break;
    case 0x3b4:
    case 0x3d4:
        s->cr_index = val;
        break;
    case 0x3b5:
    case 0x3d5:
#ifdef DEBUG_VGA_REG
        Log(("vga: write CR%x = 0x%02x\n", s->cr_index, val));
#endif
        /* handle CR0-7 protection */
        if ((s->cr[0x11] & 0x80) && s->cr_index <= 7) {
            /* can always write bit 4 of CR7 */
            if (s->cr_index == 7)
                s->cr[7] = (s->cr[7] & ~0x10) | (val & 0x10);
            return;
        }
        switch(s->cr_index) {
        case 0x01: /* horizontal display end */
        case 0x07:
        case 0x09:
        case 0x0c:
        case 0x0d:
        case 0x12: /* veritcal display end */
            s->cr[s->cr_index] = val;
            break;

        default:
            s->cr[s->cr_index] = val;
            break;
        }
        break;
    case 0x3ba:
    case 0x3da:
        s->fcr = val & 0x10;
        break;
    }
}

#ifdef CONFIG_BOCHS_VBE
static uint32_t vbe_ioport_read_index(void *opaque, uint32_t addr)
{
    VGAState *s = (VGAState*)opaque;
    uint32_t val;
    val = s->vbe_index;
    return val;
}

static uint32_t vbe_ioport_read_data(void *opaque, uint32_t addr)
{
    VGAState *s = (VGAState*)opaque;
    uint32_t val;

    if (s->vbe_index <= VBE_DISPI_INDEX_NB) {
      if (s->vbe_regs[VBE_DISPI_INDEX_ENABLE] & VBE_DISPI_GETCAPS) {
          switch(s->vbe_index) {
                /* XXX: do not hardcode ? */
            case VBE_DISPI_INDEX_XRES:
                val = VBE_DISPI_MAX_XRES;
                break;
            case VBE_DISPI_INDEX_YRES:
                val = VBE_DISPI_MAX_YRES;
                break;
            case VBE_DISPI_INDEX_BPP:
                val = VBE_DISPI_MAX_BPP;
                break;
            default:
                val = s->vbe_regs[s->vbe_index];
                break;
          }
      } else if (s->vbe_index == VBE_DISPI_INDEX_VBOX_VIDEO) {
        /* Reading from the port means that the old additions are requesting the number of monitors. */
        val = 1;
      } else {
        val = s->vbe_regs[s->vbe_index];
      }
    } else {
        val = 0;
    }
#ifdef DEBUG_BOCHS_VBE
    Log(("VBE: read index=0x%x val=0x%x\n", s->vbe_index, val));
#endif
    return val;
}

static void vbe_ioport_write_index(void *opaque, uint32_t addr, uint32_t val)
{
    VGAState *s = (VGAState*)opaque;
    s->vbe_index = val;
}

static int vbe_ioport_write_data(void *opaque, uint32_t addr, uint32_t val)
{
    VGAState *s = (VGAState*)opaque;

    if (s->vbe_index <= VBE_DISPI_INDEX_NB) {
#ifdef DEBUG_BOCHS_VBE
        Log(("VBE: write index=0x%x val=0x%x\n", s->vbe_index, val));
#endif
        switch(s->vbe_index) {
        case VBE_DISPI_INDEX_ID:
            if (val == VBE_DISPI_ID0 ||
                val == VBE_DISPI_ID1 ||
                val == VBE_DISPI_ID2) {
                s->vbe_regs[s->vbe_index] = val;
            }
#ifdef VBOX
            if (val == VBE_DISPI_ID_VBOX_VIDEO) {
                s->vbe_regs[s->vbe_index] = val;
            }
#endif /* VBOX */
            break;
        case VBE_DISPI_INDEX_XRES:
            if ((val <= VBE_DISPI_MAX_XRES) && ((val & 7) == 0)) {
                s->vbe_regs[s->vbe_index] = val;
#ifdef KEEP_SCAN_LINE_LENGTH
                if (s->vbe_regs[VBE_DISPI_INDEX_BPP] == 4)
                    s->vbe_line_offset = val >> 1;
                else
                    s->vbe_line_offset = val * ((s->vbe_regs[VBE_DISPI_INDEX_BPP] + 7) >> 3);
                /* XXX: support weird bochs semantics ? */
                s->vbe_regs[VBE_DISPI_INDEX_VIRT_WIDTH] = s->vbe_line_offset;
                s->vbe_regs[VBE_DISPI_INDEX_X_OFFSET] = 0;
                s->vbe_regs[VBE_DISPI_INDEX_Y_OFFSET] = 0;
                s->vbe_start_addr = 0;
#endif  /* KEEP_SCAN_LINE_LENGTH defined */
            }
            break;
        case VBE_DISPI_INDEX_YRES:
            if (val <= VBE_DISPI_MAX_YRES) {
                s->vbe_regs[s->vbe_index] = val;
#ifdef KEEP_SCAN_LINE_LENGTH
                s->vbe_regs[VBE_DISPI_INDEX_VIRT_HEIGHT] = val;
                s->vbe_regs[VBE_DISPI_INDEX_X_OFFSET] = 0;
                s->vbe_regs[VBE_DISPI_INDEX_Y_OFFSET] = 0;
                s->vbe_start_addr = 0;
#endif  /* KEEP_SCAN_LINE_LENGTH defined */
            }
            break;
        case VBE_DISPI_INDEX_BPP:
            if (val == 0)
                val = 8;
            if (val == 4 || val == 8 || val == 15 ||
                val == 16 || val == 24 || val == 32) {
                s->vbe_regs[s->vbe_index] = val;
#ifdef KEEP_SCAN_LINE_LENGTH
                if (val == 4)
                    s->vbe_line_offset = s->vbe_regs[VBE_DISPI_INDEX_XRES] >> 1;
                else
                    s->vbe_line_offset = s->vbe_regs[VBE_DISPI_INDEX_XRES] * ((val + 7) >> 3);
                /* XXX: support weird bochs semantics ? */
                s->vbe_regs[VBE_DISPI_INDEX_VIRT_WIDTH] = s->vbe_line_offset;
                s->vbe_regs[VBE_DISPI_INDEX_X_OFFSET] = 0;
                s->vbe_regs[VBE_DISPI_INDEX_Y_OFFSET] = 0;
                s->vbe_start_addr = 0;
#endif  /* KEEP_SCAN_LINE_LENGTH defined */
            }
            break;
        case VBE_DISPI_INDEX_BANK:
            if (s->vbe_regs[VBE_DISPI_INDEX_BPP] == 4) {
              val &= (s->vbe_bank_mask >> 2);
            } else {
              val &= s->vbe_bank_mask;
            }
            val &= s->vbe_bank_mask;
            s->vbe_regs[s->vbe_index] = val;
            s->bank_offset = (val << 16);
            break;
        case VBE_DISPI_INDEX_ENABLE:
#ifndef IN_RING3
            return VINF_IOM_HC_IOPORT_WRITE;
#else
            if (val & VBE_DISPI_ENABLED) {
                int h, shift_control;
#ifdef VBOX
                /* Check the values before we screw up with a resolution which is too big or small. */
                size_t cb = s->vbe_regs[VBE_DISPI_INDEX_XRES];
                if (s->vbe_regs[VBE_DISPI_INDEX_BPP] == 4)
                    cb = s->vbe_regs[VBE_DISPI_INDEX_XRES] >> 1;
                else
                    cb = s->vbe_regs[VBE_DISPI_INDEX_XRES] * ((s->vbe_regs[VBE_DISPI_INDEX_BPP] + 7) >> 3);
                cb *= s->vbe_regs[VBE_DISPI_INDEX_YRES];
#ifndef KEEP_SCAN_LINE_LENGTH
                if (    !s->vbe_regs[VBE_DISPI_INDEX_XRES]
                    ||  !s->vbe_regs[VBE_DISPI_INDEX_YRES]
                    ||  cb > s->vram_size)
                {
                    AssertMsgFailed(("XRES=%d YRES=%d cb=%d vram_size=%d\n",
                                     s->vbe_regs[VBE_DISPI_INDEX_XRES], s->vbe_regs[VBE_DISPI_INDEX_YRES], cb, s->vram_size));
                    return VINF_SUCCESS; /* Note: silent failure like before */
                }
#else  /* KEEP_SCAN_LINE_LENGTH defined */
                if (    !s->vbe_regs[VBE_DISPI_INDEX_VIRT_WIDTH]
                    ||  !s->vbe_regs[VBE_DISPI_INDEX_YRES]
                    ||  cb > s->vram_size)
                {
                    AssertMsgFailed(("VIRT WIDTH=%d YRES=%d cb=%d vram_size=%d\n",
                                     s->vbe_regs[VBE_DISPI_INDEX_VIRT_WIDTH], s->vbe_regs[VBE_DISPI_INDEX_YRES], cb, s->vram_size));
                    return VINF_SUCCESS; /* Note: silent failure like before */
                }
#endif  /* KEEP_SCAN_LINE_LENGTH defined */
#endif /* VBOX */

#ifndef KEEP_SCAN_LINE_LENGTH
                s->vbe_regs[VBE_DISPI_INDEX_VIRT_WIDTH] =
                    s->vbe_regs[VBE_DISPI_INDEX_XRES];
                s->vbe_regs[VBE_DISPI_INDEX_VIRT_HEIGHT] =
                    s->vbe_regs[VBE_DISPI_INDEX_YRES];
                s->vbe_regs[VBE_DISPI_INDEX_X_OFFSET] = 0;
                s->vbe_regs[VBE_DISPI_INDEX_Y_OFFSET] = 0;

                if (s->vbe_regs[VBE_DISPI_INDEX_BPP] == 4)
                    s->vbe_line_offset = s->vbe_regs[VBE_DISPI_INDEX_XRES] >> 1;
                else
                    s->vbe_line_offset = s->vbe_regs[VBE_DISPI_INDEX_XRES] *
                        ((s->vbe_regs[VBE_DISPI_INDEX_BPP] + 7) >> 3);
                s->vbe_start_addr = 0;
#endif  /* KEEP_SCAN_LINE_LENGTH not defined */

                /* clear the screen (should be done in BIOS) */
                if (!(val & VBE_DISPI_NOCLEARMEM)) {
#ifndef VBOX
                    memset(s->vram_ptr, 0,
                           s->vbe_regs[VBE_DISPI_INDEX_YRES] * s->vbe_line_offset);
#else /* VBOX */
                    memset(CTXSUFF(s->vram_ptr), 0,
                           s->vbe_regs[VBE_DISPI_INDEX_YRES] * s->vbe_line_offset);
#endif /* VBOX */
                }

                /* we initialize the VGA graphic mode (should be done
                   in BIOS) */
                s->gr[0x06] = (s->gr[0x06] & ~0x0c) | 0x05; /* graphic mode + memory map 1 */
                s->cr[0x17] |= 3; /* no CGA modes */
                s->cr[0x13] = s->vbe_line_offset >> 3;
                /* width */
                s->cr[0x01] = (s->vbe_regs[VBE_DISPI_INDEX_XRES] >> 3) - 1;
                /* height (only meaningful if < 1024) */
                h = s->vbe_regs[VBE_DISPI_INDEX_YRES] - 1;
                s->cr[0x12] = h;
                s->cr[0x07] = (s->cr[0x07] & ~0x42) |
                    ((h >> 7) & 0x02) | ((h >> 3) & 0x40);
                /* line compare to 1023 */
                s->cr[0x18] = 0xff;
                s->cr[0x07] |= 0x10;
                s->cr[0x09] |= 0x40;

                if (s->vbe_regs[VBE_DISPI_INDEX_BPP] == 4) {
                    shift_control = 0;
                    s->sr[0x01] &= ~8; /* no double line */
                } else {
                    shift_control = 2;
                    s->sr[4] |= 0x08; /* set chain 4 mode */
                    s->sr[2] |= 0x0f; /* activate all planes */
                }
                s->gr[0x05] = (s->gr[0x05] & ~0x60) | (shift_control << 5);
                s->cr[0x09] &= ~0x9f; /* no double scan */
#ifdef VBOX
                /* sunlover 30.05.2007
                 * The ar_index remains with bit 0x20 cleared after a switch from fullscreen
                 * DOS mode on Windows XP guest. That leads to GMODE_BLANK in vga_update_display.
                 * But the VBE mode is graphics, so not a blank anymore.
                 */
                s->ar_index |= 0x20;
#endif /* VBOX */
            } else {
                /* XXX: the bios should do that */
#ifdef VBOX
                /* sunlover 21.12.2006
                 * Here is probably more to reset. When this was executed in GC
                 * then the *update* functions could not detect a mode change.
                 * Or may be these update function should take the s->vbe_regs[s->vbe_index]
                 * into account when detecting a mode change.
                 *
                 * The 'mode reset not detected' problem is now fixed by executing the
                 * VBE_DISPI_INDEX_ENABLE case always in RING3 in order to call the
                 * LFBChange callback.
                 */
#endif /* VBOX */
                s->bank_offset = 0;
            }
            s->vbe_regs[s->vbe_index] = val;
            /*
             * LFB video mode is either disabled or changed. This notification
             * is used by the display to disable VBVA.
             */
            s->pDrv->pfnLFBModeChange(s->pDrv, (val & VBE_DISPI_ENABLED) != 0);
            break;
#endif /* IN_RING3 */
        case VBE_DISPI_INDEX_VIRT_WIDTH:
            {
                int w, h, line_offset;

                if (val < s->vbe_regs[VBE_DISPI_INDEX_XRES])
                    return VINF_SUCCESS;
                w = val;
                if (s->vbe_regs[VBE_DISPI_INDEX_BPP] == 4)
                    line_offset = w >> 1;
                else
                    line_offset = w * ((s->vbe_regs[VBE_DISPI_INDEX_BPP] + 7) >> 3);
                h = s->vram_size / line_offset;
                /* XXX: support weird bochs semantics ? */
                if (h < s->vbe_regs[VBE_DISPI_INDEX_YRES])
                    return VINF_SUCCESS;
                s->vbe_regs[VBE_DISPI_INDEX_VIRT_WIDTH] = w;
                s->vbe_regs[VBE_DISPI_INDEX_VIRT_HEIGHT] = h;
                s->vbe_line_offset = line_offset;
            }
            break;
        case VBE_DISPI_INDEX_X_OFFSET:
        case VBE_DISPI_INDEX_Y_OFFSET:
            {
                int x;
                s->vbe_regs[s->vbe_index] = val;
                s->vbe_start_addr = s->vbe_line_offset * s->vbe_regs[VBE_DISPI_INDEX_Y_OFFSET];
                x = s->vbe_regs[VBE_DISPI_INDEX_X_OFFSET];
                if (s->vbe_regs[VBE_DISPI_INDEX_BPP] == 4)
                    s->vbe_start_addr += x >> 1;
                else
                    s->vbe_start_addr += x * ((s->vbe_regs[VBE_DISPI_INDEX_BPP] + 7) >> 3);
                s->vbe_start_addr >>= 2;
            }
            break;
        case VBE_DISPI_INDEX_VBOX_VIDEO:
#ifdef VBOX
#ifndef IN_RING3
            return VINF_IOM_HC_IOPORT_WRITE;
#else
            /* Changes in the VGA device are minimal. The device is bypassed. The driver does all work. */
            if (val == VBOX_VIDEO_DISABLE_ADAPTER_MEMORY)
            {
                s->pDrv->pfnProcessAdapterData(s->pDrv, NULL, 0);
            }
            else if (val == VBOX_VIDEO_INTERPRET_ADAPTER_MEMORY)
            {
                s->pDrv->pfnProcessAdapterData(s->pDrv, s->CTXSUFF(vram_ptr), s->vram_size);
            }
            else if ((val & 0xFFFF0000) == VBOX_VIDEO_INTERPRET_DISPLAY_MEMORY_BASE)
            {
                s->pDrv->pfnProcessDisplayData(s->pDrv, s->CTXSUFF(vram_ptr), val & 0xFFFF);
            }
#endif /* IN_RING3 */
#endif /* VBOX */
            break;
        default:
            break;
        }
    }
    return VINF_SUCCESS;
}
#endif

/* called for accesses between 0xa0000 and 0xc0000 */
#ifdef VBOX
static
#endif /* VBOX */
uint32_t vga_mem_readb(void *opaque, target_phys_addr_t addr)
{
    VGAState *s = (VGAState*)opaque;
    int memory_map_mode, plane;
    uint32_t ret;

    /* convert to VGA memory offset */
    memory_map_mode = (s->gr[6] >> 2) & 3;
    addr &= 0x1ffff;
    switch(memory_map_mode) {
    case 0:
        break;
    case 1:
        if (addr >= 0x10000)
            return 0xff;
        addr += s->bank_offset;
        break;
    case 2:
        addr -= 0x10000;
        if (addr >= 0x8000)
            return 0xff;
        break;
    default:
    case 3:
        addr -= 0x18000;
        if (addr >= 0x8000)
            return 0xff;
        break;
    }

#ifdef IN_GC
    if (addr >= VGA_MAPPING_SIZE)
        return VINF_IOM_HC_MMIO_WRITE;
#endif

    if (s->sr[4] & 0x08) {
        /* chain 4 mode : simplest access */
#ifndef VBOX
        ret = s->vram_ptr[addr];
#else /* VBOX */
        ret = s->CTXSUFF(vram_ptr)[addr];
#endif /* VBOX */
    } else if (s->gr[5] & 0x10) {
        /* odd/even mode (aka text mode mapping) */
        plane = (s->gr[4] & 2) | (addr & 1);
#ifndef VBOX
        ret = s->vram_ptr[((addr & ~1) << 1) | plane];
#else /* VBOX */
        /* See the comment for a similar line in vga_mem_writeb. */
        ret = s->CTXSUFF(vram_ptr)[((addr & ~1) << 2) | plane];
#endif /* VBOX */
    } else {
        /* standard VGA latched access */
#ifndef VBOX
        s->latch = ((uint32_t *)s->vram_ptr)[addr];
#else /* VBOX && IN_GC */
        s->latch = ((uint32_t *)s->CTXSUFF(vram_ptr))[addr];
#endif /* VBOX && IN_GC */

        if (!(s->gr[5] & 0x08)) {
            /* read mode 0 */
            plane = s->gr[4];
            ret = GET_PLANE(s->latch, plane);
        } else {
            /* read mode 1 */
            ret = (s->latch ^ mask16[s->gr[2]]) & mask16[s->gr[7]];
            ret |= ret >> 16;
            ret |= ret >> 8;
            ret = (~ret) & 0xff;
        }
    }
    return ret;
}

#ifndef VBOX
static uint32_t vga_mem_readw(void *opaque, target_phys_addr_t addr)
{
    uint32_t v;
#ifdef TARGET_WORDS_BIGENDIAN
    v = vga_mem_readb(opaque, addr) << 8;
    v |= vga_mem_readb(opaque, addr + 1);
#else
    v = vga_mem_readb(opaque, addr);
    v |= vga_mem_readb(opaque, addr + 1) << 8;
#endif
    return v;
}

static uint32_t vga_mem_readl(void *opaque, target_phys_addr_t addr)
{
    uint32_t v;
#ifdef TARGET_WORDS_BIGENDIAN
    v = vga_mem_readb(opaque, addr) << 24;
    v |= vga_mem_readb(opaque, addr + 1) << 16;
    v |= vga_mem_readb(opaque, addr + 2) << 8;
    v |= vga_mem_readb(opaque, addr + 3);
#else
    v = vga_mem_readb(opaque, addr);
    v |= vga_mem_readb(opaque, addr + 1) << 8;
    v |= vga_mem_readb(opaque, addr + 2) << 16;
    v |= vga_mem_readb(opaque, addr + 3) << 24;
#endif
    return v;
}
#endif /* !VBOX */

/* called for accesses between 0xa0000 and 0xc0000 */
#ifdef VBOX
static
#endif /* VBOX */
int vga_mem_writeb(void *opaque, target_phys_addr_t addr, uint32_t val)
{
    VGAState *s = (VGAState*)opaque;
    int memory_map_mode, plane, write_mode, b, func_select, mask;
    uint32_t write_mask, bit_mask, set_mask;

#ifdef DEBUG_VGA_MEM
    Log(("vga: [0x%x] = 0x%02x\n", addr, val));
#endif
    /* convert to VGA memory offset */
    memory_map_mode = (s->gr[6] >> 2) & 3;
    addr &= 0x1ffff;
    switch(memory_map_mode) {
    case 0:
        break;
    case 1:
        if (addr >= 0x10000)
            return VINF_SUCCESS;
        addr += s->bank_offset;
        break;
    case 2:
        addr -= 0x10000;
        if (addr >= 0x8000)
            return VINF_SUCCESS;
        break;
    default:
    case 3:
        addr -= 0x18000;
        if (addr >= 0x8000)
            return VINF_SUCCESS;
        break;
    }

    if (s->sr[4] & 0x08) {
        /* chain 4 mode : simplest access */
        plane = addr & 3;
        mask = (1 << plane);
        if (s->sr[2] & mask) {
#ifndef VBOX
            s->vram_ptr[addr] = val;
#else /* VBOX */
#ifdef IN_GC
            if (addr >= VGA_MAPPING_SIZE)
                return VINF_IOM_HC_MMIO_WRITE;
#else
            if (addr >= s->vram_size)
            {
                AssertMsgFailed(("addr=%VGp - this needs to be done in HC! bank_offset=%08x memory_map_mode=%d\n",
                                 addr, s->bank_offset, memory_map_mode));
                return VINF_SUCCESS;
            }
#endif
            s->CTXSUFF(vram_ptr)[addr] = val;
#endif /* VBOX */
#ifdef DEBUG_VGA_MEM
            Log(("vga: chain4: [0x%x]\n", addr));
#endif
            s->plane_updated |= mask; /* only used to detect font change */
#ifndef VBOX
            cpu_physical_memory_set_dirty(s->vram_offset + addr);
#else /* VBOX */
            vga_set_dirty(s, addr);
#endif /* VBOX */
        }
    } else if (s->gr[5] & 0x10) {
        /* odd/even mode (aka text mode mapping) */
        plane = (s->gr[4] & 2) | (addr & 1);
        mask = (1 << plane);
        if (s->sr[2] & mask) {
#ifndef VBOX
            addr = ((addr & ~1) << 1) | plane;
#else
            /* 'addr' is offset in a plane, bit 0 selects the plane.
             * Mask the bit 0, convert plane index to vram offset,
             * that is multiply by the number of planes,
             * and select the plane byte in the vram offset.
             */
            addr = ((addr & ~1) << 2) | plane;
#endif /* VBOX */
#ifndef VBOX
            s->vram_ptr[addr] = val;
#else /* VBOX */
#ifdef IN_GC
            if (addr >= VGA_MAPPING_SIZE)
                return VINF_IOM_HC_MMIO_WRITE;
#else
            if (addr >= s->vram_size)
            {
                AssertMsgFailed(("addr=%VGp - this needs to be done in HC! bank_offset=%08x memory_map_mode=%d\n",
                                 addr, s->bank_offset, memory_map_mode));
                return VINF_SUCCESS;
            }
#endif
            s->CTXSUFF(vram_ptr)[addr] = val;
#endif /* VBOX */
#ifdef DEBUG_VGA_MEM
            Log(("vga: odd/even: [0x%x]\n", addr));
#endif
            s->plane_updated |= mask; /* only used to detect font change */
#ifndef VBOX
            cpu_physical_memory_set_dirty(s->vram_offset + addr);
#else /* VBOX */
            vga_set_dirty(s, addr);
#endif /* VBOX */
        }
    } else {
#ifdef IN_GC
        if (addr * 4 >= VGA_MAPPING_SIZE)
            return VINF_IOM_HC_MMIO_WRITE;
#else
        if (addr * 4 >= s->vram_size)
        {
            AssertMsgFailed(("addr=%VGp - this needs to be done in HC! bank_offset=%08x memory_map_mode=%d\n",
                             addr * 4, s->bank_offset, memory_map_mode));
            return VINF_SUCCESS;
        }
#endif

        /* standard VGA latched access */
        write_mode = s->gr[5] & 3;
        switch(write_mode) {
        default:
        case 0:
            /* rotate */
            b = s->gr[3] & 7;
            val = ((val >> b) | (val << (8 - b))) & 0xff;
            val |= val << 8;
            val |= val << 16;

            /* apply set/reset mask */
            set_mask = mask16[s->gr[1]];
            val = (val & ~set_mask) | (mask16[s->gr[0]] & set_mask);
            bit_mask = s->gr[8];
            break;
        case 1:
            val = s->latch;
            goto do_write;
        case 2:
            val = mask16[val & 0x0f];
            bit_mask = s->gr[8];
            break;
        case 3:
            /* rotate */
            b = s->gr[3] & 7;
            val = (val >> b) | (val << (8 - b));

            bit_mask = s->gr[8] & val;
            val = mask16[s->gr[0]];
            break;
        }

        /* apply logical operation */
        func_select = s->gr[3] >> 3;
        switch(func_select) {
        case 0:
        default:
            /* nothing to do */
            break;
        case 1:
            /* and */
            val &= s->latch;
            break;
        case 2:
            /* or */
            val |= s->latch;
            break;
        case 3:
            /* xor */
            val ^= s->latch;
            break;
        }

        /* apply bit mask */
        bit_mask |= bit_mask << 8;
        bit_mask |= bit_mask << 16;
        val = (val & bit_mask) | (s->latch & ~bit_mask);

    do_write:
        /* mask data according to sr[2] */
        mask = s->sr[2];
        s->plane_updated |= mask; /* only used to detect font change */
        write_mask = mask16[mask];
#ifndef VBOX
        ((uint32_t *)s->vram_ptr)[addr] =
            (((uint32_t *)s->vram_ptr)[addr] & ~write_mask) |
            (val & write_mask);
#else /* VBOX */
        ((uint32_t *)s->CTXSUFF(vram_ptr))[addr] =
            (((uint32_t *)s->CTXSUFF(vram_ptr))[addr] & ~write_mask) |
            (val & write_mask);
#endif /* VBOX */
#ifdef DEBUG_VGA_MEM
            Log(("vga: latch: [0x%x] mask=0x%08x val=0x%08x\n",
                   addr * 4, write_mask, val));
#endif
#ifndef VBOX
            cpu_physical_memory_set_dirty(s->vram_offset + (addr << 2));
#else /* VBOX */
            vga_set_dirty(s, (addr << 2));
#endif /* VBOX */
    }

    return VINF_SUCCESS;
}

#ifndef VBOX
static void vga_mem_writew(void *opaque, target_phys_addr_t addr, uint32_t val)
{
#ifdef TARGET_WORDS_BIGENDIAN
    vga_mem_writeb(opaque, addr, (val >> 8) & 0xff);
    vga_mem_writeb(opaque, addr + 1, val & 0xff);
#else
    vga_mem_writeb(opaque, addr, val & 0xff);
    vga_mem_writeb(opaque, addr + 1, (val >> 8) & 0xff);
#endif
}

static void vga_mem_writel(void *opaque, target_phys_addr_t addr, uint32_t val)
{
#ifdef TARGET_WORDS_BIGENDIAN
    vga_mem_writeb(opaque, addr, (val >> 24) & 0xff);
    vga_mem_writeb(opaque, addr + 1, (val >> 16) & 0xff);
    vga_mem_writeb(opaque, addr + 2, (val >> 8) & 0xff);
    vga_mem_writeb(opaque, addr + 3, val & 0xff);
#else
    vga_mem_writeb(opaque, addr, val & 0xff);
    vga_mem_writeb(opaque, addr + 1, (val >> 8) & 0xff);
    vga_mem_writeb(opaque, addr + 2, (val >> 16) & 0xff);
    vga_mem_writeb(opaque, addr + 3, (val >> 24) & 0xff);
#endif
}
#endif /* !VBOX */

#if !defined(VBOX) || defined(IN_RING3)
typedef void vga_draw_glyph8_func(uint8_t *d, int linesize,
                             const uint8_t *font_ptr, int h,
                             uint32_t fgcol, uint32_t bgcol);
typedef void vga_draw_glyph9_func(uint8_t *d, int linesize,
                                  const uint8_t *font_ptr, int h,
                                  uint32_t fgcol, uint32_t bgcol, int dup9);
typedef void vga_draw_line_func(VGAState *s1, uint8_t *d,
                                const uint8_t *s, int width);

static inline unsigned int rgb_to_pixel8(unsigned int r, unsigned int g, unsigned b)
{
    return ((r >> 5) << 5) | ((g >> 5) << 2) | (b >> 6);
}

static inline unsigned int rgb_to_pixel15(unsigned int r, unsigned int g, unsigned b)
{
    return ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
}

static inline unsigned int rgb_to_pixel16(unsigned int r, unsigned int g, unsigned b)
{
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

static inline unsigned int rgb_to_pixel32(unsigned int r, unsigned int g, unsigned b)
{
    return (r << 16) | (g << 8) | b;
}

#define DEPTH 8
#include "DevVGATmpl.h"

#define DEPTH 15
#include "DevVGATmpl.h"

#define DEPTH 16
#include "DevVGATmpl.h"

#define DEPTH 32
#include "DevVGATmpl.h"

static unsigned int rgb_to_pixel8_dup(unsigned int r, unsigned int g, unsigned b)
{
    unsigned int col;
    col = rgb_to_pixel8(r, g, b);
    col |= col << 8;
    col |= col << 16;
    return col;
}

static unsigned int rgb_to_pixel15_dup(unsigned int r, unsigned int g, unsigned b)
{
    unsigned int col;
    col = rgb_to_pixel15(r, g, b);
    col |= col << 16;
    return col;
}

static unsigned int rgb_to_pixel16_dup(unsigned int r, unsigned int g, unsigned b)
{
    unsigned int col;
    col = rgb_to_pixel16(r, g, b);
    col |= col << 16;
    return col;
}

static unsigned int rgb_to_pixel32_dup(unsigned int r, unsigned int g, unsigned b)
{
    unsigned int col;
    col = rgb_to_pixel32(r, g, b);
    return col;
}

/* return true if the palette was modified */
static int update_palette16(VGAState *s)
{
    int full_update, i;
    uint32_t v, col, *palette;

    full_update = 0;
    palette = s->last_palette;
    for(i = 0; i < 16; i++) {
        v = s->ar[i];
        if (s->ar[0x10] & 0x80)
            v = ((s->ar[0x14] & 0xf) << 4) | (v & 0xf);
        else
            v = ((s->ar[0x14] & 0xc) << 4) | (v & 0x3f);
        v = v * 3;
        col = s->rgb_to_pixel(c6_to_8(s->palette[v]),
                              c6_to_8(s->palette[v + 1]),
                              c6_to_8(s->palette[v + 2]));
        if (col != palette[i]) {
            full_update = 1;
            palette[i] = col;
        }
    }
    return full_update;
}

/* return true if the palette was modified */
static int update_palette256(VGAState *s)
{
    int full_update, i;
    uint32_t v, col, *palette;
    int wide_dac;

    full_update = 0;
    palette = s->last_palette;
    v = 0;
    wide_dac = (s->vbe_regs[VBE_DISPI_INDEX_ENABLE] & (VBE_DISPI_ENABLED | VBE_DISPI_8BIT_DAC))
             == (VBE_DISPI_ENABLED | VBE_DISPI_8BIT_DAC);
    for(i = 0; i < 256; i++) {
        if (wide_dac)
            col = s->rgb_to_pixel(s->palette[v],
                                  s->palette[v + 1],
                                  s->palette[v + 2]);
        else
            col = s->rgb_to_pixel(c6_to_8(s->palette[v]),
                                  c6_to_8(s->palette[v + 1]),
                                  c6_to_8(s->palette[v + 2]));
        if (col != palette[i]) {
            full_update = 1;
            palette[i] = col;
        }
        v += 3;
    }
    return full_update;
}

static void vga_get_offsets(VGAState *s,
                            uint32_t *pline_offset,
                            uint32_t *pstart_addr,
                            uint32_t *pline_compare)
{
    uint32_t start_addr, line_offset, line_compare;
#ifdef CONFIG_BOCHS_VBE
    if (s->vbe_regs[VBE_DISPI_INDEX_ENABLE] & VBE_DISPI_ENABLED) {
        line_offset = s->vbe_line_offset;
        start_addr = s->vbe_start_addr;
        line_compare = 65535;
    } else
#endif
    {
        /* compute line_offset in bytes */
        line_offset = s->cr[0x13];
        line_offset <<= 3;
#ifdef VBOX
        if (!(s->cr[0x14] & 0x40) && !(s->cr[0x17] & 0x40))
        {
            /* Word mode. Used for odd/even modes. */
            line_offset *= 2;
        }
#endif /* VBOX */

        /* starting address */
        start_addr = s->cr[0x0d] | (s->cr[0x0c] << 8);

        /* line compare */
        line_compare = s->cr[0x18] |
            ((s->cr[0x07] & 0x10) << 4) |
            ((s->cr[0x09] & 0x40) << 3);
    }
    *pline_offset = line_offset;
    *pstart_addr = start_addr;
    *pline_compare = line_compare;
}

/* update start_addr and line_offset. Return TRUE if modified */
static int update_basic_params(VGAState *s)
{
    int full_update;
    uint32_t start_addr, line_offset, line_compare;

    full_update = 0;

    s->get_offsets(s, &line_offset, &start_addr, &line_compare);

    if (line_offset != s->line_offset ||
        start_addr != s->start_addr ||
        line_compare != s->line_compare) {
        s->line_offset = line_offset;
        s->start_addr = start_addr;
        s->line_compare = line_compare;
        full_update = 1;
    }
    return full_update;
}

static inline int get_depth_index(int depth)
{
    switch(depth) {
    default:
    case 8:
        return 0;
    case 15:
        return 1;
    case 16:
        return 2;
    case 32:
        return 3;
    }
}

static vga_draw_glyph8_func *vga_draw_glyph8_table[4] = {
    vga_draw_glyph8_8,
    vga_draw_glyph8_16,
    vga_draw_glyph8_16,
    vga_draw_glyph8_32,
};

static vga_draw_glyph8_func *vga_draw_glyph16_table[4] = {
    vga_draw_glyph16_8,
    vga_draw_glyph16_16,
    vga_draw_glyph16_16,
    vga_draw_glyph16_32,
};

static vga_draw_glyph9_func *vga_draw_glyph9_table[4] = {
    vga_draw_glyph9_8,
    vga_draw_glyph9_16,
    vga_draw_glyph9_16,
    vga_draw_glyph9_32,
};

static const uint8_t cursor_glyph[32 * 4] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

/*
 * Text mode update
 * Missing:
 * - double scan
 * - double width
 * - underline
 * - flashing
 */
#ifndef VBOX
static void vga_draw_text(VGAState *s, int full_update)
#else
static int vga_draw_text(VGAState *s, int full_update)
#endif /* !VBOX */
{
    int cx, cy, cheight, cw, ch, cattr, height, width, ch_attr;
    int cx_min, cx_max, linesize, x_incr;
    uint32_t offset, fgcol, bgcol, v, cursor_offset;
    uint8_t *d1, *d, *src, *s1, *dest, *cursor_ptr;
    const uint8_t *font_ptr, *font_base[2];
    int dup9, line_offset, depth_index;
    uint32_t *palette;
    uint32_t *ch_attr_ptr;
    vga_draw_glyph8_func *vga_draw_glyph8;
    vga_draw_glyph9_func *vga_draw_glyph9;

    full_update |= update_palette16(s);
    palette = s->last_palette;

    /* compute font data address (in plane 2) */
    v = s->sr[3];
    offset = (((v >> 4) & 1) | ((v << 1) & 6)) * 8192 * 4 + 2;
    if (offset != s->font_offsets[0]) {
        s->font_offsets[0] = offset;
        full_update = 1;
    }
#ifndef VBOX
    font_base[0] = s->vram_ptr + offset;
#else /* VBOX */
    font_base[0] = s->CTXSUFF(vram_ptr) + offset;
#endif /* VBOX */

    offset = (((v >> 5) & 1) | ((v >> 1) & 6)) * 8192 * 4 + 2;
#ifndef VBOX
    font_base[1] = s->vram_ptr + offset;
#else /* VBOX */
    font_base[1] = s->CTXSUFF(vram_ptr) + offset;
#endif /* VBOX */
    if (offset != s->font_offsets[1]) {
        s->font_offsets[1] = offset;
        full_update = 1;
    }
    if (s->plane_updated & (1 << 2)) {
        /* if the plane 2 was modified since the last display, it
           indicates the font may have been modified */
        s->plane_updated = 0;
        full_update = 1;
    }
    full_update |= update_basic_params(s);

    line_offset = s->line_offset;
#ifndef VBOX
    s1 = s->vram_ptr + (s->start_addr * 4);
#else /* VBOX */
    s1 = s->CTXSUFF(vram_ptr) + (s->start_addr * 8);
#endif /* VBOX */

    /* total width & height */
    cheight = (s->cr[9] & 0x1f) + 1;
    cw = 8;
    if (!(s->sr[1] & 0x01))
        cw = 9;
    if (s->sr[1] & 0x08)
        cw = 16; /* NOTE: no 18 pixel wide */
#ifndef VBOX
    x_incr = cw * ((s->ds->depth + 7) >> 3);
#else /* VBOX */
    x_incr = cw * ((s->pDrv->cBits + 7) >> 3);
#endif /* VBOX */
    width = (s->cr[0x01] + 1);
    if (s->cr[0x06] == 100) {
        /* ugly hack for CGA 160x100x16 - explain me the logic */
        height = 100;
    } else {
        height = s->cr[0x12] |
            ((s->cr[0x07] & 0x02) << 7) |
            ((s->cr[0x07] & 0x40) << 3);
        height = (height + 1) / cheight;
    }
    if ((height * width) > CH_ATTR_SIZE) {
        /* better than nothing: exit if transient size is too big */
#ifndef VBOX
        return;
#else
        return VINF_SUCCESS;
#endif /* VBOX */
    }

    if (width != (int)s->last_width || height != (int)s->last_height ||
        cw != s->last_cw || cheight != s->last_ch) {
        s->last_scr_width = width * cw;
        s->last_scr_height = height * cheight;
#ifndef VBOX
        dpy_resize(s->ds, s->last_scr_width, s->last_scr_height);
        s->last_width = width;
        s->last_height = height;
        s->last_ch = cheight;
        s->last_cw = cw;
        full_update = 1;
#else /* VBOX */
        /* For text modes the direct use of guest VRAM is not implemented, so bpp and cbLine are 0 here. */
        int rc = s->pDrv->pfnResize(s->pDrv, 0, NULL, 0, s->last_scr_width, s->last_scr_height);
        s->last_width = width;
        s->last_height = height;
        s->last_ch = cheight;
        s->last_cw = cw;
        full_update = 1;
        if (rc == VINF_VGA_RESIZE_IN_PROGRESS)
            return rc;
        AssertRC(rc);
#endif /* VBOX */
    }
    cursor_offset = ((s->cr[0x0e] << 8) | s->cr[0x0f]) - s->start_addr;
    if (cursor_offset != s->cursor_offset ||
        s->cr[0xa] != s->cursor_start ||
        s->cr[0xb] != s->cursor_end) {
      /* if the cursor position changed, we update the old and new
         chars */
        if (s->cursor_offset < CH_ATTR_SIZE)
            s->last_ch_attr[s->cursor_offset] = ~0;
        if (cursor_offset < CH_ATTR_SIZE)
            s->last_ch_attr[cursor_offset] = ~0;
        s->cursor_offset = cursor_offset;
        s->cursor_start = s->cr[0xa];
        s->cursor_end = s->cr[0xb];
    }
#ifndef VBOX
    cursor_ptr = s->vram_ptr + (s->start_addr + cursor_offset) * 4;

    depth_index = get_depth_index(s->ds->depth);
#else /* VBOX */
    cursor_ptr = s->CTXSUFF(vram_ptr) + (s->start_addr + cursor_offset) * 8;
    depth_index = get_depth_index(s->pDrv->cBits);
#endif /* VBOX */
    if (cw == 16)
        vga_draw_glyph8 = vga_draw_glyph16_table[depth_index];
    else
        vga_draw_glyph8 = vga_draw_glyph8_table[depth_index];
    vga_draw_glyph9 = vga_draw_glyph9_table[depth_index];

#ifndef VBOX
    dest = s->ds->data;
    linesize = s->ds->linesize;
#else /* VBOX */
    dest = s->pDrv->pu8Data;
    linesize = s->pDrv->cbScanline;
#endif /* VBOX */
    ch_attr_ptr = s->last_ch_attr;

    for(cy = 0; cy < height; cy++) {
        d1 = dest;
        src = s1;
        cx_min = width;
        cx_max = -1;
        for(cx = 0; cx < width; cx++) {
            ch_attr = *(uint16_t *)src;
            if (full_update || ch_attr != (int)*ch_attr_ptr) {
                if (cx < cx_min)
                    cx_min = cx;
                if (cx > cx_max)
                    cx_max = cx;
                *ch_attr_ptr = ch_attr;
#ifdef WORDS_BIGENDIAN
                ch = ch_attr >> 8;
                cattr = ch_attr & 0xff;
#else
                ch = ch_attr & 0xff;
                cattr = ch_attr >> 8;
#endif
                font_ptr = font_base[(cattr >> 3) & 1];
                font_ptr += 32 * 4 * ch;
                bgcol = palette[cattr >> 4];
                fgcol = palette[cattr & 0x0f];
                if (cw != 9) {
                    vga_draw_glyph8(d1, linesize,
                                    font_ptr, cheight, fgcol, bgcol);
                } else {
                    dup9 = 0;
                    if (ch >= 0xb0 && ch <= 0xdf && (s->ar[0x10] & 0x04))
                        dup9 = 1;
                    vga_draw_glyph9(d1, linesize,
                                    font_ptr, cheight, fgcol, bgcol, dup9);
                }
                if (src == cursor_ptr &&
                    !(s->cr[0x0a] & 0x20)) {
                    int line_start, line_last, h;
                    /* draw the cursor */
                    line_start = s->cr[0x0a] & 0x1f;
                    line_last = s->cr[0x0b] & 0x1f;
                    /* XXX: check that */
                    if (line_last > cheight - 1)
                        line_last = cheight - 1;
                    if (line_last >= line_start && line_start < cheight) {
                        h = line_last - line_start + 1;
                        d = d1 + linesize * line_start;
                        if (cw != 9) {
                            vga_draw_glyph8(d, linesize,
                                            cursor_glyph, h, fgcol, bgcol);
                        } else {
                            vga_draw_glyph9(d, linesize,
                                            cursor_glyph, h, fgcol, bgcol, 1);
                        }
                    }
                }
            }
            d1 += x_incr;
#ifndef VBOX
            src += 4;
#else
            src += 8; /* Every second byte of a plane is used in text mode. */
#endif

            ch_attr_ptr++;
        }
#ifndef VBOX
        if (cx_max != -1) {
            dpy_update(s->ds, cx_min * cw, cy * cheight,
                       (cx_max - cx_min + 1) * cw, cheight);
        }
#else
        if (cx_max != -1)
            s->pDrv->pfnUpdateRect(s->pDrv, cx_min * cw, cy * cheight, (cx_max - cx_min + 1) * cw, cheight);
#endif
        dest += linesize * cheight;
        s1 += line_offset;
    }
#ifdef VBOX
        return VINF_SUCCESS;
#endif /* VBOX */
}

enum {
    VGA_DRAW_LINE2,
    VGA_DRAW_LINE2D2,
    VGA_DRAW_LINE4,
    VGA_DRAW_LINE4D2,
    VGA_DRAW_LINE8D2,
    VGA_DRAW_LINE8,
    VGA_DRAW_LINE15,
    VGA_DRAW_LINE16,
    VGA_DRAW_LINE24,
    VGA_DRAW_LINE32,
    VGA_DRAW_LINE_NB
};

static vga_draw_line_func *vga_draw_line_table[4 * VGA_DRAW_LINE_NB] = {
    vga_draw_line2_8,
    vga_draw_line2_16,
    vga_draw_line2_16,
    vga_draw_line2_32,

    vga_draw_line2d2_8,
    vga_draw_line2d2_16,
    vga_draw_line2d2_16,
    vga_draw_line2d2_32,

    vga_draw_line4_8,
    vga_draw_line4_16,
    vga_draw_line4_16,
    vga_draw_line4_32,

    vga_draw_line4d2_8,
    vga_draw_line4d2_16,
    vga_draw_line4d2_16,
    vga_draw_line4d2_32,

    vga_draw_line8d2_8,
    vga_draw_line8d2_16,
    vga_draw_line8d2_16,
    vga_draw_line8d2_32,

    vga_draw_line8_8,
    vga_draw_line8_16,
    vga_draw_line8_16,
    vga_draw_line8_32,

    vga_draw_line15_8,
    vga_draw_line15_15,
    vga_draw_line15_16,
    vga_draw_line15_32,

    vga_draw_line16_8,
    vga_draw_line16_15,
    vga_draw_line16_16,
    vga_draw_line16_32,

    vga_draw_line24_8,
    vga_draw_line24_15,
    vga_draw_line24_16,
    vga_draw_line24_32,

    vga_draw_line32_8,
    vga_draw_line32_15,
    vga_draw_line32_16,
    vga_draw_line32_32,
};

static int vga_get_bpp(VGAState *s)
{
    int ret;
#ifdef CONFIG_BOCHS_VBE
    if (s->vbe_regs[VBE_DISPI_INDEX_ENABLE] & VBE_DISPI_ENABLED) {
        ret = s->vbe_regs[VBE_DISPI_INDEX_BPP];
    } else
#endif
    {
        ret = 0;
    }
    return ret;
}

static void vga_get_resolution(VGAState *s, int *pwidth, int *pheight)
{
    int width, height;
#ifdef CONFIG_BOCHS_VBE
    if (s->vbe_regs[VBE_DISPI_INDEX_ENABLE] & VBE_DISPI_ENABLED) {
        width = s->vbe_regs[VBE_DISPI_INDEX_XRES];
        height = s->vbe_regs[VBE_DISPI_INDEX_YRES];
    } else
#endif
    {
        width = (s->cr[0x01] + 1) * 8;
        height = s->cr[0x12] |
            ((s->cr[0x07] & 0x02) << 7) |
            ((s->cr[0x07] & 0x40) << 3);
        height = (height + 1);
    }
    *pwidth = width;
    *pheight = height;
}

#ifndef VBOX
void vga_invalidate_scanlines(VGAState *s, int y1, int y2)
{
    int y;
    if (y1 >= VGA_MAX_HEIGHT)
        return;
    if (y2 >= VGA_MAX_HEIGHT)
        y2 = VGA_MAX_HEIGHT;
    for(y = y1; y < y2; y++) {
        s->invalidated_y_table[y >> 5] |= 1 << (y & 0x1f);
    }
}
#endif /* !VBOX*/

#ifdef VBOX
/**
 * Performs the display driver resizing when in graphics mode.
 *
 * This will recalc / update any status data depending on the driver
 * properties (bit depth mostly).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VINF_VGA_RESIZE_IN_PROGRESS if the operation wasn't complete.
 * @param   s       Pointer to the vga status.
 * @param   cx      The width.
 * @param   cy      The height.
 */
static int vga_resize_graphic(VGAState *s, int cx, int cy, int v)
{
    const unsigned cBits = s->get_bpp(s);
    /** @todo r=sunlover: If the guest changes VBE_DISPI_INDEX_X_OFFSET, VBE_DISPI_INDEX_Y_OFFSET
     *                    registers, then the third parameter of the following call should be
     *                    probably 's->CTXSUFF(vram_ptr) + s->vbe_start_addr'.
     */
    int rc = s->pDrv->pfnResize(s->pDrv, cBits, s->CTXSUFF(vram_ptr), s->line_offset, cx, cy);

    /* last stuff */
    s->last_bpp = cBits;
    s->last_scr_width = cx;
    s->last_scr_height = cy;
    s->last_width = cx;
    s->last_height = cy;

    if (rc == VINF_VGA_RESIZE_IN_PROGRESS)
        return rc;
    AssertRC(rc);

    /* update palette */
    switch (s->pDrv->cBits)
    {
        case 32:    s->rgb_to_pixel = rgb_to_pixel32_dup; break;
        case 16:
        default:    s->rgb_to_pixel = rgb_to_pixel16_dup; break;
        case 15:    s->rgb_to_pixel = rgb_to_pixel15_dup; break;
        case 8:     s->rgb_to_pixel = rgb_to_pixel8_dup;  break;
    }
    if (s->shift_control == 0)
        update_palette16(s);
    else if (s->shift_control == 1)
        update_palette16(s);
    return VINF_SUCCESS;
}
#endif /* VBOX */

/*
 * graphic modes
 */
#ifndef VBOX
static void vga_draw_graphic(VGAState *s, int full_update)
#else
static int vga_draw_graphic(VGAState *s, int full_update)
#endif /* !VBOX */
{
    int y1, y2, y, update, page_min, page_max, linesize, y_start, double_scan;
    int width, height, shift_control, line_offset, page0, page1, bwidth;
    int disp_width, multi_run;
    uint8_t *d;
    uint32_t v, addr1, addr;
    vga_draw_line_func *vga_draw_line;
    bool offsets_changed;

    offsets_changed = update_basic_params(s);

    full_update |= offsets_changed;

    s->get_resolution(s, &width, &height);
    disp_width = width;

    shift_control = (s->gr[0x05] >> 5) & 3;
    double_scan = (s->cr[0x09] >> 7);
    multi_run = double_scan;
    if (shift_control != s->shift_control ||
        double_scan != s->double_scan) {
        full_update = 1;
        s->shift_control = shift_control;
        s->double_scan = double_scan;
    }

    if (shift_control == 0) {
        full_update |= update_palette16(s);
        if (s->sr[0x01] & 8) {
            v = VGA_DRAW_LINE4D2;
            disp_width <<= 1;
        } else {
            v = VGA_DRAW_LINE4;
        }
    } else if (shift_control == 1) {
        full_update |= update_palette16(s);
        if (s->sr[0x01] & 8) {
            v = VGA_DRAW_LINE2D2;
            disp_width <<= 1;
        } else {
            v = VGA_DRAW_LINE2;
        }
    } else {
        switch(s->get_bpp(s)) {
        default:
        case 0:
            full_update |= update_palette256(s);
            v = VGA_DRAW_LINE8D2;
            break;
        case 8:
            full_update |= update_palette256(s);
            v = VGA_DRAW_LINE8;
            break;
        case 15:
            v = VGA_DRAW_LINE15;
            break;
        case 16:
            v = VGA_DRAW_LINE16;
            break;
        case 24:
            v = VGA_DRAW_LINE24;
            break;
        case 32:
            v = VGA_DRAW_LINE32;
            break;
        }
    }
#ifndef VBOX
    vga_draw_line = vga_draw_line_table[v * 4 + get_depth_index(s->ds->depth)];

    if (disp_width != s->last_width ||
        height != s->last_height) {
        dpy_resize(s->ds, disp_width, height);
        s->last_scr_width = disp_width;
        s->last_scr_height = height;
        s->last_width = disp_width;
        s->last_height = height;
        full_update = 1;
    }
#else /* VBOX */
    if (    disp_width     != (int)s->last_width
        ||  height         != (int)s->last_height
        ||  s->get_bpp(s)  != (int)s->last_bpp
        ||  offsets_changed)
    {
        int rc = vga_resize_graphic(s, disp_width, height, v);
        if (rc != VINF_SUCCESS)  /* Return any rc, particularly VINF_VGA_RESIZE_IN_PROGRESS, to the caller. */
            return rc;
        full_update = 1;
    }
    vga_draw_line = vga_draw_line_table[v * 4 + get_depth_index(s->pDrv->cBits)];

#endif /* VBOX */
    if (s->cursor_invalidate)
        s->cursor_invalidate(s);

    line_offset = s->line_offset;
#if 0
    Log(("w=%d h=%d v=%d line_offset=%d cr[0x09]=0x%02x cr[0x17]=0x%02x linecmp=%d sr[0x01]=0x%02x\n",
           width, height, v, line_offset, s->cr[9], s->cr[0x17], s->line_compare, s->sr[0x01]));
#endif
    addr1 = (s->start_addr * 4);
#ifndef VBOX
    bwidth = width * 4;
#else /* VBOX */
    /* The width of VRAM scanline. */
    bwidth = s->line_offset;
    /* In some cases the variable is not yet set, probably due to incomplete
     * programming of the virtual hardware ports. Just return.
     */
    if (bwidth == 0) return VINF_SUCCESS;
#endif /* VBOX */
    y_start = -1;
    page_min = 0x7fffffff;
    page_max = -1;
#ifndef VBOX
    d = s->ds->data;
    linesize = s->ds->linesize;
#else /* VBOX */
    d = s->pDrv->pu8Data;
    linesize = s->pDrv->cbScanline;
#endif /* VBOX */

    y1 = 0;
    y2 = s->cr[0x09] & 0x1F;    /* starting row scan count */
    for(y = 0; y < height; y++) {
        addr = addr1;
        /* CGA/MDA compatibility. Note that these addresses are all 
         * shifted left by two compared to VGA specs.
         */
        if (!(s->cr[0x17] & 1)) {
            addr = (addr & ~(1 << 15)) | ((y1 & 1) << 15);
        }
        if (!(s->cr[0x17] & 2)) {
            addr = (addr & ~(1 << 16)) | ((y1 & 2) << 15);
        }
#ifndef VBOX
        page0 = s->vram_offset + (addr & TARGET_PAGE_MASK);
        page1 = s->vram_offset + ((addr + bwidth - 1) & TARGET_PAGE_MASK);
        update = full_update |
            cpu_physical_memory_get_dirty(page0, VGA_DIRTY_FLAG) |
            cpu_physical_memory_get_dirty(page1, VGA_DIRTY_FLAG);
        if ((page1 - page0) > TARGET_PAGE_SIZE) {
            /* if wide line, can use another page */
            update |= cpu_physical_memory_get_dirty(page0 + TARGET_PAGE_SIZE,
                                                    VGA_DIRTY_FLAG);
        }
#else /* VBOX */
        page0 = addr & TARGET_PAGE_MASK;
        page1 = (addr + bwidth - 1) & TARGET_PAGE_MASK;
        update = full_update | vga_is_dirty(s, page0) | vga_is_dirty(s, page1);
        if (page1 - page0 > TARGET_PAGE_SIZE) {
            /* if wide line, can use another page */
            update |= vga_is_dirty(s, page0 + TARGET_PAGE_SIZE);
        }
#endif /* VBOX */
        /* explicit invalidation for the hardware cursor */
        update |= (s->invalidated_y_table[y >> 5] >> (y & 0x1f)) & 1;
        if (update) {
            if (y_start < 0)
                y_start = y;
            if (page0 < page_min)
                page_min = page0;
            if (page1 > page_max)
                page_max = page1;
#ifndef VBOX
            vga_draw_line(s, d, s->vram_ptr + addr, width);
#else /* VBOX */
            if (s->fRenderVRAM)
                vga_draw_line(s, d, s->CTXSUFF(vram_ptr) + addr, width);
#endif /* VBOX */
            if (s->cursor_draw_line)
                s->cursor_draw_line(s, d, y);
        } else {
            if (y_start >= 0) {
                /* flush to display */
#ifndef VBOX
                dpy_update(s->ds, 0, y_start,
                           disp_width, y - y_start);
#else /* VBOX */
                s->pDrv->pfnUpdateRect(s->pDrv, 0, y_start, disp_width, y - y_start);
#endif /* VBOX */
                y_start = -1;
            }
        }
        if (!multi_run) {
            y1++;
            multi_run = double_scan;

            if (y2 == 0) {
                y2 = s->cr[0x09] & 0x1F;
                addr1 += line_offset;
            } else {
                --y2;
            }
        } else {
            multi_run--;
        }
        /* line compare acts on the displayed lines */
        if ((uint32_t)y == s->line_compare)
            addr1 = 0;
        d += linesize;
    }
    if (y_start >= 0) {
        /* flush to display */
#ifndef VBOX
        dpy_update(s->ds, 0, y_start,
                   disp_width, y - y_start);
#else /* VBOX */
        s->pDrv->pfnUpdateRect(s->pDrv, 0, y_start, disp_width, y - y_start);
#endif /* VBOX */
    }
    /* reset modified pages */
    if (page_max != -1) {
#ifndef VBOX
        cpu_physical_memory_reset_dirty(page_min, page_max + TARGET_PAGE_SIZE,
                                        VGA_DIRTY_FLAG);
#else /* VBOX */
        vga_reset_dirty(s, page_min, page_max + TARGET_PAGE_SIZE);
#endif /* VBOX */
    }
    memset(s->invalidated_y_table, 0, ((height + 31) >> 5) * 4);
#ifdef VBOX
    return VINF_SUCCESS;
#endif /* VBOX */
}

static void vga_draw_blank(VGAState *s, int full_update)
{
#ifndef VBOX
    int i, w, val;
    uint8_t *d;

    if (!full_update)
        return;
    if (s->last_scr_width <= 0 || s->last_scr_height <= 0)
        return;
    if (s->ds->depth == 8)
        val = s->rgb_to_pixel(0, 0, 0);
    else
        val = 0;
    w = s->last_scr_width * ((s->ds->depth + 7) >> 3);
    d = s->ds->data;
    for(i = 0; i < s->last_scr_height; i++) {
        memset(d, val, w);
        d += s->ds->linesize;
    }
    dpy_update(s->ds, 0, 0,
               s->last_scr_width, s->last_scr_height);
#else  /* VBOX */

    int i, w, val;
    uint8_t *d;
    uint32_t cbScanline = s->pDrv->cbScanline;

    if (!full_update)
        return;
    if (s->last_scr_width <= 0 || s->last_scr_height <= 0)
        return;
    if (s->pDrv->cBits == 8)
        val = s->rgb_to_pixel(0, 0, 0);
    else
        val = 0;
    w = s->last_scr_width * ((s->pDrv->cBits + 7) >> 3);
    d = s->pDrv->pu8Data;
    for(i = 0; i < (int)s->last_scr_height; i++) {
        memset(d, val, w);
        d += cbScanline;
    }
    s->pDrv->pfnUpdateRect(s->pDrv, 0, 0, s->last_scr_width, s->last_scr_height);
#endif /* VBOX */
}

#define GMODE_TEXT     0
#define GMODE_GRAPH    1
#define GMODE_BLANK 2

#ifndef VBOX
void vga_update_display(void)
{
    VGAState *s = vga_state;
#else /* VBOX */
static int vga_update_display(PVGASTATE s)
{
    int rc = VINF_SUCCESS;
#endif /* VBOX */
    int full_update, graphic_mode;

#ifndef VBOX
    if (s->ds->depth == 0) {
#else /* VBOX */
    if (s->pDrv->cBits == 0) {
#endif /* VBOX */
        /* nothing to do */
    } else {
#ifndef VBOX
        switch(s->ds->depth) {
#else /* VBOX */
        switch(s->pDrv->cBits) {
#endif /* VBOX */
        case 8:
            s->rgb_to_pixel = rgb_to_pixel8_dup;
            break;
        case 15:
            s->rgb_to_pixel = rgb_to_pixel15_dup;
            break;
        default:
        case 16:
            s->rgb_to_pixel = rgb_to_pixel16_dup;
            break;
        case 32:
            s->rgb_to_pixel = rgb_to_pixel32_dup;
            break;
        }

        full_update = 0;
        if (!(s->ar_index & 0x20)) {
            graphic_mode = GMODE_BLANK;
        } else {
            graphic_mode = s->gr[6] & 1;
        }
        if (graphic_mode != s->graphic_mode) {
            s->graphic_mode = graphic_mode;
            full_update = 1;
        }
        switch(graphic_mode) {
        case GMODE_TEXT:
#ifdef VBOX
            rc =
#endif /* VBOX */
            vga_draw_text(s, full_update);
            break;
        case GMODE_GRAPH:
#ifdef VBOX
            rc =
#endif /* VBOX */
            vga_draw_graphic(s, full_update);
            break;
        case GMODE_BLANK:
        default:
            vga_draw_blank(s, full_update);
            break;
        }
    }
#ifdef VBOX
    return rc;
#endif /* VBOX */
}

/* force a full display refresh */
#ifndef VBOX
void vga_invalidate_display(void)
{
    VGAState *s = vga_state;

    s->last_width = -1;
    s->last_height = -1;
}
#endif /* !VBOX */

#ifndef VBOX /* see vgaR3Reset() */
static void vga_reset(VGAState *s)
{
    memset(s, 0, sizeof(VGAState));
    s->graphic_mode = -1; /* force full update */
}
#endif /* !VBOX */

#ifndef VBOX
static CPUReadMemoryFunc *vga_mem_read[3] = {
    vga_mem_readb,
    vga_mem_readw,
    vga_mem_readl,
};

static CPUWriteMemoryFunc *vga_mem_write[3] = {
    vga_mem_writeb,
    vga_mem_writew,
    vga_mem_writel,
};
#endif /* !VBOX */

static void vga_save(QEMUFile *f, void *opaque)
{
    VGAState *s = (VGAState*)opaque;
    int i;

    qemu_put_be32s(f, &s->latch);
    qemu_put_8s(f, &s->sr_index);
    qemu_put_buffer(f, s->sr, 8);
    qemu_put_8s(f, &s->gr_index);
    qemu_put_buffer(f, s->gr, 16);
    qemu_put_8s(f, &s->ar_index);
    qemu_put_buffer(f, s->ar, 21);
    qemu_put_be32s(f, &s->ar_flip_flop);
    qemu_put_8s(f, &s->cr_index);
    qemu_put_buffer(f, s->cr, 256);
    qemu_put_8s(f, &s->msr);
    qemu_put_8s(f, &s->fcr);
    qemu_put_8s(f, &s->st00);
    qemu_put_8s(f, &s->st01);

    qemu_put_8s(f, &s->dac_state);
    qemu_put_8s(f, &s->dac_sub_index);
    qemu_put_8s(f, &s->dac_read_index);
    qemu_put_8s(f, &s->dac_write_index);
    qemu_put_buffer(f, s->dac_cache, 3);
    qemu_put_buffer(f, s->palette, 768);

    qemu_put_be32s(f, &s->bank_offset);
#ifdef CONFIG_BOCHS_VBE
    qemu_put_byte(f, 1);
    qemu_put_be16s(f, &s->vbe_index);
    for(i = 0; i < VBE_DISPI_INDEX_NB; i++)
        qemu_put_be16s(f, &s->vbe_regs[i]);
    qemu_put_be32s(f, &s->vbe_start_addr);
    qemu_put_be32s(f, &s->vbe_line_offset);
    qemu_put_be32s(f, &s->vbe_bank_mask);
#else
    qemu_put_byte(f, 0);
#endif
}

static int vga_load(QEMUFile *f, void *opaque, int version_id)
{
    VGAState *s = (VGAState*)opaque;
    int is_vbe, i;

    if (version_id != 1)
#ifndef VBOX
        return -EINVAL;
#else /* VBOX */
    {
        Log(("vga_load: version_id=%d - UNKNOWN\n", version_id));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }
#endif /* VBOX */

    qemu_get_be32s(f, &s->latch);
    qemu_get_8s(f, &s->sr_index);
    qemu_get_buffer(f, s->sr, 8);
    qemu_get_8s(f, &s->gr_index);
    qemu_get_buffer(f, s->gr, 16);
    qemu_get_8s(f, &s->ar_index);
    qemu_get_buffer(f, s->ar, 21);
    qemu_get_be32s(f, (uint32_t *)&s->ar_flip_flop);
    qemu_get_8s(f, &s->cr_index);
    qemu_get_buffer(f, s->cr, 256);
    qemu_get_8s(f, &s->msr);
    qemu_get_8s(f, &s->fcr);
    qemu_get_8s(f, &s->st00);
    qemu_get_8s(f, &s->st01);

    qemu_get_8s(f, &s->dac_state);
    qemu_get_8s(f, &s->dac_sub_index);
    qemu_get_8s(f, &s->dac_read_index);
    qemu_get_8s(f, &s->dac_write_index);
    qemu_get_buffer(f, s->dac_cache, 3);
    qemu_get_buffer(f, s->palette, 768);

    qemu_get_be32s(f, (uint32_t *)&s->bank_offset);
    is_vbe = qemu_get_byte(f);
#ifdef CONFIG_BOCHS_VBE
    if (!is_vbe)
#ifndef VBOX
        return -EINVAL;
#else /* VBOX */
    {
        Log(("vga_load: !is_vbe !!\n"));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }
#endif /* VBOX */
    qemu_get_be16s(f, &s->vbe_index);
    for(i = 0; i < VBE_DISPI_INDEX_NB; i++)
        qemu_get_be16s(f, &s->vbe_regs[i]);
    qemu_get_be32s(f, &s->vbe_start_addr);
    qemu_get_be32s(f, &s->vbe_line_offset);
    qemu_get_be32s(f, &s->vbe_bank_mask);
#else
    if (is_vbe)
#ifndef VBOX
        return -EINVAL;
#else /* VBOX */
    {
        Log(("vga_load: is_vbe !!\n"));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }
#endif /* VBOX */
#endif

    /* force refresh */
    s->graphic_mode = -1;
    return 0;
}

#ifndef VBOX /* see vgaR3IORegionMap */
static void vga_map(PCIDevice *pci_dev, int region_num,
                    uint32_t addr, uint32_t size, int type)
{
    VGAState *s = vga_state;

    cpu_register_physical_memory(addr, s->vram_size, s->vram_offset);
}
#endif

#ifndef VBOX /* see vgaR3Construct */
void vga_common_init(VGAState *s, DisplayState *ds, uint8_t *vga_ram_base,
                     unsigned long vga_ram_offset, int vga_ram_size)
#else
static void vga_init_expand(void)
#endif
{
    int i, j, v, b;

    for(i = 0;i < 256; i++) {
        v = 0;
        for(j = 0; j < 8; j++) {
            v |= ((i >> j) & 1) << (j * 4);
        }
        expand4[i] = v;

        v = 0;
        for(j = 0; j < 4; j++) {
            v |= ((i >> (2 * j)) & 3) << (j * 4);
        }
        expand2[i] = v;
    }
    for(i = 0; i < 16; i++) {
        v = 0;
        for(j = 0; j < 4; j++) {
            b = ((i >> j) & 1);
            v |= b << (2 * j);
            v |= b << (2 * j + 1);
        }
        expand4to8[i] = v;
    }
#ifdef VBOX
}
#else /* !VBOX */
    vga_reset(s);

    s->vram_ptr = vga_ram_base;
    s->vram_offset = vga_ram_offset;
    s->vram_size = vga_ram_size;
    s->ds = ds;
    s->get_bpp = vga_get_bpp;
    s->get_offsets = vga_get_offsets;
    s->get_resolution = vga_get_resolution;
    /* XXX: currently needed for display */
    vga_state = s;
}


int vga_initialize(PCIBus *bus, DisplayState *ds, uint8_t *vga_ram_base,
                   unsigned long vga_ram_offset, int vga_ram_size)
{
    VGAState *s;

    s = qemu_mallocz(sizeof(VGAState));
    if (!s)
        return -1;

    vga_common_init(s, ds, vga_ram_base, vga_ram_offset, vga_ram_size);

    register_savevm("vga", 0, 1, vga_save, vga_load, s);

    register_ioport_write(0x3c0, 16, 1, vga_ioport_write, s);

    register_ioport_write(0x3b4, 2, 1, vga_ioport_write, s);
    register_ioport_write(0x3d4, 2, 1, vga_ioport_write, s);
    register_ioport_write(0x3ba, 1, 1, vga_ioport_write, s);
    register_ioport_write(0x3da, 1, 1, vga_ioport_write, s);

    register_ioport_read(0x3c0, 16, 1, vga_ioport_read, s);

    register_ioport_read(0x3b4, 2, 1, vga_ioport_read, s);
    register_ioport_read(0x3d4, 2, 1, vga_ioport_read, s);
    register_ioport_read(0x3ba, 1, 1, vga_ioport_read, s);
    register_ioport_read(0x3da, 1, 1, vga_ioport_read, s);
    s->bank_offset = 0;

#ifdef CONFIG_BOCHS_VBE
    s->vbe_regs[VBE_DISPI_INDEX_ID] = VBE_DISPI_ID0;
    s->vbe_bank_mask = ((s->vram_size >> 16) - 1);
#if defined (TARGET_I386)
    register_ioport_read(0x1ce, 1, 2, vbe_ioport_read_index, s);
    register_ioport_read(0x1cf, 1, 2, vbe_ioport_read_data, s);

    register_ioport_write(0x1ce, 1, 2, vbe_ioport_write_index, s);
    register_ioport_write(0x1cf, 1, 2, vbe_ioport_write_data, s);

    /* old Bochs IO ports */
    register_ioport_read(0xff80, 1, 2, vbe_ioport_read_index, s);
    register_ioport_read(0xff81, 1, 2, vbe_ioport_read_data, s);

    register_ioport_write(0xff80, 1, 2, vbe_ioport_write_index, s);
    register_ioport_write(0xff81, 1, 2, vbe_ioport_write_data, s);
#else
    register_ioport_read(0x1ce, 1, 2, vbe_ioport_read_index, s);
    register_ioport_read(0x1d0, 1, 2, vbe_ioport_read_data, s);

    register_ioport_write(0x1ce, 1, 2, vbe_ioport_write_index, s);
    register_ioport_write(0x1d0, 1, 2, vbe_ioport_write_data, s);
#endif
#endif /* CONFIG_BOCHS_VBE */

    vga_io_memory = cpu_register_io_memory(0, vga_mem_read, vga_mem_write, s);
    cpu_register_physical_memory(isa_mem_base + 0x000a0000, 0x20000,
                                 vga_io_memory);

    if (bus) {
        PCIDevice *d;
        uint8_t *pci_conf;

        d = pci_register_device(bus, "VGA",
                                sizeof(PCIDevice),
                                -1, NULL, NULL);
        pci_conf = d->config;
        pci_conf[0x00] = 0x34; // dummy VGA (same as Bochs ID)
        pci_conf[0x01] = 0x12;
        pci_conf[0x02] = 0x11;
        pci_conf[0x03] = 0x11;
        pci_conf[0x0a] = 0x00; // VGA controller
        pci_conf[0x0b] = 0x03;
        pci_conf[0x0e] = 0x00; // header_type

        /* XXX: vga_ram_size must be a power of two */
        pci_register_io_region(d, 0, vga_ram_size,
                               PCI_ADDRESS_SPACE_MEM_PREFETCH, vga_map);
    } else {
#ifdef CONFIG_BOCHS_VBE
        /* XXX: use optimized standard vga accesses */
        cpu_register_physical_memory(VBE_DISPI_LFB_PHYSICAL_ADDRESS,
                                     vga_ram_size, vga_ram_offset);
#endif
    }
    return 0;
}
#endif /* !VBOX */


#ifndef VBOX
/********************************************************/
/* vga screen dump */

static int vga_save_w, vga_save_h;

static void vga_save_dpy_update(DisplayState *s,
                                int x, int y, int w, int h)
{
}

static void vga_save_dpy_resize(DisplayState *s, int w, int h)
{
    s->linesize = w * 4;
#ifndef VBOX
    s->data = qemu_malloc(h * s->linesize);
#else /* VBOX */
    if (!s->data)
    {
        PPDMDEVINS pDevIns = VGASTATE2DEVINS((PVGASTATE)s->pvVgaState);
        s->data = PDMDevHlpMMHeapAlloc(pDevIns, h * s->linesize);
    }
    else // (32-bpp buffer is allocated by the caller)
        s->linesize = ((w * 32 + 31) / 32) * 4;
#endif /* VBOX */
    vga_save_w = w;
    vga_save_h = h;
}

static void vga_save_dpy_refresh(DisplayState *s)
{
}

static int ppm_save(const char *filename, uint8_t *data,
                    int w, int h, int linesize)
{
    FILE *f;
    uint8_t *d, *d1;
    unsigned int v;
    int y, x;

    f = fopen(filename, "wb");
    if (!f)
        return -1;
    fprintf(f, "P6\n%d %d\n%d\n",
            w, h, 255);
    d1 = data;
    for(y = 0; y < h; y++) {
        d = d1;
        for(x = 0; x < w; x++) {
            v = *(uint32_t *)d;
            fputc((v >> 16) & 0xff, f);
            fputc((v >> 8) & 0xff, f);
            fputc((v) & 0xff, f);
            d += 4;
        }
        d1 += linesize;
    }
    fclose(f);
    return 0;
}

/* save the vga display in a PPM image even if no display is
   available */
void vga_screen_dump(const char *filename)
{
    VGAState *s = vga_state;
    DisplayState *saved_ds, ds1, *ds = &ds1;

    /* XXX: this is a little hackish */
    vga_invalidate_display();
    saved_ds = s->ds;

    memset(ds, 0, sizeof(DisplayState));
    ds->dpy_update = vga_save_dpy_update;
    ds->dpy_resize = vga_save_dpy_resize;
    ds->dpy_refresh = vga_save_dpy_refresh;
    ds->depth = 32;

    s->ds = ds;
    s->graphic_mode = -1;
    vga_update_display();

    if (ds->data) {
        ppm_save(filename, ds->data, vga_save_w, vga_save_h,
                 s->ds->linesize);
        qemu_free(ds->data);
    }
    s->ds = saved_ds;
}
#endif /* !VBOX */


#if 0 //def VBOX
/* copy the vga display contents to the given buffer. the size of the buffer
   must be sufficient to store the screen copy (see below). the width and height
   parameters determine the required dimensions of the copy. If they differ
   from the actual screen dimensions, then the returned copy is shrinked or
   stretched accordingly. The copy is always a 32-bit image, so the size of
   the buffer supplied must be at least (((width * 32 + 31) / 32) * 4) * height,
   i.e. dword-aligned. returns zero if the operation was successfull and -1
   otherwise. */

static int vga_copy_screen_to(PVGASTATE s, uint8_t *buf, int width, int height)
{
    DisplayState *saved_ds, ds1, *ds = &ds1;
    if (!buf || width <= 0 || height <= 0)
        return -1;

    /* XXX: this is a little hackish */
    vga_invalidate_display(s);
    saved_ds = s->ds;

    memset(ds, 0, sizeof(DisplayState));
    ds->dpy_update = vga_save_dpy_update;
    ds->dpy_resize = vga_save_dpy_resize;
    ds->dpy_refresh = vga_save_dpy_refresh;
    ds->depth = 32;
    ds->data = buf;
    ds->pvVgaState = s;

    s->ds = ds;
    s->graphic_mode = -1;
    vga_update_display(s);

//@@TODO (dmik): implement stretching/shrinking!

    s->ds = saved_ds;
    return 0;
}

/* copy the given buffer to the vga display. width and height define the
   dimensions of the image in the buffer. x and y define the point on the
   vga display to copy the image to. the buffer is assumed to contain a 32-bit
   image, so the size of one scanline must be ((width * 32 + 31) / 32) * 4),
   i.e. dword-aligned. returns zero if the operation was successfull and -1
   otherwise. */
static int vga_copy_screen_from(PVGASTATE s, uint8_t *buf, int x, int y, int width, int height)
{
    int                 bpl = ((width * 32 + 31) / 32) * 4;
    int                 linesize = s->ds->linesize;
    uint8_t            *dst;
    uint8_t            *src;
    int                 bpp;
    vga_draw_line_func *vga_draw_line;

    if (!buf || x < 0 || y < 0 || width <= 0 || height <= 0
        || x + width > s->ds->width || y + height > s->ds->height)
        return -1;

    vga_draw_line = vga_draw_line_table[VGA_DRAW_LINE32 * 4 + get_depth_index(s->ds->depth)];
    switch (s->ds->depth) {
        case 8: bpp = 1; break;
        case 15:
        case 16: bpp = 2; break;
        case 32: bpp = 4; break;
        default: return -1;
    }

    dst = s->ds->data + y * linesize + x * bpp;
    src = buf;
    for (y = 0; y < height; y ++)
    {
        vga_draw_line(s, dst, src, width);
        dst += linesize;
        src += bpl;
    }

    return 0;
}
#endif

#endif /* !VBOX || !IN_GC || !IN_RING0 */



#ifdef VBOX /* innotek code start */


/* -=-=-=-=-=- all contexts -=-=-=-=-=- */

/**
 * Port I/O Handler for VGA OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) vgaIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    NOREF(pvUser);
    if (cb == 1)
        vga_ioport_write(PDMINS2DATA(pDevIns, PVGASTATE), Port, u32);
    else if (cb == 2)
    {
        vga_ioport_write(PDMINS2DATA(pDevIns, PVGASTATE), Port, u32 & 0xff);
        vga_ioport_write(PDMINS2DATA(pDevIns, PVGASTATE), Port + 1, u32 >> 8);
    }
    return VINF_SUCCESS;
}


/**
 * Port I/O Handler for VGA IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
PDMBOTHCBDECL(int) vgaIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    NOREF(pvUser);
    if (cb == 1)
    {
        *pu32 = vga_ioport_read(PDMINS2DATA(pDevIns, PVGASTATE), Port);
        return VINF_SUCCESS;
    }
    else if (cb == 2)
    {
        *pu32 = vga_ioport_read(PDMINS2DATA(pDevIns, PVGASTATE), Port)
             | (vga_ioport_read(PDMINS2DATA(pDevIns, PVGASTATE), Port + 1) << 8);
        return VINF_SUCCESS;
    }
    return VERR_IOM_IOPORT_UNUSED;
}


/**
 * Port I/O Handler for VBE OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) vgaIOPortWriteVBEData(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    VGAState *s = PDMINS2DATA(pDevIns, PVGASTATE);

    NOREF(pvUser);

#ifndef IN_RING3
    /*
     * This has to be done on the host in order to execute the connector callbacks.
     */
    if (s->vbe_index == VBE_DISPI_INDEX_ENABLE
        || s->vbe_index == VBE_DISPI_INDEX_VBOX_VIDEO)
    {
        Log(("vgaIOPortWriteVBEData: VBE_DISPI_INDEX_ENABLE - Switching to host...\n"));
        return VINF_IOM_HC_IOPORT_WRITE;
    }
#endif
#ifdef VBE_BYTEWISE_IO
    if (cb == 1)
    {
        if (!s->fWriteVBEData)
        {
            if (    (s->vbe_index == VBE_DISPI_INDEX_ENABLE)
                &&  (u32 & VBE_DISPI_ENABLED))
            {
                s->fWriteVBEData = false;
                return vbe_ioport_write_data(s, Port, u32 & 0xFF);
            }
            else
            {
                s->cbWriteVBEData = u32 & 0xFF;
                s->fWriteVBEData = true;
                return VINF_SUCCESS;
            }
        }
        else
        {
            u32 = (s->cbWriteVBEData << 8) | (u32 & 0xFF);
            s->fWriteVBEData = false;
            cb = 2;
        }
    }
#endif
    if (cb == 2 || cb == 4)
    {
//#ifdef IN_GC
//        /*
//         * The VBE_DISPI_INDEX_ENABLE memsets the entire frame buffer.
//         * Since we're not mapping the entire framebuffer any longer that
//         * has to be done on the host.
//         */
//        if (    (s->vbe_index == VBE_DISPI_INDEX_ENABLE)
//            &&  (u32 & VBE_DISPI_ENABLED))
//        {
//            Log(("vgaIOPortWriteVBEData: VBE_DISPI_INDEX_ENABLE & VBE_DISPI_ENABLED - Switching to host...\n"));
//            return VINF_IOM_HC_IOPORT_WRITE;
//        }
//#endif
        return vbe_ioport_write_data(s, Port, u32);
    }
    else
        AssertMsgFailed(("vgaIOPortWriteVBEData: Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
    return VINF_SUCCESS;
}


/**
 * Port I/O Handler for VBE OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) vgaIOPortWriteVBEIndex(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    NOREF(pvUser);
#ifdef VBE_BYTEWISE_IO
    if (cb == 1)
    {
        VGAState *s = PDMINS2DATA(pDevIns, PVGASTATE);
        if (!s->fWriteVBEIndex)
        {
            s->cbWriteVBEIndex = u32 & 0x00FF;
            s->fWriteVBEIndex = true;
            return VINF_SUCCESS;
        }
        else
        {
            s->fWriteVBEIndex = false;
            vbe_ioport_write_index(s, Port, (s->cbWriteVBEIndex << 8) | (u32 & 0x00FF));
            return VINF_SUCCESS;
        }
    }
    else
#endif
    if (cb == 2)
        vbe_ioport_write_index(PDMINS2DATA(pDevIns, PVGASTATE), Port, u32);
    else
        AssertMsgFailed(("vgaIOPortWriteVBEIndex: Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
    return VINF_SUCCESS;
}


/**
 * Port I/O Handler for VBE IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes to read.
 */
PDMBOTHCBDECL(int) vgaIOPortReadVBEData(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    NOREF(pvUser);
#ifdef VBE_BYTEWISE_IO
    if (cb == 1)
    {
        VGAState *s = PDMINS2DATA(pDevIns, PVGASTATE);

        if (!s->fReadVBEData)
        {
            *pu32 = (vbe_ioport_read_data(s, Port) >> 8) & 0xFF;
            s->fReadVBEData = true;
            return VINF_SUCCESS;
        }
        else
        {
            *pu32 = vbe_ioport_read_data(s, Port) & 0xFF;
            s->fReadVBEData = false;
            return VINF_SUCCESS;
        }
    }
    else
#endif
    if (cb == 2)
    {
        *pu32 = vbe_ioport_read_data(PDMINS2DATA(pDevIns, PVGASTATE), Port);
        return VINF_SUCCESS;
    }
    else if (cb == 4)
    {
        VGAState *s = PDMINS2DATA(pDevIns, PVGASTATE);
        /* Quick hack for getting the vram size. */
        *pu32 = s->vram_size;
        return VINF_SUCCESS;
    }
    AssertMsgFailed(("vgaIOPortReadVBEData: Port=%#x cb=%d\n", Port, cb));
    return VERR_IOM_IOPORT_UNUSED;
}


/**
 * Port I/O Handler for VBE IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes to read.
 */
PDMBOTHCBDECL(int) vgaIOPortReadVBEIndex(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    NOREF(pvUser);
#ifdef VBE_BYTEWISE_IO
    if (cb == 1)
    {
        VGAState *s = PDMINS2DATA(pDevIns, PVGASTATE);

        if (!s->fReadVBEIndex)
        {
            *pu32 = (vbe_ioport_read_index(s, Port) >> 8) & 0xFF;
            s->fReadVBEIndex = true;
            return VINF_SUCCESS;
        }
        else
        {
            *pu32 = vbe_ioport_read_index(s, Port) & 0xFF;
            s->fReadVBEIndex = false;
            return VINF_SUCCESS;
        }
    }
    else
#endif
    if (cb == 2)
    {
        *pu32 = vbe_ioport_read_index(PDMINS2DATA(pDevIns, PVGASTATE), Port);
        return VINF_SUCCESS;
    }
    AssertMsgFailed(("vgaIOPortReadVBEIndex: Port=%#x cb=%d\n", Port, cb));
    return VERR_IOM_IOPORT_UNUSED;
}





/* -=-=-=-=-=- Guest Context -=-=-=-=-=- */

/*
 * Internal. For use inside VGAGCMemoryFillWrite only.
 * Macro for apply logical operation and bit mask.
 */
#define APPLY_LOGICAL_AND_MASK(s, val, bit_mask) \
    /* apply logical operation */                \
    switch(s->gr[3] >> 3)                        \
    {                                            \
        case 0:                                  \
        default:                                 \
            /* nothing to do */                  \
            break;                               \
        case 1:                                  \
            /* and */                            \
            val &= s->latch;                     \
            break;                               \
        case 2:                                  \
            /* or */                             \
            val |= s->latch;                     \
            break;                               \
        case 3:                                  \
            /* xor */                            \
            val ^= s->latch;                     \
            break;                               \
    }                                            \
    /* apply bit mask */                         \
    val = (val & bit_mask) | (s->latch & ~bit_mask)

/**
 * Legacy VGA memory (0xa0000 - 0xbffff) write hook, to be called from IOM and from the inside of VGADeviceGC.cpp.
 * This is the advanced version of vga_mem_writeb function.
 *
 * @returns VBox status code.
 * @param   pDevIns     Pointer device instance.
 * @param   pvUser      User argument - ignored.
 * @param   GCPhysAddr  Physical address of memory to write.
 * @param   u32Item     Data to write, up to 4 bytes.
 * @param   cbItem      Size of data Item, only 1/2/4 bytes is allowed for now.
 * @param   cItems      Number of data items to write.
 */
PDMBOTHCBDECL(int) vgaMMIOFill(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, uint32_t u32Item, unsigned cbItem, unsigned cItems)
{
    PVGASTATE pData = PDMINS2DATA(pDevIns, PVGASTATE);
    uint32_t b;
    uint32_t write_mask, bit_mask, set_mask;
    uint32_t aVal[4]; /** @todo r=bird: Why is this an 32-bit array? */
    unsigned i;
    NOREF(pvUser);
    for (i = 0; i < cbItem; i++)
    {
        aVal[i] = u32Item & 0xff;
        u32Item >>= 8;
    }

    /* convert to VGA memory offset */
    /// @todo add check for the end of region
    GCPhysAddr &= 0x1ffff;
    switch((pData->gr[6] >> 2) & 3) {
    case 0:
        break;
    case 1:
        if (GCPhysAddr >= 0x10000)
            return VINF_SUCCESS;
        GCPhysAddr += pData->bank_offset;
        break;
    case 2:
        GCPhysAddr -= 0x10000;
        if (GCPhysAddr >= 0x8000)
            return VINF_SUCCESS;
        break;
    default:
    case 3:
        GCPhysAddr -= 0x18000;
        if (GCPhysAddr >= 0x8000)
            return VINF_SUCCESS;
        break;
    }

    if (pData->sr[4] & 0x08) {
        /* chain 4 mode : simplest access */
#ifdef IN_GC
        if (GCPhysAddr + cItems * cbItem >= VGA_MAPPING_SIZE)
            return VINF_IOM_HC_MMIO_WRITE;
#else
        if (GCPhysAddr + cItems * cbItem >= pData->vram_size)
        {
            AssertMsgFailed(("GCPhysAddr=%VGp cItems=%#x cbItem=%d\n", GCPhysAddr, cItems, cbItem));
            return VINF_SUCCESS;
        }
#endif

        while (cItems-- > 0)
            for (i = 0; i < cbItem; i++)
            {
                if (pData->sr[2] & (1 << (GCPhysAddr & 3)))
                {
                    CTXSUFF(pData->vram_ptr)[GCPhysAddr] = aVal[i];
                    vga_set_dirty(pData, GCPhysAddr);
                }
                GCPhysAddr++;
            }
    } else if (pData->gr[5] & 0x10) {
        /* odd/even mode (aka text mode mapping) */
#ifdef IN_GC
        if (GCPhysAddr * 2 + cItems * cbItem >= VGA_MAPPING_SIZE)
            return VINF_IOM_HC_MMIO_WRITE;
#else
        if (GCPhysAddr * 2 + cItems * cbItem >= pData->vram_size)
        {
            AssertMsgFailed(("GCPhysAddr=%VGp cItems=%#x cbItem=%d\n", GCPhysAddr, cItems, cbItem));
            return VINF_SUCCESS;
        }
#endif
        while (cItems-- > 0)
            for (i = 0; i < cbItem; i++)
            {
                unsigned plane = (pData->gr[4] & 2) | (GCPhysAddr & 1);
                if (pData->sr[2] & (1 << plane)) {
                    RTGCPHYS PhysAddr2 = ((GCPhysAddr & ~1) << 2) | plane;
                    CTXSUFF(pData->vram_ptr)[PhysAddr2] = aVal[i];
                    vga_set_dirty(pData, PhysAddr2);
                }
                GCPhysAddr++;
            }
    } else {
#ifdef IN_GC
        if (GCPhysAddr + cItems * cbItem >= VGA_MAPPING_SIZE)
            return VINF_IOM_HC_MMIO_WRITE;
#else
        if (GCPhysAddr + cItems * cbItem >= pData->vram_size)
        {
            AssertMsgFailed(("GCPhysAddr=%VGp cItems=%#x cbItem=%d\n", GCPhysAddr, cItems, cbItem));
            return VINF_SUCCESS;
        }
#endif

        /* standard VGA latched access */
        switch(pData->gr[5] & 3) {
        default:
        case 0:
            /* rotate */
            b = pData->gr[3] & 7;
            bit_mask = pData->gr[8];
            bit_mask |= bit_mask << 8;
            bit_mask |= bit_mask << 16;
            set_mask = mask16[pData->gr[1]];

            for (i = 0; i < cbItem; i++)
            {
                aVal[i] = ((aVal[i] >> b) | (aVal[i] << (8 - b))) & 0xff;
                aVal[i] |= aVal[i] << 8;
                aVal[i] |= aVal[i] << 16;

                /* apply set/reset mask */
                aVal[i] = (aVal[i] & ~set_mask) | (mask16[pData->gr[0]] & set_mask);

                APPLY_LOGICAL_AND_MASK(pData, aVal[i], bit_mask);
            }
            break;
        case 1:
            for (i = 0; i < cbItem; i++)
                aVal[i] = pData->latch;
            break;
        case 2:
            bit_mask = pData->gr[8];
            bit_mask |= bit_mask << 8;
            bit_mask |= bit_mask << 16;
            for (i = 0; i < cbItem; i++)
            {
                aVal[i] = mask16[aVal[i] & 0x0f];

                APPLY_LOGICAL_AND_MASK(pData, aVal[i], bit_mask);
            }
            break;
        case 3:
            /* rotate */
            b = pData->gr[3] & 7;

            for (i = 0; i < cbItem; i++)
            {
                aVal[i] = (aVal[i] >> b) | (aVal[i] << (8 - b));
                bit_mask = pData->gr[8] & aVal[i];
                bit_mask |= bit_mask << 8;
                bit_mask |= bit_mask << 16;
                aVal[i] = mask16[pData->gr[0]];

                APPLY_LOGICAL_AND_MASK(pData, aVal[i], bit_mask);
            }
            break;
        }

        /* mask data according to sr[2] */
        write_mask = mask16[pData->sr[2]];

        /* actually write data */
        if (cbItem == 1)
        {
            /* The most frequently case is 1 byte I/O. */
            while (cItems-- > 0)
            {
                ((uint32_t *)pData->CTXSUFF(vram_ptr))[GCPhysAddr] = (((uint32_t *)pData->CTXSUFF(vram_ptr))[GCPhysAddr] & ~write_mask) | (aVal[0] & write_mask);
                vga_set_dirty(pData, GCPhysAddr << 2);
                GCPhysAddr++;
            }
        }
        else if (cbItem == 2)
        {
            /* The second case is 2 bytes I/O. */
            while (cItems-- > 0)
            {
                ((uint32_t *)pData->CTXSUFF(vram_ptr))[GCPhysAddr] = (((uint32_t *)pData->CTXSUFF(vram_ptr))[GCPhysAddr] & ~write_mask) | (aVal[0] & write_mask);
                vga_set_dirty(pData, GCPhysAddr << 2);
                GCPhysAddr++;

                ((uint32_t *)pData->CTXSUFF(vram_ptr))[GCPhysAddr] = (((uint32_t *)pData->CTXSUFF(vram_ptr))[GCPhysAddr] & ~write_mask) | (aVal[1] & write_mask);
                vga_set_dirty(pData, GCPhysAddr << 2);
                GCPhysAddr++;
            }
        }
        else
        {
            /* And the rest is 4 bytes. */
            Assert(cbItem == 4);
            while (cItems-- > 0)
                for (i = 0; i < cbItem; i++)
                {
                    ((uint32_t *)pData->CTXSUFF(vram_ptr))[GCPhysAddr] = (((uint32_t *)pData->CTXSUFF(vram_ptr))[GCPhysAddr] & ~write_mask) | (aVal[i] & write_mask);
                    vga_set_dirty(pData, GCPhysAddr << 2);
                    GCPhysAddr++;
                }
        }
    }
    return VINF_SUCCESS;
}
#undef APPLY_LOGICAL_AND_MASK


/**
 * Legacy VGA memory (0xa0000 - 0xbffff) read hook, to be called from IOM.
 *
 * @returns VBox status code.
 * @param   pDevIns     Pointer device instance.
 * @param   pvUser      User argument - ignored.
 * @param   GCPhysAddr  Physical address of memory to read.
 * @param   pv          Where to store readed data.
 * @param   cb          Bytes to read.
 */
PDMBOTHCBDECL(int) vgaMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    PVGASTATE pData = PDMINS2DATA(pDevIns, PVGASTATE);
    STAM_PROFILE_START(&pData->StatGCMemoryRead, a);
    NOREF(pvUser);
    switch (cb)
    {
        case 1:
            *(uint8_t  *)pv = vga_mem_readb(pData, GCPhysAddr); break;
        case 2:
            *(uint16_t *)pv = vga_mem_readb(pData, GCPhysAddr)
                           | (vga_mem_readb(pData, GCPhysAddr + 1) << 8);
            break;
        case 4:
            *(uint32_t *)pv = vga_mem_readb(pData, GCPhysAddr)
                           | (vga_mem_readb(pData, GCPhysAddr + 1) <<  8)
                           | (vga_mem_readb(pData, GCPhysAddr + 2) << 16)
                           | (vga_mem_readb(pData, GCPhysAddr + 3) << 24);
            break;

        default:
        {
            uint8_t *pu8Data = (uint8_t *)pv;
            while (cb-- > 0)
                *pu8Data++ = vga_mem_readb(pData, GCPhysAddr++);
        }
    }
    STAM_PROFILE_STOP(&pData->StatGCMemoryRead, a);
    return VINF_SUCCESS;
}

/**
 * Legacy VGA memory (0xa0000 - 0xbffff) write hook, to be called from IOM.
 *
 * @returns VBox status code.
 * @param   pDevIns     Pointer device instance.
 * @param   pvUser      User argument - ignored.
 * @param   GCPhysAddr  Physical address of memory to write.
 * @param   pv          Pointer to data.
 * @param   cb          Bytes to write.
 */
PDMBOTHCBDECL(int) vgaMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    PVGASTATE pData = PDMINS2DATA(pDevIns, PVGASTATE);
    uint8_t  *pu8 = (uint8_t *)pv;
    int rc = VINF_SUCCESS;
    STAM_PROFILE_START(&pData->StatGCMemoryWrite, a);

    switch (cb)
    {
        case 1:
            rc = vga_mem_writeb(pData, GCPhysAddr, *pu8);
            break;
#if 1
        case 2:
            rc = vga_mem_writeb(pData, GCPhysAddr + 0, pu8[0]);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = vga_mem_writeb(pData, GCPhysAddr + 1, pu8[1]);
            break;
        case 4:
            rc = vga_mem_writeb(pData, GCPhysAddr + 0, pu8[0]);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = vga_mem_writeb(pData, GCPhysAddr + 1, pu8[1]);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = vga_mem_writeb(pData, GCPhysAddr + 2, pu8[2]);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = vga_mem_writeb(pData, GCPhysAddr + 3, pu8[3]);
            break;
#else
        case 2:
            rc = vgaMMIOFill(pDevIns, GCPhysAddr, *(uint16_t *)pv, 2, 1);
            break;
        case 4:
            rc = vgaMMIOFill(pDevIns, GCPhysAddr, *(uint32_t *)pv, 4, 1);
            break;
#endif
        default:
            while (cb-- > 0 && rc == VINF_SUCCESS)
                rc = vga_mem_writeb(pData, GCPhysAddr++, *pu8++);
            break;

    }
    STAM_PROFILE_STOP(&pData->StatGCMemoryWrite, a);
    return rc;
}


/**
 * Handle LFB access.
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   pData       VGA device instance data.
 * @param   GCPhys      The access physical address.
 * @param   GCPtr       The access virtual address (only GC).
 */
static int vgaLFBAccess(PVM pVM, PVGASTATE pData, RTGCPHYS GCPhys, RTGCPTR GCPtr)
{
    int rc;

    /*
     * Set page dirty bit.
     */
    vga_set_dirty(pData, GCPhys - pData->GCPhysVRAM);
    pData->fLFBUpdated = true;

    /*
     * Turn of the write handler for this particular page and make it R/W.
     * Then return telling the caller to restart the guest instruction.
     * ASSUME: the guest always maps video memory RW.
     */
    rc = PGMHandlerPhysicalPageTempOff(pVM, pData->GCPhysVRAM, GCPhys);
    if (VBOX_SUCCESS(rc))
    {
#ifndef IN_RING3
        rc = PGMShwModifyPage(pVM, GCPtr, 1, X86_PTE_RW, ~(uint64_t)X86_PTE_RW);
        if (VBOX_SUCCESS(rc))
            return VINF_SUCCESS;
        else
            AssertMsgFailed(("PGMShwModifyPage -> rc=%d\n", rc));
#else /* IN_RING3 : We don't have any virtual page address of the access here. */
        Assert(GCPtr == 0);
        return VINF_SUCCESS;
#endif
    }
    else
        AssertMsgFailed(("PGMHandlerPhysicalPageTempOff -> rc=%d\n", rc));

    return rc;
}


#ifdef IN_GC
/**
 * #PF Handler for VBE LFB access.
 *
 * @returns VBox status code (appropriate for GC return).
 * @param   pVM         VM Handle.
 * @param   uErrorCode  CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address (cr2).
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 * @param   pvUser      User argument, ignored.
 */
PDMBOTHCBDECL(int) vgaGCLFBAccessHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPHYS GCPhysFault, void *pvUser)
{
    PVGASTATE   pData = (PVGASTATE)pvUser;
    Assert(pData);
    Assert(GCPhysFault >= pData->GCPhysVRAM);
    AssertMsg(uErrorCode & X86_TRAP_PF_RW, ("uErrorCode=%#x\n", uErrorCode));

    return vgaLFBAccess(pVM, pData, GCPhysFault, pvFault);
}

#elif IN_RING0

/**
 * #PF Handler for VBE LFB access.
 *
 * @returns VBox status code (appropriate for GC return).
 * @param   pVM         VM Handle.
 * @param   uErrorCode  CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address (cr2).
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 * @param   pvUser      User argument, ignored.
 */
PDMBOTHCBDECL(int) vgaR0LFBAccessHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPHYS GCPhysFault, void *pvUser)
{
    PVGASTATE   pData = (PVGASTATE)pvUser;
    Assert(pData);
    Assert(GCPhysFault >= pData->GCPhysVRAM);
    AssertMsg(uErrorCode & X86_TRAP_PF_RW, ("uErrorCode=%#x\n", uErrorCode));

    return vgaLFBAccess(pVM, pData, GCPhysFault, pvFault);
}

#else /* IN_RING3 */

/**
 * HC access handler for the LFB.
 *
 * @returns VINF_SUCCESS if the handler have carried out the operation.
 * @returns VINF_PGM_HANDLER_DO_DEFAULT if the caller should carry out the access operation.
 * @param   pVM             VM Handle.
 * @param   GCPhys          The physical address the guest is writing to.
 * @param   pvPhys          The HC mapping of that address.
 * @param   pvBuf           What the guest is reading/writing.
 * @param   cbBuf           How much it's reading/writing.
 * @param   enmAccessType   The access type.
 * @param   pvUser          User argument.
 */
static DECLCALLBACK(int) vgaR3LFBAccessHandler(PVM pVM, RTGCPHYS GCPhys, void *pvPhys, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser)
{
    PVGASTATE   pData = (PVGASTATE)pvUser;
    int         rc;
    Assert(pData);
    Assert(GCPhys >= pData->GCPhysVRAM);
    rc = vgaLFBAccess(pVM, pData, GCPhys, 0);
    if (VBOX_SUCCESS(rc))
        return VINF_PGM_HANDLER_DO_DEFAULT;
    AssertMsg(rc <= VINF_SUCCESS, ("rc=%Vrc\n", rc));
    return rc;
}
#endif /* IN_RING3 */


/* -=-=-=-=-=- Ring 3 -=-=-=-=-=- */

#ifdef IN_RING3

# ifdef VBE_NEW_DYN_LIST
/**
 * Port I/O Handler for VBE Extra OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) vbeIOPortWriteVBEExtra(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    PVGASTATE pData = PDMINS2DATA(pDevIns, PVGASTATE);
    NOREF(pvUser);
    NOREF(Port);

    if (cb == 2)
    {
        Log(("vbeIOPortWriteVBEExtra: addr=%#RX32\n", u32));
        pData->u16VBEExtraAddress = u32;
        return VINF_SUCCESS;
    }

    Log(("vbeIOPortWriteVBEExtra: Ignoring invalid cb=%d writes to the VBE Extra port!!!\n", cb));
    return VINF_SUCCESS;
}


/**
 * Port I/O Handler for VBE Extra IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
PDMBOTHCBDECL(int) vbeIOPortReadVBEExtra(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    PVGASTATE pData = PDMINS2DATA(pDevIns, PVGASTATE);
    NOREF(pvUser);
    NOREF(Port);

    if (pData->u16VBEExtraAddress == 0xffff)
    {
        Log(("vbeIOPortReadVBEExtra: Requested number of 64k video banks\n"));
        *pu32 = pData->vram_size / _64K;
        return VINF_SUCCESS;
    }

    if (    pData->u16VBEExtraAddress >= pData->cbVBEExtraData
        ||  pData->u16VBEExtraAddress + cb > pData->cbVBEExtraData)
    {
        *pu32 = 0;
        Log(("vbeIOPortReadVBEExtra: Requested address is out of VBE data!!! Address=%#x(%d) cbVBEExtraData=%#x(%d)\n",
             pData->u16VBEExtraAddress, pData->u16VBEExtraAddress, pData->cbVBEExtraData, pData->cbVBEExtraData));
        return VINF_SUCCESS;
    }

    if (cb == 1)
    {
        *pu32 = pData->pu8VBEExtraData[pData->u16VBEExtraAddress] & 0xFF;

        Log(("vbeIOPortReadVBEExtra: cb=%#x %.*Vhxs\n", cb, cb, pu32));
        return VINF_SUCCESS;
    }

    if (cb == 2)
    {
        *pu32 = pData->pu8VBEExtraData[pData->u16VBEExtraAddress]
              | pData->pu8VBEExtraData[pData->u16VBEExtraAddress + 1] << 8;

        Log(("vbeIOPortReadVBEExtra: cb=%#x %.*Vhxs\n", cb, cb, pu32));
        return VINF_SUCCESS;
    }
    Log(("vbeIOPortReadVBEExtra: Invalid cb=%d read from the VBE Extra port!!!\n", cb));
    return VERR_IOM_IOPORT_UNUSED;
}
# endif /* VBE_NEW_DYN_LIST */




/* -=-=-=-=-=- Ring 3: VGA BIOS I/Os -=-=-=-=-=- */

/**
 * Port I/O Handler for VGA BIOS IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
static DECLCALLBACK(int) vgaIOPortReadBIOS(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    NOREF(pDevIns);
    NOREF(pvUser);
    NOREF(Port);
    NOREF(pu32);
    NOREF(cb);
    return VERR_IOM_IOPORT_UNUSED;
}

/**
 * Port I/O Handler for VGA BIOS OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
static DECLCALLBACK(int) vgaIOPortWriteBIOS(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    static int lastWasNotNewline = 0;  /* We are only called in a single-threaded way */
    /*
     * VGA BIOS char printing.
     */
    if (    cb == 1
        &&  Port == VBE_PRINTF_PORT)
    {
#if 0
        switch (u32)
        {
            case '\r': Log(("vgabios: <return>\n")); break;
            case '\n': Log(("vgabios: <newline>\n")); break;
            case '\t': Log(("vgabios: <tab>\n")); break;
            default:
                Log(("vgabios: %c\n", u32));
        }
#else
        if (lastWasNotNewline == 0)
            Log(("vgabios: "));
        if (u32 != '\r')  /* return - is only sent in conjunction with '\n' */
            Log(("%c", u32));
        if (u32 == '\n')
            lastWasNotNewline = 0;
        else
            lastWasNotNewline = 1;
#endif
        return VINF_SUCCESS;
    }

    /* not in use. */
    return VINF_SUCCESS;
}


/* -=-=-=-=-=- Ring 3: IBase -=-=-=-=-=- */

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 * @thread  Any thread.
 */
static DECLCALLBACK(void *) vgaPortQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PVGASTATE pData = (PVGASTATE)((uintptr_t)pInterface - RT_OFFSETOF(VGASTATE, Base));
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pData->Base;
        case PDMINTERFACE_DISPLAY_PORT:
            return &pData->Port;
        default:
            return NULL;
    }
}


/* -=-=-=-=-=- Ring 3: Dummy IDisplayConnector -=-=-=-=-=- */

/**
 * Resize the display.
 * This is called when the resolution changes. This usually happens on
 * request from the guest os, but may also happen as the result of a reset.
 *
 * @param   pInterface          Pointer to this interface.
 * @param   cx                  New display width.
 * @param   cy                  New display height
 * @thread  The emulation thread.
 */
static DECLCALLBACK(int) vgaDummyResize(PPDMIDISPLAYCONNECTOR pInterface, uint32_t bpp, void *pvVRAM, uint32_t cbLine, uint32_t cx, uint32_t cy)
{
    return VINF_SUCCESS;
}


/**
 * Update a rectangle of the display.
 * PDMIDISPLAYPORT::pfnUpdateDisplay is the caller.
 *
 * @param   pInterface          Pointer to this interface.
 * @param   x                   The upper left corner x coordinate of the rectangle.
 * @param   y                   The upper left corner y coordinate of the rectangle.
 * @param   cx                  The width of the rectangle.
 * @param   cy                  The height of the rectangle.
 * @thread  The emulation thread.
 */
static DECLCALLBACK(void) vgaDummyUpdateRect(PPDMIDISPLAYCONNECTOR pInterface, uint32_t x, uint32_t y, uint32_t cx, uint32_t cy)
{
}


/**
 * Refresh the display.
 *
 * The interval between these calls is set by
 * PDMIDISPLAYPORT::pfnSetRefreshRate(). The driver should call
 * PDMIDISPLAYPORT::pfnUpdateDisplay() if it wishes to refresh the
 * display. PDMIDISPLAYPORT::pfnUpdateDisplay calls pfnUpdateRect with
 * the changed rectangles.
 *
 * @param   pInterface          Pointer to this interface.
 * @thread  The emulation thread.
 */
static DECLCALLBACK(void) vgaDummyRefresh(PPDMIDISPLAYCONNECTOR pInterface)
{
}


/* -=-=-=-=-=- Ring 3: IDisplayPort -=-=-=-=-=- */

/** Converts a display port interface pointer to a vga state pointer. */
#define IDISPLAYPORT_2_VGASTATE(pInterface) ( (PVGASTATE)((uintptr_t)pInterface - RT_OFFSETOF(VGASTATE, Port)) )


/**
 * Update the display with any changed regions.
 *
 * @param   pInterface          Pointer to this interface.
 * @see     PDMIKEYBOARDPORT::pfnUpdateDisplay() for details.
 */
static DECLCALLBACK(int) vgaPortUpdateDisplay(PPDMIDISPLAYPORT pInterface)
{
    PVGASTATE pData = IDISPLAYPORT_2_VGASTATE(pInterface);
    PDMDEV_ASSERT_EMT(VGASTATE2DEVINS(pData));

#ifdef DEBUG_sunlover
    LogFlow(("vgaPortUpdateDisplay\n"));
#endif /* DEBUG_sunlover */

    /* This should be called only in non VBVA mode. */

    int rc = vga_update_display(pData);
    if (rc != VINF_SUCCESS)
        return rc;

    if (pData->fHaveDirtyBits)
    {
        PPDMDEVINS pDevIns = pData->pDevInsHC;
        PGMHandlerPhysicalReset(PDMDevHlpGetVM(pDevIns), pData->GCPhysVRAM);
        pData->fHaveDirtyBits = false;
    }

    return VINF_SUCCESS;
}


/**
 * Update the entire display.
 *
 * @param   pInterface          Pointer to this interface.
 * @see     PDMIKEYBOARDPORT::pfnUpdateDisplayAll() for details.
 */
static DECLCALLBACK(int) vgaPortUpdateDisplayAll(PPDMIDISPLAYPORT pInterface)
{
    PVGASTATE pData = IDISPLAYPORT_2_VGASTATE(pInterface);
    PDMDEV_ASSERT_EMT(VGASTATE2DEVINS(pData));

    /* This is called both in VBVA mode and normal modes. */

#ifdef DEBUG_sunlover
    LogFlow(("vgaPortUpdateDisplayAll\n"));
#endif /* DEBUG_sunlover */

    pData->graphic_mode = -1; /* force full update */
    return vga_update_display(pData);
}


/**
 * Sets the refresh rate and restart the timer.
 *
 * @returns VBox status code.
 * @param   pInterface          Pointer to this interface.
 * @param   cMilliesInterval    Number of millies between two refreshes.
 * @see     PDMIKEYBOARDPORT::pfnSetRefreshRate() for details.
 */
static DECLCALLBACK(int) vgaPortSetRefreshRate(PPDMIDISPLAYPORT pInterface, uint32_t cMilliesInterval)
{
    PVGASTATE pData = IDISPLAYPORT_2_VGASTATE(pInterface);

    pData->cMilliesRefreshInterval = cMilliesInterval;
    if (cMilliesInterval)
        return TMTimerSetMillies(pData->RefreshTimer, cMilliesInterval);
    return TMTimerStop(pData->RefreshTimer);
}


/** @copydoc PDMIDISPLAYPORT::pfnQueryColorDepth */
static DECLCALLBACK(int) vgaPortQueryColorDepth(PPDMIDISPLAYPORT pInterface, uint32_t *pcBits)
{
    PVGASTATE pData = IDISPLAYPORT_2_VGASTATE(pInterface);

    if (!pcBits)
        return VERR_INVALID_PARAMETER;
    *pcBits = vga_get_bpp(pData);
    return VINF_SUCCESS;
}

/**
 * Create a 32-bbp snapshot of the display.
 *
 * @param   pInterface          Pointer to this interface.
 * @param   pvData              Pointer the buffer to copy the bits to.
 * @param   cbData              Size of the buffer.
 * @param   pcx                 Where to store the width of the bitmap. (optional)
 * @param   pcy                 Where to store the height of the bitmap. (optional)
 * @param   pcbData             Where to store the actual size of the bitmap. (optional)
 * @see     PDMIKEYBOARDPORT::pfnSnapshot() for details.
 */
static DECLCALLBACK(int) vgaPortSnapshot(PPDMIDISPLAYPORT pInterface, void *pvData, size_t cbData, uint32_t *pcx, uint32_t *pcy, size_t *pcbData)
{
    /* @todo r=sunlover: replace the method with a direct VRAM rendering like in vgaPortUpdateDisplayRect.  */
    PPDMIDISPLAYCONNECTOR   pConnector;
    PDMIDISPLAYCONNECTOR    Connector;
    int32_t                 graphic_mode;
    uint32_t                fRenderVRAM;
    size_t                  cbRequired;
    PVGASTATE               pData = IDISPLAYPORT_2_VGASTATE(pInterface);
    PDMDEV_ASSERT_EMT(VGASTATE2DEVINS(pData));
    LogFlow(("vgaPortSnapshot: pvData=%p cbData=%d pcx=%p pcy=%p pcbData=%p\n", pvData, cbData, pcx, pcy, pcbData));

    /*
     * Validate input.
     */
    if (!pvData)
        return VERR_INVALID_PARAMETER;

    /*
     * Do a regular refresh first to resolve any pending resize issues.
     *
     * 20060317 It used to be pfnUpdateDisplay, but by VBVA design
     * only pfnUpdateDisplayAll is allowed to be called in VBVA mode.
     * Also since the goal here is to have updated display for screenshot,
     * the UpdateDisplayAll is even more logical to call. (sunlover)
     */
    pInterface->pfnUpdateDisplayAll(pInterface);

    /*
     * Validate the buffer size.
     */
    cbRequired = RT_ALIGN_Z(pData->last_scr_width, 4) * pData->last_scr_height * 4;
    if (cbRequired > cbData)
    {
        Log(("vgaPortSnapshot: %d bytes are required, a buffer of %d bytes is profiled.\n", cbRequired, cbData));
        return VERR_BUFFER_OVERFLOW;
    }

    /*
     * Temporarily replace the display connector interface with a fake one.
     */
    Connector.pu8Data       = (uint8_t*)pvData;
    Connector.cBits         = 32;
    Connector.cx            = pData->pDrv->cx;
    Connector.cy            = pData->pDrv->cy;
    Connector.cbScanline    = RT_ALIGN_32(Connector.cx, 4) * 4;
    Connector.pfnRefresh    = vgaDummyRefresh;
    Connector.pfnResize     = vgaDummyResize;
    Connector.pfnUpdateRect = vgaDummyUpdateRect;

    /* save & replace state data. */
    pConnector = pData->pDrv;
    pData->pDrv = &Connector;
    graphic_mode = pData->graphic_mode;
    pData->graphic_mode = -1;           /* force a full refresh. */
    fRenderVRAM = pData->fRenderVRAM;
    pData->fRenderVRAM = 1;             /* force the guest VRAM rendering to the given buffer. */

    /* make the snapshot. */
    int rc = vga_update_display(pData);

    /* restore */
    pData->pDrv = pConnector;
    pData->graphic_mode = graphic_mode;
    pData->fRenderVRAM = fRenderVRAM;

    if (rc != VINF_SUCCESS)
        return rc;

    /*
     * Return the result.
     */
    if (pcx)
        *pcx = Connector.cx;
    if (pcy)
        *pcy = Connector.cy;
    if (pcbData)
        *pcbData = cbRequired;
    LogFlow(("vgaPortSnapshot: returns VINF_SUCCESS (cx=%d cy=%d cbData=%d)\n", Connector.cx, Connector.cy, cbRequired));
    return VINF_SUCCESS;
}


/**
 * Copy bitmap to the display.
 *
 * @param   pInterface          Pointer to this interface.
 * @param   pvData              Pointer to the bitmap bits.
 * @param   x                   The upper left corner x coordinate of the destination rectangle.
 * @param   y                   The upper left corner y coordinate of the destination rectangle.
 * @param   cx                  The width of the source and destination rectangles.
 * @param   cy                  The height of the source and destination rectangles.
 * @see     PDMIDISPLAYPORT::pfnDisplayBlt() for details.
 */
static DECLCALLBACK(int) vgaPortDisplayBlt(PPDMIDISPLAYPORT pInterface, const void *pvData, uint32_t x, uint32_t y, uint32_t cx, uint32_t cy)
{
    PVGASTATE       pData = IDISPLAYPORT_2_VGASTATE(pInterface);
    int             rc = VINF_SUCCESS;
    PDMDEV_ASSERT_EMT(VGASTATE2DEVINS(pData));
    LogFlow(("vgaPortDisplayBlt: pvData=%p x=%d y=%d cx=%d cy=%d\n", pvData, x, y, cx, cy));

    /*
     * Validate input.
     */
    if (    pvData
        &&  x      <  pData->pDrv->cx
        &&  cx     <= pData->pDrv->cx
        &&  cx + x <= pData->pDrv->cx
        &&  y      <  pData->pDrv->cy
        &&  cy     <= pData->pDrv->cy
        &&  cy + y <= pData->pDrv->cy)
    {
        /*
         * Determin bytes per pixel in the destination buffer.
         */
        size_t  cbPixelDst = 0;
        switch (pData->pDrv->cBits)
        {
            case 8:
                cbPixelDst = 1;
                break;
            case 15:
            case 16:
                cbPixelDst = 2;
                break;
            case 24:
                cbPixelDst = 3;
                break;
            case 32:
                cbPixelDst = 4;
                break;
            default:
                rc = VERR_INVALID_PARAMETER;
                break;
        }
        if (VBOX_SUCCESS(rc))
        {
            /*
             * The blitting loop.
             */
            size_t      cbLineSrc   = RT_ALIGN_Z(cx, 4) * 4;
            uint8_t    *pu8Src      = (uint8_t *)pvData;
            size_t      cbLineDst   = pData->pDrv->cbScanline;
            uint8_t    *pu8Dst      = pData->pDrv->pu8Data + y * cbLineDst + x * cbPixelDst;
            uint32_t    cyLeft      = cy;
            vga_draw_line_func *pfnVgaDrawLine = vga_draw_line_table[VGA_DRAW_LINE32 * 4 + get_depth_index(pData->pDrv->cBits)];
            Assert(pfnVgaDrawLine);
            while (cyLeft-- > 0)
            {
                pfnVgaDrawLine(pData, pu8Dst, pu8Src, cx);
                pu8Dst += cbLineDst;
                pu8Src += cbLineSrc;
            }

            /*
             * Invalidate the area.
             */
            pData->pDrv->pfnUpdateRect(pData->pDrv, x, y, cx, cy);
        }
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlow(("vgaPortDisplayBlt: returns %Vrc\n", rc));
    return rc;
}

static DECLCALLBACK(void) vgaPortUpdateDisplayRect (PPDMIDISPLAYPORT pInterface, int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    uint32_t v;
    vga_draw_line_func *vga_draw_line;

    uint32_t cbPixelDst;
    uint32_t cbLineDst;
    uint8_t *pu8Dst;

    uint32_t cbPixelSrc;
    uint32_t cbLineSrc;
    uint8_t *pu8Src;

    uint32_t u32OffsetSrc, u32Dummy;

    PVGASTATE s = IDISPLAYPORT_2_VGASTATE(pInterface);

#ifdef DEBUG_sunlover
    LogFlow(("vgaPortUpdateDisplayRect: %d,%d %dx%d\n", x, y, w, h));
#endif /* DEBUG_sunlover */

    Assert(pInterface);
    Assert(s->pDrv);
    Assert(s->pDrv->pu8Data);

    /* Check if there is something to do at all. */
    if (!s->fRenderVRAM)
    {
        /* The framebuffer uses the guest VRAM directly. */
#ifdef DEBUG_sunlover
        LogFlow(("vgaPortUpdateDisplayRect: nothing to do fRender is false.\n"));
#endif /* DEBUG_sunlover */
        return;
    }

    /* Correct negative x and y coordinates. */
    if (x < 0)
    {
        x += w; /* Compute xRight which is also the new width. */
        w = (x < 0) ? 0 : x;
        x = 0;
    }

    if (y < 0)
    {
        y += h; /* Compute yBottom, which is also the new height. */
        h = (y < 0) ? 0 : y;
        y = 0;
    }

    /* Also check if coords are greater than the display resolution. */
    if (x + w > s->pDrv->cx)
    {
#ifndef VBOX
        w = s->pDrv->cx > x? s->pDrv->cx - x: 0;
#else
        // x < 0 is not possible here
        w = s->pDrv->cx > (uint32_t)x? s->pDrv->cx - x: 0;
#endif
    }

    if (y + h > s->pDrv->cy)
    {
#ifndef VBOX
        h = s->pDrv->cy > y? s->pDrv->cy - y: 0;
#else
        // y < 0 is not possible here
        h = s->pDrv->cy > (uint32_t)y? s->pDrv->cy - y: 0;
#endif
    }

#ifdef DEBUG_sunlover
    LogFlow(("vgaPortUpdateDisplayRect: %d,%d %dx%d (corrected coords)\n", x, y, w, h));
#endif /* DEBUG_sunlover */

    /* Check if there is something to do at all. */
    if (w == 0 || h == 0)
    {
        /* Empty rectangle. */
#ifdef DEBUG_sunlover
        LogFlow(("vgaPortUpdateDisplayRect: nothing to do: %dx%d\n", w, h));
#endif /* DEBUG_sunlover */
        return;
    }

    /** @todo This method should be made universal and not only for VBVA.
     *  VGA_DRAW_LINE* must be selected and src/dst address calculation
     *  changed.
     */

    /* Choose the rendering function. */
    switch(s->get_bpp(s))
    {
        default:
        case 0:
            /* A LFB mode is already disabled, but the callback is still called
             * by Display because VBVA buffer is being flushed.
             * Nothing to do, just return.
             */
            return;
        case 8:
            v = VGA_DRAW_LINE8;
            break;
        case 15:
            v = VGA_DRAW_LINE15;
            break;
        case 16:
            v = VGA_DRAW_LINE16;
            break;
        case 24:
            v = VGA_DRAW_LINE24;
            break;
        case 32:
            v = VGA_DRAW_LINE32;
            break;
    }

    vga_draw_line = vga_draw_line_table[v * 4 + get_depth_index(s->pDrv->cBits)];

    /* Compute source and destination addresses and pitches. */
    cbPixelDst = (s->pDrv->cBits + 7) / 8;
    cbLineDst  = s->pDrv->cbScanline;
    pu8Dst     = s->pDrv->pu8Data + y * cbLineDst + x * cbPixelDst;

    cbPixelSrc = (s->get_bpp(s) + 7) / 8;
    s->get_offsets (s, &cbLineSrc, &u32OffsetSrc, &u32Dummy);

    /* Assume that rendering is performed only on visible part of VRAM.
     * This is true because coordinates were verified.
     */
    pu8Src = s->vram_ptrHC;
    pu8Src += u32OffsetSrc + y * cbLineSrc + x * cbPixelSrc;

    /* Render VRAM to framebuffer. */

#ifdef DEBUG_sunlover
    LogFlow(("vgaPortUpdateDisplayRect: dst: %p, %d, %d. src: %p, %d, %d\n", pu8Dst, cbLineDst, cbPixelDst, pu8Src, cbLineSrc, cbPixelSrc));
#endif /* DEBUG_sunlover */

    while (h-- > 0)
    {
        vga_draw_line (s, pu8Dst, pu8Src, w);
        pu8Dst += cbLineDst;
        pu8Src += cbLineSrc;
    }

#ifdef DEBUG_sunlover
    LogFlow(("vgaPortUpdateDisplayRect: completed.\n"));
#endif /* DEBUG_sunlover */
}

static DECLCALLBACK(void) vgaPortSetRenderVRAM(PPDMIDISPLAYPORT pInterface, bool fRender)
{
    PVGASTATE s = IDISPLAYPORT_2_VGASTATE(pInterface);

    LogFlow(("vgaPortSetRenderVRAM: fRender = %d\n", fRender));

    s->fRenderVRAM = fRender;
}


static DECLCALLBACK(void) vgaTimerRefresh(PPDMDEVINS pDevIns, PTMTIMER pTimer)
{
    PVGASTATE pData = PDMINS2DATA(pDevIns, PVGASTATE);
    if (pData->pDrv)
        pData->pDrv->pfnRefresh(pData->pDrv);
    if (pData->cMilliesRefreshInterval)
        TMTimerSetMillies(pTimer, pData->cMilliesRefreshInterval);
}


/* -=-=-=-=-=- Ring 3: PCI Device -=-=-=-=-=- */

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
static DECLCALLBACK(int) vgaR3IORegionMap(PPCIDEVICE pPciDev, /*unsigned*/ int iRegion, RTGCPHYS GCPhysAddress, uint32_t cb, PCIADDRESSSPACE enmType)
{
    int         rc;
    PVGASTATE   pData = PDMINS2DATA(pPciDev->pDevIns, PVGASTATE);
    LogFlow(("vgaR3IORegionMap: iRegion=%d GCPhysAddress=%VGp cb=%#x enmType=%d\n", iRegion, GCPhysAddress, cb, enmType));

    /*
     * VRam mapping.
     */
    if (iRegion == 0 && enmType == PCI_ADDRESS_SPACE_MEM_PREFETCH)
    {
        /*
         * Register and lock the VRAM.
         *
         * Windows usually re-initializes the PCI devices, so we have to check whether the memory was
         * already registered before trying to do that all over again.
         */
        PVM pVM = PDMDevHlpGetVM(pPciDev->pDevIns);
        if (pData->GCPhysVRAM)
        {
            AssertMsg(pData->GCPhysVRAM == GCPhysAddress,
                      ("The Guest OS relocated our LFB! old GCPhysVRAM=%VGp new GCPhysAddress=%VGp\n",
                       pData->GCPhysVRAM, GCPhysAddress));
            rc = VINF_SUCCESS;
        }
        else
        {
            /*
             * Register and lock the VRAM.
             */
            rc = MMR3PhysRegister(pVM, pData->vram_ptrHC, GCPhysAddress, pData->vram_size, MM_RAM_FLAGS_MMIO2, "VRam");
            if (VBOX_SUCCESS(rc))
            {
                if (!pData->GCPhysVRAM)
                    rc = PGMR3HandlerPhysicalRegister(pVM, PGMPHYSHANDLERTYPE_PHYSICAL_WRITE,
                                                      GCPhysAddress, GCPhysAddress + (pData->vram_size - 1),
                                                      vgaR3LFBAccessHandler, pData,
                                                      g_DeviceVga.szR0Mod, "vgaR0LFBAccessHandler", pData->pDevInsHC->pvInstanceDataR0,
                                                      g_DeviceVga.szGCMod, "vgaGCLFBAccessHandler", pData->pDevInsHC->pvInstanceDataGC,
                                                      "VGA LFB");
                if (VBOX_SUCCESS(rc))
                {
                    /*
                     * Map the first 256KB of the VRAM into GC for GC VGA support.
                     */
                    RTGCPTR GCPtr;
                    rc = MMR3HyperMapGCPhys(pVM, GCPhysAddress, VGA_MAPPING_SIZE, "VGA VRam", &GCPtr);
                    if (VBOX_SUCCESS(rc))
                    {
                        MMR3HyperReserve(pVM, PAGE_SIZE, "fence", NULL);

                        pData->vram_ptrGC = GCPtr;
                        pData->GCPhysVRAM = GCPhysAddress;
                        return VINF_SUCCESS;
                    }
                    AssertMsgFailed(("MMR3HyperMapGCPhys failed, rc=%Vrc\n", rc));
                }
                else
                    AssertMsgFailed(("Failed to register write handler for VRAM! rc=%Vrc\n", rc));
            }
            else
                AssertReleaseMsgFailed(("Failed to register VRAM! rc=%Vra\n", rc));
        }
        return rc;
    }
    else
        AssertReleaseMsgFailed(("Huh!?! iRegion=%d enmType=%d\n", iRegion, enmType));
    return VERR_INTERNAL_ERROR;
}


/* -=-=-=-=-=- Ring3: Misc Wrappers -=-=-=-=-=- */

/**
 * Saves a state of the VGA device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to save the state to.
 */
static DECLCALLBACK(int) vgaR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle)
{
    vga_save(pSSMHandle, PDMINS2DATA(pDevIns, PVGASTATE));
    return VINF_SUCCESS;
}


/**
 * Loads a saved VGA device state.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to the saved state.
 * @param   u32Version  The data unit version number.
 */
static DECLCALLBACK(int) vgaR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle, uint32_t u32Version)
{
    if (vga_load(pSSMHandle, PDMINS2DATA(pDevIns, PVGASTATE), u32Version))
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    return VINF_SUCCESS;
}


/* -=-=-=-=-=- Ring 3: Device callbacks -=-=-=-=-=- */

/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void)  vgaR3Reset(PPDMDEVINS pDevIns)
{
    PVGASTATE       pData = PDMINS2DATA(pDevIns, PVGASTATE);
    char           *pchStart;
    char           *pchEnd;
    LogFlow(("vgaReset\n"));

    /* Clear the VRAM ourselves. */
    if (pData->vram_ptrHC && pData->vram_size)
    {
#ifdef LOG_ENABLED /** @todo separate function. */
        /* First dump the textmode contents to the log; handy for capturing Windows blue screens. */
        uint8_t graphic_mode;
        VGAState *s = pData;

        if (!(s->ar_index & 0x20)) {
            graphic_mode = GMODE_BLANK;
        } else {
            graphic_mode = s->gr[6] & 1;
        }
        switch(graphic_mode)
        case GMODE_TEXT:
        {
            int cw, height, width, cheight, cx_min, cx_max, cy, cx;
            int x_incr;
            uint8_t *s1, *src, ch, cattr;
            int line_offset;
            uint16_t ch_attr;

            line_offset = s->line_offset;
            s1 = s->CTXSUFF(vram_ptr) + (s->start_addr * 4);

            /* total width & height */
            cheight = (s->cr[9] & 0x1f) + 1;
            cw = 8;
            if (!(s->sr[1] & 0x01))
                cw = 9;
            if (s->sr[1] & 0x08)
                cw = 16; /* NOTE: no 18 pixel wide */
            x_incr = cw * ((s->pDrv->cBits + 7) >> 3);
            width = (s->cr[0x01] + 1);
            if (s->cr[0x06] == 100) {
                /* ugly hack for CGA 160x100x16 - explain me the logic */
                height = 100;
            } else {
                height = s->cr[0x12] |
                    ((s->cr[0x07] & 0x02) << 7) |
                    ((s->cr[0x07] & 0x40) << 3);
                height = (height + 1) / cheight;
            }
            if ((height * width) > CH_ATTR_SIZE) {
                /* better than nothing: exit if transient size is too big */
                break;
            }
            RTLogPrintf("VGA textmode BEGIN (%dx%d):\n\n", height, width);
            for(cy = 0; cy < height; cy++) {
                src = s1;
                cx_min = width;
                cx_max = -1;
                for(cx = 0; cx < width; cx++) {
                    ch_attr = *(uint16_t *)src;
                    if (cx < cx_min)
                        cx_min = cx;
                    if (cx > cx_max)
                        cx_max = cx;
# ifdef WORDS_BIGENDIAN
                    ch = ch_attr >> 8;
                    cattr = ch_attr & 0xff;
# else
                    ch = ch_attr & 0xff;
                    cattr = ch_attr >> 8;
# endif
                    RTLogPrintf("%c", ch);

                    src += 4;
                }
                if (cx_max != -1)
                    RTLogPrintf("\n");

                s1 += line_offset;
            }
            RTLogPrintf("VGA textmode END:\n\n");
        }

#endif
        memset(pData->vram_ptrHC, 0, pData->vram_size);
    }

    /*
     * Zero most of it.
     *
     * Unlike vga_reset we're leaving out a few members which believe must
     * remain unchanged....
     */
    /* 1st part. */
    pchStart = (char *)&pData->latch;
    pchEnd   = (char *)&pData->invalidated_y_table;
    memset(pchStart, 0, pchEnd - pchStart);

    /* 2nd part. */
    pchStart = (char *)&pData->last_palette;
    pchEnd   = (char *)&pData->u32Marker;
    memset(pchStart, 0, pchEnd - pchStart);


    /*
     * Restore and re-init some bits.
     */
    pData->get_bpp        = vga_get_bpp;
    pData->get_offsets    = vga_get_offsets;
    pData->get_resolution = vga_get_resolution;
    pData->graphic_mode   = -1;         /* Force full update. */
#ifdef CONFIG_BOCHS_VBE
    pData->vbe_regs[VBE_DISPI_INDEX_ID] = VBE_DISPI_ID0;
    pData->vbe_regs[VBE_DISPI_INDEX_VBOX_VIDEO] = 0;
    pData->vbe_bank_mask    = ((pData->vram_size >> 16) - 1);
#endif /* CONFIG_BOCHS_VBE */

    /*
     * Reset the LBF mapping.
     */
    pData->fLFBUpdated = false;
    if (    (   pData->fGCEnabled
             || pData->fR0Enabled)
        &&  pData->GCPhysVRAM)
    {
        int rc = PGMHandlerPhysicalReset(PDMDevHlpGetVM(pDevIns), pData->GCPhysVRAM);
        AssertRC(rc);
    }

    /* notify port handler */
    if (pData->pDrv)
        pData->pDrv->pfnReset(pData->pDrv);
}


/**
 * Device relocation callback.
 *
 * @param   pDevIns     Pointer to the device instance.
 * @param   offDelta    The relocation delta relative to the old location.
 *
 * @see     FNPDMDEVRELOCATE for details.
 */
static DECLCALLBACK(void) vgaR3Relocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    if (offDelta)
    {
        PVGASTATE pData = PDMINS2DATA(pDevIns, PVGASTATE);
        LogFlow(("vgaRelocate: offDelta = %08X\n", offDelta));

        pData->GCPtrLFBHandler += offDelta;
        pData->vram_ptrGC += offDelta;
    }
}


/**
 * Attach command.
 *
 * This is called to let the device attach to a driver for a specified LUN
 * during runtime. This is not called during VM construction, the device
 * constructor have to attach to all the available drivers.
 *
 * This is like plugging in the monitor after turning on the PC.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being detached.
 */
static DECLCALLBACK(int)  vgaAttach(PPDMDEVINS pDevIns, unsigned iLUN)
{
    PVGASTATE   pData = PDMINS2DATA(pDevIns, PVGASTATE);
    switch (iLUN)
    {
        /* LUN #0: Display port. */
        case 0:
        {
            int rc = PDMDevHlpDriverAttach(pDevIns, iLUN, &pData->Base, &pData->pDrvBase, "Display Port");
            if (VBOX_SUCCESS(rc))
            {
                pData->pDrv = (PDMIDISPLAYCONNECTOR*)pData->pDrvBase->pfnQueryInterface(pData->pDrvBase, PDMINTERFACE_DISPLAY_CONNECTOR);
                if (pData->pDrv)
                {
                    /* pData->pDrv->pu8Data can be NULL when there is no framebuffer. */
                    if (    pData->pDrv->pfnRefresh
                        &&  pData->pDrv->pfnResize
                        &&  pData->pDrv->pfnUpdateRect)
                        rc = VINF_SUCCESS;
                    else
                    {
                        Assert(pData->pDrv->pfnRefresh);
                        Assert(pData->pDrv->pfnResize);
                        Assert(pData->pDrv->pfnUpdateRect);
                        pData->pDrv = NULL;
                        pData->pDrvBase = NULL;
                        rc = VERR_INTERNAL_ERROR;
                    }
                }
                else
                {
                    AssertMsgFailed(("LUN #0 doesn't have a display connector interface! rc=%Vrc\n", rc));
                    pData->pDrvBase = NULL;
                    rc = VERR_PDM_MISSING_INTERFACE;
                }
            }
            else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
            {
                Log(("%s/%d: warning: no driver attached to LUN #0!\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
                rc = VINF_SUCCESS;
            }
            else
                AssertMsgFailed(("Failed to attach LUN #0! rc=%Vrc\n", rc));
            return rc;
        }

        default:
            AssertMsgFailed(("Invalid LUN #%d\n", iLUN));
            return VERR_PDM_NO_SUCH_LUN;
    }
}


/**
 * Detach notification.
 *
 * This is called when a driver is detaching itself from a LUN of the device.
 * The device should adjust it's state to reflect this.
 *
 * This is like unplugging the monitor while the PC is still running.
 *
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being detached.
 */
static DECLCALLBACK(void)  vgaDetach(PPDMDEVINS pDevIns, unsigned iLUN)
{
    /*
     * Reset the interfaces and update the controller state.
     */
    PVGASTATE   pData = PDMINS2DATA(pDevIns, PVGASTATE);
    switch (iLUN)
    {
        /* LUN #0: Display port. */
        case 0:
            pData->pDrv = NULL;
            pData->pDrvBase = NULL;
            break;

        default:
            AssertMsgFailed(("Invalid LUN #%d\n", iLUN));
            break;
    }
}



/**
 * Construct a VGA device instance for a VM.
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
static DECLCALLBACK(int)   vgaR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    static bool fExpandDone = false;
    bool        f;
    int         rc;
    unsigned i;
    PVGASTATE   pData = PDMINS2DATA(pDevIns, PVGASTATE);
    PVM         pVM = PDMDevHlpGetVM(pDevIns);
#ifdef VBE_NEW_DYN_LIST
    uint32_t    cCustomModes;
    uint32_t    cyReduction;
    PVBEHEADER  pVBEDataHdr;
    ModeInfoListItem *pCurMode;
    unsigned    cb;
#endif
    Assert(iInstance == 0);
    Assert(pVM);

    /*
     * Init static data.
     */
    if (!fExpandDone)
    {
        fExpandDone = true;
        vga_init_expand();
    }

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "VRamSize\0"
                                          "GCEnabled\0"
                                          "R0Enabled\0"
                                          "CustomVideoModes\0"
                                          "HeightReduction\0"
                                          "CustomVideoMode1\0"
                                          "CustomVideoMode2\0"
                                          "CustomVideoMode3\0"
                                          "CustomVideoMode4\0"
                                          "CustomVideoMode5\0"
                                          "CustomVideoMode6\0"
                                          "CustomVideoMode7\0"
                                          "CustomVideoMode8\0"
                                          "CustomVideoMode9\0"
                                          "CustomVideoMode10\0"
                                          "CustomVideoMode11\0"
                                          "CustomVideoMode12\0"
                                          "CustomVideoMode13\0"
                                          "CustomVideoMode14\0"
                                          "CustomVideoMode15\0"
                                          "CustomVideoMode16\0"))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("Invalid configuration for vga device"));

    /*
     * Init state data.
     */
    rc = CFGMR3QueryU32(pCfgHandle, "VRamSize", &pData->vram_size);
    if (VBOX_FAILURE(rc) || !pData->vram_size)
        pData->vram_size = VGA_VRAM_DEFAULT;
    else if (pData->vram_size > VGA_VRAM_MAX)
    {
        AssertMsgFailed(("vram_size=%d max=%d\n", pData->vram_size, VGA_VRAM_MAX));
        pData->vram_size = VGA_VRAM_MAX;
    }
    else if (pData->vram_size < VGA_VRAM_MIN)
    {
        AssertMsgFailed(("vram_size=%d min=%d\n", pData->vram_size, VGA_VRAM_MIN));
        pData->vram_size = RT_ALIGN_32(pData->vram_size, _1M);
    }
    Log(("VGA: VRamSize=%#x\n", pData->vram_size));

    pData->fGCEnabled = true;
    rc = CFGMR3QueryBool(pCfgHandle, "GCEnabled", &f);
    if (VBOX_SUCCESS(rc) && !f)
        pData->fGCEnabled = false;
    Log(("VGA: fGCEnabled=%d\n", pData->fGCEnabled));

    pData->fR0Enabled = true;
    rc = CFGMR3QueryBool(pCfgHandle, "R0Enabled", &f);
    if (VBOX_SUCCESS(rc) && !f)
        pData->fR0Enabled = false;
    Log(("VGA: fR0Enabled=%d\n", pData->fR0Enabled));

    pData->pDevInsHC = pDevIns;

    vgaR3Reset(pDevIns);

    /* The PCI devices configuration. */
    pData->Dev.config[0x00] = 0xee;              /* PCI vendor, just a free bogus value */
    pData->Dev.config[0x01] = 0x80;

    pData->Dev.config[0x02] = 0xef;              /* Device ID */
    pData->Dev.config[0x03] = 0xbe;

    pData->Dev.config[0x0a] = 0x00;              /* VGA controller */
    pData->Dev.config[0x0b] = 0x03;
    pData->Dev.config[0x0e] = 0x00;              /* header_type */

    /* The LBF access handler - error handling is better here than in the map function.  */
    rc = PDMR3GetSymbolGCLazy(pVM, pDevIns->pDevReg->szGCMod, "vgaGCLFBAccessHandler", &pData->GCPtrLFBHandler);
    if (VBOX_FAILURE(rc))
    {
        AssertReleaseMsgFailed(("PDMR3GetSymbolGC(, %s, \"vgaGCLFBAccessHandler\",) -> %Vrc\n", pDevIns->pDevReg->szGCMod, rc));
        return rc;
    }

    /* the interfaces. */
    pData->Base.pfnQueryInterface       = vgaPortQueryInterface;

    pData->Port.pfnUpdateDisplay        = vgaPortUpdateDisplay;
    pData->Port.pfnUpdateDisplayAll     = vgaPortUpdateDisplayAll;
    pData->Port.pfnQueryColorDepth      = vgaPortQueryColorDepth;
    pData->Port.pfnSetRefreshRate       = vgaPortSetRefreshRate;
    pData->Port.pfnSnapshot             = vgaPortSnapshot;
    pData->Port.pfnDisplayBlt           = vgaPortDisplayBlt;
    pData->Port.pfnUpdateDisplayRect    = vgaPortUpdateDisplayRect;
    pData->Port.pfnSetRenderVRAM        = vgaPortSetRenderVRAM;


    /*
     * Register I/O ports, ROM and save state.
     */
    rc = PDMDevHlpIOPortRegister(pDevIns,  0x3c0, 16, NULL, vgaIOPortWrite,       vgaIOPortRead, NULL, NULL,      "VGA - 3c0");
    if (VBOX_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns,  0x3b4,  2, NULL, vgaIOPortWrite,       vgaIOPortRead, NULL, NULL,      "VGA - 3b4");
    if (VBOX_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns,  0x3ba,  1, NULL, vgaIOPortWrite,       vgaIOPortRead, NULL, NULL,      "VGA - 3ba");
    if (VBOX_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns,  0x3d4,  2, NULL, vgaIOPortWrite,       vgaIOPortRead, NULL, NULL,      "VGA - 3d4");
    if (VBOX_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns,  0x3da,  1, NULL, vgaIOPortWrite,       vgaIOPortRead, NULL, NULL,      "VGA - 3da");
    if (VBOX_FAILURE(rc))
        return rc;

#ifdef CONFIG_BOCHS_VBE
    rc = PDMDevHlpIOPortRegister(pDevIns,  0x1ce,  1, NULL, vgaIOPortWriteVBEIndex, vgaIOPortReadVBEIndex, NULL, NULL, "VGA/VBE - Index");
    if (VBOX_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns,  0x1cf,  1, NULL, vgaIOPortWriteVBEData, vgaIOPortReadVBEData, NULL, NULL, "VGA/VBE - Data");
    if (VBOX_FAILURE(rc))
        return rc;
#if 0
    /* This now causes conflicts with Win2k & XP; it is not aware this range is taken
       and tries to map other devices there */
    /* Old Bochs. */
    rc = PDMDevHlpIOPortRegister(pDevIns, 0xff80,  1, NULL, vgaIOPortWriteVBEIndex, vgaIOPortReadVBEIndex, "VGA/VBE - Index Old");
    if (VBOX_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns, 0xff81,  1, NULL, vgaIOPortWriteVBEData, vgaIOPortReadVBEData, "VGA/VBE - Data Old");
    if (VBOX_FAILURE(rc))
        return rc;
#endif
#endif /* CONFIG_BOCHS_VBE */

    /* guest context extension */
    if (pData->fGCEnabled)
    {
        rc = PDMDevHlpIOPortRegisterGC(pDevIns,  0x3c0, 16, 0, "vgaIOPortWrite",       "vgaIOPortRead", NULL, NULL,     "VGA - 3c0 (GC)");
        if (VBOX_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterGC(pDevIns,  0x3b4,  2, 0, "vgaIOPortWrite",       "vgaIOPortRead", NULL, NULL,     "VGA - 3b4 (GC)");
        if (VBOX_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterGC(pDevIns,  0x3ba,  1, 0, "vgaIOPortWrite",       "vgaIOPortRead", NULL, NULL,     "VGA - 3ba (GC)");
        if (VBOX_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterGC(pDevIns,  0x3d4,  2, 0, "vgaIOPortWrite",       "vgaIOPortRead", NULL, NULL,     "VGA - 3d4 (GC)");
        if (VBOX_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterGC(pDevIns,  0x3da,  1, 0, "vgaIOPortWrite",       "vgaIOPortRead", NULL, NULL,     "VGA - 3da (GC)");
        if (VBOX_FAILURE(rc))
            return rc;
#ifdef CONFIG_BOCHS_VBE
        rc = PDMDevHlpIOPortRegisterGC(pDevIns,  0x1ce,  1, 0, "vgaIOPortWriteVBEIndex", "vgaIOPortReadVBEIndex", NULL, NULL, "VGA/VBE - Index (GC)");
        if (VBOX_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterGC(pDevIns,  0x1cf,  1, 0, "vgaIOPortWriteVBEData", "vgaIOPortReadVBEData", NULL, NULL, "VGA/VBE - Data (GC)");
        if (VBOX_FAILURE(rc))
            return rc;

#if 0
        /* This now causes conflicts with Win2k & XP; it is not aware this range is taken
           and tries to map other devices there */
        /* Old Bochs. */
        rc = PDMDevHlpIOPortRegisterGC(pDevIns, 0xff80,  1, 0, "vgaIOPortWriteVBEIndex", "vgaIOPortReadVBEIndex", "VGA/VBE - Index Old (GC)");
        if (VBOX_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterGC(pDevIns, 0xff81,  1, 0, "vgaIOPortWriteVBEData", "vgaIOPortReadVBEData", "VGA/VBE - Index Old (GC)");
        if (VBOX_FAILURE(rc))
            return rc;
#endif

#endif /* CONFIG_BOCHS_VBE */
    }

    /* R0 context extension */
    if (pData->fR0Enabled)
    {
        rc = PDMDevHlpIOPortRegisterR0(pDevIns,  0x3c0, 16, 0, "vgaIOPortWrite",       "vgaIOPortRead", NULL, NULL,     "VGA - 3c0 (GC)");
        if (VBOX_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterR0(pDevIns,  0x3b4,  2, 0, "vgaIOPortWrite",       "vgaIOPortRead", NULL, NULL,     "VGA - 3b4 (GC)");
        if (VBOX_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterR0(pDevIns,  0x3ba,  1, 0, "vgaIOPortWrite",       "vgaIOPortRead", NULL, NULL,     "VGA - 3ba (GC)");
        if (VBOX_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterR0(pDevIns,  0x3d4,  2, 0, "vgaIOPortWrite",       "vgaIOPortRead", NULL, NULL,     "VGA - 3d4 (GC)");
        if (VBOX_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterR0(pDevIns,  0x3da,  1, 0, "vgaIOPortWrite",       "vgaIOPortRead", NULL, NULL,     "VGA - 3da (GC)");
        if (VBOX_FAILURE(rc))
            return rc;
#ifdef CONFIG_BOCHS_VBE
        rc = PDMDevHlpIOPortRegisterR0(pDevIns,  0x1ce,  1, 0, "vgaIOPortWriteVBEIndex", "vgaIOPortReadVBEIndex", NULL, NULL, "VGA/VBE - Index (GC)");
        if (VBOX_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterR0(pDevIns,  0x1cf,  1, 0, "vgaIOPortWriteVBEData", "vgaIOPortReadVBEData", NULL, NULL, "VGA/VBE - Data (GC)");
        if (VBOX_FAILURE(rc))
            return rc;

#if 0
        /* This now causes conflicts with Win2k & XP; it is not aware this range is taken
           and tries to map other devices there */
        /* Old Bochs. */
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, 0xff80,  1, 0, "vgaIOPortWriteVBEIndex", "vgaIOPortReadVBEIndex", "VGA/VBE - Index Old (GC)");
        if (VBOX_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, 0xff81,  1, 0, "vgaIOPortWriteVBEData", "vgaIOPortReadVBEData", "VGA/VBE - Index Old (GC)");
        if (VBOX_FAILURE(rc))
            return rc;
#endif

#endif /* CONFIG_BOCHS_VBE */
    }

    /* vga mmio */
    rc = PDMDevHlpMMIORegister(pDevIns, 0x000a0000, 0x00020000, 0, vgaMMIOWrite, vgaMMIORead, vgaMMIOFill, "VGA - VGA Video Buffer");
    if (VBOX_FAILURE(rc))
        return rc;
    if (pData->fGCEnabled)
    {
        rc = PDMDevHlpMMIORegisterGC(pDevIns, 0x000a0000, 0x00020000, 0, "vgaMMIOWrite", "vgaMMIORead", "vgaMMIOFill", "VGA - VGA Video Buffer");
        if (VBOX_FAILURE(rc))
            return rc;
    }
    if (pData->fR0Enabled)
    {
        rc = PDMDevHlpMMIORegisterR0(pDevIns, 0x000a0000, 0x00020000, 0, "vgaMMIOWrite", "vgaMMIORead", "vgaMMIOFill", "VGA - VGA Video Buffer");
        if (VBOX_FAILURE(rc))
            return rc;
    }

    /* vga bios */
    rc = PDMDevHlpIOPortRegister(pDevIns, VBE_PRINTF_PORT, 1, NULL, vgaIOPortWriteBIOS, vgaIOPortReadBIOS, NULL, NULL, "VGA BIOS debug/panic");
    if (VBOX_FAILURE(rc))
        return rc;
    AssertReleaseMsg(g_cbVgaBiosBinary <= _64K && g_cbVgaBiosBinary >= 32*_1K, ("g_cbVgaBiosBinary=%#x\n", g_cbVgaBiosBinary));
    AssertReleaseMsg(RT_ALIGN_Z(g_cbVgaBiosBinary, PAGE_SIZE) == g_cbVgaBiosBinary, ("g_cbVgaBiosBinary=%#x\n", g_cbVgaBiosBinary));
    rc = PDMDevHlpROMRegister(pDevIns, 0x000c0000, g_cbVgaBiosBinary, &g_abVgaBiosBinary[0], 
                              false /* fShadow */, "VGA BIOS");
    if (VBOX_FAILURE(rc))
        return rc;

    /* save */
    rc = PDMDevHlpSSMRegister(pDevIns, pDevIns->pDevReg->szDeviceName, iInstance, 1 /* version */, sizeof(*pData),
                                          NULL, vgaR3SaveExec, NULL,
                                          NULL, vgaR3LoadExec, NULL);
    if (VBOX_FAILURE(rc))
        return rc;

    /* PCI */
    rc = PDMDevHlpPCIRegister(pDevIns, &pData->Dev);
    if (VBOX_FAILURE(rc))
        return rc;
    /*AssertMsg(pData->Dev.devfn == 16 || iInstance != 0, ("pData->Dev.devfn=%d\n", pData->Dev.devfn));*/
    if (pData->Dev.devfn != 16 && iInstance == 0)
        Log(("!!WARNING!!: pData->dev.devfn=%d (ignore if testcase or no started by Main)\n", pData->Dev.devfn));
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 0, pData->vram_size, PCI_ADDRESS_SPACE_MEM_PREFETCH, vgaR3IORegionMap);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Create the refresh timer.
     */
    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_REAL, vgaTimerRefresh, "VGA Refresh Timer", &pData->RefreshTimer);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Attach to the display.
     */
    rc = vgaAttach(pDevIns, 0 /* display LUN # */);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Allocate the VRAM.
     */
    rc = SUPPageAlloc(pData->vram_size >> PAGE_SHIFT, (void **)&pData->vram_ptrHC);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("SUPPageAlloc(%#x,) -> %d\n", pData->vram_size, rc));
        return rc;
    }

#ifdef VBE_NEW_DYN_LIST
    /*
     * Compute buffer size for the VBE BIOS Extra Data.
     */
    cb = sizeof(mode_info_list) + sizeof(ModeInfoListItem);

    rc = CFGMR3QueryU32(pCfgHandle, "HeightReduction", &cyReduction);
    if (VBOX_SUCCESS(rc) && cyReduction)
        cb *= 2;                            /* Default mode list will be twice long */
    else
        cyReduction = 0;

    rc = CFGMR3QueryU32(pCfgHandle, "CustomVideoModes", &cCustomModes);
    if (VBOX_SUCCESS(rc) && cCustomModes)
        cb += sizeof(ModeInfoListItem) * cCustomModes;
    else
        cCustomModes = 0;

    /*
     * Allocate and initialize buffer for the VBE BIOS Extra Data.
     */
    pData->cbVBEExtraData = sizeof(VBEHEADER) + cb;
    pData->pu8VBEExtraData = (uint8_t *)PDMDevHlpMMHeapAllocZ(pDevIns, pData->cbVBEExtraData);
    if (!pData->pu8VBEExtraData)
        return VERR_NO_MEMORY;

    pVBEDataHdr = (PVBEHEADER)pData->pu8VBEExtraData;
    pVBEDataHdr->u16Signature = VBEHEADER_MAGIC;
    pVBEDataHdr->cbData = cb;

#ifndef VRAM_SIZE_FIX
    pCurMode = memcpy(pVBEDataHdr + 1, &mode_info_list, sizeof(mode_info_list));
    pCurMode = (ModeInfoListItem *)((uintptr_t)pCurMode + sizeof(mode_info_list));
#else  /* VRAM_SIZE_FIX defined */
    pCurMode = (ModeInfoListItem *)(pVBEDataHdr + 1);
    for (i = 0; i < MODE_INFO_SIZE; i++)
    {
        uint32_t pixelWidth, reqSize;
        if (mode_info_list[i].info.MemoryModel == VBE_MEMORYMODEL_TEXT_MODE)
            pixelWidth = 2;
        else
            pixelWidth = (mode_info_list[i].info.BitsPerPixel +7) / 8;
        reqSize = mode_info_list[i].info.XResolution
                * mode_info_list[i].info.YResolution
                * pixelWidth;
        if (reqSize >= pData->vram_size)
            continue;
        *pCurMode = mode_info_list[i];
        pCurMode++;
    }
#endif  /* VRAM_SIZE_FIX defined */

    /*
     * Copy default modes with subtractred YResolution.
     */
    if (cyReduction)
    {
        ModeInfoListItem *pDefMode = mode_info_list;
        Log(("vgaR3Construct: cyReduction=%u\n", cyReduction));
#ifndef VRAM_SIZE_FIX
        for (i = 0; i < MODE_INFO_SIZE; i++, pCurMode++, pDefMode++)
        {
            *pCurMode = *pDefMode;
            pCurMode->mode += 0x30;
            pCurMode->info.YResolution -= cyReduction;
        }
#else  /* VRAM_SIZE_FIX defined */
        for (i = 0; i < MODE_INFO_SIZE; i++, pDefMode++)
        {
            uint32_t pixelWidth, reqSize;
            if (pDefMode->info.MemoryModel == VBE_MEMORYMODEL_TEXT_MODE)
                pixelWidth = 2;
            else
                pixelWidth = (pDefMode->info.BitsPerPixel + 7) / 8;
            reqSize = pDefMode->info.XResolution * pDefMode->info.YResolution *  pixelWidth;
            if (reqSize >= pData->vram_size)
                continue;
            *pCurMode = *pDefMode;
            pCurMode->mode += 0x30;
            pCurMode->info.YResolution -= cyReduction;
            pCurMode++;
        }
#endif  /* VRAM_SIZE_FIX defined */
    }


    /*
     * Add custom modes.
     */
    if (cCustomModes)
    {
        uint16_t u16CurMode = 0x160;
        for (i = 1; i <= cCustomModes; i++)
        {
            char szExtraDataKey[sizeof("CustomVideoModeXX")];
            char *pszExtraData = NULL;

            /* query and decode the custom mode string. */
            RTStrPrintf(szExtraDataKey, sizeof(szExtraDataKey), "CustomVideoMode%d", i);
            rc = CFGMR3QueryStringAlloc(pCfgHandle, szExtraDataKey, &pszExtraData);
            if (VBOX_SUCCESS(rc))
            {
                ModeInfoListItem *pDefMode = mode_info_list;
                unsigned int cx, cy, cBits, cParams;
                uint16_t u16DefMode;

                cParams = sscanf(pszExtraData, "%ux%ux%u", &cx, &cy, &cBits);
                if (    cParams != 3
                    ||  (cBits != 16 && cBits != 24 && cBits != 32))
                {
                    AssertMsgFailed(("Configuration error: Invalid mode data '%s' for '%s'! cBits=%d\n", pszExtraData, szExtraDataKey, cBits));
                    return VERR_VGA_INVALID_CUSTOM_MODE;
                }
#ifdef VRAM_SIZE_FIX
                if (cx * cy * cBits / 8 >= pData->vram_size)
                {
                    AssertMsgFailed(("Configuration error: custom video mode %dx%dx%dbits is too large for the virtual video memory of %dMb.  Please increase the video memory size.\n",
                                     cx, cy, cBits, pData->vram_size / _1M));
                    return VERR_VGA_INVALID_CUSTOM_MODE;
                }
#endif  /* VRAM_SIZE_FIX defined */
                MMR3HeapFree(pszExtraData);

                /* Use defaults from max@bpp mode. */
                switch (cBits)
                {
                    case 16:
                        u16DefMode = VBE_VESA_MODE_1024X768X565;
                        break;

                    case 24:
                        u16DefMode = VBE_VESA_MODE_1024X768X888;
                        break;

                    case 32:
                        u16DefMode = VBE_OWN_MODE_1024X768X8888;
                        break;

                    default: /* gcc, shut up! */
                        AssertMsgFailed(("gone postal!\n"));
                        continue;
                }

                while (     pDefMode->mode != u16DefMode
                       &&   pDefMode->mode != VBE_VESA_MODE_END_OF_LIST)
                    pDefMode++;
                Assert(pDefMode->mode != VBE_VESA_MODE_END_OF_LIST);

                *pCurMode  = *pDefMode;
                pCurMode->mode = u16CurMode++;

                /* adjust defaults */
                pCurMode->info.XResolution = cx;
                pCurMode->info.YResolution = cy;

                switch (cBits)
                {
                    case 16:
                        pCurMode->info.BytesPerScanLine = cx * 2;
                        pCurMode->info.LinBytesPerScanLine = cx * 2;
                        break;

                    case 24:
                        pCurMode->info.BytesPerScanLine = cx * 3;
                        pCurMode->info.LinBytesPerScanLine = cx * 3;
                        break;

                    case 32:
                        pCurMode->info.BytesPerScanLine = cx * 4;
                        pCurMode->info.LinBytesPerScanLine = cx * 4;
                        break;
                }

                /* commit it */
                pCurMode++;
            }
            else if (rc != VERR_CFGM_VALUE_NOT_FOUND)
            {
                AssertMsgFailed(("CFGMR3QueryStringAlloc(,'%s',) -> %Vrc\n", szExtraDataKey, rc));
                return rc;
            }
        } /* foreach custom mode key */
    }

    /*
     * Add the "End of list" mode.
     */
    memset(pCurMode, 0, sizeof(*pCurMode));
    pCurMode->mode = VBE_VESA_MODE_END_OF_LIST;

    /*
     * Register I/O Port for the VBE BIOS Extra Data.
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, VBE_EXTRA_PORT, 1, NULL, vbeIOPortWriteVBEExtra, vbeIOPortReadVBEExtra, NULL, NULL, "VBE BIOS Extra Data");
    if (VBOX_FAILURE(rc))
        return rc;
#endif

    /*
     * Statistics.
     */
    STAM_REG(pVM, &pData->StatGCMemoryRead,     STAMTYPE_PROFILE, "/Devices/VGA/GC/Memory/Read",         STAMUNIT_TICKS_PER_CALL, "Profiling of the VGAGCMemoryRead() body.");
    STAM_REG(pVM, &pData->StatGCMemoryWrite,    STAMTYPE_PROFILE, "/Devices/VGA/GC/Memory/Write",        STAMUNIT_TICKS_PER_CALL, "Profiling of the VGAGCMemoryWrite() body.");
    STAM_REG(pVM, &pData->StatGCIOPortRead,     STAMTYPE_PROFILE, "/Devices/VGA/GC/IOPort/Read",         STAMUNIT_TICKS_PER_CALL, "Profiling of the VGAGCIOPortRead() body.");
    STAM_REG(pVM, &pData->StatGCIOPortWrite,    STAMTYPE_PROFILE, "/Devices/VGA/GC/IOPort/Write",        STAMUNIT_TICKS_PER_CALL, "Profiling of the VGAGCIOPortWrite() body.");

    return VINF_SUCCESS;
}


/**
 * Destruct a device instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(int) vgaR3Destruct(PPDMDEVINS pDevIns)
{
#ifdef VBE_NEW_DYN_LIST
    PVGASTATE   pData = PDMINS2DATA(pDevIns, PVGASTATE);
    LogFlow(("vgaR3Destruct:\n"));

    /*
     * Free MM heap pointers.
     */
    if (pData->pu8VBEExtraData)
    {
        MMR3HeapFree(pData->pu8VBEExtraData);
        pData->pu8VBEExtraData = NULL;
    }
#endif

#if 0 /** @todo r=bird: We can't free the buffer here because it's still locked.
       * (That's the reason why we didn't do it earlier.) */
    /*
     * Free the VRAM.
     */
    int rc = SUPPageFree(pData->vram_ptrHC, pData->vram_size >> PAGE_SHIFT);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("SUPPageFree(%p, %#x) -> %d\n", pData->vram_ptrHC, pData->vram_size, rc));
        return rc;
    }
    pData->vram_ptrHC = NULL;
#endif

    return VINF_SUCCESS;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceVga =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szDeviceName */
    "vga",
    /* szGCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "VGA Adaptor with VESA extensions.",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_GRAPHICS,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(VGASTATE),
    /* pfnConstruct */
    vgaR3Construct,
    /* pfnDestruct */
    vgaR3Destruct,
    /* pfnRelocate */
    vgaR3Relocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    vgaR3Reset,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    vgaAttach,
    /* pfnDetach */
    vgaDetach,
    /* pfnQueryInterface */
    NULL,
    /* pfnInitComplete */
    NULL
};

#endif /* !IN_RING3 */
#endif /* VBOX */
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

/*
 * Local Variables:
 *   nuke-trailing-whitespace-p:nil
 * End:
 */
