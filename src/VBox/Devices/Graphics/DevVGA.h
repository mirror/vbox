/* $Id$ */
/** @file
 * DevVGA - VBox VGA/VESA device, internal header.
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
 * --------------------------------------------------------------------
 *
 * This code is based on:
 *
 * QEMU internal VGA defines.
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

#ifdef VBOX_WITH_HGSMI
#include "HGSMI/HGSMIHost.h"
#endif /* VBOX_WITH_HGSMI */

#define MSR_COLOR_EMULATION 0x01
#define MSR_PAGE_SELECT     0x20

#define ST01_V_RETRACE      0x08
#define ST01_DISP_ENABLE    0x01

/* bochs VBE support */
#define CONFIG_BOCHS_VBE

#ifdef VBOX
#define VBE_DISPI_MAX_XRES              16384
#define VBE_DISPI_MAX_YRES              16384
#else
#define VBE_DISPI_MAX_XRES              1600
#define VBE_DISPI_MAX_YRES              1200
#endif
#define VBE_DISPI_MAX_BPP               32

#define VBE_DISPI_INDEX_ID              0x0
#define VBE_DISPI_INDEX_XRES            0x1
#define VBE_DISPI_INDEX_YRES            0x2
#define VBE_DISPI_INDEX_BPP             0x3
#define VBE_DISPI_INDEX_ENABLE          0x4
#define VBE_DISPI_INDEX_BANK            0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH      0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT     0x7
#define VBE_DISPI_INDEX_X_OFFSET        0x8
#define VBE_DISPI_INDEX_Y_OFFSET        0x9
#define VBE_DISPI_INDEX_VBOX_VIDEO      0xa
#ifdef VBOX_WITH_HGSMI
/* @todo this will break saved state and is inefficient. use 2 PCI io ports. */
#define VBE_DISPI_INDEX_VBVA_HOST       0xb
#define VBE_DISPI_INDEX_VBVA_GUEST      0xc
#define VBE_DISPI_INDEX_NB              0xd
#else
#define VBE_DISPI_INDEX_NB              0xb
#endif /* !VBOX_WITH_HGSMI */

#define VBE_DISPI_ID0                   0xB0C0
#define VBE_DISPI_ID1                   0xB0C1
#define VBE_DISPI_ID2                   0xB0C2
#define VBE_DISPI_ID3                   0xB0C3
#define VBE_DISPI_ID4                   0xB0C4

#ifdef VBOX
/* The VBOX interface id. Indicates support for VBE_DISPI_INDEX_VBOX_VIDEO. */
#define VBE_DISPI_ID_VBOX_VIDEO         0xBE00
#ifdef VBOX_WITH_HGSMI
/* The VBOX interface id. Indicates support for VBVA shared memory interface,
 * VBE_DISPI_INDEX_VBVA_GUEST_CMD and VBE_DISPI_INDEX_VBVA_HOST_CMD VBE indexes.
 */
#define VBE_DISPI_ID_HGSMI              0xBE01
#endif /* VBOX_WITH_HGSMI */
#endif /* VBOX */

#define VBE_DISPI_DISABLED              0x00
#define VBE_DISPI_ENABLED               0x01
#define VBE_DISPI_GETCAPS               0x02
#define VBE_DISPI_8BIT_DAC              0x20
#define VBE_DISPI_LFB_ENABLED           0x40
#define VBE_DISPI_NOCLEARMEM            0x80

#define VBE_DISPI_LFB_PHYSICAL_ADDRESS  0xE0000000

#ifdef CONFIG_BOCHS_VBE

#define VGA_STATE_COMMON_BOCHS_VBE              \
    uint16_t vbe_index;                         \
    uint16_t vbe_regs[VBE_DISPI_INDEX_NB];      \
    uint32_t vbe_start_addr;                    \
    uint32_t vbe_line_offset;                   \
    uint32_t vbe_bank_max;

#else

