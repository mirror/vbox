/* $Id$ */
/** @file
 * DevVGA - VBox VGA/VESA device.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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

/* WARNING!!! All defines that affect VGAState should be placed to DevVGA.h !!!
 *            NEVER place them here as this would lead to VGAState inconsistency
 *            across different .cpp files !!!
 */
/** The size of the VGA GC mapping.
 * This is supposed to be all the VGA memory accessible to the guest.
 * The initial value was 256KB but NTAllInOne.iso appears to access more
 * thus the limit was upped to 512KB.
 *
 * @todo Someone with some VGA knowhow should make a better guess at this value.
 */
#define VGA_MAPPING_SIZE    _512K

#ifdef VBOX_WITH_HGSMI
#define PCIDEV_2_VGASTATE(pPciDev)    ((VGAState *)((uintptr_t)pPciDev - RT_OFFSETOF(VGAState, Dev)))
#endif /* VBOX_WITH_HGSMI */
/** Converts a vga adaptor state pointer to a device instance pointer. */
#define VGASTATE2DEVINS(pVgaState)    ((pVgaState)->CTX_SUFF(pDevIns))

/** Check that the video modes fit into virtual video memory.
 * Only works when VBE_NEW_DYN_LIST is defined! */
#define VRAM_SIZE_FIX

/** Check buffer if an VRAM offset is within the right range or not. */
#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
# define VERIFY_VRAM_WRITE_OFF_RETURN(pThis, off) \
    do { \
        if ((off) >= VGA_MAPPING_SIZE) \
        { \
            AssertMsgReturn((off) < (pThis)->vram_size, ("%RX32 !< %RX32\n", (uint32_t)(off), (pThis)->vram_size), VINF_SUCCESS); \
            Log2(("%Rfn[%d]: %RX32 -> R3\n", __PRETTY_FUNCTION__, __LINE__, (off))); \
            return VINF_IOM_R3_MMIO_WRITE; \
        } \
    } while (0)
#else
# define VERIFY_VRAM_WRITE_OFF_RETURN(pThis, off) \
        AssertMsgReturn((off) < (pThis)->vram_size, ("%RX32 !< %RX32\n", (uint32_t)(off), (pThis)->vram_size), VINF_SUCCESS)
#endif

/** Check buffer if an VRAM offset is within the right range or not. */
#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
# define VERIFY_VRAM_READ_OFF_RETURN(pThis, off, rcVar) \
    do { \
        if ((off) >= VGA_MAPPING_SIZE) \
        { \
            AssertMsgReturn((off) < (pThis)->vram_size, ("%RX32 !< %RX32\n", (uint32_t)(off), (pThis)->vram_size), 0xff); \
            Log2(("%Rfn[%d]: %RX32 -> R3\n", __PRETTY_FUNCTION__, __LINE__, (off))); \
            (rcVar) = VINF_IOM_R3_MMIO_READ; \
            return 0; \
        } \
    } while (0)
#else
# define VERIFY_VRAM_READ_OFF_RETURN(pThis, off, rcVar) \
    do { \
        AssertMsgReturn((off) < (pThis)->vram_size, ("%RX32 !< %RX32\n", (uint32_t)(off), (pThis)->vram_size), 0xff); \
        NOREF(rcVar); \
    } while (0)
#endif


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_VGA
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pgm.h>
#ifdef IN_RING3
# include <iprt/alloc.h>
# include <iprt/ctype.h>
#endif /* IN_RING3 */
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/file.h>
#include <iprt/time.h>
#include <iprt/string.h>
#include <iprt/uuid.h>

#include <VBox/VMMDev.h>
#include <VBox/VBoxVideo.h>
#include <VBox/bioslogo.h>

/* should go BEFORE any other DevVGA include to make all DevVGA.h config defines be visible */
#include "DevVGA.h"

#if defined(VBE_NEW_DYN_LIST) && defined(IN_RING3) && !defined(VBOX_DEVICE_STRUCT_TESTCASE)
# include "DevVGAModes.h"
# include <stdio.h> /* sscan */
#endif

#include "vl_vbox.h"
#include "VBoxDD.h"
#include "VBoxDD2.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
#pragma pack(1)

/** BMP File Format Bitmap Header. */
typedef struct
{
    uint16_t      Type;           /* File Type Identifier       */
    uint32_t      FileSize;       /* Size of File               */
    uint16_t      Reserved1;      /* Reserved (should be 0)     */
    uint16_t      Reserved2;      /* Reserved (should be 0)     */
    uint32_t      Offset;         /* Offset to bitmap data      */
} BMPINFO;

/** Pointer to a bitmap header*/
typedef BMPINFO *PBMPINFO;

/** OS/2 1.x Information Header Format. */
typedef struct
{
    uint32_t      Size;           /* Size of Remaining Header   */
    uint16_t      Width;          /* Width of Bitmap in Pixels  */
    uint16_t      Height;         /* Height of Bitmap in Pixels */
    uint16_t      Planes;         /* Number of Planes           */
    uint16_t      BitCount;       /* Color Bits Per Pixel       */
} OS2HDR;

/** Pointer to a OS/2 1.x header format */
typedef OS2HDR *POS2HDR;

/** OS/2 2.0 Information Header Format. */
typedef struct
{
    uint32_t      Size;           /* Size of Remaining Header         */
    uint32_t      Width;          /* Width of Bitmap in Pixels        */
    uint32_t      Height;         /* Height of Bitmap in Pixels       */
    uint16_t      Planes;         /* Number of Planes                 */
    uint16_t      BitCount;       /* Color Bits Per Pixel             */
    uint32_t      Compression;    /* Compression Scheme (0=none)      */
    uint32_t      SizeImage;      /* Size of bitmap in bytes          */
    uint32_t      XPelsPerMeter;  /* Horz. Resolution in Pixels/Meter */
    uint32_t      YPelsPerMeter;  /* Vert. Resolution in Pixels/Meter */
    uint32_t      ClrUsed;        /* Number of Colors in Color Table  */
    uint32_t      ClrImportant;   /* Number of Important Colors       */
    uint16_t      Units;          /* Resolution Measurement Used      */
    uint16_t      Reserved;       /* Reserved FIelds (always 0)       */
    uint16_t      Recording;      /* Orientation of Bitmap            */
    uint16_t      Rendering;      /* Halftone Algorithm Used on Image */
    uint32_t      Size1;          /* Halftone Algorithm Data          */
    uint32_t      Size2;          /* Halftone Algorithm Data          */
    uint32_t      ColorEncoding;  /* Color Table Format (always 0)    */
    uint32_t      Identifier;     /* Misc. Field for Application Use  */
} OS22HDR;

/** Pointer to a OS/2 2.0 header format */
typedef OS22HDR *POS22HDR;

/** Windows 3.x Information Header Format. */
typedef struct
{
    uint32_t      Size;           /* Size of Remaining Header         */
    uint32_t      Width;          /* Width of Bitmap in Pixels        */
    uint32_t      Height;         /* Height of Bitmap in Pixels       */
    uint16_t      Planes;         /* Number of Planes                 */
    uint16_t      BitCount;       /* Bits Per Pixel                   */
    uint32_t      Compression;    /* Compression Scheme (0=none)      */
    uint32_t      SizeImage;      /* Size of bitmap in bytes          */
    uint32_t      XPelsPerMeter;  /* Horz. Resolution in Pixels/Meter */
    uint32_t      YPelsPerMeter;  /* Vert. Resolution in Pixels/Meter */
    uint32_t      ClrUsed;        /* Number of Colors in Color Table  */
    uint32_t      ClrImportant;   /* Number of Important Colors       */
} WINHDR;

/** Pointer to a Windows 3.x header format */
typedef WINHDR *PWINHDR;

#pragma pack()

#define BMP_ID               0x4D42

/** @name BMP compressions.
 * @{ */
#define BMP_COMPRESS_NONE    0
#define BMP_COMPRESS_RLE8    1
#define BMP_COMPRESS_RLE4    2
/** @} */

/** @name BMP header sizes.
 * @{ */
#define BMP_HEADER_OS21      12
#define BMP_HEADER_OS22      64
#define BMP_HEADER_WIN3      40
/** @} */

/** The BIOS boot menu text position, X. */
#define LOGO_F12TEXT_X       304
/** The BIOS boot menu text position, Y. */
#define LOGO_F12TEXT_Y       464

/** Width of the "Press F12 to select boot device." bitmap.
    Anything that exceeds the limit of F12BootText below is filled with
    background. */
#define LOGO_F12TEXT_WIDTH   286
/** Height of the boot device selection bitmap, see LOGO_F12TEXT_WIDTH. */
#define LOGO_F12TEXT_HEIGHT  12

/** The BIOS logo delay time (msec). */
#define LOGO_DELAY_TIME      2000

#define LOGO_MAX_WIDTH       640
#define LOGO_MAX_HEIGHT      480
#define LOGO_MAX_SIZE        LOGO_MAX_WIDTH * LOGO_MAX_HEIGHT * 4


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/* "Press F12 to select boot device." bitmap. */
static const uint8_t g_abLogoF12BootText[] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x07, 0x0F, 0x7C,
    0xF8, 0xF0, 0x01, 0xE0, 0x81, 0x9F, 0x3F, 0x00, 0x70, 0xF8, 0x00, 0xE0, 0xC3,
    0x07, 0x0F, 0x1F, 0x3E, 0x70, 0x00, 0xF0, 0xE1, 0xC3, 0x07, 0x0E, 0x00, 0x6E,
    0x7C, 0x60, 0xE0, 0xE1, 0xC3, 0x07, 0xC6, 0x80, 0x81, 0x31, 0x63, 0xC6, 0x00,
    0x30, 0x80, 0x61, 0x0C, 0x00, 0x36, 0x63, 0x00, 0x8C, 0x19, 0x83, 0x61, 0xCC,
    0x18, 0x36, 0x00, 0xCC, 0x8C, 0x19, 0xC3, 0x06, 0xC0, 0x8C, 0x31, 0x3C, 0x30,
    0x8C, 0x19, 0x83, 0x31, 0x60, 0x60, 0x00, 0x0C, 0x18, 0x00, 0x0C, 0x60, 0x18,
    0x00, 0x80, 0xC1, 0x18, 0x00, 0x30, 0x06, 0x60, 0x18, 0x30, 0x80, 0x01, 0x00,
    0x33, 0x63, 0xC6, 0x30, 0x00, 0x30, 0x63, 0x80, 0x19, 0x0C, 0x03, 0x06, 0x00,
    0x0C, 0x18, 0x18, 0xC0, 0x81, 0x03, 0x00, 0x03, 0x18, 0x0C, 0x00, 0x60, 0x30,
    0x06, 0x00, 0x87, 0x01, 0x18, 0x06, 0x0C, 0x60, 0x00, 0xC0, 0xCC, 0x98, 0x31,
    0x0C, 0x00, 0xCC, 0x18, 0x30, 0x0C, 0xC3, 0x80, 0x01, 0x00, 0x03, 0x66, 0xFE,
    0x18, 0x30, 0x00, 0xC0, 0x02, 0x06, 0x06, 0x00, 0x18, 0x8C, 0x01, 0x60, 0xE0,
    0x0F, 0x86, 0x3F, 0x03, 0x18, 0x00, 0x30, 0x33, 0x66, 0x0C, 0x03, 0x00, 0x33,
    0xFE, 0x0C, 0xC3, 0x30, 0xE0, 0x0F, 0xC0, 0x87, 0x9B, 0x31, 0x63, 0xC6, 0x00,
    0xF0, 0x80, 0x01, 0x03, 0x00, 0x06, 0x63, 0x00, 0x8C, 0x19, 0x83, 0x61, 0xCC,
    0x18, 0x06, 0x00, 0x6C, 0x8C, 0x19, 0xC3, 0x00, 0x80, 0x8D, 0x31, 0xC3, 0x30,
    0x8C, 0x19, 0x03, 0x30, 0xB3, 0xC3, 0x87, 0x0F, 0x1F, 0x00, 0x2C, 0x60, 0x80,
    0x01, 0xE0, 0x87, 0x0F, 0x00, 0x3E, 0x7C, 0x60, 0xF0, 0xE1, 0xE3, 0x07, 0x00,
    0x0F, 0x3E, 0x7C, 0xFC, 0x00, 0xC0, 0xC3, 0xC7, 0x30, 0x0E, 0x3E, 0x7C, 0x00,
    0xCC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x23, 0x1E, 0xC0, 0x00, 0x60, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x60, 0x00, 0xC0, 0x00, 0x00, 0x00,
    0x0C, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xC0, 0x0C, 0x87, 0x31, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x06, 0x00, 0x00, 0x18, 0x00, 0x30, 0x00, 0x00, 0x00, 0x03, 0x00, 0x30,
    0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0xE0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xF8, 0x83, 0xC1, 0x07, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x01, 0x00,
    0x00, 0x04, 0x00, 0x0E, 0x00, 0x00, 0x80, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x30,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


#ifndef VBOX_DEVICE_STRUCT_TESTCASE


/**
 * Set a VRAM page dirty.
 *
 * @param   pThis       VGA instance data.
 * @param   offVRAM     The VRAM offset of the page to set.
 */
DECLINLINE(void) vga_set_dirty(VGAState *pThis, RTGCPHYS offVRAM)
{
    AssertMsg(offVRAM < pThis->vram_size, ("offVRAM = %p, pThis->vram_size = %p\n", offVRAM, pThis->vram_size));
    ASMBitSet(&pThis->au32DirtyBitmap[0], offVRAM >> PAGE_SHIFT);
    pThis->fHasDirtyBits = true;
}

/**
 * Tests if a VRAM page is dirty.
 *
 * @returns true if dirty.
 * @returns false if clean.
 * @param   pThis       VGA instance data.
 * @param   offVRAM     The VRAM offset of the page to check.
 */
DECLINLINE(bool) vga_is_dirty(VGAState *pThis, RTGCPHYS offVRAM)
{
    AssertMsg(offVRAM < pThis->vram_size, ("offVRAM = %p, pThis->vram_size = %p\n", offVRAM, pThis->vram_size));
    return ASMBitTest(&pThis->au32DirtyBitmap[0], offVRAM >> PAGE_SHIFT);
}

/**
 * Reset dirty flags in a give range.
 *
 * @param   pThis           VGA instance data.
 * @param   offVRAMStart    Offset into the VRAM buffer of the first page.
 * @param   offVRAMEnd      Offset into the VRAM buffer of the last page - exclusive.
 */
DECLINLINE(void) vga_reset_dirty(VGAState *pThis, RTGCPHYS offVRAMStart, RTGCPHYS offVRAMEnd)
{
    Assert(offVRAMStart < pThis->vram_size);
    Assert(offVRAMEnd <= pThis->vram_size);
    Assert(offVRAMStart < offVRAMEnd);
    ASMBitClearRange(&pThis->au32DirtyBitmap[0], offVRAMStart >> PAGE_SHIFT, offVRAMEnd >> PAGE_SHIFT);
}

/* force some bits to zero */
static const uint8_t sr_mask[8] = {
    (uint8_t)~0xfc,
    (uint8_t)~0xc2,
    (uint8_t)~0xf0,
    (uint8_t)~0xc0,
    (uint8_t)~0xf1,
    (uint8_t)~0xff,
    (uint8_t)~0xff,
    (uint8_t)~0x01,
};

