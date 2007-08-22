/* $Id$ */
/** @file
 * innotek Portable Runtime - Include all necessary headers for the Solaris kernel.
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
 */

#ifndef ___the_solaris_kernel_h
#define ___the_solaris_kernel_h

#include <sys/kmem.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/thread.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sdt.h>
#include <sys/schedctl.h>
#include <sys/time.h>
#include <sys/sysmacros.h>
#include <vm/hat.h>
#include <vm/seg_kmem.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#include <iprt/param.h> /* for PAGE_SIZE */

/** 
 * Used in alloc-r0drv-solaris.c and in memobj-r0drv-solaris.cpp
 * Basically needs to be used anywhere where contiguous allocation
 * is needed.
 * 
 * @todo r=bird: This should be moved out of this header as it will probably
 *               be instantiated in all the files using this header and 
 *               the compiler will bitch about it not being used.
 */
static struct ddi_dma_attr g_SolarisX86PhysMemLimits = 
{
    DMA_ATTR_V0,            /* Version Number */
    (uint64_t)0,            /* lower limit */
    (uint64_t)0xffffffff,   /* high limit (32-bit PA) */
    (uint64_t)0xffffffff,   /* counter limit */
    (uint64_t)PAGE_SIZE,    /* alignment */
    (uint64_t)PAGE_SIZE,    /* burst size */
    (uint64_t)PAGE_SIZE,    /* effective DMA size */
    (uint64_t)0xffffffff,   /* max DMA xfer size */
    (uint64_t)0xffffffff,   /* segment boundary */
    512,                    /* s/g length */
    1,                      /* device granularity */
    0                       /* bus-specific flags */
};

/* commented for now 
#include <iprt/cdefs.h>
__BEGIN_DECLS

__END_DECLS
*/

#endif
