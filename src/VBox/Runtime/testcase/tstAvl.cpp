/* $Id$ */
/** @file
 * IPRT Testcase - Avl trees.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/avl.h>
#include <iprt/asm.h>
#include <iprt/stdarg.h>
#include <iprt/string.h>

#include <stdio.h>
#include <stdlib.h> /* rand */


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct TRACKER
{
    /** The max key value (exclusive). */
    uint32_t    MaxKey;
    /** The last allocated key. */
    uint32_t    LastAllocatedKey;
    /** The number of set bits in the bitmap. */
    uint32_t    cSetBits;
    /** The bitmap size. */
    uint32_t    cbBitmap;
    /** Bitmap containing the allocated nodes. */
    uint8_t     abBitmap[1];
} TRACKER, *PTRACKER;


/**
 * Gets a random number between 0 and Max.
 *
 * @return random number.
 * @param   Max     The max number. (exclusive)
 */
uint32_t Random(uint32_t Max)
{
    unsigned rc = rand();
    if (Max < RAND_MAX)
    {
        while (rc >= Max)
            rc /= 3;
    }
    else
    {
        /// @todo...
    }
    return rc;
}


/**
 * Creates a new tracker.
 *
 * @returns Pointer to the new tracker.
 * @param   MaxKey      The max key value for the tracker. (exclusive)
 */
PTRACKER TrackerCreate(uint32_t MaxKey)
{
    uint32_t cbBitmap = (MaxKey + sizeof(uint32_t) * sizeof(uint8_t) - 1) / sizeof(uint8_t);
    PTRACKER pTracker = (PTRACKER)calloc(1, RT_OFFSETOF(TRACKER, abBitmap[cbBitmap]));
    if (pTracker)
    {
        pTracker->MaxKey = MaxKey;
        pTracker->LastAllocatedKey = MaxKey;
        pTracker->cbBitmap = cbBitmap;
    }
    return pTracker;
}


/**
 * Destroys a tracker.
 *
 * @param   pTracker        The tracker.
 */
void TrackerDestroy(PTRACKER pTracker)
{
    if (pTracker)
        free(pTracker);
}


/**
 * Inserts a key range into the tracker.
 *
 * @returns success indicator.
 * @param   pTracker    The tracker.
 * @param   Key         The first key in the range.
 * @param   KeyLast     The last key in the range. (inclusive)
 */
bool TrackerInsert(PTRACKER pTracker, uint32_t Key, uint32_t KeyLast)
{
    bool fRc = !ASMBitTestAndSet(pTracker->abBitmap, Key);
    if (fRc)
        pTracker->cSetBits++;
    while (KeyLast != Key)
    {
        if (!ASMBitTestAndSet(pTracker->abBitmap, KeyLast))
            pTracker->cSetBits++;
        else
            fRc = false;
        KeyLast--;
    }
    return fRc;
}


/**
 * Removes a key range from the tracker.
 *
 * @returns success indicator.
 * @param   pTracker    The tracker.
 * @param   Key         The first key in the range.
 * @param   KeyLast     The last key in the range. (inclusive)
 */
bool TrackerRemove(PTRACKER pTracker, uint32_t Key, uint32_t KeyLast)
{
    bool fRc = ASMBitTestAndClear(pTracker->abBitmap, Key);
    if (fRc)
        pTracker->cSetBits--;
    while (KeyLast != Key)
    {
        if (ASMBitTestAndClear(pTracker->abBitmap, KeyLast))
            pTracker->cSetBits--;
        else
            fRc = false;
        KeyLast--;
    }
    return fRc;
}


/**
 * Random key range allocation.
 *
 * @returns success indicator.
 * @param   pTracker    The tracker.
 * @param   pKey        Where to store the first key in the allocated range.
 * @param   pKeyLast    Where to store the first key in the allocated range.
 * @param   cMaxKey     The max range length.
 * @remark  The caller has to call TrackerInsert.
 */
