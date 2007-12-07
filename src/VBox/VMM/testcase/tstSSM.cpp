/* $Id$ */
/** @file
 * Saved State Manager Testcase.
 */

/*
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/ssm.h>
#include <VBox/vm.h>

#include <VBox/log.h>
#include <VBox/sup.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <iprt/runtime.h>
#include <iprt/assert.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/time.h>


const uint8_t gabPage[PAGE_SIZE] = {0};

const char gachMem1[] = "sdfg\1asdfa\177hjkl;sdfghjkl;dfghjkl;dfghjkl;\0\0asdf;kjasdf;lkjasd;flkjasd;lfkjasd\0;lfk";

uint8_t gabBigMem[8*1024*1024];

/** initializes gabBigMem with some non zero stuff. */
void initBigMem(void)
{
#if 0
    uint32_t *puch = (uint32_t *)&gabBigMem[0];
    uint32_t *puchEnd = (uint32_t *)&gabBigMem[sizeof(gabBigMem)];
    uint32_t  u32 = 0xdeadbeef;
    for (; puch < puchEnd; puch++)
    {
        *puch = u32;
        u32 += 19;
        u32 = (u32 << 1) | (u32 >> 31);
    }
#else
    uint8_t *pb = &gabBigMem[0];
    uint8_t *pbEnd = &gabBigMem[sizeof(gabBigMem)];
    for (; pb < pbEnd; pb += 16)
    {
        char szTmp[17];
        RTStrPrintf(szTmp, sizeof(szTmp), "aaaa%08Xzzzz", (uint32_t)(uintptr_t)pb);
        memcpy(pb, szTmp, 16);
    }
#endif
}

/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the device which registered the data unit.
 * @param   pSSM            SSM operation handle.
 */
DECLCALLBACK(int) Item01Save(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    uint64_t u64Start = RTTimeNanoTS();

    /*
     * Test writing some memory block.
     */
    int rc = SSMR3PutMem(pSSM, gachMem1, sizeof(gachMem1));
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("Item01: #1 - SSMR3PutMem -> %Vrc\n", rc);
        return rc;
    }

    /*
     * Test writing a zeroterminated string.
     */
    rc = SSMR3PutStrZ(pSSM, "String");
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("Item01: #1 - SSMR3PutMem -> %Vrc\n", rc);
        return rc;
    }


    /*
     * Test the individual integer put functions to see that they all work.
     * (Testcases are also known as "The Land of The Ugly Code"...)
     */
#define ITEM(suff,bits, val) \
    rc = SSMR3Put##suff(pSSM, val); \
    if (VBOX_FAILURE(rc)) \
    { \
        RTPrintf("Item01: #" #suff " - SSMR3Put" #suff "(," #val ") -> %Vrc\n", rc); \
        return rc; \
    }
    /* copy & past with the load one! */
    ITEM(U8,  uint8_t,  0xff);
    ITEM(U8,  uint8_t,  0x0);
    ITEM(U8,  uint8_t,  1);
    ITEM(U8,  uint8_t,  42);
    ITEM(U8,  uint8_t,  230);
    ITEM(S8,   int8_t,  -128);
    ITEM(S8,   int8_t,  127);
    ITEM(S8,   int8_t,  12);
    ITEM(S8,   int8_t,  -76);
    ITEM(U16, uint16_t, 0xffff);
    ITEM(U16, uint16_t, 0x0);
    ITEM(S16,  int16_t, 32767);
    ITEM(S16,  int16_t, -32768);
    ITEM(U32, uint32_t, 4294967295U);
    ITEM(U32, uint32_t, 0);
    ITEM(U32, uint32_t, 42);
    ITEM(U32, uint32_t, 2342342344U);
    ITEM(S32,  int32_t, -2147483647-1);
    ITEM(S32,  int32_t, 2147483647);
    ITEM(S32,  int32_t, 42);
    ITEM(S32,  int32_t, 568459834);
    ITEM(S32,  int32_t, -58758999);
    ITEM(U64, uint64_t, 18446744073709551615ULL);
    ITEM(U64, uint64_t, 0);
    ITEM(U64, uint64_t, 42);
    ITEM(U64, uint64_t, 593023944758394234ULL);
    ITEM(S64,  int64_t, 9223372036854775807LL);
    ITEM(S64,  int64_t, -9223372036854775807LL - 1);
    ITEM(S64,  int64_t, 42);
    ITEM(S64,  int64_t, 21398723459873LL);
    ITEM(S64,  int64_t, -5848594593453453245LL);
#undef ITEM

    uint64_t u64Elapsed = RTTimeNanoTS() - u64Start;
    RTPrintf("tstSSM: Saved 1st item in %RI64 ns\n", u64Elapsed);
    return 0;
}