static const uint8_t gr_mask[16] = {
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

#if defined(IN_RING3)
static uint32_t expand4[256];
static uint16_t expand2[256];
static uint8_t expand4to8[16];
#endif /* IN_RING3 */

/* Update the values needed for calculating Vertical Retrace and
 * Display Enable status bits more or less accurately. The Display Enable
 * bit is set (indicating *disabled* display signal) when either the
 * horizontal (hblank) or vertical (vblank) blanking is active. The
 * Vertical Retrace bit is set when vertical retrace (vsync) is active.
 * Unless the CRTC is horribly misprogrammed, vsync implies vblank.
 */
static void vga_update_retrace_state(VGAState *pThis)
{
    unsigned        htotal_cclks, vtotal_lines, chars_per_sec;
    unsigned        hblank_start_cclk, hblank_end_cclk, hblank_width, hblank_skew_cclks;
    unsigned        vsync_start_line, vsync_end, vsync_width;
    unsigned        vblank_start_line, vblank_end, vblank_width;
    unsigned        char_dots, clock_doubled, clock_index;
    const int       clocks[] = {25175000, 28322000, 25175000, 25175000};
    vga_retrace_s   *r = &pThis->retrace_state;

    /* For horizontal timings, we only care about the blanking start/end. */
    htotal_cclks = pThis->cr[0x00] + 5;
    hblank_start_cclk = pThis->cr[0x02];
    hblank_end_cclk = (pThis->cr[0x03] & 0x1f) + ((pThis->cr[0x05] & 0x80) >> 2);
    hblank_skew_cclks = (pThis->cr[0x03] >> 5) & 3;

    /* For vertical timings, we need both the blanking start/end... */
    vtotal_lines = pThis->cr[0x06] + ((pThis->cr[0x07] & 1) << 8) + ((pThis->cr[0x07] & 0x20) << 4) + 2;
    vblank_start_line = pThis->cr[0x15] + ((pThis->cr[0x07] & 8) << 5) + ((pThis->cr[0x09] & 0x20) << 4);
    vblank_end = pThis->cr[0x16];
    /* ... and the vertical retrace (vsync) start/end. */
    vsync_start_line = pThis->cr[0x10] + ((pThis->cr[0x07] & 4) << 6) + ((pThis->cr[0x07] & 0x80) << 2);
    vsync_end = pThis->cr[0x11] & 0xf;

    /* Calculate the blanking and sync widths. The way it's implemented in
     * the VGA with limited-width compare counters is quite a piece of work.
     */
    hblank_width = (hblank_end_cclk - hblank_start_cclk) & 0x3f;/* 6 bits */
    vblank_width = (vblank_end - vblank_start_line) & 0xff;     /* 8 bits */
    vsync_width  = (vsync_end - vsync_start_line) & 0xf;        /* 4 bits */

    /* Calculate the dot and character clock rates. */
    clock_doubled = (pThis->sr[0x01] >> 3) & 1; /* Clock doubling bit. */
    clock_index = (pThis->msr >> 2) & 3;
    char_dots = (pThis->sr[0x01] & 1) ? 8 : 9;  /* 8 or 9 dots per cclk. */

    chars_per_sec = clocks[clock_index] / char_dots;
    Assert(chars_per_sec);  /* Can't possibly be zero. */

    htotal_cclks <<= clock_doubled;

    /* Calculate the number of cclks per entire frame. */
    r->frame_cclks = vtotal_lines * htotal_cclks;
    Assert(r->frame_cclks); /* Can't possibly be zero. */

    if (r->v_freq_hz) { /* Could be set to emulate a specific rate. */
        r->cclk_ns = 1000000000 / (r->frame_cclks * r->v_freq_hz);
    } else {
        r->cclk_ns = 1000000000 / chars_per_sec;
    }
    Assert(r->cclk_ns);
    r->frame_ns = r->frame_cclks * r->cclk_ns;

    /* Calculate timings in cclks/lines. Stored but not directly used. */
    r->hb_start = hblank_start_cclk + hblank_skew_cclks;
    r->hb_end   = hblank_start_cclk + hblank_width + hblank_skew_cclks;
    r->h_total  = htotal_cclks;
    Assert(r->h_total);     /* Can't possibly be zero. */

    r->vb_start = vblank_start_line;
    r->vb_end   = vblank_start_line + vblank_width + 1;
    r->vs_start = vsync_start_line;
    r->vs_end   = vsync_start_line + vsync_width + 1;

    /* Calculate timings in nanoseconds. For easier comparisons, the frame
     * is considered to start at the beginning of the vertical and horizontal
     * blanking period.
     */
    r->h_total_ns  = htotal_cclks * r->cclk_ns;
    r->hb_end_ns   = hblank_width * r->cclk_ns;
    r->vb_end_ns   = vblank_width * r->h_total_ns;
    r->vs_start_ns = (r->vs_start - r->vb_start) * r->h_total_ns;
    r->vs_end_ns   = (r->vs_end   - r->vb_start) * r->h_total_ns;
    Assert(r->h_total_ns);  /* See h_total. */
}

static uint8_t vga_retrace(VGAState *pThis)
{
    vga_retrace_s   *r = &pThis->retrace_state;

    if (r->frame_ns) {
        uint8_t     val = pThis->st01 & ~(ST01_V_RETRACE | ST01_DISP_ENABLE);
        unsigned    cur_frame_ns, cur_line_ns;
        uint64_t    time_ns;

        time_ns = PDMDevHlpTMTimeVirtGetNano(VGASTATE2DEVINS(pThis));

        /* Determine the time within the frame. */
        cur_frame_ns = time_ns % r->frame_ns;

        /* See if we're in the vertical blanking period... */
        if (cur_frame_ns < r->vb_end_ns) {
            val |= ST01_DISP_ENABLE;
            /* ... and additionally in the vertical sync period. */
            if (cur_frame_ns >= r->vs_start_ns && cur_frame_ns <= r->vs_end_ns)
                val |= ST01_V_RETRACE;
        } else {
            /* Determine the time within the current scanline. */
            cur_line_ns = cur_frame_ns % r->h_total_ns;
            /* See if we're in the horizontal blanking period. */
            if (cur_line_ns < r->hb_end_ns)
                val |= ST01_DISP_ENABLE;
        }
        return val;
    } else {
        return pThis->st01 ^ (ST01_V_RETRACE | ST01_DISP_ENABLE);
    }
}

int vga_ioport_invalid(VGAState *pThis, uint32_t addr)
{
    if (pThis->msr & MSR_COLOR_EMULATION) {
        /* Color */
        return (addr >= 0x3b0 && addr <= 0x3bf);
    } else {
        /* Monochrome */
        return (addr >= 0x3d0 && addr <= 0x3df);
    }
}

static uint32_t vga_ioport_read(void *opaque, uint32_t addr)
{
    VGAState *pThis = (VGAState*)opaque;
    int val, index;

    /* check port range access depending on color/monochrome mode */
    if (vga_ioport_invalid(pThis, addr)) {
        val = 0xff;
        Log(("VGA: following read ignored\n"));
    } else {
        switch(addr) {
        case 0x3c0:
            if (pThis->ar_flip_flop == 0) {
                val = pThis->ar_index;
            } else {
                val = 0;
            }
            break;
        case 0x3c1:
            index = pThis->ar_index & 0x1f;
            if (index < 21)
                val = pThis->ar[index];
            else
                val = 0;
            break;
        case 0x3c2:
            val = pThis->st00;
            break;
        case 0x3c4:
            val = pThis->sr_index;
            break;
        case 0x3c5:
            val = pThis->sr[pThis->sr_index];
            Log2(("vga: read SR%x = 0x%02x\n", pThis->sr_index, val));
            break;
        case 0x3c7:
            val = pThis->dac_state;
            break;
        case 0x3c8:
            val = pThis->dac_write_index;
            break;
        case 0x3c9:
            val = pThis->palette[pThis->dac_read_index * 3 + pThis->dac_sub_index];
            if (++pThis->dac_sub_index == 3) {
                pThis->dac_sub_index = 0;
                pThis->dac_read_index++;
            }
            break;
        case 0x3ca:
            val = pThis->fcr;
            break;
        case 0x3cc:
            val = pThis->msr;
            break;
        case 0x3ce:
            val = pThis->gr_index;
            break;
        case 0x3cf:
            val = pThis->gr[pThis->gr_index];
            Log2(("vga: read GR%x = 0x%02x\n", pThis->gr_index, val));
            break;
        case 0x3b4:
        case 0x3d4:
            val = pThis->cr_index;
            break;
        case 0x3b5:
        case 0x3d5:
            val = pThis->cr[pThis->cr_index];
            Log2(("vga: read CR%x = 0x%02x\n", pThis->cr_index, val));
            break;
        case 0x3ba:
        case 0x3da:
            val = pThis->st01 = vga_retrace(pThis);
            pThis->ar_flip_flop = 0;
            break;
        default:
            val = 0x00;
            break;
        }
    }
    Log(("VGA: read addr=0x%04x data=0x%02x\n", addr, val));
    return val;
}

static void vga_ioport_write(void *opaque, uint32_t addr, uint32_t val)
{
    VGAState *pThis = (VGAState*)opaque;
    int index;

    Log(("VGA: write addr=0x%04x data=0x%02x\n", addr, val));

    /* check port range access depending on color/monochrome mode */
    if (vga_ioport_invalid(pThis, addr)) {
        Log(("VGA: previous write ignored\n"));
        return;
    }

    switch(addr) {
    case 0x3c0:
        if (pThis->ar_flip_flop == 0) {
            val &= 0x3f;
            pThis->ar_index = val;
        } else {
            index = pThis->ar_index & 0x1f;
            switch(index) {
            case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
            case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
                pThis->ar[index] = val & 0x3f;
                break;
            case 0x10:
                pThis->ar[index] = val & ~0x10;
                break;
            case 0x11:
                pThis->ar[index] = val;
                break;
            case 0x12:
                pThis->ar[index] = val & ~0xc0;
                break;
            case 0x13:
                pThis->ar[index] = val & ~0xf0;
                break;
            case 0x14:
                pThis->ar[index] = val & ~0xf0;
                break;
            default:
                break;
            }
        }
        pThis->ar_flip_flop ^= 1;
        break;
    case 0x3c2:
        pThis->msr = val & ~0x10;
        if (pThis->fRealRetrace)
            vga_update_retrace_state(pThis);
        pThis->st00 = (pThis->st00 & ~0x10) | (0x90 >> ((val >> 2) & 0x3));
        break;
    case 0x3c4:
        pThis->sr_index = val & 7;
        break;
    case 0x3c5:
        Log2(("vga: write SR%x = 0x%02x\n", pThis->sr_index, val));
        pThis->sr[pThis->sr_index] = val & sr_mask[pThis->sr_index];
        /* Allow SR07 to disable VBE. */
        if (pThis->sr_index == 0x07 && !(val & 1))
        {
            pThis->vbe_regs[VBE_DISPI_INDEX_ENABLE] = VBE_DISPI_DISABLED;
            pThis->bank_offset = 0;
        }
        if (pThis->fRealRetrace && pThis->sr_index == 0x01)
            vga_update_retrace_state(pThis);
#ifndef IN_RC
        /* The VGA region is (could be) affected by this change; reset all aliases we've created. */
        if (    pThis->sr_index == 4 /* mode */
            ||  pThis->sr_index == 2 /* plane mask */)
        {
            if (pThis->fRemappedVGA)
            {
                IOMMMIOResetRegion(PDMDevHlpGetVM(pThis->CTX_SUFF(pDevIns)), 0x000a0000);
                pThis->fRemappedVGA = false;
            }
        }
#endif
        break;
    case 0x3c7:
        pThis->dac_read_index = val;
        pThis->dac_sub_index = 0;
        pThis->dac_state = 3;
        break;
    case 0x3c8:
        pThis->dac_write_index = val;
        pThis->dac_sub_index = 0;
        pThis->dac_state = 0;
        break;
    case 0x3c9:
        pThis->dac_cache[pThis->dac_sub_index] = val;
        if (++pThis->dac_sub_index == 3) {
            memcpy(&pThis->palette[pThis->dac_write_index * 3], pThis->dac_cache, 3);
            pThis->dac_sub_index = 0;
            pThis->dac_write_index++;
        }
        break;
    case 0x3ce:
        pThis->gr_index = val & 0x0f;
        break;
    case 0x3cf:
        Log2(("vga: write GR%x = 0x%02x\n", pThis->gr_index, val));
        pThis->gr[pThis->gr_index] = val & gr_mask[pThis->gr_index];

#ifndef IN_RC
        /* The VGA region is (could be) affected by this change; reset all aliases we've created. */
        if (pThis->gr_index == 6 /* memory map mode */)
        {
            if (pThis->fRemappedVGA)
            {
                IOMMMIOResetRegion(PDMDevHlpGetVM(pThis->CTX_SUFF(pDevIns)), 0x000a0000);
                pThis->fRemappedVGA = false;
            }
        }
#endif
        break;

    case 0x3b4:
    case 0x3d4:
        pThis->cr_index = val;
        break;
    case 0x3b5:
    case 0x3d5:
        Log2(("vga: write CR%x = 0x%02x\n", pThis->cr_index, val));
        /* handle CR0-7 protection */
        if ((pThis->cr[0x11] & 0x80) && pThis->cr_index <= 7) {
            /* can always write bit 4 of CR7 */
            if (pThis->cr_index == 7)
                pThis->cr[7] = (pThis->cr[7] & ~0x10) | (val & 0x10);
            return;
        }
        pThis->cr[pThis->cr_index] = val;

        if (pThis->fRealRetrace) {
            /* The following registers are only updated during a mode set. */
            switch(pThis->cr_index) {
            case 0x00:
            case 0x02:
            case 0x03:
            case 0x05:
            case 0x06:
            case 0x07:
            case 0x09:
            case 0x10:
            case 0x11:
            case 0x15:
            case 0x16:
                vga_update_retrace_state(pThis);
                break;
            }
        }
        break;
    case 0x3ba:
    case 0x3da:
        pThis->fcr = val & 0x10;
        break;
    }
}

#ifdef CONFIG_BOCHS_VBE
static uint32_t vbe_ioport_read_index(void *opaque, uint32_t addr)
{
    VGAState *pThis = (VGAState*)opaque;
    uint32_t val = pThis->vbe_index;
    NOREF(addr);
    return val;
}

static uint32_t vbe_ioport_read_data(void *opaque, uint32_t addr)
{
    VGAState *pThis = (VGAState*)opaque;
    uint32_t val;
    NOREF(addr);

    if (pThis->vbe_index < VBE_DISPI_INDEX_NB) {
      if (pThis->vbe_regs[VBE_DISPI_INDEX_ENABLE] & VBE_DISPI_GETCAPS) {
          switch(pThis->vbe_index) {
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
                Assert(pThis->vbe_index < VBE_DISPI_INDEX_NB);
                val = pThis->vbe_regs[pThis->vbe_index];
                break;
          }
      } else {
          switch(pThis->vbe_index) {
          case VBE_DISPI_INDEX_VBOX_VIDEO:
              /* Reading from the port means that the old additions are requesting the number of monitors. */
              val = 1;
              break;
          default:
              Assert(pThis->vbe_index < VBE_DISPI_INDEX_NB);
              val = pThis->vbe_regs[pThis->vbe_index];
              break;
          }
      }
    } else {
        val = 0;
    }
    Log(("VBE: read index=0x%x val=0x%x\n", pThis->vbe_index, val));
    return val;
}

#define VBE_PITCH_ALIGN     4       /* Align pitch to 32 bits - Qt requires that. */

/* Calculate scanline pitch based on bit depth and width in pixels. */
static uint32_t calc_line_pitch(uint16_t bpp, uint16_t width)
{
    uint32_t    pitch, aligned_pitch;

    if (bpp <= 4)
        pitch = width >> 1;
    else
        pitch = width * ((bpp + 7) >> 3);

    /* Align the pitch to some sensible value. */
    aligned_pitch = (pitch + (VBE_PITCH_ALIGN - 1)) & ~(VBE_PITCH_ALIGN - 1);
    if (aligned_pitch != pitch)
        Log(("VBE: Line pitch %d aligned to %d bytes\n", pitch, aligned_pitch));

    return aligned_pitch;
}

#ifdef SOME_UNUSED_FUNCTION
/* Calculate line width in pixels based on bit depth and pitch. */
static uint32_t calc_line_width(uint16_t bpp, uint32_t pitch)
{
    uint32_t    width;

    if (bpp <= 4)
        width = pitch << 1;
    else
        width = pitch / ((bpp + 7) >> 3);

    return width;
}
#endif

static void recalculate_data(VGAState *pThis, bool fVirtHeightOnly)
{
    uint16_t cBPP        = pThis->vbe_regs[VBE_DISPI_INDEX_BPP];
    uint16_t cVirtWidth  = pThis->vbe_regs[VBE_DISPI_INDEX_VIRT_WIDTH];
    uint16_t cX          = pThis->vbe_regs[VBE_DISPI_INDEX_XRES];
    if (!cBPP || !cX)
        return;  /* Not enough data has been set yet. */
    uint32_t cbLinePitch = calc_line_pitch(cBPP, cVirtWidth);
    if (!cbLinePitch)
        cbLinePitch      = calc_line_pitch(cBPP, cX);
    Assert(cbLinePitch != 0);
    uint32_t cVirtHeight = pThis->vram_size / cbLinePitch;
    if (!fVirtHeightOnly)
    {
        uint16_t offX        = pThis->vbe_regs[VBE_DISPI_INDEX_X_OFFSET];
        uint16_t offY        = pThis->vbe_regs[VBE_DISPI_INDEX_Y_OFFSET];
        uint32_t offStart    = cbLinePitch * offY;
        if (cBPP == 4)
            offStart += offX >> 1;
        else
            offStart += offX * ((cBPP + 7) >> 3);
        offStart >>= 2;
        pThis->vbe_line_offset = RT_MIN(cbLinePitch, pThis->vram_size);
        pThis->vbe_start_addr  = RT_MIN(offStart, pThis->vram_size);
    }

    /* The VBE_DISPI_INDEX_VIRT_HEIGHT is used to prevent setting resolution bigger than VRAM permits
     * it is used instead of VBE_DISPI_INDEX_YRES *only* in case
     * pThis->vbe_regs[VBE_DISPI_INDEX_VIRT_HEIGHT] < pThis->vbe_regs[VBE_DISPI_INDEX_YRES]
     * We can not simply do pThis->vbe_regs[VBE_DISPI_INDEX_VIRT_HEIGHT] = cVirtHeight since
     * the cVirtHeight we calculated can exceed the 16bit value range
     * instead we'll check if it's bigger than pThis->vbe_regs[VBE_DISPI_INDEX_YRES], and if yes,
     * assign the pThis->vbe_regs[VBE_DISPI_INDEX_VIRT_HEIGHT] with a dummy UINT16_MAX value
     * that is always bigger than pThis->vbe_regs[VBE_DISPI_INDEX_YRES]
     * to just ensure the pThis->vbe_regs[VBE_DISPI_INDEX_YRES] is always used */
    pThis->vbe_regs[VBE_DISPI_INDEX_VIRT_HEIGHT] = (cVirtHeight >= (uint32_t)pThis->vbe_regs[VBE_DISPI_INDEX_YRES])
                                                 ? UINT16_MAX : (uint16_t)cVirtHeight;
}

static void vbe_ioport_write_index(void *opaque, uint32_t addr, uint32_t val)
{
    VGAState *pThis = (VGAState*)opaque;
    pThis->vbe_index = val;
    NOREF(addr);
}

static int vbe_ioport_write_data(void *opaque, uint32_t addr, uint32_t val)
{
    VGAState *pThis = (VGAState*)opaque;
    uint32_t max_bank;
    NOREF(addr);

    if (pThis->vbe_index <= VBE_DISPI_INDEX_NB) {
        bool fRecalculate = false;
        Log(("VBE: write index=0x%x val=0x%x\n", pThis->vbe_index, val));
        switch(pThis->vbe_index) {
        case VBE_DISPI_INDEX_ID:
            if (val == VBE_DISPI_ID0 ||
                val == VBE_DISPI_ID1 ||
                val == VBE_DISPI_ID2 ||
                val == VBE_DISPI_ID3 ||
                val == VBE_DISPI_ID4) {
                pThis->vbe_regs[pThis->vbe_index] = val;
            }
            if (val == VBE_DISPI_ID_VBOX_VIDEO) {
                pThis->vbe_regs[pThis->vbe_index] = val;
            } else if (val == VBE_DISPI_ID_ANYX) {
                pThis->vbe_regs[pThis->vbe_index] = val;
            }
#ifdef VBOX_WITH_HGSMI
            else if (val == VBE_DISPI_ID_HGSMI) {
                pThis->vbe_regs[pThis->vbe_index] = val;
            }
#endif /* VBOX_WITH_HGSMI */
            break;
        case VBE_DISPI_INDEX_XRES:
            if (val <= VBE_DISPI_MAX_XRES)
            {
                pThis->vbe_regs[pThis->vbe_index] = val;
                pThis->vbe_regs[VBE_DISPI_INDEX_VIRT_WIDTH] = val;
                fRecalculate = true;
            }
            break;
        case VBE_DISPI_INDEX_YRES:
            if (val <= VBE_DISPI_MAX_YRES)
                pThis->vbe_regs[pThis->vbe_index] = val;
            break;
        case VBE_DISPI_INDEX_BPP:
            if (val == 0)
                val = 8;
            if (val == 4 || val == 8 || val == 15 ||
                val == 16 || val == 24 || val == 32) {
                pThis->vbe_regs[pThis->vbe_index] = val;
                fRecalculate = true;
            }
            break;
        case VBE_DISPI_INDEX_BANK:
            if (pThis->vbe_regs[VBE_DISPI_INDEX_BPP] <= 4)
                max_bank = pThis->vbe_bank_max >> 2;    /* Each bank really covers 256K */
            else
                max_bank = pThis->vbe_bank_max;
            /* Old software may pass garbage in the high byte of bank. If the maximum
             * bank fits into a single byte, toss the high byte the user supplied.
             */
            if (max_bank < 0x100)
                val &= 0xff;
            if (val > max_bank)
                val = max_bank;
            pThis->vbe_regs[pThis->vbe_index] = val;
            pThis->bank_offset = (val << 16);

#ifndef IN_RC
            /* The VGA region is (could be) affected by this change; reset all aliases we've created. */
            if (pThis->fRemappedVGA)
            {
                IOMMMIOResetRegion(PDMDevHlpGetVM(pThis->CTX_SUFF(pDevIns)), 0x000a0000);
                pThis->fRemappedVGA = false;
            }
#endif
            break;

        case VBE_DISPI_INDEX_ENABLE:
#ifndef IN_RING3
            return VINF_IOM_R3_IOPORT_WRITE;
#else
            if ((val & VBE_DISPI_ENABLED) &&
                !(pThis->vbe_regs[VBE_DISPI_INDEX_ENABLE] & VBE_DISPI_ENABLED)) {
                int h, shift_control;
                /* Check the values before we screw up with a resolution which is too big or small. */
                size_t cb = pThis->vbe_regs[VBE_DISPI_INDEX_XRES];
                if (pThis->vbe_regs[VBE_DISPI_INDEX_BPP] == 4)
                    cb = pThis->vbe_regs[VBE_DISPI_INDEX_XRES] >> 1;
                else
                    cb = pThis->vbe_regs[VBE_DISPI_INDEX_XRES] * ((pThis->vbe_regs[VBE_DISPI_INDEX_BPP] + 7) >> 3);
                cb *= pThis->vbe_regs[VBE_DISPI_INDEX_YRES];
                uint16_t cVirtWidth = pThis->vbe_regs[VBE_DISPI_INDEX_VIRT_WIDTH];
                if (!cVirtWidth)
                    cVirtWidth = pThis->vbe_regs[VBE_DISPI_INDEX_XRES];
                if (    !cVirtWidth
                    ||  !pThis->vbe_regs[VBE_DISPI_INDEX_YRES]
                    ||  cb > pThis->vram_size)
                {
                    AssertMsgFailed(("VIRT WIDTH=%d YRES=%d cb=%d vram_size=%d\n",
                                     pThis->vbe_regs[VBE_DISPI_INDEX_VIRT_WIDTH], pThis->vbe_regs[VBE_DISPI_INDEX_YRES], cb, pThis->vram_size));
                    return VINF_SUCCESS; /* Note: silent failure like before */
                }

                /* When VBE interface is enabled, it is reset. */
                pThis->vbe_regs[VBE_DISPI_INDEX_X_OFFSET] = 0;
                pThis->vbe_regs[VBE_DISPI_INDEX_Y_OFFSET] = 0;
                fRecalculate = true;

                /* clear the screen (should be done in BIOS) */
                if (!(val & VBE_DISPI_NOCLEARMEM)) {
                    uint16_t cY = RT_MIN(pThis->vbe_regs[VBE_DISPI_INDEX_YRES],
                                         pThis->vbe_regs[VBE_DISPI_INDEX_VIRT_HEIGHT]);
                    uint16_t cbLinePitch = pThis->vbe_line_offset;
                    memset(pThis->CTX_SUFF(vram_ptr), 0,
                           cY * cbLinePitch);
                }

                /* we initialize the VGA graphic mode (should be done
                   in BIOS) */
                pThis->gr[0x06] = (pThis->gr[0x06] & ~0x0c) | 0x05; /* graphic mode + memory map 1 */
                pThis->cr[0x17] |= 3; /* no CGA modes */
                pThis->cr[0x13] = pThis->vbe_line_offset >> 3;
                /* width */
                pThis->cr[0x01] = (cVirtWidth >> 3) - 1;
                /* height (only meaningful if < 1024) */
                h = pThis->vbe_regs[VBE_DISPI_INDEX_YRES] - 1;
                pThis->cr[0x12] = h;
                pThis->cr[0x07] = (pThis->cr[0x07] & ~0x42) |
                    ((h >> 7) & 0x02) | ((h >> 3) & 0x40);
                /* line compare to 1023 */
                pThis->cr[0x18] = 0xff;
                pThis->cr[0x07] |= 0x10;
                pThis->cr[0x09] |= 0x40;

                if (pThis->vbe_regs[VBE_DISPI_INDEX_BPP] == 4) {
                    shift_control = 0;
                    pThis->sr[0x01] &= ~8; /* no double line */
                } else {
                    shift_control = 2;
                    pThis->sr[4] |= 0x08; /* set chain 4 mode */
                    pThis->sr[2] |= 0x0f; /* activate all planes */
                    /* Indicate non-VGA mode in SR07. */
                    pThis->sr[7] |= 1;
                }
                pThis->gr[0x05] = (pThis->gr[0x05] & ~0x60) | (shift_control << 5);
                pThis->cr[0x09] &= ~0x9f; /* no double scan */
                /* sunlover 30.05.2007
                 * The ar_index remains with bit 0x20 cleared after a switch from fullscreen
                 * DOS mode on Windows XP guest. That leads to GMODE_BLANK in vga_update_display.
                 * But the VBE mode is graphics, so not a blank anymore.
                 */
                pThis->ar_index |= 0x20;
            } else {
                /* XXX: the bios should do that */
                /* sunlover 21.12.2006
                 * Here is probably more to reset. When this was executed in GC
                 * then the *update* functions could not detect a mode change.
                 * Or may be these update function should take the pThis->vbe_regs[pThis->vbe_index]
                 * into account when detecting a mode change.
                 *
                 * The 'mode reset not detected' problem is now fixed by executing the
                 * VBE_DISPI_INDEX_ENABLE case always in RING3 in order to call the
                 * LFBChange callback.
                 */
                pThis->bank_offset = 0;
            }
            pThis->vbe_regs[pThis->vbe_index] = val;
            /*
             * LFB video mode is either disabled or changed. This notification
             * is used by the display to disable VBVA.
             */
            pThis->pDrv->pfnLFBModeChange(pThis->pDrv, (val & VBE_DISPI_ENABLED) != 0);

            /* The VGA region is (could be) affected by this change; reset all aliases we've created. */
            if (pThis->fRemappedVGA)
            {
                IOMMMIOResetRegion(PDMDevHlpGetVM(pThis->CTX_SUFF(pDevIns)), 0x000a0000);
                pThis->fRemappedVGA = false;
            }
            break;
#endif /* IN_RING3 */
        case VBE_DISPI_INDEX_VIRT_WIDTH:
        case VBE_DISPI_INDEX_X_OFFSET:
        case VBE_DISPI_INDEX_Y_OFFSET:
            {
                pThis->vbe_regs[pThis->vbe_index] = val;
                fRecalculate = true;
            }
            break;
        case VBE_DISPI_INDEX_VBOX_VIDEO:
#ifndef IN_RING3
            return VINF_IOM_R3_IOPORT_WRITE;
#else
            /* Changes in the VGA device are minimal. The device is bypassed. The driver does all work. */
            if (val == VBOX_VIDEO_DISABLE_ADAPTER_MEMORY)
            {
                pThis->pDrv->pfnProcessAdapterData(pThis->pDrv, NULL, 0);
            }
            else if (val == VBOX_VIDEO_INTERPRET_ADAPTER_MEMORY)
            {
                pThis->pDrv->pfnProcessAdapterData(pThis->pDrv, pThis->CTX_SUFF(vram_ptr), pThis->vram_size);
            }
            else if ((val & 0xFFFF0000) == VBOX_VIDEO_INTERPRET_DISPLAY_MEMORY_BASE)
            {
                pThis->pDrv->pfnProcessDisplayData(pThis->pDrv, pThis->CTX_SUFF(vram_ptr), val & 0xFFFF);
            }
#endif /* IN_RING3 */
            break;
        default:
            break;
        }
        if (fRecalculate)
        {
            recalculate_data(pThis, false);
        }
    }
    return VINF_SUCCESS;
}
#endif

/* called for accesses between 0xa0000 and 0xc0000 */
static uint32_t vga_mem_readb(void *opaque, target_phys_addr_t addr, int *prc)
{
    VGAState *pThis = (VGAState*)opaque;
    int memory_map_mode, plane;
    uint32_t ret;

    Log3(("vga: read [0x%x] -> ", addr));
    /* convert to VGA memory offset */
    memory_map_mode = (pThis->gr[6] >> 2) & 3;
#ifndef IN_RC
    RTGCPHYS GCPhys = addr; /* save original address */
#endif

    addr &= 0x1ffff;
    switch(memory_map_mode) {
    case 0:
        break;
    case 1:
        if (addr >= 0x10000)
            return 0xff;
        addr += pThis->bank_offset;
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

    if (pThis->sr[4] & 0x08) {
        /* chain 4 mode : simplest access */
# ifndef IN_RC
        /* If all planes are accessible, then map the page to the frame buffer and make it writable. */
        if (   (pThis->sr[2] & 3) == 3
            && !vga_is_dirty(pThis, addr))
        {
            /** @todo only allow read access (doesn't work now) */
            STAM_COUNTER_INC(&pThis->StatMapPage);
            IOMMMIOMapMMIO2Page(PDMDevHlpGetVM(pThis->CTX_SUFF(pDevIns)), GCPhys, pThis->GCPhysVRAM + addr, X86_PTE_RW|X86_PTE_P);
            /* Set as dirty as write accesses won't be noticed now. */
            vga_set_dirty(pThis, addr);
            pThis->fRemappedVGA = true;
        }
# endif /* IN_RC */
        VERIFY_VRAM_READ_OFF_RETURN(pThis, addr, *prc);
        ret = pThis->CTX_SUFF(vram_ptr)[addr];
    } else if (!(pThis->sr[4] & 0x04)) {    /* Host access is controlled by SR4, not GR5! */
        /* odd/even mode (aka text mode mapping) */
        plane = (pThis->gr[4] & 2) | (addr & 1);
        /* See the comment for a similar line in vga_mem_writeb. */
        RTGCPHYS off = ((addr & ~1) << 2) | plane;
        VERIFY_VRAM_READ_OFF_RETURN(pThis, off, *prc);
        ret = pThis->CTX_SUFF(vram_ptr)[off];
    } else {
        /* standard VGA latched access */
        VERIFY_VRAM_READ_OFF_RETURN(pThis, addr, *prc);
        pThis->latch = ((uint32_t *)pThis->CTX_SUFF(vram_ptr))[addr];

        if (!(pThis->gr[5] & 0x08)) {
            /* read mode 0 */
            plane = pThis->gr[4];
            ret = GET_PLANE(pThis->latch, plane);
        } else {
            /* read mode 1 */
            ret = (pThis->latch ^ mask16[pThis->gr[2]]) & mask16[pThis->gr[7]];
            ret |= ret >> 16;
            ret |= ret >> 8;
            ret = (~ret) & 0xff;
        }
    }
    Log3((" 0x%02x\n", ret));
    return ret;
}

/* called for accesses between 0xa0000 and 0xc0000 */
static int vga_mem_writeb(void *opaque, target_phys_addr_t addr, uint32_t val)
{
    VGAState *pThis = (VGAState*)opaque;
    int memory_map_mode, plane, write_mode, b, func_select, mask;
    uint32_t write_mask, bit_mask, set_mask;

    Log3(("vga: [0x%x] = 0x%02x\n", addr, val));
    /* convert to VGA memory offset */
    memory_map_mode = (pThis->gr[6] >> 2) & 3;
#ifndef IN_RC
    RTGCPHYS GCPhys = addr; /* save original address */
#endif

    addr &= 0x1ffff;
    switch(memory_map_mode) {
    case 0:
        break;
    case 1:
        if (addr >= 0x10000)
            return VINF_SUCCESS;
        addr += pThis->bank_offset;
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

    if (pThis->sr[4] & 0x08) {
        /* chain 4 mode : simplest access */
        plane = addr & 3;
        mask = (1 << plane);
        if (pThis->sr[2] & mask) {
# ifndef IN_RC
            /* If all planes are accessible, then map the page to the frame buffer and make it writable. */
            if (   (pThis->sr[2] & 3) == 3
                && !vga_is_dirty(pThis, addr))
            {
                STAM_COUNTER_INC(&pThis->StatMapPage);
                IOMMMIOMapMMIO2Page(PDMDevHlpGetVM(pThis->CTX_SUFF(pDevIns)), GCPhys, pThis->GCPhysVRAM + addr, X86_PTE_RW | X86_PTE_P);
                pThis->fRemappedVGA = true;
            }
# endif /* IN_RC */

            VERIFY_VRAM_WRITE_OFF_RETURN(pThis, addr);
            pThis->CTX_SUFF(vram_ptr)[addr] = val;
            Log3(("vga: chain4: [0x%x]\n", addr));
            pThis->plane_updated |= mask; /* only used to detect font change */
            vga_set_dirty(pThis, addr);
        }
    } else if (!(pThis->sr[4] & 0x04)) {    /* Host access is controlled by SR4, not GR5! */
        /* odd/even mode (aka text mode mapping) */
        plane = (pThis->gr[4] & 2) | (addr & 1);
        mask = (1 << plane);
        if (pThis->sr[2] & mask) {
            /* 'addr' is offset in a plane, bit 0 selects the plane.
             * Mask the bit 0, convert plane index to vram offset,
             * that is multiply by the number of planes,
             * and select the plane byte in the vram offset.
             */
            addr = ((addr & ~1) << 2) | plane;
            VERIFY_VRAM_WRITE_OFF_RETURN(pThis, addr);
            pThis->CTX_SUFF(vram_ptr)[addr] = val;
            Log3(("vga: odd/even: [0x%x]\n", addr));
            pThis->plane_updated |= mask; /* only used to detect font change */
            vga_set_dirty(pThis, addr);
        }
    } else {
        /* standard VGA latched access */
        VERIFY_VRAM_WRITE_OFF_RETURN(pThis, addr * 4 + 3);

#ifdef IN_RING0
        if (((++pThis->cLatchAccesses) & pThis->uMaskLatchAccess) == pThis->uMaskLatchAccess)
        {
            static uint32_t const s_aMask[5]  = {   0x3ff,   0x1ff,    0x7f,    0x3f,   0x1f};
            static uint64_t const s_aDelta[5] = {10000000, 5000000, 2500000, 1250000, 625000};
            if (PDMDevHlpCanEmulateIoBlock(pThis->CTX_SUFF(pDevIns)))
            {
                uint64_t u64CurTime = RTTimeSystemNanoTS();

                /* About 1000 (or more) accesses per 10 ms will trigger a reschedule
                * to the recompiler
                */
                if (u64CurTime - pThis->u64LastLatchedAccess < s_aDelta[pThis->iMask])
                {
                    pThis->u64LastLatchedAccess = 0;
                    pThis->iMask                = RT_MIN(pThis->iMask + 1U, RT_ELEMENTS(s_aMask) - 1U);
                    pThis->uMaskLatchAccess     = s_aMask[pThis->iMask];
                    pThis->cLatchAccesses       = pThis->uMaskLatchAccess - 1;
                    return VINF_EM_RAW_EMULATE_IO_BLOCK;
                }
                if (pThis->u64LastLatchedAccess)
                {
                    Log2(("Reset mask (was %d) delta %RX64 (limit %x)\n", pThis->iMask, u64CurTime - pThis->u64LastLatchedAccess, s_aDelta[pThis->iMask]));
                    if (pThis->iMask)
                        pThis->iMask--;
                    pThis->uMaskLatchAccess     = s_aMask[pThis->iMask];
                }
                pThis->u64LastLatchedAccess = u64CurTime;
            }
            else
            {
                pThis->u64LastLatchedAccess = 0;
                pThis->iMask                = 0;
                pThis->uMaskLatchAccess     = s_aMask[pThis->iMask];
                pThis->cLatchAccesses       = 0;
            }
        }
#endif

        write_mode = pThis->gr[5] & 3;
        switch(write_mode) {
        default:
        case 0:
            /* rotate */
            b = pThis->gr[3] & 7;
            val = ((val >> b) | (val << (8 - b))) & 0xff;
            val |= val << 8;
            val |= val << 16;

            /* apply set/reset mask */
            set_mask = mask16[pThis->gr[1]];
            val = (val & ~set_mask) | (mask16[pThis->gr[0]] & set_mask);
            bit_mask = pThis->gr[8];
            break;
        case 1:
            val = pThis->latch;
            goto do_write;
        case 2:
            val = mask16[val & 0x0f];
            bit_mask = pThis->gr[8];
            break;
        case 3:
            /* rotate */
            b = pThis->gr[3] & 7;
            val = (val >> b) | (val << (8 - b));

            bit_mask = pThis->gr[8] & val;
            val = mask16[pThis->gr[0]];
            break;
        }

        /* apply logical operation */
        func_select = pThis->gr[3] >> 3;
        switch(func_select) {
        case 0:
        default:
            /* nothing to do */
            break;
        case 1:
            /* and */
            val &= pThis->latch;
            break;
        case 2:
            /* or */
            val |= pThis->latch;
            break;
        case 3:
            /* xor */
            val ^= pThis->latch;
            break;
        }

        /* apply bit mask */
        bit_mask |= bit_mask << 8;
        bit_mask |= bit_mask << 16;
        val = (val & bit_mask) | (pThis->latch & ~bit_mask);

    do_write:
        /* mask data according to sr[2] */
        mask = pThis->sr[2];
        pThis->plane_updated |= mask; /* only used to detect font change */
        write_mask = mask16[mask];
        ((uint32_t *)pThis->CTX_SUFF(vram_ptr))[addr] =
            (((uint32_t *)pThis->CTX_SUFF(vram_ptr))[addr] & ~write_mask) |
            (val & write_mask);
            Log3(("vga: latch: [0x%x] mask=0x%08x val=0x%08x\n",
                   addr * 4, write_mask, val));
            vga_set_dirty(pThis, (addr << 2));
    }

    return VINF_SUCCESS;
}

#if defined(IN_RING3)
typedef void vga_draw_glyph8_func(uint8_t *d, int linesize,
                             const uint8_t *font_ptr, int h,
                             uint32_t fgcol, uint32_t bgcol,
                             int dscan);
typedef void vga_draw_glyph9_func(uint8_t *d, int linesize,
                                  const uint8_t *font_ptr, int h,
                                  uint32_t fgcol, uint32_t bgcol, int dup9);
typedef void vga_draw_line_func(VGAState *s1, uint8_t *d,
                                const uint8_t *pThis, int width);

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
static bool update_palette16(VGAState *pThis)
{
    bool full_update = false;
    int i;
    uint32_t v, col, *palette;

    palette = pThis->last_palette;
    for(i = 0; i < 16; i++) {
        v = pThis->ar[i];
        if (pThis->ar[0x10] & 0x80)
            v = ((pThis->ar[0x14] & 0xf) << 4) | (v & 0xf);
        else
            v = ((pThis->ar[0x14] & 0xc) << 4) | (v & 0x3f);
        v = v * 3;
        col = pThis->rgb_to_pixel(c6_to_8(pThis->palette[v]),
                              c6_to_8(pThis->palette[v + 1]),
                              c6_to_8(pThis->palette[v + 2]));
        if (col != palette[i]) {
            full_update = true;
            palette[i] = col;
        }
    }
    return full_update;
}

/* return true if the palette was modified */
static bool update_palette256(VGAState *pThis)
{
    bool full_update = false;
    int i;
    uint32_t v, col, *palette;
    int wide_dac;

    palette = pThis->last_palette;
    v = 0;
    wide_dac = (pThis->vbe_regs[VBE_DISPI_INDEX_ENABLE] & (VBE_DISPI_ENABLED | VBE_DISPI_8BIT_DAC))
             == (VBE_DISPI_ENABLED | VBE_DISPI_8BIT_DAC);
    for(i = 0; i < 256; i++) {
        if (wide_dac)
            col = pThis->rgb_to_pixel(pThis->palette[v],
                                  pThis->palette[v + 1],
                                  pThis->palette[v + 2]);
        else
            col = pThis->rgb_to_pixel(c6_to_8(pThis->palette[v]),
                                  c6_to_8(pThis->palette[v + 1]),
                                  c6_to_8(pThis->palette[v + 2]));
        if (col != palette[i]) {
            full_update = true;
            palette[i] = col;
        }
        v += 3;
    }
    return full_update;
}

static void vga_get_offsets(VGAState *pThis,
                            uint32_t *pline_offset,
                            uint32_t *pstart_addr,
                            uint32_t *pline_compare)
{
    uint32_t start_addr, line_offset, line_compare;
#ifdef CONFIG_BOCHS_VBE
    if (pThis->vbe_regs[VBE_DISPI_INDEX_ENABLE] & VBE_DISPI_ENABLED) {
        line_offset = pThis->vbe_line_offset;
        start_addr = pThis->vbe_start_addr;
        line_compare = 65535;
    } else
#endif
    {
        /* compute line_offset in bytes */
        line_offset = pThis->cr[0x13];
        line_offset <<= 3;
        if (!(pThis->cr[0x14] & 0x40) && !(pThis->cr[0x17] & 0x40))
        {
            /* Word mode. Used for odd/even modes. */
            line_offset *= 2;
        }

        /* starting address */
        start_addr = pThis->cr[0x0d] | (pThis->cr[0x0c] << 8);

        /* line compare */
        line_compare = pThis->cr[0x18] |
            ((pThis->cr[0x07] & 0x10) << 4) |
            ((pThis->cr[0x09] & 0x40) << 3);
    }
    *pline_offset = line_offset;
    *pstart_addr = start_addr;
    *pline_compare = line_compare;
}

/* update start_addr and line_offset. Return TRUE if modified */
static bool update_basic_params(VGAState *pThis)
{
    bool full_update = false;
    uint32_t start_addr, line_offset, line_compare;

    pThis->get_offsets(pThis, &line_offset, &start_addr, &line_compare);

    if (line_offset != pThis->line_offset ||
        start_addr != pThis->start_addr ||
        line_compare != pThis->line_compare) {
        pThis->line_offset = line_offset;
        pThis->start_addr = start_addr;
        pThis->line_compare = line_compare;
        full_update = true;
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
 * - underline
 * - flashing
 */
static int vga_draw_text(VGAState *pThis, bool full_update, bool fFailOnResize, bool reset_dirty)
{
    int cx, cy, cheight, cw, ch, cattr, height, width, ch_attr;
    int cx_min, cx_max, linesize, x_incr;
    int cx_min_upd, cx_max_upd, cy_start;
    uint32_t offset, fgcol, bgcol, v, cursor_offset;
    uint8_t *d1, *d, *src, *s1, *dest, *cursor_ptr;
    const uint8_t *font_ptr, *font_base[2];
    int dup9, line_offset, depth_index, dscan;
    uint32_t *palette;
    uint32_t *ch_attr_ptr;
    vga_draw_glyph8_func *vga_draw_glyph8;
    vga_draw_glyph9_func *vga_draw_glyph9;

    full_update |= update_palette16(pThis);
    palette = pThis->last_palette;

    /* compute font data address (in plane 2) */
    v = pThis->sr[3];
    offset = (((v >> 4) & 1) | ((v << 1) & 6)) * 8192 * 4 + 2;
    if (offset != pThis->font_offsets[0]) {
        pThis->font_offsets[0] = offset;
        full_update = true;
    }
    font_base[0] = pThis->CTX_SUFF(vram_ptr) + offset;

    offset = (((v >> 5) & 1) | ((v >> 1) & 6)) * 8192 * 4 + 2;
    font_base[1] = pThis->CTX_SUFF(vram_ptr) + offset;
    if (offset != pThis->font_offsets[1]) {
        pThis->font_offsets[1] = offset;
        full_update = true;
    }
    if (pThis->plane_updated & (1 << 2)) {
        /* if the plane 2 was modified since the last display, it
           indicates the font may have been modified */
        pThis->plane_updated = 0;
        full_update = true;
    }
    full_update |= update_basic_params(pThis);

    line_offset = pThis->line_offset;
    s1 = pThis->CTX_SUFF(vram_ptr) + (pThis->start_addr * 8); /** @todo r=bird: Add comment why we do *8 instead of *4, it's not so obvious... */

    /* double scanning - not for 9-wide modes */
    dscan = (pThis->cr[9] >> 7) & 1;

    /* total width & height */
    cheight = (pThis->cr[9] & 0x1f) + 1;
    cw = 8;
    if (!(pThis->sr[1] & 0x01))
        cw = 9;
    if (pThis->sr[1] & 0x08)
        cw = 16; /* NOTE: no 18 pixel wide */
    x_incr = cw * ((pThis->pDrv->cBits + 7) >> 3);
    width = (pThis->cr[0x01] + 1);
    if (pThis->cr[0x06] == 100) {
        /* ugly hack for CGA 160x100x16 - explain me the logic */
        height = 100;
    } else {
        height = pThis->cr[0x12] |
            ((pThis->cr[0x07] & 0x02) << 7) |
            ((pThis->cr[0x07] & 0x40) << 3);
        height = (height + 1) / cheight;
    }
    if ((height * width) > CH_ATTR_SIZE) {
        /* better than nothing: exit if transient size is too big */
        return VINF_SUCCESS;
    }

    if (width != (int)pThis->last_width || height != (int)pThis->last_height ||
        cw != pThis->last_cw || cheight != pThis->last_ch) {
        if (fFailOnResize)
        {
            /* The caller does not want to call the pfnResize. */
            return VERR_TRY_AGAIN;
        }
        pThis->last_scr_width = width * cw;
        pThis->last_scr_height = height * cheight;
        /* For text modes the direct use of guest VRAM is not implemented, so bpp and cbLine are 0 here. */
        int rc = pThis->pDrv->pfnResize(pThis->pDrv, 0, NULL, 0, pThis->last_scr_width, pThis->last_scr_height);
        pThis->last_width = width;
        pThis->last_height = height;
        pThis->last_ch = cheight;
        pThis->last_cw = cw;
        full_update = true;
        if (rc == VINF_VGA_RESIZE_IN_PROGRESS)
            return rc;
        AssertRC(rc);
    }
    cursor_offset = ((pThis->cr[0x0e] << 8) | pThis->cr[0x0f]) - pThis->start_addr;
    if (cursor_offset != pThis->cursor_offset ||
        pThis->cr[0xa] != pThis->cursor_start ||
        pThis->cr[0xb] != pThis->cursor_end) {
      /* if the cursor position changed, we update the old and new
         chars */
        if (pThis->cursor_offset < CH_ATTR_SIZE)
            pThis->last_ch_attr[pThis->cursor_offset] = ~0;
        if (cursor_offset < CH_ATTR_SIZE)
            pThis->last_ch_attr[cursor_offset] = ~0;
        pThis->cursor_offset = cursor_offset;
        pThis->cursor_start = pThis->cr[0xa];
        pThis->cursor_end = pThis->cr[0xb];
    }
    cursor_ptr = pThis->CTX_SUFF(vram_ptr) + (pThis->start_addr + cursor_offset) * 8;
    depth_index = get_depth_index(pThis->pDrv->cBits);
    if (cw == 16)
        vga_draw_glyph8 = vga_draw_glyph16_table[depth_index];
    else
        vga_draw_glyph8 = vga_draw_glyph8_table[depth_index];
    vga_draw_glyph9 = vga_draw_glyph9_table[depth_index];

    dest = pThis->pDrv->pu8Data;
    linesize = pThis->pDrv->cbScanline;
    ch_attr_ptr = pThis->last_ch_attr;
    cy_start = -1;
    cx_max_upd = -1;
    cx_min_upd = width;

    for(cy = 0; cy < (height - dscan); cy = cy + (1 << dscan)) {
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
                if (reset_dirty)
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
                                    font_ptr, cheight, fgcol, bgcol, dscan);
                } else {
                    dup9 = 0;
                    if (ch >= 0xb0 && ch <= 0xdf && (pThis->ar[0x10] & 0x04))
                        dup9 = 1;
                    vga_draw_glyph9(d1, linesize,
                                    font_ptr, cheight, fgcol, bgcol, dup9);
                }
                if (src == cursor_ptr &&
                    !(pThis->cr[0x0a] & 0x20)) {
                    int line_start, line_last, h;
                    /* draw the cursor */
                    line_start = pThis->cr[0x0a] & 0x1f;
                    line_last = pThis->cr[0x0b] & 0x1f;
                    /* XXX: check that */
                    if (line_last > cheight - 1)
                        line_last = cheight - 1;
                    if (line_last >= line_start && line_start < cheight) {
                        h = line_last - line_start + 1;
                        d = d1 + (linesize * line_start << dscan);
                        if (cw != 9) {
                            vga_draw_glyph8(d, linesize,
                                            cursor_glyph, h, fgcol, bgcol, dscan);
                        } else {
                            vga_draw_glyph9(d, linesize,
                                            cursor_glyph, h, fgcol, bgcol, 1);
                        }
                    }
                }
            }
            d1 += x_incr;
            src += 8; /* Every second byte of a plane is used in text mode. */
            ch_attr_ptr++;
        }
        if (cx_max != -1) {
            /* Keep track of the bounding rectangle for updates. */
            if (cy_start == -1)
                cy_start = cy;
            if (cx_min_upd > cx_min)
                cx_min_upd = cx_min;
            if (cx_max_upd < cx_max)
                cx_max_upd = cx_max;
        } else if (cy_start >= 0) {
            /* Flush updates to display. */
            pThis->pDrv->pfnUpdateRect(pThis->pDrv, cx_min_upd * cw, cy_start * cheight,
                                       (cx_max_upd - cx_min_upd + 1) * cw, (cy - cy_start) * cheight);
            cy_start = -1;
            cx_max_upd = -1;
            cx_min_upd = width;
        }
        dest += linesize * cheight << dscan;
        s1 += line_offset;
    }
    if (cy_start >= 0)
        /* Flush any remaining changes to display. */
        pThis->pDrv->pfnUpdateRect(pThis->pDrv, cx_min_upd * cw, cy_start * cheight,
                                   (cx_max_upd - cx_min_upd + 1) * cw, (cy - cy_start) * cheight);
        return VINF_SUCCESS;
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

static int vga_get_bpp(VGAState *pThis)
{
    int ret;
#ifdef CONFIG_BOCHS_VBE
    if (pThis->vbe_regs[VBE_DISPI_INDEX_ENABLE] & VBE_DISPI_ENABLED) {
        ret = pThis->vbe_regs[VBE_DISPI_INDEX_BPP];
    } else
#endif
    {
        ret = 0;
    }
    return ret;
}

static void vga_get_resolution(VGAState *pThis, int *pwidth, int *pheight)
{
    int width, height;
#ifdef CONFIG_BOCHS_VBE
    if (pThis->vbe_regs[VBE_DISPI_INDEX_ENABLE] & VBE_DISPI_ENABLED) {
        width = pThis->vbe_regs[VBE_DISPI_INDEX_XRES];
        height = RT_MIN(pThis->vbe_regs[VBE_DISPI_INDEX_YRES],
                        pThis->vbe_regs[VBE_DISPI_INDEX_VIRT_HEIGHT]);
    } else
#endif
    {
        width = (pThis->cr[0x01] + 1) * 8;
        height = pThis->cr[0x12] |
            ((pThis->cr[0x07] & 0x02) << 7) |
            ((pThis->cr[0x07] & 0x40) << 3);
        height = (height + 1);
    }
    *pwidth = width;
    *pheight = height;
}

/**
 * Performs the display driver resizing when in graphics mode.
 *
 * This will recalc / update any status data depending on the driver
 * properties (bit depth mostly).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VINF_VGA_RESIZE_IN_PROGRESS if the operation wasn't complete.
 * @param   pThis   Pointer to the vga state.
 * @param   cx      The width.
 * @param   cy      The height.
 */
static int vga_resize_graphic(VGAState *pThis, int cx, int cy)
{
    const unsigned cBits = pThis->get_bpp(pThis);

    int rc;
    AssertReturn(cx, VERR_INVALID_PARAMETER);
    AssertReturn(cy, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertReturn(pThis->line_offset, VERR_INTERNAL_ERROR);

#if 0 //def VBOX_WITH_VDMA
    /* @todo: we get a second resize here when VBVA is on, while we actually should not */
    /* do not do pfnResize in case VBVA is on since all mode changes are performed over VBVA
     * we are checking for VDMA state here to ensure this code works only for WDDM driver,
     * although we should avoid calling pfnResize for XPDM as well, since pfnResize is actually an extra resize
     * event and generally only pfnVBVAxxx calls should be used with HGSMI + VBVA
     *
     * The reason for doing this for WDDM driver only now is to avoid regressions of the current code */
    PVBOXVDMAHOST pVdma = pThis->pVdma;
    if (pVdma && vboxVDMAIsEnabled(pVdma))
        rc = VINF_SUCCESS;
    else
#endif
    {
        /* Skip the resize if the values are not valid. */
        if (pThis->start_addr * 4 + pThis->line_offset * cy < pThis->vram_size)
            /* Take into account the programmed start address (in DWORDs) of the visible screen. */
            rc = pThis->pDrv->pfnResize(pThis->pDrv, cBits, pThis->CTX_SUFF(vram_ptr) + pThis->start_addr * 4, pThis->line_offset, cx, cy);
        else
        {
            /* Change nothing in the VGA state. Lets hope the guest will eventually programm correct values. */
            return VERR_TRY_AGAIN;
        }
    }

    /* last stuff */
    pThis->last_bpp = cBits;
    pThis->last_scr_width = cx;
    pThis->last_scr_height = cy;
    pThis->last_width = cx;
    pThis->last_height = cy;

    if (rc == VINF_VGA_RESIZE_IN_PROGRESS)
        return rc;
    AssertRC(rc);

    /* update palette */
    switch (pThis->pDrv->cBits)
    {
        case 32:    pThis->rgb_to_pixel = rgb_to_pixel32_dup; break;
        case 16:
        default:    pThis->rgb_to_pixel = rgb_to_pixel16_dup; break;
        case 15:    pThis->rgb_to_pixel = rgb_to_pixel15_dup; break;
        case 8:     pThis->rgb_to_pixel = rgb_to_pixel8_dup;  break;
    }
    if (pThis->shift_control == 0)
        update_palette16(pThis);
    else if (pThis->shift_control == 1)
        update_palette16(pThis);
    return VINF_SUCCESS;
}

/*
 * graphic modes
 */
static int vga_draw_graphic(VGAState *pThis, bool full_update, bool fFailOnResize, bool reset_dirty)
{
    int y1, y2, y, page_min, page_max, linesize, y_start, double_scan;
    int width, height, shift_control, line_offset, page0, page1, bwidth, bits;
    int disp_width, multi_run;
    uint8_t *d;
    uint32_t v, addr1, addr;
    vga_draw_line_func *vga_draw_line;

    bool offsets_changed = update_basic_params(pThis);

    full_update |= offsets_changed;

    pThis->get_resolution(pThis, &width, &height);
    disp_width = width;

    shift_control = (pThis->gr[0x05] >> 5) & 3;
    double_scan = (pThis->cr[0x09] >> 7);
    multi_run = double_scan;
    if (shift_control != pThis->shift_control ||
        double_scan != pThis->double_scan) {
        full_update = true;
        pThis->shift_control = shift_control;
        pThis->double_scan = double_scan;
    }

    if (shift_control == 0) {
        full_update |= update_palette16(pThis);
        if (pThis->sr[0x01] & 8) {
            v = VGA_DRAW_LINE4D2;
            disp_width <<= 1;
        } else {
            v = VGA_DRAW_LINE4;
        }
        bits = 4;
    } else if (shift_control == 1) {
        full_update |= update_palette16(pThis);
        if (pThis->sr[0x01] & 8) {
            v = VGA_DRAW_LINE2D2;
            disp_width <<= 1;
        } else {
            v = VGA_DRAW_LINE2;
        }
        bits = 4;
    } else {
        switch(pThis->get_bpp(pThis)) {
        default:
        case 0:
            full_update |= update_palette256(pThis);
            v = VGA_DRAW_LINE8D2;
            bits = 4;
            break;
        case 8:
            full_update |= update_palette256(pThis);
            v = VGA_DRAW_LINE8;
            bits = 8;
            break;
        case 15:
            v = VGA_DRAW_LINE15;
            bits = 16;
            break;
        case 16:
            v = VGA_DRAW_LINE16;
            bits = 16;
            break;
        case 24:
            v = VGA_DRAW_LINE24;
            bits = 24;
            break;
        case 32:
            v = VGA_DRAW_LINE32;
            bits = 32;
            break;
        }
    }
    if (    disp_width     != (int)pThis->last_width
        ||  height         != (int)pThis->last_height
        ||  pThis->get_bpp(pThis)  != (int)pThis->last_bpp
        || (offsets_changed && !pThis->fRenderVRAM))
    {
        if (fFailOnResize)
        {
            /* The caller does not want to call the pfnResize. */
            return VERR_TRY_AGAIN;
        }
        int rc = vga_resize_graphic(pThis, disp_width, height);
        if (rc != VINF_SUCCESS)  /* Return any rc, particularly VINF_VGA_RESIZE_IN_PROGRESS, to the caller. */
            return rc;
        full_update = true;
    }
    vga_draw_line = vga_draw_line_table[v * 4 + get_depth_index(pThis->pDrv->cBits)];

    if (pThis->cursor_invalidate)
        pThis->cursor_invalidate(pThis);

    line_offset = pThis->line_offset;
#if 0
    Log(("w=%d h=%d v=%d line_offset=%d cr[0x09]=0x%02x cr[0x17]=0x%02x linecmp=%d sr[0x01]=0x%02x\n",
           width, height, v, line_offset, pThis->cr[9], pThis->cr[0x17], pThis->line_compare, pThis->sr[0x01]));
#endif
    addr1 = (pThis->start_addr * 4);
    bwidth = (width * bits + 7) / 8;    /* The visible width of a scanline. */
    y_start = -1;
    page_min = 0x7fffffff;
    page_max = -1;
    d = pThis->pDrv->pu8Data;
    linesize = pThis->pDrv->cbScanline;

    y1 = 0;
    y2 = pThis->cr[0x09] & 0x1F;    /* starting row scan count */
    for(y = 0; y < height; y++) {
        addr = addr1;
        /* CGA/MDA compatibility. Note that these addresses are all
         * shifted left by two compared to VGA specs.
         */
        if (!(pThis->cr[0x17] & 1)) {
            addr = (addr & ~(1 << 15)) | ((y1 & 1) << 15);
        }
        if (!(pThis->cr[0x17] & 2)) {
            addr = (addr & ~(1 << 16)) | ((y1 & 2) << 15);
        }
        page0 = addr & TARGET_PAGE_MASK;
        page1 = (addr + bwidth - 1) & TARGET_PAGE_MASK;
        bool update = full_update | vga_is_dirty(pThis, page0) | vga_is_dirty(pThis, page1);
        if (page1 - page0 > TARGET_PAGE_SIZE) {
            /* if wide line, can use another page */
            update |= vga_is_dirty(pThis, page0 + TARGET_PAGE_SIZE);
        }
        /* explicit invalidation for the hardware cursor */
        update |= (pThis->invalidated_y_table[y >> 5] >> (y & 0x1f)) & 1;
        if (update) {
            if (y_start < 0)
                y_start = y;
            if (page0 < page_min)
                page_min = page0;
            if (page1 > page_max)
                page_max = page1;
            if (pThis->fRenderVRAM)
                vga_draw_line(pThis, d, pThis->CTX_SUFF(vram_ptr) + addr, width);
            if (pThis->cursor_draw_line)
                pThis->cursor_draw_line(pThis, d, y);
        } else {
            if (y_start >= 0) {
                /* flush to display */
                pThis->pDrv->pfnUpdateRect(pThis->pDrv, 0, y_start, disp_width, y - y_start);
                y_start = -1;
            }
        }
        if (!multi_run) {
            y1++;
            multi_run = double_scan;

            if (y2 == 0) {
                y2 = pThis->cr[0x09] & 0x1F;
                addr1 += line_offset;
            } else {
                --y2;
            }
        } else {
            multi_run--;
        }
        /* line compare acts on the displayed lines */
        if ((uint32_t)y == pThis->line_compare)
            addr1 = 0;
        d += linesize;
    }
    if (y_start >= 0) {
        /* flush to display */
        pThis->pDrv->pfnUpdateRect(pThis->pDrv, 0, y_start, disp_width, y - y_start);
    }
    /* reset modified pages */
    if (page_max != -1 && reset_dirty) {
        vga_reset_dirty(pThis, page_min, page_max + TARGET_PAGE_SIZE);
    }
    memset(pThis->invalidated_y_table, 0, ((height + 31) >> 5) * 4);
    return VINF_SUCCESS;
}

static void vga_draw_blank(VGAState *pThis, int full_update)
{
    int i, w, val;
    uint8_t *d;
    uint32_t cbScanline = pThis->pDrv->cbScanline;

    if (pThis->pDrv->pu8Data == pThis->vram_ptrR3) /* Do not clear the VRAM itself. */
        return;
    if (!full_update)
        return;
    if (pThis->last_scr_width <= 0 || pThis->last_scr_height <= 0)
        return;
    if (pThis->pDrv->cBits == 8)
        val = pThis->rgb_to_pixel(0, 0, 0);
    else
        val = 0;
    w = pThis->last_scr_width * ((pThis->pDrv->cBits + 7) >> 3);
    d = pThis->pDrv->pu8Data;
    for(i = 0; i < (int)pThis->last_scr_height; i++) {
        memset(d, val, w);
        d += cbScanline;
    }
    pThis->pDrv->pfnUpdateRect(pThis->pDrv, 0, 0, pThis->last_scr_width, pThis->last_scr_height);
}

static DECLCALLBACK(void) voidUpdateRect(PPDMIDISPLAYCONNECTOR pInterface, uint32_t x, uint32_t y, uint32_t cx, uint32_t cy)
{
    NOREF(pInterface); NOREF(x); NOREF(y); NOREF(cx); NOREF(cy);
}


#define GMODE_TEXT     0
#define GMODE_GRAPH    1
#define GMODE_BLANK 2

static int vga_update_display(PVGASTATE pThis, bool fUpdateAll, bool fFailOnResize, bool reset_dirty)
{
    int rc = VINF_SUCCESS;
    int graphic_mode;

    if (pThis->pDrv->cBits == 0) {
        /* nothing to do */
    } else {
        switch(pThis->pDrv->cBits) {
        case 8:
            pThis->rgb_to_pixel = rgb_to_pixel8_dup;
            break;
        case 15:
            pThis->rgb_to_pixel = rgb_to_pixel15_dup;
            break;
        default:
        case 16:
            pThis->rgb_to_pixel = rgb_to_pixel16_dup;
            break;
        case 32:
            pThis->rgb_to_pixel = rgb_to_pixel32_dup;
            break;
        }

        if (fUpdateAll) {
            /* A full update is requested. Special processing for a "blank" mode is required, because
             * the request must process all pending resolution changes.
             *
             * Appropriate vga_draw_graphic or vga_draw_text function, which checks the resolution change,
             * must be called even if the screen has been blanked, but then the function should do no actual
             * screen update. To do this, pfnUpdateRect is replaced with a nop.
             */
            typedef DECLCALLBACK(void) FNUPDATERECT(PPDMIDISPLAYCONNECTOR pInterface, uint32_t x, uint32_t y, uint32_t cx, uint32_t cy);
            typedef FNUPDATERECT *PFNUPDATERECT;

            PFNUPDATERECT pfnUpdateRect = NULL;

            /* Detect the "screen blank" conditions. */
            int fBlank = 0;
            if (!(pThis->ar_index & 0x20) || (pThis->sr[0x01] & 0x20)) {
                fBlank = 1;
            }

            if (fBlank) {
                /* Provide a void pfnUpdateRect callback. */
                if (pThis->pDrv) {
                    pfnUpdateRect = pThis->pDrv->pfnUpdateRect;
                    pThis->pDrv->pfnUpdateRect = voidUpdateRect;
                }
            }

            /* Do a complete redraw, which will pick up a new screen resolution. */
            if (pThis->gr[6] & 1) {
                pThis->graphic_mode = GMODE_GRAPH;
                rc = vga_draw_graphic(pThis, 1, false, reset_dirty);
            } else {
                pThis->graphic_mode = GMODE_TEXT;
                rc = vga_draw_text(pThis, 1, false, reset_dirty);
            }

            if (fBlank) {
                /* Set the current mode and restore the callback. */
                pThis->graphic_mode = GMODE_BLANK;
                if (pThis->pDrv) {
                    pThis->pDrv->pfnUpdateRect = pfnUpdateRect;
                }
            }
            return rc;
        }

        if (!(pThis->ar_index & 0x20) || (pThis->sr[0x01] & 0x20)) {
            graphic_mode = GMODE_BLANK;
        } else {
            graphic_mode = pThis->gr[6] & 1;
        }
        bool full_update = graphic_mode != pThis->graphic_mode;
        if (full_update) {
            pThis->graphic_mode = graphic_mode;
        }
        switch(graphic_mode) {
        case GMODE_TEXT:
            rc = vga_draw_text(pThis, full_update, fFailOnResize, reset_dirty);
            break;
        case GMODE_GRAPH:
            rc = vga_draw_graphic(pThis, full_update, fFailOnResize, reset_dirty);
            break;
        case GMODE_BLANK:
        default:
            vga_draw_blank(pThis, full_update);
            break;
        }
    }
    return rc;
}

static void vga_save(QEMUFile *f, void *opaque)
{
    VGAState *pThis = (VGAState*)opaque;
    int i;

    qemu_put_be32s(f, &pThis->latch);
    qemu_put_8s(f, &pThis->sr_index);
    qemu_put_buffer(f, pThis->sr, 8);
    qemu_put_8s(f, &pThis->gr_index);
    qemu_put_buffer(f, pThis->gr, 16);
    qemu_put_8s(f, &pThis->ar_index);
    qemu_put_buffer(f, pThis->ar, 21);
    qemu_put_be32s(f, &pThis->ar_flip_flop);
    qemu_put_8s(f, &pThis->cr_index);
    qemu_put_buffer(f, pThis->cr, 256);
    qemu_put_8s(f, &pThis->msr);
    qemu_put_8s(f, &pThis->fcr);
    qemu_put_8s(f, &pThis->st00);
    qemu_put_8s(f, &pThis->st01);

    qemu_put_8s(f, &pThis->dac_state);
    qemu_put_8s(f, &pThis->dac_sub_index);
    qemu_put_8s(f, &pThis->dac_read_index);
    qemu_put_8s(f, &pThis->dac_write_index);
    qemu_put_buffer(f, pThis->dac_cache, 3);
    qemu_put_buffer(f, pThis->palette, 768);

    qemu_put_be32s(f, &pThis->bank_offset);
#ifdef CONFIG_BOCHS_VBE
    qemu_put_byte(f, 1);
    qemu_put_be16s(f, &pThis->vbe_index);
    for(i = 0; i < VBE_DISPI_INDEX_NB_SAVED; i++)
        qemu_put_be16s(f, &pThis->vbe_regs[i]);
    qemu_put_be32s(f, &pThis->vbe_start_addr);
    qemu_put_be32s(f, &pThis->vbe_line_offset);
#else
    qemu_put_byte(f, 0);
#endif
}

static int vga_load(QEMUFile *f, void *opaque, int version_id)
{
    VGAState *pThis = (VGAState*)opaque;
    int is_vbe, i;
    uint32_t u32Dummy;

    qemu_get_be32s(f, &pThis->latch);
    qemu_get_8s(f, &pThis->sr_index);
    qemu_get_buffer(f, pThis->sr, 8);
    qemu_get_8s(f, &pThis->gr_index);
    qemu_get_buffer(f, pThis->gr, 16);
    qemu_get_8s(f, &pThis->ar_index);
    qemu_get_buffer(f, pThis->ar, 21);
    qemu_get_be32s(f, (uint32_t *)&pThis->ar_flip_flop);
    qemu_get_8s(f, &pThis->cr_index);
    qemu_get_buffer(f, pThis->cr, 256);
    qemu_get_8s(f, &pThis->msr);
    qemu_get_8s(f, &pThis->fcr);
    qemu_get_8s(f, &pThis->st00);
    qemu_get_8s(f, &pThis->st01);

    qemu_get_8s(f, &pThis->dac_state);
    qemu_get_8s(f, &pThis->dac_sub_index);
    qemu_get_8s(f, &pThis->dac_read_index);
    qemu_get_8s(f, &pThis->dac_write_index);
    qemu_get_buffer(f, pThis->dac_cache, 3);
    qemu_get_buffer(f, pThis->palette, 768);

    qemu_get_be32s(f, (uint32_t *)&pThis->bank_offset);
    is_vbe = qemu_get_byte(f);
#ifdef CONFIG_BOCHS_VBE
    if (!is_vbe)
    {
        Log(("vga_load: !is_vbe !!\n"));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }
    qemu_get_be16s(f, &pThis->vbe_index);
    for(i = 0; i < VBE_DISPI_INDEX_NB_SAVED; i++)
        qemu_get_be16s(f, &pThis->vbe_regs[i]);
    if (version_id <= VGA_SAVEDSTATE_VERSION_INV_VHEIGHT)
        recalculate_data(pThis, false); /* <- re-calculate the pThis->vbe_regs[VBE_DISPI_INDEX_VIRT_HEIGHT] since it might be invalid */
    qemu_get_be32s(f, &pThis->vbe_start_addr);
    qemu_get_be32s(f, &pThis->vbe_line_offset);
    if (version_id < 2)
        qemu_get_be32s(f, &u32Dummy);
    pThis->vbe_bank_max = (pThis->vram_size >> 16) - 1;
#else
    if (is_vbe)
    {
        Log(("vga_load: is_vbe !!\n"));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }
#endif

    /* force refresh */
    pThis->graphic_mode = -1;
    return 0;
}

/* see vgaR3Construct */
static void vga_init_expand(void)
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
}

#endif /* !IN_RING0 */



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
    PVGASTATE pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    Assert(PDMCritSectIsOwner(pDevIns->CTX_SUFF(pCritSectRo)));

    NOREF(pvUser);
    if (cb == 1)
        vga_ioport_write(pThis, Port, u32);
    else if (cb == 2)
    {
        vga_ioport_write(pThis, Port, u32 & 0xff);
        vga_ioport_write(pThis, Port + 1, u32 >> 8);
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
    PVGASTATE pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    Assert(PDMCritSectIsOwner(pDevIns->CTX_SUFF(pCritSectRo)));
    NOREF(pvUser);

    int rc = VINF_SUCCESS;
    if (cb == 1)
        *pu32 = vga_ioport_read(pThis, Port);
    else if (cb == 2)
        *pu32 = vga_ioport_read(pThis, Port)
             | (vga_ioport_read(pThis, Port + 1) << 8);
    else
        rc = VERR_IOM_IOPORT_UNUSED;
    return rc;
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
    PVGASTATE pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    Assert(PDMCritSectIsOwner(pDevIns->CTX_SUFF(pCritSectRo)));

    NOREF(pvUser);

#ifndef IN_RING3
    /*
     * This has to be done on the host in order to execute the connector callbacks.
     */
    if (    pThis->vbe_index == VBE_DISPI_INDEX_ENABLE
        ||  pThis->vbe_index == VBE_DISPI_INDEX_VBOX_VIDEO)
    {
        Log(("vgaIOPortWriteVBEData: VBE_DISPI_INDEX_ENABLE - Switching to host...\n"));
        return VINF_IOM_R3_IOPORT_WRITE;
    }
#endif
#ifdef VBE_BYTEWISE_IO
    if (cb == 1)
    {
        if (!pThis->fWriteVBEData)
        {
            if (    (pThis->vbe_index == VBE_DISPI_INDEX_ENABLE)
                &&  (u32 & VBE_DISPI_ENABLED))
            {
                pThis->fWriteVBEData = false;
                return vbe_ioport_write_data(pThis, Port, u32 & 0xFF);
            }

            pThis->cbWriteVBEData = u32 & 0xFF;
            pThis->fWriteVBEData = true;
            return VINF_SUCCESS;
        }

        u32 = (pThis->cbWriteVBEData << 8) | (u32 & 0xFF);
        pThis->fWriteVBEData = false;
        cb = 2;
    }
#endif
    if (cb == 2 || cb == 4)
    {
//#ifdef IN_RC
//        /*
//         * The VBE_DISPI_INDEX_ENABLE memsets the entire frame buffer.
//         * Since we're not mapping the entire framebuffer any longer that
//         * has to be done on the host.
//         */
//        if (    (pThis->vbe_index == VBE_DISPI_INDEX_ENABLE)
//            &&  (u32 & VBE_DISPI_ENABLED))
//        {
//            Log(("vgaIOPortWriteVBEData: VBE_DISPI_INDEX_ENABLE & VBE_DISPI_ENABLED - Switching to host...\n"));
//            return VINF_IOM_R3_IOPORT_WRITE;
//        }
//#endif
        return vbe_ioport_write_data(pThis, Port, u32);
    }
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
    PVGASTATE pThis = PDMINS_2_DATA(pDevIns, PVGASTATE); NOREF(pvUser);
    Assert(PDMCritSectIsOwner(pDevIns->CTX_SUFF(pCritSectRo)));

#ifdef VBE_BYTEWISE_IO
    if (cb == 1)
    {
        if (!pThis->fWriteVBEIndex)
        {
            pThis->cbWriteVBEIndex = u32 & 0x00FF;
            pThis->fWriteVBEIndex = true;
            return VINF_SUCCESS;
        }
        pThis->fWriteVBEIndex = false;
        vbe_ioport_write_index(pThis, Port, (pThis->cbWriteVBEIndex << 8) | (u32 & 0x00FF));
        return VINF_SUCCESS;
    }
#endif

    if (cb == 2)
        vbe_ioport_write_index(pThis, Port, u32);
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
    PVGASTATE pThis = PDMINS_2_DATA(pDevIns, PVGASTATE); NOREF(pvUser);
    Assert(PDMCritSectIsOwner(pDevIns->CTX_SUFF(pCritSectRo)));


#ifdef VBE_BYTEWISE_IO
    if (cb == 1)
    {
        if (!pThis->fReadVBEData)
        {
            *pu32 = (vbe_ioport_read_data(pThis, Port) >> 8) & 0xFF;
            pThis->fReadVBEData = true;
            return VINF_SUCCESS;
        }
        *pu32 = vbe_ioport_read_data(pThis, Port) & 0xFF;
        pThis->fReadVBEData = false;
        return VINF_SUCCESS;
    }
#endif
    if (cb == 2)
    {
        *pu32 = vbe_ioport_read_data(pThis, Port);
        return VINF_SUCCESS;
    }
    if (cb == 4)
    {
        /* Quick hack for getting the vram size. */
        *pu32 = pThis->vram_size;
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
    VGAState *pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    Assert(PDMCritSectIsOwner(pDevIns->CTX_SUFF(pCritSectRo)));

#ifdef VBE_BYTEWISE_IO
    if (cb == 1)
    {
        if (!pThis->fReadVBEIndex)
        {
            *pu32 = (vbe_ioport_read_index(pThis, Port) >> 8) & 0xFF;
            pThis->fReadVBEIndex = true;
            return VINF_SUCCESS;
        }
        *pu32 = vbe_ioport_read_index(pThis, Port) & 0xFF;
        pThis->fReadVBEIndex = false;
        return VINF_SUCCESS;
    }
#endif
    if (cb == 2)
    {
        *pu32 = vbe_ioport_read_index(pThis, Port);
        return VINF_SUCCESS;
    }
    AssertMsgFailed(("vgaIOPortReadVBEIndex: Port=%#x cb=%d\n", Port, cb));
    return VERR_IOM_IOPORT_UNUSED;
}

#ifdef VBOX_WITH_HGSMI
#ifdef IN_RING3
/**
 * Port I/O Handler for HGSMI OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
static DECLCALLBACK(int) vgaR3IOPortHGSMIWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    VGAState *pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    Assert(PDMCritSectIsOwner(pDevIns->CTX_SUFF(pCritSectRo)));
    LogFlowFunc(("Port 0x%x, u32 0x%x, cb %d\n", Port, u32, cb));


    NOREF(pvUser);

    if (cb == 4)
    {
        switch (Port)
        {
            case VGA_PORT_HGSMI_HOST: /* Host */
            {
#if defined(VBOX_WITH_VIDEOHWACCEL) || defined(VBOX_WITH_VDMA) || defined(VBOX_WITH_WDDM)
                if (u32 == HGSMIOFFSET_VOID)
                {
                    PDMDevHlpPCISetIrq(pDevIns, 0, PDM_IRQ_LEVEL_LOW);
                    HGSMIClearHostGuestFlags(pThis->pHGSMI, HGSMIHOSTFLAGS_IRQ
#ifdef VBOX_VDMA_WITH_WATCHDOG
                            | HGSMIHOSTFLAGS_WATCHDOG
#endif
                            | HGSMIHOSTFLAGS_VSYNC
                            );
                }
                else
#endif
                {
                    HGSMIHostWrite(pThis->pHGSMI, u32);
                }
                break;
            }

            case VGA_PORT_HGSMI_GUEST: /* Guest */
                HGSMIGuestWrite(pThis->pHGSMI, u32);
                break;

            default:
#ifdef DEBUG_sunlover
                AssertMsgFailed(("vgaR3IOPortHGSMIWrite: Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
#endif
                break;
        }
    }
    else
    {
#ifdef DEBUG_sunlover
        AssertMsgFailed(("vgaR3IOPortHGSMIWrite: Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
#endif
    }

    return VINF_SUCCESS;
}

/**
 * Port I/O Handler for HGSMI IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes to read.
 */
static DECLCALLBACK(int) vgaR3IOPortHGSMIRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    VGAState *pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    Assert(PDMCritSectIsOwner(pDevIns->CTX_SUFF(pCritSectRo)));
    LogFlowFunc(("Port 0x%x, cb %d\n", Port, cb));

    NOREF(pvUser);

    int rc = VINF_SUCCESS;
    if (cb == 4)
    {
        switch (Port)
        {
            case VGA_PORT_HGSMI_HOST: /* Host */
                *pu32 = HGSMIHostRead(pThis->pHGSMI);
                break;
            case VGA_PORT_HGSMI_GUEST: /* Guest */
                *pu32 = HGSMIGuestRead(pThis->pHGSMI);
                break;
            default:
#ifdef DEBUG_sunlover
                AssertMsgFailed(("vgaR3IOPortHGSMIRead: Port=%#x cb=%d\n", Port, cb));
#endif
                rc = VERR_IOM_IOPORT_UNUSED;
                break;
        }
    }
    else
    {
#ifdef DEBUG_sunlover
        Log(("vgaR3IOPortHGSMIRead: Port=%#x cb=%d\n", Port, cb));
#endif
        rc = VERR_IOM_IOPORT_UNUSED;
    }

    return rc;
}
#endif /* IN_RING3 */
#endif /* VBOX_WITH_HGSMI */




/* -=-=-=-=-=- Guest Context -=-=-=-=-=- */

/*
 * Internal. For use inside VGAGCMemoryFillWrite only.
 * Macro for apply logical operation and bit mask.
 */
#define APPLY_LOGICAL_AND_MASK(pThis, val, bit_mask) \
    /* apply logical operation */                \
    switch(pThis->gr[3] >> 3)                        \
    {                                            \
        case 0:                                  \
        default:                                 \
            /* nothing to do */                  \
            break;                               \
        case 1:                                  \
            /* and */                            \
            val &= pThis->latch;                     \
            break;                               \
        case 2:                                  \
            /* or */                             \
            val |= pThis->latch;                     \
            break;                               \
        case 3:                                  \
            /* xor */                            \
            val ^= pThis->latch;                     \
            break;                               \
    }                                            \
    /* apply bit mask */                         \
    val = (val & bit_mask) | (pThis->latch & ~bit_mask)

/**
 * Legacy VGA memory (0xa0000 - 0xbffff) write hook, to be called from IOM and from the inside of VGADeviceGC.cpp.
 * This is the advanced version of vga_mem_writeb function.
 *
 * @returns VBox status code.
 * @param   pThis       VGA device structure
 * @param   pvUser      User argument - ignored.
 * @param   GCPhysAddr  Physical address of memory to write.
 * @param   u32Item     Data to write, up to 4 bytes.
 * @param   cbItem      Size of data Item, only 1/2/4 bytes is allowed for now.
 * @param   cItems      Number of data items to write.
 */
static int vgaInternalMMIOFill(PVGASTATE pThis, void *pvUser, RTGCPHYS GCPhysAddr, uint32_t u32Item, unsigned cbItem, unsigned cItems)
{
    uint32_t b;
    uint32_t write_mask, bit_mask, set_mask;
    uint32_t aVal[4];
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
    switch((pThis->gr[6] >> 2) & 3) {
    case 0:
        break;
    case 1:
        if (GCPhysAddr >= 0x10000)
            return VINF_SUCCESS;
        GCPhysAddr += pThis->bank_offset;
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

    if (pThis->sr[4] & 0x08) {
        /* chain 4 mode : simplest access */
        VERIFY_VRAM_WRITE_OFF_RETURN(pThis, GCPhysAddr + cItems * cbItem - 1);

        while (cItems-- > 0)
            for (i = 0; i < cbItem; i++)
            {
                if (pThis->sr[2] & (1 << (GCPhysAddr & 3)))
                {
                    pThis->CTX_SUFF(vram_ptr)[GCPhysAddr] = aVal[i];
                    vga_set_dirty(pThis, GCPhysAddr);
                }
                GCPhysAddr++;
            }
    } else if (pThis->gr[5] & 0x10) {
        /* odd/even mode (aka text mode mapping) */
        VERIFY_VRAM_WRITE_OFF_RETURN(pThis, (GCPhysAddr + cItems * cbItem) * 4 - 1);
        while (cItems-- > 0)
            for (i = 0; i < cbItem; i++)
            {
                unsigned plane = (pThis->gr[4] & 2) | (GCPhysAddr & 1);
                if (pThis->sr[2] & (1 << plane)) {
                    RTGCPHYS PhysAddr2 = ((GCPhysAddr & ~1) << 2) | plane;
                    pThis->CTX_SUFF(vram_ptr)[PhysAddr2] = aVal[i];
                    vga_set_dirty(pThis, PhysAddr2);
                }
                GCPhysAddr++;
            }
    } else {
        /* standard VGA latched access */
        VERIFY_VRAM_WRITE_OFF_RETURN(pThis, (GCPhysAddr + cItems * cbItem) * 4 - 1);

        switch(pThis->gr[5] & 3) {
        default:
        case 0:
            /* rotate */
            b = pThis->gr[3] & 7;
            bit_mask = pThis->gr[8];
            bit_mask |= bit_mask << 8;
            bit_mask |= bit_mask << 16;
            set_mask = mask16[pThis->gr[1]];

            for (i = 0; i < cbItem; i++)
            {
                aVal[i] = ((aVal[i] >> b) | (aVal[i] << (8 - b))) & 0xff;
                aVal[i] |= aVal[i] << 8;
                aVal[i] |= aVal[i] << 16;

                /* apply set/reset mask */
                aVal[i] = (aVal[i] & ~set_mask) | (mask16[pThis->gr[0]] & set_mask);

                APPLY_LOGICAL_AND_MASK(pThis, aVal[i], bit_mask);
            }
            break;
        case 1:
            for (i = 0; i < cbItem; i++)
                aVal[i] = pThis->latch;
            break;
        case 2:
            bit_mask = pThis->gr[8];
            bit_mask |= bit_mask << 8;
            bit_mask |= bit_mask << 16;
            for (i = 0; i < cbItem; i++)
            {
                aVal[i] = mask16[aVal[i] & 0x0f];

                APPLY_LOGICAL_AND_MASK(pThis, aVal[i], bit_mask);
            }
            break;
        case 3:
            /* rotate */
            b = pThis->gr[3] & 7;

            for (i = 0; i < cbItem; i++)
            {
                aVal[i] = (aVal[i] >> b) | (aVal[i] << (8 - b));
                bit_mask = pThis->gr[8] & aVal[i];
                bit_mask |= bit_mask << 8;
                bit_mask |= bit_mask << 16;
                aVal[i] = mask16[pThis->gr[0]];

                APPLY_LOGICAL_AND_MASK(pThis, aVal[i], bit_mask);
            }
            break;
        }

        /* mask data according to sr[2] */
        write_mask = mask16[pThis->sr[2]];

        /* actually write data */
        if (cbItem == 1)
        {
            /* The most frequently case is 1 byte I/O. */
            while (cItems-- > 0)
            {
                ((uint32_t *)pThis->CTX_SUFF(vram_ptr))[GCPhysAddr] = (((uint32_t *)pThis->CTX_SUFF(vram_ptr))[GCPhysAddr] & ~write_mask) | (aVal[0] & write_mask);
                vga_set_dirty(pThis, GCPhysAddr << 2);
                GCPhysAddr++;
            }
        }
        else if (cbItem == 2)
        {
            /* The second case is 2 bytes I/O. */
            while (cItems-- > 0)
            {
                ((uint32_t *)pThis->CTX_SUFF(vram_ptr))[GCPhysAddr] = (((uint32_t *)pThis->CTX_SUFF(vram_ptr))[GCPhysAddr] & ~write_mask) | (aVal[0] & write_mask);
                vga_set_dirty(pThis, GCPhysAddr << 2);
                GCPhysAddr++;

                ((uint32_t *)pThis->CTX_SUFF(vram_ptr))[GCPhysAddr] = (((uint32_t *)pThis->CTX_SUFF(vram_ptr))[GCPhysAddr] & ~write_mask) | (aVal[1] & write_mask);
                vga_set_dirty(pThis, GCPhysAddr << 2);
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
                    ((uint32_t *)pThis->CTX_SUFF(vram_ptr))[GCPhysAddr] = (((uint32_t *)pThis->CTX_SUFF(vram_ptr))[GCPhysAddr] & ~write_mask) | (aVal[i] & write_mask);
                    vga_set_dirty(pThis, GCPhysAddr << 2);
                    GCPhysAddr++;
                }
        }
    }
    return VINF_SUCCESS;
}

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
    PVGASTATE pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    Assert(PDMCritSectIsOwner(pDevIns->CTX_SUFF(pCritSectRo)));

    return vgaInternalMMIOFill(pThis, pvUser, GCPhysAddr, u32Item, cbItem, cItems);
}
#undef APPLY_LOGICAL_AND_MASK


/**
 * Legacy VGA memory (0xa0000 - 0xbffff) read hook, to be called from IOM.
 *
 * @returns VBox status code.
 * @param   pDevIns     Pointer device instance.
 * @param   pvUser      User argument - ignored.
 * @param   GCPhysAddr  Physical address of memory to read.
 * @param   pv          Where to store read data.
 * @param   cb          Bytes to read.
 */
PDMBOTHCBDECL(int) vgaMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    PVGASTATE pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    STAM_PROFILE_START(&pThis->CTX_MID_Z(Stat,MemoryRead), a);
    Assert(PDMCritSectIsOwner(pDevIns->CTX_SUFF(pCritSectRo)));
    NOREF(pvUser);

    int rc = VINF_SUCCESS;
    switch (cb)
    {
        case 1:
            *(uint8_t  *)pv = vga_mem_readb(pThis, GCPhysAddr, &rc);
            break;
        case 2:
            *(uint16_t *)pv = vga_mem_readb(pThis, GCPhysAddr, &rc)
                           | (vga_mem_readb(pThis, GCPhysAddr + 1, &rc) << 8);
            break;
        case 4:
            *(uint32_t *)pv = vga_mem_readb(pThis, GCPhysAddr, &rc)
                           | (vga_mem_readb(pThis, GCPhysAddr + 1, &rc) <<  8)
                           | (vga_mem_readb(pThis, GCPhysAddr + 2, &rc) << 16)
                           | (vga_mem_readb(pThis, GCPhysAddr + 3, &rc) << 24);
            break;

        case 8:
            *(uint64_t *)pv = (uint64_t)vga_mem_readb(pThis, GCPhysAddr, &rc)
                           | ((uint64_t)vga_mem_readb(pThis, GCPhysAddr + 1, &rc) <<  8)
                           | ((uint64_t)vga_mem_readb(pThis, GCPhysAddr + 2, &rc) << 16)
                           | ((uint64_t)vga_mem_readb(pThis, GCPhysAddr + 3, &rc) << 24)
                           | ((uint64_t)vga_mem_readb(pThis, GCPhysAddr + 4, &rc) << 32)
                           | ((uint64_t)vga_mem_readb(pThis, GCPhysAddr + 5, &rc) << 40)
                           | ((uint64_t)vga_mem_readb(pThis, GCPhysAddr + 6, &rc) << 48)
                           | ((uint64_t)vga_mem_readb(pThis, GCPhysAddr + 7, &rc) << 56);
            break;

        default:
        {
            uint8_t *pu8Data = (uint8_t *)pv;
            while (cb-- > 0)
            {
                *pu8Data++ = vga_mem_readb(pThis, GCPhysAddr++, &rc);
                if (RT_UNLIKELY(rc != VINF_SUCCESS))
                    break;
            }
        }
    }

    STAM_PROFILE_STOP(&pThis->CTX_MID_Z(Stat,MemoryRead), a);
    return rc;
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
PDMBOTHCBDECL(int) vgaMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb)
{
    PVGASTATE pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    uint8_t  *pu8 = (uint8_t *)pv;
    NOREF(pvUser);
    STAM_PROFILE_START(&pThis->CTX_MID_Z(Stat,MemoryWrite), a);
    Assert(PDMCritSectIsOwner(pDevIns->CTX_SUFF(pCritSectRo)));

    int rc;
    switch (cb)
    {
        case 1:
            rc = vga_mem_writeb(pThis, GCPhysAddr, *pu8);
            break;
#if 1
        case 2:
            rc = vga_mem_writeb(pThis, GCPhysAddr + 0, pu8[0]);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = vga_mem_writeb(pThis, GCPhysAddr + 1, pu8[1]);
            break;
        case 4:
            rc = vga_mem_writeb(pThis, GCPhysAddr + 0, pu8[0]);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = vga_mem_writeb(pThis, GCPhysAddr + 1, pu8[1]);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = vga_mem_writeb(pThis, GCPhysAddr + 2, pu8[2]);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = vga_mem_writeb(pThis, GCPhysAddr + 3, pu8[3]);
            break;
        case 8:
            rc = vga_mem_writeb(pThis, GCPhysAddr + 0, pu8[0]);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = vga_mem_writeb(pThis, GCPhysAddr + 1, pu8[1]);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = vga_mem_writeb(pThis, GCPhysAddr + 2, pu8[2]);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = vga_mem_writeb(pThis, GCPhysAddr + 3, pu8[3]);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = vga_mem_writeb(pThis, GCPhysAddr + 4, pu8[4]);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = vga_mem_writeb(pThis, GCPhysAddr + 5, pu8[5]);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = vga_mem_writeb(pThis, GCPhysAddr + 6, pu8[6]);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = vga_mem_writeb(pThis, GCPhysAddr + 7, pu8[7]);
            break;
#else
        case 2:
            rc = vgaMMIOFill(pDevIns, GCPhysAddr, *(uint16_t *)pv, 2, 1);
            break;
        case 4:
            rc = vgaMMIOFill(pDevIns, GCPhysAddr, *(uint32_t *)pv, 4, 1);
            break;
        case 8:
            rc = vgaMMIOFill(pDevIns, GCPhysAddr, *(uint64_t *)pv, 8, 1);
            break;
#endif
        default:
            rc = VINF_SUCCESS;
            while (cb-- > 0 && rc == VINF_SUCCESS)
                rc = vga_mem_writeb(pThis, GCPhysAddr++, *pu8++);
            break;

    }
    STAM_PROFILE_STOP(&pThis->CTX_MID_Z(Stat,MemoryWrite), a);
    return rc;
}


/**
 * Handle LFB access.
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   pThis       VGA device instance data.
 * @param   GCPhys      The access physical address.
 * @param   GCPtr       The access virtual address (only GC).
 */
static int vgaLFBAccess(PVM pVM, PVGASTATE pThis, RTGCPHYS GCPhys, RTGCPTR GCPtr)
{
    int rc = PDMCritSectEnter(&pThis->CritSect, VINF_EM_RAW_EMULATE_INSTR);
    if (rc != VINF_SUCCESS)
        return rc;

    /*
     * Set page dirty bit.
     */
    vga_set_dirty(pThis, GCPhys - pThis->GCPhysVRAM);
    pThis->fLFBUpdated = true;

    /*
     * Turn of the write handler for this particular page and make it R/W.
     * Then return telling the caller to restart the guest instruction.
     * ASSUME: the guest always maps video memory RW.
     */
    rc = PGMHandlerPhysicalPageTempOff(pVM, pThis->GCPhysVRAM, GCPhys);
    if (RT_SUCCESS(rc))
    {
#ifndef IN_RING3
        rc = PGMShwMakePageWritable(PDMDevHlpGetVMCPU(pThis->CTX_SUFF(pDevIns)), GCPtr,
                                    PGM_MK_PG_IS_MMIO2 | PGM_MK_PG_IS_WRITE_FAULT);
        PDMCritSectLeave(&pThis->CritSect);
        AssertMsgReturn(    rc == VINF_SUCCESS
                        /* In the SMP case the page table might be removed while we wait for the PGM lock in the trap handler. */
                        ||  rc == VERR_PAGE_TABLE_NOT_PRESENT
                        ||  rc == VERR_PAGE_NOT_PRESENT,
                        ("PGMShwModifyPage -> GCPtr=%RGv rc=%d\n", GCPtr, rc),
                        rc);
#else /* IN_RING3 : We don't have any virtual page address of the access here. */
        PDMCritSectLeave(&pThis->CritSect);
        Assert(GCPtr == 0);
#endif
        return VINF_SUCCESS;
    }

    PDMCritSectLeave(&pThis->CritSect);
    AssertMsgFailed(("PGMHandlerPhysicalPageTempOff -> rc=%d\n", rc));
    return rc;
}


#ifdef IN_RC
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
    PVGASTATE   pThis = (PVGASTATE)pvUser;
    AssertPtr(pThis);
    Assert(GCPhysFault >= pThis->GCPhysVRAM);
    AssertMsg(uErrorCode & X86_TRAP_PF_RW, ("uErrorCode=%#x\n", uErrorCode));
    NOREF(pRegFrame);

    return vgaLFBAccess(pVM, pThis, GCPhysFault, pvFault);
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
    PVGASTATE   pThis = (PVGASTATE)pvUser;
    Assert(pThis);
    Assert(GCPhysFault >= pThis->GCPhysVRAM);
    AssertMsg(uErrorCode & X86_TRAP_PF_RW, ("uErrorCode=%#x\n", uErrorCode));
    NOREF(pRegFrame);

    return vgaLFBAccess(pVM, pThis, GCPhysFault, pvFault);
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
    PVGASTATE   pThis = (PVGASTATE)pvUser;
    int         rc;
    Assert(pThis);
    Assert(GCPhys >= pThis->GCPhysVRAM);
    NOREF(pvPhys); NOREF(pvBuf); NOREF(cbBuf); NOREF(enmAccessType);

    rc = vgaLFBAccess(pVM, pThis, GCPhys, 0);
    if (RT_SUCCESS(rc))
        return VINF_PGM_HANDLER_DO_DEFAULT;
    AssertMsg(rc <= VINF_SUCCESS, ("rc=%Rrc\n", rc));
    return rc;
}
#endif /* IN_RING3 */

/* -=-=-=-=-=- All rings: VGA BIOS I/Os -=-=-=-=-=- */

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
PDMBOTHCBDECL(int) vgaIOPortReadBIOS(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
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
PDMBOTHCBDECL(int) vgaIOPortWriteBIOS(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    static int lastWasNotNewline = 0;  /* We are only called in a single-threaded way */
    PVGASTATE pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    NOREF(pvUser);
    Assert(PDMCritSectIsOwner(pDevIns->CTX_SUFF(pCritSectRo)));

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
    return VERR_IOM_IOPORT_UNUSED;
}


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
    PVGASTATE pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    Assert(PDMCritSectIsOwner(pDevIns->CTX_SUFF(pCritSectRo)));
    NOREF(pvUser); NOREF(Port);

    if (cb == 2)
    {
        Log(("vbeIOPortWriteVBEExtra: addr=%#RX32\n", u32));
        pThis->u16VBEExtraAddress = u32;
    }
    else
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
    PVGASTATE pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    NOREF(pvUser); NOREF(Port);
    Assert(PDMCritSectIsOwner(pDevIns->CTX_SUFF(pCritSectRo)));

    int rc = VINF_SUCCESS;
    if (pThis->u16VBEExtraAddress == 0xffff)
    {
        Log(("vbeIOPortReadVBEExtra: Requested number of 64k video banks\n"));
        *pu32 = pThis->vram_size / _64K;
    }
    else if (   pThis->u16VBEExtraAddress >= pThis->cbVBEExtraData
             || pThis->u16VBEExtraAddress + cb > pThis->cbVBEExtraData)
    {
        *pu32 = 0;
        Log(("vbeIOPortReadVBEExtra: Requested address is out of VBE data!!! Address=%#x(%d) cbVBEExtraData=%#x(%d)\n",
             pThis->u16VBEExtraAddress, pThis->u16VBEExtraAddress, pThis->cbVBEExtraData, pThis->cbVBEExtraData));
    }
    else if (cb == 1)
    {
        *pu32 = pThis->pu8VBEExtraData[pThis->u16VBEExtraAddress] & 0xFF;

        Log(("vbeIOPortReadVBEExtra: cb=%#x %.*Rhxs\n", cb, cb, pu32));
    }
    else if (cb == 2)
    {
        *pu32 = pThis->pu8VBEExtraData[pThis->u16VBEExtraAddress]
              | pThis->pu8VBEExtraData[pThis->u16VBEExtraAddress + 1] << 8;

        Log(("vbeIOPortReadVBEExtra: cb=%#x %.*Rhxs\n", cb, cb, pu32));
    }
    else
    {
        Log(("vbeIOPortReadVBEExtra: Invalid cb=%d read from the VBE Extra port!!!\n", cb));
        rc = VERR_IOM_IOPORT_UNUSED;
    }

    return rc;
}
# endif /* VBE_NEW_DYN_LIST */


/**
 * Parse the logo bitmap data at init time.
 *
 * @returns VBox status code.
 *
 * @param   pThis       The VGA instance data.
 */
static int vbeParseBitmap(PVGASTATE pThis)
{
    uint16_t    i;
    PBMPINFO    bmpInfo;
    POS2HDR     pOs2Hdr;
    POS22HDR    pOs22Hdr;
    PWINHDR     pWinHdr;

    /*
     * Get bitmap header data
     */
    bmpInfo = (PBMPINFO)(pThis->pu8Logo + sizeof(LOGOHDR));
    pWinHdr = (PWINHDR)(pThis->pu8Logo + sizeof(LOGOHDR) + sizeof(BMPINFO));

    if (bmpInfo->Type == BMP_ID)
    {
        switch (pWinHdr->Size)
        {
            case BMP_HEADER_OS21:
                pOs2Hdr = (POS2HDR)pWinHdr;
                pThis->cxLogo = pOs2Hdr->Width;
                pThis->cyLogo = pOs2Hdr->Height;
                pThis->cLogoPlanes = pOs2Hdr->Planes;
                pThis->cLogoBits = pOs2Hdr->BitCount;
                pThis->LogoCompression = BMP_COMPRESS_NONE;
                pThis->cLogoUsedColors = 0;
                break;

            case BMP_HEADER_OS22:
                pOs22Hdr = (POS22HDR)pWinHdr;
                pThis->cxLogo = pOs22Hdr->Width;
                pThis->cyLogo = pOs22Hdr->Height;
                pThis->cLogoPlanes = pOs22Hdr->Planes;
                pThis->cLogoBits = pOs22Hdr->BitCount;
                pThis->LogoCompression = pOs22Hdr->Compression;
                pThis->cLogoUsedColors = pOs22Hdr->ClrUsed;
                break;

            case BMP_HEADER_WIN3:
                pThis->cxLogo = pWinHdr->Width;
                pThis->cyLogo = pWinHdr->Height;
                pThis->cLogoPlanes = pWinHdr->Planes;
                pThis->cLogoBits = pWinHdr->BitCount;
                pThis->LogoCompression = pWinHdr->Compression;
                pThis->cLogoUsedColors = pWinHdr->ClrUsed;
                break;

            default:
                AssertLogRelMsgFailedReturn(("Unsupported bitmap header size %u.\n", pWinHdr->Size),
                                            VERR_INVALID_PARAMETER);
                break;
        }

        AssertLogRelMsgReturn(pThis->cxLogo <= LOGO_MAX_WIDTH && pThis->cyLogo <= LOGO_MAX_HEIGHT,
                              ("Bitmap %ux%u is too big.\n", pThis->cxLogo, pThis->cyLogo),
                              VERR_INVALID_PARAMETER);

        AssertLogRelMsgReturn(pThis->cLogoPlanes == 1,
                              ("Bitmap planes %u != 1.\n", pThis->cLogoPlanes),
                              VERR_INVALID_PARAMETER);

        AssertLogRelMsgReturn(pThis->cLogoBits == 4 || pThis->cLogoBits == 8 || pThis->cLogoBits == 24,
                              ("Unsupported %u depth.\n", pThis->cLogoBits),
                              VERR_INVALID_PARAMETER);

        AssertLogRelMsgReturn(pThis->cLogoUsedColors <= 256,
                              ("Unsupported %u colors.\n", pThis->cLogoUsedColors),
                              VERR_INVALID_PARAMETER);

        AssertLogRelMsgReturn(pThis->LogoCompression == BMP_COMPRESS_NONE,
                               ("Unsupported %u compression.\n", pThis->LogoCompression),
                               VERR_INVALID_PARAMETER);

        /*
         * Read bitmap palette
         */
        if (!pThis->cLogoUsedColors)
            pThis->cLogoPalEntries = 1 << (pThis->cLogoPlanes * pThis->cLogoBits);
        else
            pThis->cLogoPalEntries = pThis->cLogoUsedColors;

        if (pThis->cLogoPalEntries)
        {
            const uint8_t *pu8Pal = pThis->pu8Logo + sizeof(LOGOHDR) + sizeof(BMPINFO) + pWinHdr->Size; /* ASSUMES Size location (safe) */

            for (i = 0; i < pThis->cLogoPalEntries; i++)
            {
                uint16_t j;
                uint32_t u32Pal = 0;

                for (j = 0; j < 3; j++)
                {
                    uint8_t b = *pu8Pal++;
                    u32Pal <<= 8;
                    u32Pal |= b;
                }

                pu8Pal++; /* skip unused byte */
                pThis->au32LogoPalette[i] = u32Pal;
            }
        }

        /*
         * Bitmap data offset
         */
        pThis->pu8LogoBitmap = pThis->pu8Logo + sizeof(LOGOHDR) + bmpInfo->Offset;
    }
    else
        AssertLogRelMsgFailedReturn(("Not a BMP file.\n"), VERR_INVALID_PARAMETER);

    return VINF_SUCCESS;
}


/**
 * Show logo bitmap data.
 *
 * @returns VBox status code.
 *
 * @param   cbDepth     Logo depth.
 * @param   xLogo       Logo X position.
 * @param   yLogo       Logo Y position.
 * @param   cxLogo      Logo width.
 * @param   cyLogo      Logo height.
 * @param   iStep       Fade in/fade out step.
 * @param   pu32Palette Palette data.
 * @param   pu8Src      Source buffer.
 * @param   pu8Dst      Destination buffer.
 */
static void vbeShowBitmap(uint16_t cBits, uint16_t xLogo, uint16_t yLogo, uint16_t cxLogo, uint16_t cyLogo, uint8_t iStep,
                          const uint32_t *pu32Palette, const uint8_t *pu8Src, uint8_t *pu8Dst)
{
    uint16_t        i;
    size_t          cbPadBytes  = 0;
    size_t          cbLineDst   = LOGO_MAX_WIDTH * 4;
    uint16_t        cyLeft      = cyLogo;

    pu8Dst += xLogo * 4 + yLogo * cbLineDst;

    switch (cBits)
    {
        case 1:
            pu8Dst += cyLogo * cbLineDst;
            cbPadBytes = 0;
            break;

        case 4:
            if (((cxLogo % 8) == 0) || ((cxLogo % 8) > 6))
                cbPadBytes = 0;
            else if ((cxLogo % 8) <= 2)
                cbPadBytes = 3;
            else if ((cxLogo % 8) <= 4)
                cbPadBytes = 2;
            else
                cbPadBytes = 1;
            break;

        case 8:
            cbPadBytes = ((cxLogo % 4) == 0) ? 0 : (4 - (cxLogo % 4));
            break;

        case 24:
            cbPadBytes = cxLogo % 4;
            break;
    }

    uint8_t j = 0, c = 0;

    while (cyLeft-- > 0)
    {
        uint8_t *pu8TmpPtr = pu8Dst;

        if (cBits != 1)
            j = 0;

        for (i = 0; i < cxLogo; i++)
        {
            uint8_t pix;

            switch (cBits)
            {
                case 1:
                {
                    if (!j)
                        c = *pu8Src++;

                    pix = (c & 1) ? 0xFF : 0;
                    c >>= 1;

                    if (pix)
                    {
                        *pu8TmpPtr++ = pix * iStep / LOGO_SHOW_STEPS;
                        *pu8TmpPtr++ = pix * iStep / LOGO_SHOW_STEPS;
                        *pu8TmpPtr++ = pix * iStep / LOGO_SHOW_STEPS;
                        *pu8TmpPtr++;
                    }
                    else
                    {
                        pu8TmpPtr += 4;
                    }

                    j = (j + 1) % 8;
                    break;
                }

                case 4:
                {
                    if (!j)
                        c = *pu8Src++;

                    pix = (c >> 4) & 0xF;
                    c <<= 4;

                    uint32_t u32Pal = pu32Palette[pix];

                    pix = (u32Pal >> 16) & 0xFF;
                    *pu8TmpPtr++ = pix * iStep / LOGO_SHOW_STEPS;
                    pix = (u32Pal >> 8) & 0xFF;
                    *pu8TmpPtr++ = pix * iStep / LOGO_SHOW_STEPS;
                    pix = u32Pal & 0xFF;
                    *pu8TmpPtr++ = pix * iStep / LOGO_SHOW_STEPS;
                    *pu8TmpPtr++;

                    j = (j + 1) % 2;
                    break;
                }

                case 8:
                {
                    uint32_t u32Pal = pu32Palette[*pu8Src++];

                    pix = (u32Pal >> 16) & 0xFF;
                    *pu8TmpPtr++ = pix * iStep / LOGO_SHOW_STEPS;
                    pix = (u32Pal >> 8) & 0xFF;
                    *pu8TmpPtr++ = pix * iStep / LOGO_SHOW_STEPS;
                    pix = u32Pal & 0xFF;
                    *pu8TmpPtr++ = pix * iStep / LOGO_SHOW_STEPS;
                    *pu8TmpPtr++;
                    break;
                }

                case 24:
                    *pu8TmpPtr++ = *pu8Src++ * iStep / LOGO_SHOW_STEPS;
                    *pu8TmpPtr++ = *pu8Src++ * iStep / LOGO_SHOW_STEPS;
                    *pu8TmpPtr++ = *pu8Src++ * iStep / LOGO_SHOW_STEPS;
                    *pu8TmpPtr++;
                    break;
            }
        }

        pu8Dst -= cbLineDst;
        pu8Src += cbPadBytes;
    }
}




/**
 * Port I/O Handler for BIOS Logo OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) vbeIOPortWriteCMDLogo(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    PVGASTATE pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    NOREF(pvUser);
    NOREF(Port);

    Log(("vbeIOPortWriteCMDLogo: cb=%d u32=%#04x(%#04d) (byte)\n", cb, u32, u32));

    if (cb == 2)
    {
        /* Get the logo command */
        switch (u32 & 0xFF00)
        {
            case LOGO_CMD_SET_OFFSET:
                pThis->offLogoData = u32 & 0xFF;
                break;

            case LOGO_CMD_SHOW_BMP:
            {
                uint8_t         iStep = u32 & 0xFF;
                const uint8_t  *pu8Src = pThis->pu8LogoBitmap;
                uint8_t        *pu8Dst;
                PLOGOHDR        pLogoHdr = (PLOGOHDR)pThis->pu8Logo;
                uint32_t        offDirty = 0;
                uint16_t        xLogo = (LOGO_MAX_WIDTH - pThis->cxLogo) / 2;
                uint16_t        yLogo = LOGO_MAX_HEIGHT - (LOGO_MAX_HEIGHT - pThis->cyLogo) / 2;

                /* Check VRAM size */
                if (pThis->vram_size < LOGO_MAX_SIZE)
                    break;

                if (pThis->vram_size >= LOGO_MAX_SIZE * 2)
                    pu8Dst = pThis->vram_ptrR3 + LOGO_MAX_SIZE;
                else
                    pu8Dst = pThis->vram_ptrR3;

                /* Clear screen - except on power on... */
                if (!pThis->fLogoClearScreen)
                {
                    uint32_t *pu32TmpPtr = (uint32_t *)pu8Dst;

                    /* Clear vram */
                    for (int i = 0; i < LOGO_MAX_WIDTH; i++)
                    {
                        for (int j = 0; j < LOGO_MAX_HEIGHT; j++)
                            *pu32TmpPtr++ = 0;
                    }
                    pThis->fLogoClearScreen = true;
                }

                /* Show the bitmap. */
                vbeShowBitmap(pThis->cLogoBits, xLogo, yLogo,
                              pThis->cxLogo, pThis->cyLogo,
                              iStep, &pThis->au32LogoPalette[0],
                              pu8Src, pu8Dst);

                /* Show the 'Press F12...' text. */
                if (pLogoHdr->fu8ShowBootMenu == 2)
                    vbeShowBitmap(1, LOGO_F12TEXT_X, LOGO_F12TEXT_Y,
                                  LOGO_F12TEXT_WIDTH, LOGO_F12TEXT_HEIGHT,
                                  iStep, &pThis->au32LogoPalette[0],
                                  &g_abLogoF12BootText[0], pu8Dst);

                /* Blit the offscreen buffer. */
                if (pThis->vram_size >= LOGO_MAX_SIZE * 2)
                {
                    uint32_t *pu32TmpDst = (uint32_t *)pThis->vram_ptrR3;
                    uint32_t *pu32TmpSrc = (uint32_t *)(pThis->vram_ptrR3 + LOGO_MAX_SIZE);
                    for (int i = 0; i < LOGO_MAX_WIDTH; i++)
                    {
                        for (int j = 0; j < LOGO_MAX_HEIGHT; j++)
                            *pu32TmpDst++ = *pu32TmpSrc++;
                    }
                }

                /* Set the dirty flags. */
                while (offDirty <= LOGO_MAX_SIZE)
                {
                    vga_set_dirty(pThis, offDirty);
                    offDirty += PAGE_SIZE;
                }
                break;
            }

            default:
                Log(("vbeIOPortWriteCMDLogo: invalid command %d\n", u32));
                pThis->LogoCommand = LOGO_CMD_NOP;
                break;
        }

        return VINF_SUCCESS;
    }

    Log(("vbeIOPortWriteCMDLogo: Ignoring invalid cb=%d writes to the VBE Extra port!!!\n", cb));
    return VINF_SUCCESS;
}


