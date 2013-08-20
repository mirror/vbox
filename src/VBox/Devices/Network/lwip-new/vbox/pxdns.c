/* -*- indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2009-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*
 * Copyright (c) 2003,2004,2005 Armin Wolfermann
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include "proxytest.h"
#include "proxy_pollmgr.h"

#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"

#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <string.h>


struct request;


/**
 * DNS Proxy
 */
struct pxdns {
    struct pollmgr_handler pmhdl;

    struct udp_pcb *pcb4;       /* lwIP doesn't support listening for */
    struct udp_pcb *pcb6;       /*  both IPv4 and IPv6 on single pcb. */

    SOCKET sock;

    u16_t id;

    /* XXX: TODO: support multiple, support IPv6 */
    struct sockaddr_in resolver_sin;
    socklen_t resolver_sinlen;

    sys_mutex_t lock;

    size_t active_queries;
    size_t expired_queries;
    size_t late_answers;
    size_t hash_collisions;

#define TIMEOUT 5
    size_t timeout_slot;
    struct request *timeout_list[TIMEOUT];

#define HASHSIZE 10
#define HASH(id) ((id) & ((1 << HASHSIZE) - 1))
    struct request *request_hash[1 << HASHSIZE];
} g_pxdns;


struct request {
    /**
     * Request ID that we use in relayed request.
     */
    u16_t id;

    /**
     * XXX: TODO: to test rexmit code, rexmit to the same resolver
     * multiple times; to be replaced with trying the next resolver.
     */
    size_t rexmit_count;

    /**
     * PCB from which we have received this request.  lwIP doesn't
     * support listening for both IPv4 and IPv6 on the same pcb, so we
     * use two and need to keep track.
     */
    struct udp_pcb *pcb;

    /**
     * Client this request is from and its original request ID.
     */
    ipX_addr_t client_addr;
    u16_t client_port;
    u16_t client_id;

    /**
     * Chaining for pxdns::request_hash
     */
    struct request **pprev_hash;
    struct request *next_hash;

    /**
     * Chaining for pxdns::timeout_list
     */
    struct request **pprev_timeout;
    struct request *next_timeout;

    /**
     * Pbuf with reply received on pollmgr thread.
     */
    struct pbuf *reply;

    /**
     * Preallocated lwIP message to send reply from the lwIP thread.
     */
    struct tcpip_msg msg_reply;

    /**
     * Client request.  ID is replaced with ours, original saved in
     * client_id.  Use a copy since we might need to resend and we
     * don't want to hold onto pbuf of the request.
     */
    size_t size;
    u8_t data[1];
};


static void pxdns_recv4(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                        ip_addr_t *addr, u16_t port);
static void pxdns_recv6(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                        ip6_addr_t *addr, u16_t port);
static void pxdns_query(struct pxdns *pxdns, struct udp_pcb *pcb, struct pbuf *p,
                        ipX_addr_t *addr, u16_t port);
static void pxdns_timer(void *arg);
static int pxdns_rexmit(struct pxdns *pxdns, struct request *req);
static int pxdns_forward_outbound(struct pxdns *pxdns, struct request *req);

static int pxdns_pmgr_pump(struct pollmgr_handler *handler, SOCKET fd, int revents);
static void pxdns_pcb_reply(void *ctx);

static void pxdns_request_register(struct pxdns *pxdns, struct request *req);
static void pxdns_request_deregister(struct pxdns *pxdns, struct request *req);
static struct request *pxdns_request_find(struct pxdns *pxdns, u16_t id);

static void pxdns_hash_add(struct pxdns *pxdns, struct request *req);
static void pxdns_hash_del(struct pxdns *pxdns, struct request *req);
static void pxdns_timeout_add(struct pxdns *pxdns, struct request *req);
static void pxdns_timeout_del(struct pxdns *pxdns, struct request *req);

static void pxdns_request_free(struct request *req);