/**
 * Prepare state load operation.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the device which registered the data unit.
 * @param   pSSM            SSM operation handle.
 */
DECLCALLBACK(int) Item01Load(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t u32Version)
{
    /*
     * Load the memory block.
     */
    char achTmp[sizeof(gachMem1)];
    int rc = SSMR3GetMem(pSSM, achTmp, sizeof(gachMem1));
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("Item01: #1 - SSMR3GetMem -> %Vrc\n", rc);
        return rc;
    }

    /*
     * Load the string.
     */
    rc = SSMR3GetStrZ(pSSM, achTmp, sizeof(achTmp));
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("Item01: #2 - SSMR3GetStrZ -> %Vrc\n", rc);
        return rc;
    }

    /*
     * Test the individual integer put functions to see that they all work.
     * (Testcases are also known as "The Land of The Ugly Code"...)
     */
#define ITEM(suff, type, val) \
    do { \
        type var = {0}; \
        rc = SSMR3Get##suff(pSSM, &var); \
        if (VBOX_FAILURE(rc)) \
        { \
            RTPrintf("Item01: #" #suff " - SSMR3Get" #suff "(," #val ") -> %Vrc\n", rc); \
            return rc; \
        } \
        if (var != val) \
        { \
            RTPrintf("Item01: #" #suff " - SSMR3Get" #suff "(," #val ") -> %d returned wrong value!\n", rc); \
            return VERR_GENERAL_FAILURE; \
        } \
    } while (0)
    /* copy & past with the load one! */
    ITEM(U8,  uint8_t,  0xff);
    ITEM(U8,  uint8_t,  0x0);
    ITEM(U8,  uint8_t,  1);
    ITEM(U8,  uint8_t,  42);
    ITEM(U8,  uint8_t,  230);
    ITEM(S8,   int8_t,  -128);
    ITEM(S8,   int8_t,  127);
    ITEM(S8,   int8_t,  12);
    ITEM(S8,   int8_t,  -76);
    ITEM(U16, uint16_t, 0xffff);
    ITEM(U16, uint16_t, 0x0);
    ITEM(S16,  int16_t, 32767);
    ITEM(S16,  int16_t, -32768);
    ITEM(U32, uint32_t, 4294967295U);
    ITEM(U32, uint32_t, 0);
    ITEM(U32, uint32_t, 42);
    ITEM(U32, uint32_t, 2342342344U);
    ITEM(S32,  int32_t, -2147483647-1);
    ITEM(S32,  int32_t, 2147483647);
    ITEM(S32,  int32_t, 42);
    ITEM(S32,  int32_t, 568459834);
    ITEM(S32,  int32_t, -58758999);
    ITEM(U64, uint64_t, 18446744073709551615ULL);
    ITEM(U64, uint64_t, 0);
    ITEM(U64, uint64_t, 42);
    ITEM(U64, uint64_t, 593023944758394234ULL);
    ITEM(S64,  int64_t, 9223372036854775807LL);
    ITEM(S64,  int64_t, -9223372036854775807LL - 1);
    ITEM(S64,  int64_t, 42);
    ITEM(S64,  int64_t, 21398723459873LL);
    ITEM(S64,  int64_t, -5848594593453453245LL);
#undef ITEM

    return 0;
}


/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the device which registered the data unit.
 * @param   pSSM            SSM operation handle.
 */
DECLCALLBACK(int) Item02Save(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    uint64_t u64Start = RTTimeNanoTS();

    /*
     * Put the size.
     */
    size_t cb = sizeof(gabBigMem);
    int rc = SSMR3PutU32(pSSM, cb);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("Item02: PutU32 -> %Vrc\n", rc);
        return rc;
    }

    /*
     * Put 8MB of memory to the file in 3 chunks.
     */
    uint8_t *pbMem = &gabBigMem[0];
    size_t cbChunk = cb / 47;
    rc = SSMR3PutMem(pSSM, pbMem, cbChunk);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("Item02: PutMem(,%p,%#x) -> %Vrc\n", pbMem, cbChunk, rc);
        return rc;
    }
    cb -= cbChunk;
    pbMem += cbChunk;

    /* next piece. */
    cbChunk *= 19;
    rc = SSMR3PutMem(pSSM, pbMem, cbChunk);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("Item02: PutMem(,%p,%#x) -> %Vrc\n", pbMem, cbChunk, rc);
        return rc;
    }
    cb -= cbChunk;
    pbMem += cbChunk;

    /* last piece. */
    cbChunk = cb;
    rc = SSMR3PutMem(pSSM, pbMem, cbChunk);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("Item02: PutMem(,%p,%#x) -> %Vrc\n", pbMem, cbChunk, rc);
        return rc;
    }

    uint64_t u64Elapsed = RTTimeNanoTS() - u64Start;
    RTPrintf("tstSSM: Saved 2nd item in %RI64 ns\n", u64Elapsed);
    return 0;
}

