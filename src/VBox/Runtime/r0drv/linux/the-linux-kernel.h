/* $Id$ */
/** @file
 * innotek Portable Runtime - Include all necessary headers for the Linux kernel.
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

#ifndef ___the_linux_kernel_h
#define ___the_linux_kernel_h

/*
 * Include iprt/types.h to install the bool wrappers.
 * Then use the linux bool type for all the stuff include here.
 */
#include <iprt/types.h>
#define bool linux_bool

#include <linux/autoconf.h>
#include <linux/version.h>

/* We only support 2.4 and 2.6 series kernels */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
# error We only support 2.4 and 2.6 series kernels
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 0) && LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
# error We only support 2.4 and 2.6 series kernels
#endif

#if defined(CONFIG_MODVERSIONS) && !defined(MODVERSIONS)
# define MODVERSIONS
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 71)
#  include <linux/modversions.h>
# endif
#endif
#ifndef KBUILD_STR
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 16)
#  define KBUILD_STR(s) s
# else
#  define KBUILD_STR(s) #s
# endif
#endif
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <asm/semaphore.h>
#include <linux/module.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
# include <linux/moduleparam.h>
#endif
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 7)
# include <linux/time.h>
# include <linux/jiffies.h>
#endif
#include <linux/wait.h>
/* For the basic additions module */
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/compiler.h>
#ifndef HAVE_UNLOCKED_IOCTL /* linux/fs.h defines this */
# include <linux/smp_lock.h>
#endif
/* For the shared folders module */
#include <linux/vmalloc.h>
#define wchar_t linux_wchar_t
#include <linux/nls.h>
#undef wchar_t
#include <asm/mman.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/div64.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
# ifndef page_to_pfn
#  define page_to_pfn(page) ((page) - mem_map)
# endif
#endif

#ifndef DEFINE_WAIT
# define DEFINE_WAIT(name) DECLARE_WAITQUEUE(name, current)
#endif

/*
 * 2.4 compatibility wrappers
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 7)

# ifndef MAX_JIFFY_OFFSET
#  define MAX_JIFFY_OFFSET ((~0UL >> 1)-1)
# endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 29) || LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)

DECLINLINE(unsigned int) jiffies_to_msecs(unsigned long cJiffies)
{
#  if HZ <= 1000 && !(1000 % HZ)
    return (1000 / HZ) * cJiffies;
#  elif HZ > 1000 && !(HZ % 1000)
    return (cJiffies + (HZ / 1000) - 1) / (HZ / 1000);
#  else
    return (cJiffies * 1000) / HZ;
#  endif
}

DECLINLINE(unsigned long) msecs_to_jiffies(unsigned int cMillies)
{
#  if HZ > 1000
    if (cMillies > jiffies_to_msecs(MAX_JIFFY_OFFSET))
        return MAX_JIFFY_OFFSET;
#  endif
#  if HZ <= 1000 && !(1000 % HZ)
    return (cMillies + (1000 / HZ) - 1) / (1000 / HZ);
#  elif HZ > 1000 && !(HZ % 1000)
    return cMillies * (HZ / 1000);
#  else
    return (cMillies * HZ + 999) / 1000;
#  endif
}

# endif  /* < 2.4.29 || >= 2.6.0 */

# define prepare_to_wait(q, wait, state) \
    do { \
        set_current_state(state); \
        add_wait_queue(q, wait); \
    } while (0)

# define finish_wait(q, wait) \
    do { \
        remove_wait_queue(q, wait); \
        set_current_state(TASK_RUNNING); \
    } while (0)

#endif /* < 2.6.7 */


/*
 * This sucks soooo badly on x86! Why don't they export __PAGE_KERNEL_EXEC so PAGE_KERNEL_EXEC would be usable?
 */
#if defined(RT_ARCH_AMD64)
# define MY_PAGE_KERNEL_EXEC    PAGE_KERNEL_EXEC
#elif defined(PAGE_KERNEL_EXEC) && defined(CONFIG_X86_PAE)
# define MY_PAGE_KERNEL_EXEC    __pgprot(cpu_has_pge ? _PAGE_KERNEL_EXEC | _PAGE_GLOBAL : _PAGE_KERNEL_EXEC)
#else
# define MY_PAGE_KERNEL_EXEC    PAGE_KERNEL
#endif