/**
 * Port I/O Handler for BIOS Logo IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   Port        Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
PDMBOTHCBDECL(int) vbeIOPortReadCMDLogo(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    PVGASTATE pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    NOREF(pvUser);
    NOREF(Port);

    PRTUINT64U  p;

    if (pThis->offLogoData + cb > pThis->cbLogo)
    {
        Log(("vbeIOPortReadCMDLogo: Requested address is out of Logo data!!! offLogoData=%#x(%d) cbLogo=%#x(%d)\n",
             pThis->offLogoData, pThis->offLogoData, pThis->cbLogo, pThis->cbLogo));
        return VINF_SUCCESS;
    }
    p = (PRTUINT64U)&pThis->pu8Logo[pThis->offLogoData];

    switch (cb)
    {
        case 1: *pu32 = p->au8[0]; break;
        case 2: *pu32 = p->au16[0]; break;
        case 4: *pu32 = p->au32[0]; break;
        //case 8: *pu32 = p->au64[0]; break;
        default: AssertFailed(); break;
    }
    Log(("vbeIOPortReadCMDLogo: LogoOffset=%#x(%d) cb=%#x %.*Rhxs\n", pThis->offLogoData, pThis->offLogoData, cb, cb, pu32));

    pThis->LogoCommand = LOGO_CMD_NOP;
    pThis->offLogoData += cb;

    return VINF_SUCCESS;
}

/**
 * Info handler, device version. Dumps several interesting bits of the
 * VGA state that are difficult to decode from the registers.
 *
 * @param   pDevIns     Device instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
static DECLCALLBACK(void) vgaInfoState(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PVGASTATE       pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    int             is_graph, double_scan;
    int             w, h, char_height, char_dots;
    int             val, vfreq_hz, hfreq_hz;
    vga_retrace_s   *r = &pThis->retrace_state;
    const char      *clocks[] = { "25.175 MHz", "28.322 MHz", "External", "Reserved?!" };
    NOREF(pszArgs);

    is_graph  = pThis->gr[6] & 1;
    char_dots = (pThis->sr[0x01] & 1) ? 8 : 9;
    double_scan = pThis->cr[9] >> 7;
    pHlp->pfnPrintf(pHlp, "pixel clock: %s\n", clocks[(pThis->msr >> 2) & 3]);
    pHlp->pfnPrintf(pHlp, "double scanning %s\n", double_scan ? "on" : "off");
    pHlp->pfnPrintf(pHlp, "double clocking %s\n", pThis->sr[1] & 0x08 ? "on" : "off");
    val = pThis->cr[0] + 5;
    pHlp->pfnPrintf(pHlp, "htotal: %d px (%d cclk)\n", val * char_dots, val);
    val = pThis->cr[6] + ((pThis->cr[7] & 1) << 8) + ((pThis->cr[7] & 0x20) << 4) + 2;
    pHlp->pfnPrintf(pHlp, "vtotal: %d px\n", val);
    val = pThis->cr[1] + 1;
    w   = val * char_dots;
    pHlp->pfnPrintf(pHlp, "hdisp : %d px (%d cclk)\n", w, val);
    val = pThis->cr[0x12] + ((pThis->cr[7] & 2) << 7) + ((pThis->cr[7] & 0x40) << 4) + 1;
    h   = val;
    pHlp->pfnPrintf(pHlp, "vdisp : %d px\n", val);
    val = ((pThis->cr[9] & 0x40) << 3) + ((pThis->cr[7] & 0x10) << 4) + pThis->cr[0x18];
    pHlp->pfnPrintf(pHlp, "split : %d ln\n", val);
    val = (pThis->cr[0xc] << 8) + pThis->cr[0xd];
    pHlp->pfnPrintf(pHlp, "start : %#x\n", val);
    if (!is_graph)
    {
        val = (pThis->cr[9] & 0x1f) + 1;
        char_height = val;
        pHlp->pfnPrintf(pHlp, "char height %d\n", val);
        pHlp->pfnPrintf(pHlp, "text mode %dx%d\n", w / char_dots, h / (char_height << double_scan));
    }
    if (pThis->fRealRetrace)
    {
        val = r->hb_start;
        pHlp->pfnPrintf(pHlp, "hblank start: %d px (%d cclk)\n", val * char_dots, val);
        val = r->hb_end;
        pHlp->pfnPrintf(pHlp, "hblank end  : %d px (%d cclk)\n", val * char_dots, val);
        pHlp->pfnPrintf(pHlp, "vblank start: %d px, end: %d px\n", r->vb_start, r->vb_end);
        pHlp->pfnPrintf(pHlp, "vsync start : %d px, end: %d px\n", r->vs_start, r->vs_end);
        pHlp->pfnPrintf(pHlp, "cclks per frame: %d\n", r->frame_cclks);
        pHlp->pfnPrintf(pHlp, "cclk time (ns) : %d\n", r->cclk_ns);
        vfreq_hz = 1000000000 / r->frame_ns;
        hfreq_hz = 1000000000 / r->h_total_ns;
        pHlp->pfnPrintf(pHlp, "vfreq: %d Hz, hfreq: %d.%03d kHz\n",
                        vfreq_hz, hfreq_hz / 1000, hfreq_hz % 1000);
    }
    pHlp->pfnPrintf(pHlp, "display refresh interval: %u ms\n", pThis->cMilliesRefreshInterval);
}


/**
 * Prints a separator line.
 *
 * @param   pHlp                Callback functions for doing output.
 * @param   cCols               The number of columns.
 * @param   pszTitle            The title text, NULL if none.
 */