#define VGA_STATE_COMMON_BOCHS_VBE

#endif /* !CONFIG_BOCHS_VBE */

#define CH_ATTR_SIZE (160 * 100)
#define VGA_MAX_HEIGHT VBE_DISPI_MAX_YRES

#ifndef VBOX
#define VGA_STATE_COMMON                                                \
    uint8_t *vram_ptr;                                                  \
    unsigned long vram_offset;                                          \
    unsigned int vram_size;                                             \
    uint32_t latch;                                                     \
    uint8_t sr_index;                                                   \
    uint8_t sr[256];                                                    \
    uint8_t gr_index;                                                   \
    uint8_t gr[256];                                                    \
    uint8_t ar_index;                                                   \
    uint8_t ar[21];                                                     \
    int ar_flip_flop;                                                   \
    uint8_t cr_index;                                                   \
    uint8_t cr[256]; /* CRT registers */                                \
    uint8_t msr; /* Misc Output Register */                             \
    uint8_t fcr; /* Feature Control Register */                         \
    uint8_t st00; /* status 0 */                                        \
    uint8_t st01; /* status 1 */                                        \
    uint8_t dac_state;                                                  \
    uint8_t dac_sub_index;                                              \
    uint8_t dac_read_index;                                             \
    uint8_t dac_write_index;                                            \
    uint8_t dac_cache[3]; /* used when writing */                       \
    uint8_t palette[768];                                               \
    int32_t bank_offset;                                                \
    int (*get_bpp)(struct VGAState *s);                                 \
    void (*get_offsets)(struct VGAState *s,                             \
                        uint32_t *pline_offset,                         \
                        uint32_t *pstart_addr,                          \
                        uint32_t *pline_compare);                       \
    void (*get_resolution)(struct VGAState *s,                          \
                        int *pwidth,                                    \
                        int *pheight);                                  \
    VGA_STATE_COMMON_BOCHS_VBE                                          \
    /* display refresh support */                                       \
    DisplayState *ds;                                                   \
    uint32_t font_offsets[2];                                           \
    int graphic_mode;                                                   \
    uint8_t shift_control;                                              \
    uint8_t double_scan;                                                \
    uint32_t line_offset;                                               \
    uint32_t line_compare;                                              \
    uint32_t start_addr;                                                \
    uint32_t plane_updated;                                             \
    uint8_t last_cw, last_ch;                                           \
    uint32_t last_width, last_height; /* in chars or pixels */          \
    uint32_t last_scr_width, last_scr_height; /* in pixels */           \
    uint8_t cursor_start, cursor_end;                                   \
    uint32_t cursor_offset;                                             \
    unsigned int (*rgb_to_pixel)(unsigned int r,                        \
                                 unsigned int g, unsigned b);           \
    /* hardware mouse cursor support */                                 \
    uint32_t invalidated_y_table[VGA_MAX_HEIGHT / 32];                  \
    void (*cursor_invalidate)(struct VGAState *s);                      \
    void (*cursor_draw_line)(struct VGAState *s, uint8_t *d, int y);    \
    /* tell for each page if it has been updated since the last time */ \
    uint32_t last_palette[256];                                         \
    uint32_t last_ch_attr[CH_ATTR_SIZE]; /* XXX: make it dynamic */

#else /* VBOX */

struct VGAState;
typedef int FNGETBPP(struct VGAState *s);
typedef void FNGETOFFSETS(struct VGAState *s, uint32_t *pline_offset, uint32_t *pstart_addr, uint32_t *pline_compare);
typedef void FNGETRESOLUTION(struct VGAState *s, int *pwidth, int *pheight);
typedef unsigned int FNRGBTOPIXEL(unsigned int r, unsigned int g, unsigned b);
typedef void FNCURSORINVALIDATE(struct VGAState *s);
typedef void FNCURSORDRAWLINE(struct VGAState *s, uint8_t *d, int y);