bool TrackerNewRandomEx(PTRACKER pTracker, uint32_t *pKey, uint32_t *pKeyLast, uint32_t cMaxKeys)
{
    /*
     * Find a key.
     */
    uint32_t Key = Random(pTracker->MaxKey);
    if (ASMBitTest(pTracker->abBitmap, Key))
    {
        if (pTracker->cSetBits >= pTracker->MaxKey)
            return false;

        int Key2 = ASMBitNextClear(pTracker->abBitmap, pTracker->MaxKey, Key);
        if (Key2 > 0)
            Key = Key2;
        else
        {
            /* we're missing a ASMBitPrevClear function, so just try another, lower, value.*/
            for (;;)
            {
                const uint32_t KeyPrev = Key;
                Key = Random(KeyPrev);
                if (!ASMBitTest(pTracker->abBitmap, Key))
                    break;
                Key2 = ASMBitNextClear(pTracker->abBitmap, RT_ALIGN_32(KeyPrev, 32), Key);
                if (Key2 > 0)
                {
                    Key = Key2;
                    break;
                }
            }
        }
    }

    /*
     * Determin the range.
     */
    uint32_t KeyLast;
    if (cMaxKeys == 1 || !pKeyLast)
        KeyLast = Key;
    else
    {
        uint32_t cKeys = Random(RT_MIN(pTracker->MaxKey - Key, cMaxKeys));
        KeyLast = Key + cKeys;
        int Key2 = ASMBitNextSet(pTracker->abBitmap, RT_ALIGN_32(KeyLast, 32), Key);
        if (    Key2 > 0
            &&  (unsigned)Key2 <= KeyLast)
            KeyLast = Key2 - 1;
    }

    /*
     * Return.
     */
    *pKey = Key;
    if (pKeyLast)
        *pKeyLast = KeyLast;
    return true;
}


/**
 * Random single key allocation.
 *
 * @returns success indicator.
 * @param   pTracker    The tracker.
 * @param   pKey        Where to store the allocated key.
 * @remark  The caller has to call TrackerInsert.
 */
bool TrackerNewRandom(PTRACKER pTracker, uint32_t *pKey)
{
    return TrackerNewRandomEx(pTracker, pKey, NULL, 1);
}


/**
 * Random single key 'lookup'.
 *
 * @returns success indicator.
 * @param   pTracker    The tracker.
 * @param   pKey        Where to store the allocated key.
 * @remark  The caller has to call TrackerRemove.
 */
bool TrackerFindRandom(PTRACKER pTracker, uint32_t *pKey)
{
    uint32_t Key = Random(pTracker->MaxKey);
    if (!ASMBitTest(pTracker->abBitmap, Key))
    {
        if (!pTracker->cSetBits)
            return false;

        int Key2 = ASMBitNextSet(pTracker->abBitmap, pTracker->MaxKey, Key);
        if (Key2 > 0)
            Key = Key2;
        else
        {
            /* we're missing a ASMBitPrevSet function, so just try another, lower, value.*/
            for (;;)
            {
                const uint32_t KeyPrev = Key;
                Key = Random(KeyPrev);
                if (ASMBitTest(pTracker->abBitmap, Key))
                    break;
                Key2 = ASMBitNextSet(pTracker->abBitmap, RT_ALIGN_32(KeyPrev, 32), Key);
                if (Key2 > 0)
                {
                    Key = Key2;
                    break;
                }
            }
        }
    }

    *pKey = Key;
    return true;
}


/*
bool TrackerAllocSeq(PTRACKER pTracker, uint32_t *pKey, uint32_t *pKeyLast, uint32_t cMaxKeys)
{
    return false;
}*/


/**
 * Prints an unbuffered char.
 * @param   ch  The char.
 */
void ProgressChar(char ch)
{
    fputc(ch, stdout);
    fflush(stdout);
}

/**
 * Prints a progress indicator label.
 * @param   cMax        The max number of operations (exclusive).
 * @param   pszFormat   The format string.
 * @param   ...         The arguments to the format string.
 */
DECLINLINE(void) ProgressPrintf(unsigned cMax, const char *pszFormat, ...)
{
    if (cMax < 10000)
        return;
    va_list va;
    va_start(va, pszFormat);
    vfprintf(stdout, pszFormat, va);
    va_end(va);
}


/**
 * Prints a progress indicator dot.
 * @param   iCur    The current operation. (can be decending too)
 * @param   cMax    The max number of operations (exclusive).
 */
DECLINLINE(void) Progress(unsigned iCur, unsigned cMax)
{
    if (cMax < 10000)
        return;
    if (!(iCur % (cMax / 20)))
        ProgressChar('.');
}