static void vgaInfoTextPrintSeparatorLine(PCDBGFINFOHLP pHlp, size_t cCols, const char *pszTitle)
{
    if (pszTitle)
    {
        size_t cchTitle = strlen(pszTitle);
        if (cchTitle + 6 >= cCols)
        {
            pHlp->pfnPrintf(pHlp, "-- %s --", pszTitle);
            cCols = 0;
        }
        else
        {
            size_t cchLeft = (cCols - cchTitle - 2) / 2;
            cCols -= cchLeft + cchTitle + 2;
            while (cchLeft-- > 0)
                pHlp->pfnPrintf(pHlp, "-");
            pHlp->pfnPrintf(pHlp, " %s ", pszTitle);
        }
    }

    while (cCols-- > 0)
        pHlp->pfnPrintf(pHlp, "-");
    pHlp->pfnPrintf(pHlp, "\n");
}


/**
 * Worker for vgaInfoText.
 *
 * @param   pThis       The vga state.
 * @param   pHlp        Callback functions for doing output.
 * @param   offStart    Where to start dumping (relative to the VRAM).
 * @param   cbLine      The source line length (aka line_offset).
 * @param   cCols       The number of columns on the screen.
 * @param   cRows       The number of rows to dump.
 * @param   iScrBegin   The row at which the current screen output starts.
 * @param   iScrEnd     The row at which the current screen output end
 *                      (exclusive).
 */
