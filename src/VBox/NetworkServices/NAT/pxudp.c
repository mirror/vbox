/* -*- indent-tabs-mode: nil; -*- */

#include "winutils.h"
#include "proxy.h"
#include "proxy_pollmgr.h"
#include "pxremap.h"

#ifndef RT_OS_WINDOWS
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <poll.h>

#include <err.h>                /* BSD'ism */
#else
#include <stdlib.h>
#include <iprt/stdint.h>
#include <stdio.h>
#include "winpoll.h"
#endif

#include "lwip/opt.h"

#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"

struct pxudp {
    /**
     * Our poll manager handler.
     */
    struct pollmgr_handler pmhdl;

    /**
     * lwIP ("internal") side of the proxied connection.
     */
    struct udp_pcb *pcb;

    /**
     * Host ("external") side of the proxied connection.
     */
    SOCKET sock;

    /**
     * For some protocols (notably: DNS) we know we are getting just
     * one reply, so we don't want the pcb and the socket to sit there
     * waiting to be g/c'ed by timeout.  This field counts request and
     * replies for them.
     */
    int count;

    /**
     * Mailbox for inbound pbufs.
     *
     * XXX: since we have single producer and single consumer we can
     * use lockless ringbuf like for pxtcp.
     */
    sys_mbox_t inmbox;

    /**
     * lwIP thread's strong reference to us.
     */
    struct pollmgr_refptr *rp;

    /*
     * We use static messages to void malloc/free overhead.
     */
    struct tcpip_msg msg_delete;   /* delete pxudp */
    struct tcpip_msg msg_inbound;  /* trigger send of inbound data */
};


static struct pxudp *pxudp_allocate(void);
static void pxudp_drain_inmbox(struct pxudp *);
static void pxudp_free(struct pxudp *);

static struct udp_pcb *pxudp_pcb_dissociate(struct pxudp *);

/* poll manager callbacks for pxudp related channels */
static int pxudp_pmgr_chan_add(struct pollmgr_handler *, SOCKET, int);
static int pxudp_pmgr_chan_del(struct pollmgr_handler *, SOCKET, int);

/* helper functions for sending/receiving pxudp over poll manager channels */
static ssize_t pxudp_chan_send(enum pollmgr_slot_t, struct pxudp *);
static ssize_t pxudp_chan_send_weak(enum pollmgr_slot_t, struct pxudp *);
static struct pxudp *pxudp_chan_recv(struct pollmgr_handler *, SOCKET, int);
static struct pxudp *pxudp_chan_recv_strong(struct pollmgr_handler *, SOCKET, int);

/* poll manager callbacks for individual sockets */
static int pxudp_pmgr_pump(struct pollmgr_handler *, SOCKET, int);

/* convenience function for poll manager callback */
static int pxudp_schedule_delete(struct pxudp *);

/* lwip thread callbacks called via proxy_lwip_post() */
static void pxudp_pcb_delete_pxudp(void *);

/* udp pcb callbacks &c */
static void pxudp_pcb_accept(void *, struct udp_pcb *, struct pbuf *, ip_addr_t *, u16_t);
static void pxudp_pcb_recv(void *, struct udp_pcb *, struct pbuf *, ip_addr_t *, u16_t);
static void pxudp_pcb_forward_outbound(struct pxudp *, struct pbuf *, ip_addr_t *, u16_t);
static void pxudp_pcb_expired(struct pxudp *);
static void pxudp_pcb_write_inbound(void *);
static void pxudp_pcb_forward_inbound(struct pxudp *);

/* poll manager handlers for pxudp channels */
static struct pollmgr_handler pxudp_pmgr_chan_add_hdl;
static struct pollmgr_handler pxudp_pmgr_chan_del_hdl;


