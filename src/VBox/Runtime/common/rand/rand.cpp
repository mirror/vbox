/* $Id$ */
/** @file
 * IPRT - Random Number
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
#include <iprt/rand.h>
#include <iprt/time.h>
#include <iprt/asm.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include <iprt/thread.h>
#include "internal/rand.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Lazy init. */
static volatile bool    g_fInitialized = false;
/** The context variable for the fallback path. */
static uint32_t         g_u32Ctx;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static uint32_t rtRandGenBytesFallbackU31(uint32_t *pCtx);


/**
 * Lazy initialization of the native and fallback random byte sources.
 *
 */
static void rtRandLazyInit(void)
{
    /*
     * Seed the fallback random code.
     */
    g_u32Ctx = (uint32_t)(ASMReadTSC() >> 8);

    /*
     * Call host specific init.
     */
    rtRandLazyInitNative();
    g_fInitialized = true;
}


/**
 * Internal wrapper for the native and generic random byte methods.
 *
 * @param   pv  Where to store the random bytes.
 * @param   cb  Number of bytes to generate.
 */
static void rtRandGenBytes(void *pv, size_t cb)
{
    Assert(cb);
    if (RT_UNLIKELY(!g_fInitialized))
        rtRandLazyInit();

    int rc = rtRandGenBytesNative(pv, cb);
    if (RT_FAILURE(rc))
        rtRandGenBytesFallback(pv, cb);
}


/**
 * Fills a buffer with random bytes.
 *
 * @param   pv  Where to store the random bytes.
 * @param   cb  Number of bytes to generate.
 */
RTDECL(void) RTRandBytes(void *pv, size_t cb) RT_NO_THROW
{
    if (cb)
        rtRandGenBytes(pv, cb);
}


/**
 * Generate a 32-bit unsigned random number in the set [u32First..u32Last].
 *
 * @returns The random number.
 * @param   u32First    First number in the set.
 * @param   u32Last     Last number in the set.
 */
RTDECL(uint32_t) RTRandU32Ex(uint32_t u32First, uint32_t u32Last) RT_NO_THROW
{
    union
    {
        uint32_t    off;
        uint8_t     ab[5];
    } u;

    const uint32_t offLast = u32Last - u32First;
    if (offLast == UINT32_MAX)
        /* get 4 random bytes and return them raw. */
        rtRandGenBytes(&u.ab[0], sizeof(u.off));
    else if (!(offLast & UINT32_C(0xf0000000)))
    {
        /* get 4 random bytes and do simple squeeze. */
        rtRandGenBytes(&u.ab[0], sizeof(u.off));
        u.off %= offLast + 1;
        u.off += u32First;
    }
    else
    {
        /* get 5 random bytes and do shifted squeeze. (this ain't perfect) */
        rtRandGenBytes(&u.ab[0], sizeof(u.ab));
        u.off %= (offLast >> 4) + 1;
        u.off <<= 4;
        u.off |= u.ab[4] & 0xf;
        if (u.off > offLast)
            u.off = offLast;
        u.off += u32First;
    }
    return u.off;
}


/**
 * Generate a 32-bit unsigned random number.
 *
 * @returns The random number.
 */
RTDECL(uint32_t) RTRandU32(void) RT_NO_THROW
{
    return RTRandU32Ex(0, UINT32_MAX);
}


/**
 * Generate a 32-bit signed random number in the set [i32First..i32Last].
 *
 * @returns The random number.
 * @param   i32First    First number in the set.
 * @param   i32Last     Last number in the set.
 */
RTDECL(int32_t) RTRandS32Ex(int32_t i32First, int32_t i32Last) RT_NO_THROW
{
    if (i32First == INT32_MIN && i32Last == INT32_MAX)
        return (int32_t)RTRandU32Ex(0, UINT32_MAX);
    return RTRandU32Ex(0, i32Last - i32First) + i32First;
}


/**
 * Generate a 32-bit signed random number.
 *
 * @returns The random number.
 */
RTDECL(int32_t) RTRandS32(void) RT_NO_THROW
{
    return (int32_t)RTRandU32Ex(0, UINT32_MAX);
}


/**
 * Generate a 64-bit unsigned random number in the set [u64First..u64Last].
 *
 * @returns The random number.
 * @param   u64First    First number in the set.
 * @param   u64Last     Last number in the set.
 */
