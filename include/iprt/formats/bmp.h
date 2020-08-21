/* $Id$ */
/** @file
 * Bitmap (BMP) format defines.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
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

#ifndef IPRT_INCLUDED_formats_bmp_h
#define IPRT_INCLUDED_formats_bmp_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>
#include <iprt/assertcompile.h>

#pragma pack(1)

/** BMP File Format Bitmap Header. */
typedef struct
{
    /** File Type Identifier. */
    uint16_t      Type;
    /** Size of File. */
    uint32_t      FileSize;
    /** Reserved (should be 0). */
    uint16_t      Reserved1;
    /** Reserved (should be 0). */
    uint16_t      Reserved2;
    /** Offset to bitmap data. */
    uint32_t      Offset;
} BMPINFO;
AssertCompileSize(BMPINFO, 14);
/** Pointer to a bitmap header. */
typedef BMPINFO *PBMPINFO;

/** OS/2 1.x Information Header Format. */
typedef struct
{
    /** Size of Remaining Header. */
    uint32_t      Size;
    /** Width of Bitmap in Pixels. */
    uint16_t      Width;
    /** Height of Bitmap in Pixels. */
    uint16_t      Height;
    /** Number of Planes. */
    uint16_t      Planes;
    /** Color Bits Per Pixel. */
    uint16_t      BitCount;
} OS2HDR;
AssertCompileSize(OS2HDR, 12);
/** Pointer to a OS/2 1.x header format. */
typedef OS2HDR *POS2HDR;

/** OS/2 2.0 Information Header Format. */
typedef struct
{
    /** Size of Remaining Header. */
    uint32_t      Size;
    /** Width of Bitmap in Pixels. */
    uint32_t      Width;
    /** Height of Bitmap in Pixels. */
    uint32_t      Height;
    /** Number of Planes. */
    uint16_t      Planes;
    /** Color Bits Per Pixel. */
    uint16_t      BitCount;
    /** Compression Scheme (0=none). */
    uint32_t      Compression;
    /** Size of bitmap in bytes. */
    uint32_t      SizeImage;
    /** Horz. Resolution in Pixels/Meter. */
    uint32_t      XPelsPerMeter;
    /** Vert. Resolution in Pixels/Meter. */
    uint32_t      YPelsPerMeter;
    /** Number of Colors in Color Table. */
    uint32_t      ClrUsed;
    /** Number of Important Colors. */
    uint32_t      ClrImportant;
    /** Resolution Measurement Used. */
    uint16_t      Units;
    /** Reserved Fields (always 0). */
    uint16_t      Reserved;
    /** Orientation of Bitmap. */
    uint16_t      Recording;
    /** Halftone Algorithm Used on Image. */
    uint16_t      Rendering;
    /** Halftone Algorithm Data. */
    uint32_t      Size1;
    /** Halftone Algorithm Data. */
    uint32_t      Size2;
    /** Color Table Format (always 0). */
    uint32_t      ColorEncoding;
    /** Misc. Field for Application Use  . */
    uint32_t      Identifier;
} OS22HDR;
AssertCompileSize(OS22HDR, 64);
/** Pointer to a OS/2 2.0 header format . */
typedef OS22HDR *POS22HDR;

/** Windows 3.x Information Header Format. */
typedef struct
{
    /** Size of Remaining Header. */
    uint32_t      Size;
    /** Width of Bitmap in Pixels. */
    uint32_t      Width;
    /** Height of Bitmap in Pixels. */
    uint32_t      Height;
    /** Number of Planes. */
    uint16_t      Planes;
    /** Bits Per Pixel. */
    uint16_t      BitCount;
    /** Compression Scheme (0=none). */
    uint32_t      Compression;
    /** Size of bitmap in bytes. */
    uint32_t      SizeImage;
    /** Horz. Resolution in Pixels/Meter. */
    uint32_t      XPelsPerMeter;
    /** Vert. Resolution in Pixels/Meter. */
    uint32_t      YPelsPerMeter;
    /** Number of Colors in Color Table. */
    uint32_t      ClrUsed;
    /** Number of Important Colors. */
    uint32_t      ClrImportant;
} WINHDR;
/** Pointer to a Windows 3.x header format. */
typedef WINHDR *PWINHDR;

#pragma pack()

/** BMP file magic number. */
#define BMP_HDR_MAGIC (RT_H2LE_U16_C(0x4d42))

/** @name BMP compressions.
 * @{ . */
#define BMP_COMPRESS_NONE    0
#define BMP_COMPRESS_RLE8    1
#define BMP_COMPRESS_RLE4    2
/** @} . */

/** @name BMP header sizes.
 * @{ . */
#define BMP_HEADER_OS21      12
#define BMP_HEADER_OS22      64
#define BMP_HEADER_WIN3      40
/** @} . */

#endif /* !IPRT_INCLUDED_formats_bmp_h */

