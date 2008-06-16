/** @file
 *
 * Simple VBox HDD container test utility.
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
 */

#include <VBox/err.h>
#include <VBox/VBoxHDD-new.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/initterm.h>
#include <iprt/rand.h>
#include "stdio.h"
#include "stdlib.h"

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The error count. */
unsigned g_cErrors = 0;


static void tstVDError(void *pvUser, int rc, RT_SRC_POS_DECL,
                       const char *pszFormat, va_list va)
{
    g_cErrors++;
    RTPrintf("tstVD: Error %Vrc at %s:%u (%s): ", rc, RT_SRC_POS_ARGS);
    RTPrintfV(pszFormat, va);
    RTPrintf("\n");
}


static int tstVDCreateDelete(const char *pszBackend, const char *pszFilename,
                             uint64_t cbSize, VDIMAGETYPE enmType,
                             unsigned uFlags, bool fDelete)
{
    int rc;
    PVBOXHDD pVD = NULL;
    PDMMEDIAGEOMETRY PCHS = { 0, 0, 0 };
    PDMMEDIAGEOMETRY LCHS = { 0, 0, 0 };

#define CHECK(str) \
    do \
    { \
        RTPrintf("%s rc=%Vrc\n", str, rc); \
        if (VBOX_FAILURE(rc)) \
        { \
            VDCloseAll(pVD); \
            return rc; \
        } \
    } while (0)

    rc = VDCreate(tstVDError, NULL, &pVD);
    CHECK("VDCreate()");

    rc = VDCreateBase(pVD, pszBackend, pszFilename, enmType, cbSize,
                      uFlags, "Test image", &PCHS, &LCHS, VD_OPEN_FLAGS_NORMAL,
                      NULL, NULL);
    CHECK("VDCreateBase()");

    VDDumpImages(pVD);

    VDClose(pVD, fDelete);
#undef CHECK
    return 0;
}


#undef RTDECL
#define RTDECL(x) static x

/* Start of IPRT code */

/**
 * The following code is based on the work of George Marsaglia 
 * taken from 
 * http://groups.google.ws/group/comp.sys.sun.admin/msg/7c667186f6cbf354
 * and 
 * http://groups.google.ws/group/comp.lang.c/msg/0e170777c6e79e8d 
 */

/* 
A C version of a very very good 64-bit RNG is given below. 
You should be able to adapt it to your particular needs. 

It is based on the complimentary-multiple-with-carry 
sequence 
         x(n)=a*x(n-4)+carry mod 2^64-1, 
which works as follows: 
Assume a certain multiplier 'a' and a base 'b'. 
Given a current x value and a current carry 'c', 
form:               t=a*x+c 
Then the new carry is     c=floor(t/b) 
and the new x value is    x = b-1-(t mod b). 


Ordinarily, for 32-bit mwc or cmwc sequences, the 
value t=a*x+c can be formed in 64 bits, then the new c 
is the top and the new x the bottom 32 bits (with a little 
fiddling when b=2^32-1 and cmwc rather than mwc.) 


To generate 64-bit x's, it is difficult to form 
t=a*x+c in 128 bits then get the new c and new x 
from the the top and bottom halves. 
But if 'a' has a special form, for example, 
a=2^62+2^47+2 and b=2^64-1, then the new c and 
the new x can be formed with shifts, tests and +/-'s, 
again with a little fiddling because b=2^64-1 rather 
than 2^64.   (The latter is not an optimal choice because, 
being a square, it cannot be a primitive root of the 
prime a*b^k+1, where 'k' is the 'lag': 
        x(n)=a*x(n-k)+carry mod b.) 
But the multiplier a=2^62+2^47+2 makes a*b^4+1 a prime for 
which b=2^64-1 is a primitive root, and getting  the new x and 
new c  can be done with arithmetic on integers the size of x. 
*/ 

struct RndCtx
{
    uint64_t x;
    uint64_t y;
    uint64_t z;
    uint64_t w;
    uint64_t c;
    uint32_t u32x;
    uint32_t u32y;
};
typedef struct RndCtx RNDCTX;
typedef RNDCTX *PRNDCTX;