int avlogcphys(unsigned cMax)
{
    /*
     * Simple linear insert and remove.
     */
    ProgressPrintf(cMax, "tstAVL oGCPhys(%d): linear left", cMax);
    PAVLOGCPHYSTREE pTree = (PAVLOGCPHYSTREE)calloc(sizeof(*pTree),1);
    unsigned i;
    for (i = 0; i < cMax; i++)
    {
        Progress(i, cMax);
        PAVLOGCPHYSNODECORE pNode = (PAVLOGCPHYSNODECORE)malloc(sizeof(*pNode));
        pNode->Key = i;
        if (!RTAvloGCPhysInsert(pTree, pNode))
        {
            printf("\ntstAvl: FAILURE - oGCPhys - linear left insert i=%d\n", i);
            return 1;
        }
        /* negative. */
        AVLOGCPHYSNODECORE Node = *pNode;
        if (RTAvloGCPhysInsert(pTree, &Node))
        {
            printf("\ntstAvl: FAILURE - oGCPhys - linear left negative insert i=%d\n", i);
            return 1;
        }
    }

    ProgressPrintf(cMax, "~");
    for (i = 0; i < cMax; i++)
    {
        Progress(i, cMax);
        PAVLOGCPHYSNODECORE pNode = RTAvloGCPhysRemove(pTree, i);
        if (!pNode)
        {
            printf("\ntstAvl: FAILURE - oGCPhys - linear left remove i=%d\n", i);
            return 1;
        }
        memset(pNode, 0xcc, sizeof(*pNode));
        free(pNode);

        /* negative */
        pNode = RTAvloGCPhysRemove(pTree, i);
        if (pNode)
        {
            printf("\ntstAvl: FAILURE - oGCPhys - linear left negative remove i=%d\n", i);
            return 1;
        }
    }

    /*
     * Simple linear insert and remove from the right.
     */
    ProgressPrintf(cMax, "\ntstAVL oGCPhys(%d): linear right", cMax);
    for (i = 0; i < cMax; i++)
    {
        Progress(i, cMax);
        PAVLOGCPHYSNODECORE pNode = (PAVLOGCPHYSNODECORE)malloc(sizeof(*pNode));
        pNode->Key = i;
        if (!RTAvloGCPhysInsert(pTree, pNode))
        {
            printf("\ntstAvl: FAILURE - oGCPhys - linear right insert i=%d\n", i);
            return 1;
        }
        /* negative. */
        AVLOGCPHYSNODECORE Node = *pNode;
        if (RTAvloGCPhysInsert(pTree, &Node))
        {
            printf("\ntstAvl: FAILURE - oGCPhys - linear right negative insert i=%d\n", i);
            return 1;
        }
    }

    ProgressPrintf(cMax, "~");
    while (i-- > 0)
    {
        Progress(i, cMax);
        PAVLOGCPHYSNODECORE pNode = RTAvloGCPhysRemove(pTree, i);
        if (!pNode)
        {
            printf("\ntstAvl: FAILURE - oGCPhys - linear right remove i=%d\n", i);
            return 1;
        }
        memset(pNode, 0xcc, sizeof(*pNode));
        free(pNode);

        /* negative */
        pNode = RTAvloGCPhysRemove(pTree, i);
        if (pNode)
        {
            printf("\ntstAvl: FAILURE - oGCPhys - linear right negative remove i=%d\n", i);
            return 1;
        }
    }

    /*
     * Linear insert but root based removal.
     */
    ProgressPrintf(cMax, "\ntstAVL oGCPhys(%d): linear root", cMax);
    for (i = 0; i < cMax; i++)
    {
        Progress(i, cMax);
        PAVLOGCPHYSNODECORE pNode = (PAVLOGCPHYSNODECORE)malloc(sizeof(*pNode));
        pNode->Key = i;
        if (!RTAvloGCPhysInsert(pTree, pNode))
        {
            printf("\ntstAvl: FAILURE - oGCPhys - linear root insert i=%d\n", i);
            return 1;
        }
        /* negative. */
        AVLOGCPHYSNODECORE Node = *pNode;
        if (RTAvloGCPhysInsert(pTree, &Node))
        {
            printf("\ntstAvl: FAILURE - oGCPhys - linear root negative insert i=%d\n", i);
            return 1;
        }
    }

    ProgressPrintf(cMax, "~");
    while (i-- > 0)
    {
        Progress(i, cMax);
        PAVLOGCPHYSNODECORE pNode = (PAVLOGCPHYSNODECORE)((intptr_t)pTree + *pTree);
        RTGCPHYS Key = pNode->Key;
        pNode = RTAvloGCPhysRemove(pTree, Key);
        if (!pNode)
        {
            printf("\ntstAvl: FAILURE - oGCPhys - linear root remove i=%d Key=%d\n", i, (unsigned)Key);
            return 1;
        }
        memset(pNode, 0xcc, sizeof(*pNode));
        free(pNode);

        /* negative */
        pNode = RTAvloGCPhysRemove(pTree, Key);
        if (pNode)
        {
            printf("\ntstAvl: FAILURE - oGCPhys - linear root negative remove i=%d Key=%d\n", i, (unsigned)Key);
            return 1;
        }
    }
    if (*pTree)
    {
        printf("\ntstAvl: FAILURE - oGCPhys - sparse remove didn't remove it all!\n");
        return 1;
    }

    /*
     * Make a sparsely populated tree and remove the nodes using best fit in 5 cycles.
     */
    const unsigned cMaxSparse = RT_ALIGN(cMax, 32);
    ProgressPrintf(cMaxSparse, "\ntstAVL oGCPhys(%d): sparse", cMax);
    for (i = 0; i < cMaxSparse; i += 8)
    {
        Progress(i, cMaxSparse);
        PAVLOGCPHYSNODECORE pNode = (PAVLOGCPHYSNODECORE)malloc(sizeof(*pNode));
        pNode->Key = i;
        if (!RTAvloGCPhysInsert(pTree, pNode))
        {
            printf("\ntstAvl: FAILURE - oGCPhys - sparse insert i=%d\n", i);
            return 1;
        }
        /* negative. */
        AVLOGCPHYSNODECORE Node = *pNode;
        if (RTAvloGCPhysInsert(pTree, &Node))
        {
            printf("\ntstAvl: FAILURE - oGCPhys - sparse negative insert i=%d\n", i);
            return 1;
        }
    }

    /* Remove using best fit in 5 cycles. */
    ProgressPrintf(cMaxSparse, "~");
    unsigned j;
    for (j = 0; j < 4; j++)
    {
        for (i = 0; i < cMaxSparse; i += 8 * 4)
        {
            Progress(i, cMax); // good enough
            PAVLOGCPHYSNODECORE pNode = RTAvloGCPhysRemoveBestFit(pTree, i, true);
            if (!pNode)
            {
                printf("\ntstAvl: FAILURE - oGCPhys - sparse remove i=%d j=%d\n", i, j);
                return 1;
            }
            if (pNode->Key - (unsigned long)i >= 8 * 4)
            {
                printf("\ntstAvl: FAILURE - oGCPhys - sparse remove i=%d j=%d Key=%d\n", i, j, (unsigned)pNode->Key);
                return 1;
            }
            memset(pNode, 0xdd, sizeof(*pNode));
            free(pNode);
        }
    }
    if (*pTree)
    {
        printf("\ntstAvl: FAILURE - oGCPhys - sparse remove didn't remove it all!\n");
        return 1;
    }
    free(pTree);
    ProgressPrintf(cMaxSparse, "\n");
    return 0;
}


