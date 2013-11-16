/* -*- indent-tabs-mode: nil; -*- */

#include "winutils.h"
#include "proxy.h"
#include "proxy_pollmgr.h"
#include "pxremap.h"

#ifndef RT_OS_WINDOWS
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>          /* XXX: inet_ntop */
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#else
#include <iprt/stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "winpoll.h"
#endif

#include "lwip/opt.h"

#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "lwip/inet_chksum.h"
#include "lwip/ip.h"
#include "lwip/icmp.h"

#if 1 /* XXX: force debug for now */
#undef  DPRINTF0
#undef  DPRINTF
#undef  DPRINTF1
#undef  DPRINTF2
#define DPRINTF0(args) do { printf args; } while (0)
#define DPRINTF(args)  do { printf args; } while (0)
#define DPRINTF1(args) do { printf args; } while (0)
#define DPRINTF2(args) do { printf args; } while (0)
#endif


/* forward */
struct ping_pcb;


/**
 * Global state for ping proxy collected in one entity to minimize
 * globals.  There's only one instance of this structure.
 *
 * Raw ICMP sockets are promiscuous, so it doesn't make sense to have
 * multiple.  If this code ever needs to support multiple netifs, the
 * netif member should be exiled into "pcb".
 */
struct pxping {
    SOCKET sock4;
    SOCKET sock6;

    struct pollmgr_handler pmhdl4;
    struct pollmgr_handler pmhdl6;

    struct netif *netif;

    /**
     * Protect lwIP and pmgr accesses to the list of pcbs.
     */
    sys_mutex_t lock;

    /*
     * We need to find pcbs both from the guest side and from the host
     * side.  If we need to support industrial grade ping throughput,
     * we will need two pcb hashes.  For now, a short linked list
     * should be enough.  Cf. pxping_pcb_for_request() and
     * pxping_pcb_for_reply().
     */
#define PXPING_MAX_PCBS 8
    size_t npcbs;
    struct ping_pcb *pcbs;

#define TIMEOUT 5
    int timer_active;
    size_t timeout_slot;
    struct ping_pcb *timeout_list[TIMEOUT];
};


/**
 * Quasi PCB for ping.
 */
struct ping_pcb {
    ipX_addr_t src;
    ipX_addr_t dst;

    u8_t is_ipv6;

    u16_t guest_id;
    u16_t host_id;

    /**
     * Desired slot in pxping::timeout_list.  See pxping_timer().
     */
    size_t timeout_slot;

    /**
     * Chaining for pxping::timeout_list
     */
    struct ping_pcb **pprev_timeout;
    struct ping_pcb *next_timeout;

    /**
     * Chaining for pxping::pcbs
     */
    struct ping_pcb *next;

    union {
        struct sockaddr_in sin;
        struct sockaddr_in6 sin6;
    } peer;
};


/**
 * lwIP thread callback message.
 */
struct ping_msg {
    struct tcpip_msg msg;
    struct pxping *pxping;
    struct pbuf *p;
};


static void pxping_recv4(void *arg, struct pbuf *p);

static void pxping_timer(void *arg);
static void pxping_timer_needed(struct pxping *pxping);

static struct ping_pcb *pxping_pcb_for_request(struct pxping *pxping,
                                               int is_ipv6,
                                               ipX_addr_t *src, ipX_addr_t *dst,
                                               u16_t guest_id);
static struct ping_pcb *pxping_pcb_for_reply(struct pxping *pxping, int is_ipv6,
                                             ipX_addr_t *dst, u16_t host_id);

static struct ping_pcb *pxping_pcb_allocate(struct pxping *pxping);
static void pxping_pcb_register(struct pxping *pxping, struct ping_pcb *pcb);
static void pxping_pcb_deregister(struct pxping *pxping, struct ping_pcb *pcb);
static void pxping_pcb_delete(struct pxping *pxping, struct ping_pcb *pcb);
static void pxping_timeout_add(struct pxping *pxping, struct ping_pcb *pcb);
static void pxping_timeout_del(struct pxping *pxping, struct ping_pcb *pcb);
static void pxping_pcb_debug_print(struct ping_pcb *pcb);

