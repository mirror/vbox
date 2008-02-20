/** @file
 * VBox Remote Desktop Protocol:
 * VRDP orders structures.
 */

/*
 * Copyright (C) 2006-2008 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_vrdporders_h
#define ___VBox_vrdporders_h

/*
 * The VRDP server gets an information about a graphical update as a pointer
 * to a memory block and the size of the memory block.
 * The memory block layout is:
 *    VRDPORDERHDR - Describes the affected rectangle.
 *    Then VRDP orders follow:
 *        VRDPORDERCODE
 *        A VRDPORDER* structure.
 *
 * If size of the memory block is equal to the VRDPORDERHDR, then a bitmap
 * update is assumed.
 */

/* 128 bit bitmap hash. */
typedef uint8_t VRDPBITMAPHASH[16];

#pragma pack(1)
typedef struct _VRDPORDERHDR
{
   /** Coordinates of the affected rectangle. */
   int16_t x;
   int16_t y;
   uint16_t w;
   uint16_t h;
} VRDPORDERHDR;

typedef struct _VRDPORDERCODE
{
   uint32_t u32Code;
} VRDPORDERCODE;

/* VRDP order codes. Must be >= 0, because the VRDP server internally
 * uses negative values to mark some operations.
 */
#define VRDP_ORDER_DIRTY_RECT     (0)
#define VRDP_ORDER_SOLIDRECT      (1)
#define VRDP_ORDER_SOLIDBLT       (2)
#define VRDP_ORDER_DSTBLT         (3)
#define VRDP_ORDER_SCREENBLT      (4)
#define VRDP_ORDER_PATBLTBRUSH    (5)
#define VRDP_ORDER_MEMBLT         (6)
#define VRDP_ORDER_CACHED_BITMAP  (7)
#define VRDP_ORDER_DELETED_BITMAP (8)
#define VRDP_ORDER_LINE           (9)
#define VRDP_ORDER_BOUNDS         (10)
#define VRDP_ORDER_REPEAT         (11)
#define VRDP_ORDER_POLYLINE       (12)
#define VRDP_ORDER_ELLIPSE        (13)
#define VRDP_ORDER_SAVESCREEN     (14)
#define VRDP_ORDER_TEXT           (15)

typedef struct _VRDPORDERPOINT
{
    int16_t  x;
    int16_t  y;
} VRDPORDERPOINT;

typedef struct _VRDPORDERPOLYPOINTS
{
    uint8_t  c;
    VRDPORDERPOINT a[16];
} VRDPORDERPOLYPOINTS;

typedef struct _VRDPORDERAREA
{
    int16_t  x;
    int16_t  y;
    uint16_t w;
    uint16_t h;
} VRDPORDERAREA;

typedef struct _VRDPORDERRECT
{
    int16_t left;
    int16_t top;
    int16_t right;
    int16_t bottom;
} VRDPORDERRECT;


typedef struct _VRDPORDERBOUNDS
{
    VRDPORDERPOINT pt1;
    VRDPORDERPOINT pt2;
} VRDPORDERBOUNDS;

typedef struct _VRDPORDERREPEAT
{
    VRDPORDERBOUNDS bounds;
} VRDPORDERREPEAT;


/* Header for bitmap bits in VBVA VRDP operations. */
typedef struct _VRDPDATABITS
{
    /* Size of bitmap data without the header. */
    uint32_t cb;
    int16_t  x;
    int16_t  y;
    uint16_t cWidth;
    uint16_t cHeight;
    uint8_t cbPixel;
} VRDPDATABITS;

typedef struct _VRDPORDERSOLIDRECT
{
    int16_t  x;
    int16_t  y;
    uint16_t w;
    uint16_t h;
    uint32_t rgb;
} VRDPORDERSOLIDRECT;

typedef struct _VRDPORDERSOLIDBLT
{
    int16_t  x;
    int16_t  y;
    uint16_t w;
    uint16_t h;
    uint32_t rgb;
    uint8_t  rop;
} VRDPORDERSOLIDBLT;

