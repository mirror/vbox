/** @file
 *
 * VBox VGA/VESA device:
 * List of static mode information, containing all "supported" VBE
 * modes and their 'settings'
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
 */

#ifdef VBE_NEW_DYN_LIST

/* VBE Mode Numbers */
#define VBE_MODE_VESA_DEFINED                            0x0100
#define VBE_MODE_REFRESH_RATE_USE_CRTC                   0x0800
#define VBE_MODE_LINEAR_FRAME_BUFFER                     0x4000
#define VBE_MODE_PRESERVE_DISPLAY_MEMORY                 0x8000

/* VBE GFX Mode Number */
#define VBE_VESA_MODE_640X400X8                          0x100
#define VBE_VESA_MODE_640X480X8                          0x101
#define VBE_VESA_MODE_800X600X4                          0x102
#define VBE_VESA_MODE_800X600X8                          0x103
#define VBE_VESA_MODE_1024X768X4                         0x104
#define VBE_VESA_MODE_1024X768X8                         0x105
#define VBE_VESA_MODE_1280X1024X4                        0x106
#define VBE_VESA_MODE_1280X1024X8                        0x107
#define VBE_VESA_MODE_320X200X1555                       0x10D
#define VBE_VESA_MODE_320X200X565                        0x10E
#define VBE_VESA_MODE_320X200X888                        0x10F
#define VBE_VESA_MODE_640X480X1555                       0x110
#define VBE_VESA_MODE_640X480X565                        0x111
#define VBE_VESA_MODE_640X480X888                        0x112
#define VBE_VESA_MODE_800X600X1555                       0x113
#define VBE_VESA_MODE_800X600X565                        0x114
#define VBE_VESA_MODE_800X600X888                        0x115
#define VBE_VESA_MODE_1024X768X1555                      0x116
#define VBE_VESA_MODE_1024X768X565                       0x117
#define VBE_VESA_MODE_1024X768X888                       0x118
#define VBE_VESA_MODE_1280X1024X1555                     0x119
#define VBE_VESA_MODE_1280X1024X565                      0x11A
#define VBE_VESA_MODE_1280X1024X888                      0x11B

/* BOCHS/PLEX86 'own' mode numbers */
#define VBE_OWN_MODE_320X200X8888                        0x120
#define VBE_OWN_MODE_640X400X8888                        0x121
#define VBE_OWN_MODE_640X480X8888                        0x122
#define VBE_OWN_MODE_800X600X8888                        0x123
#define VBE_OWN_MODE_1024X768X8888                       0x124
#define VBE_OWN_MODE_1280X1024X8888                      0x125
#define VBE_OWN_MODE_320X200X8                           0x126

#define VBE_VESA_MODE_END_OF_LIST                        0xFFFF

/* Capabilities */
#define VBE_CAPABILITY_8BIT_DAC                          0x0001
#define VBE_CAPABILITY_NOT_VGA_COMPATIBLE                0x0002
#define VBE_CAPABILITY_RAMDAC_USE_BLANK_BIT              0x0004
#define VBE_CAPABILITY_STEREOSCOPIC_SUPPORT              0x0008
#define VBE_CAPABILITY_STEREO_VIA_VESA_EVC               0x0010

/* Mode Attributes */
#define VBE_MODE_ATTRIBUTE_SUPPORTED                     0x0001
#define VBE_MODE_ATTRIBUTE_EXTENDED_INFORMATION_AVAILABLE  0x0002
#define VBE_MODE_ATTRIBUTE_TTY_BIOS_SUPPORT              0x0004
#define VBE_MODE_ATTRIBUTE_COLOR_MODE                    0x0008
#define VBE_MODE_ATTRIBUTE_GRAPHICS_MODE                 0x0010
#define VBE_MODE_ATTRIBUTE_NOT_VGA_COMPATIBLE            0x0020
#define VBE_MODE_ATTRIBUTE_NO_VGA_COMPATIBLE_WINDOW      0x0040
#define VBE_MODE_ATTRIBUTE_LINEAR_FRAME_BUFFER_MODE      0x0080
#define VBE_MODE_ATTRIBUTE_DOUBLE_SCAN_MODE              0x0100
#define VBE_MODE_ATTRIBUTE_INTERLACE_MODE                0x0200
#define VBE_MODE_ATTRIBUTE_HARDWARE_TRIPLE_BUFFER        0x0400
#define VBE_MODE_ATTRIBUTE_HARDWARE_STEREOSCOPIC_DISPLAY 0x0800
#define VBE_MODE_ATTRIBUTE_DUAL_DISPLAY_START_ADDRESS    0x1000

#define VBE_MODE_ATTTRIBUTE_LFB_ONLY                     ( VBE_MODE_ATTRIBUTE_NO_VGA_COMPATIBLE_WINDOW | VBE_MODE_ATTRIBUTE_LINEAR_FRAME_BUFFER_MODE )

/* Window attributes */
#define VBE_WINDOW_ATTRIBUTE_RELOCATABLE                 0x01
#define VBE_WINDOW_ATTRIBUTE_READABLE                    0x02
#define VBE_WINDOW_ATTRIBUTE_WRITEABLE                   0x04

/* Memory model */
#define VBE_MEMORYMODEL_TEXT_MODE                        0x00
#define VBE_MEMORYMODEL_CGA_GRAPHICS                     0x01
#define VBE_MEMORYMODEL_HERCULES_GRAPHICS                0x02
#define VBE_MEMORYMODEL_PLANAR                           0x03
#define VBE_MEMORYMODEL_PACKED_PIXEL                     0x04
#define VBE_MEMORYMODEL_NON_CHAIN_4_256                  0x05
#define VBE_MEMORYMODEL_DIRECT_COLOR                     0x06
#define VBE_MEMORYMODEL_YUV                              0x07

/* DirectColorModeInfo */
#define VBE_DIRECTCOLOR_COLOR_RAMP_PROGRAMMABLE          0x01
#define VBE_DIRECTCOLOR_RESERVED_BITS_AVAILABLE          0x02

/* Video memory */
#define VGAMEM_GRAPH                                     0xA000
#define VBE_DISPI_BANK_SIZE_KB                           64
#define VBE_DISPI_LFB_PHYSICAL_ADDRESS                   0xE0000000

/*
 * This one is for compactly storing a list of mode info blocks
 */
#pragma pack(1) /* pack(1) is important! (you'll get a byte extra for each of the u8 fields elsewise...) */
typedef struct ModeInfoBlockCompact
{
    /* Mandatory information for all VBE revisions */
    uint16_t     ModeAttributes;
    uint8_t      WinAAttributes;
    uint8_t      WinBAttributes;
    uint16_t     WinGranularity;
    uint16_t     WinSize;
    uint16_t     WinASegment;
    uint16_t     WinBSegment;
    uint32_t     WinFuncPtr;
    uint16_t     BytesPerScanLine;
    /* Mandatory information for VBE 1.2 and above */
    uint16_t     XResolution;
    uint16_t     YResolution;
    uint8_t      XCharSize;
    uint8_t      YCharSize;
    uint8_t      NumberOfPlanes;
    uint8_t      BitsPerPixel;
    uint8_t      NumberOfBanks;
    uint8_t      MemoryModel;
    uint8_t      BankSize;
    uint8_t      NumberOfImagePages;
    uint8_t      Reserved_page;
    /* Direct Color fields (required for direct/6 and YUV/7 memory models) */
    uint8_t      RedMaskSize;
    uint8_t      RedFieldPosition;
    uint8_t      GreenMaskSize;
    uint8_t      GreenFieldPosition;
    uint8_t      BlueMaskSize;
    uint8_t      BlueFieldPosition;
    uint8_t      RsvdMaskSize;
    uint8_t      RsvdFieldPosition;
    uint8_t      DirectColorModeInfo;
    /* Mandatory information for VBE 2.0 and above */
    uint32_t     PhysBasePtr;
    uint32_t     OffScreenMemOffset;
    uint16_t     OffScreenMemSize;
    /* Mandatory information for VBE 3.0 and above */
    uint16_t     LinBytesPerScanLine;
    uint8_t      BnkNumberOfPages;
    uint8_t      LinNumberOfPages;
    uint8_t      LinRedMaskSize;
    uint8_t      LinRedFieldPosition;
    uint8_t      LinGreenMaskSize;
    uint8_t      LinGreenFieldPosition;
    uint8_t      LinBlueMaskSize;
    uint8_t      LinBlueFieldPosition;
    uint8_t      LinRsvdMaskSize;
    uint8_t      LinRsvdFieldPosition;
    uint32_t     MaxPixelClock;
} ModeInfoBlockCompact;

#pragma pack()


typedef struct ModeInfoListItem
{
    uint16_t                mode;
    ModeInfoBlockCompact    info;
} ModeInfoListItem;


