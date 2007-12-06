/** @file
 *
 * VBoxGuest -- VirtualBox Win 2000/XP guest display driver
 *
 * VRDP bitmap cache.
 *
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
 */

#include "driver.h"
#include "vrdpbmp.h"
#include <iprt/crc64.h>
#include <VBox/VRDPOrders.h>

/*
 * Cache has a fixed number of preallocated entries. Entries are linked in the MRU
 * list. The list contains both used and free entries. Free entries are at the end.
 * The most recently used entry is in the head.
 *
 * The purpose of the cache is to answer whether the bitmap was already encountered
 * before.
 *
 * No serialization because the code is executed under vboxHwBuffer* semaphore.
 */

static uint64_t surfHash (const SURFOBJ *pso, uint32_t cbLine)
{
    uint64_t u64CRC = RTCrc64Start ();

    uint32_t h   = pso->sizlBitmap.cy;
    uint8_t *pu8 = (uint8_t *)pso->pvScan0;

    while (h > 0)
    {
        u64CRC = RTCrc64Process (u64CRC, pu8, cbLine);
        pu8 += pso->lDelta;
        h--;
    }

    u64CRC = RTCrc64Finish (u64CRC);

    return u64CRC;
} /* Hash function end. */


static BOOL bcComputeHash (const SURFOBJ *pso, VRDPBCHASH *phash)
{
    uint32_t cbLine;

    int bytesPerPixel = format2BytesPerPixel (pso);

    if (bytesPerPixel == 0)
    {
        return FALSE;
    }

    phash->cx            = (uint16_t)pso->sizlBitmap.cx;
    phash->cy            = (uint16_t)pso->sizlBitmap.cy;
    phash->bytesPerPixel = bytesPerPixel;

    cbLine               = pso->sizlBitmap.cx * bytesPerPixel;
    phash->hash64        = surfHash (pso, cbLine);

    memset (phash->padding, 0, sizeof (phash->padding));

    return TRUE;
}

/* Meves an entry to the head of MRU list. */
static void bcMoveToHead (VRDPBC *pCache, VRDPBCENTRY *pEntry)
{
    if (pEntry->prev)
    {
        /* The entry is not yet in the head. Exclude from list. */
        pEntry->prev->next = pEntry->next;

        if (pEntry->next)
        {
            pEntry->next->prev = pEntry->prev;
        }
        else
        {
            pCache->tail = pEntry->prev;
        }

        /* Insert the entry at the head of MRU list. */
        pEntry->prev = NULL;
        pEntry->next = pCache->head;

        VBVA_ASSERT(pCache->head);

        pCache->head->prev = pEntry;
        pCache->head = pEntry;
    }
}

/* Returns TRUE if the hash already presents in the cache.
 * Moves the found entry to the head of MRU list.
 */
static BOOL bcFindHash (VRDPBC *pCache, const VRDPBCHASH *phash)
{
    /* Search the MRU list. */
    VRDPBCENTRY *pEntry = pCache->head;

    while (pEntry && pEntry->fUsed)
    {
        if (memcmp (&pEntry->hash, phash, sizeof (VRDPBCHASH)) == 0)
        {
            /* Found the entry. Move it to the head of MRU list. */
            bcMoveToHead (pCache, pEntry);

            return TRUE;
        }

        pEntry = pEntry->next;
    }

    return FALSE;
}

/* Returns TRUE is a entry was also deleted to nake room for new entry. */
static BOOL bcInsertHash (VRDPBC *pCache, const VRDPBCHASH *phash, VRDPBCHASH *phashDeleted)
{
    BOOL bRc = FALSE;
    VRDPBCENTRY *pEntry;

    DISPDBG((1, "insert hash cache %p, tail %p.\n", pCache, pCache->tail));

    /* Get the free entry to be used. Try tail, that should be */
    pEntry = pCache->tail;
    
    if (pEntry == NULL)
    {
        return bRc;
    }

    if (pEntry->fUsed)
    {
        /* The cache is full. Remove the tail. */
        memcpy (phashDeleted, &pEntry->hash, sizeof (VRDPBCHASH));
        bRc = TRUE;
    }

    bcMoveToHead (pCache, pEntry);

    memcpy (&pEntry->hash, phash, sizeof (VRDPBCHASH));
    pEntry->fUsed = TRUE;

    return bRc;
}

/*
 * Public functions.
 */

/* Find out whether the surface already in the cache.
 * Insert in the cache if not.
 */
int vrdpbmpCacheSurface (VRDPBC *pCache, const SURFOBJ *pso, VRDPBCHASH *phash, VRDPBCHASH *phashDeleted)
{
    int rc;

    VRDPBCHASH hash;

    BOOL bResult = bcComputeHash (pso, &hash);

    DISPDBG((1, "vrdpbmpCacheSurface: compute hash %d.\n", bResult));
    if (!bResult)
    {
        DISPDBG((1, "MEMBLT: vrdpbmpCacheSurface: could not compute hash.\n"));
        return VRDPBMP_RC_NOT_CACHED;
    }

    bResult = bcFindHash (pCache, &hash);

    DISPDBG((1, "vrdpbmpCacheSurface: find hash %d.\n", bResult));
    *phash = hash;

    if (bResult)
    {
        return VRDPBMP_RC_ALREADY_CACHED;
    }

    rc = VRDPBMP_RC_CACHED;

    bResult = bcInsertHash (pCache, &hash, phashDeleted);

    DISPDBG((1, "vrdpbmpCacheSurface: insert hash %d.\n", bResult));
    if (bResult)
    {
        rc |= VRDPBMP_RC_F_DELETED;
    }

    return rc;
}

/* Setup the initial state of the cache. */
void vrdpbmpReset (VRDPBC *pCache)
{
    int i;

    VBVA_ASSERT(sizeof (VRDPBCHASH) == sizeof (VRDPBITMAPHASH));

    /* Reinitialize the cache structure. */
    memset (pCache, 0, sizeof (VRDPBC));

    pCache->head = &pCache->aEntries[0];
    pCache->tail = &pCache->aEntries[ELEMENTS(pCache->aEntries) - 1];

    for (i = 0; i < ELEMENTS(pCache->aEntries); i++)
    {
        VRDPBCENTRY *pEntry = &pCache->aEntries[i];

        if (pEntry != pCache->tail)
        {
            pEntry->next = &pCache->aEntries[i + 1];
        }

        if (pEntry != pCache->head)
        {
            pEntry->prev = &pCache->aEntries[i - 1];
        }
    }
}
