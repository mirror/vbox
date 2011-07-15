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
 * Copyright 2010-2011 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

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
#include <sys/machparam.h>
#include <sys/utsname.h>
#include <sys/ctf_api.h>

#include <iprt/assert.h>

#include "vbi.h"

#define VBIPROC() ((proc_t *)vbi_proc())

/*
 * We have to use dl_lookup to find contig_free().
 */
extern void *contig_alloc(size_t, ddi_dma_attr_t *, uintptr_t, int);
extern void contig_free(void *, size_t);
#pragma weak contig_free
static void (*p_contig_free)(void *, size_t) = contig_free;

/*
 * We have to use dl_lookup to find kflt_init() and thereby use kernel pages from
 * the freelists if we no longer get user pages from freelist and cachelists.
 */
/* Introduced in v9 */
static int use_kflt = 0;
static page_t *vbi_page_get_fromlist(uint_t freelist, caddr_t virtAddr, size_t pgsize);


/*
 * Workarounds for running on old versions of solaris with different cross call
 * interfaces. If we find xc_init_cpu() in the kernel, then just use the defined
 * interfaces for xc_call() from the include file where the xc_call()
 * interfaces just takes a pointer to a ulong_t array. The array must be long
 * enough to hold "ncpus" bits at runtime.

 * The reason for the hacks is that using the type "cpuset_t" is pretty much
 * impossible from code built outside the Solaris source repository that wants
 * to run on multiple releases of Solaris.
 *
 * For old style xc_call()s, 32 bit solaris and older 64 bit versions use
 * "ulong_t" as cpuset_t.
 *
 * Later versions of 64 bit Solaris used: struct {ulong_t words[x];}
 * where "x" depends on NCPU.
 *
 * We detect the difference in 64 bit support by checking the kernel value of
 * max_cpuid, which always holds the compiled value of NCPU - 1.
 *
 * If Solaris increases NCPU to more than 256, this module will continue
 * to work on all versions of Solaris as long as the number of installed
 * CPUs in the machine is <= VBI_NCPU. If VBI_NCPU is increased, this code
 * has to be re-written some to provide compatibility with older Solaris which
 * expects cpuset_t to be based on NCPU==256 -- or we discontinue support
 * of old Nevada/S10.
 */
static int use_old = 0;
static int use_old_with_ulong = 0;
static void (*p_xc_call)() = (void (*)())xc_call;

#define	VBI_NCPU	256
#define	VBI_SET_WORDS	(VBI_NCPU / (sizeof (ulong_t) * 8))
typedef struct vbi_cpuset {
	ulong_t words[VBI_SET_WORDS];
} vbi_cpuset_t;
#define	X_CALL_HIPRI	(2)	/* for old Solaris interface */

/*
 * module linkage stuff
 */
#if 0
static struct modlmisc vbi_modlmisc = {
	&mod_miscops, "VirtualBox Interfaces V8"
};

static struct modlinkage vbi_modlinkage = {
	MODREV_1, { (void *)&vbi_modlmisc, NULL }
};
#endif

extern uintptr_t kernelbase;
#define	IS_KERNEL(v)	((uintptr_t)(v) >= kernelbase)

#if 0
static int vbi_verbose = 0;

#define VBI_VERBOSE(msg) {if (vbi_verbose) cmn_err(CE_WARN, msg);}
#endif

/* Introduced in v8 */
static int vbi_is_initialized = 0;

/* Which offsets will be used */
static int off_cpu_runrun       = -1;
static int off_cpu_kprunrun     = -1;
static int off_t_preempt        = -1;

#define VBI_T_PREEMPT            (*((char *)curthread + off_t_preempt))
#define VBI_CPU_KPRUNRUN         (*((char *)CPU + off_cpu_kprunrun))
#define VBI_CPU_RUNRUN           (*((char *)CPU + off_cpu_runrun))

#undef kpreempt_disable
#undef kpreempt_enable

#define	VBI_PREEMPT_DISABLE()			\
	{									\
		VBI_T_PREEMPT++;				\
		ASSERT(VBI_T_PREEMPT >= 1);		\
	}
#define	VBI_PREEMPT_ENABLE()			\
	{									\
		ASSERT(VBI_T_PREEMPT >= 1);		\
		if (--VBI_T_PREEMPT == 0 &&	\
		    VBI_CPU_RUNRUN)				\
			kpreempt(KPREEMPT_SYNC);	\
	}

/* End of v6 intro */

#if 0
int
_init(void)
{
	int err = vbi_init();
	if (!err)
		err = mod_install(&vbi_modlinkage);
	return (err);
}
#endif