static struct ping_msg *pxping_msg_allocate(struct pxping *pxping, struct pbuf *p);

static int pxping_pmgr_pump(struct pollmgr_handler *handler, SOCKET fd, int revents);
static void pxping_pmgr_icmp4(struct pxping *pxping,
                              size_t nread, struct sockaddr_in *peer);
static void pxping_pmgr_icmp4_echo(struct pxping *pxping,
                                   size_t iplen, struct sockaddr_in *peer);
static void pxping_pmgr_icmp4_error(struct pxping *pxping,
                                    size_t iplen, struct sockaddr_in *peer);
static void pxping_pmgr_forward_inbound(struct pxping *pxping, size_t iplen);
static void pxping_pcb_forward_inbound(void *arg);

/*
 * NB: This is not documented except in RTFS.
 *
 * If ip_output_if() is passed dest == NULL then it treats p as
 * complete IP packet with payload pointing to the IP header.  It does
 * not build IP header, ignores all header-related arguments, fetches
 * real destination from the header in the pbuf and outputs pbuf to
 * the specified netif.
 */
#define ip_raw_output_if(p, netif)                      \
    (ip_output_if((p), NULL, NULL, 0, 0, 0, (netif)))



static struct pxping g_pxping;


err_t
pxping_init(struct netif *netif, SOCKET sock)
{
    if (sock == INVALID_SOCKET) {
	return ERR_VAL;
    }

    g_pxping.sock4 = sock;
    g_pxping.netif = netif;

    sys_mutex_new(&g_pxping.lock);

    g_pxping.pmhdl4.callback = pxping_pmgr_pump;
    g_pxping.pmhdl4.data = (void *)&g_pxping;
    g_pxping.pmhdl4.slot = -1;

    pollmgr_add(&g_pxping.pmhdl4, g_pxping.sock4, POLLIN);

    ping_proxy_accept(pxping_recv4, &g_pxping);
    return ERR_OK;
}


static u32_t
update16_with_chksum(u16_t *oldp, u16_t h)
{
    u32_t sum = (u16_t)~*oldp;
    sum += h;

    *oldp = h;
    return sum;
}


static u32_t
update32_with_chksum(u32_t *oldp, u32_t u)
{
    u32_t sum = ~*oldp;
    sum = FOLD_U32T(sum);
    sum += FOLD_U32T(u);

    *oldp = u;
    return sum;
}


/**
 * ICMP Echo Request in pbuf "p" is to be proxied.
 */
static void
pxping_recv4(void *arg, struct pbuf *p)
{
    struct pxping *pxping = (struct pxping *)arg;
    struct ping_pcb *pcb;
    struct ip_hdr *iph;
    struct icmp_echo_hdr *icmph;
    u32_t sum;
    u16_t iphlen;
    u16_t id, seq;

    iph = (/* UNCONST */ struct ip_hdr *)ip_current_header();
    iphlen = ip_current_header_tot_len();

    icmph = (struct icmp_echo_hdr *)p->payload;

    id  = icmph->id;
    seq = icmph->seqno;

    pcb = pxping_pcb_for_request(pxping, 0,
                                 ip_2_ipX(ip_current_src_addr()),
                                 ip_2_ipX(ip_current_dest_addr()),
                                 id);
    if (pcb == NULL) {
        pbuf_free(p);
        return;
    }

    pxping_pcb_debug_print(pcb); /* XXX */
    printf(" seq %d len %u\n", ntohs(seq), (unsigned int)p->tot_len);

    /* rewrite ICMP echo header */
    sum = (u16_t)~icmph->chksum;
    sum += update16_with_chksum(&icmph->id, pcb->host_id);
    sum = FOLD_U32T(sum);
    icmph->chksum = ~sum;

    /*
     * TODO: Support TTL, TOS (and may be options?).
     *
     * At least on Linux "when the IP_HDRINCL option is set, datagrams
     * will not be fragmented and are limited to the interface MTU",
     * so we need to use setsockopt() to set those.
     */

    proxy_sendto(pxping->sock4, p,
                 &pcb->peer.sin, sizeof(pcb->peer.sin));

    pbuf_free(p);
}


