#ifndef VBOX_LWIP_OPTS_H_
#define VBOX_LWIP_OPTS_H_

#include <iprt/mem.h>
#include <iprt/alloca.h>    /* This may include malloc.h (msc), which is something that has
                             * to be done before redefining any of the functions therein. */
#include <iprt/rand.h> /* see LWIP_RAND() definition */

/* lwip/sockets.h assumes that if FD_SET is defined (in case of Innotek GCC
 * its definition is dragged through iprt/types.h) then struct timeval is
 * defined as well, but it's not the case. So include it manually. */
#ifdef RT_OS_OS2
# include <sys/time.h>
#endif

/** Make lwIP use the libc malloc, or more precisely (see below) the IPRT
 * memory allocation functions. */
#define MEM_LIBC_MALLOC 1

/** Set proper memory alignment. */
#if HC_ARCH_BITS == 64
# define MEM_ALIGNMENT 8
#else
#define MEM_ALIGNMENT 4
#endif

/* IP */
#define IP_REASSEMBLY 1
#define IP_REASS_MAX_PBUFS      128



/** Increase maximum TCP window size. */
#define TCP_WND 32768

/** Increase TCP maximum segment size. */
#define TCP_MSS 1460

/** Enable queueing of out-of-order segments. */
#define TCP_QUEUE_OOSEQ 1

/** TCP sender buffer space (bytes). */
#define TCP_SND_BUF (32 * TCP_MSS)

/* TCP sender buffer space (pbufs). This must be at least = 2 *
   TCP_SND_BUF/TCP_MSS for things to work. */
#define TCP_SND_QUEUELEN        64

/* MEMP_NUM_PBUF: the number of memp struct pbufs. If the application
   sends a lot of data out of ROM (or other static memory), this
   should be set high.

   NB: This is for PBUF_ROM and PBUF_REF pbufs only!

   Number of PBUF_POOL pbufs is controlled by PBUF_POOL_SIZE that,
   somewhat confusingly, breaks MEMP_NUM_* pattern.

   PBUF_RAM pbufs are allocated with mem_malloc (with MEM_LIBC_MALLOC
   set to 1 this is just system malloc), not memp_malloc.  */
#define MEMP_NUM_PBUF (1024 * 4)


/* MEMP_NUM_MLD6_GROUP: Maximum number of IPv6 multicast groups that
   can be joined.

   We need to be able to join solicited node multicast for each
   address (potentially different) and two groups for DHCP6.  All
   routers multicast is hardcoded in ip6.c and does not require
   explicit joining.  Provide also for a few extra groups just in
   case.  */
#define MEMP_NUM_MLD6_GROUP     (LWIP_IPV6_NUM_ADDRESSES + /* dhcp6 */ 2 + /* extra */ 8)


/* MEMP_NUM_TCP_SEG: the number of simultaneously queued TCP
   segments. */
#define MEMP_NUM_TCP_SEG (MEMP_NUM_TCP_PCB * TCP_SND_QUEUELEN / 2)

/* MEMP_NUM_TCP_PCB: the number of simulatenously active TCP
   connections. */
#define MEMP_NUM_TCP_PCB        128

/* MEMP_NUM_TCPIP_MSG_*: the number of struct tcpip_msg, which is used
   for sequential API communication and incoming packets. Used in
   src/api/tcpip.c. */
#define MEMP_NUM_TCPIP_MSG_API   128
#define MEMP_NUM_TCPIP_MSG_INPKT 1024

/* MEMP_NUM_UDP_PCB: the number of UDP protocol control blocks. One
   per active UDP "connection". */
#define MEMP_NUM_UDP_PCB        32

/* Pbuf options */
/* PBUF_POOL_SIZE: the number of buffers in the pbuf pool.
   This is only for PBUF_POOL pbufs, primarily used by netif drivers.

   This should have been named with the MEMP_NUM_ prefix (cf.
   MEMP_NUM_PBUF for PBUF_ROM and PBUF_REF) as it controls the size of
   yet another memp_malloc() pool.  */