/**
 * Prepare state load operation.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the device which registered the data unit.
 * @param   pSSM            SSM operation handle.
 */
DECLCALLBACK(int) Item02Load(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t u32Version)
{
    /*
     * Load the size.
     */
    uint32_t cb;
    int rc = SSMR3GetU32(pSSM, &cb);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("Item02: SSMR3GetU32 -> %Vrc\n", rc);
        return rc;
    }
    if (cb != sizeof(gabBigMem))
    {
        RTPrintf("Item02: loaded size doesn't match the real thing. %#x != %#x\n", cb, sizeof(gabBigMem));
        return VERR_GENERAL_FAILURE;
    }

    /*
     * Load the memory chunk by chunk.
     */
    uint8_t    *pbMem = &gabBigMem[0];
    char        achTmp[16383];
    size_t      cbChunk = sizeof(achTmp);
    while (cb > 0)
    {
        cbChunk -= 7;
        if (cbChunk < 64)
            cbChunk = sizeof(achTmp) - (cbChunk % 47);
        if (cbChunk > cb)
            cbChunk = cb;
        rc = SSMR3GetMem(pSSM, &achTmp[0], cbChunk);
        if (VBOX_FAILURE(rc))
        {
            RTPrintf("Item02: SSMR3GetMem(,,%#x) -> %d offset %#x\n", cbChunk, rc, pbMem - &gabBigMem[0]);
            return rc;
        }
        if (memcmp(achTmp, pbMem, cbChunk))
        {
            RTPrintf("Item02: compare failed. mem offset=%#x cbChunk=%#x\n", pbMem - &gabBigMem[0], cbChunk);
            return VERR_GENERAL_FAILURE;
        }

        /* next */
        pbMem += cbChunk;
        cb -= cbChunk;
    }

    return 0;
}


/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the device which registered the data unit.
 * @param   pSSM            SSM operation handle.
 */