static void
pxping_pcb_debug_print(struct ping_pcb *pcb)
{
    printf("ping %p: %d.%d.%d.%d -> %d.%d.%d.%d id %04x->%04x",
           (void *)pcb,
           ip4_addr1(&pcb->src), ip4_addr2(&pcb->src),
           ip4_addr3(&pcb->src), ip4_addr4(&pcb->src),
           ip4_addr1(&pcb->dst), ip4_addr2(&pcb->dst),
           ip4_addr3(&pcb->dst), ip4_addr4(&pcb->dst),
           ntohs(pcb->guest_id),
           ntohs(pcb->host_id));
}


static struct ping_pcb *
pxping_pcb_allocate(struct pxping *pxping)
{
    struct ping_pcb *pcb;

    if (pxping->npcbs >= PXPING_MAX_PCBS) {
        return NULL;
    }

    pcb = (struct ping_pcb *)malloc(sizeof(*pcb));
    if (pcb == NULL) {
        return NULL;
    }

    ++pxping->npcbs;
    return pcb;
}


static void
pxping_pcb_delete(struct pxping *pxping, struct ping_pcb *pcb)
{
    LWIP_ASSERT1(pxping->npcbs > 0);
    LWIP_ASSERT1(pxping->next == NULL);
    LWIP_ASSERT1(pxping->pprev_timeout == NULL);

    printf("%s: ping %p\n", __func__, (void *)pcb);

    --pxping->npcbs;
    free(pcb);
}


static void
pxping_timeout_add(struct pxping *pxping, struct ping_pcb *pcb)
{
    struct ping_pcb **chain;

    LWIP_ASSERT1(pcb->pprev_timeout == NULL);

    chain = &pxping->timeout_list[pcb->timeout_slot];
    if ((pcb->next_timeout = *chain) != NULL) {
        (*chain)->pprev_timeout = &pcb->next_timeout;
    }
    *chain = pcb;
    pcb->pprev_timeout = chain;
}


static void
pxping_timeout_del(struct pxping *pxping, struct ping_pcb *pcb)
{
    LWIP_ASSERT1(pcb->pprev_timeout != NULL);
    if (pcb->next_timeout != NULL) {
        pcb->next_timeout->pprev_timeout = pcb->pprev_timeout;
    }
    *pcb->pprev_timeout = pcb->next_timeout;
    pcb->pprev_timeout = NULL;
    pcb->next_timeout = NULL;
}


static void
pxping_pcb_register(struct pxping *pxping, struct ping_pcb *pcb)
{
    pcb->next = pxping->pcbs;
    pxping->pcbs = pcb;

    pxping_timeout_add(pxping, pcb);
}


static void
pxping_pcb_deregister(struct pxping *pxping, struct ping_pcb *pcb)
{
    struct ping_pcb **p;

    for (p = &pxping->pcbs; *p != NULL; p = &(*p)->next) {
        if (*p == pcb) {
            *p = pcb->next;
            break;
        }
    }

    pxping_timeout_del(pxping, pcb);
}


