/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Memory Allocation, Ring-0 Driver.
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

#ifndef __r0drv_alloc_r0drv_h__
#define __r0drv_alloc_r0drv_h__

#include <iprt/cdefs.h>
#include <iprt/types.h>

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

/** Magic number for heap blocks. (Edgar Allan Poe) */
#define RTMEMHDR_MAGIC      0x18090119

/** @name RTMEMHDR::fFlags.
 * @{ */
#define RTMEMHDR_FLAG_ZEROED    BIT(0)
#define RTMEMHDR_FLAG_EXEC      BIT(1)
#ifdef __LINUX__
#define RTMEMHDR_FLAG_KMALLOC   BIT(31)
#endif
/** @} */


PRTMEMHDR   rtMemAlloc(size_t cb, uint32_t fFlags);
void        rtMemFree(PRTMEMHDR pHdr);

__END_DECLS
#endif