/**
 * Initialize seeds. 
 *  
 * @remarks You should choose ANY 4 random 64-bit
 * seeds x,y,z,w < 2^64-1 and a random seed c in
 * 0<= c < a = 2^62+2^47+2.
 * There are P=(2^62+2^46+2)*(2^64-1)^4 > 2^318 possible choices
 * for seeds, the period of the RNG.
 */
RTDECL(int) RTPRandInit(PRNDCTX pCtx, uint32_t u32Seed)
{
    if (u32Seed == 0)
        u32Seed = (uint32_t)(ASMReadTSC() >> 8);
    /* Zero is not a good seed. */
    if (u32Seed == 0)
        u32Seed = 362436069;
    pCtx->x = u32Seed;
    pCtx->y = 17280675555674358941ULL;
    pCtx->z = 6376492577913983186ULL;
    pCtx->w = 9064188857900113776ULL;
    pCtx->c = 123456789;
    pCtx->u32x = 2282008;
    pCtx->u32y = u32Seed;
    return VINF_SUCCESS;
}

RTDECL(uint32_t) RTPRandGetSeedInfo(PRNDCTX pCtx)
{
    return pCtx->u32y;
}

/**
 * Generate a 64-bit unsigned random number.
 *
 * @returns The pseudo random number.
 */
RTDECL(uint64_t) RTPRandU64(PRNDCTX pCtx)
{
    uint64_t t; 
    t = (pCtx->x<<47) + (pCtx->x<<62) + (pCtx->x<<1); 
    t += pCtx->c; t+= (t < pCtx->c); 
    pCtx->c = (t<pCtx->c) + (pCtx->x>>17) + (pCtx->x>>2) + (pCtx->x>>63); 
    pCtx->x = pCtx->y;  pCtx->y = pCtx->z ; pCtx->z = pCtx->w; 
    return (pCtx->w = ~(t + pCtx->c)-1); 
} 

/**
 * Generate a 64-bit unsigned pseudo random number in the set 
 * [u64First..u64Last]. 
 *
 * @returns The pseudo random number.
 * @param   u64First    First number in the set.
 * @param   u64Last     Last number in the set.
 */
RTDECL(uint64_t) RTPRandU64Ex(PRNDCTX pCtx, uint64_t u64First, uint64_t u64Last)
{
    if (u64First == 0 && u64Last == UINT64_MAX)
        return RTPRandU64(pCtx);

    uint64_t u64Tmp;
    uint64_t u64Range = u64Last - u64First + 1;
    uint64_t u64Scale = UINT64_MAX / u64Range;

    do
    {
        u64Tmp = RTPRandU64(pCtx) / u64Scale;
    } while (u64Tmp >= u64Range);
    return u64First + u64Tmp;
}

/**
 * Generate a 32-bit unsigned random number.
 *
 * @returns The pseudo random number.
 */
RTDECL(uint32_t) RTPRandU32(PRNDCTX pCtx)
{
    return ( pCtx->u32x = 69069 * pCtx->u32x + 123,
             pCtx->u32y ^= pCtx->u32y<<13,
             pCtx->u32y ^= pCtx->u32y>>17,
             pCtx->u32y ^= pCtx->u32y<<5,
             pCtx->u32x + pCtx->u32y );
} 

/**
 * Generate a 32-bit unsigned pseudo random number in the set 
 * [u32First..u32Last]. 
 *
 * @returns The pseudo random number.
 * @param   u32First    First number in the set.
 * @param   u32Last     Last number in the set.
 */
RTDECL(uint32_t) RTPRandU32Ex(PRNDCTX pCtx, uint32_t u32First, uint32_t u32Last)
{
    if (u32First == 0 && u32Last == UINT32_MAX)
        return RTPRandU32(pCtx);

    uint32_t u32Tmp;
    uint32_t u32Range = u32Last - u32First + 1;
    uint32_t u32Scale = UINT32_MAX / u32Range;

    do
    {
        u32Tmp = RTPRandU32(pCtx) / u32Scale;
    } while (u32Tmp >= u32Range);
    return u32First + u32Tmp;
}

/* End of IPRT code */

