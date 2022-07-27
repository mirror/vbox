/* $Id$ */
/** @file
 * IPRT - Memory Allocation, C++ electric fence.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/nocrt/new>

#include <iprt/assert.h>
#include <iprt/mem.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** @todo test this on MSC */

void *RT_NEW_DELETE_CDECL operator new(RT_NEW_DELETE_SIZE_T cb) RT_NEW_DELETE_THROWS_BAD_ALLOC
{
    void *pv = RTMemAlloc(cb);
#ifdef RT_EXCEPTIONS_ENABLED
    if (!pv)
        throw std::bad_alloc();
#endif
    return pv;
}


void *RT_NEW_DELETE_CDECL operator new(RT_NEW_DELETE_SIZE_T cb, const std::nothrow_t &) RT_NEW_DELETE_NOTHROW
{
    return RTMemAlloc(cb);
}


void RT_NEW_DELETE_CDECL operator delete(void *pv) RT_NEW_DELETE_NOTHROW
{
    RTMemFree(pv);
}


#ifdef __cpp_sized_deallocation
void RT_NEW_DELETE_CDECL operator delete(void *pv, RT_NEW_DELETE_SIZE_T cb) RT_NEW_DELETE_NOTHROW
{
    NOREF(cb);
    AssertMsgFailed(("cb ignored!\n"));
    RTMemFree(pv);
}
#endif


void RT_NEW_DELETE_CDECL operator delete(void *pv, const std::nothrow_t &) RT_NEW_DELETE_NOTHROW
{
    RTMemFree(pv);
}


/*
 *
 * Array object form.
 * Array object form.
 * Array object form.
 *
 */

void *RT_NEW_DELETE_CDECL operator new[](RT_NEW_DELETE_SIZE_T cb) RT_NEW_DELETE_THROWS_BAD_ALLOC
{
    void *pv = RTMemAlloc(cb);
#ifdef RT_EXCEPTIONS_ENABLED
    if (!pv)
        throw std::bad_alloc();
#endif
    return pv;
}


void *RT_NEW_DELETE_CDECL operator new[](RT_NEW_DELETE_SIZE_T cb, const std::nothrow_t &) RT_NEW_DELETE_NOTHROW
{
    return RTMemAlloc(cb);
}


void RT_NEW_DELETE_CDECL operator delete[](void * pv) RT_NEW_DELETE_NOTHROW
{
    RTMemFree(pv);
}


#ifdef __cpp_sized_deallocation
void RT_NEW_DELETE_CDECL operator delete[](void * pv, RT_NEW_DELETE_SIZE_T cb) RT_NEW_DELETE_NOTHROW
{
    NOREF(cb);
    AssertMsgFailed(("cb ignored!\n"));
    RTMemFree(pv);
}
#endif


void RT_NEW_DELETE_CDECL operator delete[](void *pv, const std::nothrow_t &) RT_NEW_DELETE_NOTHROW
{
    RTMemFree(pv);
}