DECLCALLBACK(int) Item03Save(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    uint64_t u64Start = RTTimeNanoTS();

    /*
     * Put the size.
     */
    size_t cb = 512*_1M;
    int rc = SSMR3PutU32(pSSM, cb);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("Item03: PutU32 -> %Vrc\n", rc);
        return rc;
    }

    /*
     * Put 512 MB page by page.
     */
    const uint8_t *pu8Org = &gabBigMem[0];
    while (cb > 0)
    {
        rc = SSMR3PutMem(pSSM, pu8Org, PAGE_SIZE);
        if (VBOX_FAILURE(rc))
        {
            RTPrintf("Item03: PutMem(,%p,%#x) -> %Vrc\n", pu8Org, PAGE_SIZE, rc);
            return rc;
        }

        /* next */
        cb -= PAGE_SIZE;
        pu8Org += PAGE_SIZE;
        if (pu8Org > &gabBigMem[sizeof(gabBigMem)])
            pu8Org = &gabBigMem[0];
    }

    uint64_t u64Elapsed = RTTimeNanoTS() - u64Start;
    RTPrintf("tstSSM: Saved 3rd item in %RI64 ns\n", u64Elapsed);
    return 0;
}

/**
 * Prepare state load operation.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the device which registered the data unit.
 * @param   pSSM            SSM operation handle.
 */
DECLCALLBACK(int) Item03Load(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t u32Version)
{
    /*
     * Load the size.
     */
    uint32_t cb;
    int rc = SSMR3GetU32(pSSM, &cb);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("Item03: SSMR3GetU32 -> %Vrc\n", rc);
        return rc;
    }
    if (cb != 512*_1M)
    {
        RTPrintf("Item03: loaded size doesn't match the real thing. %#x != %#x\n", cb, 512*_1M);
        return VERR_GENERAL_FAILURE;
    }

    /*
     * Load the memory page by page.
     */
    const uint8_t *pu8Org = &gabBigMem[0];
    while (cb > 0)
    {
        char achPage[PAGE_SIZE];
        rc = SSMR3GetMem(pSSM, &achPage[0], PAGE_SIZE);
        if (VBOX_FAILURE(rc))
        {
            RTPrintf("Item03: SSMR3GetMem(,,%#x) -> %Vrc offset %#x\n", PAGE_SIZE, rc, 512*_1M - cb);
            return rc;
        }
        if (memcmp(achPage, pu8Org, PAGE_SIZE))
        {
            RTPrintf("Item03: compare failed. mem offset=%#x\n", 512*_1M - cb);
            return VERR_GENERAL_FAILURE;
        }

        /* next */
        cb -= PAGE_SIZE;
        pu8Org += PAGE_SIZE;
        if (pu8Org > &gabBigMem[sizeof(gabBigMem)])
            pu8Org = &gabBigMem[0];
    }

    return 0;
}


/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the device which registered the data unit.
 * @param   pSSM            SSM operation handle.
 */
DECLCALLBACK(int) Item04Save(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    uint64_t u64Start = RTTimeNanoTS();

    /*
     * Put the size.
     */
    size_t cb = 512*_1M;
    int rc = SSMR3PutU32(pSSM, cb);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("Item04: PutU32 -> %Vrc\n", rc);
        return rc;
    }

    /*
     * Put 512 MB page by page.
     */
    while (cb > 0)
    {
        rc = SSMR3PutMem(pSSM, gabPage, PAGE_SIZE);
        if (VBOX_FAILURE(rc))
        {
            RTPrintf("Item04: PutMem(,%p,%#x) -> %Vrc\n", gabPage, PAGE_SIZE, rc);
            return rc;
        }

        /* next */
        cb -= PAGE_SIZE;
    }

    uint64_t u64Elapsed = RTTimeNanoTS() - u64Start;
    RTPrintf("tstSSM: Saved 4th item in %RI64 ns\n", u64Elapsed);
    return 0;
}

/**
 * Prepare state load operation.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the device which registered the data unit.
 * @param   pSSM            SSM operation handle.
 */
