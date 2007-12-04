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
#include <sys/vmsystm.h>
#include <sys/cyclic.h>
#include <sys/class.h>
#include <vm/hat.h>
#include <vm/seg_vn.h>
#include <vm/seg_kmem.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#undef u /* /usr/include/sys/user.h:249:1 is where this is defined to (curproc->p_user). very cool. */

#include <iprt/cdefs.h>

__BEGIN_DECLS
extern struct ddi_dma_attr g_SolarisX86PhysMemLimits;
__END_DECLS

#endif