void
pxudp_init(void)
{
    /*
     * Create channels.
     */
    pxudp_pmgr_chan_add_hdl.callback = pxudp_pmgr_chan_add;
    pxudp_pmgr_chan_add_hdl.data = NULL;
    pxudp_pmgr_chan_add_hdl.slot = -1;
    pollmgr_add_chan(POLLMGR_CHAN_PXUDP_ADD, &pxudp_pmgr_chan_add_hdl);

    pxudp_pmgr_chan_del_hdl.callback = pxudp_pmgr_chan_del;
    pxudp_pmgr_chan_del_hdl.data = NULL;
    pxudp_pmgr_chan_del_hdl.slot = -1;
    pollmgr_add_chan(POLLMGR_CHAN_PXUDP_DEL, &pxudp_pmgr_chan_del_hdl);

    udp_proxy_accept(pxudp_pcb_accept);
}


/**
 * Syntactic sugar for sending pxudp pointer over poll manager
 * channel.  Used by lwip thread functions.
 */
static ssize_t
pxudp_chan_send(enum pollmgr_slot_t chan, struct pxudp *pxudp)
{
    return pollmgr_chan_send(chan, &pxudp, sizeof(pxudp));
}


/**
 * Syntactic sugar for sending weak reference to pxudp over poll
 * manager channel.  Used by lwip thread functions.
 */
static ssize_t
pxudp_chan_send_weak(enum pollmgr_slot_t chan, struct pxudp *pxudp)
{
    pollmgr_refptr_weak_ref(pxudp->rp);
    return pollmgr_chan_send(chan, &pxudp->rp, sizeof(pxudp->rp));
}


/**
 * Counterpart of pxudp_chan_send().
 */
static struct pxudp *
pxudp_chan_recv(struct pollmgr_handler *handler, SOCKET fd, int revents)
{
    struct pxudp *pxudp;

    pxudp = (struct pxudp *)pollmgr_chan_recv_ptr(handler, fd, revents);
    return pxudp;
}


/**
 * Counterpart of pxudp_chan_send_weak().
 */
struct pxudp *
pxudp_chan_recv_strong(struct pollmgr_handler *handler, SOCKET fd, int revents)
{
    struct pollmgr_refptr *rp;
    struct pollmgr_handler *base;
    struct pxudp *pxudp;

    rp = (struct pollmgr_refptr *)pollmgr_chan_recv_ptr(handler, fd, revents);
    base = (struct pollmgr_handler *)pollmgr_refptr_get(rp);
    pxudp = (struct pxudp *)base;

    return pxudp;
}


/**
 * POLLMGR_CHAN_PXUDP_ADD handler.
 *
 * Get new pxudp from lwip thread and start polling its socket.
 */
static int
pxudp_pmgr_chan_add(struct pollmgr_handler *handler, SOCKET fd, int revents)
{
    struct pxudp *pxudp;
    int status;

    pxudp = pxudp_chan_recv(handler, fd, revents);
    DPRINTF(("pxudp_add: new pxudp %p; pcb %p\n",
             (void *)pxudp, (void *)pxudp->pcb));

    LWIP_ASSERT1(pxudp != NULL);
    LWIP_ASSERT1(pxudp->pmhdl.callback != NULL);
    LWIP_ASSERT1(pxudp->pmhdl.data = (void *)pxudp);
    LWIP_ASSERT1(pxudp->pmhdl.slot < 0);


    status = pollmgr_add(&pxudp->pmhdl, pxudp->sock, POLLIN);
    if (status < 0) {
        pxudp_schedule_delete(pxudp);
    }

    return POLLIN;
}


/**
 * POLLMGR_CHAN_PXUDP_DEL handler.
 */
static int
pxudp_pmgr_chan_del(struct pollmgr_handler *handler, SOCKET fd, int revents)
{
    struct pxudp *pxudp;

    pxudp = pxudp_chan_recv_strong(handler, fd, revents);
    if (pxudp == NULL) {
        return POLLIN;
    }

    DPRINTF(("pxudp_del: pxudp %p; socket %d\n", (void *)pxudp, pxudp->sock));

    pollmgr_del_slot(pxudp->pmhdl.slot);

    /*
     * Go back to lwip thread to delete after any pending callbacks
     * for unprocessed inbound traffic are drained.
     */
    pxudp_schedule_delete(pxudp);

    return POLLIN;
}