struct Segment
{
    uint64_t u64Offset;
    uint32_t u32Length;
    uint32_t u8Value;
};
typedef struct Segment *PSEGMENT;

static void initializeRandomGenerator(PRNDCTX pCtx, uint32_t u32Seed)
{
    int rc = RTPRandInit(pCtx, u32Seed);
    if (VBOX_FAILURE(rc))
        RTPrintf("ERROR: Failed to initialize random generator. RC=%Vrc\n", rc);
    else
    {
        RTPrintf("INFO: Random generator seed used: %x\n", RTPRandGetSeedInfo(pCtx));    
        RTLogPrintf("INFO: Random generator seed used: %x\n", RTPRandGetSeedInfo(pCtx));    
    }
}

static int compareSegments(const void *left, const void *right)
{
    /* Note that no duplicates are allowed in the array being sorted. */
    return ((PSEGMENT)left)->u64Offset < ((PSEGMENT)right)->u64Offset ? -1 : 1;
}

static void generateRandomSegments(PRNDCTX pCtx, PSEGMENT pSegment, uint32_t nSegments, uint32_t u32MaxSegmentSize, uint64_t u64DiskSize, uint32_t u32SectorSize, uint8_t u8ValueLow, uint8_t u8ValueHigh)
{
    uint32_t i;
    /* Generate segment offsets. */
    for (i = 0; i < nSegments; i++)
    {
        bool fDuplicateFound;
        do
        {
            pSegment[i].u64Offset = RTPRandU64Ex(pCtx, 0, u64DiskSize / u32SectorSize - 1) * u32SectorSize;
            fDuplicateFound = false;
            for (uint32_t j = 0; j < i; j++)
                if (pSegment[i].u64Offset == pSegment[j].u64Offset)
                {
                    fDuplicateFound = true;
                    break;
                }
        } while (fDuplicateFound);
    }
    /* Sort in offset-ascending order. */
    qsort(pSegment, nSegments, sizeof(*pSegment), compareSegments);
    /* Put a sentinel at the end. */
    pSegment[nSegments].u64Offset = u64DiskSize;
    pSegment[nSegments].u32Length = 0;
    /* Generate segment lengths and values. */
    for (i = 0; i < nSegments; i++)
    {
        pSegment[i].u32Length = RTPRandU32Ex(pCtx, 1, RT_MIN(pSegment[i+1].u64Offset - pSegment[i].u64Offset,
                                                             u32MaxSegmentSize) / u32SectorSize) * u32SectorSize;
        Assert(pSegment[i].u32Length <= u32MaxSegmentSize);
        pSegment[i].u8Value  = RTPRandU32Ex(pCtx, (uint32_t)u8ValueLow, (uint32_t)u8ValueHigh);
    }
}

static void mergeSegments(PSEGMENT pBaseSegment, PSEGMENT pDiffSegment, PSEGMENT pMergeSegment, uint32_t u32MaxLength)
{
    while (pBaseSegment->u32Length > 0 || pDiffSegment->u32Length > 0)
    {
        if (pBaseSegment->u64Offset < pDiffSegment->u64Offset)
        {
            *pMergeSegment = *pBaseSegment;
            if (pMergeSegment->u64Offset + pMergeSegment->u32Length <= pDiffSegment->u64Offset)
                pBaseSegment++;
            else
            {
                pMergeSegment->u32Length = pDiffSegment->u64Offset - pMergeSegment->u64Offset;
                Assert(pMergeSegment->u32Length <= u32MaxLength);
                if (pBaseSegment->u64Offset + pBaseSegment->u32Length >
                    pDiffSegment->u64Offset + pDiffSegment->u32Length)
                {
                    pBaseSegment->u32Length -= pDiffSegment->u64Offset + pDiffSegment->u32Length - pBaseSegment->u64Offset;
                    Assert(pBaseSegment->u32Length <= u32MaxLength);
                    pBaseSegment->u64Offset = pDiffSegment->u64Offset + pDiffSegment->u32Length;
                }
                else
                    pBaseSegment++;
            }
            pMergeSegment++;
        }
        else
        {
            *pMergeSegment = *pDiffSegment;
            if (pMergeSegment->u64Offset + pMergeSegment->u32Length <= pBaseSegment->u64Offset)
            {
                pDiffSegment++;
                pMergeSegment++;
            }
            else
            {
                if (pBaseSegment->u64Offset + pBaseSegment->u32Length > pDiffSegment->u64Offset + pDiffSegment->u32Length)
                {
                    pBaseSegment->u32Length -= pDiffSegment->u64Offset + pDiffSegment->u32Length - pBaseSegment->u64Offset;
                    Assert(pBaseSegment->u32Length <= u32MaxLength);
                    pBaseSegment->u64Offset = pDiffSegment->u64Offset + pDiffSegment->u32Length;
                    pDiffSegment++;
                    pMergeSegment++;
                }
                else
                    pBaseSegment++;
            }
        }
    }
}

