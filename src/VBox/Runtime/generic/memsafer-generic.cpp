/* $Id$ */
/** @file
 * IPRT - Memory Allocate for Sensitive Data, generic heap-based implementation.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
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
#include "internal/iprt.h"
#include <iprt/memsafer.h>

#include <iprt/assert.h>
#include <iprt/string.h>
#if defined(IN_SUP_R3) && defined(VBOX) && !defined(RT_NO_GIP)
# include <iprt/param.h>
# include <VBox/sup.h>
#endif /* IN_SUP_R3 */


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Allocation size alignment. */
#define RTMEMSAFER_ALIGN        16
/** Padding after the block to avoid small overruns. */
#define RTMEMSAFER_PAD_BEFORE   96
/** Padding after the block to avoid small underruns. */
#define RTMEMSAFER_PAD_AFTER    32

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * Supported allocation methods.
 */
typedef enum RTMEMSAFERALLOCMETHOD
{
    /** Invalid method. */
    RTMEMSAFERALLOCMETHOD_INVALID = 0,
    /** RTMem{Alloc|Free} methods, least secure!. */
    RTMEMSAFERALLOCMETHOD_RTMEM,
    /** Support library. */
    RTMEMSAFERALLOCMETHOD_SUPR3,
    /** 32bit hack. */
    RTMEMSAFERALLOCMETHOD_32BIT_HACK = 0x7fffffff
} RTMEMSAFERALLOCMETHOD;
/** Pointer to a allocation method enum. */
typedef RTMEMSAFERALLOCMETHOD *PRTMEMSAFERALLOCMETHOD;

/**
 * Memory header for safer memory allocations.
 *
 * @note: There is no magic value used deliberately to make identifying this structure
 *        as hard as possible.
 */
typedef struct RTMEMSAFERHDR
{
    /** Flags passed to this allocation - used for freeing and reallocation. */
    uint32_t              fFlags;
    /** Allocation method used. */
    RTMEMSAFERALLOCMETHOD enmAllocMethod;
    /** Amount of bytes allocated. */
    size_t                cb;
} RTMEMSAFERHDR;
/** Pointer to a safer memory header. */
typedef RTMEMSAFERHDR *PRTMEMSAFERHDR;
/** Make sure we are staying in the padding area. */
AssertCompile(sizeof(RTMEMSAFERHDR) < RTMEMSAFER_PAD_BEFORE);

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** XOR scrambler value.
 * @todo determine this at runtime */
#if ARCH_BITS == 32
static uintptr_t g_uScramblerXor = UINT32_C(0x867af88d);
#elif ARCH_BITS == 64
static uintptr_t g_uScramblerXor = UINT64_C(0xed95ecc99416d312);
#else
# error "Bad ARCH_BITS value"
#endif



/**
 * Support (SUPR3) based allocator.
 *
 * @returns VBox status code.
 * @retval VERR_NOT_SUPPORTED if this allocation method is not supported in this
 *         version of the library.
 * @param  ppvNew    Where to store the pointer to the new buffer on success.
 * @param  cb        Amount of bytes to allocate.
 *
 * @note: The allocation will have an extra page allocated before and after the
 *        user area with all access rights removed to prevent heartbleed like
 *        attacks.
 */
static int rtMemSaferSupR3Alloc(void **ppvNew, size_t cb)
{
#if defined(IN_SUP_R3) && defined(VBOX) && !defined(RT_NO_GIP)
    /*
     * Allocate locked memory from the support library.
     * 
     */
    size_t cbUser = RT_ALIGN_Z(cb, PAGE_SIZE);
    size_t cPages = cbUser / PAGE_SIZE + 2; /* For the extra guarding pages. */
    void *pvNew = NULL;
    int rc = SUPR3PageAllocEx(cPages, 0 /* fFlags */, &pvNew, NULL /* pR0Ptr */, NULL /* paPages */);
    if (RT_SUCCESS(rc))
    {
        /* Change the memory protection of the pages guarding the allocation. */
        rc = SUPR3PageProtect(pvNew, NIL_RTR0PTR, 0, PAGE_SIZE, RTMEM_PROT_NONE);
        if (RT_SUCCESS(rc))
        {
            rc = SUPR3PageProtect(pvNew, NIL_RTR0PTR, PAGE_SIZE + cbUser, PAGE_SIZE, RTMEM_PROT_NONE);
            if (RT_SUCCESS(rc))
            {
                *ppvNew = (uint8_t *)pvNew + PAGE_SIZE;
                return VINF_SUCCESS;
            }
        }

        rc = SUPR3PageFreeEx(pvNew, cPages);
        AssertRC(rc);
    }

    return rc;
#else
    return VERR_NOT_SUPPORTED;
#endif
}


