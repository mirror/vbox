/* $Id$ */
/** @file
 * NAT - slirp interface.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef _LIBSLIRP_H
#define _LIBSLIRP_H

#ifdef RT_OS_WINDOWS
# include <winsock2.h>
# ifdef __cplusplus
extern "C" {
# endif
int inet_aton(const char *cp, struct in_addr *ia);
# ifdef __cplusplus
}
# endif
#else
# ifdef RT_OS_OS2 /* temporary workaround, see ticket #127 */
#  include <sys/time.h>
# endif
# include <sys/select.h>
# include <poll.h>
# include <arpa/inet.h>
#endif

#include <VBox/types.h>
#include <VBox/vmm/dbgf.h>

typedef struct NATState *PNATState;
struct mbuf;

#ifdef __cplusplus
extern "C" {
#endif

int slirp_init(PNATState *, uint32_t, uint32_t, bool, bool, int, void *);
void slirp_register_statistics(PNATState pData, PPDMDRVINS pDrvIns);
void slirp_deregister_statistics(PNATState pData, PPDMDRVINS pDrvIns);
void slirp_term(PNATState);
void slirp_link_up(PNATState);
void slirp_link_down(PNATState);

#if defined(RT_OS_WINDOWS)
void slirp_select_fill(PNATState pData, int *pndfs);

void slirp_select_poll(PNATState pData, int fTimeout, int fIcmp);
#else /* RT_OS_WINDOWS */
void slirp_select_fill(PNATState pData, int *pnfds, struct pollfd *polls);
void slirp_select_poll(PNATState pData, struct pollfd *polls, int ndfs);
#endif /* !RT_OS_WINDOWS */

void slirp_input(PNATState pData, struct mbuf *m, size_t cbBuf);
void slirp_set_ethaddr_and_activate_port_forwarding(PNATState pData, const uint8_t *ethaddr, uint32_t GuestIP);

/* you must provide the following functions: */
void slirp_arm_fast_timer(void *pvUser);
int slirp_can_output(void * pvUser);
void slirp_output(void * pvUser, struct mbuf *m, const uint8_t *pkt, int pkt_len);
void slirp_output_pending(void * pvUser);
void slirp_urg_output(void *pvUser, struct mbuf *, const uint8_t *pu8Buf, int cb);
void slirp_post_sent(PNATState pData, void *pvArg);

int slirp_add_redirect(PNATState pData, int is_udp, struct in_addr host_addr,
                int host_port, struct in_addr guest_addr,
                int guest_port, const uint8_t *);
int slirp_remove_redirect(PNATState pData, int is_udp, struct in_addr host_addr,
                int host_port, struct in_addr guest_addr,
                int guest_port);
int slirp_add_exec(PNATState pData, int do_pty, const char *args, int addr_low_byte,
                   int guest_port);

void slirp_set_dhcp_TFTP_prefix(PNATState pData, const char *tftpPrefix);
void slirp_set_dhcp_TFTP_bootfile(PNATState pData, const char *bootFile);
void slirp_set_dhcp_next_server(PNATState pData, const char *nextServer);
void slirp_set_dhcp_dns_proxy(PNATState pData, bool fDNSProxy);
void slirp_set_rcvbuf(PNATState pData, int kilobytes);
void slirp_set_sndbuf(PNATState pData, int kilobytes);
void slirp_set_tcp_rcvspace(PNATState pData, int kilobytes);
void slirp_set_tcp_sndspace(PNATState pData, int kilobytes);

int  slirp_set_binding_address(PNATState, char *addr);
void slirp_set_mtu(PNATState, int);
void slirp_info(PNATState pData, PCDBGFINFOHLP pHlp, const char *pszArgs);
void slirp_set_somaxconn(PNATState pData, int iSoMaxConn);

#if defined(RT_OS_WINDOWS)


/*
 * ICMP handle state change
 */
# define VBOX_ICMP_EVENT_INDEX           0

/**
 * This event is for
 *  - slirp_input
 *  - slirp_link_up
 *  - slirp_link_down
 *  - wakeup
 */
# define VBOX_WAKEUP_EVENT_INDEX       1

/*
 * UDP/TCP socket state change (socket ready to receive, to send, ...)
 */
# define VBOX_SOCKET_EVENT_INDEX       2

/*
 * The number of events for WSAWaitForMultipleEvents().
 */
# define VBOX_EVENT_COUNT              3

HANDLE *slirp_get_events(PNATState pData);
void slirp_register_external_event(PNATState pData, HANDLE hEvent, int index);
#endif /* RT_OS_WINDOWS */

#ifdef VBOX_WITH_SLIRP_MT
void slirp_process_queue(PNATState pData);
void *slirp_get_queue(PNATState pData);
#endif

struct mbuf *slirp_ext_m_get(PNATState pData, size_t cbMin, void **ppvBuf, size_t *pcbBuf);
void slirp_ext_m_free(PNATState pData, struct mbuf *, uint8_t *pu8Buf);

/*
 * Returns the timeout.
 */
unsigned int slirp_get_timeout_ms(PNATState pData);

# ifndef RT_OS_WINDOWS
/*
 * Returns the number of sockets.
 */
int slirp_get_nsock(PNATState pData);
# endif

#ifdef __cplusplus
}
#endif

#endif
