/* $Id$ */
/** @file
 * innotek Portable Runtime Testcase - Inlined Bit Operations.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/asm.h>
#include <iprt/stream.h>
#include <iprt/string.h>


int main()
{
    int rcRet = 0;
    int i;
    int j;
    int k;
    RTPrintf("tstBitOperations: TESTING\n");


    /*
     * Tests
     */
    uint32_t au32[4];
    #define MAP_CLEAR(a) memset(&a,    0, sizeof(a));
    #define MAP_SET(a)   memset(&a, 0xff, sizeof(a));
    #define DUMP()       RTPrintf("au32={%08x,%08x,%08x,%08x}\n", au32[0], au32[1], au32[2], au32[3])
    #define CHECK(expr)  do { if (!(expr)) { RTPrintf("tstBitOperations: error line %d: %s\n", __LINE__, #expr); DUMP(); rcRet++; } } while (0)
    #define CHECK_BIT(expr,  b1)            do { if (!(expr)) { RTPrintf("tstBitOperations: error line %d, b1=%d: %s\n", __LINE__, b1, #expr); rcRet++; } } while (0)
    #define CHECK_BIT2(expr, b1, b2)        do { if (!(expr)) { RTPrintf("tstBitOperations: error line %d, b1=%d b2=%d: %s\n", __LINE__, b1, b2, #expr); rcRet++; } } while (0)
    #define CHECK_BIT3(expr, b1, b2, b3)    do { if (!(expr)) { RTPrintf("tstBitOperations: error line %d, b1=%d b2=%d b3=%d: %s\n", __LINE__, b1, b2, b3, #expr); rcRet++; } } while (0)

    /* set */
    MAP_CLEAR(au32);
    ASMBitSet(&au32[0], 0);
    ASMBitSet(&au32[0], 31);
    ASMBitSet(&au32[0], 65);
    CHECK(au32[0] == 0x80000001U);
    CHECK(au32[2] == 0x00000002U);
    CHECK(ASMBitTestAndSet(&au32[0], 0)   && au32[0] == 0x80000001U);
    CHECK(!ASMBitTestAndSet(&au32[0], 16) && au32[0] == 0x80010001U);
    CHECK(ASMBitTestAndSet(&au32[0], 16)  && au32[0] == 0x80010001U);
    CHECK(!ASMBitTestAndSet(&au32[0], 80) && au32[2] == 0x00010002U);

    MAP_CLEAR(au32);
    ASMAtomicBitSet(&au32[0], 0);
    ASMAtomicBitSet(&au32[0], 30);
    ASMAtomicBitSet(&au32[0], 64);
    CHECK(au32[0] == 0x40000001U);
    CHECK(au32[2] == 0x00000001U);
    CHECK(ASMAtomicBitTestAndSet(&au32[0], 0)   && au32[0] == 0x40000001U);
    CHECK(!ASMAtomicBitTestAndSet(&au32[0], 16) && au32[0] == 0x40010001U);
    CHECK(ASMAtomicBitTestAndSet(&au32[0], 16)  && au32[0] == 0x40010001U);
    CHECK(!ASMAtomicBitTestAndSet(&au32[0], 80) && au32[2] == 0x00010001U);

    /* clear */
    MAP_SET(au32);
    ASMBitClear(&au32[0], 0);
    ASMBitClear(&au32[0], 31);
    ASMBitClear(&au32[0], 65);
    CHECK(au32[0] == ~0x80000001U);
    CHECK(au32[2] == ~0x00000002U);
    CHECK(!ASMBitTestAndClear(&au32[0], 0)   && au32[0] == ~0x80000001U);
    CHECK(ASMBitTestAndClear(&au32[0], 16)   && au32[0] == ~0x80010001U);
    CHECK(!ASMBitTestAndClear(&au32[0], 16)  && au32[0] == ~0x80010001U);
    CHECK(ASMBitTestAndClear(&au32[0], 80)   && au32[2] == ~0x00010002U);

    MAP_SET(au32);
    ASMAtomicBitClear(&au32[0], 0);
    ASMAtomicBitClear(&au32[0], 30);
    ASMAtomicBitClear(&au32[0], 64);
    CHECK(au32[0] == ~0x40000001U);
    CHECK(au32[2] == ~0x00000001U);
    CHECK(!ASMAtomicBitTestAndClear(&au32[0], 0)   && au32[0] == ~0x40000001U);
    CHECK(ASMAtomicBitTestAndClear(&au32[0], 16)   && au32[0] == ~0x40010001U);
    CHECK(!ASMAtomicBitTestAndClear(&au32[0], 16)  && au32[0] == ~0x40010001U);
    CHECK(ASMAtomicBitTestAndClear(&au32[0], 80)   && au32[2] == ~0x00010001U);

    /* toggle */
    MAP_SET(au32);
    ASMBitToggle(&au32[0], 0);
    ASMBitToggle(&au32[0], 31);
    ASMBitToggle(&au32[0], 65);
    ASMBitToggle(&au32[0], 47);
    ASMBitToggle(&au32[0], 47);
    CHECK(au32[0] == ~0x80000001U);
    CHECK(au32[2] == ~0x00000002U);
    CHECK(!ASMBitTestAndToggle(&au32[0], 0)   && au32[0] == ~0x80000000U);
    CHECK(ASMBitTestAndToggle(&au32[0], 0)    && au32[0] == ~0x80000001U);
    CHECK(ASMBitTestAndToggle(&au32[0], 16)   && au32[0] == ~0x80010001U);
    CHECK(!ASMBitTestAndToggle(&au32[0], 16)  && au32[0] == ~0x80000001U);
    CHECK(ASMBitTestAndToggle(&au32[0], 80)   && au32[2] == ~0x00010002U);

    MAP_SET(au32);
    ASMAtomicBitToggle(&au32[0], 0);
    ASMAtomicBitToggle(&au32[0], 30);
    ASMAtomicBitToggle(&au32[0], 64);
    ASMAtomicBitToggle(&au32[0], 47);
    ASMAtomicBitToggle(&au32[0], 47);
    CHECK(au32[0] == ~0x40000001U);
    CHECK(au32[2] == ~0x00000001U);
    CHECK(!ASMAtomicBitTestAndToggle(&au32[0], 0)   && au32[0] == ~0x40000000U);
    CHECK(ASMAtomicBitTestAndToggle(&au32[0], 0)    && au32[0] == ~0x40000001U);
    CHECK(ASMAtomicBitTestAndToggle(&au32[0], 16)   && au32[0] == ~0x40010001U);
    CHECK(!ASMAtomicBitTestAndToggle(&au32[0], 16)  && au32[0] == ~0x40000001U);
    CHECK(ASMAtomicBitTestAndToggle(&au32[0], 80)   && au32[2] == ~0x00010001U);

    /* test bit. */
    for (i = 0; i < 128; i++)
    {
        MAP_SET(au32);
        CHECK_BIT(ASMBitTest(&au32[0], i), i);
        ASMBitToggle(&au32[0], i);
        CHECK_BIT(!ASMBitTest(&au32[0], i), i);
        CHECK_BIT(!ASMBitTestAndToggle(&au32[0], i), i);
        CHECK_BIT(ASMBitTest(&au32[0], i), i);
        CHECK_BIT(ASMBitTestAndToggle(&au32[0], i), i);
        CHECK_BIT(!ASMBitTest(&au32[0], i), i);

        MAP_SET(au32);
        CHECK_BIT(ASMBitTest(&au32[0], i), i);
        ASMAtomicBitToggle(&au32[0], i);
        CHECK_BIT(!ASMBitTest(&au32[0], i), i);
        CHECK_BIT(!ASMAtomicBitTestAndToggle(&au32[0], i), i);
        CHECK_BIT(ASMBitTest(&au32[0], i), i);
        CHECK_BIT(ASMAtomicBitTestAndToggle(&au32[0], i), i);
        CHECK_BIT(!ASMBitTest(&au32[0], i), i);

    }

    /* bit searching */
    MAP_SET(au32);
    CHECK(ASMBitFirstClear(&au32[0], sizeof(au32) * 8) == -1);
    CHECK(ASMBitFirstSet(&au32[0], sizeof(au32) * 8) == 0);

    ASMBitClear(&au32[0], 1);
    CHECK(ASMBitFirstClear(&au32[0], sizeof(au32) * 8) == 1);
    CHECK(ASMBitFirstSet(&au32[0], sizeof(au32) * 8) == 0);

    MAP_SET(au32);
    ASMBitClear(&au32[0], 95);
    CHECK(ASMBitFirstClear(&au32[0], sizeof(au32) * 8) == 95);
    CHECK(ASMBitFirstSet(&au32[0], sizeof(au32) * 8) == 0);

    MAP_SET(au32);
    ASMBitClear(&au32[0], 127);
    CHECK(ASMBitFirstClear(&au32[0], sizeof(au32) * 8) == 127);
    CHECK(ASMBitFirstSet(&au32[0], sizeof(au32) * 8) == 0);
    CHECK(ASMBitNextSet(&au32[0], sizeof(au32) * 8, 0) == 1);
    CHECK(ASMBitNextSet(&au32[0], sizeof(au32) * 8, 1) == 2);
    CHECK(ASMBitNextSet(&au32[0], sizeof(au32) * 8, 2) == 3);


    MAP_SET(au32);
    CHECK(ASMBitNextClear(&au32[0], sizeof(au32) * 8, 0) == -1);
    ASMBitClear(&au32[0], 32);
    CHECK(ASMBitNextClear(&au32[0], sizeof(au32) * 8, 32) == -1);
    ASMBitClear(&au32[0], 88);
    CHECK(ASMBitNextClear(&au32[0], sizeof(au32) * 8,  57) ==  88);

    MAP_SET(au32);
    ASMBitClear(&au32[0], 31);
    ASMBitClear(&au32[0], 57);
    ASMBitClear(&au32[0], 88);
    ASMBitClear(&au32[0], 101);
    ASMBitClear(&au32[0], 126);
    ASMBitClear(&au32[0], 127);
    CHECK(ASMBitFirstClear(&au32[0], sizeof(au32) * 8) == 31);
    CHECK(ASMBitNextClear(&au32[0], sizeof(au32) * 8,  31) ==  57);
    CHECK(ASMBitNextClear(&au32[0], sizeof(au32) * 8,  57) ==  88);
    CHECK(ASMBitNextClear(&au32[0], sizeof(au32) * 8,  88) == 101);
    CHECK(ASMBitNextClear(&au32[0], sizeof(au32) * 8, 101) == 126);
    CHECK(ASMBitNextClear(&au32[0], sizeof(au32) * 8, 126) == 127);
    CHECK(ASMBitNextClear(&au32[0], sizeof(au32) * 8, 127) == -1);

    CHECK(ASMBitNextSet(&au32[0], sizeof(au32) * 8, 29) == 30);
    CHECK(ASMBitNextSet(&au32[0], sizeof(au32) * 8, 30) == 32);

    MAP_CLEAR(au32);
    for (i = 1; i < 128; i++)
        CHECK_BIT(ASMBitNextClear(&au32[0], sizeof(au32) * 8, i - 1) == i, i);
    for (i = 0; i < 128; i++)
    {
        MAP_SET(au32);
        ASMBitClear(&au32[0], i);
        CHECK_BIT(ASMBitFirstClear(&au32[0], sizeof(au32) * 8) == i, i);
        for (j = 0; j < i; j++)
            CHECK_BIT(ASMBitNextClear(&au32[0], sizeof(au32) * 8, j) == i, i);
        for (j = i; j < 128; j++)
            CHECK_BIT(ASMBitNextClear(&au32[0], sizeof(au32) * 8, j) == -1, i);
    }

    /* clear range. */
    MAP_SET(au32);
    ASMBitClearRange(&au32, 0, 128);
    CHECK(!au32[0] && !au32[1] && !au32[2] && !au32[3]);
    for (i = 0; i < 128; i++)
    {
        for (j = i + 1; j <= 128; j++)
        {
            MAP_SET(au32);
            ASMBitClearRange(&au32, i, j);
            for (k = 0; k < i; k++)
                CHECK_BIT3(ASMBitTest(&au32[0], k), i, j, k);
            for (k = i; k < j; k++)
                CHECK_BIT3(!ASMBitTest(&au32[0], k), i, j, k);
            for (k = j; k < 128; k++)
                CHECK_BIT3(ASMBitTest(&au32[0], k), i, j, k);
        }
    }

    /* searching for set bits. */
    MAP_CLEAR(au32);
    CHECK(ASMBitFirstSet(&au32[0], sizeof(au32) * 8) == -1);

    ASMBitSet(&au32[0], 65);
    CHECK(ASMBitFirstSet(&au32[0], sizeof(au32) * 8) == 65);
    CHECK(ASMBitNextSet(&au32[0], sizeof(au32) * 8, 65) == -1);
    for (i = 0; i < 65; i++)
        CHECK(ASMBitNextSet(&au32[0], sizeof(au32) * 8, i) == 65);
    for (i = 65; i < 128; i++)
        CHECK(ASMBitNextSet(&au32[0], sizeof(au32) * 8, i) == -1);

    ASMBitSet(&au32[0], 17);
    CHECK(ASMBitFirstSet(&au32[0], sizeof(au32) * 8) == 17);
    CHECK(ASMBitNextSet(&au32[0], sizeof(au32) * 8, 17) == 65);
    for (i = 0; i < 16; i++)
        CHECK(ASMBitNextSet(&au32[0], sizeof(au32) * 8, i) == 17);
    for (i = 17; i < 65; i++)
        CHECK(ASMBitNextSet(&au32[0], sizeof(au32) * 8, i) == 65);

    MAP_SET(au32);
    for (i = 1; i < 128; i++)
        CHECK_BIT(ASMBitNextSet(&au32[0], sizeof(au32) * 8, i - 1) == i, i);
    for (i = 0; i < 128; i++)
    {
        MAP_CLEAR(au32);
        ASMBitSet(&au32[0], i);
        CHECK_BIT(ASMBitFirstSet(&au32[0], sizeof(au32) * 8) == i, i);
        for (j = 0; j < i; j++)
            CHECK_BIT(ASMBitNextSet(&au32[0], sizeof(au32) * 8, j) == i, i);
        for (j = i; j < 128; j++)
            CHECK_BIT(ASMBitNextSet(&au32[0], sizeof(au32) * 8, j) == -1, i);
    }


    CHECK(ASMBitLastSetU32(0) == 0);
    CHECK(ASMBitLastSetU32(1) == 1);
    CHECK(ASMBitLastSetU32(0x80000000) == 32);
    CHECK(ASMBitLastSetU32(0xffffffff) == 32);
    CHECK(ASMBitLastSetU32(RT_BIT(23) | RT_BIT(11)) == 24);
    for (i = 0; i < 32; i++)
        CHECK(ASMBitLastSetU32(1 << i) == (unsigned)i + 1);

    CHECK(ASMBitFirstSetU32(0) == 0);
    CHECK(ASMBitFirstSetU32(1) == 1);
    CHECK(ASMBitFirstSetU32(0x80000000) == 32);
    CHECK(ASMBitFirstSetU32(0xffffffff) == 1);
    CHECK(ASMBitFirstSetU32(RT_BIT(23) | RT_BIT(11)) == 12);
    for (i = 0; i < 32; i++)
        CHECK(ASMBitFirstSetU32(1 << i) == (unsigned)i + 1);

    /*
     * Summary
     */
    if (!rcRet)
        RTPrintf("tstBitOperations: SUCCESS\n");
    else
        RTPrintf("tstBitOperations: FAILURE - %d errors\n", rcRet);
    return rcRet;
}