int avlogcphysRand(unsigned cMax, unsigned cMax2)
{
    PAVLOGCPHYSTREE pTree = (PAVLOGCPHYSTREE)calloc(sizeof(*pTree),1);
    unsigned i;

    /*
     * Random tree.
     */
    ProgressPrintf(cMax, "tstAVL oGCPhys(%d, %d): random", cMax, cMax2);
    PTRACKER pTracker = TrackerCreate(cMax2);
    if (!pTracker)
    {
        printf("tstAVL: Failure - oGCPhys - failed to create %d tracker!\n", cMax2);
        return 1;
    }

    /* Insert a number of nodes in random order. */
    for (i = 0; i < cMax; i++)
    {
        Progress(i, cMax);
        uint32_t Key;
        if (!TrackerNewRandom(pTracker, &Key))
        {
            printf("\ntstAVL: Failure - oGCPhys - failed to allocate node no. %d\n", i);
            TrackerDestroy(pTracker);
            return 1;
        }
        PAVLOGCPHYSNODECORE pNode = (PAVLOGCPHYSNODECORE)malloc(sizeof(*pNode));
        pNode->Key = Key;
        if (!RTAvloGCPhysInsert(pTree, pNode))
        {
            printf("\ntstAvl: FAILURE - oGCPhys - random insert i=%d Key=%#x\n", i, Key);
            return 1;
        }
        /* negative. */
        AVLOGCPHYSNODECORE Node = *pNode;
        if (RTAvloGCPhysInsert(pTree, &Node))
        {
            printf("\ntstAvl: FAILURE - oGCPhys - linear negative insert i=%d Key=%#x\n", i, Key);
            return 1;
        }
        TrackerInsert(pTracker, Key, Key);
    }


    /* delete the nodes in random order. */
    ProgressPrintf(cMax, "~");
    while (i-- > 0)
    {
        Progress(i, cMax);
        uint32_t Key;
        if (!TrackerFindRandom(pTracker, &Key))
        {
            printf("\ntstAVL: Failure - oGCPhys - failed to find free node no. %d\n", i);
            TrackerDestroy(pTracker);
            return 1;
        }

        PAVLOGCPHYSNODECORE pNode = RTAvloGCPhysRemove(pTree, Key);
        if (!pNode)
        {
            printf("\ntstAvl: FAILURE - oGCPhys - random remove i=%d Key=%#x\n", i, Key);
            return 1;
        }
        if (pNode->Key != Key)
        {
            printf("\ntstAvl: FAILURE - oGCPhys - random remove i=%d Key=%#x pNode->Key=%#x\n", i, Key, (unsigned)pNode->Key);
            return 1;
        }
        TrackerRemove(pTracker, Key, Key);
        memset(pNode, 0xdd, sizeof(*pNode));
        free(pNode);
    }
    if (*pTree)
    {
        printf("\ntstAvl: FAILURE - oGCPhys - random remove didn't remove it all!\n");
        return 1;
    }
    ProgressPrintf(cMax, "\n");
    TrackerDestroy(pTracker);
    free(pTree);
    return 0;
}