#define PBUF_POOL_SIZE          (1024 * 4)

/* PBUF_POOL_BUFSIZE: the size of each pbuf in the pbuf pool.
   Use default that is based on TCP_MSS and PBUF_LINK_HLEN.  */
#undef PBUF_POOL_BUFSIZE  

/** Turn on support for lightweight critical region protection. Leaving this
 * off uses synchronization code in pbuf.c which is totally polluted with
 * races. All the other lwip source files would fall back to semaphore-based
 * synchronization, but pbuf.c is just broken, leading to incorrect allocation
 * and as a result to assertions due to buffers being double freed. */
#define SYS_LIGHTWEIGHT_PROT 1

/** Attempt to get rid of htons etc. macro issues. */
#define LWIP_PREFIX_BYTEORDER_FUNCS

#define LWIP_NOASSERT 0

#define LWIP_TCPIP_CORE_LOCKING_INPUT 0
#define LWIP_TCPIP_CORE_LOCKING 0
#define LWIP_TCP 1
#define LWIP_SOCKET 1
#define LWIP_ARP 1
#define ARP_PROXY 1
#define LWIP_ETHERNET 1
#define LWIP_COMPAT_SOCKETS 0
#define LWIP_COMPAT_MUTEX 1

#define LWIP_IPV6 1
#define LWIP_IPV6_FORWARD 1
#define LWIP_ND6_PROXY 1

#define LWIP_ND6_ALLOW_RA_UPDATES       (!LWIP_IPV6_FORWARD)
#define LWIP_IPV6_SEND_ROUTER_SOLICIT   (!LWIP_IPV6_FORWARD)
/* IPv6 autoconfig we don't need in proxy, but it required for very seldom cases 
 * iSCSI over intnet with IPv6
 */
#define LWIP_IPV6_AUTOCONFIG            1 
#if LWIP_IPV6_FORWARD /* otherwise use the default from lwip/opt.h */
#define LWIP_IPV6_DUP_DETECT_ATTEMPTS   0
#endif


/** 
 * aka Slirp mode.
 */
#define LWIP_CONNECTION_PROXY 1
#define IP_FORWARD            1

/* MEMP_NUM_SYS_TIMEOUT: the number of simultaneously active
   timeouts. */
#define MEMP_NUM_SYS_TIMEOUT    16


/* this is required for IPv6 and IGMP needs */
#define LWIP_RAND() RTRandU32()

/* Debugging stuff. */
#ifdef DEBUG
/* filter in debugging severity */
# define DBG_TYPES_ON (LWIP_DBG_ON | LWIP_DBG_TRACE | LWIP_DBG_STATE | LWIP_DBG_FRESH | LWIP_DBG_HALT)
# define DBG_MIN_LEVEL 0

# define LWIP_DEBUG LWIP_DBG_ON
/* Ethernet & ARP debugging */
# define ETHARP_DEBUG    LWIP_DBG_ON
/* IPv4 debugging */
# define IP_DEBUG    LWIP_DBG_ON
# define IP_REASS_DEBUG  LWIP_DBG_ON
/* IPv6 debugging */
# define IP6_DEBUG LWIP_DBG_ON
/* ICMP debugging */
# define ICMP_DEBUG  LWIP_DBG_ON
/* TCP debugging */
# define TCP_DEBUG   LWIP_DBG_ON
# define TCP_INPUT_DEBUG LWIP_DBG_ON
# define TCP_FR_DEBUG    LWIP_DBG_ON
# define TCP_RTO_DEBUG   LWIP_DBG_ON
# define TCP_CWND_DEBUG  LWIP_DBG_ON
# define TCP_WND_DEBUG   LWIP_DBG_ON
# define TCP_OUTPUT_DEBUG    LWIP_DBG_ON
# define TCP_RST_DEBUG   LWIP_DBG_ON
# define TCP_QLEN_DEBUG  LWIP_DBG_ON
/* RAW API debugging */
/* API debugging */
# define NETIF_DEBUG LWIP_DBG_ON
# define PBUF_DEBUG  LWIP_DBG_ON
# define API_LIB_DEBUG   LWIP_DBG_ON
# define API_MSG_DEBUG   LWIP_DBG_ON
# define SOCKETS_DEBUG   LWIP_DBG_ON