/* bird: vram_offset have been remove, function pointers declared external,
         some type changes, and some padding have been added. */
#define VGA_STATE_COMMON                                                \
    R3PTRTYPE(uint8_t *) vram_ptrR3;                                    \
    uint32_t vram_size;                                                 \
    uint32_t latch;                                                     \
    uint8_t sr_index;                                                   \
    uint8_t sr[256];                                                    \
    uint8_t gr_index;                                                   \
    uint8_t gr[256];                                                    \
    uint8_t ar_index;                                                   \
    uint8_t ar[21];                                                     \
    int32_t ar_flip_flop;                                               \
    uint8_t cr_index;                                                   \
    uint8_t cr[256]; /* CRT registers */                                \
    uint8_t msr; /* Misc Output Register */                             \
    uint8_t fcr; /* Feature Control Register */                         \
    uint8_t st00; /* status 0 */                                        \
    uint8_t st01; /* status 1 */                                        \
    uint8_t dac_state;                                                  \
    uint8_t dac_sub_index;                                              \
    uint8_t dac_read_index;                                             \
    uint8_t dac_write_index;                                            \
    uint8_t dac_cache[3]; /* used when writing */                       \
    uint8_t palette[768];                                               \
    int32_t bank_offset;                                                \
    int32_t padding0;                                                   \
    R3PTRTYPE(FNGETBPP *) get_bpp;                                      \
    R3PTRTYPE(FNGETOFFSETS *) get_offsets;                              \
    R3PTRTYPE(FNGETRESOLUTION *) get_resolution;                        \
    VGA_STATE_COMMON_BOCHS_VBE                                          \
    /* display refresh support */                                       \
    uint32_t font_offsets[2];                                           \
    int32_t graphic_mode;                                               \
    uint8_t shift_control;                                              \
    uint8_t double_scan;                                                \
    uint8_t padding1[2];                                                \
    uint32_t line_offset;                                               \
    uint32_t line_compare;                                              \
    uint32_t start_addr;                                                \
    uint32_t plane_updated;                                             \
    uint8_t last_cw, last_ch, padding2[2];                              \
    uint32_t last_width, last_height; /* in chars or pixels */          \
    uint32_t last_scr_width, last_scr_height; /* in pixels */           \
    uint32_t last_bpp;                                                  \
    uint8_t cursor_start, cursor_end, padding3[2];                      \
    uint32_t cursor_offset;                                             \
    uint32_t padding4;                                                  \
    R3PTRTYPE(FNRGBTOPIXEL *) rgb_to_pixel;                             \
    /* hardware mouse cursor support */                                 \
    uint32_t invalidated_y_table[VGA_MAX_HEIGHT / 32];                  \
    R3PTRTYPE(FNCURSORINVALIDATE *) cursor_invalidate;                  \
    R3PTRTYPE(FNCURSORDRAWLINE *) cursor_draw_line;                     \
    /* tell for each page if it has been updated since the last time */ \
    uint32_t last_palette[256];                                         \
    uint32_t last_ch_attr[CH_ATTR_SIZE]; /* XXX: make it dynamic */

#endif /* VBOX */