static struct ping_pcb *
pxping_pcb_for_request(struct pxping *pxping,
                       int is_ipv6, ipX_addr_t *src, ipX_addr_t *dst,
                       u16_t guest_id)
{
    struct ping_pcb *pcb;

    /* on lwip thread, so no concurrent updates */
    for (pcb = pxping->pcbs; pcb != NULL; pcb = pcb->next) {
        if (pcb->guest_id == guest_id
            && pcb->is_ipv6 == is_ipv6
            && ipX_addr_cmp(is_ipv6, &pcb->dst, dst)
            && ipX_addr_cmp(is_ipv6, &pcb->src, src))
        {
            break;
        }
    }

    if (pcb == NULL) {
        pcb = pxping_pcb_allocate(pxping);
        if (pcb == NULL) {
            return NULL;
        }

        pcb->is_ipv6 = is_ipv6;
        ipX_addr_copy(is_ipv6, pcb->src, *src);
        ipX_addr_copy(is_ipv6, pcb->dst, *dst);

        pcb->guest_id = guest_id;
        pcb->host_id = random() & 0xffffUL; /* XXX */

        pcb->pprev_timeout = NULL;
        pcb->next_timeout = NULL;

        if (is_ipv6) {
        }
        else {
            pcb->peer.sin.sin_family = AF_INET;
#if HAVE_SA_LEN
            pcb->peer.sin.sin_len = sizeof(pcb->peer.sin);
#endif
            pxremap_outbound_ip4((ip_addr_t *)&pcb->peer.sin.sin_addr,
                                 ipX_2_ip(&pcb->dst));
            pcb->peer.sin.sin_port = htons(IP_PROTO_ICMP);
        }

        pcb->timeout_slot = pxping->timeout_slot;

        sys_mutex_lock(&pxping->lock);
        pxping_pcb_register(pxping, pcb);
        sys_mutex_unlock(&pxping->lock);

        pxping_pcb_debug_print(pcb); /* XXX */
        printf(" - created\n");

        pxping_timer_needed(pxping);
    }
    else {
        /* just bump up expiration timeout lazily */
        pxping_pcb_debug_print(pcb); /* XXX */
        printf(" - slot %d -> %d\n",
               (unsigned int)pcb->timeout_slot,
               (unsigned int)pxping->timeout_slot);
        pcb->timeout_slot = pxping->timeout_slot;
    }

    return pcb;
}


/**
 * Called on pollmgr thread.  Caller must do the locking since caller
 * is going to use the returned pcb, which needs to be protected from
 * being expired by pxping_timer() on lwip thread.
 */
static struct ping_pcb *
pxping_pcb_for_reply(struct pxping *pxping,
                     int is_ipv6, ipX_addr_t *dst, u16_t host_id)
{
    struct ping_pcb *pcb;

    for (pcb = pxping->pcbs; pcb != NULL; pcb = pcb->next) {
        if (pcb->host_id == host_id
            && pcb->is_ipv6 == is_ipv6
            /* XXX: allow broadcast pings? */
            && ipX_addr_cmp(is_ipv6, &pcb->dst, dst))
        {
            return pcb;
        }
    }

    return NULL;
}


static void
pxping_timer(void *arg)
{
    struct pxping *pxping = (struct pxping *)arg;
    struct ping_pcb **chain, *pcb;
    u32_t mask;

    pxping->timer_active = 0;

    /*
     * New slot points to the list of pcbs to check for expiration.
     */
    LWIP_ASSERT1(pxping->timeout_slot < TIMEOUT);
    if (++pxping->timeout_slot == TIMEOUT) {
        pxping->timeout_slot = 0;
    }

    chain = &pxping->timeout_list[pxping->timeout_slot];
    pcb = *chain;

    /* protect from pollmgr concurrent reads */
    sys_mutex_lock(&pxping->lock);

    while (pcb != NULL) {
        struct ping_pcb *xpcb = pcb;
        pcb = pcb->next_timeout;

        if (xpcb->timeout_slot == pxping->timeout_slot) {
            /* expired */
            printf("... ");
            pxping_pcb_debug_print(xpcb);
            printf(" - expired\n");

            pxping_pcb_deregister(pxping, xpcb);
            pxping_pcb_delete(pxping, xpcb);
        }
        else {
            /*
             * If there was another request, we updated timeout_slot
             * but delayed actually moving the pcb until now.
             */
            printf("... ");
            pxping_pcb_debug_print(xpcb);
            printf(" - alive slot %d -> %d\n",
                   (unsigned int)pxping->timeout_slot,
                   (unsigned int)xpcb->timeout_slot);

            pxping_timeout_del(pxping, xpcb); /* from current slot */
            pxping_timeout_add(pxping, xpcb); /* to new slot */
        }
    }

    sys_mutex_unlock(&pxping->lock);
    pxping_timer_needed(pxping);
}