static void vgaInfoTextWorker(PVGASTATE pThis, PCDBGFINFOHLP pHlp,
                              uint32_t offStart, uint32_t cbLine,
                              uint32_t cCols, uint32_t cRows,
                              uint32_t iScrBegin, uint32_t iScrEnd)
{
    /* Title, */
    char szTitle[32];
    if (iScrBegin || iScrEnd < cRows)
        RTStrPrintf(szTitle, sizeof(szTitle), "%ux%u (+%u before, +%u after)",
                    cCols, iScrEnd - iScrBegin, iScrBegin, cRows - iScrEnd);
    else
        RTStrPrintf(szTitle, sizeof(szTitle), "%ux%u", cCols, iScrEnd - iScrBegin);

    /* Do the dumping. */
    uint8_t const *pbSrcOuter = pThis->CTX_SUFF(vram_ptr) + offStart;
    uint32_t iRow;
    for (iRow = 0; iRow < cRows; iRow++, pbSrcOuter += cbLine)
    {
        if ((uintptr_t)(pbSrcOuter + cbLine - pThis->CTX_SUFF(vram_ptr)) > pThis->vram_size) {
            pHlp->pfnPrintf(pHlp, "The last %u row/rows is/are outside the VRAM.\n", cRows - iRow);
            break;
        }

        if (iRow == 0)
            vgaInfoTextPrintSeparatorLine(pHlp, cCols, szTitle);
        else if (iRow == iScrBegin)
            vgaInfoTextPrintSeparatorLine(pHlp, cCols, "screen start");
        else if (iRow == iScrEnd)
            vgaInfoTextPrintSeparatorLine(pHlp, cCols, "screen end");

        uint8_t const *pbSrc = pbSrcOuter;
        for (uint32_t iCol = 0; iCol < cCols; ++iCol)
        {
            if (RT_C_IS_PRINT(*pbSrc))
                pHlp->pfnPrintf(pHlp, "%c", *pbSrc);
            else
                pHlp->pfnPrintf(pHlp, ".");
            pbSrc += 8;   /* chars are spaced 8 bytes apart */
        }
        pHlp->pfnPrintf(pHlp, "\n");
    }

    /* Final separator. */
    vgaInfoTextPrintSeparatorLine(pHlp, cCols, NULL);
}


/**
 * Info handler, device version. Dumps VGA memory formatted as
 * ASCII text, no attributes. Only looks at the first page.
 *
 * @param   pDevIns     Device instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
static DECLCALLBACK(void) vgaInfoText(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PVGASTATE pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);

    /*
     * Parse args.
     */
    bool fAll = true;
    if (pszArgs && *pszArgs)
    {
        if (!strcmp(pszArgs, "all"))
            fAll = true;
        else if (!strcmp(pszArgs, "scr") || !strcmp(pszArgs, "screen"))
            fAll = false;
        else
        {
            pHlp->pfnPrintf(pHlp, "Invalid argument: '%s'\n", pszArgs);
            return;
        }
    }

    /*
     * Check that we're in text mode and that the VRAM is accessible.
     */
    if (!(pThis->gr[6] & 1))
    {
        uint8_t *pbSrc = pThis->vram_ptrR3;
        if (pbSrc)
        {
            /*
             * Figure out the display size and where the text is.
             *
             * Note! We're cutting quite a few corners here and this code could
             *       do with some brushing up.  Dumping from the start of the
             *       frame buffer is done intentionally so that we're more
             *       likely to obtain the full scrollback of a linux panic.
             */
            uint32_t cbLine;
            uint32_t offStart;
            uint32_t uLineCompareIgn;
            vga_get_offsets(pThis, &cbLine, &offStart, &uLineCompareIgn);
            if (!cbLine)
                cbLine = 80 * 8;
            offStart *= 8;

            uint32_t uVDisp      = pThis->cr[0x12] + ((pThis->cr[7] & 2) << 7) + ((pThis->cr[7] & 0x40) << 4) + 1;
            uint32_t uCharHeight = (pThis->cr[9] & 0x1f) + 1;
            uint32_t uDblScan    = pThis->cr[9] >> 7;
            uint32_t cScrRows    = uVDisp / (uCharHeight << uDblScan);
            if (cScrRows < 25)
                cScrRows = 25;
            uint32_t iScrBegin   = offStart / cbLine;
            uint32_t cRows       = iScrBegin + cScrRows;
            uint32_t cCols       = cbLine / 8;

            if (fAll) {
                vgaInfoTextWorker(pThis, pHlp, offStart - iScrBegin * cbLine, cbLine,
                                  cCols, cRows, iScrBegin, iScrBegin + cScrRows);
            } else {
                vgaInfoTextWorker(pThis, pHlp, offStart, cbLine, cCols, cScrRows, 0, cScrRows);
            }
        }
        else
            pHlp->pfnPrintf(pHlp, "VGA memory not available!\n");
    }
    else
        pHlp->pfnPrintf(pHlp, "Not in text mode!\n");
}