typedef struct VGAState {
    VGA_STATE_COMMON
#ifdef VBOX
    /** end-of-common-state-marker */
    uint32_t                    u32Marker;
    /** The physical address the VRAM was assigned. */
    RTGCPHYS32                  GCPhysVRAM;
    /** The R0 vram pointer... */
    R0PTRTYPE(uint8_t *)        vram_ptrR0;
    /** Pointer to the GC vram mapping. */
    RCPTRTYPE(uint8_t *)        vram_ptrRC;
    /** LFB was updated flag. */
    bool                        fLFBUpdated;
    /** Indicates if the GC extensions are enabled or not. */
    bool                        fGCEnabled;
    /** Indicates if the R0 extensions are enabled or not. */
    bool                        fR0Enabled;
    /** Flag indicating that there are dirty bits. This is used to optimize the handler resetting. */
    bool                        fHasDirtyBits;
    /** Flag indicating that the VGA memory in the 0xa0000-0xbffff region has been remapped to allow direct access. */
    bool                        fRemappedVGA;
    /** Whether to render the guest VRAM to the framebuffer memory. False only for some LFB modes. */
    bool                        fRenderVRAM;
    bool                        padding9[2];

#ifdef VBOX_WITH_HGSMI
    R3PTRTYPE(PHGSMIINSTANCE)   pHGSMI;
#endif /* VBOX_WITH_HGSMI */

    /** Current refresh timer interval. */
    uint32_t                    cMilliesRefreshInterval;
    /** Refresh timer handle - HC. */
    PTMTIMERR3                  RefreshTimer;

    /** Bitmap tracking dirty pages. */
    uint32_t                    au32DirtyBitmap[VGA_VRAM_MAX / PAGE_SIZE / 32];
    /** Pointer to vgaGCLFBAccessHandler(). */
    RTRCPTR                     RCPtrLFBHandler;
    /** Pointer to the device instance - RC Ptr. */
    PPDMDEVINSRC                pDevInsRC;
    /** Pointer to the device instance - R3 Ptr. */
    PPDMDEVINSR3                pDevInsR3;
    /** Pointer to the device instance - R0 Ptr. */
    PPDMDEVINSR0                pDevInsR0;

    /** The display port base interface. */
    PDMIBASE                    Base;
    /** The display port interface. */
    PDMIDISPLAYPORT             Port;
    /** Pointer to base interface of the driver. */
    R3PTRTYPE(PPDMIBASE)        pDrvBase;
    /** Pointer to display connector interface of the driver. */
    R3PTRTYPE(PPDMIDISPLAYCONNECTOR) pDrv;

    /** The PCI device. */
    PCIDEVICE                   Dev;

    STAMPROFILE                 StatRZMemoryRead;
    STAMPROFILE                 StatR3MemoryRead;
    STAMPROFILE                 StatRZMemoryWrite;
    STAMPROFILE                 StatR3MemoryWrite;

    /* Keep track of ring 0 latched accesses to the VGA MMIO memory. */
    uint64_t                    u64LastLatchedAccess;
    uint32_t                    cLatchAccesses;
    uint16_t                    uMaskLatchAccess;
    uint16_t                    iMask;

#ifdef VBE_BYTEWISE_IO
    /** VBE read/write data/index flags */
    uint8_t                     fReadVBEData;
    uint8_t                     fWriteVBEData;
    uint8_t                     fReadVBEIndex;
    uint8_t                     fWriteVBEIndex;
    /** VBE write data/index one byte buffer */
    uint8_t                     cbWriteVBEData;
    uint8_t                     cbWriteVBEIndex;
# ifdef VBE_NEW_DYN_LIST
    /** VBE Extra Data write address one byte buffer */
    uint8_t                     cbWriteVBEExtraAddress;
    uint8_t                     Padding6;       /**< Alignment padding. */
# else
    uint8_t                     Padding6[2];    /**< Alignment padding. */
# endif
#endif

#ifdef VBE_NEW_DYN_LIST
    /** The VBE BIOS extra data. */
    R3PTRTYPE(uint8_t *)        pu8VBEExtraData;
    /** The size of the VBE BIOS extra data. */
    uint16_t                    cbVBEExtraData;
    /** The VBE BIOS current memory address. */
    uint16_t                    u16VBEExtraAddress;
    uint16_t                    Padding7[2];    /**< Alignment padding. */
#endif
    /** Current logo data offset. */
    uint32_t                    offLogoData;
    /** The size of the BIOS logo data. */
    uint32_t                    cbLogo;
    /** The BIOS logo data. */
    R3PTRTYPE(uint8_t *)        pu8Logo;
    /** The name of the logo file. */
    R3PTRTYPE(char *)           pszLogoFile;
    /** Bitmap image data. */
    R3PTRTYPE(uint8_t *)        pu8LogoBitmap;
    /** Current logo command. */
    uint16_t                    LogoCommand;
    /** Bitmap width. */
    uint16_t                    cxLogo;
    /** Bitmap height. */
    uint16_t                    cyLogo;
    /** Bitmap planes. */
    uint16_t                    cLogoPlanes;
    /** Bitmap depth. */
    uint16_t                    cLogoBits;
    /** Bitmap compression. */
    uint16_t                    LogoCompression;
    /** Bitmap colors used. */
    uint16_t                    cLogoUsedColors;
    /** Palette size. */
    uint16_t                    cLogoPalEntries;
    /** Clear screen flag. */
    uint8_t                     fLogoClearScreen;
    uint8_t                     Padding8[7];    /**< Alignment padding. */
    /** Palette data. */
    uint32_t                    au32LogoPalette[256];
#endif /* VBOX */
} VGAState;
#ifdef VBOX
/** VGA state. */
typedef VGAState VGASTATE;
/** Pointer to the VGA state. */
typedef VGASTATE *PVGASTATE;
#endif