RTDECL(uint64_t) RTRandU64Ex(uint64_t u64First, uint64_t u64Last) RT_NO_THROW
{
    union
    {
        uint64_t    off;
        uint32_t    off32;
        uint8_t     ab[9];
    } u;

    const uint64_t offLast = u64Last - u64First;
    if (offLast == UINT64_MAX)
        /* get 8 random bytes and return them raw. */
        rtRandGenBytes(&u.ab[0], sizeof(u.off));
    else if (!(offLast & UINT64_C(0xf000000000000000)))
    {
        /* get 8 random bytes and do simple squeeze. */
        rtRandGenBytes(&u.ab[0], sizeof(u.off));
        u.off %= offLast + 1;
        u.off += u64First;
    }
    else
    {
        /* get 9 random bytes and do shifted squeeze. (this ain't perfect) */
        rtRandGenBytes(&u.ab[0], sizeof(u.ab));
        u.off %= (offLast >> 4) + 1;
        u.off <<= 4;
        u.off |= u.ab[8] & 0xf;
        if (u.off > offLast)
            u.off = offLast;
        u.off += u64First;
    }
    return u.off;
}


/**
 * Generate a 64-bit unsigned random number.
 *
 * @returns The random number.
 */
RTDECL(uint64_t) RTRandU64(void) RT_NO_THROW
{
    return RTRandU64Ex(0, UINT64_MAX);
}


/**
 * Generate a 64-bit signed random number in the set [i64First..i64Last].
 *
 * @returns The random number.
 * @param   i64First    First number in the set.
 * @param   i64Last     Last number in the set.
 */
RTDECL(int64_t) RTRandS64Ex(int64_t i64First, int64_t i64Last) RT_NO_THROW
{
    if (i64First == INT64_MIN && i64Last == INT64_MAX)
        return (int64_t)RTRandU64Ex(0, UINT64_MAX);
    return (int64_t)RTRandU64Ex(0, i64Last - i64First) + i64First;
}


/**
 * Generate a 64-bit signed random number.
 *
 * @returns The random number.
 */
RTDECL(int64_t) RTRandS64(void) RT_NO_THROW
{
    return (int64_t)RTRandU64Ex(0, UINT64_MAX);
}


/**
 * Fallback random byte source.
 *
 * @param   pv  Where to store the random bytes.
 * @param   cb  Number of bytes to generate.
 */
void rtRandGenBytesFallback(void *pv, size_t cb) RT_NO_THROW
{
    uint64_t u64Last = 0;
    uint8_t *pb = (uint8_t *)pv;
    for (unsigned i = 0;; i++)
    {
        uint32_t u32 = rtRandGenBytesFallbackU31(&g_u32Ctx);

        *pb++ = (uint8_t)u32;
        if (!--cb)
            break;

        u32 >>= 8;
        *pb++ = (uint8_t)u32;
        if (!--cb)
            break;

        u32 >>= 8;
        *pb++ = (uint8_t)u32;
        if (!--cb)
            break;

        /*
         * Is this really a good idea? Looks like we cannot
         * quite trust the lower bits here for instance...
         */
        if (!(i % 3))
        {
            uint64_t u64 = ASMReadTSC();
            uint64_t u64Delta = u64 - u64Last;
            if (u64Delta > 0xff)
            {
                u32 >>= 8;
                *pb++ = ((uint8_t)u64 >> 5) | (u32 << 3);
                if (!--cb)
                    break;
                u64Last = u64;
            }
        }
    }
}


/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* The core of:
__FBSDID("$FreeBSD: /repoman/r/ncvs/src/lib/libc/stdlib/rand.c,v 1.16 2007/01/09 00:28:10 imp Exp $");
*/

/**
 * Generates an unsigned 31 bit pseudo random number.
 *
 * @returns pseudo random number.
 * @param   pCtx    The context.
 */
static uint32_t rtRandGenBytesFallbackU31(uint32_t *pCtx)
{
    /*
     * Compute x = (7^5 * x) mod (2^31 - 1)
     * without overflowing 31 bits:
     *      (2^31 - 1) = 127773 * (7^5) + 2836
     *
     * From "Random number generators: good ones are hard to find",  Park and
     * Miller, Communications of the ACM, vol. 31, no. 10, October 1988, p. 1195.
     */
    uint32_t Ctx = *pCtx;
    if (!Ctx) /* must not be zero. */
        Ctx = 0x20070212;
    uint32_t Hi = Ctx / 127773;
    uint32_t Lo = Ctx % 127773;
    int32_t x = 16807 * Lo  -  2836 * Hi;
    if (x < 0)
        x += INT32_MAX;
    *pCtx = x;
    return x % INT32_MAX;
}

