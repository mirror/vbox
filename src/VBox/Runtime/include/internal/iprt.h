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
 && defined(MODULE) \
 && !defined(RT_NO_EXPORT_SYMBOL)
# define bool linux_bool /* see r0drv/linux/the-linux-kernel.h */
# include <linux/autoconf.h>
# include <linux/module.h>
# undef bool
# define RT_EXPORT_SYMBOL(Name) EXPORT_SYMBOL(Name)
#else
# define RT_EXPORT_SYMBOL(Name) extern int g_rtExportSymbolDummyVariable
#endif


/** @def RT_MORE_STRICT
 * Enables more assertions in IPRT.  */
#if !defined(RT_MORE_STRICT) && (defined(DEBUG) || defined(RT_STRICT) || defined(DOXYGEN_RUNNING))
# define RT_MORE_STRICT
#endif 

/** @def RT_ASSERT_PREEMPT_CPUID_VAR
 * Partner to RT_ASSERT_PREEMPT_CPUID_VAR. Declares and initializes a variable 
 * idAssertCpu to NIL_RTCPUID if preemption is enabled and to RTMpCpuId if 
 * disabled.  When RT_MORE_STRICT isn't defined it declares an uninitialized 
 * dummy variable. 
 *  
 * Requires iprt/mp.h and iprt/asm.h. 
 */ 
/** @def RT_ASSERT_PREEMPT_CPUID
 * Asserts that we didn't change CPU since RT_ASSERT_PREEMPT_CPUID_VAR if 
 * preemption is disabled.  Will also detect changes in preemption 
 * disable/enable status.  This is a noop when RT_MORE_STRICT isn't defined. */ 
#ifdef RT_MORE_STRICT
# define RT_ASSERT_PREEMPT_CPUID_VAR() \
    RTCPUID const idAssertCpu = RTThreadPreemptIsEnabled(NIL_RTTHREAD) ? NIL_RTCPUID : RTMpCpuId()
# define RT_ASSERT_PREEMPT_CPUID() \
   do \
   { \
        RTCPUID const idAssertCpuNow = RTThreadPreemptIsEnabled(NIL_RTTHREAD) ? NIL_RTCPUID : RTMpCpuId(); \
        AssertMsg(idAssertCpu == idAssertCpuNow,  ("%#x, %#x\n", idAssertCpu, idAssertCpuNow)); \
   } while (0)
                                                 
#else
# define RT_ASSERT_PREEMPT_CPUID_VAR()  RTCPUID idAssertCpuDummy
# define RT_ASSERT_PREEMPT_CPUID()      NOREF(idAssertCpuDummy)
#endif 

/** @def RT_ASSERT_INTS_ON
 * Asserts that interrupts are disabled when RT_MORE_STRICT is defined.   */
#ifdef RT_MORE_STRICT
# define RT_ASSERT_INTS_ON()            Assert(ASMIntAreEnabled())
#else
# define RT_ASSERT_INTS_ON()            do { } while (0)
#endif 

/** @def RT_ASSERT_PREEMPTIBLE
 * Asserts that preemption hasn't been disabled (using 
 * RTThreadPreemptDisable) when RT_MORE_STRICT is defined. */ 
#ifdef RT_MORE_STRICT
# define RT_ASSERT_PREEMPTIBLE()        Assert(RTThreadPreemptIsEnabled(NIL_RTTHREAD))
#else
# define RT_ASSERT_PREEMPTIBLE()        do { } while (0)
#endif 

#endif