static void
pxping_timer_needed(struct pxping *pxping)
{
    if (!pxping->timer_active && pxping->pcbs != NULL) {
        pxping->timer_active = 1;
        sys_timeout(1 * 1000, pxping_timer, pxping);
    }
}


static struct ping_msg *
pxping_msg_allocate(struct pxping *pxping, struct pbuf *p)
{
    struct ping_msg *msg;

    msg = (struct ping_msg *)malloc(sizeof(*msg));
    if (msg == NULL) {
        return NULL;
    }

    msg->msg.type = TCPIP_MSG_CALLBACK_STATIC;
    msg->msg.sem = NULL;
    msg->msg.msg.cb.function = pxping_pcb_forward_inbound;
    msg->msg.msg.cb.ctx = (void *)msg;

    msg->pxping = pxping;
    msg->p = p;

    return msg;
}


static int
pxping_pmgr_pump(struct pollmgr_handler *handler, SOCKET fd, int revents)
{
    struct pxping *pxping;
    struct ping_pcb *pcb;
    struct sockaddr_storage ss;
    socklen_t sslen = sizeof(ss);
    ssize_t nread;

    pxping = (struct pxping *)handler->data;

    if (revents & ~(POLLIN|POLLERR)) {
        DPRINTF0(("%s: unexpected revents 0x%x\n", __func__, revents));
        return POLLIN;
    }

    if (revents & POLLERR) {
        int sockerr = -1;
        socklen_t optlen = (socklen_t)sizeof(sockerr);
        int status;

        status = getsockopt(fd, SOL_SOCKET,
                            SO_ERROR, (char *)&sockerr, &optlen);
        if (status < 0) {
            DPRINTF(("%s: sock %d: SO_ERROR failed with errno %d\n",
                     __func__, fd, errno));
        }
        else {
            DPRINTF(("%s: sock %d: errno %d\n",
                     __func__, fd, sockerr));
        }
    }

    if ((revents & POLLIN) == 0) {
        return POLLIN;
    }

    memset(&ss, 0, sizeof(ss));
    nread = recvfrom(fd, pollmgr_udpbuf, sizeof(pollmgr_udpbuf), 0,
                     (struct sockaddr *)&ss, &sslen);
    if (nread < 0) {
        perror(__func__);
        return POLLIN;
    }

    pxping_pmgr_icmp4(pxping, nread, (struct sockaddr_in *)&ss);

    return POLLIN;
}


/**
 * Process incoming ICMP message for the host.
 * NB: we will get a lot of spam here and have to sift through it.
 */
