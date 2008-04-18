/* $Id$ */
/** @file
 * Incredibly Portable Runtime - Include all necessary headers for the Darwing kernel.
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

#ifndef ___the_darwin_kernel_h
#define ___the_darwin_kernel_h

/* Problematic header(s) containing conflicts with IPRT  first. */
#define __STDC_CONSTANT_MACROS
#define __STDC_LIMIT_MACROS
#include <sys/param.h>
#include <mach/vm_param.h>
#undef ALIGN
#undef MIN
#undef MAX
#undef PAGE_SIZE
#undef PAGE_SHIFT

/* Include the IPRT definitions of the conflicting #defines & typedefs. */
#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/param.h>


/* After including cdefs, we can check that this really is Darwin. */
#ifndef RT_OS_DARWIN
# error "RT_OS_DARWIN must be defined!"
#endif

/* now we're ready for including the rest of the Darwin headers. */
#include <kern/thread.h>
#include <kern/clock.h>
#include <kern/sched_prim.h>
#include <kern/locks.h>
#include <libkern/libkern.h>
#include <mach/thread_act.h>
#include <mach/vm_map.h>
#include <pexpert/pexpert.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/ioccom.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOMapper.h>


__BEGIN_DECLS
/* mach/vm_types.h */
typedef struct pmap *pmap_t;

/* vm/vm_kern.h */
extern vm_map_t kernel_map;

/* vm/pmap.h */
extern pmap_t kernel_pmap;

/* kern/task.h */
extern vm_map_t get_task_map(task_t);

/* osfmk/i386/pmap.h */
extern ppnum_t pmap_find_phys(pmap_t, addr64_t);

/* vm/vm_map.h */
extern kern_return_t vm_map_wire(vm_map_t, vm_map_offset_t, vm_map_offset_t, vm_prot_t, boolean_t);
extern kern_return_t vm_map_unwire(vm_map_t, vm_map_offset_t, vm_map_offset_t, boolean_t);

/* mach/i386/thread_act.h */
extern kern_return_t thread_terminate(thread_t);
__END_DECLS


/*
 * Internals of the Darwin Ring-0 IPRT.
 */

__BEGIN_DECLS
extern lck_grp_t *g_pDarwinLockGroup;
__END_DECLS


/**
 * Converts from nanoseconds to Darwin absolute time units.
 * @returns Darwin absolute time.
 * @param   u64Nano     Time interval in nanoseconds
 */
DECLINLINE(uint64_t) rtDarwinAbsTimeFromNano(const uint64_t u64Nano)
{
    uint64_t u64AbsTime;
    nanoseconds_to_absolutetime(u64Nano, &u64AbsTime);
    return u64AbsTime;
}


#include <iprt/err.h>

/**
 * Convert from mach kernel return code to IPRT status code.
 * @todo put this where it belongs! (i.e. in a separate file and prototype in iprt/err.h)
 */
DECLINLINE(int) RTErrConvertFromMachKernReturn(kern_return_t rc)
{
    switch (rc)
    {
        case KERN_SUCCESS:      return VINF_SUCCESS;
        default:                return VERR_GENERAL_FAILURE;
    }
}

#endif

