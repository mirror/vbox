/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_VBI_H
#define	_SYS_VBI_H

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Private interfaces for VirtualBox access to Solaris kernel internal
 * facilities. The interface uses limited types when crossing the kernel
 * to hypervisor boundary. (void *) is for handles and function and other
 * pointers. uint64 for physical addresses, size_t and int elsewhere.
 * The goal is for this module to eventually be part of OpenSolaris once
 * interfaces have become more stable.
 */

/*
 * Allocate and free physically contiguous, page aligned memory.
 *
 *
 * return value is a) NULL if contig memory not available or
 * b) virtual address of memory in kernel heap
 *
 * *phys on input is set to the upper boundary of acceptable memory
 * *phys on output returns the starting physical address
 *
 * size is the amount to allocate and must be a multiple of PAGESIZE
 */
extern void *vbi_contig_alloc(uint64_t *phys, size_t size);

/*
 * va is from vbi_contig_alloc() return value
 * size must match from vbi_contig_alloc()
 */
extern void vbi_contig_free(void *va, size_t size);


/*
 * Map physical page into kernel heap space
 *
 * return value is a) NULL if out of kernel virtual address space or
 * on success b) virtual address of memory in kernel heap
 *
 * pa is the starting physical address, must be PAGESIZE aligned
 *
 * size is the amount to map and must be a multiple of PAGESIZE
 */
extern void *vbi_kernel_map(uint64_t pa, size_t size, uint_t prot);

/*
 * Map physical pages into user part of the address space.
 *
 * returns 0 on success, non-zero on failure
 */
extern int vbi_user_map(caddr_t *va, uint_t prot, uint64_t *palist, size_t len);

/*
 * Unmap physical memory from virtual address space. No freeing of physical
 * memory is done.
 *
 * va is from a call to vbi_kernel_map() or vbi_user_map() that used sz
 */
extern void vbi_unmap(void *va, size_t size);


/*
 * return value is an OS handle for the current thread.
 */
extern void *vbi_curthread(void);


/*
 * Yield thread.
 * return value is an inexact value for:
 * 0 - the thread probably didn't yield
 * 1 - the thread probably did yield
 */
extern int vbi_yield(void);


/*
 * Timer functions, times expressed in nanoseconds.
 */
extern uint64_t vbi_timer_granularity(void);

/* old timer funcs */
extern void *vbi_timer_create(void *callback, void *arg1, void *arg2,
    uint64_t interval);
extern void vbi_timer_destroy(void *timer);
extern void vbi_timer_start(void *timer, uint64_t when);
extern void vbi_timer_stop(void *timer);

/*
 * Returns current time of day in nanoseconds.
 */
extern uint64_t vbi_tod(void);

/*
 * Process handle
 */
extern void *vbi_proc(void);

/*
 * thread functions
 */
extern void vbi_set_priority(void *thread, int priority);
extern void *vbi_thread_create(void *func, void *arg, size_t len, int priority);
extern void vbi_thread_exit(void);

/*
 * Allocate and free kernel heap suitable for use as executable text
 *
 * return value is a) NULL if memory not available or
 * b) virtual address of memory in kernel heap
 *
 * size is the amount to allocate
 *
 * Note that the virtual mappings to memory allocated by this interface are
 * locked.
 */
extern void *vbi_text_alloc(size_t size);

/*
 * va is from vbi_text_alloc() return value
 * size must match from vbi_text_alloc()
 */
extern void vbi_text_free(void *va, size_t size);

/*
 * CPU and cross call related functionality
 *
 * return current cpu identifier
 */
extern int vbi_cpu_id(void);

/*
 * return maximum possible cpu identifier
 */
extern int vbi_max_cpu_id(void);

/*
 * return max number of CPUs supported by OS
 */
extern int vbi_cpu_maxcount(void);

/*
 * return number of CPUs actually in system
 */
extern int vbi_cpu_count(void);

/*
 * returns 0 if cpu is offline, 1 if online
 */
extern int vbi_cpu_online(int c);

/*
 * disable/enable thread preemption
 */
extern void vbi_preempt_disable(void);
extern void vbi_preempt_enable(void);

/*
 * causes "func(arg)" to be invoked on all online CPUs.
 */
