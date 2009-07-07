/* $Id$ */
/** @file
 * IPRT - Internal header for miscellaneous global defs and types.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#ifndef ___internal_iprt_h
#define ___internal_iprt_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

/** @def RT_EXPORT_SYMBOL
 * This define is really here just for the linux kernel.
 * @param   Name        The symbol name.
 */
#if defined(RT_OS_LINUX) \
 && defined(IN_RING0) \
 && defined(MODULE)
# define bool linux_bool /* see r0drv/linux/the-linux-kernel.h */
# include <linux/autoconf.h>
# include <linux/module.h>
# undef bool
# define RT_EXPORT_SYMBOL(Name) EXPORT_SYMBOL(Name)
#else
# define RT_EXPORT_SYMBOL(Name) extern int g_rtExportSymbolDummyVariable
#endif

#endif