err_t
pxdns_init(struct netif *proxy_netif)
{
    struct pxdns *pxdns = &g_pxdns;
    err_t error;

    LWIP_UNUSED_ARG(proxy_netif);

    pxdns->pmhdl.callback = pxdns_pmgr_pump;
    pxdns->pmhdl.data = (void *)pxdns;
    pxdns->pmhdl.slot = -1;

    pxdns->pcb4 = udp_new();
    if (pxdns->pcb4 == NULL) {
        error = ERR_MEM;
        goto err_cleanup_pcb;
    }

    pxdns->pcb6 = udp_new_ip6();
    if (pxdns->pcb6 == NULL) {
        error = ERR_MEM;
        goto err_cleanup_pcb;
    }

    error = udp_bind(pxdns->pcb4, IP_ADDR_ANY, 53);
    if (error != ERR_OK) {
        goto err_cleanup_pcb;
    }

    error = udp_bind_ip6(pxdns->pcb4, IP6_ADDR_ANY, 53);
    if (error != ERR_OK) {
        goto err_cleanup_pcb;
    }

    udp_recv(pxdns->pcb4, pxdns_recv4, pxdns);
    udp_recv_ip6(pxdns->pcb6, pxdns_recv6, pxdns);

    pxdns->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (pxdns->sock == INVALID_SOCKET) {
        goto err_cleanup_pcb;
    }

    /* XXX: TODO: support multiple, support IPv6 */
    pxdns->resolver_sin.sin_family = AF_INET;
    pxdns->resolver_sin.sin_addr.s_addr = PP_HTONL(0x7f000001); /* XXX */
    pxdns->resolver_sin.sin_port = PP_HTONS(53);
#if HAVE_SA_LEN
    pxdns->resolver_sin.sin_len =
#endif
        pxdns->resolver_sinlen = sizeof(pxdns->resolver_sin);

    sys_mutex_new(&pxdns->lock);

    pxdns->timeout_slot = 0;

    /* XXX: assumes pollmgr thread is not running yet */
    pollmgr_add(&pxdns->pmhdl, pxdns->sock, POLLIN);

    sys_timeout(1 * 1000, pxdns_timer, pxdns);

    return ERR_OK;

  err_cleanup_pcb:
    if (pxdns->pcb4 != NULL) {
        udp_remove(pxdns->pcb4);
        pxdns->pcb4 = NULL;
    }
    if (pxdns->pcb6 != NULL) {
        udp_remove(pxdns->pcb6);
        pxdns->pcb4 = NULL;
    }

    return error;
}


static void
pxdns_request_free(struct request *req)
{
    LWIP_ASSERT1(req->pprev_hash == NULL);
    LWIP_ASSERT1(req->pprev_timeout == NULL);

    if (req->reply != NULL) {
        pbuf_free(req->reply);
    }
    free(req);
}


static void
pxdns_hash_add(struct pxdns *pxdns, struct request *req)
{
    struct request **chain;

    LWIP_ASSERT1(req->pprev_hash == NULL);
    chain = &pxdns->request_hash[HASH(req->id)];
    if ((req->next_hash = *chain) != NULL) {
        (*chain)->pprev_hash = &req->next_hash;
        ++pxdns->hash_collisions;
    }
    *chain = req;
    req->pprev_hash = chain;
}


static void
pxdns_timeout_add(struct pxdns *pxdns, struct request *req)
{
    struct request **chain;

    LWIP_ASSERT1(req->pprev_timeout == NULL);
    chain = &pxdns->timeout_list[pxdns->timeout_slot];
    if ((req->next_timeout = *chain) != NULL) {
        (*chain)->pprev_timeout = &req->next_timeout;
    }
    *chain = req;
    req->pprev_timeout = chain;
}


static void
pxdns_hash_del(struct pxdns *pxdns, struct request *req)
{
    LWIP_ASSERT1(req->pprev_hash != NULL);
    --pxdns->active_queries;

    if (req->next_hash != NULL) {
        req->next_hash->pprev_hash = req->pprev_hash;
    }
    *req->pprev_hash = req->next_hash;
    req->pprev_hash = NULL;
    req->next_hash = NULL;
}


static void
pxdns_timeout_del(struct pxdns *pxdns, struct request *req)
{
    LWIP_ASSERT1(req->pprev_timeout != NULL);

    if (req->next_timeout != NULL) {
        req->next_timeout->pprev_timeout = req->pprev_timeout;
    }
    *req->pprev_timeout = req->next_timeout;
    req->pprev_timeout = NULL;
    req->next_timeout = NULL;
}



/**
 * Do bookkeeping on new request.  Called from pxdns_query().
 */
static void
pxdns_request_register(struct pxdns *pxdns, struct request *req)
{
    sys_mutex_lock(&pxdns->lock);

    pxdns_hash_add(pxdns, req);
    pxdns_timeout_add(pxdns, req);
    ++pxdns->active_queries;

    sys_mutex_unlock(&pxdns->lock);
}


static void
pxdns_request_deregister(struct pxdns *pxdns, struct request *req)
{
    sys_mutex_lock(&pxdns->lock);

    pxdns_hash_del(pxdns, req);
    pxdns_timeout_del(pxdns, req);
    --pxdns->active_queries;

    sys_mutex_unlock(&pxdns->lock);
}


