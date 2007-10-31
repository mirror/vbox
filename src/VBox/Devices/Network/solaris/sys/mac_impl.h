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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_SYS_MAC_IMPL_H
#define	_SYS_MAC_IMPL_H

#pragma ident	"@(#)mac_impl.h	1.21	06/11/22 SMI"

#include <sys/mac.h>
#include <net/if.h>
#include <sys/flow_impl.h>
#include <sys/mac_soft_ring.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Statistics maintained internally by the mac module.
 */
enum mac_mod_stat {
	MAC_STAT_LINK_STATE,
	MAC_STAT_LINK_UP,
	MAC_STAT_PROMISC
};

/* Generic linked chain type */
typedef	struct mac_chain_s {
	struct mac_chain_s	*next;
	void			*item;
} mac_chain_t;

typedef struct mac_multicst_addr_s	mac_multicst_addr_t;

struct mac_multicst_addr_s {
	mac_multicst_addr_t	*mma_nextp;
	uint_t			mma_ref;
	uint8_t			mma_addr[MAXMACADDRLEN];
};

typedef struct mac_notify_fn_s		mac_notify_fn_t;

struct mac_notify_fn_s {
	mac_notify_fn_t		*mnf_nextp;
	mac_notify_t		mnf_fn;
	void			*mnf_arg;
};

typedef struct mac_rx_fn_s		mac_rx_fn_t;

struct mac_rx_fn_s {
	mac_rx_fn_t		*mrf_nextp;
	mac_rx_t		mrf_fn;
	void			*mrf_arg;
	boolean_t		mrf_inuse;
	boolean_t		mrf_active;
};

typedef struct mac_txloop_fn_s		mac_txloop_fn_t;

struct mac_txloop_fn_s {
	mac_txloop_fn_t		*mtf_nextp;
	mac_txloop_t		mtf_fn;
	void			*mtf_arg;
};

typedef struct mactype_s {
	const char	*mt_ident;
	uint32_t	mt_ref;
	uint_t		mt_type;
	size_t		mt_addr_length;
	uint8_t		*mt_brdcst_addr;
	mactype_ops_t	mt_ops;
	mac_stat_info_t	*mt_stats;	/* array of mac_stat_info_t elements */
	size_t		mt_statcount;	/* number of elements in mt_stats */
} mactype_t;

typedef void	(*mac_ring_remove_func_t)(void *, void *);
typedef int	(*mac_ring_bind_func_t)(void *, void *, processorid_t);
typedef int	(*mac_ring_modify_func_t)(void *, void *, mac_resource_t *);

#define	MAC_VNIC_TXINFO_REFHOLD(mvt) {				\
	mutex_enter(&(mvt)->mv_lock);				\
	(mvt)->mv_refs++;					\
	mutex_exit(&(mvt)->mv_lock);				\
}

#define	MAC_VNIC_TXINFO_REFRELE(mvt) {				\
	mutex_enter(&(mvt)->mv_lock);				\
	if (--(mvt)->mv_refs == 0 && (mvt)->mv_clearing) {	\
	    (mvt)->mv_clearing = B_FALSE;			\
	    cv_signal(&(mvt)->mv_cv);				\
	}							\
	mutex_exit(&(mvt)->mv_lock);				\
}

typedef struct mac_vnic_tx_s {
	mac_txinfo_t	mv_txinfo;	/* provided by VNIC */
	uint32_t	mv_refs;
	kmutex_t	mv_lock;
	kcondvar_t	mv_cv;
	boolean_t	mv_clearing;
} mac_vnic_tx_t;

/*
 * Each registered MAC is associated with a mac_t structure.
 */
struct mac_impl_s {
	char			mi_name[LIFNAMSIZ];
	const char		*mi_drvname;
	uint_t			mi_instance;
	void			*mi_driver;	/* Driver private data */
	mac_info_t		mi_info;
	mactype_t		*mi_type;
	void			*mi_pdata;
	size_t			mi_pdata_size;
	mac_callbacks_t		*mi_callbacks;
	dev_info_t		*mi_dip;
	uint32_t		mi_ref;
	boolean_t		mi_disabled;
	uint_t			mi_active;
	link_state_t		mi_linkstate;
	uint_t			mi_promisc;
	uint_t			mi_devpromisc;
	uint8_t			mi_addr[MAXMACADDRLEN];
	uint8_t			mi_dstaddr[MAXMACADDRLEN];
	mac_multicst_addr_t	*mi_mmap;