static void writeSegmentsToDisk(PVBOXHDD pVD, void *pvBuf, PSEGMENT pSegment)
{
    while (pSegment->u32Length)
    {
        //memset((uint8_t*)pvBuf + pSegment->u64Offset, pSegment->u8Value, pSegment->u32Length);
        memset(pvBuf, pSegment->u8Value, pSegment->u32Length);
        VDWrite(pVD, pSegment->u64Offset, pvBuf, pSegment->u32Length);
        pSegment++;
    }
}

static int readAndCompareSegments(PVBOXHDD pVD, void *pvBuf, PSEGMENT pSegment)
{
    while (pSegment->u32Length)
    {
        int rc = VDRead(pVD, pSegment->u64Offset, pvBuf, pSegment->u32Length);
        if (VBOX_FAILURE(rc))
        {
            RTPrintf("ERROR: Failed to read from virtual disk\n");
            return rc;
        }
        else
        {
            for (unsigned i = 0; i < pSegment->u32Length; i++)
                if (((uint8_t*)pvBuf)[i] != pSegment->u8Value)
                {
                    RTPrintf("ERROR: Segment at %Lx of %x bytes is corrupt at offset %x (found %x instead of %x)\n",
                             pSegment->u64Offset, pSegment->u32Length, i, ((uint8_t*)pvBuf)[i],
                             pSegment->u8Value);
                    RTLogPrintf("ERROR: Segment at %Lx of %x bytes is corrupt at offset %x (found %x instead of %x)\n",
                             pSegment->u64Offset, pSegment->u32Length, i, ((uint8_t*)pvBuf)[i],
                             pSegment->u8Value);
                    return VERR_INTERNAL_ERROR;
                }
        }
        pSegment++;
    }

    return VINF_SUCCESS;
}

