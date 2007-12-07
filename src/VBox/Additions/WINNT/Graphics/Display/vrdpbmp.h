/** @file
 *
 * VBoxGuest -- VirtualBox Win 2000/XP guest display driver
 *
 * VRDP bitmap cache.
 *
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __DISPLAY_VRDPBMP__H
#define __DISPLAY_VRDPBMP__H

/* RDP cache holds about 350 tiles 64x64. Therefore
 * the driver does not have to cache more then the
 * RDP capacity. Most of bitmaps will be tiled, so
 * number of RDP tiles will be greater than number of
 * bitmaps. Also the number of bitmaps must be a power
 * of 2. So the 256 is a good number.
 */
#define VRDPBMP_N_CACHED_BITMAPS  (256)

#define VRDPBMP_RC_NOT_CACHED     (0x0000)
#define VRDPBMP_RC_CACHED         (0x0001)
#define VRDPBMP_RC_ALREADY_CACHED (0x0002)

#define VRDPBMP_RC_F_DELETED      (0x10000)

/* Bitmap hash. */
#pragma pack (1)
typedef struct _VRDPBCHASH
{
    /* A 64 bit hash value of pixels. */
    uint64_t hash64;

    /* Bitmap width. */
    uint16_t cx;

    /* Bitmap height. */
    uint16_t cy;

    /* Bytes per pixel at the bitmap. */
    uint8_t bytesPerPixel;

    /* Padding to 16 bytes. */
    uint8_t padding[3];
} VRDPBCHASH;
#pragma pack ()

typedef struct _VRDPBCENTRY
{
    bool fUsed;
    struct _VRDPBCENTRY *next;
    struct _VRDPBCENTRY *prev;
    VRDPBCHASH hash;
} VRDPBCENTRY;

typedef struct _VRDPBC
{
    VRDPBCENTRY *head;
    VRDPBCENTRY *tail;
    VRDPBCENTRY aEntries[VRDPBMP_N_CACHED_BITMAPS];
} VRDPBC;

void vrdpbmpReset (VRDPBC *pCache);

int vrdpbmpCacheSurface (VRDPBC *pCache, const SURFOBJ *pso, VRDPBCHASH *phash, VRDPBCHASH *phashDeleted);

#endif /* __DISPLAY_VRDPBMP__H */
