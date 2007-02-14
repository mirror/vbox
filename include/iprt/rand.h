/** @file
 * InnoTek Portable Runtime - Random Numbers and Byte Streams.
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

#ifndef __iprt_rand_h__
#define __iprt_rand_h__

#include <iprt/cdefs.h>
#include <iprt/types.h>

__BEGIN_DECLS

/** @defgroup grp_rt_rand       RTRand - Random Numbers and Byte Streams
 * @ingroup grp_rt
 * @{
 */

/**
 * Fills a buffer with random bytes.
 *
 * @param   pv  Where to store the random bytes.
 * @param   cb  Number of bytes to generate.
 */
RTDECL(void) RTRandBytes(void *pv, size_t cb);

/**
 * Generate a 32-bit signed random number in the set [i32First..i32Last].
 * 
 * @returns The random number.
 * @param   i32First    First number in the set.
 * @param   i32Last     Last number in the set.
 */
RTDECL(int32_t) RTRandS32Ex(int32_t i32First, int32_t i32Last);

/**
 * Generate a 32-bit signed random number.
 * 
 * @returns The random number.
 */
RTDECL(int32_t) RTRandS32(void);

/**
 * Generate a 32-bit unsigned random number in the set [u32First..u32Last].
 * 
 * @returns The random number.
 * @param   u32First    First number in the set.
 * @param   u32Last     Last number in the set.
 */
RTDECL(uint32_t) RTRandU32Ex(uint32_t u32First, uint32_t u32Last);

/**
 * Generate a 32-bit unsigned random number.
 * 
 * @returns The random number.
 */
RTDECL(uint32_t) RTRandU32(void);

/**
 * Generate a 32-bit signed random number in the set [i32First..i32Last].
 * 
 * @returns The random number.
 * @param   i32First    First number in the set.
 * @param   i32Last     Last number in the set.
 */
RTDECL(int64_t) RTRandS64Ex(int64_t i64First, int64_t i64Last);

/**
 * Generate a 64-bit signed random number.
 * 
 * @returns The random number.
 */
RTDECL(int64_t) RTRandS64(void);

/**
 * Generate a 64-bit unsigned random number in the set [u64First..u64Last].
 * 
 * @returns The random number.
 * @param   u64First    First number in the set.
 * @param   u64Last     Last number in the set.
 */
RTDECL(uint64_t) RTRandU64Ex(uint64_t u64First, uint64_t u64Last);

/**
 * Generate a 64-bit unsigned random number.
 * 
 * @returns The random number.
 */
RTDECL(uint64_t) RTRandU64(void);

/** @} */

__END_DECLS


#endif