static int
vbi_get_ctf_member_offset(ctf_file_t *ctfp, const char *structname, const char *membername, int *offset)
{
	AssertReturn(ctfp, CTF_ERR);
	AssertReturn(structname, CTF_ERR);
	AssertReturn(membername, CTF_ERR);
	AssertReturn(offset, CTF_ERR);

	ctf_id_t typeident = ctf_lookup_by_name(ctfp, structname);
	if (typeident != CTF_ERR)
	{
		ctf_membinfo_t memberinfo;
		bzero(&memberinfo, sizeof(memberinfo));
		if (ctf_member_info(ctfp, typeident, membername, &memberinfo) != CTF_ERR)
		{
			*offset = (memberinfo.ctm_offset >> 3);
			cmn_err(CE_NOTE, "%s::%s at %d\n", structname, membername, *offset);
			return (0);
		}
		else
			cmn_err(CE_NOTE, "ctf_member_info failed for struct %s member %s\n", structname, membername);
	}
	else
		cmn_err(CE_NOTE, "ctf_lookup_by_name failed for struct %s\n", structname);

	return (CTF_ERR);
}


int
vbi_init(void)
{
	/*
	 * Check to see if this version of virtualbox interface module will work
	 * with the kernel.
	 */
	if (kobj_getsymvalue("xc_init_cpu", 1) != NULL) {
		/*
		 * Our bit vector storage needs to be large enough for the
		 * actual number of CPUs running in the system.
		 */
		if (ncpus > VBI_NCPU) {
			cmn_err(CE_NOTE, "cpu count mismatch.\n");
			return (EINVAL);
		}
	} else {
		use_old = 1;
		if (max_cpuid + 1 == sizeof(ulong_t) * 8)
			use_old_with_ulong = 1;
		else if (max_cpuid + 1 != VBI_NCPU)
		{
			cmn_err(CE_NOTE, "cpuset_t size mismatch. probably too old a kernel.\n");
			return (EINVAL);	/* cpuset_t size mismatch */
		}
	}

	/*
	 * In older versions of Solaris contig_free() is a static routine.
	 */
	if (p_contig_free == NULL) {
		p_contig_free = (void (*)(void *, size_t))
			kobj_getsymvalue("contig_free", 1);
		if (p_contig_free == NULL) {
			cmn_err(CE_NOTE, "contig_free() not found in kernel\n");
			return (EINVAL);
		}
	}

	/*
	 * Use kernel page freelist flags to get pages from kernel page freelists
	 * while allocating physical pages, once the userpages are exhausted.
	 * snv_161+, see @bugref{5632}.
	 */
	if (kobj_getsymvalue("kflt_init", 1) != NULL)
	{
		int *p_kflt_disable = (int*)kobj_getsymvalue("kflt_disable", 1);	/* amd64 only, on 32-bit kflt's are disabled. */
		if (p_kflt_disable && *p_kflt_disable == 0)
		{
			use_kflt = 1;
		}
	}

	/*
	 * CTF probing for fluid, private members.
	 */
	int err = 0;
	modctl_t *genunix_modctl = mod_hold_by_name("genunix");
	if (genunix_modctl)
	{
		ctf_file_t *ctfp = ctf_modopen(genunix_modctl->mod_mp, &err);
		if (ctfp)
		{
			do {
				err = vbi_get_ctf_member_offset(ctfp, "kthread_t", "t_preempt", &off_t_preempt); AssertBreak(!err);
				err = vbi_get_ctf_member_offset(ctfp, "cpu_t", "cpu_runrun", &off_cpu_runrun); AssertBreak(!err);
				err = vbi_get_ctf_member_offset(ctfp, "cpu_t", "cpu_kprunrun", &off_cpu_kprunrun); AssertBreak(!err);
			} while (0);
		}

		mod_release_mod(genunix_modctl);
	}
	else
	{
		cmn_err(CE_NOTE, "failed to open module genunix.\n");
		err = EINVAL;
	}

	if (err)
		return (EINVAL);

	vbi_is_initialized = 1;

	return (0);
}

#if 0
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
#endif


static ddi_dma_attr_t base_attr = {
	DMA_ATTR_V0,		/* Version Number */
	(uint64_t)0,		/* lower limit */
	(uint64_t)0,		/* high limit */
	(uint64_t)0xffffffff,	/* counter limit */
	(uint64_t)PAGESIZE,	/* pagesize alignment */
	(uint64_t)PAGESIZE,	/* pagesize burst size */
	(uint64_t)PAGESIZE,	/* pagesize effective DMA size */
	(uint64_t)0xffffffff,	/* max DMA xfer size */
	(uint64_t)0xffffffff,	/* segment boundary */
	1,			/* list length (1 for contiguous) */
	1,			/* device granularity */
	0			/* bus-specific flags */
};