int avlrogcphys(void)
{
    unsigned            i;
    unsigned            j;
    unsigned            k;
    PAVLROGCPHYSTREE    pTree = (PAVLROGCPHYSTREE)calloc(sizeof(*pTree), 1);

    AssertCompileSize(AVLOGCPHYSNODECORE, 24);
    AssertCompileSize(AVLROGCPHYSNODECORE, 32);

    /*
     * Simple linear insert, get and remove.
     */
    /* insert */
    for (i = 0; i < 65536; i += 4)
    {
        PAVLROGCPHYSNODECORE pNode = (PAVLROGCPHYSNODECORE)malloc(sizeof(*pNode));
        pNode->Key = i;
        pNode->KeyLast = i + 3;
        if (!RTAvlroGCPhysInsert(pTree, pNode))
        {
            printf("tstAvl: FAILURE - roGCPhys - linear insert i=%d\n", (unsigned)i);
            return 1;
        }

        /* negative. */
        AVLROGCPHYSNODECORE Node = *pNode;
        for (j = i + 3; j > i - 32; j--)
        {
            for (k = i; k < i + 32; k++)
            {
                Node.Key = RT_MIN(j, k);
                Node.KeyLast = RT_MAX(k, j);
                if (RTAvlroGCPhysInsert(pTree, &Node))
                {
                    printf("tstAvl: FAILURE - roGCPhys - linear negative insert i=%d j=%d k=%d\n", i, j, k);
                    return 1;
                }
            }
        }
    }

    /* do gets. */
    for (i = 0; i < 65536; i += 4)
    {
        PAVLROGCPHYSNODECORE pNode = RTAvlroGCPhysGet(pTree, i);
        if (!pNode)
        {
            printf("tstAvl: FAILURE - roGCPhys - linear get i=%d\n", i);
            return 1;
        }
        if (pNode->Key > i || pNode->KeyLast < i)
        {
            printf("tstAvl: FAILURE - roGCPhys - linear get i=%d Key=%d KeyLast=%d\n", i, (unsigned)pNode->Key, (unsigned)pNode->KeyLast);
            return 1;
        }

        for (j = 0; j < 4; j++)
        {
            if (RTAvlroGCPhysRangeGet(pTree, i + j) != pNode)
            {
                printf("tstAvl: FAILURE - roGCPhys - linear range get i=%d j=%d\n", i, j);
                return 1;
            }
        }

        /* negative. */
        if (    RTAvlroGCPhysGet(pTree, i + 1)
            ||  RTAvlroGCPhysGet(pTree, i + 2)
            ||  RTAvlroGCPhysGet(pTree, i + 3))
        {
            printf("tstAvl: FAILURE - roGCPhys - linear negative get i=%d + n\n", i);
            return 1;
        }

    }

    /* remove */
    for (i = 0; i < 65536; i += 4)
    {
        PAVLROGCPHYSNODECORE pNode = RTAvlroGCPhysRemove(pTree, i);
        if (!pNode)
        {
            printf("tstAvl: FAILURE - roGCPhys - linear remove i=%d\n", i);
            return 1;
        }
        memset(pNode, 0xcc, sizeof(*pNode));
        free(pNode);

        /* negative */
        if (    RTAvlroGCPhysRemove(pTree, i)
            ||  RTAvlroGCPhysRemove(pTree, i + 1)
            ||  RTAvlroGCPhysRemove(pTree, i + 2)
            ||  RTAvlroGCPhysRemove(pTree, i + 3))
        {
            printf("tstAvl: FAILURE - roGCPhys - linear negative remove i=%d + n\n", i);
            return 1;
        }
    }

    /*
     * Make a sparsely populated tree.
     */
    for (i = 0; i < 65536; i += 8)
    {
        PAVLROGCPHYSNODECORE pNode = (PAVLROGCPHYSNODECORE)malloc(sizeof(*pNode));
        pNode->Key = i;
        pNode->KeyLast = i + 3;
        if (!RTAvlroGCPhysInsert(pTree, pNode))
        {
            printf("tstAvl: FAILURE - roGCPhys - sparse insert i=%d\n", i);
            return 1;
        }
        /* negative. */
        AVLROGCPHYSNODECORE Node = *pNode;
        const RTGCPHYS jMin = i > 32 ? i - 32 : 1;
        const RTGCPHYS kMax = i + 32;
        for (j = pNode->KeyLast; j >= jMin; j--)
        {
            for (k = pNode->Key; k < kMax; k++)
            {
                Node.Key = RT_MIN(j, k);
                Node.KeyLast = RT_MAX(k, j);
                if (RTAvlroGCPhysInsert(pTree, &Node))
                {
                    printf("tstAvl: FAILURE - roGCPhys - sparse negative insert i=%d j=%d k=%d\n", i, j, k);
                    return 1;
                }
            }
        }
    }

    /*
     * Get and Remove using range matching in 5 cycles.
     */
    for (j = 0; j < 4; j++)
    {
        for (i = 0; i < 65536; i += 8 * 4)
        {
            /* gets */
            RTGCPHYS  KeyBase = i + j * 8;
            PAVLROGCPHYSNODECORE pNode = RTAvlroGCPhysGet(pTree, KeyBase);
            if (!pNode)
            {
                printf("tstAvl: FAILURE - roGCPhys - sparse get i=%d j=%d KeyBase=%d\n", i, j, (unsigned)KeyBase);
                return 1;
            }
            if (pNode->Key > KeyBase || pNode->KeyLast < KeyBase)
            {
                printf("tstAvl: FAILURE - roGCPhys - sparse get i=%d j=%d KeyBase=%d pNode->Key=%d\n", i, j, (unsigned)KeyBase, (unsigned)pNode->Key);
                return 1;
            }
            for (k = KeyBase; k < KeyBase + 4; k++)
            {
                if (RTAvlroGCPhysRangeGet(pTree, k) != pNode)
                {
                    printf("tstAvl: FAILURE - roGCPhys - sparse range get i=%d j=%d k=%d\n", i, j, k);
                    return 1;
                }
            }

            /* negative gets */
            for (k = i + j; k < KeyBase + 8; k++)
            {
                if (    k != KeyBase
                    &&  RTAvlroGCPhysGet(pTree, k))
                {
                    printf("tstAvl: FAILURE - roGCPhys - sparse negative get i=%d j=%d k=%d\n", i, j, k);
                    return 1;
                }
            }
            for (k = i + j; k < KeyBase; k++)
            {
                if (RTAvlroGCPhysRangeGet(pTree, k))
                {
                    printf("tstAvl: FAILURE - roGCPhys - sparse negative range get i=%d j=%d k=%d\n", i, j, k);
                    return 1;
                }
            }
            for (k = KeyBase + 4; k < KeyBase + 8; k++)
            {
                if (RTAvlroGCPhysRangeGet(pTree, k))
                {
                    printf("tstAvl: FAILURE - roGCPhys - sparse negative range get i=%d j=%d k=%d\n", i, j, k);
                    return 1;
                }
            }

            /* remove */
            RTGCPHYS Key = KeyBase + ((i / 19) % 4);
            if (RTAvlroGCPhysRangeRemove(pTree, Key) != pNode)
            {
                printf("tstAvl: FAILURE - roGCPhys - sparse remove i=%d j=%d Key=%d\n", i, j, (unsigned)Key);
                return 1;
            }
            memset(pNode, 0xdd, sizeof(*pNode));
            free(pNode);
        }
    }
    if (*pTree)
    {
        printf("tstAvl: FAILURE - roGCPhys - sparse remove didn't remove it all!\n");
        return 1;
    }


    /*
     * Realworld testcase.
     */
    struct
    {
        AVLROGCPHYSTREE     Tree;
        AVLROGCPHYSNODECORE aNode[4];
    } s1 = {0}, s2 = {0}, s3 = {0};

    s1.aNode[0].Key        = 0x00030000;
    s1.aNode[0].KeyLast    = 0x00030fff;
    s1.aNode[1].Key        = 0x000a0000;
    s1.aNode[1].KeyLast    = 0x000bffff;
    s1.aNode[2].Key        = 0xe0000000;
    s1.aNode[2].KeyLast    = 0xe03fffff;
    s1.aNode[3].Key        = 0xfffe0000;
    s1.aNode[3].KeyLast    = 0xfffe0ffe;
    for (i = 0; i < RT_ELEMENTS(s1.aNode); i++)
    {
        PAVLROGCPHYSNODECORE pNode = &s1.aNode[i];
        if (!RTAvlroGCPhysInsert(&s1.Tree, pNode))
        {
            printf("tstAvl: FAILURE - roGCPhys - real insert i=%d\n", i);
            return 1;
        }
        if (RTAvlroGCPhysInsert(&s1.Tree, pNode))
        {
            printf("tstAvl: FAILURE - roGCPhys - real negative insert i=%d\n", i);
            return 1;
        }
        if (RTAvlroGCPhysGet(&s1.Tree, pNode->Key) != pNode)
        {
            printf("tstAvl: FAILURE - roGCPhys - real get (1) i=%d\n", i);
            return 1;
        }
        if (RTAvlroGCPhysGet(&s1.Tree, pNode->KeyLast) != NULL)
        {
            printf("tstAvl: FAILURE - roGCPhys - real negative get (2) i=%d\n", i);
            return 1;
        }
        if (RTAvlroGCPhysRangeGet(&s1.Tree, pNode->Key) != pNode)
        {
            printf("tstAvl: FAILURE - roGCPhys - real range get (1) i=%d\n", i);
            return 1;
        }
        if (RTAvlroGCPhysRangeGet(&s1.Tree, pNode->Key + 1) != pNode)
        {
            printf("tstAvl: FAILURE - roGCPhys - real range get (2) i=%d\n", i);
            return 1;
        }
        if (RTAvlroGCPhysRangeGet(&s1.Tree, pNode->KeyLast) != pNode)
        {
            printf("tstAvl: FAILURE - roGCPhys - real range get (3) i=%d\n", i);
            return 1;
        }
    }

    s3 = s1;
    s1 = s2;
    for (i = 0; i < RT_ELEMENTS(s3.aNode); i++)
    {
        PAVLROGCPHYSNODECORE pNode = &s3.aNode[i];
        if (RTAvlroGCPhysGet(&s3.Tree, pNode->Key) != pNode)
        {
            printf("tstAvl: FAILURE - roGCPhys - real get (10) i=%d\n", i);
            return 1;
        }
        if (RTAvlroGCPhysRangeGet(&s3.Tree, pNode->Key) != pNode)
        {
            printf("tstAvl: FAILURE - roGCPhys - real range get (10) i=%d\n", i);
            return 1;
        }

        j = pNode->Key + 1;
        do
        {
            if (RTAvlroGCPhysGet(&s3.Tree, j) != NULL)
            {
                printf("tstAvl: FAILURE - roGCPhys - real negative get (11) i=%d j=%#x\n", i, j);
                return 1;
            }
            if (RTAvlroGCPhysRangeGet(&s3.Tree, j) != pNode)
            {
                printf("tstAvl: FAILURE - roGCPhys - real range get (11) i=%d j=%#x\n", i, j);
                return 1;
            }
        } while (j++ < pNode->KeyLast);
    }

    return 0;
}


