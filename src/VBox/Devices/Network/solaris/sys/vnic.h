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

#ifndef	_SYS_VNIC_H
#define	_SYS_VNIC_H

#pragma ident	"@(#)vnic.h	1.17	06/12/06 SMI"

#include <sys/types.h>
#include <sys/ethernet.h>
#include <sys/param.h>
#include <sys/mac.h>
#include <sys/flow.h>
#include <inet/ip.h>
#include <inet/ip6.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* control interface name */
#define	VNIC_CTL_NODE_NAME	"ctl"
#define	VNIC_CTL_NODE_MINOR	1		/* control interface minor */

#define	VNIC_IOC(x)	(('v' << 24) | ('n' << 16) | ('i' << 8) | (x))

/*
 * Allowed VNIC MAC address types.
 *
 * - VNIC_MAC_ADDR_TYPE_FIXED, VNIC_MAC_ADDR_TYPE_RANDOM:
 *   The MAC address is specified by value by the caller, which
 *   itself can obtain it from the user directly,
 *   or pick it in a random fashion. Which method is used by the
 *   caller is irrelevant to the VNIC driver. However two different
 *   types are provided so that the information can be made available
 *   back to user-space when listing the kernel defined VNICs.
 *
 *   When a VNIC is created, the address in passed through the
 *   vc_mac_addr and vc_mac_len fields of the vnic_ioc_create_t
 *   structure.
 *
 * - VNIC_MAC_ADDR_TYPE_FACTORY: the MAC address is obtained from
 *   one of the MAC factory MAC addresses of the underyling NIC.
 *
 * - VNIC_MAC_ADDR_TYPE_AUTO: the VNIC driver attempts to
 *   obtain the address from one of the factory MAC addresses of
 *   the underlying NIC. If none is available, the specified
 *   MAC address value is used.
 */

typedef enum {
	VNIC_MAC_ADDR_TYPE_FIXED,
	VNIC_MAC_ADDR_TYPE_RANDOM,
	VNIC_MAC_ADDR_TYPE_FACTORY,
	VNIC_MAC_ADDR_TYPE_AUTO
} vnic_mac_addr_type_t;

#define	VNIC_IOC_CREATE		VNIC_IOC(1)

typedef struct vnic_ioc_create {
	uint_t		vc_vnic_id;
	uchar_t		vc_dev_name[MAXNAMELEN];
	vnic_mac_addr_type_t vc_mac_addr_type;
	uint_t		vc_mac_len;
	uchar_t		vc_mac_addr[MAXMACADDRLEN];
	char		vc_pad[4];
	uint_t		vc_mac_slot;
	net_resource_ctl_t vc_resource_ctl;
	net_bind_cpu_t	vc_bind_cpu;
} vnic_ioc_create_t;

#ifdef _SYSCALL32

typedef struct vnic_ioc_create32 {
	uint32_t	vc_vnic_id;
	uchar_t		vc_dev_name[MAXNAMELEN];
	vnic_mac_addr_type_t vc_mac_addr_type;
	uint32_t	vc_mac_len;
	uchar_t		vc_mac_addr[MAXMACADDRLEN];
	char		vc_pad[4];
	uint32_t	vc_mac_slot;
	net_resource_ctl_t vc_resource_ctl;
	net_bind_cpu_t	vc_bind_cpu;
} vnic_ioc_create32_t;

#endif /* _SYSCALL32 */

#define	VNIC_IOC_DELETE		VNIC_IOC(2)

typedef struct vnic_ioc_delete {
	uint_t		vd_vnic_id;
} vnic_ioc_delete_t;

#ifdef _SYSCALL32

typedef struct vnic_ioc_delete32 {
	uint32_t	vd_vnic_id;
} vnic_ioc_delete32_t;

#endif /* _SYSCALL32 */

#define	VNIC_IOC_INFO		VNIC_IOC(3)

typedef struct vnic_ioc_info_vnic {
	uint32_t	vn_vnic_id;
	uchar_t		vn_mac_addr[MAXMACADDRLEN];
	char		vn_dev_name[MAXNAMELEN];
	flow_desc_t	vn_flow_desc;
	net_resource_ctl_t	vn_resource_ctl;
} vnic_ioc_info_vnic_t;

typedef struct vnic_ioc_info {
	uint_t		vi_nvnics;
	uint_t		vi_vnic_id;	/* 0 returns all */
	char		vi_dev_name[MAXNAMELEN];
} vnic_ioc_info_t;

#ifdef _SYSCALL32

typedef struct vnic_ioc_info32 {
	uint32_t	vi_nvnics;
	uint32_t	vi_vnic_id;	/* 0 returns all */
	char		vi_dev_name[MAXNAMELEN];
} vnic_ioc_info32_t;

#endif /* _SYSCALL32 */

#define	VNIC_IOC_MODIFY		VNIC_IOC(4)

#define	VNIC_IOC_MODIFY_ADDR		0x01
#define	VNIC_IOC_MODIFY_RESOURCE_CTL	0x02

typedef struct vnic_ioc_modify {
	uint_t		vm_vnic_id;
	uint_t		vm_modify_mask;
	uint_t		vm_mac_len;
	uint_t		vm_mac_slot;
	uchar_t		vm_mac_addr[MAXMACADDRLEN];
	vnic_mac_addr_type_t vm_mac_addr_type;
	net_resource_ctl_t vm_resource_ctl;
} vnic_ioc_modify_t;

#ifdef _SYSCALL32

typedef struct vnic_ioc_modify32 {
	uint32_t	vm_vnic_id;
	uint32_t	vm_modify_mask;
	uint32_t	vm_mac_len;
	uint32_t	vm_mac_slot;
	uchar_t		vm_mac_addr[MAXMACADDRLEN];
	vnic_mac_addr_type_t vm_mac_addr_type;
	net_resource_ctl_t vm_resource_ctl;
} vnic_ioc_modify32_t;

#endif /* _SYSCALL32 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_VNIC_H */