static void *
vbi_internal_alloc(uint64_t *phys, size_t size, uint64_t alignment, int contig)
{
	ddi_dma_attr_t attr;
	pfn_t pfn;
	void *ptr;
	uint_t npages;

	if ((size & PAGEOFFSET) != 0)
		return (NULL);
	npages = (size + PAGESIZE - 1) >> PAGESHIFT;
	if (npages == 0)
		return (NULL);

	attr = base_attr;
	attr.dma_attr_addr_hi = *phys;
	attr.dma_attr_align   = alignment;
	if (!contig)
		attr.dma_attr_sgllen = npages;
	ptr = contig_alloc(size, &attr, PAGESIZE, 1);

	if (ptr == NULL) {
		cmn_err(CE_NOTE, "vbi_internal_alloc() failure for %lu bytes contig=%d", size, contig);
		return (NULL);
	}

	pfn = hat_getpfnum(kas.a_hat, (caddr_t)ptr);
	if (pfn == PFN_INVALID)
		panic("vbi_contig_alloc(): hat_getpfnum() failed\n");
	*phys = (uint64_t)pfn << PAGESHIFT;
	return (ptr);
}

void *
vbi_contig_alloc(uint64_t *phys, size_t size)
{
	/* Obsolete */
	return (vbi_internal_alloc(phys, size, PAGESIZE /* alignment */, 1 /* contiguous */));
}

void
vbi_contig_free(void *va, size_t size)
{
	/* Obsolete */
	p_contig_free(va, size);
}