static int tstVDOpenCreateWriteMerge(const char *pszBackend,
                                     const char *pszBaseFilename,
                                     const char *pszDiffFilename,
                                     uint32_t u32Seed)
{
    int rc;
    PVBOXHDD pVD = NULL;
    char *pszFormat;
    PDMMEDIAGEOMETRY PCHS = { 0, 0, 0 };
    PDMMEDIAGEOMETRY LCHS = { 0, 0, 0 };
    uint64_t u64DiskSize  = 1000 * _1M;
    uint32_t u32SectorSize = 512;

#define CHECK(str) \
    do \
    { \
        RTPrintf("%s rc=%Vrc\n", str, rc); \
        if (VBOX_FAILURE(rc)) \
        { \
            VDCloseAll(pVD); \
            return rc; \
        } \
    } while (0)

    rc = VDCreate(tstVDError, NULL, &pVD);
    CHECK("VDCreate()");

    RTFILE File;
    rc = RTFileOpen(&File, pszBaseFilename, RTFILE_O_READ);
    if (VBOX_SUCCESS(rc))
    {
        RTFileClose(File);
        rc = VDGetFormat(pszBaseFilename, &pszFormat);
        RTPrintf("VDGetFormat() pszFormat=%s rc=%Vrc\n", pszFormat, rc);
        if (VBOX_SUCCESS(rc) && strcmp(pszFormat, pszBackend))
        {
            rc = VERR_GENERAL_FAILURE;
            RTPrintf("VDGetFormat() returned incorrect backend name\n");
        }
        RTStrFree(pszFormat);
        CHECK("VDGetFormat()");

        rc = VDOpen(pVD, pszBackend, pszBaseFilename, VD_OPEN_FLAGS_NORMAL);
        CHECK("VDOpen()");
    }
    else
    {
        rc = VDCreateBase(pVD, pszBackend, pszBaseFilename,
                          VD_IMAGE_TYPE_NORMAL, u64DiskSize,
                          VD_IMAGE_FLAGS_NONE, "Test image",
                          &PCHS, &LCHS, VD_OPEN_FLAGS_NORMAL,
                          NULL, NULL);
        CHECK("VDCreateBase()");
    }

    int nSegments = 100;
    /* Allocate one extra element for a sentinel. */
    PSEGMENT paBaseSegments  = (PSEGMENT)RTMemAllocZ(sizeof(struct Segment) * (nSegments + 1));
    PSEGMENT paDiffSegments  = (PSEGMENT)RTMemAllocZ(sizeof(struct Segment) * (nSegments + 1));
    PSEGMENT paMergeSegments = (PSEGMENT)RTMemAllocZ(sizeof(struct Segment) * (nSegments + 1) * 3);

    void *pvBuf = RTMemAlloc(_1M);

    RNDCTX ctx;
    initializeRandomGenerator(&ctx, u32Seed);
    generateRandomSegments(&ctx, paBaseSegments, nSegments, _1M, u64DiskSize, u32SectorSize, 0u, 127u);
    generateRandomSegments(&ctx, paDiffSegments, nSegments, _1M, u64DiskSize, u32SectorSize, 128u, 255u);

    /*PSEGMENT pSegment;
    RTPrintf("Base segments:\n");
    for (pSegment = paBaseSegments; pSegment->u32Length; pSegment++)
        RTPrintf("off: %08Lx len: %05x val: %02x\n", pSegment->u64Offset, pSegment->u32Length, pSegment->u8Value);*/
    writeSegmentsToDisk(pVD, pvBuf, paBaseSegments);

    rc = VDCreateDiff(pVD, pszBackend, pszDiffFilename,
                      VD_IMAGE_FLAGS_NONE, "Test diff image",
                      VD_OPEN_FLAGS_NORMAL, NULL, NULL);
    CHECK("VDCreateDiff()");

    /*RTPrintf("\nDiff segments:\n");
    for (pSegment = paDiffSegments; pSegment->u32Length; pSegment++)
        RTPrintf("off: %08Lx len: %05x val: %02x\n", pSegment->u64Offset, pSegment->u32Length, pSegment->u8Value);*/
    writeSegmentsToDisk(pVD, pvBuf, paDiffSegments);

    VDDumpImages(pVD);

    RTPrintf("Merging diff into base..\n");
    rc = VDMerge(pVD, (unsigned)-1, 0, NULL, NULL);
    CHECK("VDMerge()");

    mergeSegments(paBaseSegments, paDiffSegments, paMergeSegments, _1M);
    /*RTPrintf("\nMerged segments:\n");
    for (pSegment = paMergeSegments; pSegment->u32Length; pSegment++)
        RTPrintf("off: %08Lx len: %05x val: %02x\n", pSegment->u64Offset, pSegment->u32Length, pSegment->u8Value);*/
    rc = readAndCompareSegments(pVD, pvBuf, paMergeSegments);
    CHECK("readAndCompareSegments()");

    RTMemFree(paMergeSegments);
    RTMemFree(paDiffSegments);
    RTMemFree(paBaseSegments);

    VDDumpImages(pVD);

    VDCloseAll(pVD);
#undef CHECK
    return 0;
}

