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

#pragma ident	"@(#)vbi.c	1.1	08/05/26 SMI"

/*
 * Private interfaces for VirtualBox access to Solaris kernel internal
 * facilities.
 *
 * See sys/vbi.h for what each function does.
 */

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
#include <sys/kobj.h>
#include <sys/x_call.h>
#include <sys/x86_archext.h>
#include <vm/hat.h>
#include <vm/seg_vn.h>
#include <vm/seg_kmem.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>

#include <sys/vbi.h>

/*
 * If we are running on an old version of Solaris, then
 * we have to use dl_lookup to find contig_free().
 */
extern void *contig_alloc(size_t, ddi_dma_attr_t *, uintptr_t, int);
extern void contig_free(void *, size_t);
#pragma weak contig_free
static void (*p_contig_free)(void *, size_t) = contig_free;

/*
 * Workarounds for running on old versions of solaris with lower NCPU.
 * If we detect this, the assumption is that NCPU was such that a cpuset_t
 * is just a ulong_t
 */
static int use_old_xc_call = 0;
static void (*p_xc_call)() = (void (*)())xc_call;
#pragma weak cpuset_all
#pragma weak cpuset_all_but

static struct modlmisc vbi_modlmisc = {
	&mod_miscops, "Vbox Interfaces Ver 1"
};

static struct modlinkage vbi_modlinkage = {
	MODREV_1, (void *)&vbi_modlmisc, NULL
};

#define	IS_KERNEL(v)	((uintptr_t)(v) >= kernelbase)

int
_init(void)
{
	int err;

	/*
	 * Check to see if this version of virtualbox interface module will work
	 * with the kernel. The sizeof (cpuset_t) is problematic, as it changed
	 * with the change to NCPU in nevada build 87 and S10U6.
	 */
	if (max_cpuid + 1 != NCPU)
		use_old_xc_call = 1;

	/*
	 * In older versions of Solaris contig_free() is a static routine.
	 */
	if (p_contig_free == NULL) {
		p_contig_free = (void (*)(void *, size_t))
		    kobj_getsymvalue("contig_free", 1);
		if (p_contig_free == NULL) {
			cmn_err(CE_NOTE, " contig_free() not found in kernel");
			return (EINVAL);
		}
	}

	err = mod_install(&vbi_modlinkage);
	if (err != 0)
		return (err);

	return (0);
}

int
_fini(void)
{
	int err = mod_remove(&vbi_modlinkage);
	if (err != 0)
		return (err);

	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&vbi_modlinkage, modinfop));
}

static ddi_dma_attr_t base_attr = {
	DMA_ATTR_V0,		/* Version Number */
	(uint64_t)0,		/* lower limit */
	(uint64_t)0,		/* high limit */
	(uint64_t)0xffffffff,	/* counter limit */
	(uint64_t)MMU_PAGESIZE,	/* alignment */
	(uint64_t)MMU_PAGESIZE,	/* burst size */
	(uint64_t)MMU_PAGESIZE,	/* effective DMA size */
	(uint64_t)0xffffffff,	/* max DMA xfer size */
	(uint64_t)0xffffffff,	/* segment boundary */
	1,			/* list length (1 for contiguous) */
	1,			/* device granularity */
	0			/* bus-specific flags */
};

void *
vbi_contig_alloc(uint64_t *phys, size_t size)
{
	ddi_dma_attr_t attr;
	pfn_t pfn;
	void *ptr;

	if ((size >> MMU_PAGESHIFT) << MMU_PAGESHIFT != size)
		return (NULL);

	attr = base_attr;
	attr.dma_attr_addr_hi = *phys;
	ptr = contig_alloc(size, &attr, MMU_PAGESIZE, 1);

	if (ptr == NULL)
		return (NULL);

	pfn = hat_getpfnum(kas.a_hat, (caddr_t)ptr);
	if (pfn == PFN_INVALID)
		panic("vbi_contig_alloc(): hat_getpfnum() failed\n");
	*phys = (uint64_t)pfn << MMU_PAGESHIFT;
	return (ptr);
}

void
vbi_contig_free(void *va, size_t size)
{
	p_contig_free(va, size);
}