/**
 * Info handler, device version. Dumps VGA Sequencer registers.
 *
 * @param   pDevIns     Device instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
static DECLCALLBACK(void) vgaInfoSR(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PVGASTATE   pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    unsigned    i;
    NOREF(pszArgs);

    pHlp->pfnPrintf(pHlp, "VGA Sequencer (3C5): SR index 3C4:%02X\n", pThis->sr_index);
    Assert(sizeof(pThis->sr) >= 8);
    for (i = 0; i < 5; ++i)
        pHlp->pfnPrintf(pHlp, " SR%02X:%02X", i, pThis->sr[i]);
    pHlp->pfnPrintf(pHlp, "\n");
}


/**
 * Info handler, device version. Dumps VGA CRTC registers.
 *
 * @param   pDevIns     Device instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
static DECLCALLBACK(void) vgaInfoCR(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PVGASTATE   pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    unsigned    i;
    NOREF(pszArgs);

    pHlp->pfnPrintf(pHlp, "VGA CRTC (3D5): CRTC index 3D4:%02X\n", pThis->cr_index);
    Assert(sizeof(pThis->cr) >= 24);
    for (i = 0; i < 10; ++i)
    {
        pHlp->pfnPrintf(pHlp, " CR%02X:%02X", i, pThis->cr[i]);
    }
    pHlp->pfnPrintf(pHlp, "\n");
    for (i = 10; i < 20; ++i)
    {
        pHlp->pfnPrintf(pHlp, " CR%02X:%02X", i, pThis->cr[i]);
    }
    pHlp->pfnPrintf(pHlp, "\n");
    for (i = 20; i < 25; ++i)
    {
        pHlp->pfnPrintf(pHlp, " CR%02X:%02X", i, pThis->cr[i]);
    }
    pHlp->pfnPrintf(pHlp, "\n");
}


/**
 * Info handler, device version. Dumps VGA Graphics Controller registers.
 *
 * @param   pDevIns     Device instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
static DECLCALLBACK(void) vgaInfoGR(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PVGASTATE   pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    unsigned    i;
    NOREF(pszArgs);

    pHlp->pfnPrintf(pHlp, "VGA Graphics Controller (3CF): GR index 3CE:%02X\n", pThis->gr_index);
    Assert(sizeof(pThis->gr) >= 9);
    for (i = 0; i < 9; ++i)
    {
        pHlp->pfnPrintf(pHlp, " GR%02X:%02X", i, pThis->gr[i]);
    }
    pHlp->pfnPrintf(pHlp, "\n");
}


/**
 * Info handler, device version. Dumps VGA Sequencer registers.
 *
 * @param   pDevIns     Device instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
static DECLCALLBACK(void) vgaInfoAR(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PVGASTATE   pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    unsigned    i;
    NOREF(pszArgs);

    pHlp->pfnPrintf(pHlp, "VGA Attribute Controller (3C0): index reg %02X, flip-flop: %d (%s)\n",
                    pThis->ar_index, pThis->ar_flip_flop, pThis->ar_flip_flop ? "data" : "index" );
    Assert(sizeof(pThis->ar) >= 0x14);
    pHlp->pfnPrintf(pHlp, " Palette:");
    for (i = 0; i < 0x10; ++i)
    {
        pHlp->pfnPrintf(pHlp, " %02X", pThis->ar[i]);
    }
    pHlp->pfnPrintf(pHlp, "\n");
    for (i = 0x10; i <= 0x14; ++i)
    {
        pHlp->pfnPrintf(pHlp, " AR%02X:%02X", i, pThis->ar[i]);
    }
    pHlp->pfnPrintf(pHlp, "\n");
}

/**
 * Info handler, device version. Dumps VGA DAC registers.
 *
 * @param   pDevIns     Device instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
static DECLCALLBACK(void) vgaInfoDAC(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PVGASTATE   pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    unsigned    i;
    NOREF(pszArgs);

    pHlp->pfnPrintf(pHlp, "VGA DAC contents:\n");
    for (i = 0; i < 0x100; ++i)
    {
        pHlp->pfnPrintf(pHlp, " %02X: %02X %02X %02X\n",
                        i, pThis->palette[i*3+0], pThis->palette[i*3+1], pThis->palette[i*3+2]);
    }
}


/**
 * Info handler, device version. Dumps VBE registers.
 *
 * @param   pDevIns     Device instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
static DECLCALLBACK(void) vgaInfoVBE(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PVGASTATE   pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    NOREF(pszArgs);

    pHlp->pfnPrintf(pHlp, "LFB at %RGp\n", pThis->GCPhysVRAM);

    if (!(pThis->vbe_regs[VBE_DISPI_INDEX_ENABLE] & VBE_DISPI_ENABLED))
    {
        pHlp->pfnPrintf(pHlp, "VBE disabled\n");
        return;
    }

    pHlp->pfnPrintf(pHlp, "VBE state (chip ID 0x%04x):\n", pThis->vbe_regs[VBE_DISPI_INDEX_ID]);
    pHlp->pfnPrintf(pHlp, " Display resolution: %d x %d @ %dbpp\n",
                    pThis->vbe_regs[VBE_DISPI_INDEX_XRES], pThis->vbe_regs[VBE_DISPI_INDEX_YRES],
                    pThis->vbe_regs[VBE_DISPI_INDEX_BPP]);
    pHlp->pfnPrintf(pHlp, " Virtual resolution: %d x %d\n",
                    pThis->vbe_regs[VBE_DISPI_INDEX_VIRT_WIDTH], pThis->vbe_regs[VBE_DISPI_INDEX_VIRT_HEIGHT]);
    pHlp->pfnPrintf(pHlp, " Display start addr: %d, %d\n",
                    pThis->vbe_regs[VBE_DISPI_INDEX_X_OFFSET], pThis->vbe_regs[VBE_DISPI_INDEX_Y_OFFSET]);
    pHlp->pfnPrintf(pHlp, " Linear scanline pitch: 0x%04x\n", pThis->vbe_line_offset);
    pHlp->pfnPrintf(pHlp, " Linear display start : 0x%04x\n", pThis->vbe_start_addr);
    pHlp->pfnPrintf(pHlp, " Selected bank: 0x%04x\n", pThis->vbe_regs[VBE_DISPI_INDEX_BANK]);
}


/**
 * Info handler, device version. Dumps register state relevant
 * to 16-color planar graphics modes (GR/SR) in human-readable form.
 *
 * @param   pDevIns     Device instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
static DECLCALLBACK(void) vgaInfoPlanar(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PVGASTATE       pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    int             val1, val2;
    NOREF(pszArgs);

    val1 = (pThis->gr[5] >> 3) & 1;
    val2 = pThis->gr[5] & 3;
    pHlp->pfnPrintf(pHlp, "read mode     : %d     write mode: %d\n", val1, val2);
    val1 = pThis->gr[0];
    val2 = pThis->gr[1];
    pHlp->pfnPrintf(pHlp, "set/reset data: %02X    S/R enable: %02X\n", val1, val2);
    val1 = pThis->gr[2];
    val2 = pThis->gr[4] & 3;
    pHlp->pfnPrintf(pHlp, "color compare : %02X    read map  : %d\n", val1, val2);
    val1 = pThis->gr[3] & 7;
    val2 = (pThis->gr[3] >> 3) & 3;
    pHlp->pfnPrintf(pHlp, "rotate        : %d     function  : %d\n", val1, val2);
    val1 = pThis->gr[7];
    val2 = pThis->gr[8];
    pHlp->pfnPrintf(pHlp, "don't care    : %02X    bit mask  : %02X\n", val1, val2);
    val1 = pThis->sr[2];
    val2 = pThis->sr[4] & 8;
    pHlp->pfnPrintf(pHlp, "seq plane mask: %02X    chain-4   : %s\n", val1, val2 ? "on" : "off");
}


/* -=-=-=-=-=- Ring 3: IBase -=-=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) vgaPortQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PVGASTATE pThis = RT_FROM_MEMBER(pInterface, VGASTATE, IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIDISPLAYPORT, &pThis->IPort);
#if defined(VBOX_WITH_HGSMI) && (defined(VBOX_WITH_VIDEOHWACCEL) || defined(VBOX_WITH_CRHGSMI))
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIDISPLAYVBVACALLBACKS, &pThis->IVBVACallbacks);
#endif
    return NULL;
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
static DECLCALLBACK(int) vgaDummyResize(PPDMIDISPLAYCONNECTOR pInterface, uint32_t bpp, void *pvVRAM,
                                        uint32_t cbLine, uint32_t cx, uint32_t cy)
{
    NOREF(pInterface); NOREF(bpp); NOREF(pvVRAM); NOREF(cbLine); NOREF(cx); NOREF(cy);
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
    NOREF(pInterface); NOREF(x); NOREF(y); NOREF(cx); NOREF(cy);
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
    NOREF(pInterface);
}


/* -=-=-=-=-=- Ring 3: IDisplayPort -=-=-=-=-=- */

/** Converts a display port interface pointer to a vga state pointer. */
#define IDISPLAYPORT_2_VGASTATE(pInterface) ( (PVGASTATE)((uintptr_t)pInterface - RT_OFFSETOF(VGASTATE, IPort)) )


/**
 * Update the display with any changed regions.
 *
 * @param   pInterface          Pointer to this interface.
 * @see     PDMIKEYBOARDPORT::pfnUpdateDisplay() for details.
 */
static DECLCALLBACK(int) vgaPortUpdateDisplay(PPDMIDISPLAYPORT pInterface)
{
    PVGASTATE pThis = IDISPLAYPORT_2_VGASTATE(pInterface);
    PDMDEV_ASSERT_EMT(VGASTATE2DEVINS(pThis));
    PPDMDEVINS pDevIns = pThis->CTX_SUFF(pDevIns);

    int rc = PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);
    AssertRC(rc);

#ifndef VBOX_WITH_HGSMI
    /* This should be called only in non VBVA mode. */
#else
    if (VBVAUpdateDisplay (pThis) == VINF_SUCCESS)
    {
        PDMCritSectLeave(&pThis->CritSect);
        return VINF_SUCCESS;
    }
#endif /* VBOX_WITH_HGSMI */

    STAM_COUNTER_INC(&pThis->StatUpdateDisp);
    if (pThis->fHasDirtyBits && pThis->GCPhysVRAM && pThis->GCPhysVRAM != NIL_RTGCPHYS)
    {
        PGMHandlerPhysicalReset(PDMDevHlpGetVM(pDevIns), pThis->GCPhysVRAM);
        pThis->fHasDirtyBits = false;
    }
    if (pThis->fRemappedVGA)
    {
        IOMMMIOResetRegion(PDMDevHlpGetVM(pDevIns), 0x000a0000);
        pThis->fRemappedVGA = false;
    }

    rc = vga_update_display(pThis, false, false, true);
    PDMCritSectLeave(&pThis->CritSect);
    return rc;
}


/**
 * Internal vgaPortUpdateDisplayAll worker called under pThis->CritSect.
 */
static int updateDisplayAll(PVGASTATE pThis)
{
    PPDMDEVINS pDevIns = pThis->CTX_SUFF(pDevIns);

    /* The dirty bits array has been just cleared, reset handlers as well. */
    if (pThis->GCPhysVRAM && pThis->GCPhysVRAM != NIL_RTGCPHYS)
        PGMHandlerPhysicalReset(PDMDevHlpGetVM(pDevIns), pThis->GCPhysVRAM);
    if (pThis->fRemappedVGA)
    {
        IOMMMIOResetRegion(PDMDevHlpGetVM(pDevIns), 0x000a0000);
        pThis->fRemappedVGA = false;
    }

    pThis->graphic_mode = -1; /* force full update */

    return vga_update_display(pThis, true, false, true);
}


/**
 * Update the entire display.
 *
 * @param   pInterface          Pointer to this interface.
 * @see     PDMIKEYBOARDPORT::pfnUpdateDisplayAll() for details.
 */
static DECLCALLBACK(int) vgaPortUpdateDisplayAll(PPDMIDISPLAYPORT pInterface)
{
    PVGASTATE pThis = IDISPLAYPORT_2_VGASTATE(pInterface);
    PDMDEV_ASSERT_EMT(VGASTATE2DEVINS(pThis));

    /* This is called both in VBVA mode and normal modes. */

#ifdef DEBUG_sunlover
    LogFlow(("vgaPortUpdateDisplayAll\n"));
#endif /* DEBUG_sunlover */

    int rc = PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);
    AssertRC(rc);

    rc = updateDisplayAll(pThis);

    PDMCritSectLeave(&pThis->CritSect);
    return rc;
}


/**
 * Sets the refresh rate and restart the timer.
 *
 * @returns VBox status code.
 * @param   pInterface          Pointer to this interface.
 * @param   cMilliesInterval    Number of millis between two refreshes.
 * @see     PDMIKEYBOARDPORT::pfnSetRefreshRate() for details.
 */
static DECLCALLBACK(int) vgaPortSetRefreshRate(PPDMIDISPLAYPORT pInterface, uint32_t cMilliesInterval)
{
    PVGASTATE pThis = IDISPLAYPORT_2_VGASTATE(pInterface);

    pThis->cMilliesRefreshInterval = cMilliesInterval;
    if (cMilliesInterval)
        return TMTimerSetMillies(pThis->RefreshTimer, cMilliesInterval);
    return TMTimerStop(pThis->RefreshTimer);
}


/** @copydoc PDMIDISPLAYPORT::pfnQueryColorDepth */
static DECLCALLBACK(int) vgaPortQueryColorDepth(PPDMIDISPLAYPORT pInterface, uint32_t *pcBits)
{
    PVGASTATE pThis = IDISPLAYPORT_2_VGASTATE(pInterface);

    if (!pcBits)
        return VERR_INVALID_PARAMETER;
    *pcBits = vga_get_bpp(pThis);
    return VINF_SUCCESS;
}


/**
 * Create a 32-bbp screenshot of the display. Size of the bitmap scanline in bytes is 4*width.
 *
 * @param   pInterface          Pointer to this interface.
 * @param   ppu8Data            Where to store the pointer to the allocated buffer.
 * @param   pcbData             Where to store the actual size of the bitmap.
 * @param   pcx                 Where to store the width of the bitmap.
 * @param   pcy                 Where to store the height of the bitmap.
 * @see     PDMIDISPLAYPORT::pfnTakeScreenshot() for details.
 */
static DECLCALLBACK(int) vgaPortTakeScreenshot(PPDMIDISPLAYPORT pInterface, uint8_t **ppu8Data, size_t *pcbData, uint32_t *pcx, uint32_t *pcy)
{
    PVGASTATE pThis = IDISPLAYPORT_2_VGASTATE(pInterface);
    PDMDEV_ASSERT_EMT(VGASTATE2DEVINS(pThis));

    LogFlow(("vgaPortTakeScreenshot: ppu8Data=%p pcbData=%p pcx=%p pcy=%p\n", ppu8Data, pcbData, pcx, pcy));

    /*
     * Validate input.
     */
    if (!RT_VALID_PTR(ppu8Data) || !RT_VALID_PTR(pcbData) || !RT_VALID_PTR(pcx) || !RT_VALID_PTR(pcy))
        return VERR_INVALID_PARAMETER;

    int rc = PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);
    AssertRCReturn(rc, rc);

    /*
     * Get screenshot. This function will fail if a resize is required.
     * So there is not need to do a 'updateDisplayAll' before taking screenshot.
     */

    /*
     * Allocate the buffer for 32 bits per pixel bitmap
     *
     * Note! The size can't be zero or greater than the size of the VRAM.
     *       Inconsistent VGA device state can cause the incorrect size values.
     */
    size_t cbRequired = pThis->last_scr_width * 4 * pThis->last_scr_height;
    if (cbRequired && cbRequired <= pThis->vram_size)
    {
        uint8_t *pu8Data = (uint8_t *)RTMemAlloc(cbRequired);
        if (pu8Data != NULL)
        {
            /*
             * Only 3 methods, assigned below, will be called during the screenshot update.
             * All other are already set to NULL.
             */
            /* The display connector interface is temporarily replaced with the fake one. */
            PDMIDISPLAYCONNECTOR Connector;
            RT_ZERO(Connector);
            Connector.pu8Data       = pu8Data;
            Connector.cBits         = 32;
            Connector.cx            = pThis->last_scr_width;
            Connector.cy            = pThis->last_scr_height;
            Connector.cbScanline    = Connector.cx * 4;
            Connector.pfnRefresh    = vgaDummyRefresh;
            Connector.pfnResize     = vgaDummyResize;
            Connector.pfnUpdateRect = vgaDummyUpdateRect;

            /* Save & replace state data. */
            PPDMIDISPLAYCONNECTOR pConnectorSaved = pThis->pDrv;
            int32_t graphic_mode_saved = pThis->graphic_mode;
            bool fRenderVRAMSaved = pThis->fRenderVRAM;

            pThis->pDrv = &Connector;
            pThis->graphic_mode = -1;           /* force a full refresh. */
            pThis->fRenderVRAM = 1;             /* force the guest VRAM rendering to the given buffer. */

            /*
             * Make the screenshot.
             *
             * The second parameter is 'false' because the current display state is being rendered to an
             * external buffer using a fake connector. That is if display is blanked, we expect a black
             * screen in the external buffer.
             * If there is a pending resize, the function will fail.
             */
            rc = vga_update_display(pThis, false, true, false);

            /* Restore. */
            pThis->pDrv = pConnectorSaved;
            pThis->graphic_mode = graphic_mode_saved;
            pThis->fRenderVRAM = fRenderVRAMSaved;

            if (rc == VINF_SUCCESS)
            {
                /*
                 * Return the result.
                 */
                *ppu8Data = pu8Data;
                *pcbData = cbRequired;
                *pcx = Connector.cx;
                *pcy = Connector.cy;
            }
            else
            {
                /* If we do not return a success, then the data buffer must be freed. */
                RTMemFree(pu8Data);
                if (RT_SUCCESS_NP(rc))
                {
                    AssertMsgFailed(("%Rrc\n", rc));
                    rc = VERR_INTERNAL_ERROR_5;
                }
            }
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = VERR_NOT_SUPPORTED;

    PDMCritSectLeave(&pThis->CritSect);

    LogFlow(("vgaPortTakeScreenshot: returns %Rrc (cbData=%d cx=%d cy=%d)\n", rc, *pcbData, *pcx, *pcy));
    return rc;
}

/**
 * Free a screenshot buffer allocated in vgaPortTakeScreenshot.
 *
 * @param   pInterface          Pointer to this interface.
 * @param   pu8Data             Pointer returned by vgaPortTakeScreenshot.
 * @see     PDMIDISPLAYPORT::pfnFreeScreenshot() for details.
 */
static DECLCALLBACK(void) vgaPortFreeScreenshot(PPDMIDISPLAYPORT pInterface, uint8_t *pu8Data)
{
    NOREF(pInterface);

    LogFlow(("vgaPortFreeScreenshot: pu8Data=%p\n", pu8Data));

    RTMemFree(pu8Data);
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
    PVGASTATE       pThis = IDISPLAYPORT_2_VGASTATE(pInterface);
    int             rc = VINF_SUCCESS;
    PDMDEV_ASSERT_EMT(VGASTATE2DEVINS(pThis));
    LogFlow(("vgaPortDisplayBlt: pvData=%p x=%d y=%d cx=%d cy=%d\n", pvData, x, y, cx, cy));

    rc = PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);
    AssertRC(rc);

    /*
     * Validate input.
     */
    if (    pvData
        &&  x      <  pThis->pDrv->cx
        &&  cx     <= pThis->pDrv->cx
        &&  cx + x <= pThis->pDrv->cx
        &&  y      <  pThis->pDrv->cy
        &&  cy     <= pThis->pDrv->cy
        &&  cy + y <= pThis->pDrv->cy)
    {
        /*
         * Determine bytes per pixel in the destination buffer.
         */
        size_t  cbPixelDst = 0;
        switch (pThis->pDrv->cBits)
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
        if (RT_SUCCESS(rc))
        {
            /*
             * The blitting loop.
             */
            size_t      cbLineSrc   = cx * 4; /* 32 bits per pixel. */
            uint8_t    *pu8Src      = (uint8_t *)pvData;
            size_t      cbLineDst   = pThis->pDrv->cbScanline;
            uint8_t    *pu8Dst      = pThis->pDrv->pu8Data + y * cbLineDst + x * cbPixelDst;
            uint32_t    cyLeft      = cy;
            vga_draw_line_func *pfnVgaDrawLine = vga_draw_line_table[VGA_DRAW_LINE32 * 4 + get_depth_index(pThis->pDrv->cBits)];
            Assert(pfnVgaDrawLine);
            while (cyLeft-- > 0)
            {
                pfnVgaDrawLine(pThis, pu8Dst, pu8Src, cx);
                pu8Dst += cbLineDst;
                pu8Src += cbLineSrc;
            }

            /*
             * Invalidate the area.
             */
            pThis->pDrv->pfnUpdateRect(pThis->pDrv, x, y, cx, cy);
        }
    }
    else
        rc = VERR_INVALID_PARAMETER;

    PDMCritSectLeave(&pThis->CritSect);

    LogFlow(("vgaPortDisplayBlt: returns %Rrc\n", rc));
    return rc;
}

static DECLCALLBACK(void) vgaPortUpdateDisplayRect(PPDMIDISPLAYPORT pInterface, int32_t x, int32_t y, uint32_t w, uint32_t h)
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

    PVGASTATE pThis = IDISPLAYPORT_2_VGASTATE(pInterface);

#ifdef DEBUG_sunlover
    LogFlow(("vgaPortUpdateDisplayRect: %d,%d %dx%d\n", x, y, w, h));
#endif /* DEBUG_sunlover */

    Assert(pInterface);
    Assert(pThis->pDrv);
    Assert(pThis->pDrv->pu8Data);

    /* Check if there is something to do at all. */
    if (!pThis->fRenderVRAM)
    {
        /* The framebuffer uses the guest VRAM directly. */
#ifdef DEBUG_sunlover
        LogFlow(("vgaPortUpdateDisplayRect: nothing to do fRender is false.\n"));
#endif /* DEBUG_sunlover */
        return;
    }

    int rc = PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);
    AssertRC(rc);

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
    if (x + w > pThis->pDrv->cx)
    {
        // x < 0 is not possible here
        w = pThis->pDrv->cx > (uint32_t)x? pThis->pDrv->cx - x: 0;
    }

    if (y + h > pThis->pDrv->cy)
    {
        // y < 0 is not possible here
        h = pThis->pDrv->cy > (uint32_t)y? pThis->pDrv->cy - y: 0;
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
        PDMCritSectLeave(&pThis->CritSect);
        return;
    }

    /** @todo This method should be made universal and not only for VBVA.
     *  VGA_DRAW_LINE* must be selected and src/dst address calculation
     *  changed.
     */

    /* Choose the rendering function. */
    switch(pThis->get_bpp(pThis))
    {
        default:
        case 0:
            /* A LFB mode is already disabled, but the callback is still called
             * by Display because VBVA buffer is being flushed.
             * Nothing to do, just return.
             */
            PDMCritSectLeave(&pThis->CritSect);
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

    vga_draw_line = vga_draw_line_table[v * 4 + get_depth_index(pThis->pDrv->cBits)];

    /* Compute source and destination addresses and pitches. */
    cbPixelDst = (pThis->pDrv->cBits + 7) / 8;
    cbLineDst  = pThis->pDrv->cbScanline;
    pu8Dst     = pThis->pDrv->pu8Data + y * cbLineDst + x * cbPixelDst;

    cbPixelSrc = (pThis->get_bpp(pThis) + 7) / 8;
    pThis->get_offsets(pThis, &cbLineSrc, &u32OffsetSrc, &u32Dummy);

    /* Assume that rendering is performed only on visible part of VRAM.
     * This is true because coordinates were verified.
     */
    pu8Src = pThis->vram_ptrR3;
    pu8Src += u32OffsetSrc * 4 + y * cbLineSrc + x * cbPixelSrc;

    /* Render VRAM to framebuffer. */

#ifdef DEBUG_sunlover
    LogFlow(("vgaPortUpdateDisplayRect: dst: %p, %d, %d. src: %p, %d, %d\n", pu8Dst, cbLineDst, cbPixelDst, pu8Src, cbLineSrc, cbPixelSrc));
#endif /* DEBUG_sunlover */

    while (h-- > 0)
    {
        vga_draw_line (pThis, pu8Dst, pu8Src, w);
        pu8Dst += cbLineDst;
        pu8Src += cbLineSrc;
    }

    PDMCritSectLeave(&pThis->CritSect);
#ifdef DEBUG_sunlover
    LogFlow(("vgaPortUpdateDisplayRect: completed.\n"));
#endif /* DEBUG_sunlover */
}

