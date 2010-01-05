/* $Id$ */
/** @file
 * IPRT - Internal RTRand header
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

#ifndef ___internal_rand_h
#define ___internal_rand_h

#include <iprt/types.h>
#include <iprt/critsect.h>

/** Pointer to a random number generator instance. */
typedef struct RTRANDINT *PRTRANDINT;

/**
 * Random number generator instance.
 *
 * @remarks Not sure if it makes sense to have three random getters...
 */
typedef struct RTRANDINT
{
    /** Magic value (RTRANDINT_MAGIC). */
    uint32_t    u32Magic;
#if 0 /** @todo later. */
    /** Fast mutex semaphore that serializes the access, this is optional. */
    PRTCRITSECT pCritSect;
#endif

    /**
     * Generates random bytes.
     *
     * @param   pThis       Pointer to the instance data.
     * @param   pb          Where to store the bytes.
     * @param   cb          The number of bytes to produce.
     */
    DECLCALLBACKMEMBER(void ,    pfnGetBytes)(PRTRANDINT pThis, uint8_t *pb, size_t cb);

    /**
     * Generates a unsigned 32-bit random number.
     *
     * @returns The random number.
     * @param   pThis       Pointer to the instance data.
     * @param   u32First    The first number in the range.
     * @param   u32Last     The last number in the range (i.e. inclusive).
     */
    DECLCALLBACKMEMBER(uint32_t, pfnGetU32)(PRTRANDINT pThis, uint32_t u32First, uint32_t u32Last);

    /**
     * Generates a unsigned 64-bit random number.
     *
     * @returns The random number.
     * @param   pThis       Pointer to the instance data.
     * @param   u64First    The first number in the range.
     * @param   u64Last     The last number in the range (i.e. inclusive).
     */
    DECLCALLBACKMEMBER(uint64_t, pfnGetU64)(PRTRANDINT pThis, uint64_t u64First, uint64_t u64Last);

    /**
     * Generic seeding.
     *
     * @returns IPRT status code.
     * @retval  VERR_NOT_SUPPORTED if it isn't a pseudo generator.
     *
     * @param   pThis       Pointer to the instance data.
     * @param   u64Seed     The seed.
     */
    DECLCALLBACKMEMBER(int, pfnSeed)(PRTRANDINT pThis, uint64_t u64Seed);

    /**
     * Save the current state of a pseudo generator.
     *
     * This can be use to save the state so it can later be resumed at the same
     * position.
     *
     * @returns IPRT status code.
     * @retval  VINF_SUCCESS on success. *pcbState contains the length of the
     *          returned string and pszState contains the state string.
     * @retval  VERR_BUFFER_OVERFLOW if the supplied buffer is too small. *pcbState
     *          will contain the necessary buffer size.
     * @retval  VERR_NOT_SUPPORTED by non-psuedo generators.
     *
     * @param   pThis       Pointer to the instance data.
     * @param   pszState    Where to store the state. The returned string will be
     *                      null terminated and printable.
     * @param   pcbState    The size of the buffer pszState points to on input, the
     *                      size required / used on return (including the
     *                      terminator, thus the 'cb' instead of 'cch').
     */
    DECLCALLBACKMEMBER(int, pfnSaveState)(PRTRANDINT pThis, char *pszState, size_t *pcbState);

    /**
     * Restores the state of a pseudo generator.
     *
     * The state must've been obtained using pfnGetState.
     *
     * @returns IPRT status code.
     * @retval  VERR_PARSE_ERROR if the state string is malformed.
     * @retval  VERR_NOT_SUPPORTED by non-psuedo generators.
     *
     * @param   pThis       Pointer to the instance data.
     * @param   pszState    The state to load.
     */
    DECLCALLBACKMEMBER(int, pfnRestoreState)(PRTRANDINT pThis, char const *pszState);

    /**
     * Destroys the instance.
     *
     * The callee is responsible for freeing all resources, including
     * the instance data.
     *
     * @returns IPRT status code. State undefined on failure.
     * @param   pThis       Pointer to the instance data.
     */
    DECLCALLBACKMEMBER(int, pfnDestroy)(PRTRANDINT pThis);

    /** Union containing the specific state info for each generator. */
    union
    {
        struct RTRandParkMiller
        {
            /** The context. */
            uint32_t    u32Ctx;
            /** The number of single bits used to fill in the 31st bit. */
            uint32_t    u32Bits;
            /** The number bits in u32Bits. */
            uint32_t    cBits;
        } ParkMiller;

        struct RTRandFile
        {
            /** The file handle. */
            RTFILE      hFile;
        } File;
    } u;
} RTRANDINT;


RT_C_DECLS_BEGIN

/**
 * Initialize OS facilities for generating random bytes.
 */
void rtRandLazyInitNative(void);

/**
 * Generate random bytes using OS facilities.
 *
 * @returns VINF_SUCCESS on success, some error status code on failure.
 * @param   pv      Where to store the random bytes.
 * @param   cb      How many random bytes to store.
 */
int rtRandGenBytesNative(void *pv, size_t cb);

void rtRandGenBytesFallback(void *pv, size_t cb) RT_NO_THROW;

DECLCALLBACK(void)      rtRandAdvSynthesizeBytesFromU32(PRTRANDINT pThis, uint8_t *pb, size_t cb);
DECLCALLBACK(void)      rtRandAdvSynthesizeBytesFromU64(PRTRANDINT pThis, uint8_t *pb, size_t cb);
DECLCALLBACK(uint32_t)  rtRandAdvSynthesizeU32FromBytes(PRTRANDINT pThis, uint32_t u32First, uint32_t u32Last);
DECLCALLBACK(uint32_t)  rtRandAdvSynthesizeU32FromU64(PRTRANDINT pThis, uint32_t u32First, uint32_t u32Last);
DECLCALLBACK(uint64_t)  rtRandAdvSynthesizeU64FromBytes(PRTRANDINT pThis, uint64_t u64First, uint64_t u64Last);
DECLCALLBACK(uint64_t)  rtRandAdvSynthesizeU64FromU32(PRTRANDINT pThis, uint64_t u64First, uint64_t u64Last);
DECLCALLBACK(int)       rtRandAdvStubSeed(PRTRANDINT pThis, uint64_t u64Seed);
DECLCALLBACK(int)       rtRandAdvStubSaveState(PRTRANDINT pThis, char *pszState, size_t *pcbState);
DECLCALLBACK(int)       rtRandAdvStubRestoreState(PRTRANDINT pThis, char const *pszState);
DECLCALLBACK(int)       rtRandAdvDefaultDestroy(PRTRANDINT pThis);

RT_C_DECLS_END

#endif

