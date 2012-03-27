/* $Id$ */
/** @file
 * IPRT - Include all necessary headers for the Solaris kernel.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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
#include <sys/cmn_err.h>
#include <sys/vmsystm.h>
#include <sys/cyclic.h>
#include <sys/class.h>
#include <sys/cpuvar.h>
#include <sys/archsystm.h>
#include <sys/x_call.h> /* in platform dir */
#include <sys/x86_archext.h>
#include <vm/hat.h>
#include <vm/seg_vn.h>
#include <vm/seg_kmem.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/spl.h>
#include <sys/archsystm.h>
#include <sys/callo.h>
#include <sys/kobj.h>
#include "vbi.h"

#undef u /* /usr/include/sys/user.h:249:1 is where this is defined to (curproc->p_user). very cool. */

#include <iprt/cdefs.h>
#include <iprt/types.h>

RT_C_DECLS_BEGIN

typedef callout_id_t (*PFNSOL_timeout_generic)(int type, void (*func)(void *),
                                               void *arg, hrtime_t expiration,
                                               hrtime_t resultion, int flags);
typedef hrtime_t    (*PFNSOL_untimeout_generic)(callout_id_t id, int nowait);
typedef int         (*PFNSOL_cyclic_reprogram)(cyclic_id_t id, hrtime_t expiration);


/* IPRT globals. */
extern bool                     g_frtSolarisSplSetsEIF;
extern struct ddi_dma_attr      g_SolarisX86PhysMemLimits;
extern RTCPUSET                 g_rtMpSolarisCpuSet;
extern PFNSOL_timeout_generic   g_pfnrtR0Sol_timeout_generic;
extern PFNSOL_untimeout_generic g_pfnrtR0Sol_untimeout_generic;
extern PFNSOL_cyclic_reprogram  g_pfnrtR0Sol_cyclic_reprogram;

/* Solaris globals. */
extern uintptr_t                kernelbase;

/* Misc stuff from newer kernels. */
#ifndef CALLOUT_FLAG_ABSOLUTE
# define CALLOUT_FLAG_ABSOLUTE 2
#endif

RT_C_DECLS_END

#endif /* ___the_solaris_kernel_h */

