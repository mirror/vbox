/* $Id$ */
/** @file
 * innotek Portable Runtime - Memory Allocation, Ring-0 Driver.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#ifndef ___r0drv_alloc_r0drv_h
#define ___r0drv_alloc_r0drv_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include "internal/magics.h"

__BEGIN_DECLS

/**
 * Header which heading all memory blocks.
 */
typedef struct RTMEMHDR
{
    /** Magic (RTMEMHDR_MAGIC). */
    uint32_t    u32Magic;
    /** Block flags (RTMEMHDR_FLAG_*). */
    uint32_t    fFlags;
    /** The size of the block. */
    uint32_t    cb;
    /** Alignment padding. */
    uint32_t    u32Padding;
} RTMEMHDR, *PRTMEMHDR;


/** @name RTMEMHDR::fFlags.
 * @{ */
#define RTMEMHDR_FLAG_ZEROED    RT_BIT(0)
#define RTMEMHDR_FLAG_EXEC      RT_BIT(1)
#ifdef RT_OS_LINUX
#define RTMEMHDR_FLAG_EXEC_HEAP RT_BIT(30)
#define RTMEMHDR_FLAG_KMALLOC   RT_BIT(31)
#endif
/** @} */


PRTMEMHDR   rtMemAlloc(size_t cb, uint32_t fFlags);
void        rtMemFree(PRTMEMHDR pHdr);

__END_DECLS
#endif