int avlul(void)
{
    /*
     * Simple linear insert and remove.
     */
    PAVLULNODECORE  pTree = 0;
    unsigned i;
    /* insert */
    for (i = 0; i < 65536; i++)
    {
        PAVLULNODECORE pNode = (PAVLULNODECORE)malloc(sizeof(*pNode));
        pNode->Key = i;
        if (!RTAvlULInsert(&pTree, pNode))
        {
            printf("tstAvl: FAILURE - UL - linear insert i=%d\n", i);
            return 1;
        }
        /* negative. */
        AVLULNODECORE Node = *pNode;
        if (RTAvlULInsert(&pTree, &Node))
        {
            printf("tstAvl: FAILURE - UL - linear negative insert i=%d\n", i);
            return 1;
        }
    }

    for (i = 0; i < 65536; i++)
    {
        PAVLULNODECORE pNode = RTAvlULRemove(&pTree, i);
        if (!pNode)
        {
            printf("tstAvl: FAILURE - UL - linear remove i=%d\n", i);
            return 1;
        }
        pNode->pLeft     = (PAVLULNODECORE)0xaaaaaaaa;
        pNode->pRight    = (PAVLULNODECORE)0xbbbbbbbb;
        pNode->uchHeight = 'e';
        free(pNode);

        /* negative */
        pNode = RTAvlULRemove(&pTree, i);
        if (pNode)
        {
            printf("tstAvl: FAILURE - UL - linear negative remove i=%d\n", i);
            return 1;
        }
    }

    /*
     * Make a sparsely populated tree.
     */
    for (i = 0; i < 65536; i += 8)
    {
        PAVLULNODECORE pNode = (PAVLULNODECORE)malloc(sizeof(*pNode));
        pNode->Key = i;
        if (!RTAvlULInsert(&pTree, pNode))
        {
            printf("tstAvl: FAILURE - UL - linear insert i=%d\n", i);
            return 1;
        }
        /* negative. */
        AVLULNODECORE Node = *pNode;
        if (RTAvlULInsert(&pTree, &Node))
        {
            printf("tstAvl: FAILURE - UL - linear negative insert i=%d\n", i);
            return 1;
        }
    }

    /*
     * Remove using best fit in 5 cycles.
     */
    unsigned j;
    for (j = 0; j < 4; j++)
    {
        for (i = 0; i < 65536; i += 8 * 4)
        {
            PAVLULNODECORE pNode = RTAvlULRemoveBestFit(&pTree, i, true);
            //PAVLULNODECORE pNode = RTAvlULRemove(&pTree, i + j * 8);
            if (!pNode)
            {
                printf("tstAvl: FAILURE - UL - sparse remove i=%d j=%d\n", i, j);
                return 1;
            }
            pNode->pLeft     = (PAVLULNODECORE)0xdddddddd;
            pNode->pRight    = (PAVLULNODECORE)0xcccccccc;
            pNode->uchHeight = 'E';
            free(pNode);
        }
    }

    return 0;
}


int main()
{
    int cErrors = 0;
    unsigned i;

    ProgressPrintf(~0, "tstAvl oGCPhys(32..2048)\n");
    for (i = 32; i < 2048 && !cErrors; i++)
        cErrors += avlogcphys(i);
    cErrors += avlogcphys(_64K);
    cErrors += avlogcphys(_512K);
    cErrors += avlogcphys(_4M);
    for (unsigned j = 0; j < /*256*/ 1 && !cErrors; j++)
    {
        ProgressPrintf(~0, "tstAvl oGCPhys(32..2048, *1K)\n");
        for (i = 32; i < 4096 && !cErrors; i++)
            cErrors += avlogcphysRand(i, i + _1K);
        for (; i <= _4M && !cErrors; i *= 2)
            cErrors += avlogcphysRand(i, i * 8);
    }

    cErrors += avlrogcphys();
    cErrors += avlul();

    if (!cErrors)
        printf("tstAvl: SUCCESS\n");
    else
        printf("tstAvl: FAILURE - %d errors\n", cErrors);
    return !!cErrors;
}