static int tstVDCreateWriteOpenRead(const char *pszBackend,
                                    const char *pszFilename,
                                    uint32_t u32Seed)
{
    int rc;
    PVBOXHDD pVD = NULL;
    PDMMEDIAGEOMETRY PCHS = { 0, 0, 0 };
    PDMMEDIAGEOMETRY LCHS = { 0, 0, 0 };
    uint64_t u64DiskSize  = 1000 * _1M;
    uint32_t u32SectorSize = 512;

#define CHECK(str) \
    do \
    { \
        RTPrintf("%s rc=%Vrc\n", str, rc); \
        if (VBOX_FAILURE(rc)) \
        { \
            VDCloseAll(pVD); \
            return rc; \
        } \
    } while (0)

    rc = VDCreate(tstVDError, NULL, &pVD);
    CHECK("VDCreate()");

    RTFILE File;
    rc = RTFileOpen(&File, pszFilename, RTFILE_O_READ);
    if (VBOX_SUCCESS(rc))
    {
        RTFileClose(File);
        RTFileDelete(pszFilename);
    }

    rc = VDCreateBase(pVD, pszBackend, pszFilename,
                      VD_IMAGE_TYPE_NORMAL, u64DiskSize,
                      VD_IMAGE_FLAGS_NONE, "Test image",
                      &PCHS, &LCHS, VD_OPEN_FLAGS_NORMAL,
                      NULL, NULL);
    CHECK("VDCreateBase()");

    int nSegments = 100;
    /* Allocate one extra element for a sentinel. */
    PSEGMENT paSegments  = (PSEGMENT)RTMemAllocZ(sizeof(struct Segment) * (nSegments + 1));

    void *pvBuf = RTMemAlloc(_1M);

    RNDCTX ctx;
    initializeRandomGenerator(&ctx, u32Seed);
    generateRandomSegments(&ctx, paSegments, nSegments, _1M, u64DiskSize, u32SectorSize, 0u, 127u);
    /*for (PSEGMENT pSegment = paSegments; pSegment->u32Length; pSegment++)
        RTPrintf("off: %08Lx len: %05x val: %02x\n", pSegment->u64Offset, pSegment->u32Length, pSegment->u8Value);*/

    writeSegmentsToDisk(pVD, pvBuf, paSegments);

    VDCloseAll(pVD);

    rc = VDOpen(pVD, pszBackend, pszFilename, VD_OPEN_FLAGS_NORMAL);
    CHECK("VDOpen()");
    rc = readAndCompareSegments(pVD, pvBuf, paSegments);
    CHECK("readAndCompareSegments()");

    RTMemFree(paSegments);

    VDCloseAll(pVD);
#undef CHECK
    return 0;
}