void *
vbi_kernel_map(uint64_t pa, size_t size, uint_t prot)
{
	caddr_t va;

	if ((pa & MMU_PAGEOFFSET) || (size & MMU_PAGEOFFSET))
		return (NULL);

	va = vmem_alloc(heap_arena, size, VM_SLEEP);

	hat_devload(kas.a_hat, va, size, (pfn_t)(pa >> MMU_PAGESHIFT),
	    prot, HAT_LOAD | HAT_LOAD_LOCK | HAT_UNORDERED_OK);

	return (va);
}

void
vbi_unmap(void *va, size_t size)
{
	if (IS_KERNEL(va)) {
		hat_unload(kas.a_hat, va, size, HAT_UNLOAD | HAT_UNLOAD_UNLOCK);
		vmem_free(heap_arena, va, size);
	} else {
		struct as *as = curproc->p_as;

		as_rangelock(as);
		(void) as_unmap(as, va, size);
		as_rangeunlock(as);
	}
}

void *
vbi_curthread(void)
{
	return (curthread);
}

int
vbi_yield(void)
{
	int rv = 0;

	kpreempt_disable();
	if (curthread->t_preempt == 1 && CPU->cpu_kprunrun)
		rv = 1;
	kpreempt_enable();
	return (rv);
}

uint64_t
vbi_timer_granularity(void)
{
	return (nsec_per_tick);
}

typedef struct vbi_timer {
	cyc_handler_t	vbi_handler;
	cyclic_id_t	vbi_cyclic;
	uint64_t	vbi_interval;
	void		(*vbi_func)();
	void		*vbi_arg1;
	void		*vbi_arg2;
} vbi_timer_t;

static void
vbi_timer_callback(void *arg)
{
	vbi_timer_t *t = arg;

	if (t->vbi_interval == 0)
		vbi_timer_stop(arg);
	t->vbi_func(t->vbi_arg1, t->vbi_arg2);
}

void *
vbi_timer_create(void *callback, void *arg1, void *arg2, uint64_t interval)
{
	vbi_timer_t *t = kmem_zalloc(sizeof (*t), KM_SLEEP);

	t->vbi_func = (void (*)())callback;
	t->vbi_arg1 = arg1;
	t->vbi_arg2 = arg2;
	t->vbi_handler.cyh_func = vbi_timer_callback;
	t->vbi_handler.cyh_arg = (void *)t;
	t->vbi_handler.cyh_level = CY_LOCK_LEVEL;
	t->vbi_cyclic = CYCLIC_NONE;
	t->vbi_interval = interval;
	return (t);
}

void
vbi_timer_destroy(void *timer)
{
	vbi_timer_t *t = timer;
	if (t != NULL) {
		vbi_timer_stop(timer);
		kmem_free(t, sizeof (*t));
	}
}

void
vbi_timer_start(void *timer, uint64_t when)
{
	vbi_timer_t *t = timer;
	cyc_time_t fire_time;
	uint64_t interval = t->vbi_interval;

	mutex_enter(&cpu_lock);
	when += gethrtime();
	fire_time.cyt_when = when;
	if (interval == 0)
		fire_time.cyt_interval = when;
	else
		fire_time.cyt_interval = interval;
	t->vbi_cyclic = cyclic_add(&t->vbi_handler, &fire_time);
	mutex_exit(&cpu_lock);
}

void
vbi_timer_stop(void *timer)
{
	vbi_timer_t *t = timer;

	if (t->vbi_cyclic == CYCLIC_NONE)
		return;
	mutex_enter(&cpu_lock);
	if (t->vbi_cyclic != CYCLIC_NONE) {
		cyclic_remove(t->vbi_cyclic);
		t->vbi_cyclic = CYCLIC_NONE;
	}
	mutex_exit(&cpu_lock);
}