/**
 * Find request by the id we used when relaying it and remove it from
 * id hash and timeout list.  Called from pxdns_pmgr_pump() when reply
 * comes.
 */
static struct request *
pxdns_request_find(struct pxdns *pxdns, u16_t id)
{
    struct request *req = NULL;

    sys_mutex_lock(&pxdns->lock);

    /* find request in the id->req hash */
    for (req = pxdns->request_hash[HASH(id)]; req != NULL; req = req->next_hash) {
        if (req->id == id) {
            break;
        }
    }

    if (req != NULL) {
        pxdns_hash_del(pxdns, req);
        pxdns_timeout_del(pxdns, req);
        --pxdns->active_queries;
    }

    sys_mutex_unlock(&pxdns->lock);
    return req;
}


/**
 * Retransmit of g/c expired requests and move timeout slot forward.
 */
static void
pxdns_timer(void *arg)
{
    struct pxdns *pxdns = (struct pxdns *)arg;
    struct request **chain, *req;

    sys_mutex_lock(&pxdns->lock);

    /*
     * Move timeout slot first.  New slot points to the list of
     * expired requests.  If any expired request is retransmitted, we
     * keep it on the list (that is now current), effectively
     * resetting the timeout.
     */
    LWIP_ASSERT1(pxdns->timeout_slot < TIMEOUT);
    if (++pxdns->timeout_slot == TIMEOUT) {
        pxdns->timeout_slot = 0;
    }

    chain = &pxdns->timeout_list[pxdns->timeout_slot]; 
    req = *chain;
    while (req != NULL) {
        struct request *expired = req;
        req = req->next_timeout;

        if (pxdns_rexmit(pxdns, expired)) {
            continue;
        }

        pxdns_hash_del(pxdns, expired);
        pxdns_timeout_del(pxdns, expired);
        ++pxdns->expired_queries;

        pxdns_request_free(expired);
    }

    sys_mutex_unlock(&pxdns->lock);

    sys_timeout(1 * 1000, pxdns_timer, pxdns);
}


static void
pxdns_recv4(void *arg, struct udp_pcb *pcb, struct pbuf *p,
            ip_addr_t *addr, u16_t port)
{
    struct pxdns *pxdns = (struct pxdns *)arg;
    pxdns_query(pxdns, pcb, p, ip_2_ipX(addr), port);
}

static void
pxdns_recv6(void *arg, struct udp_pcb *pcb, struct pbuf *p,
            ip6_addr_t *addr, u16_t port)
{
    struct pxdns *pxdns = (struct pxdns *)arg;
    pxdns_query(pxdns, pcb, p, ip6_2_ipX(addr), port);
}


static void
pxdns_query(struct pxdns *pxdns, struct udp_pcb *pcb, struct pbuf *p,
            ipX_addr_t *addr, u16_t port)
{
    struct request *req;
    u16_t client_id;
    int sent;

    req = calloc(1, sizeof(struct request) - 1 + p->tot_len);
    if (req == NULL) {
        return;
    }

    /* copy request data */
    req->size = p->tot_len;
    pbuf_copy_partial(p, req->data, p->tot_len, 0);

    /* save client identity and client's request id */
    req->pcb = pcb;
    ipX_addr_copy(PCB_ISIPV6(pcb), req->client_addr, *addr);
    req->client_port = port;
    memcpy(&req->client_id, req->data, sizeof(req->client_id));

    /* slap our request id onto it */
    req->id = pxdns->id++;
    memcpy(req->data, &req->id, sizeof(u16_t));

    /* XXX */
    req->rexmit_count = 1;

    req->msg_reply.type = TCPIP_MSG_CALLBACK_STATIC;
    req->msg_reply.sem = NULL;
    req->msg_reply.msg.cb.function = pxdns_pcb_reply;
    req->msg_reply.msg.cb.ctx = (void *)req;

    DPRINTF2(("%s: req=%p: client id %d -> id %d\n",
              __func__, (void *)req, req->client_id, req->id));

    pxdns_request_register(pxdns, req);

    sent = pxdns_forward_outbound(pxdns, req);
    while (!sent) {
        sent = pxdns_rexmit(pxdns, req);
    }
    if (!sent) {
        pxdns_request_deregister(pxdns, req);
        pxdns_request_free(req);
    }
}