int main(int argc, char *argv[])
{
    int rc;

    uint32_t u32Seed = 0; // Means choose random

    if (argc > 1)
        if (sscanf(argv[1], "%x", &u32Seed) != 1)
        {
            RTPrintf("ERROR: Invalid parameter %s. Valid usage is %s <32-bit seed>.\n",
                     argv[1], argv[0]);
            return 1;
        }

    RTR3Init();
    RTPrintf("tstVD: TESTING...\n");

    /*
     * Clean up potential leftovers from previous unsuccessful runs.
     */
    RTFileDelete("tmpVDCreate.vdi");
    RTFileDelete("tmpVDCreate.vmdk");
    RTFileDelete("tmpVDCreate.vhd");
    RTFileDelete("tmpVDBase.vdi");
    RTFileDelete("tmpVDDiff.vdi");
    RTFileDelete("tmpVDBase.vmdk");
    RTFileDelete("tmpVDDiff.vmdk");
    RTFileDelete("tmpVDBase.vhd");
    RTFileDelete("tmpVDDiff.vhd");
#if 1
    rc = tstVDCreateDelete("VDI", "tmpVDCreate.vdi", 2 * _4G,
                           VD_IMAGE_TYPE_NORMAL, VD_IMAGE_FLAGS_NONE,
                           true);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("tstVD: dynamic VDI create test failed! rc=%Vrc\n", rc);
        g_cErrors++;
    }
    rc = tstVDCreateDelete("VDI", "tmpVDCreate.vdi", 2 * _4G,
                           VD_IMAGE_TYPE_FIXED, VD_IMAGE_FLAGS_NONE,
                           true);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("tstVD: fixed VDI create test failed! rc=%Vrc\n", rc);
        g_cErrors++;
    }
    rc = tstVDCreateDelete("VMDK", "tmpVDCreate.vmdk", 2 * _4G,
                           VD_IMAGE_TYPE_NORMAL, VD_IMAGE_FLAGS_NONE,
                           true);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("tstVD: dynamic VMDK create test failed! rc=%Vrc\n", rc);
        g_cErrors++;
    }
    rc = tstVDCreateDelete("VMDK", "tmpVDCreate.vmdk", 2 * _4G,
                           VD_IMAGE_TYPE_NORMAL, VD_VMDK_IMAGE_FLAGS_SPLIT_2G,
                           true);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("tstVD: dynamic split VMDK create test failed! rc=%Vrc\n", rc);
        g_cErrors++;
    }
    rc = tstVDCreateDelete("VMDK", "tmpVDCreate.vmdk", 2 * _4G,
                           VD_IMAGE_TYPE_FIXED, VD_IMAGE_FLAGS_NONE,
                           true);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("tstVD: fixed VMDK create test failed! rc=%Vrc\n", rc);
        g_cErrors++;
    }
    rc = tstVDCreateDelete("VMDK", "tmpVDCreate.vmdk", 2 * _4G,
                           VD_IMAGE_TYPE_FIXED, VD_VMDK_IMAGE_FLAGS_SPLIT_2G,
                           true);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("tstVD: fixed split VMDK create test failed! rc=%Vrc\n", rc);
        g_cErrors++;
    }
    rc = tstVDCreateDelete("VHD", "tmpVDCreate.vhd", 2 * _4G,
                           VD_IMAGE_TYPE_NORMAL, VD_IMAGE_FLAGS_NONE,
                           true);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("tstVD: dynamic VHD create test failed! rc=%Vrc\n", rc);
        g_cErrors++;
    }
    rc = tstVDCreateDelete("VHD", "tmpVDCreate.vhd", 2 * _4G,
                           VD_IMAGE_TYPE_FIXED, VD_IMAGE_FLAGS_NONE,
                           true);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("tstVD: fixed VHD create test failed! rc=%Vrc\n", rc);
        g_cErrors++;
    }

    rc = tstVDOpenCreateWriteMerge("VDI", "tmpVDBase.vdi", "tmpVDDiff.vdi", u32Seed);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("tstVD: VDI test failed (new image)! rc=%Vrc\n", rc);
        g_cErrors++;
    }
    rc = tstVDOpenCreateWriteMerge("VDI", "tmpVDBase.vdi", "tmpVDDiff.vdi", u32Seed);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("tstVD: VDI test failed (existing image)! rc=%Vrc\n", rc);
        g_cErrors++;
    }
    rc = tstVDOpenCreateWriteMerge("VMDK", "tmpVDBase.vmdk", "tmpVDDiff.vmdk", u32Seed);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("tstVD: VMDK test failed (new image)! rc=%Vrc\n", rc);
        g_cErrors++;
    }
    rc = tstVDOpenCreateWriteMerge("VMDK", "tmpVDBase.vmdk", "tmpVDDiff.vmdk", u32Seed);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("tstVD: VMDK test failed (existing image)! rc=%Vrc\n", rc);
        g_cErrors++;
    }
#endif

    rc = tstVDCreateWriteOpenRead("VHD", "tmpVDCreate.vhd", u32Seed);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("tstVD: VHD test failed (creating image)! rc=%Vrc\n", rc);
        g_cErrors++;
    }

    rc = tstVDOpenCreateWriteMerge("VHD", "tmpVDBase.vhd", "tmpVDDiff.vhd", u32Seed);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("tstVD: VHD test failed (existing image)! rc=%Vrc\n", rc);
        g_cErrors++;
    }

    /*
     * Clean up any leftovers.
     */
    RTFileDelete("tmpVDCreate.vdi");
    RTFileDelete("tmpVDCreate.vmdk");
    RTFileDelete("tmpVDCreate.vhd");
    RTFileDelete("tmpVDBase.vdi");
    RTFileDelete("tmpVDDiff.vdi");
    RTFileDelete("tmpVDBase.vmdk");
    RTFileDelete("tmpVDDiff.vmdk");
    RTFileDelete("tmpVDBase.vhd");
    RTFileDelete("tmpVDDiff.vhd");

    /*
     * Summary
     */
    if (!g_cErrors)
        RTPrintf("tstVD: SUCCESS\n");
    else
        RTPrintf("tstVD: FAILURE - %d errors\n", g_cErrors);

    return !!g_cErrors;
}