uint64_t
vbi_tod(void)
{
	timestruc_t ts;

	mutex_enter(&tod_lock);
	ts = tod_get();
	mutex_exit(&tod_lock);
	return ((uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec);
}


void *
vbi_proc(void)
{
	return (curproc);
}

void
vbi_set_priority(void *thread, int priority)
{
	kthread_t *t = thread;

	thread_lock(t);
	(void) thread_change_pri(t, priority, 0);
	thread_unlock(t);
}

void *
vbi_thread_create(void *func, void *arg, size_t len, int priority)
{
	kthread_t *t;

	t = thread_create(NULL, NULL, (void (*)())func, arg, len,
	    curproc, LMS_USER, priority);
	return (t);
}

void
vbi_thread_exit(void)
{
	thread_exit();
}

void *
vbi_text_alloc(size_t size)
{
	return (segkmem_alloc(heaptext_arena, size, KM_SLEEP));
}

void
vbi_text_free(void *va, size_t size)
{
	segkmem_free(heaptext_arena, va, size);
}

int
vbi_cpu_id(void)
{
	return (CPU->cpu_id);
}

int
vbi_max_cpu_id(void)
{
	return (NCPU - 1);
}

int
vbi_cpu_maxcount(void)
{
	return (NCPU);
}

int
vbi_cpu_count(void)
{
	return (ncpus);
}

int
vbi_cpu_online(int c)
{
	int x;

	mutex_enter(&cpu_lock);
	x = cpu_is_online(cpu[c]);
	mutex_exit(&cpu_lock);
	return (x);
}

void
vbi_preempt_disable(void)
{
	kpreempt_disable();
}

void
vbi_preempt_enable(void)
{
	kpreempt_enable();
}

void
vbi_execute_on_all(void *func, void *arg)
{
	cpuset_t set;
	ulong_t hack_set;
	int i;

	/*
	 * hack for a kernel compiled with the different NCPU than this module
	 */
	if (use_old_xc_call) {
		hack_set = 0;
		for (i = 0; i < ncpus; ++i)
			hack_set |= 1 << i;
		p_xc_call((xc_arg_t)arg, 0, 0, X_CALL_HIPRI, hack_set,
		    (xc_func_t)func);
	} else {
		CPUSET_ALL(set);
		xc_call((xc_arg_t)arg, 0, 0, X_CALL_HIPRI, set,
		    (xc_func_t)func);
	}
}

void
vbi_execute_on_others(void *func, void *arg)
{
	cpuset_t set;
	ulong_t hack_set;
	int i;

	/*
	 * hack for a kernel compiled with the different NCPU than this module
	 */
	if (use_old_xc_call) {
		hack_set = 0;
		for (i = 0; i < ncpus; ++i) {
			if (i != CPU->cpu_id)
				hack_set |= 1 << i;
		}
		p_xc_call((xc_arg_t)arg, 0, 0, X_CALL_HIPRI, hack_set,
		    (xc_func_t)func);
	} else {
		CPUSET_ALL_BUT(set, CPU->cpu_id);
		xc_call((xc_arg_t)arg, 0, 0, X_CALL_HIPRI, set,
		    (xc_func_t)func);
	}
}

void
vbi_execute_on_one(void *func, void *arg, int c)
{
	cpuset_t set;
	ulong_t hack_set;

	/*
	 * hack for a kernel compiled with the different NCPU than this module
	 */
	if (use_old_xc_call) {
		hack_set = 1 << c;
		p_xc_call((xc_arg_t)arg, 0, 0, X_CALL_HIPRI, hack_set,
		    (xc_func_t)func);
	} else {
		CPUSET_ALL_BUT(set, c);
		xc_call((xc_arg_t)arg, 0, 0, X_CALL_HIPRI, set,
		    (xc_func_t)func);
	}
}

int
vbi_lock_va(void *addr, size_t len, void **handle)
{
	page_t **ppl;
	int rc = 0;

	if (IS_KERNEL(addr)) {
		/* kernel mappings on x86 are always locked */
		*handle = NULL;
	} else {
		rc = as_pagelock(curproc->p_as, &ppl, (caddr_t)addr, len,
		    S_WRITE);
		if (rc != 0)
			return (rc);
		*handle = (void *)ppl;
	}
	return (rc);
}

void
vbi_unlock_va(void *addr, size_t len, void *handle)
{
	page_t **ppl = (page_t **)handle;

	if (IS_KERNEL(addr))
		ASSERT(handle == NULL);
	else
		as_pageunlock(curproc->p_as, ppl, (caddr_t)addr, len, S_WRITE);
}

uint64_t
vbi_va_to_pa(void *addr)
{
	struct hat *hat;
	pfn_t pfn;
	uintptr_t v = (uintptr_t)addr;

	if (IS_KERNEL(v))
		hat = kas.a_hat;
	else
		hat = curproc->p_as->a_hat;
	pfn = hat_getpfnum(hat, (caddr_t)(v & MMU_PAGEMASK));
	if (pfn == PFN_INVALID)
		return (-(uint64_t)1);
	return (((uint64_t)pfn << MMU_PAGESHIFT) | (v & MMU_PAGEOFFSET));
}


struct segvbi_crargs {
	uint64_t *palist;
	uint_t prot;
};

struct segvbi_data {
	uint_t prot;
};

static struct seg_ops segvbi_ops;

static int
segvbi_create(struct seg *seg, void *args)
{
	struct segvbi_crargs *a = args;
	struct segvbi_data *data;
	struct as *as = seg->s_as;
	int error = 0;
	caddr_t va;
	ulong_t pgcnt;
	ulong_t p;

	hat_map(as->a_hat, seg->s_base, seg->s_size, HAT_MAP);
	data = kmem_zalloc(sizeof (*data), KM_SLEEP);
	data->prot = a->prot | PROT_USER;

	seg->s_ops = &segvbi_ops;
	seg->s_data = data;

	/*
	 * now load locked mappings to the pages
	 */
	va = seg->s_base;
	ASSERT(((uintptr_t)va & MMU_PAGEOFFSET) == 0);
	pgcnt = seg->s_size >> MMU_PAGESHIFT;
	for (p = 0; p < pgcnt; ++p, va += MMU_PAGESIZE) {
		ASSERT((a->palist[p] & MMU_PAGEOFFSET) == 0);
		hat_devload(as->a_hat, va,
		    MMU_PAGESIZE, a->palist[p] >> MMU_PAGESHIFT,
		    data->prot | HAT_UNORDERED_OK, HAT_LOAD | HAT_LOAD_LOCK);
	}

	return (error);
}

/*
 * Duplicate a seg and return new segment in newseg.
 */
static int
segvbi_dup(struct seg *seg, struct seg *newseg)
{
	struct segvbi_data *data = seg->s_data;
	struct segvbi_data *ndata;

	ndata = kmem_zalloc(sizeof (*data), KM_SLEEP);
	ndata->prot = data->prot;
	newseg->s_ops = &segvbi_ops;
	newseg->s_data = ndata;

	return (0);
}

/*ARGSUSED*/
static int
segvbi_unmap(struct seg *seg, caddr_t addr, size_t len)
{
	if (addr < seg->s_base || addr + len > seg->s_base + seg->s_size ||
	    (len & MMU_PAGEOFFSET) || ((uintptr_t)addr & MMU_PAGEOFFSET))
		panic("segvbi_unmap");

	if (addr != seg->s_base || len != seg->s_size)
		return (ENOTSUP);

	hat_unload(seg->s_as->a_hat, addr, len,
	    HAT_UNLOAD_UNMAP | HAT_UNLOAD_UNLOCK);

	seg_free(seg);
	return (0);
}

static void
segvbi_free(struct seg *seg)
{
	struct segvbi_data *data = seg->s_data;
	kmem_free(data, sizeof (*data));
}

/*
 * We never demand-fault for seg_vbi.
 */
/*ARGSUSED*/
static int
segvbi_fault(struct hat *hat, struct seg *seg, caddr_t addr, size_t len,
    enum fault_type type, enum seg_rw rw)
{
	return (FC_MAKE_ERR(EFAULT));
}

/*ARGSUSED*/
static int
segvbi_faulta(struct seg *seg, caddr_t addr)
{
	return (0);
}

/*ARGSUSED*/
static int
segvbi_setprot(struct seg *seg, caddr_t addr, size_t len, uint_t prot)
{
	return (EACCES);
}

/*ARGSUSED*/
static int
segvbi_checkprot(struct seg *seg, caddr_t addr, size_t len, uint_t prot)
{
	return (EINVAL);
}

/*ARGSUSED*/
static int
segvbi_kluster(struct seg *seg, caddr_t addr, ssize_t delta)
{
	return (-1);
}

/*ARGSUSED*/
static int
segvbi_sync(struct seg *seg, caddr_t addr, size_t len, int attr, uint_t flags)
{
	return (0);
}

/*ARGSUSED*/
static size_t
segvbi_incore(struct seg *seg, caddr_t addr, size_t len, char *vec)
{
	size_t v;

	for (v = 0, len = (len + MMU_PAGEOFFSET) & MMU_PAGEMASK; len;
	    len -= MMU_PAGESIZE, v += MMU_PAGESIZE)
		*vec++ = 1;
	return (v);
}

/*ARGSUSED*/
static int
segvbi_lockop(struct seg *seg, caddr_t addr,
    size_t len, int attr, int op, ulong_t *lockmap, size_t pos)
{
	return (0);
}

/*ARGSUSED*/
static int
segvbi_getprot(struct seg *seg, caddr_t addr, size_t len, uint_t *protv)
{
	struct segvbi_data *data = seg->s_data;
	return (data->prot);
}

static u_offset_t
segvbi_getoffset(struct seg *seg, caddr_t addr)
{
	return ((uintptr_t)addr - (uintptr_t)seg->s_base);
}

/*ARGSUSED*/
static int
segvbi_gettype(struct seg *seg, caddr_t addr)
{
	return (MAP_SHARED);
}

static vnode_t vbivp;

/*ARGSUSED*/
static int
segvbi_getvp(struct seg *seg, caddr_t addr, struct vnode **vpp)
{
	*vpp = &vbivp;
	return (0);
}

/*ARGSUSED*/
static int
segvbi_advise(struct seg *seg, caddr_t addr, size_t len, uint_t behav)
{
	return (0);
}

/*ARGSUSED*/
static void
segvbi_dump(struct seg *seg)
{}

/*ARGSUSED*/
static int
segvbi_pagelock(struct seg *seg, caddr_t addr, size_t len,
    struct page ***ppp, enum lock_type type, enum seg_rw rw)
{
	return (ENOTSUP);
}

/*ARGSUSED*/
static int
segvbi_setpagesize(struct seg *seg, caddr_t addr, size_t len, uint_t szc)
{
	return (ENOTSUP);
}

/*ARGSUSED*/
static int
segvbi_getmemid(struct seg *seg, caddr_t addr, memid_t *memid)
{
	return (ENODEV);
}

/*ARGSUSED*/
static lgrp_mem_policy_info_t *
segvbi_getpolicy(struct seg *seg, caddr_t addr)
{
	return (NULL);
}

/*ARGSUSED*/
static int
segvbi_capable(struct seg *seg, segcapability_t capability)
{
	return (0);
}

static struct seg_ops segvbi_ops = {
	segvbi_dup,
	segvbi_unmap,
	segvbi_free,
	segvbi_fault,
	segvbi_faulta,
	segvbi_setprot,
	segvbi_checkprot,
	(int (*)())segvbi_kluster,
	(size_t (*)(struct seg *))NULL, /* swapout */
	segvbi_sync,
	segvbi_incore,
	segvbi_lockop,
	segvbi_getprot,
	segvbi_getoffset,
	segvbi_gettype,
	segvbi_getvp,
	segvbi_advise,
	segvbi_dump,
	segvbi_pagelock,
	segvbi_setpagesize,
	segvbi_getmemid,
	segvbi_getpolicy,
	segvbi_capable
};



/*
 * Interfaces to inject physical pages into user address space
 * and later remove them.
 */
int
vbi_user_map(caddr_t *va, uint_t prot, uint64_t *palist, size_t len)
{
	struct as *as = curproc->p_as;
	struct segvbi_crargs args;
	int error = 0;

	args.palist = palist;
	args.prot = prot;
	as_rangelock(as);
	map_addr(va, len, 0, 0, MAP_SHARED);
	ASSERT(((uintptr_t)*va & MMU_PAGEOFFSET) == 0);
	ASSERT((len & MMU_PAGEOFFSET) == 0);
	ASSERT(len != 0);
	if (*va != NULL)
		error = as_map(as, *va, len, segvbi_create, &args);
	else
		error = ENOMEM;
	as_rangeunlock(as);
	return (error);
}

/*
 * This is revision 1 of the interface. As more functions are added,
 * they should go after this point in the file and the revision level
 * increased.
 */
uint_t vbi_revision_level = 1;