DECLCALLBACK(int) Item04Load(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t u32Version)
{
    /*
     * Load the size.
     */
    uint32_t cb;
    int rc = SSMR3GetU32(pSSM, &cb);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("Item04: SSMR3GetU32 -> %Vrc\n", rc);
        return rc;
    }
    if (cb != 512*_1M)
    {
        RTPrintf("Item04: loaded size doesn't match the real thing. %#x != %#x\n", cb, 512*_1M);
        return VERR_GENERAL_FAILURE;
    }

    /*
     * Load the memory page by page.
     */
    while (cb > 0)
    {
        char achPage[PAGE_SIZE];
        rc = SSMR3GetMem(pSSM, &achPage[0], PAGE_SIZE);
        if (VBOX_FAILURE(rc))
        {
            RTPrintf("Item04: SSMR3GetMem(,,%#x) -> %Vrc offset %#x\n", PAGE_SIZE, rc, 512*_1M - cb);
            return rc;
        }
        if (memcmp(achPage, gabPage, PAGE_SIZE))
        {
            RTPrintf("Item04: compare failed. mem offset=%#x\n", 512*_1M - cb);
            return VERR_GENERAL_FAILURE;
        }

        /* next */
        cb -= PAGE_SIZE;
    }

    return 0;
}


int main(int argc, char **argv)
{
    /*
     * Init runtime and static data.
     */
    RTR3Init();
    RTPrintf("tstSSM: TESTING...\n");
    initBigMem();
    const char *pszFilename = "SSMTestSave#1";

    /*
     * Create empty VM structure and init SSM.
     */
    PVM         pVM;
    int rc = SUPInit(NULL);
    if (VBOX_SUCCESS(rc))
        rc = SUPPageAlloc((sizeof(*pVM) + PAGE_SIZE - 1) >> PAGE_SHIFT, (void **)&pVM);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("Fatal error: SUP Failure! rc=%Vrc\n", rc);
        return 1;
    }

    rc = STAMR3Init(pVM);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("Fatal error: STAMR3Init failed! rc=%Vrc\n", rc);
        return 1;
    }