static struct pxudp *
pxudp_allocate(void)
{
    struct pxudp *pxudp;
    err_t error;

    pxudp = (struct pxudp *)malloc(sizeof(*pxudp));
    if (pxudp == NULL) {
        return NULL;
    }

    pxudp->pmhdl.callback = NULL;
    pxudp->pmhdl.data = (void *)pxudp;
    pxudp->pmhdl.slot = -1;

    pxudp->pcb = NULL;
    pxudp->sock = INVALID_SOCKET;
    pxudp->count = 0;

    pxudp->rp = pollmgr_refptr_create(&pxudp->pmhdl);
    if (pxudp->rp == NULL) {
        free(pxudp);
        return NULL;
    }

    error = sys_mbox_new(&pxudp->inmbox, 16);
    if (error != ERR_OK) {
        pollmgr_refptr_unref(pxudp->rp);
        free(pxudp);
        return NULL;
    }

#define CALLBACK_MSG(MSG, FUNC)                         \
    do {                                                \
        pxudp->MSG.type = TCPIP_MSG_CALLBACK_STATIC;    \
        pxudp->MSG.sem = NULL;                          \
        pxudp->MSG.msg.cb.function = FUNC;              \
        pxudp->MSG.msg.cb.ctx = (void *)pxudp;          \
    } while (0)

    CALLBACK_MSG(msg_delete, pxudp_pcb_delete_pxudp);
    CALLBACK_MSG(msg_inbound, pxudp_pcb_write_inbound);

    return pxudp;
}


static void
pxudp_drain_inmbox(struct pxudp *pxudp)
{
    void *ptr;

    if (!sys_mbox_valid(&pxudp->inmbox)) {
        return;
    }

    while (sys_mbox_tryfetch(&pxudp->inmbox, &ptr) != SYS_MBOX_EMPTY) {
        struct pbuf *p = (struct pbuf *)ptr;
        pbuf_free(p);
    }

    sys_mbox_free(&pxudp->inmbox);
    sys_mbox_set_invalid(&pxudp->inmbox);
}


static void
pxudp_free(struct pxudp *pxudp)
{
    pxudp_drain_inmbox(pxudp);
    free(pxudp);
}


/**
 * Dissociate pxudp and its udp_pcb.
 *
 * Unlike its TCP cousin returns the pcb since UDP pcbs need to be
 * actively deleted, so save callers the trouble of saving a copy
 * before calling us.
 */
static struct udp_pcb *
pxudp_pcb_dissociate(struct pxudp *pxudp)
{
    struct udp_pcb *pcb;

    if (pxudp == NULL || pxudp->pcb == NULL) {
        return NULL;
    }

    pcb = pxudp->pcb;

    udp_recv(pxudp->pcb, NULL, NULL);
    pxudp->pcb = NULL;

    return pcb;
}


/**
 * Lwip thread callback invoked via pxudp::msg_delete
 *
 * Since we use static messages to communicate to the lwip thread, we
 * cannot delete pxudp without making sure there are no unprocessed
 * messages in the lwip thread mailbox.
 *
 * The easiest way to ensure that is to send this "delete" message as
 * the last one and when it's processed we know there are no more and
 * it's safe to delete pxudp.
 *
 * Channel callback should use pxudp_schedule_delete() convenience
 * function defined below.
 */
static void
pxudp_pcb_delete_pxudp(void *arg)
{
    struct pxudp *pxudp = (struct pxudp *)arg;
    struct udp_pcb *pcb;

    LWIP_ASSERT1(pxudp != NULL);

    if (pxudp->sock != INVALID_SOCKET) {
        closesocket(pxudp->sock);
        pxudp->sock = INVALID_SOCKET;
    }

    pcb = pxudp_pcb_dissociate(pxudp);
    if (pcb != NULL) {
        udp_remove(pcb);
    }

    pollmgr_refptr_unref(pxudp->rp);
    pxudp_free(pxudp);
}


/**
 * Poll manager callback should use this convenience wrapper to
 * schedule pxudp deletion on the lwip thread and to deregister from
 * the poll manager.
 */