/*
 * The redhat hack section.
 *  - The current hacks are for 2.4.21-15.EL only.
 */
#ifndef NO_REDHAT_HACKS
/* accounting. */
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#  ifdef VM_ACCOUNT
#   define MY_DO_MUNMAP(a,b,c) do_munmap(a, b, c, 0) /* should it be 1 or 0? */
#  endif
# endif

/* backported remap_page_range. */
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#  include <asm/tlb.h>
#  ifdef tlb_vma /* probably not good enough... */
#   define HAVE_26_STYLE_REMAP_PAGE_RANGE 1
#  endif
# endif

# ifndef RT_ARCH_AMD64
/* In 2.6.9-22.ELsmp we have to call change_page_attr() twice when changing
 * the page attributes from PAGE_KERNEL to something else, because there appears
 * to be a bug in one of the many patches that redhat applied.
 * It should be safe to do this on less buggy linux kernels too. ;-)
 */
#  define MY_CHANGE_PAGE_ATTR(pPages, cPages, prot) \
    do { \
        if (pgprot_val(prot) != pgprot_val(PAGE_KERNEL)) \
            change_page_attr(pPages, cPages, prot); \
        change_page_attr(pPages, cPages, prot); \
    } while (0)
# endif  /* !RT_ARCH_AMD64 */
#endif /* !NO_REDHAT_HACKS */

#ifndef MY_DO_MUNMAP
# define MY_DO_MUNMAP(a,b,c) do_munmap(a, b, c)
#endif

#ifndef MY_CHANGE_PAGE_ATTR
# ifdef RT_ARCH_AMD64 /** @todo This is a cheap hack, but it'll get around that 'else BUG();' in __change_page_attr().  */
#  define MY_CHANGE_PAGE_ATTR(pPages, cPages, prot) \
    do { \
        change_page_attr(pPages, cPages, PAGE_KERNEL_NOCACHE); \
        change_page_attr(pPages, cPages, prot); \
    } while (0)
# else
#  define MY_CHANGE_PAGE_ATTR(pPages, cPages, prot) change_page_attr(pPages, cPages, prot)
# endif
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)
# define MY_SET_PAGES_EXEC(pPages, cPages)    set_pages_x(pPages, cPages)
# define MY_SET_PAGES_NOEXEC(pPages, cPages)  set_pages_nx(pPages, cPages)
#else
# define MY_SET_PAGES_EXEC(pPages, cPages) \
    do { \
        if (pgprot_val(MY_PAGE_KERNEL_EXEC) != pgprot_val(PAGE_KERNEL)) \
            MY_CHANGE_PAGE_ATTR(pPages, cPages, MY_PAGE_KERNEL_EXEC); \
    } while (0)
# define MY_SET_PAGES_NOEXEC(pPages, cPages) \
    do { \
        if (pgprot_val(MY_PAGE_KERNEL_EXEC) != pgprot_val(PAGE_KERNEL)) \
            MY_CHANGE_PAGE_ATTR(pPages, cPages, PAGE_KERNEL); \
    } while (0)
#endif

#ifndef PAGE_OFFSET_MASK
# define PAGE_OFFSET_MASK (PAGE_SIZE - 1)
#endif

/*
 * Stop using the linux bool type.
 */
#undef bool

/*
 * There are post-2.6.24 kernels (confusingly with unchanged version number)
 * which eliminate macros which were marked as deprecated.
 */
#ifndef __attribute_used__
#define __attribute_used__ __used
#endif

/**
 * Hack for shortening pointers on linux so we can stuff more stuff into the
 * task_struct::comm field. This is used by the semaphore code but put here
 * because we don't have any better place atm. Don't use outside IPRT, please.
 */
#ifdef RT_ARCH_AMD64
# define IPRT_DEBUG_SEMS_ADDRESS(addr)  ( ((long)(addr) & (long)~UINT64_C(0xfffffff000000000)) )
#else
# define IPRT_DEBUG_SEMS_ADDRESS(addr)  ( (long)(addr) )
#endif

#endif

