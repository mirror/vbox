/** @file
 * innotek Portable Runtime - alloca().
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

#ifndef __iprt_alloca_h__
#define __iprt_alloca_h__

/*
 * If there are more difficult platforms out there, we'll do OS
 * specific #ifdefs. But for now we'll just include the headers
 * which normally contains the alloca() prototype.
 * When we're in kernel territory it starts getting a bit more
 * interesting of course...
 */
#if defined(IN_RING0) && defined(RT_OS_LINUX)
/* ASSUMES GNU C */
# define alloca(cb) __builtin_alloca(cb)
#else
# include <stdlib.h>
# if !defined(RT_OS_DARWIN) && !defined(RT_OS_FREEBSD)
#  include <malloc.h>
# endif
# ifdef RT_OS_SOLARIS
#  include <alloca.h>
# endif
#endif

#endif