#ifdef VBE_NEW_DYN_LIST
/**
 * VBE Bios Extra Data structure.
 * @remark duplicated in vbe.h.
 */
typedef struct VBEHeader
{
    /** Signature (VBEHEADER_MAGIC). */
    uint16_t        u16Signature;
    /** Data size. */
    uint16_t        cbData;
} VBEHeader;

/** VBE Extra Data. */
typedef VBEHeader VBEHEADER;
/** Pointer to the VBE Extra Data. */
typedef VBEHEADER *PVBEHEADER;

/** The value of the VBEHEADER::u16Signature field.
 * @remark duplicated in vbe.h. */
#define VBEHEADER_MAGIC      0x77CC

/** The extra port which is used to read the mode list.
 * @remark duplicated in vbe.h. */
#define VBE_EXTRA_PORT       0x3b6

/** The extra port which is used for debug printf.
 * @remark duplicated in vbe.h. */
#define VBE_PRINTF_PORT      0x3b7

#endif /* VBE_NEW_DYN_LIST */

#if !defined(VBOX) || defined(IN_RING3)
static inline int c6_to_8(int v)
{
    int b;
    v &= 0x3f;
    b = v & 1;
    return (v << 2) | (b << 1) | b;
}
#endif /* !VBOX || IN_RING3 */


#ifdef VBOX_WITH_HGSMI
int      VBVAInit       (PVGASTATE pVGAState);
void     VBVADestroy    (PVGASTATE pVGAState);
int      VBVAUpdateDisplay (PVGASTATE pVGAState);
#endif /* VBOX_WITH_HGSMI */

#ifndef VBOX
void vga_common_init(VGAState *s, DisplayState *ds, uint8_t *vga_ram_base,
                     unsigned long vga_ram_offset, int vga_ram_size);
uint32_t vga_mem_readb(void *opaque, target_phys_addr_t addr);
void vga_mem_writeb(void *opaque, target_phys_addr_t addr, uint32_t val);
void vga_invalidate_scanlines(VGAState *s, int y1, int y2);

void vga_draw_cursor_line_8(uint8_t *d1, const uint8_t *src1,
                            int poffset, int w,
                            unsigned int color0, unsigned int color1,
                            unsigned int color_xor);
void vga_draw_cursor_line_16(uint8_t *d1, const uint8_t *src1,
                             int poffset, int w,
                             unsigned int color0, unsigned int color1,
                             unsigned int color_xor);
void vga_draw_cursor_line_32(uint8_t *d1, const uint8_t *src1,
                             int poffset, int w,
                             unsigned int color0, unsigned int color1,
                             unsigned int color_xor);

extern const uint8_t sr_mask[8];
extern const uint8_t gr_mask[16];
#endif /* !VBOX */

