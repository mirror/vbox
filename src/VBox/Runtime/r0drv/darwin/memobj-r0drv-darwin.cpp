/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Ring-0 Memory Objects, Darwin.
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
#include "the-darwin-kernel.h"

#include <iprt/memobj.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include "r0drv/memobj-r0drv.h"



int rtR0MemObjNativeFree(RTR0MEMOBJ pMem)
{
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeAllocPage(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeAllocLow(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeAllocCont(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeLockUser(PPRTR0MEMOBJINTERNAL ppMem, void *pv, size_t cb)
{
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeLockKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pv, size_t cb)
{
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeAllocPhys(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, RTHCPHYS PhysHighest)
{
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeEnterPhys(PPRTR0MEMOBJINTERNAL ppMem, RTHCPHYS Phys, size_t cb)
{
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeReserveKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pvFixed, size_t cb, size_t uAlignment)
{
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeReserveUser(PPRTR0MEMOBJINTERNAL ppMem, void *pvFixed, size_t cb, size_t uAlignment)
{
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeMapKernel(PPRTR0MEMOBJINTERNAL ppMem, RTR0MEMOBJ pMemToMap, void *pvFixed, size_t uAlignment, unsigned fProt)
{
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeMapUser(PPRTR0MEMOBJINTERNAL ppMem, RTR0MEMOBJ pMemToMap, void *pvFixed, size_t uAlignment, unsigned fProt)
{
    return VERR_NOT_IMPLEMENTED;
}


RTHCPHYS rtR0MemObjNativeGetPagePhysAddr(RTR0MEMOBJ pMem, unsigned iPage)
{
    return NIL_RTHCPHYS;
}