static int
pxdns_forward_outbound(struct pxdns *pxdns, struct request *req)
{
    ssize_t nsent;

    DPRINTF2(("%s: req %p\n", __func__, (void *)req));

    nsent = sendto(pxdns->sock, req->data, req->size, 0,
                   (struct sockaddr *)&pxdns->resolver_sin,
                   pxdns->resolver_sinlen);

    if ((size_t)nsent == req->size) {
        return 1; /* sent */
    }

    if (nsent < 0) {
        perror("dnsproxy");
    }
    else if ((size_t)nsent != req->size) {
        DPRINTF(("%s: sent only %lu of %lu\n",
                 __func__, (unsigned long)nsent, (unsigned long)req->size));
    }
    return 0; /* not sent, caller will retry as necessary */
}


static int
pxdns_rexmit(struct pxdns *pxdns, struct request *req)
{
    DPRINTF2(("%s: req %p: rexmit count %lu\n",
              __func__, (void *)req, (unsigned long)req->rexmit_count));

    /* XXX: TODO: use the next resolver instead */
    if (req->rexmit_count == 0) {
        return 0;
    }
    --req->rexmit_count;

    return pxdns_forward_outbound(pxdns, req);
}


static int
pxdns_pmgr_pump(struct pollmgr_handler *handler, SOCKET fd, int revents)
{
    struct pxdns *pxdns;
    struct request *req;
    ssize_t nread;
    err_t error;
    u16_t id;

    pxdns = (struct pxdns *)handler->data;
    LWIP_ASSERT1(handler == &pxdns->pmhdl);
    LWIP_ASSERT1(fd = pxdns->sock);
    LWIP_UNUSED_ARG(fd);

    if (revents & ~(POLLIN|POLLERR)) {
        DPRINTF0(("%s: unexpected revents 0x%x\n", __func__, revents));
        return POLLIN;
    }

    if (revents & POLLERR) {
        int sockerr = -1;
        socklen_t optlen = (socklen_t)sizeof(sockerr);
        int status;

        status = getsockopt(pxdns->sock, SOL_SOCKET,
                            SO_ERROR, &sockerr, &optlen);
        if (status < 0) {
            DPRINTF(("%s: sock %d: SO_ERROR failed with errno %d\n",
                     __func__, pxdns->sock, errno));
        }
        else {
            DPRINTF(("%s: sock %d: errno %d\n",
                     __func__, pxdns->sock, sockerr));
        }
    }

    if ((revents & POLLIN) == 0) {
        return POLLIN;
    }


    nread = recv(pxdns->sock, pollmgr_udpbuf, sizeof(pollmgr_udpbuf), 0);
    if (nread < 0) {
        perror(__func__);
        return POLLIN;
    }

    /* check for minimum dns packet length */
    if (nread < 12) {
        DPRINTF2(("%s: short reply %lu bytes\n",
                  __func__, (unsigned long)nread));
        return POLLIN;
    }

    memcpy(&id, pollmgr_udpbuf, sizeof(id));
    req = pxdns_request_find(pxdns, id);
    if (req == NULL) {
        DPRINTF2(("%s: orphaned reply for %d\n", __func__, id));
        ++pxdns->late_answers;
        return POLLIN;
    }

    DPRINTF2(("%s: reply for req=%p: client id %d -> id %d\n",
              __func__, (void *)req, req->client_id, req->id));

    req->reply = pbuf_alloc(PBUF_RAW, nread, PBUF_RAM);
    if (req->reply == NULL) {
        DPRINTF(("%s: pbuf_alloc(%d) failed\n", __func__, (int)nread));
        pxdns_request_free(req);
        return POLLIN;
    }

    memcpy(pollmgr_udpbuf, &req->client_id, sizeof(req->client_id));
    error = pbuf_take(req->reply, pollmgr_udpbuf, nread);
    if (error != ERR_OK) {
        DPRINTF(("%s: pbuf_take(%d) failed\n", __func__, (int)nread));
        pxdns_request_free(req);
        return POLLIN;
    }

    proxy_lwip_post(&req->msg_reply);
    return POLLIN;
}


/**
 * Called on lwIP thread via request::msg_reply callback.
 */
static void
pxdns_pcb_reply(void *ctx)
{
    struct request *req = (struct request *)ctx;
    err_t error;

    error = udp_sendto(req->pcb, req->reply,
                       ipX_2_ip(&req->client_addr), req->client_port);
    if (error != ERR_OK) {
        DPRINTF(("%s: udp_sendto err %d\n",
                 __func__, proxy_lwip_strerr(error)));
    }

    pxdns_request_free(req);
}