/**
 * Free method for memory allocated using the Support (SUPR3) based allocator.
 *
 * @returns nothing.
 * @param   pv    Pointer to the memory to free.
 * @param   cb    Amount of bytes allocated.
 */
static void rtMemSafeSupR3Free(void *pv, size_t cb)
{
#if defined(IN_SUP_R3) && defined(VBOX) && !defined(RT_NO_GIP)
    size_t cbUser = RT_ALIGN_Z(cb, PAGE_SIZE);
    size_t cPages = cbUser / PAGE_SIZE + 2; /* For the extra pages. */
    void *pvStart = (uint8_t *)pv - PAGE_SIZE;

    int rc = SUPR3PageFreeEx(pvStart, cPages);
    AssertRC(rc);
#else
    AssertMsgFailed(("SUPR3 allocated memory but freeing is not supported, messed up\n"));
#endif
}


RTDECL(int) RTMemSaferScramble(void *pv, size_t cb)
{
    PRTMEMSAFERHDR pHdr = (PRTMEMSAFERHDR)((char *)pv - RTMEMSAFER_PAD_BEFORE);
    AssertMsg(pHdr->cb == cb, ("pHdr->cb=%#zx cb=%#zx\n", pHdr->cb, cb));

    /* Note! This isn't supposed to be safe, just less obvious. */
    uintptr_t *pu = (uintptr_t *)pv;
    cb = RT_ALIGN_Z(cb, RTMEMSAFER_ALIGN);
    while (cb > 0)
    {
        *pu ^= g_uScramblerXor;
        pu++;
        cb -= sizeof(*pu);
    }

    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTMemSaferScramble);


RTDECL(int) RTMemSaferUnscramble(void *pv, size_t cb)
{
    PRTMEMSAFERHDR pHdr = (PRTMEMSAFERHDR)((char *)pv - RTMEMSAFER_PAD_BEFORE);
    AssertMsg(pHdr->cb == cb, ("pHdr->cb=%#zx cb=%#zx\n", pHdr->cb, cb));

    /* Note! This isn't supposed to be safe, just less obvious. */
    uintptr_t *pu = (uintptr_t *)pv;
    cb = RT_ALIGN_Z(cb, RTMEMSAFER_ALIGN);
    while (cb > 0)
    {
        *pu ^= g_uScramblerXor;
        pu++;
        cb -= sizeof(*pu);
    }

    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTMemSaferUnscramble);


RTDECL(int) RTMemSaferAllocZExTag(void **ppvNew, size_t cb, uint32_t fFlags, const char *pszTag) RT_NO_THROW
{
    AssertReturn(cb, VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppvNew, VERR_INVALID_PARAMETER);
    *ppvNew = NULL;

    /*
     * Don't request zeroed memory.  We want random heap garbage in the
     * padding zones, nothing that makes our allocations easier to find.
     */
    RTMEMSAFERALLOCMETHOD enmAllocMethod = RTMEMSAFERALLOCMETHOD_SUPR3;
    size_t cbUser = RT_ALIGN_Z(cb, RTMEMSAFER_ALIGN);
    void *pvNew = NULL;
    int rc = rtMemSaferSupR3Alloc(&pvNew, cbUser + RTMEMSAFER_PAD_BEFORE + RTMEMSAFER_PAD_AFTER);
    if (   RT_FAILURE(rc)
        && fFlags & RTMEMSAFER_ALLOC_EX_ALLOW_PAGEABLE_BACKING)
    {
        /* Pageable memory allowed. */
        enmAllocMethod = RTMEMSAFERALLOCMETHOD_RTMEM;
        pvNew = RTMemAlloc(cbUser + RTMEMSAFER_PAD_BEFORE + RTMEMSAFER_PAD_AFTER);
    }

    if (pvNew)
    {
        PRTMEMSAFERHDR pHdr = (PRTMEMSAFERHDR)pvNew;
        pHdr->fFlags         = fFlags;
        pHdr->cb             = cb;
        pHdr->enmAllocMethod = enmAllocMethod;
#ifdef RT_STRICT /* For checking input in strict builds. */
        memset((char *)pvNew + sizeof(RTMEMSAFERHDR), 0xad, RTMEMSAFER_PAD_BEFORE - sizeof(RTMEMSAFERHDR));
        memset((char *)pvNew + RTMEMSAFER_PAD_BEFORE + cb, 0xda, RTMEMSAFER_PAD_AFTER + (cbUser - cb));
#endif

        void *pvUser = (char *)pvNew + RTMEMSAFER_PAD_BEFORE;
        *ppvNew = pvUser;

        /* You don't use this API for performance, so we always clean memory. */
        RT_BZERO(pvUser, cb);

        return VINF_SUCCESS;
    }
    return rc;
}
RT_EXPORT_SYMBOL(RTMemSaferAllocZExTag);