static void
pxping_pmgr_icmp4(struct pxping *pxping,
                  size_t nread, struct sockaddr_in *peer)
{
    struct ip_hdr *iph;
    struct icmp_echo_hdr *icmph;
    u16_t iplen;

    if (nread < IP_HLEN) {
        DPRINTF2(("%s: read %d bytes, IP header truncated\n",
                  __func__, (unsigned int)nread));
        return;
    }

    iph = (struct ip_hdr *)pollmgr_udpbuf;

    /* match version */
    if (IPH_V(iph) != 4) {
        DPRINTF2(("%s: unexpected IP version %d\n", __func__, IPH_V(iph)));
        return;
    }

    /* no fragmentation */
    if ((IPH_OFFSET(iph) & PP_HTONS(IP_OFFMASK | IP_MF)) != 0) {
        DPRINTF2(("%s: dropping fragmented datagram\n", __func__));
        return;
    }

    /* no options */
    if (IPH_HL(iph) * 4 != IP_HLEN) {
        DPRINTF2(("%s: dropping datagram with options (IP header length %d)\n",
                  __func__, IPH_HL(iph) * 4));
        return;
    }

    if (IPH_PROTO(iph) != IP_PROTO_ICMP) {
        DPRINTF2(("%s: unexpected protocol %d\n", __func__, IPH_PROTO(iph)));
        return;
    }

    if (IPH_TTL(iph) == 1) {
        DPRINTF2(("%s: dropping packet with ttl 1\n", __func__));
        return;
    }

    iplen = IPH_LEN(iph);
#if !defined(RT_OS_DARWIN)
    /* darwin reports IPH_LEN in host byte order */
    iplen = ntohs(iplen);
#endif
#if defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS)
    /* darwin and solaris change IPH_LEN to payload length only */
    iplen += IP_HLEN;           /* we verified there are no options */
    IPH_LEN(iph) = htons(iplen);
#endif
    if (nread < iplen) {
        DPRINTF2(("%s: read %d bytes but total length is %d bytes\n",
                  __func__, (unsigned int)nread, (unsigned int)iplen));
        return;
    }

    if (iplen < IP_HLEN + ICMP_HLEN) {
        DPRINTF2(("%s: IP length %d bytes, ICMP header truncated\n",
                  __func__, iplen));
        return;
    }

    icmph = (struct icmp_echo_hdr *)(pollmgr_udpbuf + IP_HLEN);
    if (ICMPH_TYPE(icmph) == ICMP_ER) {
        pxping_pmgr_icmp4_echo(pxping, iplen, peer);
    }
    else if (ICMPH_TYPE(icmph) == ICMP_DUR || ICMPH_TYPE(icmph) == ICMP_TE) {
        pxping_pmgr_icmp4_error(pxping, iplen, peer);
    }
#if 1
    else {
        DPRINTF2(("%s: ignoring ICMP type %d\n", __func__, ICMPH_TYPE(icmph)));
    }
#endif
}


/**
 * Check if this incoming ICMP echo reply is for one of our pings and
 * forward it to the guest.
 */
static void
pxping_pmgr_icmp4_echo(struct pxping *pxping,
                       size_t iplen, struct sockaddr_in *peer)
{
    struct ip_hdr *iph;
    struct icmp_echo_hdr *icmph;
    u16_t id, seq;
    struct ping_pcb *pcb;
    ip_addr_t pcb_src, pcb_dst;
    u16_t guest_id;
    u32_t sum;

    iph = (struct ip_hdr *)pollmgr_udpbuf;
    icmph = (struct icmp_echo_hdr *)(pollmgr_udpbuf + IP_HLEN);

    id  = icmph->id;
    seq = icmph->seqno;

    {
        char addrbuf[sizeof "255.255.255.255"];
        const char *addrstr;

        addrstr = inet_ntop(AF_INET, &peer->sin_addr, addrbuf, sizeof(addrbuf));
        DPRINTF(("<--- PING %s id 0x%x seq %d\n",
                 addrstr, ntohs(id), ntohs(seq)));
    }

    sys_mutex_lock(&pxping->lock);
    pcb = pxping_pcb_for_reply(pxping, 0, ip_2_ipX(&iph->src), id);
    if (pcb == NULL) {
        sys_mutex_unlock(&pxping->lock);
        DPRINTF2(("%s: no match\n", __func__));
        return;
    }

    DPRINTF2(("%s: pcb %p\n", __func__, (void *)pcb));

    /* save info before unlocking since pcb may expire */
    ip_addr_copy(pcb_src, *ipX_2_ip(&pcb->src));
    ip_addr_copy(pcb_dst, *ipX_2_ip(&pcb->dst));
    guest_id = pcb->guest_id;

    sys_mutex_unlock(&pxping->lock);

    /* rewrite inner ICMP echo header */
    sum = (u16_t)~icmph->chksum;
    sum += update16_with_chksum(&icmph->id, guest_id);
    sum = FOLD_U32T(sum);
    icmph->chksum = ~sum;

    /* rewrite outer IP header */
    sum = (u16_t)~IPH_CHKSUM(iph);
    sum += update32_with_chksum((u32_t *)&iph->dest, ip4_addr_get_u32(&pcb_src));
    IPH_TTL_SET(iph, IPH_TTL(iph) - 1);
    sum += PP_NTOHS(~0x0100);
    sum = FOLD_U32T(sum);
    IPH_CHKSUM_SET(iph, ~sum);

    pxping_pmgr_forward_inbound(pxping, iplen);
}