# define INET_DEBUG  LWIP_DBG_ON
# define RAW_DEBUG   LWIP_DBG_ON
# define MEM_DEBUG   LWIP_DBG_ON
# define MEMP_DEBUG  LWIP_DBG_ON
# define SYS_DEBUG   LWIP_DBG_ON

# define UDP_DEBUG   LWIP_DBG_ON
# define TCPIP_DEBUG LWIP_DBG_ON
# define DHCP_DEBUG  LWIP_DBG_ON

# define LWIP_PROXY_DEBUG LWIP_DBG_ON
/* Debug checks */
# define TCP_OVERSIZE_DBGCHECK 1
#endif /* DEBUG */

/* printf formatter definitions */
#define U16_F "hu"
#define S16_F "hd"
#define X16_F "hx"
#define U32_F "lu"
#define S32_F "ld"
#define X32_F "lx"

/* Redirect libc memory alloc functions to IPRT. */
#define malloc(x) RTMemAlloc(x)
#define realloc(x,y) RTMemRealloc((x), (y))
#define free(x) RTMemFree(x)

/* Hack to avoid function name collisions with slirp or any other code. */
#define api_msg_input lwip_api_msg_input
#define api_msg_post lwip_api_msg_post
#define etharp_arp_input lwip_etharp_arp_input
#define etharp_find_addr lwip_etharp_find_addr
#define etharp_ip_input lwip_etharp_ip_input
#define etharp_output lwip_etharp_output
#define etharp_query lwip_etharp_query
#define etharp_request lwip_etharp_request
#define etharp_tmr lwip_etharp_tmr
#define icmp_dest_unreach lwip_icmp_dest_unreach
#define icmp_input lwip_icmp_input
#define inet_chksum lwip_inet_chksum
#define inet_chksum_pbuf lwip_inet_chksum_pbuf
#define inet_chksum_pseudo lwip_inet_chksum_pseudo
#define lwip_inet_aton(cp, addr)   ipaddr_aton(cp, (ip_addr_t*)(addr))
#define ip_addr_any lwip_ip_addr_any
#define ip_addr_broadcast lwip_ip_addr_broadcast
#ifdef ip_addr_isbroadcast
# undef ip_addr_isbroadcast
# if defined(ip4_addr_isbroadcast)
#  define lwip_ip_addr_isbroadcast(ipaddr. netif) lwip_ip4_addr_isbroadcast((ipaddr)->addr, (netif))
#  define ip4_addr_isbroadcast lwip_ip4_addr_isbroadcast
# endif
#endif
#define ip_frag lwip_ip_frag
#define ip_frag_init lwip_ip_frag_init
#if 0
#define ip_init lwip_ip_init
#endif
#define ip_input lwip_ip_input
#define ip_output lwip_ip_output
#define ip_output_if lwip_ip_output_if
#define ip_reass lwip_ip_reass
#define ip_reass_tmr lwip_ip_reass_tmr
#define ip_route lwip_ip_route
#define netbuf_alloc lwip_netbuf_alloc
#define netbuf_chain lwip_netbuf_chain
#define netbuf_data lwip_netbuf_data
#define netbuf_delete lwip_netbuf_delete
#define netbuf_first lwip_netbuf_first
#define netbuf_free lwip_netbuf_free
#define netbuf_new lwip_netbuf_new
#define netbuf_next lwip_netbuf_next
#define netbuf_ref lwip_netbuf_ref
#define netconn_accept lwip_netconn_accept
#if 0
#define netconn_addr lwip_netconn_addr
#endif
#define netconn_bind lwip_netconn_bind
#define netconn_close lwip_netconn_close
#define netconn_connect lwip_netconn_connect
#define netconn_delete lwip_netconn_delete
#define netconn_disconnect lwip_netconn_disconnect
#if 0
#define netconn_err lwip_netconn_err
#define netconn_listen lwip_netconn_listen
#define netconn_new lwip_netconn_new
#define netconn_new_with_callback lwip_netconn_new_with_callback
#endif
#define netconn_new_with_proto_and_callback lwip_netconn_new_with_proto_and_callback
#if 0
#define netconn_peer lwip_netconn_peer
#endif
#define netconn_recv lwip_netconn_recv
#define netconn_send lwip_netconn_send
#if 0
#define netconn_type lwip_netconn_type
#define netconn_write lwip_netconn_write
#endif
#define netif_add lwip_netif_add
#define netif_default lwip_netif_default
#define netif_find lwip_netif_find
#define netif_init lwip_netif_init
#define netif_list lwip_netif_list
#define netif_remove lwip_netif_remove
#define netif_set_addr lwip_netif_set_addr
#define netif_set_default lwip_netif_set_default
#define netif_set_down lwip_netif_set_down
#define netif_set_gw lwip_netif_set_gw
#define netif_set_ipaddr lwip_netif_set_ipaddr
#define netif_set_netmask lwip_netif_set_netmask
#define netif_set_up lwip_netif_set_up
#if MEM_LIBC_MALLOC == 0
#define mem_free lwip_mem_free
#define mem_init lwip_mem_init
#define mem_malloc lwip_mem_malloc
#define mem_realloc lwip_mem_realloc
#endif
#define memp_free lwip_memp_free
#define memp_init lwip_memp_init
#define memp_malloc lwip_memp_malloc
#define pbuf_alloc lwip_pbuf_alloc
#define pbuf_cat lwip_pbuf_cat
#define pbuf_chain lwip_pbuf_chain
#define pbuf_clen lwip_pbuf_clen
#define pbuf_dechain lwip_pbuf_dechain
#define pbuf_dequeue lwip_pbuf_dequeue
#define pbuf_free lwip_pbuf_free
#define pbuf_header lwip_pbuf_header
#if 0
#define pbuf_init lwip_pbuf_init
#endif
#define pbuf_queue lwip_pbuf_queue
#define pbuf_realloc lwip_pbuf_realloc
#define pbuf_ref lwip_pbuf_ref
#define pbuf_take lwip_pbuf_take
#define raw_bind lwip_raw_bind
#define raw_connect lwip_raw_connect
#if 0
#define raw_init lwip_raw_init
#endif
#define raw_input lwip_raw_input
#define raw_new lwip_raw_new
#define raw_recv lwip_raw_recv
#define raw_remove lwip_raw_remove
#define raw_send lwip_raw_send
#define raw_sendto lwip_raw_sendto
#define stats_init lwip_stats_init
#define sys_arch_mbox_fetch lwip_sys_arch_mbox_fetch
#define sys_arch_mbox_tryfetch(mbox, msg) lwip_sys_arch_mbox_fetch((mbox), (msg), 1)
#define sys_arch_protect lwip_sys_arch_protect
#define sys_arch_sem_wait lwip_sys_arch_sem_wait
#define sys_arch_timeouts lwip_sys_arch_timeouts
#define sys_arch_unprotect lwip_sys_arch_unprotect
#define sys_init lwip_sys_init
#if 0
#define sys_mbox_fetch lwip_sys_mbox_fetch
#endif
#define sys_mbox_free lwip_sys_mbox_free
#define sys_mbox_new lwip_sys_mbox_new
#define sys_mbox_post lwip_sys_mbox_post
#define sys_thread_new lwip_sys_thread_new
#define sys_msleep lwip_sys_msleep
#define sys_mbox_set_invalid lwip_sys_mbox_set_invalid
#define sys_mbox_valid lwip_sys_mbox_valid
#if 1
#define sys_sem_wait_timeout lwip_sys_sem_wait_timeout
#define sys_sem_free lwip_sys_sem_free
#define sys_sem_new lwip_sys_sem_new
#define sys_sem_signal lwip_sys_sem_signal
#define lwip_sys_sem_wait lwip_sys_arch_sem_wait
#define sys_arch_sem_wait lwip_sys_arch_sem_wait
#endif
#define sys_timeout_debug lwip_sys_timeout_debug
#define sys_untimeout lwip_sys_untimeout
#define tcp_abort lwip_tcp_abort
#define tcp_accept lwip_tcp_accept
#define tcp_active_pcbs lwip_tcp_active_pcbs
#define tcp_alloc lwip_tcp_alloc
#define tcp_arg lwip_tcp_arg
#define tcp_backoff lwip_tcp_backoff
#define tcp_bind lwip_tcp_bind
#define tcp_close lwip_tcp_close
#define tcp_connect lwip_tcp_connect
#define tcp_enqueue lwip_tcp_enqueue
#define tcp_err lwip_tcp_err
#define tcp_fasttmr lwip_tcp_fasttmr
#define tcp_init lwip_tcp_init
#define tcp_input lwip_tcp_input
#define tcp_input_pcb lwip_tcp_input_pcb
#define tcp_keepalive lwip_tcp_keepalive
#if defined(tcp_listen)
# undef tcp_listen
# define tcp_listen(pcb) lwip_tcp_listen_with_backlog(pcb, TCP_DEFAULT_LISTEN_BACKLOG)
#endif
#define tcp_listen_with_backlog  lwip_tcp_listen_with_backlog
#define tcp_listen_pcbs lwip_tcp_listen_pcbs
#define tcp_new lwip_tcp_new
#define tcp_next_iss lwip_tcp_next_iss
#define tcp_output lwip_tcp_output
#define tcp_pcb_purge lwip_tcp_pcb_purge
#define tcp_pcb_remove lwip_tcp_pcb_remove
#define tcp_poll lwip_tcp_poll
#define tcp_recv lwip_tcp_recv
#define tcp_recved lwip_tcp_recved
#define tcp_rexmit lwip_tcp_rexmit
#define tcp_rexmit_rto lwip_tcp_rexmit_rto
/* tcp_rst is renaming to tcp_rst_impl, 
 * so for cleaner ABI, _impl was added. */
