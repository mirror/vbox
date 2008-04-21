/* $Id$ */
/** @file
 * IPRT - Memory Allocation, Win32.
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
#define LOG_GROUP RTLOGGROUP_MEM
#include <Windows.h>

#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include <iprt/err.h>

#ifndef USE_VIRTUAL_ALLOC
# include <malloc.h>
#endif


/**
 * Allocates memory which may contain code.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 * @param   cb      Size in bytes of the memory block to allocate.
 */
RTDECL(void *) RTMemExecAlloc(size_t cb)
{
    /*
     * Allocate first.
     */
    AssertMsg(cb, ("Allocating ZERO bytes is really not a good idea! Good luck with the next assertion!\n"));
    cb = RT_ALIGN_Z(cb, 32);
    void *pv = malloc(cb);
    AssertMsg(pv, ("malloc(%d) failed!!!\n", cb));
    if (pv)
    {
        /*
         * Add PROT_EXEC flag to the page.
         *
         * This is in violation of the SuS where I think it saith that mprotect() shall
         * only be used with mmap()'ed memory. Works on linux and OS/2 LIBC v0.6.
         */
        memset(pv, 0xcc, cb);
        void   *pvProt = (void *)((uintptr_t)pv & ~PAGE_OFFSET_MASK);
        size_t  cbProt = ((uintptr_t)pv & PAGE_OFFSET_MASK) + cb;
        cbProt = RT_ALIGN_Z(cbProt, PAGE_SIZE);
        DWORD fFlags = 0;
        if (!VirtualProtect(pvProt, cbProt, PAGE_EXECUTE_READWRITE, &fFlags))
        {
            AssertMsgFailed(("VirtualProtect(%p, %#x,,) -> lasterr=%d\n", pvProt, cbProt, GetLastError()));
            free(pv);
            pv = NULL;
        }
    }
    return pv;
}


/**
 * Free executable/read/write memory allocated by RTMemExecAlloc().
 *
 * @param   pv      Pointer to memory block.
 */
RTDECL(void)    RTMemExecFree(void *pv)
{
    if (pv)
        free(pv);
}


/**
 * Allocate page aligned memory.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL if we're out of memory.
 * @param   cb  Size of the memory block. Will be rounded up to page size.
 */
RTDECL(void *) RTMemPageAlloc(size_t cb)
{
#ifdef USE_VIRTUAL_ALLOC
    void *pv = VirtualAlloc(NULL, RT_ALIGN_Z(cb, PAGE_SIZE), MEM_COMMIT, PAGE_READWRITE);
#else
    void *pv = _aligned_malloc(cb, PAGE_SIZE);
#endif
    AssertMsg(pv, ("cb=%d lasterr=%d\n", cb, GetLastError()));
    return pv;
}


/**
 * Allocate zero'ed page aligned memory.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL if we're out of memory.
 * @param   cb  Size of the memory block. Will be rounded up to page size.
 */
RTDECL(void *) RTMemPageAllocZ(size_t cb)
{
#ifdef USE_VIRTUAL_ALLOC
    void *pv = VirtualAlloc(NULL, RT_ALIGN_Z(cb, PAGE_SIZE), MEM_COMMIT, PAGE_READWRITE);
#else
    void *pv = _aligned_malloc(cb, PAGE_SIZE);
#endif
    if (pv)
    {
        memset(pv, 0, RT_ALIGN_Z(cb, PAGE_SIZE));
        return pv;
    }
    AssertMsgFailed(("cb=%d lasterr=%d\n", cb, GetLastError()));
    return NULL;
}


/**
 * Free a memory block allocated with RTMemPageAlloc() or RTMemPageAllocZ().
 *
 * @param   pv      Pointer to the block as it was returned by the allocation function.
 *                  NULL will be ignored.
 */
RTDECL(void) RTMemPageFree(void *pv)
{
    if (pv)
    {
#ifdef USE_VIRTUAL_ALLOC
        if (!VirtualFree(pv, 0, MEM_RELEASE))
            AssertMsgFailed(("pv=%p lasterr=%d\n", pv, GetLastError()));
#else
        _aligned_free(pv);
#endif
    }
}


/**
 * Change the page level protection of a memory region.
 *
 * @returns iprt status code.
 * @param   pv          Start of the region. Will be rounded down to nearest page boundary.
 * @param   cb          Size of the region. Will be rounded up to the nearest page boundary.
 * @param   fProtect    The new protection, a combination of the RTMEM_PROT_* defines.
 */
RTDECL(int) RTMemProtect(void *pv, size_t cb, unsigned fProtect)
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
    Assert(!RTMEM_PROT_NONE);
    switch (fProtect & (RTMEM_PROT_NONE | RTMEM_PROT_READ | RTMEM_PROT_WRITE | RTMEM_PROT_EXEC))
    {
        case RTMEM_PROT_NONE:
            fProt = PAGE_NOACCESS;
            break;

        case RTMEM_PROT_READ:
            fProt = PAGE_READONLY;
            break;

        case RTMEM_PROT_READ | RTMEM_PROT_WRITE:
            fProt = PAGE_READWRITE;
            break;

        case RTMEM_PROT_READ | RTMEM_PROT_WRITE | RTMEM_PROT_EXEC:
            fProt = PAGE_EXECUTE_READWRITE;
            break;

        case RTMEM_PROT_READ | RTMEM_PROT_EXEC:
            fProt = PAGE_EXECUTE_READWRITE;
            break;

        case RTMEM_PROT_WRITE:
            fProt = PAGE_READWRITE;
            break;

        case RTMEM_PROT_WRITE | RTMEM_PROT_EXEC:
            fProt = PAGE_EXECUTE_READWRITE;
            break;

        case RTMEM_PROT_EXEC:
            fProt = PAGE_EXECUTE_READWRITE;
            break;

        /* If the compiler had any brains it would warn about this case. */
        default:
            AssertMsgFailed(("fProtect=%#x\n", fProtect));
            return VERR_INTERNAL_ERROR;
    }

    /*
     * Align the request.
     */
    cb += (uintptr_t)pv & PAGE_OFFSET_MASK;
    pv = (void *)((uintptr_t)pv & ~PAGE_OFFSET_MASK);

    /*
     * Change the page attributes.
     */
    DWORD fFlags = 0;
    if (VirtualProtect(pv, cb, fProt, &fFlags))
        return VINF_SUCCESS;
    return RTErrConvertFromWin32(GetLastError());
}