/**
 * Check if this incoming ICMP error (destination unreachable or time
 * exceeded) is about one of our pings and forward it to the guest.
 */
static void
pxping_pmgr_icmp4_error(struct pxping *pxping,
                        size_t iplen, struct sockaddr_in *peer)
{
    struct ip_hdr *iph, *oiph;
    struct icmp_echo_hdr *icmph, *oicmph;
    u16_t oipoff, oiphlen, oiplen;
    u16_t id, seq;
    struct ping_pcb *pcb;
    ip_addr_t pcb_src, pcb_dst;
    u16_t guest_id;
    u32_t sum;

    iph = (struct ip_hdr *)pollmgr_udpbuf;
    icmph = (struct icmp_echo_hdr *)(pollmgr_udpbuf + IP_HLEN);

    oipoff = IP_HLEN + ICMP_HLEN;
    oiplen = iplen - oipoff; /* NB: truncated length, not IPH_LEN(oiph) */
    if (oiplen < IP_HLEN) {
        DPRINTF2(("%s: original datagram truncated to %d bytes\n",
                  __func__, oiplen));
    }

    /* IP header of the original message */
    oiph = (struct ip_hdr *)(pollmgr_udpbuf + oipoff);

    /* match version */
    if (IPH_V(oiph) != 4) {
        DPRINTF2(("%s: unexpected IP version %d\n", __func__, IPH_V(oiph)));
        return;
    }

    /* can't match fragments except the first one */
    if ((IPH_OFFSET(oiph) & PP_HTONS(IP_OFFMASK)) != 0) {
        DPRINTF2(("%s: ignoring fragment with offset %d\n",
                  __func__, ntohs(IPH_OFFSET(oiph) & PP_HTONS(IP_OFFMASK))));
        return;
    }

    if (IPH_PROTO(oiph) != IP_PROTO_ICMP) {
#if 0
        /* don't spam with every "destination unreachable" in the system */
        DPRINTF2(("%s: ignoring protocol %d\n", __func__, IPH_PROTO(oiph)));
#endif
        return;
    }

    oiphlen = IPH_HL(oiph) * 4;
    if (oiplen < oiphlen + ICMP_HLEN) {
        DPRINTF2(("%s: original datagram truncated to %d bytes\n",
                  __func__, oiplen));
        return;
    }

    oicmph = (struct icmp_echo_hdr *)(pollmgr_udpbuf + oipoff + oiphlen);
    if (ICMPH_TYPE(oicmph) != ICMP_ECHO) {
        DPRINTF2(("%s: ignoring ICMP error for original ICMP type %d\n",
                  __func__, ICMPH_TYPE(oicmph)));
        return;
    }

    id  = oicmph->id;
    seq = oicmph->seqno;

    {
        char addrbuf[sizeof "255.255.255.255"];
        const char *addrstr;

        addrstr = inet_ntop(AF_INET, &oiph->dest, addrbuf, sizeof(addrbuf));
        DPRINTF2(("%s: ping %s id 0x%x seq %d",
                  __func__, addrstr, ntohs(id), ntohs(seq)));
    }

    if (ICMPH_TYPE(icmph) == ICMP_DUR) {
        DPRINTF2((" unreachable (code %d)\n", ICMPH_CODE(icmph)));
    }
    else {
        DPRINTF2((" time exceeded\n"));
    }

    sys_mutex_lock(&pxping->lock);
    pcb = pxping_pcb_for_reply(pxping, 0, ip_2_ipX(&oiph->dest), id);
    if (pcb == NULL) {
        sys_mutex_unlock(&pxping->lock);
        DPRINTF2(("%s: no match\n", __func__));
        return;
    }

    DPRINTF2(("%s: pcb %p\n", __func__, (void *)pcb));

    /* save info before unlocking since pcb may expire */
    ip_addr_copy(pcb_src, *ipX_2_ip(&pcb->src));
    ip_addr_copy(pcb_dst, *ipX_2_ip(&pcb->dst));
    guest_id = pcb->guest_id;

    sys_mutex_unlock(&pxping->lock);

    /*
     * NB: Checksum in the outer ICMP error header is not affected by
     * changes to inner headers.
     */

    /* rewrite inner ICMP echo header */
    sum = (u16_t)~oicmph->chksum;
    sum += update16_with_chksum(&oicmph->id, guest_id);
    sum = FOLD_U32T(sum);
    oicmph->chksum = ~sum;

    /* rewrite inner IP header */
    sum = (u16_t)~IPH_CHKSUM(oiph);
    sum += update32_with_chksum((u32_t *)&oiph->src, ip4_addr_get_u32(&pcb_src));
    sum = FOLD_U32T(sum);
    IPH_CHKSUM_SET(oiph, ~sum);

    /* rewrite outer IP header */
    sum = (u16_t)~IPH_CHKSUM(iph);
    sum += update32_with_chksum((u32_t *)&iph->dest, ip4_addr_get_u32(&pcb_src));
    IPH_TTL_SET(iph, IPH_TTL(iph) - 1);
    sum += PP_NTOHS(~0x0100);
    sum = FOLD_U32T(sum);
    IPH_CHKSUM_SET(iph, ~sum);

    pxping_pmgr_forward_inbound(pxping, iplen);
}