static int
pxudp_schedule_delete(struct pxudp *pxudp)
{
    /*
     * If pollmgr_refptr_get() is called by any channel before
     * scheduled deletion happens, let them know we are gone.
     */
    pxudp->pmhdl.slot = -1;

    /*
     * Schedule deletion.  Since poll manager thread may be pre-empted
     * right after we send the message, the deletion may actually
     * happen on the lwip thread before we return from this function,
     * so it's not safe to refer to pxudp after this call.
     */
    proxy_lwip_post(&pxudp->msg_delete);

    /* tell poll manager to deregister us */
    return -1;
}


/**
 * New proxied UDP conversation created.
 * Global callback for udp_proxy_accept().
 */
static void
pxudp_pcb_accept(void *arg, struct udp_pcb *newpcb, struct pbuf *p,
                 ip_addr_t *addr, u16_t port)
{
    struct pxudp *pxudp;
    ipX_addr_t dst_addr;
    int mapping;
    int sdom;
    SOCKET sock;

    LWIP_ASSERT1(newpcb != NULL);
    LWIP_ASSERT1(p != NULL);
    LWIP_UNUSED_ARG(arg);

    pxudp = pxudp_allocate();
    if (pxudp == NULL) {
        DPRINTF(("pxudp_allocate: failed\n"));
        udp_remove(newpcb);
        pbuf_free(p);
        return;
    }

    sdom = PCB_ISIPV6(newpcb) ? PF_INET6 : PF_INET;
    mapping = pxremap_outbound_ipX(PCB_ISIPV6(newpcb), &dst_addr, &newpcb->local_ip);

#if 0 /* XXX: DNS IPv6->IPv4 remapping hack */
    if (mapping == PXREMAP_MAPPED
        && newpcb->local_port == 53
        && PCB_ISIPV6(newpcb))
    {
        /*
         * "Remap" DNS over IPv6 to IPv4 since Ubuntu dnsmasq does not
         * listen on IPv6.
         */
        sdom = PF_INET;
        ipX_addr_set_loopback(0, &dst_addr);
    }
#endif  /* DNS IPv6->IPv4 remapping hack */

    sock = proxy_connected_socket(sdom, SOCK_DGRAM,
                                  &dst_addr, newpcb->local_port);
    if (sock == INVALID_SOCKET) {
        udp_remove(newpcb);
        pbuf_free(p);
        return;
    }

    pxudp->sock = sock;
    pxudp->pcb = newpcb;
    udp_recv(newpcb, pxudp_pcb_recv, pxudp);

    pxudp->pmhdl.callback = pxudp_pmgr_pump;
    pxudp_chan_send(POLLMGR_CHAN_PXUDP_ADD, pxudp);

    /* dispatch directly instead of calling pxudp_pcb_recv() */
    pxudp_pcb_forward_outbound(pxudp, p, addr, port);
}


/**
 * udp_recv() callback.
 */
static void
pxudp_pcb_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
               ip_addr_t *addr, u16_t port)
{
    struct pxudp *pxudp = (struct pxudp *)arg;

    LWIP_ASSERT1(pxudp != NULL);
    LWIP_ASSERT1(pcb == pxudp->pcb);
    LWIP_UNUSED_ARG(pcb);

    if (p != NULL) {
        pxudp_pcb_forward_outbound(pxudp, p, addr, port);
    }
    else {
        pxudp_pcb_expired(pxudp);
    }
}


static void
pxudp_pcb_forward_outbound(struct pxudp *pxudp, struct pbuf *p,
                           ip_addr_t *addr, u16_t port)
{
    LWIP_UNUSED_ARG(addr);
    LWIP_UNUSED_ARG(port);

    if (pxudp->pcb->local_port == 53) {
        ++pxudp->count;
    }

    proxy_sendto(pxudp->sock, p, NULL, 0);
}


/**
 * Proxy udp_pcbs are expired by timer, which is signaled by passing
 * NULL pbuf to the udp_recv() callback.  At that point the pcb is
 * removed from the list of proxy udp pcbs so no new datagrams will be
 * delivered.
 */