void *
vbi_kernel_map(uint64_t pa, size_t size, uint_t prot)
{
	caddr_t va;

	if ((pa & PAGEOFFSET) || (size & PAGEOFFSET)) {
		cmn_err(CE_NOTE, "vbi_kernel_map() bad pa (0x%lx) or size (%lu)", pa, size);
		return (NULL);
	}

	va = vmem_alloc(heap_arena, size, VM_SLEEP);

	hat_devload(kas.a_hat, va, size, (pfn_t)(pa >> PAGESHIFT),
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
		struct as *as = VBIPROC()->p_as;

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

	vbi_preempt_disable();

	char tpr = VBI_T_PREEMPT;
	char kpr = VBI_CPU_KPRUNRUN;
	if (tpr == 1 && kpr)
		rv = 1;

	vbi_preempt_enable();
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
	proc_t *p;
	drv_getparm(UPROCP, &p);
	return (p);
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
vbi_thread_create(void (*func)(void *), void *arg, size_t len, int priority)
{
	kthread_t *t;

	t = thread_create(NULL, NULL, (void (*)())func, arg, len,
		VBIPROC(), TS_RUN, priority);
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
	return (max_cpuid);
}

int
vbi_cpu_maxcount(void)
{
	return (max_cpuid + 1);
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
	VBI_PREEMPT_DISABLE();
}

void
vbi_preempt_enable(void)
{
	VBI_PREEMPT_ENABLE();
}

void
vbi_execute_on_all(void *func, void *arg)
{
	vbi_cpuset_t set;
	int i;

	for (i = 0; i < VBI_SET_WORDS; ++i)
		set.words[i] = (ulong_t)-1L;
	if (use_old) {
		if (use_old_with_ulong) {
			p_xc_call((xc_arg_t)arg, 0, 0, X_CALL_HIPRI,
				set.words[0], (xc_func_t)func);
		} else {
			p_xc_call((xc_arg_t)arg, 0, 0, X_CALL_HIPRI,
				set, (xc_func_t)func);
		}
	} else {
		xc_call((xc_arg_t)arg, 0, 0, &set.words[0], (xc_func_t)func);
	}
}

void
vbi_execute_on_others(void *func, void *arg)
{
	vbi_cpuset_t set;
	int i;

	for (i = 0; i < VBI_SET_WORDS; ++i)
		set.words[i] = (ulong_t)-1L;
	BT_CLEAR(set.words, vbi_cpu_id());
	if (use_old) {
		if (use_old_with_ulong) {
			p_xc_call((xc_arg_t)arg, 0, 0, X_CALL_HIPRI,
				set.words[0], (xc_func_t)func);
		} else {
			p_xc_call((xc_arg_t)arg, 0, 0, X_CALL_HIPRI,
				set, (xc_func_t)func);
		}
	} else {
		xc_call((xc_arg_t)arg, 0, 0, &set.words[0], (xc_func_t)func);
	}
}

void
vbi_execute_on_one(void *func, void *arg, int c)
{
	vbi_cpuset_t set;
	int i;

	for (i = 0; i < VBI_SET_WORDS; ++i)
		set.words[i] = 0;
	BT_SET(set.words, c);
	if (use_old) {
		if (use_old_with_ulong) {
			p_xc_call((xc_arg_t)arg, 0, 0, X_CALL_HIPRI,
				set.words[0], (xc_func_t)func);
		} else {
			p_xc_call((xc_arg_t)arg, 0, 0, X_CALL_HIPRI,
				set, (xc_func_t)func);
		}
	} else {
		xc_call((xc_arg_t)arg, 0, 0, &set.words[0], (xc_func_t)func);
	}
}

int
vbi_lock_va(void *addr, size_t len, int access, void **handle)
{
	faultcode_t err;

	/*
	 * kernel mappings on x86 are always locked, so only handle user.
	 */
	*handle = NULL;
	if (!IS_KERNEL(addr)) {
		err = as_fault(VBIPROC()->p_as->a_hat, VBIPROC()->p_as,
			(caddr_t)addr, len, F_SOFTLOCK, access);
		if (err != 0) {
			cmn_err(CE_NOTE, "vbi_lock_va() failed to lock");
			return (-1);
		}
	}
	return (0);
}

/*ARGSUSED*/
void
vbi_unlock_va(void *addr, size_t len, int access, void *handle)
{
	if (!IS_KERNEL(addr))
		as_fault(VBIPROC()->p_as->a_hat, VBIPROC()->p_as,
			(caddr_t)addr, len, F_SOFTUNLOCK, access);
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
		hat = VBIPROC()->p_as->a_hat;
	pfn = hat_getpfnum(hat, (caddr_t)(v & PAGEMASK));
	if (pfn == PFN_INVALID)
		return (-(uint64_t)1);
	return (((uint64_t)pfn << PAGESHIFT) | (v & PAGEOFFSET));
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
	pgcnt = (seg->s_size + PAGESIZE - 1) >> PAGESHIFT;
	for (p = 0; p < pgcnt; ++p, va += PAGESIZE) {
		hat_devload(as->a_hat, va,
			PAGESIZE, a->palist[p] >> PAGESHIFT,
			data->prot | HAT_UNORDERED_OK, HAT_LOAD | HAT_LOAD_LOCK);
	}

	return (0);
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

static int
segvbi_unmap(struct seg *seg, caddr_t addr, size_t len)
{
	if (addr < seg->s_base || addr + len > seg->s_base + seg->s_size ||
		(len & PAGEOFFSET) || ((uintptr_t)addr & PAGEOFFSET))
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
 * We would demand fault if the (u)read() path would SEGOP_FAULT()
 * on buffers mapped in via vbi_user_map() i.e. prefaults before DMA.
 * Don't fail in such case where we're called directly, see #5047.
 */
static int
segvbi_fault(struct hat *hat, struct seg *seg, caddr_t addr, size_t len,
	enum fault_type type, enum seg_rw rw)
{
	return (0);
}

static int
segvbi_faulta(struct seg *seg, caddr_t addr)
{
	return (0);
}

static int
segvbi_setprot(struct seg *seg, caddr_t addr, size_t len, uint_t prot)
{
	return (EACCES);
}

static int
segvbi_checkprot(struct seg *seg, caddr_t addr, size_t len, uint_t prot)
{
	return (EINVAL);
}

static int
segvbi_kluster(struct seg *seg, caddr_t addr, ssize_t delta)
{
	return (-1);
}

static int
segvbi_sync(struct seg *seg, caddr_t addr, size_t len, int attr, uint_t flags)
{
	return (0);
}

static size_t
segvbi_incore(struct seg *seg, caddr_t addr, size_t len, char *vec)
{
	size_t v;

	for (v = 0, len = (len + PAGEOFFSET) & PAGEMASK; len;
		len -= PAGESIZE, v += PAGESIZE)
		*vec++ = 1;
	return (v);
}

static int
segvbi_lockop(struct seg *seg, caddr_t addr,
	size_t len, int attr, int op, ulong_t *lockmap, size_t pos)
{
	return (0);
}

static int
segvbi_getprot(struct seg *seg, caddr_t addr, size_t len, uint_t *protv)
{
	struct segvbi_data *data = seg->s_data;
	size_t pgno = seg_page(seg, addr + len) - seg_page(seg, addr) + 1;
	if (pgno != 0)
	{
		do
		{
			pgno--;
			protv[pgno] = data->prot;
		} while (pgno != 0);
	}
	return (0);
}

static u_offset_t
segvbi_getoffset(struct seg *seg, caddr_t addr)
{
	return ((uintptr_t)addr - (uintptr_t)seg->s_base);
}

static int
segvbi_gettype(struct seg *seg, caddr_t addr)
{
	return (MAP_SHARED);
}

static vnode_t vbivp;

static int
segvbi_getvp(struct seg *seg, caddr_t addr, struct vnode **vpp)
{
	*vpp = &vbivp;
	return (0);
}

static int
segvbi_advise(struct seg *seg, caddr_t addr, size_t len, uint_t behav)
{
	return (0);
}

static void
segvbi_dump(struct seg *seg)
{}

static int
segvbi_pagelock(struct seg *seg, caddr_t addr, size_t len,
	struct page ***ppp, enum lock_type type, enum seg_rw rw)
{
	return (ENOTSUP);
}

static int
segvbi_setpagesize(struct seg *seg, caddr_t addr, size_t len, uint_t szc)
{
	return (ENOTSUP);
}

static int
segvbi_getmemid(struct seg *seg, caddr_t addr, memid_t *memid)
{
	return (ENODEV);
}

static lgrp_mem_policy_info_t *
segvbi_getpolicy(struct seg *seg, caddr_t addr)
{
	return (NULL);
}

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
	struct as *as = VBIPROC()->p_as;
	struct segvbi_crargs args;
	int error = 0;

	args.palist = palist;
	args.prot = prot;
	as_rangelock(as);
	map_addr(va, len, 0, 0, MAP_SHARED);
	if (*va != NULL)
		error = as_map(as, *va, len, segvbi_create, &args);
	else
		error = ENOMEM;
	if (error)
		cmn_err(CE_NOTE, "vbi_user_map() failed error=%d", error);
	as_rangeunlock(as);
	return (error);
}


/*
 * This is revision 2 of the interface.
 */

struct vbi_cpu_watch {
	void (*vbi_cpu_func)(void *, int, int);
	void *vbi_cpu_arg;
};

static int
vbi_watcher(cpu_setup_t state, int icpu, void *arg)
{
	vbi_cpu_watch_t *w = arg;
	int online;

	if (state == CPU_ON)
		online = 1;
	else if (state == CPU_OFF)
		online = 0;
	else
		return (0);
	w->vbi_cpu_func(w->vbi_cpu_arg, icpu, online);
	return (0);
}

vbi_cpu_watch_t *
vbi_watch_cpus(void (*func)(void *, int, int), void *arg, int current_too)
{
	int c;
	vbi_cpu_watch_t *w;

	w = kmem_alloc(sizeof (*w), KM_SLEEP);
	w->vbi_cpu_func = func;
	w->vbi_cpu_arg = arg;
	mutex_enter(&cpu_lock);
	register_cpu_setup_func(vbi_watcher, w);
	if (current_too) {
		for (c = 0; c < ncpus; ++c) {
			if (cpu_is_online(cpu[c]))
				func(arg, c, 1);
		}
	}
	mutex_exit(&cpu_lock);
	return (w);
}

void
vbi_ignore_cpus(vbi_cpu_watch_t *w)
{
	mutex_enter(&cpu_lock);
	unregister_cpu_setup_func(vbi_watcher, w);
	mutex_exit(&cpu_lock);
	kmem_free(w, sizeof (*w));
}

/*
 * Simple timers are pretty much a pass through to the cyclic subsystem.
 */
struct vbi_stimer {
	cyc_handler_t	s_handler;
	cyc_time_t	s_fire_time;
	cyclic_id_t	s_cyclic;
	uint64_t	s_tick;
	void		(*s_func)(void *, uint64_t);
	void		*s_arg;
};

static void
vbi_stimer_func(void *arg)
{
	vbi_stimer_t *t = arg;
	t->s_func(t->s_arg, ++t->s_tick);
}

extern vbi_stimer_t *
vbi_stimer_begin(
	void (*func)(void *, uint64_t),
	void *arg,
	uint64_t when,
	uint64_t interval,
	int on_cpu)
{
	vbi_stimer_t *t = kmem_zalloc(sizeof (*t), KM_SLEEP);

	t->s_handler.cyh_func = vbi_stimer_func;
	t->s_handler.cyh_arg = t;
	t->s_handler.cyh_level = CY_LOCK_LEVEL;
	t->s_tick = 0;
	t->s_func = func;
	t->s_arg = arg;

	mutex_enter(&cpu_lock);
	if (on_cpu != VBI_ANY_CPU && !cpu_is_online(cpu[on_cpu])) {
		t = NULL;
		goto done;
	}

	when += gethrtime();
	t->s_fire_time.cyt_when = when;
	if (interval == 0)
		t->s_fire_time.cyt_interval = INT64_MAX - when;
	else
		t->s_fire_time.cyt_interval = interval;
	t->s_cyclic = cyclic_add(&t->s_handler, &t->s_fire_time);
	if (on_cpu != VBI_ANY_CPU)
		cyclic_bind(t->s_cyclic, cpu[on_cpu], NULL);
done:
	mutex_exit(&cpu_lock);
	return (t);
}

extern void
vbi_stimer_end(vbi_stimer_t *t)
{
	mutex_enter(&cpu_lock);
	cyclic_remove(t->s_cyclic);
	mutex_exit(&cpu_lock);
	kmem_free(t, sizeof (*t));
}

/*
 * Global timers are more complicated. They include a counter on the callback,
 * that indicates the first call on a given cpu.
 */
struct vbi_gtimer {
	uint64_t	*g_counters;
	void		(*g_func)(void *, uint64_t);
	void		*g_arg;
	uint64_t	g_when;
	uint64_t	g_interval;
	cyclic_id_t	g_cyclic;
};

static void
vbi_gtimer_func(void *arg)
{
	vbi_gtimer_t *t = arg;
	t->g_func(t->g_arg, ++t->g_counters[vbi_cpu_id()]);
}

/*
 * Whenever a cpu is onlined, need to reset the g_counters[] for it to zero.
 */
static void
vbi_gtimer_online(void *arg, cpu_t *pcpu, cyc_handler_t *h, cyc_time_t *ct)
{
	vbi_gtimer_t *t = arg;
	hrtime_t now;

	t->g_counters[pcpu->cpu_id] = 0;
	h->cyh_func = vbi_gtimer_func;
	h->cyh_arg = t;
	h->cyh_level = CY_LOCK_LEVEL;
	now = gethrtime();
	if (t->g_when < now)
		ct->cyt_when = now + t->g_interval / 2;
	else
		ct->cyt_when = t->g_when;
	ct->cyt_interval = t->g_interval;
}


vbi_gtimer_t *
vbi_gtimer_begin(
	void (*func)(void *, uint64_t),
	void *arg,
	uint64_t when,
	uint64_t interval)
{
	vbi_gtimer_t *t;
	cyc_omni_handler_t omni;

	/*
	 * one shot global timer is not supported yet.
	 */
	if (interval == 0)
		return (NULL);

	t = kmem_zalloc(sizeof (*t), KM_SLEEP);
	t->g_counters = kmem_zalloc(ncpus * sizeof (uint64_t), KM_SLEEP);
	t->g_when = when + gethrtime();
	t->g_interval = interval;
	t->g_arg = arg;
	t->g_func = func;
	t->g_cyclic = CYCLIC_NONE;

	omni.cyo_online = (void (*)(void *, cpu_t *, cyc_handler_t *, cyc_time_t *))vbi_gtimer_online;
	omni.cyo_offline = NULL;
	omni.cyo_arg = t;

	mutex_enter(&cpu_lock);
	t->g_cyclic = cyclic_add_omni(&omni);
	mutex_exit(&cpu_lock);
	return (t);
}

extern void
vbi_gtimer_end(vbi_gtimer_t *t)
{
	mutex_enter(&cpu_lock);
	cyclic_remove(t->g_cyclic);
	mutex_exit(&cpu_lock);
	kmem_free(t->g_counters, ncpus * sizeof (uint64_t));
	kmem_free(t, sizeof (*t));
}

int
vbi_is_preempt_enabled(void)
{
	if (vbi_is_initialized) {
		char tpr = VBI_T_PREEMPT;
		return (tpr == 0);
	} else {
		cmn_err(CE_NOTE, "vbi_is_preempt_enabled: called without initializing vbi!\n");
		return 1;
	}
}

void
vbi_poke_cpu(int c)
{
	if (c < ncpus)
		poke_cpu(c);
}

/*
 * This is revision 5 of the interface.
 */

void *
vbi_lowmem_alloc(uint64_t phys, size_t size)
{
	return (vbi_internal_alloc(&phys, size, PAGESIZE /* alignment */, 0 /* non-contiguous */));
}

void
vbi_lowmem_free(void *va, size_t size)
{
	p_contig_free(va, size);
}

/*
 * This is revision 6 of the interface.
 */

int
vbi_is_preempt_pending(void)
{
	char crr = VBI_CPU_RUNRUN;
	char krr = VBI_CPU_KPRUNRUN;
	return crr != 0 || krr != 0;
}

/*
 * This is revision 7 of the interface.
 */

void *
vbi_phys_alloc(uint64_t *phys, size_t size, uint64_t alignment, int contig)
{
	return (vbi_internal_alloc(phys, size, alignment, contig));
}

void
vbi_phys_free(void *va, size_t size)
{
	p_contig_free(va, size);
}


/*
 * This is revision 8 of the interface.
 */
static vnode_t vbipagevp;

page_t **
vbi_pages_alloc(uint64_t *phys, size_t size)
{
	/*
	 * the page freelist and cachelist both hold pages that are not mapped into any address space.
	 * the cachelist is not really free pages but when memory is exhausted they'll be moved to the
	 * free lists.
	 * it's the total of the free+cache list that we see on the 'free' column in vmstat.
	 */
	page_t **pp_pages = NULL;
	pgcnt_t npages = (size + PAGESIZE - 1) >> PAGESHIFT;

	/* reserve available memory for pages */
	int rc = page_resv(npages, KM_NOSLEEP);
	if (rc)
	{
		/* create the pages */
		rc = page_create_wait(npages, 0 /* flags */);
		if (rc)
		{
			/* alloc space for page_t pointer array */
			size_t pp_size = npages * sizeof(page_t *);
			pp_pages = kmem_zalloc(pp_size, KM_SLEEP);
			if (pp_pages)
			{
				/*
				 * get pages from kseg, the 'virtAddr' here is only for colouring but unfortunately
				 * we don't have the 'virtAddr' to which this memory may be mapped.
				 */
				caddr_t virtAddr = NULL;
				for (int64_t i = 0; i < npages; i++, virtAddr += PAGESIZE)
				{
					/* get a page from the freelists */
					page_t *ppage = vbi_page_get_fromlist(1 /* freelist */, virtAddr, PAGESIZE);
					if (!ppage)
					{
						/* try from the cachelists */
						ppage = vbi_page_get_fromlist(2 /* cachelist */, virtAddr, PAGESIZE);
						if (!ppage)
						{
							/* damn */
							page_create_putback(npages - i);
							while (--i >= 0)
								page_free(pp_pages[i], 0 /* don't need, move to tail */);
							kmem_free(pp_pages, pp_size);
							page_unresv(npages);
							return NULL;
						}

						/* remove association with the vnode for pages from the cachelist */
						if (!PP_ISAGED(ppage))
							page_hashout(ppage, NULL /* mutex */);
					}

					PP_CLRFREE(ppage);		/* Page is not free */
					PP_CLRAGED(ppage);		/* Page is not hashed in */
					pp_pages[i] = ppage;
				}

				/*
				 * we now have the pages locked exclusively, before they are mapped in
				 * we must downgrade the lock.
				 */
				*phys = (uint64_t)page_pptonum(pp_pages[0]) << PAGESHIFT;
				return pp_pages;
			}

			page_create_putback(npages);
		}

		page_unresv(npages);
	}

	return NULL;
}


void
vbi_pages_free(page_t **pp_pages, size_t size)
{
	pgcnt_t npages = (size + PAGESIZE - 1) >> PAGESHIFT;
	size_t pp_size = npages * sizeof(page_t *);
	for (pgcnt_t i = 0; i < npages; i++)
	{
		/* we need to exclusive lock the pages before freeing them */
		int rc = page_tryupgrade(pp_pages[i]);
		if (!rc)
		{
			page_unlock(pp_pages[i]);
			while (!page_lock(pp_pages[i], SE_EXCL, NULL /* mutex */, P_RECLAIM))
				;
		}

		page_free(pp_pages[i], 0 /* don't need, move to tail */);
	}

	kmem_free(pp_pages, pp_size);
	page_unresv(npages);
}


int
vbi_pages_premap(page_t **pp_pages, size_t size, uint64_t *pphysaddrs)
{
	if (!pphysaddrs)
		return -1;

	pgcnt_t npages = (size + PAGESIZE - 1) >> PAGESHIFT;
	for (pgcnt_t i = 0; i < npages; i++)
	{
		/*
		 * prepare pages for mapping into kernel/user space, we need to
		 * downgrade the exclusive page lock to a shared lock if the
		 * pages is locked exclusively.
		 */
		if (page_tryupgrade(pp_pages[i]) == 1)
			page_downgrade(pp_pages[i]);
		pphysaddrs[i] = vbi_page_to_pa(pp_pages, i);
	}

	return 0;
}


uint64_t
vbi_page_to_pa(page_t **pp_pages, pgcnt_t i)
{
	pfn_t pfn = page_pptonum(pp_pages[i]);
	if (pfn == PFN_INVALID)
		panic("vbi_page_to_pa: page_pptonum() failed\n");
	return (uint64_t)pfn << PAGESHIFT;
}


static page_t *
vbi_page_get_fromlist(uint_t freelist, caddr_t virtAddr, size_t pgsize)
{
	/* pgsize only applies when using the freelist */
	seg_t kernseg;
	kernseg.s_as = &kas;
	page_t *ppage = NULL;
	if (freelist == 1)
	{
		ppage = page_get_freelist(&vbipagevp, 0 /* offset */, &kernseg, virtAddr,
							pgsize, 0 /* flags */, NULL /* local group */);
		if (!ppage && use_kflt)
		{
			ppage = page_get_freelist(&vbipagevp, 0 /* offset */, &kernseg, virtAddr,
						pgsize, 0x0200 /* PG_KFLT */, NULL /* local group */);
		}
	}
	else
	{
		/* cachelist */
		ppage = page_get_cachelist(&vbipagevp, 0 /* offset */, &kernseg, virtAddr,
							0 /* flags */, NULL /* local group */);
		if (!ppage && use_kflt)
		{
			ppage = page_get_cachelist(&vbipagevp, 0 /* offset */, &kernseg, virtAddr,
						0x0200 /* PG_KFLT */, NULL /* local group */);
		}
	}
	return ppage;
}


/*
 * Large page code.
 */

page_t *
vbi_large_page_alloc(uint64_t *pphys, size_t pgsize)
{
	pgcnt_t const npages = pgsize >> PAGESHIFT;
	page_t *pproot, *pp, *pplist;
	pgcnt_t ipage;
	caddr_t vaddr;
	seg_t kernseg;
	int rc;

	/*
	 * Reserve available memory for a large page and create it.
	 */
	rc = page_resv(npages, KM_NOSLEEP);
	if (!rc)
		return NULL;

	rc = page_create_wait(npages, 0 /* flags */);
	if (!rc) {
		page_unresv(npages);
		return NULL;
	}

	/*
	 * Get a page off the free list.  We set vaddr to 0 since we don't know
	 * where the memory is going to be mapped.
	 */
	vaddr = NULL;
	kernseg.s_as = &kas;
	pproot = vbi_page_get_fromlist(1 /* freelist */, vaddr, pgsize);
	if (!pproot)
	{
		page_create_putback(npages);
		page_unresv(npages);
		return NULL;
	}
	AssertMsg(!(page_pptonum(pproot) & (npages - 1)), ("%p:%lx npages=%lx\n", pproot, page_pptonum(pproot), npages));

	/*
	 * Mark all the sub-pages as non-free and not-hashed-in.
	 * It is paramount that we destroy the list (before freeing it).
	 */
	pplist = pproot;
	for (ipage = 0; ipage < npages; ipage++) {
		pp = pplist;
		AssertPtr(pp);
		AssertMsg(page_pptonum(pp) == ipage + page_pptonum(pproot),
			("%p:%lx %lx+%lx\n", pp, page_pptonum(pp), ipage, page_pptonum(pproot)));
		page_sub(&pplist, pp);
		AssertMsg(PP_ISFREE(pp), ("%p\n", pp));
		AssertMsg(pp->p_szc == pproot->p_szc, ("%p - %d expected %d \n", pp, pp->p_szc, pproot->p_szc));

		PP_CLRFREE(pp);
		PP_CLRAGED(pp);
	}

	*pphys = (uint64_t)page_pptonum(pproot) << PAGESHIFT;
	AssertMsg(!(*pphys & (pgsize - 1)), ("%llx %zx\n", *pphys, pgsize));
	return pproot;
}

void
vbi_large_page_free(page_t *pproot, size_t pgsize)
{
	pgcnt_t const npages = pgsize >> PAGESHIFT;
	pgcnt_t ipage;

	Assert(page_get_pagecnt(pproot->p_szc) == npages);
	AssertMsg(!(page_pptonum(pproot) & (npages - 1)), ("%p:%lx npages=%lx\n", pproot, page_pptonum(pproot), npages));

	/*
	 * We need to exclusively lock the sub-pages before freeing
	 * the large one.
	 */
	for (ipage = 0; ipage < npages; ipage++) {
		page_t *pp = page_nextn(pproot, ipage);
		AssertMsg(page_pptonum(pp) == ipage + page_pptonum(pproot),
			("%p:%lx %lx+%lx\n", pp, page_pptonum(pp), ipage, page_pptonum(pproot)));
		AssertMsg(!PP_ISFREE(pp), ("%p\n", pp));

		int rc = page_tryupgrade(pp);
		if (!rc) {
			page_unlock(pp);
			while (!page_lock(pp, SE_EXCL, NULL /* mutex */, P_RECLAIM)) {
				/*nothing*/;
			}
		}
	}

	/*
	 * Free the large page and unreserve the memory.
	 */
	page_free_pages(pproot);
	page_unresv(npages);
}

int
vbi_large_page_premap(page_t *pproot, size_t pgsize)
{
	pgcnt_t const npages = pgsize >> PAGESHIFT;
	pgcnt_t ipage;

	Assert(page_get_pagecnt(pproot->p_szc) == npages);
	AssertMsg(!(page_pptonum(pproot) & (npages - 1)), ("%p:%lx npages=%lx\n", pproot, page_pptonum(pproot), npages));

	/*
	 * We need to downgrade the sub-pages from exclusive to shared locking
	 * because otherwise we cannot <you go figure>.
	 */
	for (ipage = 0; ipage < npages; ipage++) {
	    page_t *pp = page_nextn(pproot, ipage);
	    AssertMsg(page_pptonum(pp) == ipage + page_pptonum(pproot),
		    ("%p:%lx %lx+%lx\n", pp, page_pptonum(pp), ipage, page_pptonum(pproot)));
	    AssertMsg(!PP_ISFREE(pp), ("%p\n", pp));

	    if (page_tryupgrade(pp) == 1)
		    page_downgrade(pp);
	    AssertMsg(!PP_ISFREE(pp), ("%p\n", pp));
	}

	return 0;
}


/*
 * As more functions are added, they should start with a comment indicating
 * the revision and above this point in the file and the revision level should
 * be increased. Also change vbi_modlmisc at the top of the file.
 *
 * NOTE! We'll start care about this if anything in here ever makes it into
 *       the solaris kernel proper.
 */
uint_t vbi_revision_level = 9;