typedef struct _VRDPORDERDSTBLT
{
    int16_t  x;
    int16_t  y;
    uint16_t w;
    uint16_t h;
    uint8_t  rop;
} VRDPORDERDSTBLT;

typedef struct _VRDPORDERSCREENBLT
{
    int16_t  x;
    int16_t  y;
    uint16_t w;
    uint16_t h;
    int16_t  xSrc;
    int16_t  ySrc;
    uint8_t  rop;
} VRDPORDERSCREENBLT;

typedef struct _VRDPORDERPATBLTBRUSH
{
    int16_t  x;
    int16_t  y;
    uint16_t w;
    uint16_t h;
    int8_t   xSrc;
    int8_t   ySrc;
    uint32_t rgbFG;
    uint32_t rgbBG;
    uint8_t  rop;
    uint8_t  pattern[8];
} VRDPORDERPATBLTBRUSH;

typedef struct _VRDPORDERMEMBLT
{
    int16_t  x;
    int16_t  y;
    uint16_t w;
    uint16_t h;
    int16_t  xSrc;
    int16_t  ySrc;
    uint8_t  rop;
    VRDPBITMAPHASH hash;
} VRDPORDERMEMBLT;

typedef struct _VRDPORDERCACHEDBITMAP
{
    VRDPBITMAPHASH hash;
    /* VRDPDATABITS and the bitmap data follows. */
} VRDPORDERCACHEDBITMAP;

typedef struct _VRDPORDERDELETEDBITMAP
{
    VRDPBITMAPHASH hash;
} VRDPORDERDELETEDBITMAP;

typedef struct _VRDPORDERLINE
{
    int16_t  x1;
    int16_t  y1;
    int16_t  x2;
    int16_t  y2;
    int16_t  xBounds1;
    int16_t  yBounds1;
    int16_t  xBounds2;
    int16_t  yBounds2;
    uint8_t  mix;
    uint32_t rgb;
} VRDPORDERLINE;

typedef struct _VRDPORDERPOLYLINE
{
    VRDPORDERPOINT ptStart;
    uint8_t  mix;
    uint32_t rgb;
    VRDPORDERPOLYPOINTS points;
} VRDPORDERPOLYLINE;

typedef struct _VRDPORDERELLIPSE
{
    VRDPORDERPOINT pt1;
    VRDPORDERPOINT pt2;
    uint8_t  mix;
    uint8_t  fillMode;
    uint32_t rgb;
} VRDPORDERELLIPSE;

typedef struct _VRDPORDERSAVESCREEN
{
    VRDPORDERPOINT pt1;
    VRDPORDERPOINT pt2;
    uint8_t ident;
    uint8_t restore;
} VRDPORDERSAVESCREEN;

typedef struct _VRDPORDERGLYPH
{
    uint32_t o32NextGlyph;
    uint64_t u64Handle;
    
    /* The glyph origin position on the screen. */
    int16_t  x;
    int16_t  y;
    
    /* The glyph bitmap dimensions. Note w == h == 0 for the space character. */
    uint16_t w;
    uint16_t h;

    /* The character origin in the bitmap. */
    int16_t  xOrigin;
    int16_t  yOrigin;
    
    /* 1BPP bitmap. Rows are byte aligned. Size is (((w + 7)/8) * h + 3) & ~3. */
    uint8_t au8Bitmap[1];
} VRDPORDERGLYPH;

typedef struct _VRDPORDERTEXT
{
    uint32_t cbOrder;

    int16_t  xBkGround;
    int16_t  yBkGround;
    uint16_t wBkGround;
    uint16_t hBkGround;

    int16_t  xOpaque;
    int16_t  yOpaque;
    uint16_t wOpaque;
    uint16_t hOpaque;

    uint16_t u16MaxGlyph;

    uint8_t  u8Glyphs;
    uint8_t  u8Flags;
    uint16_t u8CharInc;
    uint32_t u32FgRGB;
    uint32_t u32BgRGB;

    /* u8Glyphs glyphs follow. Size of each glyph structure may vary. */
} VRDPORDERTEXT;
#pragma pack()

#endif