extern void vbi_execute_on_all(void *func, void *arg);

/*
 * causes "func(arg)" to be invoked on all CPUs except the current one.
 * should be called with preemption disabled.
 */
extern void vbi_execute_on_others(void *func, void *arg);

/*
 * causes "func(arg)" to be invoked on just one CPU
 */
extern void vbi_execute_on_one(void *func, void *arg, int c);


/*
 * Lock/unlock pages in virtual address space. Pages behind the virtual
 * mappings are pinned and virtual mappings are locked. Handles to the
 * pages are returned in the array.
 *
 * returns 0 for success, non-zero errno on failure
 */
extern int vbi_lock_va(void *addr, size_t len, void **handle);
extern void vbi_unlock_va(void *addr, size_t len, void *handle);

/*
 * Return the physical address of memory behind a VA. If the
 * memory is not locked, it may return -(uint64_t)1 to indicate that
 * no physical page is at the address currently.
 */
extern uint64_t vbi_va_to_pa(void *addr);

/* end of interfaces defined for version 1 */

extern uint_t vbi_revision_level;

/* begin interfaces defined for version 2 */

/*
 * Install/remove a call back for CPU online/offline event notification.
 *
 * The call back func is invoked with 3 arguments:
 *      void func(void *arg, int cpu, int online);
 * - arg is passed through from vbi_watch_cpus()
 * - cpu is the CPU id involved
 * - online is non-zero for a CPU that comes online and 0 for a CPU that is
 *   going offline.
 *
 * If current_too is non-zero, then a cpu online event will be invoked for all
 * currently online CPUs.
 *
 * Note there is no guarantee about which CPU the function is invoked on.
 */
typedef struct vbi_cpu_watch vbi_cpu_watch_t;
extern vbi_cpu_watch_t *vbi_watch_cpus(void (*func)(), void *arg,
    int current_too);
extern void vbi_ignore_cpus(vbi_cpu_watch_t *);
#pragma weak vbi_watch_cpus
#pragma weak vbi_ignore_cpus

/*
 * New timer interfaces
 *
 * A simple timer fires just once and on only one cpu. It may be repeating or
 * a one shot.
 *
 * Support for one shot (ie interval == 0) global timers is optional.
 *
 * For simple timers, if cpu is VBI_ANY_CPU, the timer may fire on any
 * available cpu otherwise the timer will fire only on the given cpu
 *
 * The repeating call back, func, is invoked with 2 arguments:
 * - arg is just passed through
 * - a uint64_t counter that starts at 0 and increments with each timer event.
 *   The count is per-cpu and resets whenever a cpu goes offline and comes back
 *   online.
 *
 * The when parameter is time relative to now.
 *
 * vbi_stimer_begin() may return NULL if there was an error in
 * creating the timer, for example if a requested cpu is not online.
 *
 * vbi_gtimer_begin() may return NULL when called if it does not
 * support the requested kind of timer (ie interval == 0)
 */
#define	VBI_ANY_CPU	(-1)

typedef struct vbi_stimer vbi_stimer_t;
extern vbi_stimer_t *vbi_stimer_begin(void (*func)(void *, uint64_t), void *arg,
    uint64_t when, uint64_t interval, int cpu);
extern void vbi_stimer_end(vbi_stimer_t *);
#pragma weak vbi_stimer_begin
#pragma weak vbi_stimer_end

typedef struct vbi_gtimer vbi_gtimer_t;
extern vbi_gtimer_t *vbi_gtimer_begin(void (*func)(void *, uint64_t), void *arg,
    uint64_t when, uint64_t interval);
extern void vbi_gtimer_end(vbi_gtimer_t *);
#pragma weak vbi_gtimer_begin
#pragma weak vbi_gtimer_end


/* end of interfaces defined for version 2 */

/* begin interfaces defined for version 3 */

/*
 * returns non-zero if the thread might be preempted at any time
 */
extern int vbi_is_preempt_enabled(void);

/* end of interfaces defined for version 3 */

/* begin interfaces defined for version 4 */

/*
 * poke the given cpu with an IPI
 */
extern void vbi_poke_cpu(int);

/* end of interfaces defined for version 4 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_VBI_H */
