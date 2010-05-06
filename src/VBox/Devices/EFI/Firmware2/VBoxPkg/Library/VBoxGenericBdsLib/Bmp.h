/* $Id$ */
/** @file
 * Bmp.h - This file defines BMP file header data structures.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*

This code is based on:

Copyright (c) 2006 - 2008, Intel Corporation. <BR>
All rights reserved. This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

*/

#ifndef _GENERIC_BDS_BMP_H__
#define _GENERIC_BDS_BMP_H__

#pragma pack(1)

typedef struct {
  UINT8   Blue;
  UINT8   Green;
  UINT8   Red;
  UINT8   Reserved;
} BMP_COLOR_MAP;

typedef struct {
  CHAR8         CharB;
  CHAR8         CharM;
  UINT32        Size;
  UINT16        Reserved[2];
  UINT32        ImageOffset;
  UINT32        HeaderSize;
  UINT32        PixelWidth;
  UINT32        PixelHeight;
  UINT16        Planes;          ///< Must be 1
  UINT16        BitPerPixel;     ///< 1, 4, 8, or 24
  UINT32        CompressionType;
  UINT32        ImageSize;       ///< Compressed image size in bytes
  UINT32        XPixelsPerMeter;
  UINT32        YPixelsPerMeter;
  UINT32        NumberOfColors;
  UINT32        ImportantColors;
} BMP_IMAGE_HEADER;

#pragma pack()

#endif