/*
    rc = SSMR3Init(pVM);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("Fatal error: SSMR3Init failed! rc=%Vrc\n", rc);
        return 1;
    }
*/

    /*
     * Register a few callbacks.
     */
    rc = SSMR3Register(pVM, NULL, "SSM Testcase Data Item no.1 (all types)", 1, 0, 256,
        NULL, Item01Save, NULL,
        NULL, Item01Load, NULL);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("SSMR3Register #1 -> %Vrc\n", rc);
        return 1;
    }

    rc = SSMR3Register(pVM, NULL, "SSM Testcase Data Item no.2 (rand mem)", 2, 0, _1M * 8,
        NULL, Item02Save, NULL,
        NULL, Item02Load, NULL);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("SSMR3Register #2 -> %Vrc\n", rc);
        return 1;
    }

    rc = SSMR3Register(pVM, NULL, "SSM Testcase Data Item no.3 (big mem)", 0, 123, 512*_1M,
        NULL, Item03Save, NULL,
        NULL, Item03Load, NULL);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("SSMR3Register #3 -> %Vrc\n", rc);
        return 1;
    }

    rc = SSMR3Register(pVM, NULL, "SSM Testcase Data Item no.4 (big zero mem)", 0, 42, 512*_1M,
        NULL, Item04Save, NULL,
        NULL, Item04Load, NULL);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("SSMR3Register #4 -> %Vrc\n", rc);
        return 1;
    }

    /*
     * Attempt a save.
     */
    uint64_t u64Start = RTTimeNanoTS();
    rc = SSMR3Save(pVM, pszFilename, SSMAFTER_DESTROY, NULL, NULL);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("SSMR3Save #1 -> %Vrc\n", rc);
        return 1;
    }
    uint64_t u64Elapsed = RTTimeNanoTS() - u64Start;
    RTPrintf("tstSSM: Saved in %RI64 ns\n", u64Elapsed);

    /*
     * Attempt a load.
     */
    u64Start = RTTimeNanoTS();
    rc = SSMR3Load(pVM, pszFilename, SSMAFTER_RESUME, NULL, NULL);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("SSMR3Load #1 -> %Vrc\n", rc);
        return 1;
    }
    u64Elapsed = RTTimeNanoTS() - u64Start;
    RTPrintf("tstSSM: Loaded in %RI64 ns\n", u64Elapsed);

    /*
     * Validate it.
     */
    u64Start = RTTimeNanoTS();
    rc = SSMR3ValidateFile(pszFilename);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("SSMR3ValidateFile #1 -> %Vrc\n", rc);
        return 1;
    }
    u64Elapsed = RTTimeNanoTS() - u64Start;
    RTPrintf("tstSSM: Validated in %RI64 ns\n", u64Elapsed);

    /*
     * Open it and read.
     */
    u64Start = RTTimeNanoTS();
    PSSMHANDLE pSSM;
    rc = SSMR3Open(pszFilename, 0, &pSSM);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("SSMR3Open #1 -> %Vrc\n", rc);
        return 1;
    }
    u64Elapsed = RTTimeNanoTS() - u64Start;
    RTPrintf("tstSSM: Opened in %RI64 ns\n", u64Elapsed);

    /* negative */
    u64Start = RTTimeNanoTS();
    rc = SSMR3Seek(pSSM, "some unit that doesn't exist", 0, NULL);
    if (rc != VERR_SSM_UNIT_NOT_FOUND)
    {
        RTPrintf("SSMR3Seek #1 negative -> %Vrc\n", rc);
        return 1;
    }
    u64Elapsed = RTTimeNanoTS() - u64Start;
    RTPrintf("tstSSM: Failed seek in %RI64 ns\n", u64Elapsed);

    /* 2nd unit */
    rc = SSMR3Seek(pSSM, "SSM Testcase Data Item no.2 (rand mem)", 0, NULL);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("SSMR3Seek #1 unit 2-> %Vrc\n", rc);
        return 1;
    }
    uint32_t u32Version = 0xbadc0ded;
    rc = SSMR3Seek(pSSM, "SSM Testcase Data Item no.2 (rand mem)", 0, &u32Version);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("SSMR3Seek #1 unit 2-> %Vrc\n", rc);
        return 1;
    }
    u64Start = RTTimeNanoTS();
    rc = Item02Load(NULL, pSSM, u32Version);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("Item02Load #1 -> %Vrc\n", rc);
        return 1;
    }
    u64Elapsed = RTTimeNanoTS() - u64Start;
    RTPrintf("tstSSM: Loaded 2nd item in %RI64 ns\n", u64Elapsed);

    /* 1st unit */
    u32Version = 0xbadc0ded;
    rc = SSMR3Seek(pSSM, "SSM Testcase Data Item no.1 (all types)", 0, &u32Version);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("SSMR3Seek #1 unit 1 -> %Vrc\n", rc);
        return 1;
    }
    u64Start = RTTimeNanoTS();
    rc = Item01Load(NULL, pSSM, u32Version);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("Item01Load #1 -> %Vrc\n", rc);
        return 1;
    }
    u64Elapsed = RTTimeNanoTS() - u64Start;
    RTPrintf("tstSSM: Loaded 1st item in %RI64 ns\n", u64Elapsed);

    /* 3st unit */
    u32Version = 0xbadc0ded;
    rc = SSMR3Seek(pSSM, "SSM Testcase Data Item no.3 (big mem)", 123, &u32Version);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("SSMR3Seek #3 unit 1 -> %Vrc\n", rc);
        return 1;
    }
    u64Start = RTTimeNanoTS();
    rc = Item03Load(NULL, pSSM, u32Version);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("Item01Load #3 -> %Vrc\n", rc);
        return 1;
    }
    u64Elapsed = RTTimeNanoTS() - u64Start;
    RTPrintf("tstSSM: Loaded 3rd item in %RI64 ns\n", u64Elapsed);

    /* close */
    rc = SSMR3Close(pSSM);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("SSMR3Close #1 -> %Vrc\n", rc);
        return 1;
    }

    RTPrintf("tstSSM: SUCCESS\n");
    return 0;
}

