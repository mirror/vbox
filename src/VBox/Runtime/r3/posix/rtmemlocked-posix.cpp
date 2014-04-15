/* $Id$ */
/** @file
 * IPRT - RTMemLocked*, POSIX.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
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
#include <iprt/mem.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include "internal/mem.h"

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/errno.h>
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
# define MAP_ANONYMOUS MAP_ANON
#endif

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Header for a locked memory region.
 */
typedef struct RTMEMLOCKEDHDR
{
    /** Magic value. */
    uint32_t            u32Magic;
    /** Size of the region in bytes including the header, rounded to a page aligned value. */
    size_t              cbRegion;
#if HC_ARCH_BITS == 64
    /** 8 byte alignment of the structure on 64bit platforms. */
    uint32_t            u32Alignment;
#endif
} RTMEMLOCKEDHDR;
/** Pointer to a locked memory region header. */
typedef RTMEMLOCKEDHDR *PRTMEMLOCKEDHDR;
/** Magic for the header. (Vienna Teng) */
#define RTMEMLOCKEDHDR_MAGIC UINT32_C(0x19781003)

RTDECL(int) RTMemLockedAllocExTag(size_t cb, const char *pszTag, void **ppv) RT_NO_THROW
{
    int rc = VINF_SUCCESS;
    size_t cbAlloc = RT_ALIGN_Z(cb + sizeof(RTMEMLOCKEDHDR), PAGE_SIZE);
    void *pv;

    NOREF(pszTag);

    pv = mmap(NULL, cbAlloc, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
              -1, 0);
    if (pv != MAP_FAILED)
    {
        int rcPosix = mlock(pv, cbAlloc);
        if (!rcPosix)
        {
            PRTMEMLOCKEDHDR pHdr = (PRTMEMLOCKEDHDR)pv;
            AssertPtr(pHdr);

            pHdr->u32Magic = RTMEMLOCKEDHDR_MAGIC;
            pHdr->cbRegion = cbAlloc;
            *ppv = pHdr + 1;
        }
        else
            rc = RTErrConvertFromErrno(errno);
    }

    return rc;
}

RTDECL(int) RTMemLockedAllocZExTag(size_t cb, const char *pszTag, void **ppv) RT_NO_THROW
{
    void *pv = NULL;
    int rc = RTMemLockedAllocExTag(cb, pszTag, &pv);

    if (RT_SUCCESS(rc))
    {
        RT_BZERO(pv, cb);
        *ppv = pv;
    }

    return rc;
}

RTDECL(void *) RTMemLockedAllocTag(size_t cb, const char *pszTag) RT_NO_THROW
{
    void *pv = NULL;
    int rc = RTMemLockedAllocExTag(cb, pszTag, &pv);

    if (RT_FAILURE(rc))
        return NULL;

    return pv;
}

RTDECL(void *) RTMemLockedAllocZTag(size_t cb, const char *pszTag) RT_NO_THROW
{
    void *pv = NULL;
    int rc = RTMemLockedAllocZExTag(cb, pszTag, &pv);

    if (RT_FAILURE(rc))
        return NULL;

    return pv;
}

RTDECL(void) RTMemLockedFree(void *pv) RT_NO_THROW
{
    /*
     * Validate & adjust the input.
     */
    if (!pv)
        return;
    AssertPtr(pv);

    int rcPosix;
    PRTMEMLOCKEDHDR pHdr = ((PRTMEMLOCKEDHDR)pv) - 1;
    AssertReturnVoid(pHdr->u32Magic == RTMEMLOCKEDHDR_MAGIC);

    pHdr->u32Magic = ~RTMEMLOCKEDHDR_MAGIC;
    rcPosix = munlock(pHdr, pHdr->cbRegion);
    AssertMsg(rcPosix == 0, ("rc=%d errno=%d pv=%p\n", rcPosix, errno, pHdr)); NOREF(rcPosix);

    rcPosix = munmap(pHdr, pHdr->cbRegion);
    AssertMsg(rcPosix == 0, ("rc=%d errno=%d pv=%p\n", rcPosix, errno, pHdr)); NOREF(rcPosix);
}