RTDECL(void) RTMemSaferFree(void *pv, size_t cb) RT_NO_THROW
{
    if (pv)
    {
        Assert(cb);

        size_t cbUser = RT_ALIGN_Z(cb, RTMEMSAFER_ALIGN);
        void *pvStart = (char *)pv - RTMEMSAFER_PAD_BEFORE;
        PRTMEMSAFERHDR pHdr = (PRTMEMSAFERHDR)pvStart;
        AssertMsg(pHdr->cb == cb, ("pHdr->cb=%#zx cb=%#zx\n", pHdr->cb, cb));

        RTMemWipeThoroughly(pv, RT_ALIGN_Z(cb, RTMEMSAFER_ALIGN), 3);

        switch (pHdr->enmAllocMethod)
        {
            case RTMEMSAFERALLOCMETHOD_SUPR3:
                rtMemSafeSupR3Free(pvStart, cbUser + RTMEMSAFER_PAD_BEFORE + RTMEMSAFER_PAD_AFTER);
                break;
            case RTMEMSAFERALLOCMETHOD_RTMEM:
                RTMemFree(pvStart);
                break;
            default:
                AssertMsgFailed(("Invalid allocation method, corrupted header\n"));
        }
    }
    else
        Assert(cb == 0);
}
RT_EXPORT_SYMBOL(RTMemSaferFree);


RTDECL(int) RTMemSaferReallocZExTag(size_t cbOld, void *pvOld, size_t cbNew, void **ppvNew, uint32_t fFlags, const char *pszTag) RT_NO_THROW
{
    /*
     * We cannot let the heap move us around because we will be failing in our
     * duty to clean things up.  So, allocate a new block, copy over the old
     * content, and free the old one.
     */
    int rc;
    /* Real realloc. */
    if (cbNew && cbOld)
    {
        PRTMEMSAFERHDR pHdr = (PRTMEMSAFERHDR)((char *)pvOld - RTMEMSAFER_PAD_BEFORE);
        AssertPtr(pvOld);
        AssertMsg(*(size_t *)((char *)pvOld - RTMEMSAFER_PAD_BEFORE) == cbOld,
                  ("*pvStart=%#zx cbOld=%#zx\n", *(size_t *)((char *)pvOld - RTMEMSAFER_PAD_BEFORE), cbOld));

        void *pvNew;
        rc = RTMemSaferAllocZExTag(&pvNew, cbNew, pHdr->fFlags, pszTag);
        if (RT_SUCCESS(rc))
        {
            memcpy(pvNew, pvOld, RT_MIN(cbNew, cbOld));
            RTMemSaferFree(pvOld, cbOld);
            *ppvNew = pvNew;
        }
    }
    /* First allocation. */
    else if (!cbOld)
    {
        Assert(pvOld == NULL);
        rc = RTMemSaferAllocZExTag(ppvNew, cbNew, fFlags, pszTag);
    }
    /* Free operation*/
    else
    {
        RTMemSaferFree(pvOld, cbOld);
        rc = VINF_SUCCESS;
    }
    return rc;
}
RT_EXPORT_SYMBOL(RTMemSaferReallocZExTag);


RTDECL(void *) RTMemSaferAllocZTag(size_t cb, const char *pszTag) RT_NO_THROW
{
    void *pvNew = NULL;
    int rc = RTMemSaferAllocZExTag(&pvNew, cb, RTMEMSAFER_ALLOC_EX_FLAGS_DEFAULT, pszTag);
    if (RT_SUCCESS(rc))
        return pvNew;
    return NULL;
}
RT_EXPORT_SYMBOL(RTMemSaferAllocZTag);


RTDECL(void *) RTMemSaferReallocZTag(size_t cbOld, void *pvOld, size_t cbNew, const char *pszTag) RT_NO_THROW
{
    void *pvNew = NULL;
    int rc = RTMemSaferReallocZExTag(cbOld, pvOld, cbNew, &pvNew, RTMEMSAFER_ALLOC_EX_FLAGS_DEFAULT, pszTag);
    if (RT_SUCCESS(rc))
        return pvNew;
    return NULL;
}
RT_EXPORT_SYMBOL(RTMemSaferReallocZTag);