static DECLCALLBACK(int) vgaPortCopyRect (PPDMIDISPLAYPORT pInterface,
                                          uint32_t w,
                                          uint32_t h,
                                          const uint8_t *pu8Src,
                                          int32_t xSrc,
                                          int32_t ySrc,
                                          uint32_t u32SrcWidth,
                                          uint32_t u32SrcHeight,
                                          uint32_t u32SrcLineSize,
                                          uint32_t u32SrcBitsPerPixel,
                                          uint8_t *pu8Dst,
                                          int32_t xDst,
                                          int32_t yDst,
                                          uint32_t u32DstWidth,
                                          uint32_t u32DstHeight,
                                          uint32_t u32DstLineSize,
                                          uint32_t u32DstBitsPerPixel)
{
    uint32_t v;
    vga_draw_line_func *vga_draw_line;

    uint32_t cbPixelDst;
    uint32_t cbLineDst;
    uint8_t *pu8DstPtr;

    uint32_t cbPixelSrc;
    uint32_t cbLineSrc;
    const uint8_t *pu8SrcPtr;

#ifdef DEBUG_sunlover
    LogFlow(("vgaPortCopyRect: %d,%d %dx%d -> %d,%d\n", xSrc, ySrc, w, h, xDst, yDst));
#endif /* DEBUG_sunlover */

    PVGASTATE pThis = IDISPLAYPORT_2_VGASTATE(pInterface);

    Assert(pInterface);
    Assert(pThis->pDrv);

    int32_t xSrcCorrected = xSrc;
    int32_t ySrcCorrected = ySrc;
    uint32_t wCorrected = w;
    uint32_t hCorrected = h;

    /* Correct source coordinates to be within the source bitmap. */
    if (xSrcCorrected < 0)
    {
        xSrcCorrected += wCorrected; /* Compute xRight which is also the new width. */
        wCorrected = (xSrcCorrected < 0) ? 0 : xSrcCorrected;
        xSrcCorrected = 0;
    }

    if (ySrcCorrected < 0)
    {
        ySrcCorrected += hCorrected; /* Compute yBottom, which is also the new height. */
        hCorrected = (ySrcCorrected < 0) ? 0 : ySrcCorrected;
        ySrcCorrected = 0;
    }

    /* Also check if coords are greater than the display resolution. */
    if (xSrcCorrected + wCorrected > u32SrcWidth)
    {
        /* xSrcCorrected < 0 is not possible here */
        wCorrected = u32SrcWidth > (uint32_t)xSrcCorrected? u32SrcWidth - xSrcCorrected: 0;
    }

    if (ySrcCorrected + hCorrected > u32SrcHeight)
    {
        /* y < 0 is not possible here */
        hCorrected = u32SrcHeight > (uint32_t)ySrcCorrected? u32SrcHeight - ySrcCorrected: 0;
    }

#ifdef DEBUG_sunlover
    LogFlow(("vgaPortCopyRect: %d,%d %dx%d (corrected coords)\n", xSrcCorrected, ySrcCorrected, wCorrected, hCorrected));
#endif /* DEBUG_sunlover */

    /* Check if there is something to do at all. */
    if (wCorrected == 0 || hCorrected == 0)
    {
        /* Empty rectangle. */
#ifdef DEBUG_sunlover
        LogFlow(("vgaPortUpdateDisplayRectEx: nothing to do: %dx%d\n", wCorrected, hCorrected));
#endif /* DEBUG_sunlover */
        return VINF_SUCCESS;
    }

    /* Check that the corrected source rectangle is within the destination.
     * Note: source rectangle is adjusted, but the target must be large enough.
     */
    if (   xDst < 0
        || yDst < 0
        || xDst + wCorrected > u32DstWidth
        || yDst + hCorrected > u32DstHeight)
    {
        return VERR_INVALID_PARAMETER;
    }

    /* Choose the rendering function. */
    switch(u32SrcBitsPerPixel)
    {
        default:
        case 0:
            /* Nothing to do, just return. */
            return VINF_SUCCESS;
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

    int rc = PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);
    AssertRC(rc);

    vga_draw_line = vga_draw_line_table[v * 4 + get_depth_index(u32DstBitsPerPixel)];

    /* Compute source and destination addresses and pitches. */
    cbPixelDst = (u32DstBitsPerPixel + 7) / 8;
    cbLineDst  = u32DstLineSize;
    pu8DstPtr  = pu8Dst + yDst * cbLineDst + xDst * cbPixelDst;

    cbPixelSrc = (u32SrcBitsPerPixel + 7) / 8;
    cbLineSrc = u32SrcLineSize;
    pu8SrcPtr = pu8Src + ySrcCorrected * cbLineSrc + xSrcCorrected * cbPixelSrc;

#ifdef DEBUG_sunlover
    LogFlow(("vgaPortCopyRect: dst: %p, %d, %d. src: %p, %d, %d\n", pu8DstPtr, cbLineDst, cbPixelDst, pu8SrcPtr, cbLineSrc, cbPixelSrc));
#endif /* DEBUG_sunlover */

    while (hCorrected-- > 0)
    {
        vga_draw_line (pThis, pu8DstPtr, pu8SrcPtr, wCorrected);
        pu8DstPtr += cbLineDst;
        pu8SrcPtr += cbLineSrc;
    }

    PDMCritSectLeave(&pThis->CritSect);
#ifdef DEBUG_sunlover
    LogFlow(("vgaPortCopyRect: completed.\n"));
#endif /* DEBUG_sunlover */

    return VINF_SUCCESS;
}

static DECLCALLBACK(void) vgaPortSetRenderVRAM(PPDMIDISPLAYPORT pInterface, bool fRender)
{
    PVGASTATE pThis = IDISPLAYPORT_2_VGASTATE(pInterface);

    LogFlow(("vgaPortSetRenderVRAM: fRender = %d\n", fRender));

    pThis->fRenderVRAM = fRender;
}


static DECLCALLBACK(void) vgaTimerRefresh(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    PVGASTATE pThis = (PVGASTATE)pvUser;
    NOREF(pDevIns);

    if (pThis->fScanLineCfg & VBVASCANLINECFG_ENABLE_VSYNC_IRQ)
    {
        VBVARaiseIrq(pThis, HGSMIHOSTFLAGS_VSYNC);
    }

    if (pThis->pDrv)
        pThis->pDrv->pfnRefresh(pThis->pDrv);

    if (pThis->cMilliesRefreshInterval)
        TMTimerSetMillies(pTimer, pThis->cMilliesRefreshInterval);
}


/* -=-=-=-=-=- Ring 3: PCI Device -=-=-=-=-=- */

/**
 * Callback function for unmapping and/or mapping the VRAM MMIO2 region (called by the PCI bus).
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
    PPDMDEVINS  pDevIns = pPciDev->pDevIns;
    PVGASTATE   pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    LogFlow(("vgaR3IORegionMap: iRegion=%d GCPhysAddress=%RGp cb=%#x enmType=%d\n", iRegion, GCPhysAddress, cb, enmType));
    AssertReturn(iRegion == 0 && enmType == PCI_ADDRESS_SPACE_MEM_PREFETCH, VERR_INTERNAL_ERROR);

    if (GCPhysAddress != NIL_RTGCPHYS)
    {
        /*
         * Mapping the VRAM.
         */
        rc = PDMDevHlpMMIO2Map(pDevIns, iRegion, GCPhysAddress);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            rc = PGMR3HandlerPhysicalRegister(PDMDevHlpGetVM(pDevIns),
                                              PGMPHYSHANDLERTYPE_PHYSICAL_WRITE,
                                              GCPhysAddress, GCPhysAddress + (pThis->vram_size - 1),
                                              vgaR3LFBAccessHandler, pThis,
                                              g_DeviceVga.szR0Mod, "vgaR0LFBAccessHandler", pDevIns->pvInstanceDataR0,
                                              g_DeviceVga.szRCMod, "vgaGCLFBAccessHandler", pDevIns->pvInstanceDataRC,
                                              "VGA LFB");
            AssertRC(rc);
            if (RT_SUCCESS(rc))
            {
                pThis->GCPhysVRAM = GCPhysAddress;
                pThis->vbe_regs[VBE_DISPI_INDEX_FB_BASE_HI] = GCPhysAddress >> 16;
            }
        }
    }
    else
    {
        /*
         * Unmapping of the VRAM in progress.
         * Deregister the access handler so PGM doesn't get upset.
         */
        Assert(pThis->GCPhysVRAM);
        rc = PGMHandlerPhysicalDeregister(PDMDevHlpGetVM(pDevIns), pThis->GCPhysVRAM);
        AssertRC(rc);
        pThis->GCPhysVRAM = 0;
        /* NB: VBE_DISPI_INDEX_FB_BASE_HI is left unchanged here. */
    }
    return rc;
}


/* -=-=-=-=-=- Ring3: Misc Wrappers & Sidekicks -=-=-=-=-=- */

/**
 * Saves a important bits of the VGA device config.
 *
 * @param   pThis       The VGA instance data.
 * @param   pSSM        The saved state handle.
 */
static void vgaR3SaveConfig(PVGASTATE pThis, PSSMHANDLE pSSM)
{
    SSMR3PutU32(pSSM, pThis->vram_size);
    SSMR3PutU32(pSSM, pThis->cMonitors);
}


/**
 * @copydoc FNSSMDEVLIVEEXEC
 */
static DECLCALLBACK(int) vgaR3LiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    PVGASTATE pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    Assert(uPass == 0); NOREF(uPass);
    vgaR3SaveConfig(pThis, pSSM);
    return VINF_SSM_DONT_CALL_AGAIN;
}


/**
 * @copydoc FNSSMDEVSAVEPREP
 */
static DECLCALLBACK(int) vgaR3SavePrep(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
#ifdef VBOX_WITH_VIDEOHWACCEL
    return vboxVBVASaveStatePrep(pDevIns, pSSM);
#else
    return VINF_SUCCESS;
#endif
}

static DECLCALLBACK(int) vgaR3SaveDone(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
#ifdef VBOX_WITH_VIDEOHWACCEL
    return vboxVBVASaveStateDone(pDevIns, pSSM);
#else
    return VINF_SUCCESS;
#endif
}

/**
 * @copydoc FNSSMDEVSAVEEXEC
 */
static DECLCALLBACK(int) vgaR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PVGASTATE pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
#ifdef VBOX_WITH_VDMA
    vboxVDMASaveStateExecPrep(pThis->pVdma, pSSM);
#endif
    vgaR3SaveConfig(pThis, pSSM);
    vga_save(pSSM, PDMINS_2_DATA(pDevIns, PVGASTATE));
#ifdef VBOX_WITH_HGSMI
    SSMR3PutBool(pSSM, true);
    int rc = vboxVBVASaveStateExec(pDevIns, pSSM);
# ifdef VBOX_WITH_VDMA
    vboxVDMASaveStateExecDone(pThis->pVdma, pSSM);
# endif
    return rc;
#else
    SSMR3PutBool(pSSM, false);
    return VINF_SUCCESS;
#endif
}


/**
 * @copydoc FNSSMDEVSAVEEXEC
 */
static DECLCALLBACK(int) vgaR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PVGASTATE   pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    int         rc;

    if (uVersion < VGA_SAVEDSTATE_VERSION_ANCIENT || uVersion > VGA_SAVEDSTATE_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    if (uVersion > VGA_SAVEDSTATE_VERSION_HGSMI)
    {
        /* Check the config */
        uint32_t cbVRam;
        rc = SSMR3GetU32(pSSM, &cbVRam);
        AssertRCReturn(rc, rc);
        if (pThis->vram_size != cbVRam)
            return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("VRAM size changed: config=%#x state=%#x"), pThis->vram_size, cbVRam);

        uint32_t cMonitors;
        rc = SSMR3GetU32(pSSM, &cMonitors);
        AssertRCReturn(rc, rc);
        if (pThis->cMonitors != cMonitors)
            return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Monitor count changed: config=%u state=%u"), pThis->cMonitors, cMonitors);
    }

    if (uPass == SSM_PASS_FINAL)
    {
        rc = vga_load(pSSM, pThis, uVersion);
        if (RT_FAILURE(rc))
            return rc;
        bool fWithHgsmi = uVersion == VGA_SAVEDSTATE_VERSION_HGSMI;
        if (uVersion > VGA_SAVEDSTATE_VERSION_HGSMI)
        {
            rc = SSMR3GetBool(pSSM, &fWithHgsmi);
            AssertRCReturn(rc, rc);
        }
        if (fWithHgsmi)
        {
#ifdef VBOX_WITH_HGSMI
            rc = vboxVBVALoadStateExec(pDevIns, pSSM, uVersion);
            AssertRCReturn(rc, rc);
#else
            return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("HGSMI is not compiled in, but it is present in the saved state"));
#endif
        }
    }
    return VINF_SUCCESS;
}


/**
 * @copydoc FNSSMDEVLOADDONE
 */
static DECLCALLBACK(int) vgaR3LoadDone(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
#ifdef VBOX_WITH_HGSMI
    return vboxVBVALoadStateDone(pDevIns, pSSM);
#else
    return VINF_SUCCESS;
#endif
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
    PVGASTATE       pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    char           *pchStart;
    char           *pchEnd;
    LogFlow(("vgaReset\n"));

#ifdef VBOX_WITH_HGSMI
    VBVAReset(pThis);
#endif /* VBOX_WITH_HGSMI */


    /* Clear the VRAM ourselves. */
    if (pThis->vram_ptrR3 && pThis->vram_size)
        memset(pThis->vram_ptrR3, 0, pThis->vram_size);

    /*
     * Zero most of it.
     *
     * Unlike vga_reset we're leaving out a few members which we believe
     * must remain unchanged....
     */
    /* 1st part. */
    pchStart = (char *)&pThis->latch;
    pchEnd   = (char *)&pThis->invalidated_y_table;
    memset(pchStart, 0, pchEnd - pchStart);

    /* 2nd part. */
    pchStart = (char *)&pThis->last_palette;
    pchEnd   = (char *)&pThis->u32Marker;
    memset(pchStart, 0, pchEnd - pchStart);


    /*
     * Restore and re-init some bits.
     */
    pThis->get_bpp        = vga_get_bpp;
    pThis->get_offsets    = vga_get_offsets;
    pThis->get_resolution = vga_get_resolution;
    pThis->graphic_mode   = -1;         /* Force full update. */
#ifdef CONFIG_BOCHS_VBE
    pThis->vbe_regs[VBE_DISPI_INDEX_ID] = VBE_DISPI_ID0;
    pThis->vbe_regs[VBE_DISPI_INDEX_VBOX_VIDEO] = 0;
    pThis->vbe_regs[VBE_DISPI_INDEX_FB_BASE_HI] = pThis->GCPhysVRAM >> 16;
    pThis->vbe_bank_max   = (pThis->vram_size >> 16) - 1;
#endif /* CONFIG_BOCHS_VBE */

    /*
     * Reset the LBF mapping.
     */
    pThis->fLFBUpdated = false;
    if (    (   pThis->fGCEnabled
             || pThis->fR0Enabled)
        &&  pThis->GCPhysVRAM
        &&  pThis->GCPhysVRAM != NIL_RTGCPHYS)
    {
        int rc = PGMHandlerPhysicalReset(PDMDevHlpGetVM(pDevIns), pThis->GCPhysVRAM);
        AssertRC(rc);
    }
    if (pThis->fRemappedVGA)
    {
        IOMMMIOResetRegion(PDMDevHlpGetVM(pDevIns), 0x000a0000);
        pThis->fRemappedVGA = false;
    }

    /*
     * Reset the logo data.
     */
    pThis->LogoCommand = LOGO_CMD_NOP;
    pThis->offLogoData = 0;

    /* notify port handler */
    if (pThis->pDrv)
        pThis->pDrv->pfnReset(pThis->pDrv);

    /* Reset latched access mask. */
    pThis->uMaskLatchAccess     = 0x3ff;
    pThis->cLatchAccesses       = 0;
    pThis->u64LastLatchedAccess = 0;
    pThis->iMask                = 0;

    /* Reset retrace emulation. */
    memset(&pThis->retrace_state, 0, sizeof(pThis->retrace_state));
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
        PVGASTATE pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
        LogFlow(("vgaRelocate: offDelta = %08X\n", offDelta));

        pThis->RCPtrLFBHandler += offDelta;
        pThis->vram_ptrRC += offDelta;
        pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
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
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 */
static DECLCALLBACK(int)  vgaAttach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PVGASTATE   pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);

    AssertMsgReturn(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG,
                    ("VGA device does not support hotplugging\n"),
                    VERR_INVALID_PARAMETER);

    switch (iLUN)
    {
        /* LUN #0: Display port. */
        case 0:
        {
            int rc = PDMDevHlpDriverAttach(pDevIns, iLUN, &pThis->IBase, &pThis->pDrvBase, "Display Port");
            if (RT_SUCCESS(rc))
            {
                pThis->pDrv = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMIDISPLAYCONNECTOR);
                if (pThis->pDrv)
                {
                    /* pThis->pDrv->pu8Data can be NULL when there is no framebuffer. */
                    if (    pThis->pDrv->pfnRefresh
                        &&  pThis->pDrv->pfnResize
                        &&  pThis->pDrv->pfnUpdateRect)
                        rc = VINF_SUCCESS;
                    else
                    {
                        Assert(pThis->pDrv->pfnRefresh);
                        Assert(pThis->pDrv->pfnResize);
                        Assert(pThis->pDrv->pfnUpdateRect);
                        pThis->pDrv = NULL;
                        pThis->pDrvBase = NULL;
                        rc = VERR_INTERNAL_ERROR;
                    }
#ifdef VBOX_WITH_VIDEOHWACCEL
                    if(rc == VINF_SUCCESS)
                    {
                        rc = vbvaVHWAConstruct(pThis);
                        if (rc != VERR_NOT_IMPLEMENTED)
                            AssertRC(rc);
                    }
#endif
                }
                else
                {
                    AssertMsgFailed(("LUN #0 doesn't have a display connector interface! rc=%Rrc\n", rc));
                    pThis->pDrvBase = NULL;
                    rc = VERR_PDM_MISSING_INTERFACE;
                }
            }
            else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
            {
                Log(("%s/%d: warning: no driver attached to LUN #0!\n", pDevIns->pReg->szName, pDevIns->iInstance));
                rc = VINF_SUCCESS;
            }
            else
                AssertLogRelMsgFailed(("Failed to attach LUN #0! rc=%Rrc\n", rc));
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
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 */
static DECLCALLBACK(void)  vgaDetach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    /*
     * Reset the interfaces and update the controller state.
     */
    PVGASTATE   pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);

    AssertMsg(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG,
              ("VGA device does not support hotplugging\n"));

    switch (iLUN)
    {
        /* LUN #0: Display port. */
        case 0:
            pThis->pDrv = NULL;
            pThis->pDrvBase = NULL;
            break;

        default:
            AssertMsgFailed(("Invalid LUN #%d\n", iLUN));
            break;
    }
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
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);

#ifdef VBE_NEW_DYN_LIST
    PVGASTATE   pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    LogFlow(("vgaR3Destruct:\n"));

#ifdef VBOX_WITH_VDMA
    vboxVDMADestruct(pThis->pVdma);
#endif

    /*
     * Free MM heap pointers.
     */
    if (pThis->pu8VBEExtraData)
    {
        MMR3HeapFree(pThis->pu8VBEExtraData);
        pThis->pu8VBEExtraData = NULL;
    }
#endif
    if (pThis->pu8VgaBios)
    {
        MMR3HeapFree(pThis->pu8VgaBios);
        pThis->pu8VgaBios = NULL;
    }

    if (pThis->pszVgaBiosFile)
    {
        MMR3HeapFree(pThis->pszVgaBiosFile);
        pThis->pszVgaBiosFile = NULL;
    }

    if (pThis->pszLogoFile)
    {
        MMR3HeapFree(pThis->pszLogoFile);
        pThis->pszLogoFile = NULL;
    }

    PDMR3CritSectDelete(&pThis->CritSect);
    return VINF_SUCCESS;
}

/**
 * Adjust VBE mode information
 *
 * Depending on the configured VRAM size, certain parts of VBE mode
 * information must be updated.
 *
 * @param   pThis       The device instance data.
 * @param   pMode       The mode information structure.
 */
static void vgaAdjustModeInfo(PVGASTATE pThis, ModeInfoListItem *pMode)
{
    int         maxPage;
    int         bpl;


    /* For 4bpp modes, the planes are "stacked" on top of each other. */
    bpl = pMode->info.BytesPerScanLine * pMode->info.NumberOfPlanes;
    /* The "number of image pages" is really the max page index... */
    maxPage = pThis->vram_size / (pMode->info.YResolution * bpl) - 1;
    Assert(maxPage >= 0);
    if (maxPage > 255)
        maxPage = 255;  /* 8-bit value. */
    pMode->info.NumberOfImagePages = maxPage;
    pMode->info.LinNumberOfPages   = maxPage;
}

/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int)   vgaR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{

    static bool s_fExpandDone = false;
    int         rc;
    unsigned    i;
#ifdef VBE_NEW_DYN_LIST
    uint32_t    cCustomModes;
    uint32_t    cyReduction;
    uint32_t    cbPitch;
    PVBEHEADER  pVBEDataHdr;
    ModeInfoListItem *pCurMode;
    unsigned    cb;
#endif
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PVGASTATE   pThis = PDMINS_2_DATA(pDevIns, PVGASTATE);
    PVM         pVM   = PDMDevHlpGetVM(pDevIns);

    Assert(iInstance == 0);
    Assert(pVM);

    /*
     * Init static data.
     */
    if (!s_fExpandDone)
    {
        s_fExpandDone = true;
        vga_init_expand();
    }

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "VRamSize\0"
                                          "MonitorCount\0"
                                          "GCEnabled\0"
                                          "R0Enabled\0"
                                          "FadeIn\0"
                                          "FadeOut\0"
                                          "LogoTime\0"
                                          "LogoFile\0"
                                          "ShowBootMenu\0"
                                          "BiosRom\0"
                                          "RealRetrace\0"
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
                                          "CustomVideoMode16\0"
                                          "MaxBiosXRes\0"
                                          "MaxBiosYRes\0"))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("Invalid configuration for vga device"));

    /*
     * Init state data.
     */
    rc = CFGMR3QueryU32Def(pCfg, "VRamSize", &pThis->vram_size, VGA_VRAM_DEFAULT);
    AssertLogRelRCReturn(rc, rc);
    if (pThis->vram_size > VGA_VRAM_MAX)
        return PDMDevHlpVMSetError(pDevIns, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                   "VRamSize is too large, %#x, max %#x", pThis->vram_size, VGA_VRAM_MAX);
    if (pThis->vram_size < VGA_VRAM_MIN)
        return PDMDevHlpVMSetError(pDevIns, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                   "VRamSize is too small, %#x, max %#x", pThis->vram_size, VGA_VRAM_MIN);
    if (pThis->vram_size & (_256K - 1)) /* Make sure there are no partial banks even in planar modes. */
        return PDMDevHlpVMSetError(pDevIns, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                   "VRamSize is not a multiple of 256K (%#x)", pThis->vram_size);

    rc = CFGMR3QueryU32Def(pCfg, "MonitorCount", &pThis->cMonitors, 1);
    AssertLogRelRCReturn(rc, rc);

    rc = CFGMR3QueryBoolDef(pCfg, "GCEnabled", &pThis->fGCEnabled, true);
    AssertLogRelRCReturn(rc, rc);

    rc = CFGMR3QueryBoolDef(pCfg, "R0Enabled", &pThis->fR0Enabled, true);
    AssertLogRelRCReturn(rc, rc);
    Log(("VGA: VRamSize=%#x fGCenabled=%RTbool fR0Enabled=%RTbool\n", pThis->vram_size, pThis->fGCEnabled, pThis->fR0Enabled));

    pThis->pDevInsR3 = pDevIns;
    pThis->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

    vgaR3Reset(pDevIns);

    /* The PCI devices configuration. */
    PCIDevSetVendorId(  &pThis->Dev, 0x80ee);   /* PCI vendor, just a free bogus value */
    PCIDevSetDeviceId(  &pThis->Dev, 0xbeef);
    PCIDevSetClassSub(  &pThis->Dev,   0x00);   /* VGA controller */
    PCIDevSetClassBase( &pThis->Dev,   0x03);
    PCIDevSetHeaderType(&pThis->Dev,   0x00);
#if defined(VBOX_WITH_HGSMI) && (defined(VBOX_WITH_VIDEOHWACCEL) || defined(VBOX_WITH_VDMA) || defined(VBOX_WITH_WDDM))
    PCIDevSetInterruptPin(&pThis->Dev, 1);
#endif

    /* The LBF access handler - error handling is better here than in the map function.  */
    rc = PDMR3LdrGetSymbolRCLazy(pVM, pDevIns->pReg->szRCMod, NULL, "vgaGCLFBAccessHandler", &pThis->RCPtrLFBHandler);
    if (RT_FAILURE(rc))
    {
        AssertReleaseMsgFailed(("PDMR3LdrGetSymbolRC(, %s, \"vgaGCLFBAccessHandler\",) -> %Rrc\n", pDevIns->pReg->szRCMod, rc));
        return rc;
    }

    /* the interfaces. */
    pThis->IBase.pfnQueryInterface      = vgaPortQueryInterface;

    pThis->IPort.pfnUpdateDisplay       = vgaPortUpdateDisplay;
    pThis->IPort.pfnUpdateDisplayAll    = vgaPortUpdateDisplayAll;
    pThis->IPort.pfnQueryColorDepth     = vgaPortQueryColorDepth;
    pThis->IPort.pfnSetRefreshRate      = vgaPortSetRefreshRate;
    pThis->IPort.pfnTakeScreenshot      = vgaPortTakeScreenshot;
    pThis->IPort.pfnFreeScreenshot      = vgaPortFreeScreenshot;
    pThis->IPort.pfnDisplayBlt          = vgaPortDisplayBlt;
    pThis->IPort.pfnUpdateDisplayRect   = vgaPortUpdateDisplayRect;
    pThis->IPort.pfnCopyRect            = vgaPortCopyRect;
    pThis->IPort.pfnSetRenderVRAM       = vgaPortSetRenderVRAM;

#if defined(VBOX_WITH_HGSMI)
# if defined(VBOX_WITH_VIDEOHWACCEL)
    pThis->IVBVACallbacks.pfnVHWACommandCompleteAsynch = vbvaVHWACommandCompleteAsynch;
# endif
#if defined(VBOX_WITH_CRHGSMI)
    pThis->IVBVACallbacks.pfnCrHgsmiCommandCompleteAsync = vboxVDMACrHgsmiCommandCompleteAsync;
    pThis->IVBVACallbacks.pfnCrHgsmiControlCompleteAsync = vboxVDMACrHgsmiControlCompleteAsync;
# endif
#endif

    /*
     * We use our own critical section to avoid unncessary pointer indirections
     * in interface methods (as we all as for historical reasons).
     */
    rc = PDMDevHlpCritSectInit(pDevIns, &pThis->CritSect, RT_SRC_POS, "VGA#%u", iInstance);
    AssertRCReturn(rc, rc);
    rc = PDMDevHlpSetDeviceCritSect(pDevIns, &pThis->CritSect);
    AssertRCReturn(rc, rc);

    /*
     * Allocate the VRAM and map the first 512KB of it into GC so we can speed up VGA support.
     */
    rc = PDMDevHlpMMIO2Register(pDevIns, 0 /* iRegion */, pThis->vram_size, 0, (void **)&pThis->vram_ptrR3, "VRam");
    AssertLogRelMsgRCReturn(rc, ("PDMDevHlpMMIO2Register(%#x,) -> %Rrc\n", pThis->vram_size, rc), rc);
    pThis->vram_ptrR0 = (RTR0PTR)pThis->vram_ptrR3; /** @todo @bugref{1865} Map parts into R0 or just use PGM access (Mac only). */

    if (pThis->fGCEnabled)
    {
        RTRCPTR pRCMapping = 0;
        rc = PDMDevHlpMMHyperMapMMIO2(pDevIns, 0 /* iRegion */, 0 /* off */,  VGA_MAPPING_SIZE, "VGA VRam", &pRCMapping);
        AssertLogRelMsgRCReturn(rc, ("PDMDevHlpMMHyperMapMMIO2(%#x,) -> %Rrc\n", VGA_MAPPING_SIZE, rc), rc);
        pThis->vram_ptrRC = pRCMapping;
    }

#if defined(VBOX_WITH_2X_4GB_ADDR_SPACE)
    if (pThis->fR0Enabled)
    {
        RTR0PTR pR0Mapping = 0;
        rc = PDMDevHlpMMIO2MapKernel(pDevIns, 0 /* iRegion */, 0 /* off */,  VGA_MAPPING_SIZE, "VGA VRam", &pR0Mapping);
        AssertLogRelMsgRCReturn(rc, ("PDMDevHlpMapMMIO2IntoR0(%#x,) -> %Rrc\n", VGA_MAPPING_SIZE, rc), rc);
        pThis->vram_ptrR0 = pR0Mapping;
    }
#endif

    /*
     * Register I/O ports, ROM and save state.
     */
    rc = PDMDevHlpIOPortRegister(pDevIns,  0x3c0, 16, NULL, vgaIOPortWrite,       vgaIOPortRead, NULL, NULL,      "VGA - 3c0");
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns,  0x3b4,  2, NULL, vgaIOPortWrite,       vgaIOPortRead, NULL, NULL,      "VGA - 3b4");
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns,  0x3ba,  1, NULL, vgaIOPortWrite,       vgaIOPortRead, NULL, NULL,      "VGA - 3ba");
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns,  0x3d4,  2, NULL, vgaIOPortWrite,       vgaIOPortRead, NULL, NULL,      "VGA - 3d4");
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns,  0x3da,  1, NULL, vgaIOPortWrite,       vgaIOPortRead, NULL, NULL,      "VGA - 3da");
    if (RT_FAILURE(rc))
        return rc;
