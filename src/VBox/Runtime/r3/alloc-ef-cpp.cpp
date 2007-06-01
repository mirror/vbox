/* $Id$ */
/** @file
 * innotek Portable Runtime - Memory Allocation, C++ electric fence.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#include "alloc-ef.h"

#ifdef RTALLOC_EFENCE_CPP /* rest of the file */

#include <new>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @todo test this on MSC */

/* MSC declares the operators as cdecl it seems. */
#ifdef _MSC_VER
# define RT_EF_CDECL    __cdecl
#else
# define RT_EF_CDECL
#endif

/* MSC doesn't use the standard namespace. */
#ifdef _MSC_VER
# define RT_EF_SIZE_T   size_t
#else
# define RT_EF_SIZE_T   std::size_t
#endif


void *RT_EF_CDECL operator new(RT_EF_SIZE_T cb) throw(std::bad_alloc)
{
    void *pv = rtMemAlloc("new", RTMEMTYPE_NEW, cb, ((void **)&cb)[-1], 0, NULL, NULL);
    if (!pv)
        throw std::bad_alloc();
    return pv;
}


void *RT_EF_CDECL operator new(RT_EF_SIZE_T cb, const std::nothrow_t &) throw()
{
    void *pv = rtMemAlloc("new nothrow", RTMEMTYPE_NEW, cb, ((void **)&cb)[-1], 0, NULL, NULL);
    return pv;
}


void RT_EF_CDECL operator delete(void *pv) throw()
{
    rtMemFree("delete", RTMEMTYPE_DELETE, pv, ((void **)&pv)[-1], 0, NULL, NULL);
}


void RT_EF_CDECL operator delete(void * pv, const std::nothrow_t &) throw()
{
    rtMemFree("delete nothrow", RTMEMTYPE_DELETE, pv, ((void **)&pv)[-1], 0, NULL, NULL);
}


/*
 *
 * Array object form.
 * Array object form.
 * Array object form.
 *
 */

void *RT_EF_CDECL operator new[](RT_EF_SIZE_T cb) throw(std::bad_alloc)
{
    void *pv = rtMemAlloc("new[]", RTMEMTYPE_NEW_ARRAY, cb, ((void **)&cb)[-1], 0, NULL, NULL);
    if (!pv)
        throw std::bad_alloc();
    return pv;
}


void * RT_EF_CDECL operator new[](RT_EF_SIZE_T cb, const std::nothrow_t &) throw()
{
    void *pv = rtMemAlloc("new[] nothrow", RTMEMTYPE_NEW_ARRAY, cb, ((void **)&cb)[-1], 0, NULL, NULL);
    return pv;
}


void RT_EF_CDECL operator delete[](void * pv) throw()
{
    rtMemFree("delete[]", RTMEMTYPE_DELETE_ARRAY, pv, ((void **)&pv)[-1], 0, NULL, NULL);
}


void RT_EF_CDECL operator delete[](void *pv, const std::nothrow_t &) throw()
{
    rtMemFree("delete[] nothrow", RTMEMTYPE_DELETE_ARRAY, pv, ((void **)&pv)[-1], 0, NULL, NULL);
}

#endif /* RTALLOC_EFENCE_CPP */