	/*
	 * These two locks are used for shortlived, non-performance
	 * critical operations.
	 */
	kmutex_t		mi_lock;
	krwlock_t		mi_rw_lock;

	krwlock_t		mi_notify_lock;
	mac_notify_fn_t		*mi_mnfp;
	uint32_t		mi_notify_ref;
	kcondvar_t		mi_notify_cv;
	krwlock_t		mi_rx_lock;
	kcondvar_t		mi_rx_cv;
	mac_rx_fn_t		*mi_mrfp;
	uint32_t		mi_rx_ref;
	uint32_t		mi_rx_removed;
	krwlock_t		mi_tx_lock;
	mac_txloop_fn_t		*mi_mtfp;
	mac_resource_add_t	mi_resource_add;
	mac_resource_bind_t	mi_resource_bind;
	void			*mi_resource_arg;

	/* soft ring stuff */
	mac_resource_add_t	mi_ring_add;
	mac_ring_remove_func_t	mi_ring_remove;
	mac_ring_bind_func_t	mi_ring_bind;
	mac_ring_modify_func_t	mi_ring_modify;
	mac_soft_ring_t		**mi_soft_ring_list;
	uint_t			mi_soft_ring_list_size;
	uint_t			mi_soft_ring_size;
	kcondvar_t		mi_soft_ring_cv;
	boolean_t		mi_soft_ring_deleting;
	uint32_t		mi_fanout_ref;

	/*
	 * place to store the argument to
	 * mi_ring_add and mi_ring_remove
	 */
	void			*mi_soft_ring_arg1;

	/*
	 * mi_rx_rings is the chain of available receive rings from the
	 * MAC provider.
	 * mi_rx_rings_inuse are the rings being used for a flow/VNIC
	 * mi_rx_soft_rings are the software ring descriptor chain.
	 */
	mac_chain_t		*mi_rx_rings;
	mac_chain_t		*mi_rx_rings_inuse;
	mac_chain_t		*mi_rx_soft_rings;

	/*
	 * This MAC's table of sub-flows
	 */
	flow_tab_t		*mi_flow_tab;
	mac_rxclassify_fn_t	mi_mcfp;

	/*
	 * and the multiple MAC addresses
	 */
	multiaddress_capab_t	mi_mmcp;

	kstat_t			*mi_ksp;
	uint_t			mi_kstat_count;
	boolean_t		mi_activelink;
	mac_rx_fn_t		*mi_active_mrfp;
	mac_txinfo_t		mi_txinfo;
	mac_txinfo_t		mi_txloopinfo;

	/*
	 * VNIC support
	 */
	boolean_t		mi_shareable;
	boolean_t		mi_vnic_present;
	mac_vnic_tx_t		*mi_vnic_tx;
	mac_txinfo_t		mi_vnic_txinfo;
	mac_txinfo_t		mi_vnic_txloopinfo;
	mac_getcapab_t		mi_vnic_getcapab_fn;
	void			*mi_vnic_getcapab_arg;
	net_bind_cpu_t		mi_bind_cpu;
};

#define	mi_getstat	mi_callbacks->mc_getstat
#define	mi_start	mi_callbacks->mc_start
#define	mi_stop		mi_callbacks->mc_stop
#define	mi_setpromisc	mi_callbacks->mc_setpromisc
#define	mi_multicst	mi_callbacks->mc_multicst
#define	mi_unicst	mi_callbacks->mc_unicst
#define	mi_resources	mi_callbacks->mc_resources
#define	mi_tx		mi_callbacks->mc_tx
#define	mi_ioctl	mi_callbacks->mc_ioctl
#define	mi_getcapab	mi_callbacks->mc_getcapab

typedef struct mac_notify_task_arg {
	mac_impl_t		*mnt_mip;
	mac_notify_type_t	mnt_type;
} mac_notify_task_arg_t;

#define	MRD_DRIVER_RX_HANDLE(mrd) ((mrd)->mrd_driver_rx).mr_fifo.mrf_arg

extern void	mac_init(void);
extern int	mac_fini(void);

extern void	mac_stat_create(mac_impl_t *);
extern void	mac_stat_destroy(mac_impl_t *);
extern uint64_t	mac_stat_default(mac_impl_t *, uint_t);
extern void	mac_create_soft_ring_kstats(mac_impl_t *, int32_t);

extern boolean_t mac_enqueue(mac_chain_t **head, void *item);
extern void mac_dequeue(mac_chain_t **head, void *item);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MAC_IMPL_H */