static ModeInfoListItem mode_info_list[]=
{
        {
                VBE_VESA_MODE_640X400X8,
                {
/*typedef struct ModeInfoBlock
{*/
/* Mandatory information for all VBE revisions */
   /*Bit16u ModeAttributes*/            VBE_MODE_ATTRIBUTE_SUPPORTED |
                                        VBE_MODE_ATTRIBUTE_EXTENDED_INFORMATION_AVAILABLE |
                                        VBE_MODE_ATTRIBUTE_COLOR_MODE |
                                        VBE_MODE_ATTRIBUTE_LINEAR_FRAME_BUFFER_MODE |
                                        VBE_MODE_ATTRIBUTE_GRAPHICS_MODE,
   /*Bit8u  WinAAttributes*/            VBE_WINDOW_ATTRIBUTE_RELOCATABLE |
                                        VBE_WINDOW_ATTRIBUTE_READABLE |
                                        VBE_WINDOW_ATTRIBUTE_WRITEABLE,
   /*Bit8u  WinBAttributes*/            0,
   /*Bit16u WinGranularity*/            VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinSize*/                   VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinASegment*/               VGAMEM_GRAPH,
   /*Bit16u WinBSegment*/               0,
   /*Bit32u WinFuncPtr*/                0,
   /*Bit16u BytesPerScanLine*/          640,
/* Mandatory information for VBE 1.2 and above */
   /*Bit16u XResolution*/               640,
   /*Bit16u YResolution*/               400,
   /*Bit8u  XCharSize*/                 8,
   /*Bit8u  YCharSize*/                 16,
   /*Bit8u  NumberOfPlanes*/            1,
   /*Bit8u  BitsPerPixel*/              8,
   /*Bit8u  NumberOfBanks*/             4, /* 640x400/64kb == 4 */
   /*Bit8u  MemoryModel*/               VBE_MEMORYMODEL_PACKED_PIXEL,
   /*Bit8u  BankSize*/                  0,
   /*Bit8u  NumberOfImagePages*/        15,
   /*Bit8u  Reserved_page*/             0,
/* Direct Color fields (required for direct/6 and YUV/7 memory models) */
   /*Bit8u  RedMaskSize*/               0,
   /*Bit8u  RedFieldPosition*/          0,
   /*Bit8u  GreenMaskSize*/             0,
   /*Bit8u  GreenFieldPosition*/        0,
   /*Bit8u  BlueMaskSize*/              0,
   /*Bit8u  BlueFieldPosition*/         0,
   /*Bit8u  RsvdMaskSize*/              0,
   /*Bit8u  RsvdFieldPosition*/         0,
   /*Bit8u  DirectColorModeInfo*/       0,
/* Mandatory information for VBE 2.0 and above */
   /*Bit32u PhysBasePtr*/               VBE_DISPI_LFB_PHYSICAL_ADDRESS,
   /*Bit32u OffScreenMemOffset*/        0,
   /*Bit16u OffScreenMemSize*/          0,
/* Mandatory information for VBE 3.0 and above */
   /*Bit16u LinBytesPerScanLine*/       640,
   /*Bit8u  BnkNumberOfPages*/          0,
   /*Bit8u  LinNumberOfPages*/          0,
   /*Bit8u  LinRedMaskSize*/            0,
   /*Bit8u  LinRedFieldPosition*/       0,
   /*Bit8u  LinGreenMaskSize*/          0,
   /*Bit8u  LinGreenFieldPosition*/     0,
   /*Bit8u  LinBlueMaskSize*/           0,
   /*Bit8u  LinBlueFieldPosition*/      0,
   /*Bit8u  LinRsvdMaskSize*/           0,
   /*Bit8u  LinRsvdFieldPosition*/      0,
   /*Bit32u MaxPixelClock*/             0,
/*} ModeInfoBlock;*/
                }
        },

        {
                VBE_VESA_MODE_640X480X8,
                {
/*typedef struct ModeInfoBlock
{*/
/* Mandatory information for all VBE revisions */
   /*Bit16u ModeAttributes*/            VBE_MODE_ATTRIBUTE_SUPPORTED |
                                        VBE_MODE_ATTRIBUTE_EXTENDED_INFORMATION_AVAILABLE |
                                        VBE_MODE_ATTRIBUTE_COLOR_MODE |
                                        VBE_MODE_ATTRIBUTE_LINEAR_FRAME_BUFFER_MODE |
                                        VBE_MODE_ATTRIBUTE_GRAPHICS_MODE,
   /*Bit8u  WinAAttributes*/            VBE_WINDOW_ATTRIBUTE_RELOCATABLE |
                                        VBE_WINDOW_ATTRIBUTE_READABLE |
                                        VBE_WINDOW_ATTRIBUTE_WRITEABLE,
   /*Bit8u  WinBAttributes*/            0,
   /*Bit16u WinGranularity*/            VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinSize*/                   VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinASegment*/               VGAMEM_GRAPH,
   /*Bit16u WinBSegment*/               0,
   /*Bit32u WinFuncPtr*/                0,
   /*Bit16u BytesPerScanLine*/          640,
/* Mandatory information for VBE 1.2 and above */
   /*Bit16u XResolution*/               640,
   /*Bit16u YResolution*/               480,
   /*Bit8u  XCharSize*/                 8,
   /*Bit8u  YCharSize*/                 16,
   /*Bit8u  NumberOfPlanes*/            1,
   /*Bit8u  BitsPerPixel*/              8,
   /*Bit8u  NumberOfBanks*/             5, /* 640x480/64kb == 5 */
   /*Bit8u  MemoryModel*/               VBE_MEMORYMODEL_PACKED_PIXEL,
   /*Bit8u  BankSize*/                  0,
   /*Bit8u  NumberOfImagePages*/        11,
   /*Bit8u  Reserved_page*/             0,
/* Direct Color fields (required for direct/6 and YUV/7 memory models) */
   /*Bit8u  RedMaskSize*/               0,
   /*Bit8u  RedFieldPosition*/          0,
   /*Bit8u  GreenMaskSize*/             0,
   /*Bit8u  GreenFieldPosition*/        0,
   /*Bit8u  BlueMaskSize*/              0,
   /*Bit8u  BlueFieldPosition*/         0,
   /*Bit8u  RsvdMaskSize*/              0,
   /*Bit8u  RsvdFieldPosition*/         0,
   /*Bit8u  DirectColorModeInfo*/       0,
/* Mandatory information for VBE 2.0 and above */
   /*Bit32u PhysBasePtr*/               VBE_DISPI_LFB_PHYSICAL_ADDRESS,
   /*Bit32u OffScreenMemOffset*/        0,
   /*Bit16u OffScreenMemSize*/          0,
/* Mandatory information for VBE 3.0 and above */
   /*Bit16u LinBytesPerScanLine*/       640,
   /*Bit8u  BnkNumberOfPages*/          0,
   /*Bit8u  LinNumberOfPages*/          0,
   /*Bit8u  LinRedMaskSize*/            0,
   /*Bit8u  LinRedFieldPosition*/       0,
   /*Bit8u  LinGreenMaskSize*/          0,
   /*Bit8u  LinGreenFieldPosition*/     0,
   /*Bit8u  LinBlueMaskSize*/           0,
   /*Bit8u  LinBlueFieldPosition*/      0,
   /*Bit8u  LinRsvdMaskSize*/           0,
   /*Bit8u  LinRsvdFieldPosition*/      0,
   /*Bit32u MaxPixelClock*/             0,
/*} ModeInfoBlock;*/
                }
        },

        {
                VBE_VESA_MODE_800X600X4,
                {
/*typedef struct ModeInfoBlock
{*/
/* Mandatory information for all VBE revisions */
   /*Bit16u ModeAttributes*/            VBE_MODE_ATTRIBUTE_SUPPORTED |
                                        VBE_MODE_ATTRIBUTE_EXTENDED_INFORMATION_AVAILABLE |
                                        VBE_MODE_ATTRIBUTE_TTY_BIOS_SUPPORT |
                                        VBE_MODE_ATTRIBUTE_COLOR_MODE |
                                        VBE_MODE_ATTRIBUTE_GRAPHICS_MODE,
   /*Bit8u  WinAAttributes*/            VBE_WINDOW_ATTRIBUTE_RELOCATABLE |
                                        VBE_WINDOW_ATTRIBUTE_READABLE |
                                        VBE_WINDOW_ATTRIBUTE_WRITEABLE,
   /*Bit8u  WinBAttributes*/            0,
   /*Bit16u WinGranularity*/            VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinSize*/                   VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinASegment*/               VGAMEM_GRAPH,
   /*Bit16u WinBSegment*/               0,
   /*Bit32u WinFuncPtr*/                0,
   /*Bit16u BytesPerScanLine*/          100,
/* Mandatory information for VBE 1.2 and above */
   /*Bit16u XResolution*/               800,
   /*Bit16u YResolution*/               600,
   /*Bit8u  XCharSize*/                 8,
   /*Bit8u  YCharSize*/                 16,
   /*Bit8u  NumberOfPlanes*/            4,
   /*Bit8u  BitsPerPixel*/              4,
   /*Bit8u  NumberOfBanks*/             16,
   /*Bit8u  MemoryModel*/               VBE_MEMORYMODEL_PLANAR,
   /*Bit8u  BankSize*/                  0,
   /*Bit8u  NumberOfImagePages*/        15,
   /*Bit8u  Reserved_page*/             0,
/* Direct Color fields (required for direct/6 and YUV/7 memory models) */
   /*Bit8u  RedMaskSize*/               0,
   /*Bit8u  RedFieldPosition*/          0,
   /*Bit8u  GreenMaskSize*/             0,
   /*Bit8u  GreenFieldPosition*/        0,
   /*Bit8u  BlueMaskSize*/              0,
   /*Bit8u  BlueFieldPosition*/         0,
   /*Bit8u  RsvdMaskSize*/              0,
   /*Bit8u  RsvdFieldPosition*/         0,
   /*Bit8u  DirectColorModeInfo*/       0,
/* Mandatory information for VBE 2.0 and above */
   /*Bit32u PhysBasePtr*/               0,
   /*Bit32u OffScreenMemOffset*/        0,
   /*Bit16u OffScreenMemSize*/          0,
/* Mandatory information for VBE 3.0 and above */
   /*Bit16u LinBytesPerScanLine*/       100,
   /*Bit8u  BnkNumberOfPages*/          0,
   /*Bit8u  LinNumberOfPages*/          0,
   /*Bit8u  LinRedMaskSize*/            0,
   /*Bit8u  LinRedFieldPosition*/       0,
   /*Bit8u  LinGreenMaskSize*/          0,
   /*Bit8u  LinGreenFieldPosition*/     0,
   /*Bit8u  LinBlueMaskSize*/           0,
   /*Bit8u  LinBlueFieldPosition*/      0,
   /*Bit8u  LinRsvdMaskSize*/           0,
   /*Bit8u  LinRsvdFieldPosition*/      0,
   /*Bit32u MaxPixelClock*/             0,
/*} ModeInfoBlock;*/
                }
        },

        {
                VBE_VESA_MODE_800X600X8,
                {
/*typedef struct ModeInfoBlock
{*/
/* Mandatory information for all VBE revisions */
   /*Bit16u ModeAttributes*/            VBE_MODE_ATTRIBUTE_SUPPORTED |
                                        VBE_MODE_ATTRIBUTE_EXTENDED_INFORMATION_AVAILABLE |
                                        VBE_MODE_ATTRIBUTE_COLOR_MODE |
                                        VBE_MODE_ATTRIBUTE_LINEAR_FRAME_BUFFER_MODE |
                                        VBE_MODE_ATTRIBUTE_GRAPHICS_MODE,
   /*Bit8u  WinAAttributes*/            VBE_WINDOW_ATTRIBUTE_RELOCATABLE |
                                        VBE_WINDOW_ATTRIBUTE_READABLE |
                                        VBE_WINDOW_ATTRIBUTE_WRITEABLE,
   /*Bit8u  WinBAttributes*/            0,
   /*Bit16u WinGranularity*/            VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinSize*/                   VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinASegment*/               VGAMEM_GRAPH,
   /*Bit16u WinBSegment*/               0,
   /*Bit32u WinFuncPtr*/                0,
   /*Bit16u BytesPerScanLine*/          800,
/* Mandatory information for VBE 1.2 and above */
   /*Bit16u XResolution*/               800,
   /*Bit16u YResolution*/               600,
   /*Bit8u  XCharSize*/                 8,
   /*Bit8u  YCharSize*/                 16,
   /*Bit8u  NumberOfPlanes*/            1,
   /*Bit8u  BitsPerPixel*/              8,
   /*Bit8u  NumberOfBanks*/             8, /* 800x600/64kb == 8 */
   /*Bit8u  MemoryModel*/               VBE_MEMORYMODEL_PACKED_PIXEL,
   /*Bit8u  BankSize*/                  0,
   /*Bit8u  NumberOfImagePages*/        7,
   /*Bit8u  Reserved_page*/             0,
/* Direct Color fields (required for direct/6 and YUV/7 memory models) */
   /*Bit8u  RedMaskSize*/               0,
   /*Bit8u  RedFieldPosition*/          0,
   /*Bit8u  GreenMaskSize*/             0,
   /*Bit8u  GreenFieldPosition*/        0,
   /*Bit8u  BlueMaskSize*/              0,
   /*Bit8u  BlueFieldPosition*/         0,
   /*Bit8u  RsvdMaskSize*/              0,
   /*Bit8u  RsvdFieldPosition*/         0,
   /*Bit8u  DirectColorModeInfo*/       0,
/* Mandatory information for VBE 2.0 and above */
   /*Bit32u PhysBasePtr*/               VBE_DISPI_LFB_PHYSICAL_ADDRESS,
   /*Bit32u OffScreenMemOffset*/        0,
   /*Bit16u OffScreenMemSize*/          0,
/* Mandatory information for VBE 3.0 and above */
   /*Bit16u LinBytesPerScanLine*/       800,
   /*Bit8u  BnkNumberOfPages*/          0,
   /*Bit8u  LinNumberOfPages*/          0,
   /*Bit8u  LinRedMaskSize*/            0,
   /*Bit8u  LinRedFieldPosition*/       0,
   /*Bit8u  LinGreenMaskSize*/          0,
   /*Bit8u  LinGreenFieldPosition*/     0,
   /*Bit8u  LinBlueMaskSize*/           0,
   /*Bit8u  LinBlueFieldPosition*/      0,
   /*Bit8u  LinRsvdMaskSize*/           0,
   /*Bit8u  LinRsvdFieldPosition*/      0,
   /*Bit32u MaxPixelClock*/             0,
/*} ModeInfoBlock;*/
                }
        },

        {
                VBE_VESA_MODE_1024X768X8,
                {
/*typedef struct ModeInfoBlock
{*/
/* Mandatory information for all VBE revisions */
   /*Bit16u ModeAttributes*/            VBE_MODE_ATTRIBUTE_SUPPORTED |
                                        VBE_MODE_ATTRIBUTE_EXTENDED_INFORMATION_AVAILABLE |
                                        VBE_MODE_ATTRIBUTE_COLOR_MODE |
                                        VBE_MODE_ATTRIBUTE_LINEAR_FRAME_BUFFER_MODE |
                                        VBE_MODE_ATTRIBUTE_GRAPHICS_MODE,
   /*Bit8u  WinAAttributes*/            VBE_WINDOW_ATTRIBUTE_RELOCATABLE |
                                        VBE_WINDOW_ATTRIBUTE_READABLE |
                                        VBE_WINDOW_ATTRIBUTE_WRITEABLE,
   /*Bit8u  WinBAttributes*/            0,
   /*Bit16u WinGranularity*/            VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinSize*/                   VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinASegment*/               VGAMEM_GRAPH,
   /*Bit16u WinBSegment*/               0,
   /*Bit32u WinFuncPtr*/                0,
   /*Bit16u BytesPerScanLine*/          1024,
/* Mandatory information for VBE 1.2 and above */
   /*Bit16u XResolution*/               1024,
   /*Bit16u YResolution*/               768,
   /*Bit8u  XCharSize*/                 8,
   /*Bit8u  YCharSize*/                 16,
   /*Bit8u  NumberOfPlanes*/            1,
   /*Bit8u  BitsPerPixel*/              8,
   /*Bit8u  NumberOfBanks*/             12, /* 1024x768/64kb == 12 */
   /*Bit8u  MemoryModel*/               VBE_MEMORYMODEL_PACKED_PIXEL,
   /*Bit8u  BankSize*/                  0,
   /*Bit8u  NumberOfImagePages*/        3,
   /*Bit8u  Reserved_page*/             0,
/* Direct Color fields (required for direct/6 and YUV/7 memory models) */
   /*Bit8u  RedMaskSize*/               0,
   /*Bit8u  RedFieldPosition*/          0,
   /*Bit8u  GreenMaskSize*/             0,
   /*Bit8u  GreenFieldPosition*/        0,
   /*Bit8u  BlueMaskSize*/              0,
   /*Bit8u  BlueFieldPosition*/         0,
   /*Bit8u  RsvdMaskSize*/              0,
   /*Bit8u  RsvdFieldPosition*/         0,
   /*Bit8u  DirectColorModeInfo*/       0,
/* Mandatory information for VBE 2.0 and above */
   /*Bit32u PhysBasePtr*/               VBE_DISPI_LFB_PHYSICAL_ADDRESS,
   /*Bit32u OffScreenMemOffset*/        0,
   /*Bit16u OffScreenMemSize*/          0,
/* Mandatory information for VBE 3.0 and above */
   /*Bit16u LinBytesPerScanLine*/       1024,
   /*Bit8u  BnkNumberOfPages*/          0,
   /*Bit8u  LinNumberOfPages*/          0,
   /*Bit8u  LinRedMaskSize*/            0,
   /*Bit8u  LinRedFieldPosition*/       0,
   /*Bit8u  LinGreenMaskSize*/          0,
   /*Bit8u  LinGreenFieldPosition*/     0,
   /*Bit8u  LinBlueMaskSize*/           0,
   /*Bit8u  LinBlueFieldPosition*/      0,
   /*Bit8u  LinRsvdMaskSize*/           0,
   /*Bit8u  LinRsvdFieldPosition*/      0,
   /*Bit32u MaxPixelClock*/             0,
/*} ModeInfoBlock;*/
                }
        },

        {
                VBE_VESA_MODE_640X480X1555,
                {
/*typedef struct ModeInfoBlock
{*/
/* Mandatory information for all VBE revisions */
   /*Bit16u ModeAttributes*/            VBE_MODE_ATTRIBUTE_SUPPORTED |
                                        VBE_MODE_ATTRIBUTE_EXTENDED_INFORMATION_AVAILABLE |
                                        VBE_MODE_ATTRIBUTE_COLOR_MODE |
                                        VBE_MODE_ATTRIBUTE_LINEAR_FRAME_BUFFER_MODE |
                                        VBE_MODE_ATTRIBUTE_GRAPHICS_MODE,
   /*Bit8u  WinAAttributes*/            VBE_WINDOW_ATTRIBUTE_RELOCATABLE |
                                        VBE_WINDOW_ATTRIBUTE_READABLE |
                                        VBE_WINDOW_ATTRIBUTE_WRITEABLE,
   /*Bit8u  WinBAttributes*/            0,
   /*Bit16u WinGranularity*/            VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinSize*/                   VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinASegment*/               VGAMEM_GRAPH,
   /*Bit16u WinBSegment*/               0,
   /*Bit32u WinFuncPtr*/                0,
   /*Bit16u BytesPerScanLine*/          640*2,
/* Mandatory information for VBE 1.2 and above */
   /*Bit16u XResolution*/               640,
   /*Bit16u YResolution*/               480,
   /*Bit8u  XCharSize*/                 8,
   /*Bit8u  YCharSize*/                 16,
   /*Bit8u  NumberOfPlanes*/            1,
   /*Bit8u  BitsPerPixel*/              15,
   /*Bit8u  NumberOfBanks*/             1,
   /*Bit8u  MemoryModel*/               VBE_MEMORYMODEL_DIRECT_COLOR,
   /*Bit8u  BankSize*/                  0,
   /*Bit8u  NumberOfImagePages*/        5,
   /*Bit8u  Reserved_page*/             0,
/* Direct Color fields (required for direct/6 and YUV/7 memory models) */
   /*Bit8u  RedMaskSize*/               5,
   /*Bit8u  RedFieldPosition*/          10,
   /*Bit8u  GreenMaskSize*/             5,
   /*Bit8u  GreenFieldPosition*/        5,
   /*Bit8u  BlueMaskSize*/              5,
   /*Bit8u  BlueFieldPosition*/         0,
   /*Bit8u  RsvdMaskSize*/              1,
   /*Bit8u  RsvdFieldPosition*/         15,
   /*Bit8u  DirectColorModeInfo*/       0,
/* Mandatory information for VBE 2.0 and above */
   /*Bit32u PhysBasePtr*/               VBE_DISPI_LFB_PHYSICAL_ADDRESS,
   /*Bit32u OffScreenMemOffset*/        0,
   /*Bit16u OffScreenMemSize*/          0,
/* Mandatory information for VBE 3.0 and above */
   /*Bit16u LinBytesPerScanLine*/       640*2,
   /*Bit8u  BnkNumberOfPages*/          0,
   /*Bit8u  LinNumberOfPages*/          0,
   /*Bit8u  LinRedMaskSize*/            5,
   /*Bit8u  LinRedFieldPosition*/       10,
   /*Bit8u  LinGreenMaskSize*/          0,
   /*Bit8u  LinGreenFieldPosition*/     5,
   /*Bit8u  LinBlueMaskSize*/           5,
   /*Bit8u  LinBlueFieldPosition*/      0,
   /*Bit8u  LinRsvdMaskSize*/           1,
   /*Bit8u  LinRsvdFieldPosition*/      15,
   /*Bit32u MaxPixelClock*/             0,
/*} ModeInfoBlock;*/
                }
        },

        {
                VBE_VESA_MODE_800X600X1555,
                {
/*typedef struct ModeInfoBlock
{*/
/* Mandatory information for all VBE revisions */
   /*Bit16u ModeAttributes*/            VBE_MODE_ATTRIBUTE_SUPPORTED |
                                        VBE_MODE_ATTRIBUTE_EXTENDED_INFORMATION_AVAILABLE |
                                        VBE_MODE_ATTRIBUTE_COLOR_MODE |
                                        VBE_MODE_ATTRIBUTE_LINEAR_FRAME_BUFFER_MODE |
                                        VBE_MODE_ATTRIBUTE_GRAPHICS_MODE,
   /*Bit8u  WinAAttributes*/            VBE_WINDOW_ATTRIBUTE_RELOCATABLE |
                                        VBE_WINDOW_ATTRIBUTE_READABLE |
                                        VBE_WINDOW_ATTRIBUTE_WRITEABLE,
   /*Bit8u  WinBAttributes*/            0,
   /*Bit16u WinGranularity*/            VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinSize*/                   VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinASegment*/               VGAMEM_GRAPH,
   /*Bit16u WinBSegment*/               0,
   /*Bit32u WinFuncPtr*/                0,
   /*Bit16u BytesPerScanLine*/          800*2,
/* Mandatory information for VBE 1.2 and above */
   /*Bit16u XResolution*/               800,
   /*Bit16u YResolution*/               600,
   /*Bit8u  XCharSize*/                 8,
   /*Bit8u  YCharSize*/                 16,
   /*Bit8u  NumberOfPlanes*/            1,
   /*Bit8u  BitsPerPixel*/              15,
   /*Bit8u  NumberOfBanks*/             1,
   /*Bit8u  MemoryModel*/               VBE_MEMORYMODEL_DIRECT_COLOR,
   /*Bit8u  BankSize*/                  0,
   /*Bit8u  NumberOfImagePages*/        3,
   /*Bit8u  Reserved_page*/             0,
/* Direct Color fields (required for direct/6 and YUV/7 memory models) */
   /*Bit8u  RedMaskSize*/               5,
   /*Bit8u  RedFieldPosition*/          10,
   /*Bit8u  GreenMaskSize*/             5,
   /*Bit8u  GreenFieldPosition*/        5,
   /*Bit8u  BlueMaskSize*/              5,
   /*Bit8u  BlueFieldPosition*/         0,
   /*Bit8u  RsvdMaskSize*/              1,
   /*Bit8u  RsvdFieldPosition*/         15,
   /*Bit8u  DirectColorModeInfo*/       0,
/* Mandatory information for VBE 2.0 and above */
   /*Bit32u PhysBasePtr*/               VBE_DISPI_LFB_PHYSICAL_ADDRESS,
   /*Bit32u OffScreenMemOffset*/        0,
   /*Bit16u OffScreenMemSize*/          0,
/* Mandatory information for VBE 3.0 and above */
   /*Bit16u LinBytesPerScanLine*/       800*2,
   /*Bit8u  BnkNumberOfPages*/          0,
   /*Bit8u  LinNumberOfPages*/          0,
   /*Bit8u  LinRedMaskSize*/            5,
   /*Bit8u  LinRedFieldPosition*/       10,
   /*Bit8u  LinGreenMaskSize*/          5,
   /*Bit8u  LinGreenFieldPosition*/     5,
   /*Bit8u  LinBlueMaskSize*/           5,
   /*Bit8u  LinBlueFieldPosition*/      0,
   /*Bit8u  LinRsvdMaskSize*/           1,
   /*Bit8u  LinRsvdFieldPosition*/      15,
   /*Bit32u MaxPixelClock*/             0,
/*} ModeInfoBlock;*/
                }
        },

        {
                VBE_VESA_MODE_1024X768X1555,
                {
/*typedef struct ModeInfoBlock
{*/
/* Mandatory information for all VBE revisions */
   /*Bit16u ModeAttributes*/            VBE_MODE_ATTRIBUTE_SUPPORTED |
                                        VBE_MODE_ATTRIBUTE_EXTENDED_INFORMATION_AVAILABLE |
                                        VBE_MODE_ATTRIBUTE_COLOR_MODE |
                                        VBE_MODE_ATTRIBUTE_LINEAR_FRAME_BUFFER_MODE |
                                        VBE_MODE_ATTRIBUTE_GRAPHICS_MODE,
   /*Bit8u  WinAAttributes*/            VBE_WINDOW_ATTRIBUTE_RELOCATABLE |
                                        VBE_WINDOW_ATTRIBUTE_READABLE |
                                        VBE_WINDOW_ATTRIBUTE_WRITEABLE,
   /*Bit8u  WinBAttributes*/            0,
   /*Bit16u WinGranularity*/            VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinSize*/                   VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinASegment*/               VGAMEM_GRAPH,
   /*Bit16u WinBSegment*/               0,
   /*Bit32u WinFuncPtr*/                0,
   /*Bit16u BytesPerScanLine*/          1024*2,
/* Mandatory information for VBE 1.2 and above */
   /*Bit16u XResolution*/               1024,
   /*Bit16u YResolution*/               768,
   /*Bit8u  XCharSize*/                 8,
   /*Bit8u  YCharSize*/                 16,
   /*Bit8u  NumberOfPlanes*/            1,
   /*Bit8u  BitsPerPixel*/              15,
   /*Bit8u  NumberOfBanks*/             1,
   /*Bit8u  MemoryModel*/               VBE_MEMORYMODEL_DIRECT_COLOR,
   /*Bit8u  BankSize*/                  0,
   /*Bit8u  NumberOfImagePages*/        1,
   /*Bit8u  Reserved_page*/             0,
/* Direct Color fields (required for direct/6 and YUV/7 memory models) */
   /*Bit8u  RedMaskSize*/               5,
   /*Bit8u  RedFieldPosition*/          10,
   /*Bit8u  GreenMaskSize*/             5,
   /*Bit8u  GreenFieldPosition*/        5,
   /*Bit8u  BlueMaskSize*/              5,
   /*Bit8u  BlueFieldPosition*/         0,
   /*Bit8u  RsvdMaskSize*/              1,
   /*Bit8u  RsvdFieldPosition*/         15,
   /*Bit8u  DirectColorModeInfo*/       0,
/* Mandatory information for VBE 2.0 and above */
   /*Bit32u PhysBasePtr*/               VBE_DISPI_LFB_PHYSICAL_ADDRESS,
   /*Bit32u OffScreenMemOffset*/        0,
   /*Bit16u OffScreenMemSize*/          0,
/* Mandatory information for VBE 3.0 and above */
   /*Bit16u LinBytesPerScanLine*/       1024*2,
   /*Bit8u  BnkNumberOfPages*/          0,
   /*Bit8u  LinNumberOfPages*/          0,
   /*Bit8u  LinRedMaskSize*/            5,
   /*Bit8u  LinRedFieldPosition*/       10,
   /*Bit8u  LinGreenMaskSize*/          5,
   /*Bit8u  LinGreenFieldPosition*/     5,
   /*Bit8u  LinBlueMaskSize*/           5,
   /*Bit8u  LinBlueFieldPosition*/      0,
   /*Bit8u  LinRsvdMaskSize*/           1,
   /*Bit8u  LinRsvdFieldPosition*/      15,
   /*Bit32u MaxPixelClock*/             0,
/*} ModeInfoBlock;*/
                }
        },

        {
                VBE_VESA_MODE_640X480X565,
                {
/*typedef struct ModeInfoBlock
{*/
/* Mandatory information for all VBE revisions */
   /*Bit16u ModeAttributes*/            VBE_MODE_ATTRIBUTE_SUPPORTED |
                                        VBE_MODE_ATTRIBUTE_EXTENDED_INFORMATION_AVAILABLE |
                                        VBE_MODE_ATTRIBUTE_COLOR_MODE |
                                        VBE_MODE_ATTRIBUTE_LINEAR_FRAME_BUFFER_MODE |
                                        VBE_MODE_ATTRIBUTE_GRAPHICS_MODE,
   /*Bit8u  WinAAttributes*/            VBE_WINDOW_ATTRIBUTE_RELOCATABLE |
                                        VBE_WINDOW_ATTRIBUTE_READABLE |
                                        VBE_WINDOW_ATTRIBUTE_WRITEABLE,
   /*Bit8u  WinBAttributes*/            0,
   /*Bit16u WinGranularity*/            VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinSize*/                   VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinASegment*/               VGAMEM_GRAPH,
   /*Bit16u WinBSegment*/               0,
   /*Bit32u WinFuncPtr*/                0,
   /*Bit16u BytesPerScanLine*/          640*2,
/* Mandatory information for VBE 1.2 and above */
   /*Bit16u XResolution*/               640,
   /*Bit16u YResolution*/               480,
   /*Bit8u  XCharSize*/                 8,
   /*Bit8u  YCharSize*/                 16,
   /*Bit8u  NumberOfPlanes*/            1,
   /*Bit8u  BitsPerPixel*/              16,
   /*Bit8u  NumberOfBanks*/             1,
   /*Bit8u  MemoryModel*/               VBE_MEMORYMODEL_DIRECT_COLOR,
   /*Bit8u  BankSize*/                  0,
   /*Bit8u  NumberOfImagePages*/        5,
   /*Bit8u  Reserved_page*/             0,
/* Direct Color fields (required for direct/6 and YUV/7 memory models) */
   /*Bit8u  RedMaskSize*/               5,
   /*Bit8u  RedFieldPosition*/          11,
   /*Bit8u  GreenMaskSize*/             6,
   /*Bit8u  GreenFieldPosition*/        5,
   /*Bit8u  BlueMaskSize*/              5,
   /*Bit8u  BlueFieldPosition*/         0,
   /*Bit8u  RsvdMaskSize*/              0,
   /*Bit8u  RsvdFieldPosition*/         0,
   /*Bit8u  DirectColorModeInfo*/       0,
/* Mandatory information for VBE 2.0 and above */
   /*Bit32u PhysBasePtr*/               VBE_DISPI_LFB_PHYSICAL_ADDRESS,
   /*Bit32u OffScreenMemOffset*/        0,
   /*Bit16u OffScreenMemSize*/          0,
/* Mandatory information for VBE 3.0 and above */
   /*Bit16u LinBytesPerScanLine*/       640*2,
   /*Bit8u  BnkNumberOfPages*/          0,
   /*Bit8u  LinNumberOfPages*/          0,
   /*Bit8u  LinRedMaskSize*/            5,
   /*Bit8u  LinRedFieldPosition*/       11,
   /*Bit8u  LinGreenMaskSize*/          6,
   /*Bit8u  LinGreenFieldPosition*/     5,
   /*Bit8u  LinBlueMaskSize*/           5,
   /*Bit8u  LinBlueFieldPosition*/      0,
   /*Bit8u  LinRsvdMaskSize*/           0,
   /*Bit8u  LinRsvdFieldPosition*/      0,
   /*Bit32u MaxPixelClock*/             0,
/*} ModeInfoBlock;*/
                }
        },

        {
                VBE_VESA_MODE_800X600X565,
                {
/*typedef struct ModeInfoBlock
{*/
/* Mandatory information for all VBE revisions */
   /*Bit16u ModeAttributes*/            VBE_MODE_ATTRIBUTE_SUPPORTED |
                                        VBE_MODE_ATTRIBUTE_EXTENDED_INFORMATION_AVAILABLE |
                                        VBE_MODE_ATTRIBUTE_COLOR_MODE |
                                        VBE_MODE_ATTRIBUTE_LINEAR_FRAME_BUFFER_MODE |
                                        VBE_MODE_ATTRIBUTE_GRAPHICS_MODE,
   /*Bit8u  WinAAttributes*/            VBE_WINDOW_ATTRIBUTE_RELOCATABLE |
                                        VBE_WINDOW_ATTRIBUTE_READABLE |
                                        VBE_WINDOW_ATTRIBUTE_WRITEABLE,
   /*Bit8u  WinBAttributes*/            0,
   /*Bit16u WinGranularity*/            VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinSize*/                   VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinASegment*/               VGAMEM_GRAPH,
   /*Bit16u WinBSegment*/               0,
   /*Bit32u WinFuncPtr*/                0,
   /*Bit16u BytesPerScanLine*/          800*2,
/* Mandatory information for VBE 1.2 and above */
   /*Bit16u XResolution*/               800,
   /*Bit16u YResolution*/               600,
   /*Bit8u  XCharSize*/                 8,
   /*Bit8u  YCharSize*/                 16,
   /*Bit8u  NumberOfPlanes*/            1,
   /*Bit8u  BitsPerPixel*/              16,
   /*Bit8u  NumberOfBanks*/             1,
   /*Bit8u  MemoryModel*/               VBE_MEMORYMODEL_DIRECT_COLOR,
   /*Bit8u  BankSize*/                  0,
   /*Bit8u  NumberOfImagePages*/        3,
   /*Bit8u  Reserved_page*/             0,
/* Direct Color fields (required for direct/6 and YUV/7 memory models) */
   /*Bit8u  RedMaskSize*/               5,
   /*Bit8u  RedFieldPosition*/          11,
   /*Bit8u  GreenMaskSize*/             6,
   /*Bit8u  GreenFieldPosition*/        5,
   /*Bit8u  BlueMaskSize*/              5,
   /*Bit8u  BlueFieldPosition*/         0,
   /*Bit8u  RsvdMaskSize*/              0,
   /*Bit8u  RsvdFieldPosition*/         0,
   /*Bit8u  DirectColorModeInfo*/       0,
/* Mandatory information for VBE 2.0 and above */
   /*Bit32u PhysBasePtr*/               VBE_DISPI_LFB_PHYSICAL_ADDRESS,
   /*Bit32u OffScreenMemOffset*/        0,
   /*Bit16u OffScreenMemSize*/          0,
/* Mandatory information for VBE 3.0 and above */
   /*Bit16u LinBytesPerScanLine*/       800*2,
   /*Bit8u  BnkNumberOfPages*/          0,
   /*Bit8u  LinNumberOfPages*/          0,
   /*Bit8u  LinRedMaskSize*/            5,
   /*Bit8u  LinRedFieldPosition*/       11,
   /*Bit8u  LinGreenMaskSize*/          6,
   /*Bit8u  LinGreenFieldPosition*/     5,
   /*Bit8u  LinBlueMaskSize*/           5,
   /*Bit8u  LinBlueFieldPosition*/      0,
   /*Bit8u  LinRsvdMaskSize*/           0,
   /*Bit8u  LinRsvdFieldPosition*/      0,
   /*Bit32u MaxPixelClock*/             0,
/*} ModeInfoBlock;*/
                }
        },

        {
                VBE_VESA_MODE_1024X768X565,
                {
/*typedef struct ModeInfoBlock
{*/
/* Mandatory information for all VBE revisions */
   /*Bit16u ModeAttributes*/            VBE_MODE_ATTRIBUTE_SUPPORTED |
                                        VBE_MODE_ATTRIBUTE_EXTENDED_INFORMATION_AVAILABLE |
                                        VBE_MODE_ATTRIBUTE_COLOR_MODE |
                                        VBE_MODE_ATTRIBUTE_LINEAR_FRAME_BUFFER_MODE |
                                        VBE_MODE_ATTRIBUTE_GRAPHICS_MODE,
   /*Bit8u  WinAAttributes*/            VBE_WINDOW_ATTRIBUTE_RELOCATABLE |
                                        VBE_WINDOW_ATTRIBUTE_READABLE |
                                        VBE_WINDOW_ATTRIBUTE_WRITEABLE,
   /*Bit8u  WinBAttributes*/            0,
   /*Bit16u WinGranularity*/            VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinSize*/                   VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinASegment*/               VGAMEM_GRAPH,
   /*Bit16u WinBSegment*/               0,
   /*Bit32u WinFuncPtr*/                0,
   /*Bit16u BytesPerScanLine*/          1024*2,
/* Mandatory information for VBE 1.2 and above */
   /*Bit16u XResolution*/               1024,
   /*Bit16u YResolution*/               768,
   /*Bit8u  XCharSize*/                 8,
   /*Bit8u  YCharSize*/                 16,
   /*Bit8u  NumberOfPlanes*/            1,
   /*Bit8u  BitsPerPixel*/              16,
   /*Bit8u  NumberOfBanks*/             1,
   /*Bit8u  MemoryModel*/               VBE_MEMORYMODEL_DIRECT_COLOR,
   /*Bit8u  BankSize*/                  0,
   /*Bit8u  NumberOfImagePages*/        1,
   /*Bit8u  Reserved_page*/             0,
/* Direct Color fields (required for direct/6 and YUV/7 memory models) */
   /*Bit8u  RedMaskSize*/               5,
   /*Bit8u  RedFieldPosition*/          11,
   /*Bit8u  GreenMaskSize*/             6,
   /*Bit8u  GreenFieldPosition*/        5,
   /*Bit8u  BlueMaskSize*/              5,
   /*Bit8u  BlueFieldPosition*/         0,
   /*Bit8u  RsvdMaskSize*/              0,
   /*Bit8u  RsvdFieldPosition*/         0,
   /*Bit8u  DirectColorModeInfo*/       0,
/* Mandatory information for VBE 2.0 and above */
   /*Bit32u PhysBasePtr*/               VBE_DISPI_LFB_PHYSICAL_ADDRESS,
   /*Bit32u OffScreenMemOffset*/        0,
   /*Bit16u OffScreenMemSize*/          0,
/* Mandatory information for VBE 3.0 and above */
   /*Bit16u LinBytesPerScanLine*/       1024*2,
   /*Bit8u  BnkNumberOfPages*/          0,
   /*Bit8u  LinNumberOfPages*/          0,
   /*Bit8u  LinRedMaskSize*/            5,
   /*Bit8u  LinRedFieldPosition*/       11,
   /*Bit8u  LinGreenMaskSize*/          6,
   /*Bit8u  LinGreenFieldPosition*/     5,
   /*Bit8u  LinBlueMaskSize*/           5,
   /*Bit8u  LinBlueFieldPosition*/      0,
   /*Bit8u  LinRsvdMaskSize*/           0,
   /*Bit8u  LinRsvdFieldPosition*/      0,
   /*Bit32u MaxPixelClock*/             0,
/*} ModeInfoBlock;*/
                }
        },

        {
                VBE_VESA_MODE_640X480X888,
                {
/*typedef struct ModeInfoBlock
{*/
/* Mandatory information for all VBE revisions */
   /*Bit16u ModeAttributes*/            VBE_MODE_ATTRIBUTE_SUPPORTED |
                                        VBE_MODE_ATTRIBUTE_EXTENDED_INFORMATION_AVAILABLE |
                                        VBE_MODE_ATTRIBUTE_COLOR_MODE |
                                        VBE_MODE_ATTRIBUTE_LINEAR_FRAME_BUFFER_MODE |
                                        VBE_MODE_ATTRIBUTE_GRAPHICS_MODE,
   /*Bit8u  WinAAttributes*/            VBE_WINDOW_ATTRIBUTE_RELOCATABLE |
                                        VBE_WINDOW_ATTRIBUTE_READABLE |
                                        VBE_WINDOW_ATTRIBUTE_WRITEABLE,
   /*Bit8u  WinBAttributes*/            0,
   /*Bit16u WinGranularity*/            VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinSize*/                   VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinASegment*/               VGAMEM_GRAPH,
   /*Bit16u WinBSegment*/               0,
   /*Bit32u WinFuncPtr*/                0,
   /*Bit16u BytesPerScanLine*/          640*3,
/* Mandatory information for VBE 1.2 and above */
   /*Bit16u XResolution*/               640,
   /*Bit16u YResolution*/               480,
   /*Bit8u  XCharSize*/                 8,
   /*Bit8u  YCharSize*/                 16,
   /*Bit8u  NumberOfPlanes*/            1,
   /*Bit8u  BitsPerPixel*/              24,
   /*Bit8u  NumberOfBanks*/             1,
   /*Bit8u  MemoryModel*/               VBE_MEMORYMODEL_DIRECT_COLOR,
   /*Bit8u  BankSize*/                  0,
   /*Bit8u  NumberOfImagePages*/        3,
   /*Bit8u  Reserved_page*/             0,
/* Direct Color fields (required for direct/6 and YUV/7 memory models) */
   /*Bit8u  RedMaskSize*/               8,
   /*Bit8u  RedFieldPosition*/          16,
   /*Bit8u  GreenMaskSize*/             8,
   /*Bit8u  GreenFieldPosition*/        8,
   /*Bit8u  BlueMaskSize*/              8,
   /*Bit8u  BlueFieldPosition*/         0,
   /*Bit8u  RsvdMaskSize*/              0,
   /*Bit8u  RsvdFieldPosition*/         0,
   /*Bit8u  DirectColorModeInfo*/       0,
/* Mandatory information for VBE 2.0 and above */
   /*Bit32u PhysBasePtr*/               VBE_DISPI_LFB_PHYSICAL_ADDRESS,
   /*Bit32u OffScreenMemOffset*/        0,
   /*Bit16u OffScreenMemSize*/          0,
/* Mandatory information for VBE 3.0 and above */
   /*Bit16u LinBytesPerScanLine*/       640*3,
   /*Bit8u  BnkNumberOfPages*/          0,
   /*Bit8u  LinNumberOfPages*/          0,
   /*Bit8u  LinRedMaskSize*/            8,
   /*Bit8u  LinRedFieldPosition*/       16,
   /*Bit8u  LinGreenMaskSize*/          8,
   /*Bit8u  LinGreenFieldPosition*/     8,
   /*Bit8u  LinBlueMaskSize*/           8,
   /*Bit8u  LinBlueFieldPosition*/      0,
   /*Bit8u  LinRsvdMaskSize*/           0,
   /*Bit8u  LinRsvdFieldPosition*/      0,
   /*Bit32u MaxPixelClock*/             0,
/*} ModeInfoBlock;*/
                }
        },

        {
                VBE_VESA_MODE_800X600X888,
                {
/*typedef struct ModeInfoBlock
{*/
/* Mandatory information for all VBE revisions */
   /*Bit16u ModeAttributes*/            VBE_MODE_ATTRIBUTE_SUPPORTED |
                                        VBE_MODE_ATTRIBUTE_EXTENDED_INFORMATION_AVAILABLE |
                                        VBE_MODE_ATTRIBUTE_COLOR_MODE |
                                        VBE_MODE_ATTRIBUTE_LINEAR_FRAME_BUFFER_MODE |
                                        VBE_MODE_ATTRIBUTE_GRAPHICS_MODE,
   /*Bit8u  WinAAttributes*/            VBE_WINDOW_ATTRIBUTE_RELOCATABLE |
                                        VBE_WINDOW_ATTRIBUTE_READABLE |
                                        VBE_WINDOW_ATTRIBUTE_WRITEABLE,
   /*Bit8u  WinBAttributes*/            0,
   /*Bit16u WinGranularity*/            VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinSize*/                   VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinASegment*/               VGAMEM_GRAPH,
   /*Bit16u WinBSegment*/               0,
   /*Bit32u WinFuncPtr*/                0,
   /*Bit16u BytesPerScanLine*/          800*3,
/* Mandatory information for VBE 1.2 and above */
   /*Bit16u XResolution*/               800,
   /*Bit16u YResolution*/               600,
   /*Bit8u  XCharSize*/                 8,
   /*Bit8u  YCharSize*/                 16,
   /*Bit8u  NumberOfPlanes*/            1,
   /*Bit8u  BitsPerPixel*/              24,
   /*Bit8u  NumberOfBanks*/             1,
   /*Bit8u  MemoryModel*/               VBE_MEMORYMODEL_DIRECT_COLOR,
   /*Bit8u  BankSize*/                  0,
   /*Bit8u  NumberOfImagePages*/        1,
   /*Bit8u  Reserved_page*/             0,
/* Direct Color fields (required for direct/6 and YUV/7 memory models) */
   /*Bit8u  RedMaskSize*/               8,
   /*Bit8u  RedFieldPosition*/          16,
   /*Bit8u  GreenMaskSize*/             8,
   /*Bit8u  GreenFieldPosition*/        8,
   /*Bit8u  BlueMaskSize*/              8,
   /*Bit8u  BlueFieldPosition*/         0,
   /*Bit8u  RsvdMaskSize*/              0,
   /*Bit8u  RsvdFieldPosition*/         0,
   /*Bit8u  DirectColorModeInfo*/       0,
/* Mandatory information for VBE 2.0 and above */
   /*Bit32u PhysBasePtr*/               VBE_DISPI_LFB_PHYSICAL_ADDRESS,
   /*Bit32u OffScreenMemOffset*/        0,
   /*Bit16u OffScreenMemSize*/          0,
/* Mandatory information for VBE 3.0 and above */
   /*Bit16u LinBytesPerScanLine*/       800*3,
   /*Bit8u  BnkNumberOfPages*/          0,
   /*Bit8u  LinNumberOfPages*/          0,
   /*Bit8u  LinRedMaskSize*/            8,
   /*Bit8u  LinRedFieldPosition*/       16,
   /*Bit8u  LinGreenMaskSize*/          8,
   /*Bit8u  LinGreenFieldPosition*/     8,
   /*Bit8u  LinBlueMaskSize*/           8,
   /*Bit8u  LinBlueFieldPosition*/      0,
   /*Bit8u  LinRsvdMaskSize*/           0,
   /*Bit8u  LinRsvdFieldPosition*/      0,
   /*Bit32u MaxPixelClock*/             0,
/*} ModeInfoBlock;*/
                }
        },

        {
                VBE_VESA_MODE_1024X768X888,
                {
/*typedef struct ModeInfoBlock
{*/
/* Mandatory information for all VBE revisions */
   /*Bit16u ModeAttributes*/            VBE_MODE_ATTRIBUTE_SUPPORTED |
                                        VBE_MODE_ATTRIBUTE_EXTENDED_INFORMATION_AVAILABLE |
                                        VBE_MODE_ATTRIBUTE_COLOR_MODE |
                                        VBE_MODE_ATTRIBUTE_LINEAR_FRAME_BUFFER_MODE |
                                        VBE_MODE_ATTRIBUTE_GRAPHICS_MODE,
   /*Bit8u  WinAAttributes*/            VBE_WINDOW_ATTRIBUTE_RELOCATABLE |
                                        VBE_WINDOW_ATTRIBUTE_READABLE |
                                        VBE_WINDOW_ATTRIBUTE_WRITEABLE,
   /*Bit8u  WinBAttributes*/            0,
   /*Bit16u WinGranularity*/            VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinSize*/                   VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinASegment*/               VGAMEM_GRAPH,
   /*Bit16u WinBSegment*/               0,
   /*Bit32u WinFuncPtr*/                0,
   /*Bit16u BytesPerScanLine*/          1024*3,
/* Mandatory information for VBE 1.2 and above */
   /*Bit16u XResolution*/               1024,
   /*Bit16u YResolution*/               768,
   /*Bit8u  XCharSize*/                 8,
   /*Bit8u  YCharSize*/                 16,
   /*Bit8u  NumberOfPlanes*/            1,
   /*Bit8u  BitsPerPixel*/              24,
   /*Bit8u  NumberOfBanks*/             1,
   /*Bit8u  MemoryModel*/               VBE_MEMORYMODEL_DIRECT_COLOR,
   /*Bit8u  BankSize*/                  0,
   /*Bit8u  NumberOfImagePages*/        0,
   /*Bit8u  Reserved_page*/             0,
/* Direct Color fields (required for direct/6 and YUV/7 memory models) */
   /*Bit8u  RedMaskSize*/               8,
   /*Bit8u  RedFieldPosition*/          16,
   /*Bit8u  GreenMaskSize*/             8,
   /*Bit8u  GreenFieldPosition*/        8,
   /*Bit8u  BlueMaskSize*/              8,
   /*Bit8u  BlueFieldPosition*/         0,
   /*Bit8u  RsvdMaskSize*/              0,
   /*Bit8u  RsvdFieldPosition*/         0,
   /*Bit8u  DirectColorModeInfo*/       0,
/* Mandatory information for VBE 2.0 and above */
   /*Bit32u PhysBasePtr*/               VBE_DISPI_LFB_PHYSICAL_ADDRESS,
   /*Bit32u OffScreenMemOffset*/        0,
   /*Bit16u OffScreenMemSize*/          0,
/* Mandatory information for VBE 3.0 and above */
   /*Bit16u LinBytesPerScanLine*/       1024*3,
   /*Bit8u  BnkNumberOfPages*/          0,
   /*Bit8u  LinNumberOfPages*/          0,
   /*Bit8u  LinRedMaskSize*/            8,
   /*Bit8u  LinRedFieldPosition*/       16,
   /*Bit8u  LinGreenMaskSize*/          8,
   /*Bit8u  LinGreenFieldPosition*/     8,
   /*Bit8u  LinBlueMaskSize*/           8,
   /*Bit8u  LinBlueFieldPosition*/      0,
   /*Bit8u  LinRsvdMaskSize*/           0,
   /*Bit8u  LinRsvdFieldPosition*/      0,
   /*Bit32u MaxPixelClock*/             0,
/*} ModeInfoBlock;*/
                }
        },

        {
                VBE_OWN_MODE_640X480X8888,
                {
/*typedef struct ModeInfoBlock
{*/
/* Mandatory information for all VBE revisions */
   /*Bit16u ModeAttributes*/            VBE_MODE_ATTRIBUTE_SUPPORTED |
                                        VBE_MODE_ATTRIBUTE_EXTENDED_INFORMATION_AVAILABLE |
                                        VBE_MODE_ATTRIBUTE_COLOR_MODE |
                                        VBE_MODE_ATTRIBUTE_LINEAR_FRAME_BUFFER_MODE |
                                        VBE_MODE_ATTRIBUTE_GRAPHICS_MODE,
   /*Bit8u  WinAAttributes*/            VBE_WINDOW_ATTRIBUTE_RELOCATABLE |
                                        VBE_WINDOW_ATTRIBUTE_READABLE |
                                        VBE_WINDOW_ATTRIBUTE_WRITEABLE,
   /*Bit8u  WinBAttributes*/            0,
   /*Bit16u WinGranularity*/            VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinSize*/                   VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinASegment*/               VGAMEM_GRAPH,
   /*Bit16u WinBSegment*/               0,
   /*Bit32u WinFuncPtr*/                0,
   /*Bit16u BytesPerScanLine*/          640*4,
/* Mandatory information for VBE 1.2 and above */
   /*Bit16u XResolution*/               640,
   /*Bit16u YResolution*/               480,
   /*Bit8u  XCharSize*/                 8,
   /*Bit8u  YCharSize*/                 16,
   /*Bit8u  NumberOfPlanes*/            1,
   /*Bit8u  BitsPerPixel*/              32,
   /*Bit8u  NumberOfBanks*/             1,
   /*Bit8u  MemoryModel*/               VBE_MEMORYMODEL_DIRECT_COLOR,
   /*Bit8u  BankSize*/                  0,
   /*Bit8u  NumberOfImagePages*/        1,
   /*Bit8u  Reserved_page*/             0,
/* Direct Color fields (required for direct/6 and YUV/7 memory models) */
   /*Bit8u  RedMaskSize*/               8,
   /*Bit8u  RedFieldPosition*/          16,
   /*Bit8u  GreenMaskSize*/             8,
   /*Bit8u  GreenFieldPosition*/        8,
   /*Bit8u  BlueMaskSize*/              8,
   /*Bit8u  BlueFieldPosition*/         0,
   /*Bit8u  RsvdMaskSize*/              8,
   /*Bit8u  RsvdFieldPosition*/         24,
   /*Bit8u  DirectColorModeInfo*/       VBE_DIRECTCOLOR_RESERVED_BITS_AVAILABLE,
/* Mandatory information for VBE 2.0 and above */
   /*Bit32u PhysBasePtr*/               VBE_DISPI_LFB_PHYSICAL_ADDRESS,
   /*Bit32u OffScreenMemOffset*/        0,
   /*Bit16u OffScreenMemSize*/          0,
/* Mandatory information for VBE 3.0 and above */
   /*Bit16u LinBytesPerScanLine*/       640*4,
   /*Bit8u  BnkNumberOfPages*/          0,
   /*Bit8u  LinNumberOfPages*/          0,
   /*Bit8u  LinRedMaskSize*/            8,
   /*Bit8u  LinRedFieldPosition*/       16,
   /*Bit8u  LinGreenMaskSize*/          8,
   /*Bit8u  LinGreenFieldPosition*/     8,
   /*Bit8u  LinBlueMaskSize*/           8,
   /*Bit8u  LinBlueFieldPosition*/      0,
   /*Bit8u  LinRsvdMaskSize*/           8,
   /*Bit8u  LinRsvdFieldPosition*/      24,
   /*Bit32u MaxPixelClock*/             0,
/*} ModeInfoBlock;*/
                }
        },

        {
                VBE_OWN_MODE_800X600X8888,
                {
/*typedef struct ModeInfoBlock
{*/
/* Mandatory information for all VBE revisions */
   /*Bit16u ModeAttributes*/            VBE_MODE_ATTRIBUTE_SUPPORTED |
                                        VBE_MODE_ATTRIBUTE_EXTENDED_INFORMATION_AVAILABLE |
                                        VBE_MODE_ATTRIBUTE_COLOR_MODE |
                                        VBE_MODE_ATTRIBUTE_LINEAR_FRAME_BUFFER_MODE |
                                        VBE_MODE_ATTRIBUTE_GRAPHICS_MODE,
   /*Bit8u  WinAAttributes*/            VBE_WINDOW_ATTRIBUTE_RELOCATABLE |
                                        VBE_WINDOW_ATTRIBUTE_READABLE |
                                        VBE_WINDOW_ATTRIBUTE_WRITEABLE,
   /*Bit8u  WinBAttributes*/            0,
   /*Bit16u WinGranularity*/            VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinSize*/                   VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinASegment*/               VGAMEM_GRAPH,
   /*Bit16u WinBSegment*/               0,
   /*Bit32u WinFuncPtr*/                0,
   /*Bit16u BytesPerScanLine*/          800*4,
/* Mandatory information for VBE 1.2 and above */
   /*Bit16u XResolution*/               800,
   /*Bit16u YResolution*/               600,
   /*Bit8u  XCharSize*/                 8,
   /*Bit8u  YCharSize*/                 16,
   /*Bit8u  NumberOfPlanes*/            1,
   /*Bit8u  BitsPerPixel*/              32,
   /*Bit8u  NumberOfBanks*/             1,
   /*Bit8u  MemoryModel*/               VBE_MEMORYMODEL_DIRECT_COLOR,
   /*Bit8u  BankSize*/                  0,
   /*Bit8u  NumberOfImagePages*/        1,
   /*Bit8u  Reserved_page*/             0,
/* Direct Color fields (required for direct/6 and YUV/7 memory models) */
   /*Bit8u  RedMaskSize*/               8,
   /*Bit8u  RedFieldPosition*/          16,
   /*Bit8u  GreenMaskSize*/             8,
   /*Bit8u  GreenFieldPosition*/        8,
   /*Bit8u  BlueMaskSize*/              8,
   /*Bit8u  BlueFieldPosition*/         0,
   /*Bit8u  RsvdMaskSize*/              8,
   /*Bit8u  RsvdFieldPosition*/         24,
   /*Bit8u  DirectColorModeInfo*/       VBE_DIRECTCOLOR_RESERVED_BITS_AVAILABLE,
/* Mandatory information for VBE 2.0 and above */
   /*Bit32u PhysBasePtr*/               VBE_DISPI_LFB_PHYSICAL_ADDRESS,
   /*Bit32u OffScreenMemOffset*/        0,
   /*Bit16u OffScreenMemSize*/          0,
/* Mandatory information for VBE 3.0 and above */
   /*Bit16u LinBytesPerScanLine*/       800*4,
   /*Bit8u  BnkNumberOfPages*/          0,
   /*Bit8u  LinNumberOfPages*/          0,
   /*Bit8u  LinRedMaskSize*/            8,
   /*Bit8u  LinRedFieldPosition*/       16,
   /*Bit8u  LinGreenMaskSize*/          8,
   /*Bit8u  LinGreenFieldPosition*/     8,
   /*Bit8u  LinBlueMaskSize*/           8,
   /*Bit8u  LinBlueFieldPosition*/      0,
   /*Bit8u  LinRsvdMaskSize*/           8,
   /*Bit8u  LinRsvdFieldPosition*/      24,
   /*Bit32u MaxPixelClock*/             0,
/*} ModeInfoBlock;*/
                }
        },

        {
                VBE_OWN_MODE_1024X768X8888,
                {
/*typedef struct ModeInfoBlock
{*/
/* Mandatory information for all VBE revisions */
   /*Bit16u ModeAttributes*/            VBE_MODE_ATTRIBUTE_SUPPORTED |
                                        VBE_MODE_ATTRIBUTE_EXTENDED_INFORMATION_AVAILABLE |
                                        VBE_MODE_ATTRIBUTE_COLOR_MODE |
                                        VBE_MODE_ATTRIBUTE_LINEAR_FRAME_BUFFER_MODE |
                                        VBE_MODE_ATTRIBUTE_GRAPHICS_MODE,
   /*Bit8u  WinAAttributes*/            VBE_WINDOW_ATTRIBUTE_RELOCATABLE |
                                        VBE_WINDOW_ATTRIBUTE_READABLE |
                                        VBE_WINDOW_ATTRIBUTE_WRITEABLE,
   /*Bit8u  WinBAttributes*/            0,
   /*Bit16u WinGranularity*/            VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinSize*/                   VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinASegment*/               VGAMEM_GRAPH,
   /*Bit16u WinBSegment*/               0,
   /*Bit32u WinFuncPtr*/                0,
   /*Bit16u BytesPerScanLine*/          1024*4,
/* Mandatory information for VBE 1.2 and above */
   /*Bit16u XResolution*/               1024,
   /*Bit16u YResolution*/               768,
   /*Bit8u  XCharSize*/                 8,
   /*Bit8u  YCharSize*/                 16,
   /*Bit8u  NumberOfPlanes*/            1,
   /*Bit8u  BitsPerPixel*/              32,
   /*Bit8u  NumberOfBanks*/             1,
   /*Bit8u  MemoryModel*/               VBE_MEMORYMODEL_DIRECT_COLOR,
   /*Bit8u  BankSize*/                  0,
   /*Bit8u  NumberOfImagePages*/        1,
   /*Bit8u  Reserved_page*/             0,
/* Direct Color fields (required for direct/6 and YUV/7 memory models) */
   /*Bit8u  RedMaskSize*/               8,
   /*Bit8u  RedFieldPosition*/          16,
   /*Bit8u  GreenMaskSize*/             8,
   /*Bit8u  GreenFieldPosition*/        8,
   /*Bit8u  BlueMaskSize*/              8,
   /*Bit8u  BlueFieldPosition*/         0,
   /*Bit8u  RsvdMaskSize*/              8,
   /*Bit8u  RsvdFieldPosition*/         24,
   /*Bit8u  DirectColorModeInfo*/       VBE_DIRECTCOLOR_RESERVED_BITS_AVAILABLE,
/* Mandatory information for VBE 2.0 and above */
   /*Bit32u PhysBasePtr*/               VBE_DISPI_LFB_PHYSICAL_ADDRESS,
   /*Bit32u OffScreenMemOffset*/        0,
   /*Bit16u OffScreenMemSize*/          0,
/* Mandatory information for VBE 3.0 and above */
   /*Bit16u LinBytesPerScanLine*/       1024*4,
   /*Bit8u  BnkNumberOfPages*/          0,
   /*Bit8u  LinNumberOfPages*/          0,
   /*Bit8u  LinRedMaskSize*/            8,
   /*Bit8u  LinRedFieldPosition*/       16,
   /*Bit8u  LinGreenMaskSize*/          8,
   /*Bit8u  LinGreenFieldPosition*/     8,
   /*Bit8u  LinBlueMaskSize*/           8,
   /*Bit8u  LinBlueFieldPosition*/      0,
   /*Bit8u  LinRsvdMaskSize*/           8,
   /*Bit8u  LinRsvdFieldPosition*/      24,
   /*Bit32u MaxPixelClock*/             0,
/*} ModeInfoBlock;*/
                }
        },

        {
                VBE_OWN_MODE_320X200X8,
                {
/*typedef struct ModeInfoBlock
{*/
/* Mandatory information for all VBE revisions */
   /*Bit16u ModeAttributes*/            VBE_MODE_ATTRIBUTE_SUPPORTED |
                                        VBE_MODE_ATTRIBUTE_EXTENDED_INFORMATION_AVAILABLE |
                                        VBE_MODE_ATTRIBUTE_COLOR_MODE |
                                        VBE_MODE_ATTRIBUTE_LINEAR_FRAME_BUFFER_MODE |
                                        VBE_MODE_ATTRIBUTE_GRAPHICS_MODE,
   /*Bit8u  WinAAttributes*/            VBE_WINDOW_ATTRIBUTE_READABLE |
                                        VBE_WINDOW_ATTRIBUTE_WRITEABLE,
   /*Bit8u  WinBAttributes*/            0,
   /*Bit16u WinGranularity*/            VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinSize*/                   VBE_DISPI_BANK_SIZE_KB,
   /*Bit16u WinASegment*/               VGAMEM_GRAPH,
   /*Bit16u WinBSegment*/               0,
   /*Bit32u WinFuncPtr*/                0,
   /*Bit16u BytesPerScanLine*/          320,
/* Mandatory information for VBE 1.2 and above */
   /*Bit16u XResolution*/               320,
   /*Bit16u YResolution*/               200,
   /*Bit8u  XCharSize*/                 8,
   /*Bit8u  YCharSize*/                 16,
   /*Bit8u  NumberOfPlanes*/            1,
   /*Bit8u  BitsPerPixel*/              8,
   /*Bit8u  NumberOfBanks*/             1,
   /*Bit8u  MemoryModel*/               VBE_MEMORYMODEL_PACKED_PIXEL,
   /*Bit8u  BankSize*/                  0,
   /*Bit8u  NumberOfImagePages*/        3,
   /*Bit8u  Reserved_page*/             0,
/* Direct Color fields (required for direct/6 and YUV/7 memory models) */
   /*Bit8u  RedMaskSize*/               0,
   /*Bit8u  RedFieldPosition*/          0,
   /*Bit8u  GreenMaskSize*/             0,
   /*Bit8u  GreenFieldPosition*/        0,
   /*Bit8u  BlueMaskSize*/              0,
   /*Bit8u  BlueFieldPosition*/         0,
   /*Bit8u  RsvdMaskSize*/              0,
   /*Bit8u  RsvdFieldPosition*/         0,
   /*Bit8u  DirectColorModeInfo*/       0,
/* Mandatory information for VBE 2.0 and above */
   /*Bit32u PhysBasePtr*/               VBE_DISPI_LFB_PHYSICAL_ADDRESS,
   /*Bit32u OffScreenMemOffset*/        0,
   /*Bit16u OffScreenMemSize*/          0,
/* Mandatory information for VBE 3.0 and above */
   /*Bit16u LinBytesPerScanLine*/       320,
   /*Bit8u  BnkNumberOfPages*/          0,
   /*Bit8u  LinNumberOfPages*/          0,
   /*Bit8u  LinRedMaskSize*/            0,
   /*Bit8u  LinRedFieldPosition*/       0,
   /*Bit8u  LinGreenMaskSize*/          0,
   /*Bit8u  LinGreenFieldPosition*/     0,
   /*Bit8u  LinBlueMaskSize*/           0,
   /*Bit8u  LinBlueFieldPosition*/      0,
   /*Bit8u  LinRsvdMaskSize*/           0,
   /*Bit8u  LinRsvdFieldPosition*/      0,
   /*Bit32u MaxPixelClock*/             0,
/*} ModeInfoBlock;*/
                }
        },
};

#define MODE_INFO_SIZE ( sizeof(mode_info_list) / sizeof(ModeInfoListItem) )

#endif /* VBE_NEW_DYN_LIST */