static void
pxudp_pcb_expired(struct pxudp *pxudp)
{
    struct udp_pcb *pcb;

    DPRINTF2(("%s: pxudp %p, pcb %p, sock %d: expired\n",
              __func__, (void *)pxudp, (void *)pxudp->pcb, pxudp->sock));

    pcb = pxudp_pcb_dissociate(pxudp);
    if (pcb != NULL) {
        udp_remove(pcb);
    }

    pxudp_chan_send_weak(POLLMGR_CHAN_PXUDP_DEL, pxudp);
}


/**
 */
static int
pxudp_pmgr_pump(struct pollmgr_handler *handler, SOCKET fd, int revents)
{
    struct pxudp *pxudp;
    struct pbuf *p;
    ssize_t nread;
    err_t error;

    pxudp = (struct pxudp *)handler->data;
    LWIP_ASSERT1(handler == &pxudp->pmhdl);
    LWIP_ASSERT1(fd == pxudp->sock);
    LWIP_UNUSED_ARG(fd);


    if (revents & ~(POLLIN|POLLERR)) {
        DPRINTF(("%s: unexpected revents 0x%x\n", __func__, revents));
        return pxudp_schedule_delete(pxudp);
    }

    /*
     * XXX: AFAICS, there's no way to match the error with the
     * outgoing datagram that triggered it, since we do non-blocking
     * sends from lwip thread.
     */
    if (revents & POLLERR) {
        int sockerr = -1;
        socklen_t optlen = (socklen_t)sizeof(sockerr);
        int status;

        status = getsockopt(pxudp->sock, SOL_SOCKET,
                            SO_ERROR, (char *)&sockerr, &optlen);
        if (status < 0) {
            DPRINTF(("%s: sock %d: SO_ERROR failed with errno %d\n",
                     __func__, pxudp->sock, errno));
        }
        else {
            DPRINTF(("%s: sock %d: errno %d\n",
                     __func__, pxudp->sock, sockerr));
        }
    }

    if ((revents & POLLIN) == 0) {
        return POLLIN;
    }

    nread = recv(pxudp->sock, pollmgr_udpbuf, sizeof(pollmgr_udpbuf), 0);
    if (nread == SOCKET_ERROR) {
        perror(__func__);
        return POLLIN;
    }

    p = pbuf_alloc(PBUF_RAW, (u16_t)nread, PBUF_RAM);
    if (p == NULL) {
        DPRINTF(("%s: pbuf_alloc(%d) failed\n", __func__, (int)nread));
        return POLLIN;
    }

    error = pbuf_take(p, pollmgr_udpbuf, (u16_t)nread);
    if (error != ERR_OK) {
        DPRINTF(("%s: pbuf_take(%d) failed\n", __func__, (int)nread));
        pbuf_free(p);
        return POLLIN;
    }

    error = sys_mbox_trypost(&pxudp->inmbox, p);
    if (error != ERR_OK) {
        pbuf_free(p);
        return POLLIN;
    }

    proxy_lwip_post(&pxudp->msg_inbound);

    return POLLIN;
}


/**
 * Callback from poll manager to trigger sending to guest.
 */
static void
pxudp_pcb_write_inbound(void *ctx)
{
    struct pxudp *pxudp = (struct pxudp *)ctx;
    LWIP_ASSERT1(pxudp != NULL);

    if (pxudp->pcb == NULL) {
        return;
    }

    pxudp_pcb_forward_inbound(pxudp);
}


static void
pxudp_pcb_forward_inbound(struct pxudp *pxudp)
{
    struct pbuf *p;
    u32_t timo;
    err_t error;

    if (!sys_mbox_valid(&pxudp->inmbox)) {
        return;
    }

    timo = sys_mbox_tryfetch(&pxudp->inmbox, (void **)&p);
    if (timo == SYS_MBOX_EMPTY) {
        return;
    }

    error = udp_send(pxudp->pcb, p);
    if (error != ERR_OK) {
        DPRINTF(("%s: udp_send(pcb %p) err %d\n",
                 __func__, (void *)pxudp, error));
    }

    pbuf_free(p);

    /*
     * If we enabled counting in pxudp_pcb_forward_outbound() check
     * that we have (all) the reply(s).
     */
    if (pxudp->count > 0) {
        --pxudp->count;
        if (pxudp->count == 0) {
            pxudp_pcb_expired(pxudp);
        }
    }
}