#define tcp_rst_impl lwip_tcp_rst_impl
#define tcp_seg_copy lwip_tcp_seg_copy
#define tcp_seg_free lwip_tcp_seg_free
#define tcp_segs_free lwip_tcp_segs_free
#define tcp_send_ctrl lwip_tcp_send_ctrl
#define tcp_sent lwip_tcp_sent
#define tcp_setprio lwip_tcp_setprio
#define tcp_slowtmr lwip_tcp_slowtmr
#define tcp_ticks lwip_tcp_ticks
#define tcp_timer_needed lwip_tcp_timer_needed
#define tcp_tmp_pcb lwip_tcp_tmp_pcb
#define tcp_tmr lwip_tcp_tmr
#define tcp_tw_pcbs lwip_tcp_tw_pcbs
#define tcp_write lwip_tcp_write
#define tcpip_apimsg lwip_tcpip_apimsg
#if 0
#define tcpip_callback lwip_tcpip_callback
#endif
#define tcpip_init lwip_tcpip_init
#define tcpip_input lwip_tcpip_input
#define udp_bind lwip_udp_bind
#define udp_connect lwip_udp_connect
#define udp_disconnect lwip_udp_disconnect
#define udp_init lwip_udp_init
#define udp_input lwip_udp_input
#define udp_new lwip_udp_new
#define udp_pcbs lwip_udp_pcbs
#define udp_recv lwip_udp_recv
#define udp_remove lwip_udp_remove
#define udp_send lwip_udp_send
#define udp_sendto lwip_udp_sendto

#define lwip_pbuf_init()
#define lwip_etharp_init()

#ifndef htons
# define htons   lwip_htons
#endif

#ifndef ntohs
# define ntohs   lwip_ntohs
#endif

#ifndef htonl
# define htonl   lwip_htonl
#endif

#ifndef ntohl
# define ntohl   lwip_ntohl
#endif
#endif
