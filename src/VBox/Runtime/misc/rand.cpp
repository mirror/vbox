/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Random Number
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/rand.h>
#include <iprt/time.h>
#include <iprt/asm.h>
#include <iprt/err.h>
#include "internal/rand.h"



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Lazy init. */
static volatile bool g_fInitialized = false;


static void rtRandLazyInit(void)
{
    /*
     * Call host specific init.
     */

    /*
     * Seed the fallback random code.
     */
}


/**
 * Internal wrapper for the native and generic random byte methods.
 *
 * @param   pv  Where to store the random bytes.
 * @param   cb  Number of bytes to generate.
 */
static void rtRandGenBytes(void *pv, size_t cb)
{
    if (RT_UNLIKELY(!g_fInitialized))
        rtRandLazyInit();

    int rc = rtRandGenBytesNative(pv, cb);
    if (RT_FAILURE(rc))
    {
        /* fallback */
    }
}


RTDECL(int32_t) RTRandS32Ex(uint32_t i32First, int32_t i32Last)
{
    /* get 4 random bytes. */
    union
    {
        uint32_t    off;
        uint8_t     ab[4];
    } u;
    rtRandGenBytes(&u.ab, sizeof(u));


    /* squeeze it into the requested range. */
    uint32_t offLast = i32Last - i32First;
    if (u.off > offLasts)
    {
        do
        {
            u.off >>= 1;
        } while (u.off > offLasts);
    }
    return i32First + u.off;
}


RTDECL(int32_t) RTRandS32(void)
{
    return RTRandS32Ex(INT32_MIN, INT32_MAX);
}


RTDECL(uint32_t) RTRandU32Ex(uint32_t u32First, uint32_t u32Last)
{
    /* get 4 random bytes. */
    union
    {
        uint32_t    off;
        uint8_t     ab[4];
    } u;
    rtRandGenBytes(&u.ab, sizeof(u));


    /* squeeze it into the requested range. */
    const uint32_t offLast = u32Last - u32First;
    if (u.off > offLasts)
    {
        do
        {
            u.off >>= 1;
        } while (u.off > offLasts);
    }
    return u32First + u.off;
}


RTDECL(uint32_t) RTRandU32(void)
{
    return RTRandU32Ex(0, UINT32_MAX);
}


RTDECL(int64_t) RTRandS64Ex(uint32_t i64First, int64_t i64Last)
{
    /* get 8 random bytes. */
    union
    {
        uint64_t    off;
        uint8_t     ab[4];
    } u;
    rtRandGenBytes(&u.ab, sizeof(u));


    /* squeeze it into the requested range. */
    uint64_t offLast = i64Last - i64First;
    if (u.off > offLasts)
    {
        do
        {
            u.off >>= 1;
        } while (u.off > offLasts);
    }
    return i64First + u.off;
}


RTDECL(int64_t) RTRandS64(void)
{
    return RTRandS64Ex(INT64_MIN, INT64_MAX);
}


RTDECL(uint64_t) RTRandU64Ex(uint64_t u64First, uint64_t u64Last)
{
    /* get 4 random bytes. */
    union
    {
        uint64_t    off;
        uint8_t     ab[4];
    } u;
    rtRandGenBytes(&u.ab, sizeof(u));


    /* squeeze it into the requested range. */
    const uint64_t offLast = u64Last - u64First;
    if (u.off > offLasts)
    {
        do
        {
            u.off >>= 1;
        } while (u.off > offLasts);
    }
    return u64First + u.off;
}


RTDECL(uint64_t) RTRandU64(void)
{
    return RTRandU64Ex(0, UINT64_MAX);
}