#ifdef VBOX_WITH_HGSMI
    /* Use reserved VGA IO ports for HGSMI. */
    rc = PDMDevHlpIOPortRegister(pDevIns,  VGA_PORT_HGSMI_HOST,  4, NULL, vgaR3IOPortHGSMIWrite, vgaR3IOPortHGSMIRead, NULL, NULL, "VGA - 3b0 (HGSMI host)");
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns,  VGA_PORT_HGSMI_GUEST,  4, NULL, vgaR3IOPortHGSMIWrite, vgaR3IOPortHGSMIRead, NULL, NULL, "VGA - 3d0 (HGSMI guest)");
    if (RT_FAILURE(rc))
        return rc;
#endif /* VBOX_WITH_HGSMI */

#ifdef CONFIG_BOCHS_VBE
    rc = PDMDevHlpIOPortRegister(pDevIns,  0x1ce,  1, NULL, vgaIOPortWriteVBEIndex, vgaIOPortReadVBEIndex, NULL, NULL, "VGA/VBE - Index");
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns,  0x1cf,  1, NULL, vgaIOPortWriteVBEData, vgaIOPortReadVBEData, NULL, NULL, "VGA/VBE - Data");
    if (RT_FAILURE(rc))
        return rc;
#endif /* CONFIG_BOCHS_VBE */

    /* guest context extension */
    if (pThis->fGCEnabled)
    {
        rc = PDMDevHlpIOPortRegisterRC(pDevIns,  0x3c0, 16, 0, "vgaIOPortWrite",       "vgaIOPortRead", NULL, NULL,     "VGA - 3c0 (GC)");
        if (RT_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterRC(pDevIns,  0x3b4,  2, 0, "vgaIOPortWrite",       "vgaIOPortRead", NULL, NULL,     "VGA - 3b4 (GC)");
        if (RT_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterRC(pDevIns,  0x3ba,  1, 0, "vgaIOPortWrite",       "vgaIOPortRead", NULL, NULL,     "VGA - 3ba (GC)");
        if (RT_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterRC(pDevIns,  0x3d4,  2, 0, "vgaIOPortWrite",       "vgaIOPortRead", NULL, NULL,     "VGA - 3d4 (GC)");
        if (RT_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterRC(pDevIns,  0x3da,  1, 0, "vgaIOPortWrite",       "vgaIOPortRead", NULL, NULL,     "VGA - 3da (GC)");
        if (RT_FAILURE(rc))
            return rc;
#ifdef CONFIG_BOCHS_VBE
        rc = PDMDevHlpIOPortRegisterRC(pDevIns,  0x1ce,  1, 0, "vgaIOPortWriteVBEIndex", "vgaIOPortReadVBEIndex", NULL, NULL, "VGA/VBE - Index (GC)");
        if (RT_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterRC(pDevIns,  0x1cf,  1, 0, "vgaIOPortWriteVBEData", "vgaIOPortReadVBEData", NULL, NULL, "VGA/VBE - Data (GC)");
        if (RT_FAILURE(rc))
            return rc;
#endif /* CONFIG_BOCHS_VBE */
    }

    /* R0 context extension */
    if (pThis->fR0Enabled)
    {
        rc = PDMDevHlpIOPortRegisterR0(pDevIns,  0x3c0, 16, 0, "vgaIOPortWrite",       "vgaIOPortRead", NULL, NULL,     "VGA - 3c0 (GC)");
        if (RT_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterR0(pDevIns,  0x3b4,  2, 0, "vgaIOPortWrite",       "vgaIOPortRead", NULL, NULL,     "VGA - 3b4 (GC)");
        if (RT_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterR0(pDevIns,  0x3ba,  1, 0, "vgaIOPortWrite",       "vgaIOPortRead", NULL, NULL,     "VGA - 3ba (GC)");
        if (RT_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterR0(pDevIns,  0x3d4,  2, 0, "vgaIOPortWrite",       "vgaIOPortRead", NULL, NULL,     "VGA - 3d4 (GC)");
        if (RT_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterR0(pDevIns,  0x3da,  1, 0, "vgaIOPortWrite",       "vgaIOPortRead", NULL, NULL,     "VGA - 3da (GC)");
        if (RT_FAILURE(rc))
            return rc;
#ifdef CONFIG_BOCHS_VBE
        rc = PDMDevHlpIOPortRegisterR0(pDevIns,  0x1ce,  1, 0, "vgaIOPortWriteVBEIndex", "vgaIOPortReadVBEIndex", NULL, NULL, "VGA/VBE - Index (GC)");
        if (RT_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterR0(pDevIns,  0x1cf,  1, 0, "vgaIOPortWriteVBEData", "vgaIOPortReadVBEData", NULL, NULL, "VGA/VBE - Data (GC)");
        if (RT_FAILURE(rc))
            return rc;
#endif /* CONFIG_BOCHS_VBE */
    }

    /* vga mmio */
    rc = PDMDevHlpMMIORegisterEx(pDevIns, 0x000a0000, 0x00020000, NULL /*pvUser*/,
                                 IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU,
                                 vgaMMIOWrite, vgaMMIORead, vgaMMIOFill, "VGA - VGA Video Buffer");
    if (RT_FAILURE(rc))
        return rc;
    if (pThis->fGCEnabled)
    {
        rc = PDMDevHlpMMIORegisterRCEx(pDevIns, 0x000a0000, 0x00020000, NIL_RTRCPTR /*pvUser*/,
                                       "vgaMMIOWrite", "vgaMMIORead", "vgaMMIOFill");
        if (RT_FAILURE(rc))
            return rc;
    }
    if (pThis->fR0Enabled)
    {
        rc = PDMDevHlpMMIORegisterR0Ex(pDevIns, 0x000a0000, 0x00020000, NIL_RTR0PTR /*pvUser*/,
                                       "vgaMMIOWrite", "vgaMMIORead", "vgaMMIOFill");
        if (RT_FAILURE(rc))
            return rc;
    }

    /* vga bios */
    rc = PDMDevHlpIOPortRegister(pDevIns, VBE_PRINTF_PORT, 1, NULL, vgaIOPortWriteBIOS, vgaIOPortReadBIOS, NULL, NULL, "VGA BIOS debug/panic");
    if (RT_FAILURE(rc))
        return rc;
    if (pThis->fR0Enabled)
    {
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, VBE_PRINTF_PORT,  1, 0, "vgaIOPortWriteBIOS", "vgaIOPortReadBIOS", NULL, NULL, "VGA BIOS debug/panic");
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Get the VGA BIOS ROM file name.
     */
    rc = CFGMR3QueryStringAlloc(pCfg, "BiosRom", &pThis->pszVgaBiosFile);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
    {
        pThis->pszVgaBiosFile = NULL;
        rc = VINF_SUCCESS;
    }
    else if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"BiosRom\" as a string failed"));
    else if (!*pThis->pszVgaBiosFile)
    {
        MMR3HeapFree(pThis->pszVgaBiosFile);
        pThis->pszVgaBiosFile = NULL;
    }

    const uint8_t *pu8VgaBiosBinary = NULL;
    uint64_t cbVgaBiosBinary;
    /*
     * Determine the VGA BIOS ROM size, open specified ROM file in the process.
     */
    RTFILE FileVgaBios = NIL_RTFILE;
    if (pThis->pszVgaBiosFile)
    {
        rc = RTFileOpen(&FileVgaBios, pThis->pszVgaBiosFile,
                        RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
        if (RT_SUCCESS(rc))
        {
            rc = RTFileGetSize(FileVgaBios, &pThis->cbVgaBios);
            if (RT_SUCCESS(rc))
            {
                if (    RT_ALIGN(pThis->cbVgaBios, _4K) != pThis->cbVgaBios
                    ||  pThis->cbVgaBios > _64K
                    ||  pThis->cbVgaBios < 16 * _1K)
                    rc = VERR_TOO_MUCH_DATA;
            }
        }
        if (RT_FAILURE(rc))
        {
            /*
             * In case of failure simply fall back to the built-in VGA BIOS ROM.
             */
            Log(("vgaConstruct: Failed to open VGA BIOS ROM file '%s', rc=%Rrc!\n", pThis->pszVgaBiosFile, rc));
            RTFileClose(FileVgaBios);
            FileVgaBios = NIL_RTFILE;
            MMR3HeapFree(pThis->pszVgaBiosFile);
            pThis->pszVgaBiosFile = NULL;
        }
    }

    /*
     * Attempt to get the VGA BIOS ROM data from file.
     */
    if (pThis->pszVgaBiosFile)
    {
        /*
         * Allocate buffer for the VGA BIOS ROM data.
         */
        pThis->pu8VgaBios = (uint8_t *)PDMDevHlpMMHeapAlloc(pDevIns, pThis->cbVgaBios);
        if (pThis->pu8VgaBios)
        {
            rc = RTFileRead(FileVgaBios, pThis->pu8VgaBios, pThis->cbVgaBios, NULL);
            if (RT_FAILURE(rc))
            {
                AssertMsgFailed(("RTFileRead(,,%d,NULL) -> %Rrc\n", pThis->cbVgaBios, rc));
                MMR3HeapFree(pThis->pu8VgaBios);
                pThis->pu8VgaBios = NULL;
            }
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        pThis->pu8VgaBios = NULL;

    /* cleanup */
    if (FileVgaBios != NIL_RTFILE)
        RTFileClose(FileVgaBios);

    /* If we were unable to get the data from file for whatever reason, fall
       back to the built-in ROM image. */
    uint32_t fFlags = 0;
    if (pThis->pu8VgaBios == NULL)
    {
        pu8VgaBiosBinary = g_abVgaBiosBinary;
        cbVgaBiosBinary  = g_cbVgaBiosBinary;
        fFlags           = PGMPHYS_ROM_FLAGS_PERMANENT_BINARY;
    }
    else
    {
        pu8VgaBiosBinary = pThis->pu8VgaBios;
        cbVgaBiosBinary  = pThis->cbVgaBios;
    }

    AssertReleaseMsg(g_cbVgaBiosBinary <= _64K && g_cbVgaBiosBinary >= 32*_1K, ("g_cbVgaBiosBinary=%#x\n", g_cbVgaBiosBinary));
    AssertReleaseMsg(RT_ALIGN_Z(g_cbVgaBiosBinary, PAGE_SIZE) == g_cbVgaBiosBinary, ("g_cbVgaBiosBinary=%#x\n", g_cbVgaBiosBinary));
    /* Note! Because of old saved states we'll always register at least 36KB of ROM. */
    rc = PDMDevHlpROMRegister(pDevIns, 0x000c0000, RT_MAX(cbVgaBiosBinary, 36*_1K), pu8VgaBiosBinary, cbVgaBiosBinary,
                              fFlags, "VGA BIOS");
    if (RT_FAILURE(rc))
        return rc;

    /* save */
    rc = PDMDevHlpSSMRegisterEx(pDevIns, VGA_SAVEDSTATE_VERSION, sizeof(*pThis), NULL,
                                NULL,          vgaR3LiveExec, NULL,
                                vgaR3SavePrep, vgaR3SaveExec, vgaR3SaveDone,
                                NULL,          vgaR3LoadExec, vgaR3LoadDone);
    if (RT_FAILURE(rc))
        return rc;

    /* PCI */
    rc = PDMDevHlpPCIRegister(pDevIns, &pThis->Dev);
    if (RT_FAILURE(rc))
        return rc;
    /*AssertMsg(pThis->Dev.devfn == 16 || iInstance != 0, ("pThis->Dev.devfn=%d\n", pThis->Dev.devfn));*/
    if (pThis->Dev.devfn != 16 && iInstance == 0)
        Log(("!!WARNING!!: pThis->dev.devfn=%d (ignore if testcase or not started by Main)\n", pThis->Dev.devfn));

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 0 /* iRegion */, pThis->vram_size, PCI_ADDRESS_SPACE_MEM_PREFETCH, vgaR3IORegionMap);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Create the refresh timer.
     */
    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_REAL, vgaTimerRefresh,
                                pThis, TMTIMER_FLAGS_NO_CRIT_SECT,
                                "VGA Refresh Timer", &pThis->RefreshTimer);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Attach to the display.
     */
    rc = vgaAttach(pDevIns, 0 /* display LUN # */, PDM_TACH_FLAGS_NOT_HOT_PLUG);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Initialize the retrace flag.
     */
    rc = CFGMR3QueryBoolDef(pCfg, "RealRetrace", &pThis->fRealRetrace, false);
    AssertLogRelRCReturn(rc, rc);

#ifdef VBE_NEW_DYN_LIST

    uint16_t maxBiosXRes;
    rc = CFGMR3QueryU16Def(pCfg, "MaxBiosXRes", &maxBiosXRes, UINT16_MAX);
    AssertLogRelRCReturn(rc, rc);
    uint16_t maxBiosYRes;
    rc = CFGMR3QueryU16Def(pCfg, "MaxBiosYRes", &maxBiosYRes, UINT16_MAX);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Compute buffer size for the VBE BIOS Extra Data.
     */
    cb = sizeof(mode_info_list) + sizeof(ModeInfoListItem);

    rc = CFGMR3QueryU32(pCfg, "HeightReduction", &cyReduction);
    if (RT_SUCCESS(rc) && cyReduction)
        cb *= 2;                            /* Default mode list will be twice long */
    else
        cyReduction = 0;

    rc = CFGMR3QueryU32(pCfg, "CustomVideoModes", &cCustomModes);
    if (RT_SUCCESS(rc) && cCustomModes)
        cb += sizeof(ModeInfoListItem) * cCustomModes;
    else
        cCustomModes = 0;

    /*
     * Allocate and initialize buffer for the VBE BIOS Extra Data.
     */
    AssertRelease(sizeof(VBEHEADER) + cb < 65536);
    pThis->cbVBEExtraData = (uint16_t)(sizeof(VBEHEADER) + cb);
    pThis->pu8VBEExtraData = (uint8_t *)PDMDevHlpMMHeapAllocZ(pDevIns, pThis->cbVBEExtraData);
    if (!pThis->pu8VBEExtraData)
        return VERR_NO_MEMORY;

    pVBEDataHdr = (PVBEHEADER)pThis->pu8VBEExtraData;
    pVBEDataHdr->u16Signature = VBEHEADER_MAGIC;
    pVBEDataHdr->cbData = cb;

# ifndef VRAM_SIZE_FIX
    pCurMode = memcpy(pVBEDataHdr + 1, &mode_info_list, sizeof(mode_info_list));
    pCurMode = (ModeInfoListItem *)((uintptr_t)pCurMode + sizeof(mode_info_list));
# else  /* VRAM_SIZE_FIX defined */
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
        if (reqSize >= pThis->vram_size)
            continue;
        if (   mode_info_list[i].info.XResolution > maxBiosXRes
            || mode_info_list[i].info.YResolution > maxBiosYRes)
            continue;
        *pCurMode = mode_info_list[i];
        vgaAdjustModeInfo(pThis, pCurMode);
        pCurMode++;
    }
# endif  /* VRAM_SIZE_FIX defined */

    /*
     * Copy default modes with subtracted YResolution.
     */
    if (cyReduction)
    {
        ModeInfoListItem *pDefMode = mode_info_list;
        Log(("vgaR3Construct: cyReduction=%u\n", cyReduction));
# ifndef VRAM_SIZE_FIX
        for (i = 0; i < MODE_INFO_SIZE; i++, pCurMode++, pDefMode++)
        {
            *pCurMode = *pDefMode;
            pCurMode->mode += 0x30;
            pCurMode->info.YResolution -= cyReduction;
        }
# else  /* VRAM_SIZE_FIX defined */
        for (i = 0; i < MODE_INFO_SIZE; i++, pDefMode++)
        {
            uint32_t pixelWidth, reqSize;
            if (pDefMode->info.MemoryModel == VBE_MEMORYMODEL_TEXT_MODE)
                pixelWidth = 2;
            else
                pixelWidth = (pDefMode->info.BitsPerPixel + 7) / 8;
            reqSize = pDefMode->info.XResolution * pDefMode->info.YResolution *  pixelWidth;
            if (reqSize >= pThis->vram_size)
                continue;
            if (   pDefMode->info.XResolution > maxBiosXRes
                || pDefMode->info.YResolution - cyReduction > maxBiosYRes)
                continue;
            *pCurMode = *pDefMode;
            pCurMode->mode += 0x30;
            pCurMode->info.YResolution -= cyReduction;
            pCurMode++;
        }
# endif  /* VRAM_SIZE_FIX defined */
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
            rc = CFGMR3QueryStringAlloc(pCfg, szExtraDataKey, &pszExtraData);
            if (RT_SUCCESS(rc))
            {
                ModeInfoListItem *pDefMode = mode_info_list;
                unsigned int cx, cy, cBits, cParams, j;
                uint16_t u16DefMode;

                cParams = sscanf(pszExtraData, "%ux%ux%u", &cx, &cy, &cBits);
                if (    cParams != 3
                    ||  (cBits != 16 && cBits != 24 && cBits != 32))
                {
                    AssertMsgFailed(("Configuration error: Invalid mode data '%s' for '%s'! cBits=%d\n", pszExtraData, szExtraDataKey, cBits));
                    return VERR_VGA_INVALID_CUSTOM_MODE;
                }
                cbPitch = calc_line_pitch(cBits, cx);
# ifdef VRAM_SIZE_FIX
                if (cy * cbPitch >= pThis->vram_size)
                {
                    AssertMsgFailed(("Configuration error: custom video mode %dx%dx%dbits is too large for the virtual video memory of %dMb.  Please increase the video memory size.\n",
                                     cx, cy, cBits, pThis->vram_size / _1M));
                    return VERR_VGA_INVALID_CUSTOM_MODE;
                }
# endif  /* VRAM_SIZE_FIX defined */
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

                /* mode_info_list is not terminated */
                for (j = 0; j < MODE_INFO_SIZE && pDefMode->mode != u16DefMode; j++)
                    pDefMode++;
                Assert(j < MODE_INFO_SIZE);

                *pCurMode  = *pDefMode;
                pCurMode->mode = u16CurMode++;

                /* adjust defaults */
                pCurMode->info.XResolution = cx;
                pCurMode->info.YResolution = cy;
                pCurMode->info.BytesPerScanLine    = cbPitch;
                pCurMode->info.LinBytesPerScanLine = cbPitch;
                vgaAdjustModeInfo(pThis, pCurMode);

                /* commit it */
                pCurMode++;
            }
            else if (rc != VERR_CFGM_VALUE_NOT_FOUND)
            {
                AssertMsgFailed(("CFGMR3QueryStringAlloc(,'%s',) -> %Rrc\n", szExtraDataKey, rc));
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
    if (RT_FAILURE(rc))
        return rc;
#endif /* VBE_NEW_DYN_LIST */

    /*
     * Register I/O Port for the BIOS Logo.
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, LOGO_IO_PORT, 1, NULL, vbeIOPortWriteCMDLogo, vbeIOPortReadCMDLogo, NULL, NULL, "BIOS Logo");
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Register debugger info callbacks.
     */
    PDMDevHlpDBGFInfoRegister(pDevIns, "vga", "Display basic VGA state.", vgaInfoState);
    PDMDevHlpDBGFInfoRegister(pDevIns, "vgatext", "Display VGA memory formatted as text.", vgaInfoText);
    PDMDevHlpDBGFInfoRegister(pDevIns, "vgacr", "Dump VGA CRTC registers.", vgaInfoCR);
    PDMDevHlpDBGFInfoRegister(pDevIns, "vgagr", "Dump VGA Graphics Controller registers.", vgaInfoGR);
    PDMDevHlpDBGFInfoRegister(pDevIns, "vgasr", "Dump VGA Sequencer registers.", vgaInfoSR);
    PDMDevHlpDBGFInfoRegister(pDevIns, "vgaar", "Dump VGA Attribute Controller registers.", vgaInfoAR);
    PDMDevHlpDBGFInfoRegister(pDevIns, "vgapl", "Dump planar graphics state.", vgaInfoPlanar);
    PDMDevHlpDBGFInfoRegister(pDevIns, "vgadac", "Dump VGA DAC registers.", vgaInfoDAC);
    PDMDevHlpDBGFInfoRegister(pDevIns, "vbe", "Dump VGA VBE registers.", vgaInfoVBE);

    /*
     * Construct the logo header.
     */
    LOGOHDR LogoHdr = { LOGO_HDR_MAGIC, 0, 0, 0, 0, 0, 0 };

    rc = CFGMR3QueryU8(pCfg, "FadeIn", &LogoHdr.fu8FadeIn);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        LogoHdr.fu8FadeIn = 1;
    else if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"FadeIn\" as integer failed"));

    rc = CFGMR3QueryU8(pCfg, "FadeOut", &LogoHdr.fu8FadeOut);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        LogoHdr.fu8FadeOut = 1;
    else if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"FadeOut\" as integer failed"));

    rc = CFGMR3QueryU16(pCfg, "LogoTime", &LogoHdr.u16LogoMillies);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        LogoHdr.u16LogoMillies = 0;
    else if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"LogoTime\" as integer failed"));

    rc = CFGMR3QueryU8(pCfg, "ShowBootMenu", &LogoHdr.fu8ShowBootMenu);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        LogoHdr.fu8ShowBootMenu = 0;
    else if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"ShowBootMenu\" as integer failed"));

#if defined(DEBUG) && !defined(DEBUG_sunlover)
    /* Disable the logo abd menu if all default settings. */
    if (   LogoHdr.fu8FadeIn
        && LogoHdr.fu8FadeOut
        && LogoHdr.u16LogoMillies == 0
        && LogoHdr.fu8ShowBootMenu == 2)
        LogoHdr.fu8FadeIn = LogoHdr.fu8FadeOut = LogoHdr.fu8ShowBootMenu = 0;
#endif

    /* Delay the logo a little bit */
    if (LogoHdr.fu8FadeIn && LogoHdr.fu8FadeOut && !LogoHdr.u16LogoMillies)
        LogoHdr.u16LogoMillies = RT_MAX(LogoHdr.u16LogoMillies, LOGO_DELAY_TIME);

    /*
     * Get the Logo file name.
     */
    rc = CFGMR3QueryStringAlloc(pCfg, "LogoFile", &pThis->pszLogoFile);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pThis->pszLogoFile = NULL;
    else if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"LogoFile\" as a string failed"));
    else if (!*pThis->pszLogoFile)
    {
        MMR3HeapFree(pThis->pszLogoFile);
        pThis->pszLogoFile = NULL;
    }

    /*
     * Determine the logo size, open any specified logo file in the process.
     */
    LogoHdr.cbLogo = g_cbVgaDefBiosLogo;
    RTFILE FileLogo = NIL_RTFILE;
    if (pThis->pszLogoFile)
    {
        rc = RTFileOpen(&FileLogo, pThis->pszLogoFile,
                        RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
        if (RT_SUCCESS(rc))
        {
            uint64_t cbFile;
            rc = RTFileGetSize(FileLogo, &cbFile);
            if (RT_SUCCESS(rc))
            {
                if (cbFile > 0 && cbFile < 32*_1M)
                    LogoHdr.cbLogo = (uint32_t)cbFile;
                else
                    rc = VERR_TOO_MUCH_DATA;
            }
        }
        if (RT_FAILURE(rc))
        {
            /*
             * Ignore failure and fall back to the default logo.
             */
            LogRel(("vgaR3Construct: Failed to open logo file '%s', rc=%Rrc!\n", pThis->pszLogoFile, rc));
            if (FileLogo != NIL_RTFILE)
                RTFileClose(FileLogo);
            FileLogo = NIL_RTFILE;
            MMR3HeapFree(pThis->pszLogoFile);
            pThis->pszLogoFile = NULL;
        }
    }

    /*
     * Disable graphic splash screen if it doesn't fit into VRAM.
     */
    if (pThis->vram_size < LOGO_MAX_SIZE)
        LogoHdr.fu8FadeIn = LogoHdr.fu8FadeOut = LogoHdr.u16LogoMillies = 0;

    /*
     * Allocate buffer for the logo data.
     * RT_MAX() is applied to let us fall back to default logo on read failure.
     */
    pThis->cbLogo = sizeof(LogoHdr) + LogoHdr.cbLogo;
    pThis->pu8Logo = (uint8_t *)PDMDevHlpMMHeapAlloc(pDevIns, RT_MAX(pThis->cbLogo, g_cbVgaDefBiosLogo + sizeof(LogoHdr)));
    if (pThis->pu8Logo)
    {
        /*
         * Write the logo header.
         */
        PLOGOHDR pLogoHdr = (PLOGOHDR)pThis->pu8Logo;
        *pLogoHdr = LogoHdr;

        /*
         * Write the logo bitmap.
         */
        if (pThis->pszLogoFile)
        {
            rc = RTFileRead(FileLogo, pLogoHdr + 1, LogoHdr.cbLogo, NULL);
            if (RT_SUCCESS(rc))
                rc = vbeParseBitmap(pThis);
            if (RT_FAILURE(rc))
            {
                LogRel(("Error %Rrc reading logo file '%s', using internal logo\n",
                       rc, pThis->pszLogoFile));
                pLogoHdr->cbLogo = LogoHdr.cbLogo = g_cbVgaDefBiosLogo;
            }
        }
        if (   !pThis->pszLogoFile
            || RT_FAILURE(rc))
        {
            memcpy(pLogoHdr + 1, g_abVgaDefBiosLogo, LogoHdr.cbLogo);
            rc = vbeParseBitmap(pThis);
            if (RT_FAILURE(rc))
                AssertReleaseMsgFailed(("Parsing of internal bitmap failed! vbeParseBitmap() -> %Rrc\n", rc));
        }

        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_NO_MEMORY;

    /*
     * Cleanup.
     */
    if (FileLogo != NIL_RTFILE)
        RTFileClose(FileLogo);

#ifdef VBOX_WITH_HGSMI
    VBVAInit (pThis);
#endif /* VBOX_WITH_HGSMI */

#ifdef VBOX_WITH_VDMA
    if (rc == VINF_SUCCESS)
    {
        rc = vboxVDMAConstruct(pThis, 1024);
        AssertRC(rc);
    }
#endif
    /*
     * Statistics.
     */
    STAM_REG(pVM, &pThis->StatRZMemoryRead,     STAMTYPE_PROFILE, "/Devices/VGA/RZ/MMIO-Read",  STAMUNIT_TICKS_PER_CALL, "Profiling of the VGAGCMemoryRead() body.");
    STAM_REG(pVM, &pThis->StatR3MemoryRead,     STAMTYPE_PROFILE, "/Devices/VGA/R3/MMIO-Read",  STAMUNIT_TICKS_PER_CALL, "Profiling of the VGAGCMemoryRead() body.");
    STAM_REG(pVM, &pThis->StatRZMemoryWrite,    STAMTYPE_PROFILE, "/Devices/VGA/RZ/MMIO-Write", STAMUNIT_TICKS_PER_CALL, "Profiling of the VGAGCMemoryWrite() body.");
    STAM_REG(pVM, &pThis->StatR3MemoryWrite,    STAMTYPE_PROFILE, "/Devices/VGA/R3/MMIO-Write", STAMUNIT_TICKS_PER_CALL, "Profiling of the VGAGCMemoryWrite() body.");
    STAM_REG(pVM, &pThis->StatMapPage,          STAMTYPE_COUNTER, "/Devices/VGA/MapPageCalls",  STAMUNIT_OCCURENCES,     "Calls to IOMMMIOMapMMIO2Page.");
    STAM_REG(pVM, &pThis->StatUpdateDisp,       STAMTYPE_COUNTER, "/Devices/VGA/UpdateDisplay", STAMUNIT_OCCURENCES,     "Calls to vgaPortUpdateDisplay().");

    /* Init latched access mask. */
    pThis->uMaskLatchAccess = 0x3ff;
    return rc;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceVga =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "vga",
    /* szRCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "VGA Adaptor with VESA extensions.",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
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
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};

#endif /* !IN_RING3 */
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

/*
 * Local Variables:
 *   nuke-trailing-whitespace-p:nil
 * End:
 */
