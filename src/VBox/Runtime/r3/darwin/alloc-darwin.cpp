/* $Id$ */
/** @file
 * IPRT - Memory Allocation, Darwin.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/param.h>
#include <iprt/err.h>
#include <iprt/string.h>

#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>


RTDECL(void *) RTMemExecAllocTag(size_t cb, const char *pszTag) RT_NO_THROW
{
    /*
     * Allocate first.
     */
    AssertMsg(cb, ("Allocating ZERO bytes is really not a good idea! Good luck with the next assertion!\n"));
    cb = RT_ALIGN_Z(cb, PAGE_SIZE);
    void *pv = valloc(cb);
    AssertMsg(pv, ("posix_memalign(%d) failed!!! errno=%d\n", cb, errno));
    if (pv)
    {
        /*
         * Add PROT_EXEC flag to the page(s).
         */
        memset(pv, 0xcc, cb);
        int rc = mprotect(pv, cb, PROT_READ | PROT_WRITE | PROT_EXEC);
        if (rc)
        {
            AssertMsgFailed(("mprotect(%p, %#x,,) -> rc=%d, errno=%d\n", pv, cb, rc, errno));
            free(pv);
            pv = NULL;
        }
    }
    return pv;
}


RTDECL(void)    RTMemExecFree(void *pv, size_t cb) RT_NO_THROW
{
    if (pv)
        free(pv);
}


RTDECL(void *) RTMemPageAllocTag(size_t cb, const char *pszTag) RT_NO_THROW
{
    return valloc(RT_ALIGN_Z(cb, PAGE_SIZE));
}


RTDECL(void *) RTMemPageAllocZTag(size_t cb, const char *pszTag) RT_NO_THROW
{
    cb = RT_ALIGN_Z(cb, PAGE_SIZE);
    void *pv = valloc(cb);
    if (pv)
        bzero(pv, RT_ALIGN_Z(cb, PAGE_SIZE));
    return pv;
}


RTDECL(void) RTMemPageFree(void *pv, size_t cb) RT_NO_THROW
{
    if (pv)
        free(pv);
}


RTDECL(int) RTMemProtect(void *pv, size_t cb, unsigned fProtect) RT_NO_THROW
{
    /*
     * Validate input.
     */
    if (cb == 0)
    {
        AssertMsgFailed(("!cb\n"));
        return VERR_INVALID_PARAMETER;
    }
    if (fProtect & ~(RTMEM_PROT_NONE | RTMEM_PROT_READ | RTMEM_PROT_WRITE | RTMEM_PROT_EXEC))
    {
        AssertMsgFailed(("fProtect=%#x\n", fProtect));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Convert the flags.
     */
    int fProt;
#if     RTMEM_PROT_NONE  == PROT_NONE \
    &&  RTMEM_PROT_READ  == PROT_READ \
    &&  RTMEM_PROT_WRITE == PROT_WRITE \
    &&  RTMEM_PROT_EXEC  == PROT_EXEC
    fProt = fProtect;
#else
    Assert(!RTMEM_PROT_NONE);
    if (!fProtect)
        fProt = PROT_NONE;
    else
    {
        fProt = 0;
        if (fProtect & RTMEM_PROT_READ)
            fProt |= PROT_READ;
        if (fProtect & RTMEM_PROT_WRITE)
            fProt |= PROT_WRITE;
        if (fProtect & RTMEM_PROT_EXEC)
            fProt |= PROT_EXEC;
    }
#endif

    /*
     * Align the request.
     */
    cb += (uintptr_t)pv & PAGE_OFFSET_MASK;
    pv = (void *)((uintptr_t)pv & ~PAGE_OFFSET_MASK);

    /*
     * Change the page attributes.
     */
    int rc = mprotect(pv, cb, fProt);
    if (!rc)
        return rc;
    return RTErrConvertFromErrno(errno);
}

