/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_SYS_DLS_SOFT_RING_H
#define	_SYS_DLS_SOFT_RING_H

#pragma ident	"@(#)mac_soft_ring.h	1.6	06/11/20 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/cpuvar.h>
#include <sys/processor.h>
#include <sys/stream.h>
#include <sys/squeue.h>
#include <sys/dlpi.h>

#define	S_RING_NAMELEN 64

typedef void (*s_ring_proc_t)(void *, void *, mblk_t *);

typedef struct mac_soft_ring_s {
	/* Keep the most used members 64bytes cache aligned */
	kmutex_t	s_ring_lock;	/* lock before using any member */
	uint16_t	s_ring_type;	/* processing model of the sq */
	uint16_t	s_ring_state;	/* state flags and message count */
	int		s_ring_count;	/* # of mblocks in mac_soft_ring */
	mblk_t		*s_ring_first;	/* first mblk chain or NULL */
	mblk_t		*s_ring_last;	/* last mblk chain or NULL */
	s_ring_proc_t	s_ring_rx_func;
	void		*s_ring_rx_arg1;
	void		*s_ring_rx_arg2;
	s_ring_proc_t	s_ring_rx_saved_func;
	void		*s_ring_rx_saved_arg1;
	void		*s_ring_rx_saved_arg2;
	clock_t		s_ring_awaken;	/* time async thread was awakened */

	kthread_t	*s_ring_run;	/* Current thread processing sq */
	processorid_t	s_ring_bind;	/* processor to bind to */
	kcondvar_t	s_ring_async;	/* async thread blocks on */
	clock_t		s_ring_wait;	/* lbolts to wait after a fill() */
	timeout_id_t	s_ring_tid;	/* timer id of pending timeout() */
	kthread_t	*s_ring_worker;	/* kernel thread id */
	char		s_ring_name[S_RING_NAMELEN + 1];
	uint32_t	s_ring_total_inpkt;
	uint32_t	s_ring_drops;
	void		*s_ring_mip;
	kstat_t		*s_ring_ksp;
} mac_soft_ring_t;

/*
 * type flags - combination allowed to process and drain the queue
 */
#define	S_RING_WORKER_ONLY  	0x0001	/* Worker thread only */
#define	S_RING_ANY		0x0002	/* Any thread can process the queue */

/*
 * State flags.
 */
#define	S_RING_PROC	0x0001	/* being processed */
#define	S_RING_BOUND	0x0004	/* Worker thread is bound to a cpu */
#define	S_RING_DESTROY	0x0008	/* Ring is being destroyed */
#define	S_RING_DEAD	0x0010	/* Worker thread is no more */
#define	S_RING_SQUEUE	0x0020	/* This soft ring has a matching squeue */
#define	S_RING_BLANK	0x0040	/* soft ring has been put into polling mode */

/*
 * arguments for processors to bind to
 */
#define	S_RING_BIND_NONE	-1

/*
 * Structure for dls statistics
 */
struct dls_kstats {
	kstat_named_t	dlss_soft_ring_pkt_drop;
};

extern struct dls_kstats dls_kstat;

#define	DLS_BUMP_STAT(x, y)	(dls_kstat.x.value.ui32 += y)

extern void mac_soft_ring_init(void);
extern void mac_soft_ring_finish(void);
extern void mac_soft_ring_nosqueue(void *);
extern void mac_soft_ring_fanout_disable(mac_handle_t);
void mac_soft_ring_resource_set(mac_handle_t, dl_capab_dls_t *);
extern boolean_t mac_soft_ring_fanout_enable(mac_handle_t);
extern int mac_soft_ring_fanout_modify(mac_handle_t);
extern mblk_t *mac_ether_soft_ring_fanout(mac_handle_t,
    mac_resource_handle_t, mblk_t *);
extern mac_soft_ring_t *mac_soft_ring_create(char *, clock_t, uint_t, pri_t,
    mac_handle_t);
extern cpu_t *mac_soft_ring_bind(mac_soft_ring_t *, processorid_t);
extern void mac_soft_ring_process(mac_handle_t, mac_soft_ring_t *, mblk_t *);
extern void mac_soft_ring_blank(void *, time_t, uint_t, int);
extern mblk_t *mac_soft_ring_poll(mac_soft_ring_t *, int);
extern void mac_soft_ring_destroy(mac_soft_ring_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DLS_SOFT_RING_H */
