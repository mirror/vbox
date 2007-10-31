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

#ifndef	_INET_FLOW_H
#define	_INET_FLOW_H

#pragma ident	"@(#)flow.h	1.11	06/12/06 SMI"

/*
 * Main structure describing a flow of packets, for classification use
 */

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <netinet/in.h>		/* for IPPROTO_* constants */
#include <sys/ethernet.h>

/* Bit-mask for the selectors carried in the flow descriptor */
typedef	uint64_t	flow_mask_t;

/*
 * The values are defined such that less specific IP flows are numerically
 * smaller than more specific IP flows. This makes comparison easier.
 * In particular
 *	FLOW_IP_LOCAL < FLOW_IP_PROTOCOL < FLOW_TRANSPORT_LOCAL.
 *	FLOW_IP_LOCAL < FLOW_IP_PROTOCOL < FLOW_TRANSPORT_REMOTE.
 */
#define	FLOW_ETHER_DHOST	0x00000001	/* Destination MAC addr */
#define	FLOW_ETHER_SHOST	0x00000002	/* Source MAC address */
#define	FLOW_ETHER_TPID		0x00000004	/* For VLAN TAG */
#define	FLOW_ETHER_TCI		0x00000008	/* Ditto */
#define	FLOW_ETHER_TYPE		0x00000010	/* payload protocol type */
#define	FLOW_IP_VERSION		0x00000020	/* V4 or V6 */
#define	FLOW_IP_REMOTE		0x00000040
#define	FLOW_IP_LOCAL		0x00000080
#define	FLOW_IP_PROTOCOL	0x00000100	/* Upper Later transport type */
#define	FLOW_ULP_PORT_REMOTE	0x00000200	/* Upper Layer Transport */
#define	FLOW_ULP_PORT_LOCAL	0x00000400	/* port numbers */
#define	FLOW_SAP		0x00000800	/* SAP value */

#define	FLOW_TRANSPORT_LOCAL	(FLOW_IP_PROTOCOL | FLOW_ULP_PORT_LOCAL)
#define	FLOW_TRANSPORT_REMOTE	(FLOW_IP_PROTOCOL | FLOW_ULP_PORT_REMOTE)

#define	FLOW_LESS_SPECIFIC	-1
#define	FLOW_EQUAL		0
#define	FLOW_MORE_SPECIFIC	1

#define	FLOW_IP_COMPARE(flmask1,  flmask2)		\
	(((flmask1) == (flmask2)) ? FLOW_EQUAL : 	\
	((flmask1 < flmask2) ? FLOW_LESS_SPECIFIC : FLOW_MORE_SPECIFIC))

/* Define additional flow description criteria here */


typedef struct flow_desc_s {
	flow_mask_t			fd_mask;
	struct ether_vlan_header	fd_mac;
	char				fd_ipversion;
	char				fd_protocol;
	in6_addr_t			fd_remoteaddr;
	in6_addr_t			fd_localaddr;
	in_port_t			fd_remoteport;
	in_port_t			fd_localport;
	uint32_t			fd_sap;
	char				fd_pad[4];	/* 64-bit alignment */
} flow_desc_t;

/* Bit-mask for the network resource control types types */
typedef	uint64_t	net_service_type_t;

#define	NET_BANDWIDTH_SHARE	0x00000001
#define	NET_BANDWIDTH_LIMIT	0x00000002
#define	NET_BANDWIDTH_GUARANTEE	0x00000004
#define	NET_FLOW_PRIORITY	0x00000008

/* Define additional resource controls */

typedef	struct net_resource_ctl_s {
	net_service_type_t	nr_mask;
	uint32_t		nr_bw_shares;	/* bandwidh shares */
	uint32_t		nr_bw_limit;	/* bandwidh limit in Kbps */
	uint32_t		nr_bw_guarantee; /* bandwidh gua'tee in Kbps */
	uint32_t		nr_flow_priority; /* relative flow priority */
	boolean_t		nr_hw_classifier; /* assign h/w classifier */
	char			nr_pad[4];	/* 64-bit alignment */
} net_resource_ctl_t;

typedef struct flow_stats_s {
	uint64_t	fs_rbytes;
	uint64_t	fs_ipackets;
	uint64_t	fs_ierrors;
	uint64_t	fs_obytes;
	uint64_t	fs_opackets;
	uint64_t	fs_oerrors;
} flow_stats_t;

typedef struct net_bind_cpus_s {
	uint32_t	nbc_ncpus;
	uint32_t	nbc_cpus[128];
	char		nbc_pad[4];
} net_bind_cpu_t;

typedef enum {
	FLOW_STAT_RBYTES,
	FLOW_STAT_IPACKETS,
	FLOW_STAT_IERRORS,
	FLOW_STAT_OBYTES,
	FLOW_STAT_OPACKETS,
	FLOW_STAT_OERRORS
} flow_stat_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_FLOW_H */