/**
 * Hand off ICMP datagram to the lwip thread where it will be
 * forwarded to the guest.
 *
 * We no longer need ping_pcb.  The pcb may get expired on the lwip
 * thread, but we have already patched necessary information into the
 * datagram.
 */
static void
pxping_pmgr_forward_inbound(struct pxping *pxping, size_t iplen)
{
    struct pbuf *p;
    struct ping_msg *msg;
    err_t error;

    p = pbuf_alloc(PBUF_LINK, iplen, PBUF_RAM);
    if (p == NULL) {
        DPRINTF(("%s: pbuf_alloc(%d) failed\n",
                 __func__, (unsigned int)iplen));
        return;
    }

    error = pbuf_take(p, pollmgr_udpbuf, (u16_t)iplen);
    if (error != ERR_OK) {
        DPRINTF(("%s: pbuf_take(%d) failed\n",
                 __func__, (unsigned int)iplen));
        pbuf_free(p);
        return;
    }

    msg = pxping_msg_allocate(pxping, p);
    if (msg == NULL) {
        pbuf_free(p);
        return;
    }

    /* call pxping_pcb_forward_inbound() on lwip thread */
    proxy_lwip_post(&msg->msg);
}


static void
pxping_pcb_forward_inbound(void *arg)
{
    struct ping_msg *msg = (struct ping_msg *)arg;
    err_t error;

    LWIP_ASSERT1(msg != NULL);
    LWIP_ASSERT1(msg->pxping != NULL);
    LWIP_ASSERT1(msg->p != NULL);

    error = ip_raw_output_if(msg->p, msg->pxping->netif);
    if (error != ERR_OK) {
        DPRINTF(("%s: ip_output_if: %s\n",
                 __func__, proxy_lwip_strerr(error)));
    }

    free(msg);
}
