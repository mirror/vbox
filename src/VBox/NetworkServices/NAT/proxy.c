/* -*- indent-tabs-mode: nil; -*- */
#include "winutils.h"

#include "proxy.h"
#include "proxy_pollmgr.h"
#include "portfwd.h"

#include "lwip/opt.h"

#include "lwip/sys.h"
#include "lwip/tcpip.h"

#ifndef RT_OS_WINDOWS
#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <iprt/string.h>
#include <unistd.h>
#include <err.h>
#else
# include <iprt/string.h>
#endif

#if defined(SOCK_NONBLOCK) && defined(RT_OS_NETBSD) /* XXX: PR kern/47569 */
# undef SOCK_NONBLOCK
#endif

#ifndef __arraycount
# define __arraycount(a) (sizeof(a)/sizeof(a[0]))
#endif

static SOCKET proxy_create_socket(int, int);

volatile struct proxy_options *g_proxy_options;
static sys_thread_t pollmgr_tid;

/* XXX: for mapping loopbacks to addresses in our network (ip4) */
struct netif *g_proxy_netif;
/*
 * Called on the lwip thread (aka tcpip thread) from tcpip_init() via
 * its "tcpip_init_done" callback.  Raw API is ok to use here
 * (e.g. rtadvd), but netconn API is not.
 */
void
proxy_init(struct netif *proxy_netif, struct proxy_options *opts)
{
    int status;

    LWIP_ASSERT1(opts != NULL);
    LWIP_UNUSED_ARG(proxy_netif);

    g_proxy_options = opts;
    g_proxy_netif = proxy_netif;

#if 1
    proxy_rtadvd_start(proxy_netif);
#endif

    /*
     * XXX: We use stateless DHCPv6 only to report IPv6 address(es) of
     * nameserver(s).  Since we don't yet support IPv6 addresses in
     * HostDnsService, there's no point in running DHCPv6.
     */
#if 0
    dhcp6ds_init(proxy_netif);
#endif

    if (opts->tftp_root != NULL) {
        tftpd_init(proxy_netif, opts->tftp_root);
    }

    status = pollmgr_init();
    if (status < 0) {
        errx(EXIT_FAILURE, "failed to initialize poll manager");
        /* NOTREACHED */
    }

    pxtcp_init();
    pxudp_init();

    portfwd_init();

    pxdns_init(proxy_netif);

    pollmgr_tid = sys_thread_new("pollmgr_thread",
                                 pollmgr_thread, NULL,
                                 DEFAULT_THREAD_STACKSIZE,
                                 DEFAULT_THREAD_PRIO);
    if (!pollmgr_tid) {
        errx(EXIT_FAILURE, "failed to create poll manager thread");
        /* NOTREACHED */
    }
}


/**
 * Send static callback message from poll manager thread to lwip
 * thread, scheduling a function call in lwip thread context.
 *
 * XXX: Existing lwip api only provides non-blocking version for this.
 * It may fail when lwip thread is not running (mbox invalid) or if
 * post failed (mbox full).  How to handle these?
 */
void
proxy_lwip_post(struct tcpip_msg *msg)
{
    struct tcpip_callback_msg *m;
    err_t error;

    LWIP_ASSERT1(msg != NULL);

    /*
     * lwip plays games with fake incomplete struct tag to enforce API
     */
    m = (struct tcpip_callback_msg *)msg;
    error = tcpip_callbackmsg(m);

    if (error == ERR_VAL) {
        /* XXX: lwip thread is not running (mbox invalid) */
        LWIP_ASSERT1(error != ERR_VAL);
    }

    LWIP_ASSERT1(error == ERR_OK);
}


/**
 * Create a non-blocking socket.  Disable SIGPIPE for TCP sockets if
 * possible.  On Linux it's not possible and should be disabled for
 * each send(2) individually.
 */
static SOCKET
proxy_create_socket(int sdom, int stype)
{
    SOCKET s;
    int stype_and_flags;
    int status;

    LWIP_UNUSED_ARG(status);    /* depends on ifdefs */


    stype_and_flags = stype;

#if defined(SOCK_NONBLOCK)
    stype_and_flags |= SOCK_NONBLOCK;
#endif

    /*
     * Disable SIGPIPE on disconnected socket.  It might be easier to
     * forgo it and just use MSG_NOSIGNAL on each send*(2), since we
     * have to do it for Linux anyway, but Darwin does NOT have that
     * flag (but has SO_NOSIGPIPE socket option).
     */
#if !defined(SOCK_NOSIGPIPE) && !defined(SO_NOSIGPIPE) && !defined(MSG_NOSIGNAL)
#if 0 /* XXX: Solaris has neither, the program should ignore SIGPIPE globally */
#error Need a way to disable SIGPIPE on connection oriented sockets!
#endif
#endif

#if defined(SOCK_NOSIGPIPE)
    if (stype == SOCK_STREAM) {
        stype_and_flags |= SOCK_NOSIGPIPE;
    }
#endif

    s = socket(sdom, stype_and_flags, 0);
    if (s == INVALID_SOCKET) {
        perror("socket");
        return INVALID_SOCKET;
    }

#if !defined(SOCK_NONBLOCK) && !defined(RT_OS_WINDOWS)
    {
        int sflags;

        status = fcntl(s, F_GETFL, &sflags);
        if (status < 0) {
            perror("F_GETFL");
            closesocket(s);
            return INVALID_SOCKET;
        }

        status = fcntl(s, F_SETFL, sflags | O_NONBLOCK);
        if (status < 0) {
            perror("O_NONBLOCK");
            closesocket(s);
            return INVALID_SOCKET;
        }
    }
#endif

#if !defined(SOCK_NOSIGPIPE) && defined(SO_NOSIGPIPE)
    if (stype == SOCK_STREAM) {
        int on = 1;
        const socklen_t onlen = sizeof(on);

        status = setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &on, onlen);
        if (status < 0) {
            perror("SO_NOSIGPIPE");
            closesocket(s);
            return INVALID_SOCKET;
        }
    }
#endif

#if defined(RT_OS_WINDOWS)
    {
        u_long mode = 0;
        status = ioctlsocket(s, FIONBIO, &mode);
        if (status == SOCKET_ERROR) {
            warn("ioctl error: %d\n", WSAGetLastError());
            return INVALID_SOCKET;
        }
    }
#endif

    return s;
}


/**
 * Create a socket for outbound connection to dst_addr:dst_port.
 *
 * The socket is non-blocking and TCP sockets has SIGPIPE disabled if
 * possible.  On Linux it's not possible and should be disabled for
 * each send(2) individually.
 */
SOCKET
proxy_connected_socket(int sdom, int stype,
                       ipX_addr_t *dst_addr, u16_t dst_port)
{
    struct sockaddr_in6 dst_sin6;
    struct sockaddr_in dst_sin;
    struct sockaddr *pdst_sa;
    socklen_t dst_sa_len;
    void *pdst_addr;
    const struct sockaddr *psrc_sa;
    socklen_t src_sa_len;
    int status;
    SOCKET s;

    LWIP_ASSERT1(sdom == PF_INET || sdom == PF_INET6);
    LWIP_ASSERT1(stype == SOCK_STREAM || stype == SOCK_DGRAM);

    if (sdom == PF_INET6) {
        pdst_sa = (struct sockaddr *)&dst_sin6;
        pdst_addr = (void *)&dst_sin6.sin6_addr;

        memset(&dst_sin6, 0, sizeof(dst_sin6));
#if HAVE_SA_LEN
        dst_sin6.sin6_len =
#endif
            dst_sa_len = sizeof(dst_sin6);
        dst_sin6.sin6_family = AF_INET6;
        memcpy(&dst_sin6.sin6_addr, &dst_addr->ip6, sizeof(ip6_addr_t));
        dst_sin6.sin6_port = htons(dst_port);
    }
    else { /* sdom = PF_INET */
        pdst_sa = (struct sockaddr *)&dst_sin;
        pdst_addr = (void *)&dst_sin.sin_addr;

        memset(&dst_sin, 0, sizeof(dst_sin));
#if HAVE_SA_LEN
        dst_sin.sin_len =
#endif
            dst_sa_len = sizeof(dst_sin);
        dst_sin.sin_family = AF_INET;
        dst_sin.sin_addr.s_addr = dst_addr->ip4.addr; /* byte-order? */
        dst_sin.sin_port = htons(dst_port);
    }

#if LWIP_PROXY_DEBUG && !RT_OS_WINDOWS
    {
        char addrbuf[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"];
        const char *addrstr;

        addrstr = inet_ntop(sdom, pdst_addr, addrbuf, sizeof(addrbuf));
        DPRINTF(("---> %s %s%s%s:%d ",
                 stype == SOCK_STREAM ? "TCP" : "UDP",
                 sdom == PF_INET6 ? "[" : "",
                 addrstr,
                 sdom == PF_INET6 ? "]" : "",
                 dst_port));
    }
#endif

    s = proxy_create_socket(sdom, stype);
    if (s == INVALID_SOCKET) {
        return INVALID_SOCKET;
    }
    DPRINTF(("socket %d\n", s));

    /* TODO: needs locking if dynamic modifyvm is allowed */
    if (sdom == PF_INET6) {
        psrc_sa = (const struct sockaddr *)g_proxy_options->src6;
        src_sa_len = sizeof(struct sockaddr_in6);
    }
    else {
        psrc_sa = (const struct sockaddr *)g_proxy_options->src4;
        src_sa_len = sizeof(struct sockaddr_in);
    }
    if (psrc_sa != NULL) {
        status = bind(s, psrc_sa, src_sa_len);
        if (status == SOCKET_ERROR) {
            DPRINTF(("socket %d: bind: %s\n", s, strerror(errno)));
            closesocket(s);
            return INVALID_SOCKET;
        }
    }

    status = connect(s, pdst_sa, dst_sa_len);
    if (status == SOCKET_ERROR && errno != EINPROGRESS) {
        DPRINTF(("socket %d: connect: %s\n", s, strerror(errno)));
        closesocket(s);
        return INVALID_SOCKET;
    }

    return s;
}


/**
 * Create a socket for inbound (port-forwarded) connections to
 * src_addr (port is part of sockaddr, so not a separate argument).
 *
 * The socket is non-blocking and TCP sockets has SIGPIPE disabled if
 * possible.  On Linux it's not possible and should be disabled for
 * each send(2) individually.
 *
 * TODO?: Support v6-mapped v4 so that user can specify she wants
 * "udp" and get both versions?
 */
SOCKET
proxy_bound_socket(int sdom, int stype, struct sockaddr *src_addr)
{
    SOCKET s;
    int on;
    const socklen_t onlen = sizeof(on);
    int status;

    s = proxy_create_socket(sdom, stype);
    if (s == INVALID_SOCKET) {
        return INVALID_SOCKET;
    }
    DPRINTF(("socket %d\n", s));

    on = 1;
    status = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&on, onlen);
    if (status < 0) {           /* not good, but not fatal */
        warn("SO_REUSEADDR");
    }

    status = bind(s, src_addr,
                  sdom == PF_INET ?
                    sizeof(struct sockaddr_in)
                  : sizeof(struct sockaddr_in6));
    if (status < 0) {
        perror("bind");
        closesocket(s);
        return INVALID_SOCKET;
    }

    if (stype == SOCK_STREAM) {
        status = listen(s, 5);
        if (status < 0) {
            perror("listen");
            closesocket(s);
            return INVALID_SOCKET;
        }
    }

    return s;
}


void
proxy_reset_socket(SOCKET s)
{
    struct linger linger;

    linger.l_onoff = 1;
    linger.l_linger = 0;

    /* On Windows we can run into issue here, perhaps SO_LINGER isn't enough, and
     * we should use WSA{Send,Recv}Disconnect instead.
     *
     * Links for the reference:
     * http://msdn.microsoft.com/en-us/library/windows/desktop/ms738547%28v=vs.85%29.aspx
     * http://bugs.sun.com/bugdatabase/view_bug.do?bug_id=4468997
     */
    setsockopt(s, SOL_SOCKET, SO_LINGER, (char *)&linger, sizeof(linger));
    
    closesocket(s);
}


void
proxy_sendto(SOCKET sock, struct pbuf *p, void *name, size_t namelen)
{
    struct pbuf *q;
    size_t i, clen;
#ifndef RT_OS_WINDOWS
    struct msghdr mh;
#else
    int rc;
#endif
    IOVEC fixiov[8];     /* fixed size (typical case) */
    const size_t fixiovsize = sizeof(fixiov)/sizeof(fixiov[0]);
    IOVEC *dyniov;       /* dynamically sized */
    IOVEC *iov;
    ssize_t nsent;

    /*
     * Static iov[] is usually enough since UDP protocols use small
     * datagrams to avoid fragmentation, but be prepared.
     */
    clen = pbuf_clen(p);
    if (clen > fixiovsize) {
        /*
         * XXX: TODO: check that clen is shorter than IOV_MAX
         */
        dyniov = (IOVEC *)malloc(clen * sizeof(*dyniov));
        if (dyniov == NULL) {
            goto out;
        }
        iov = dyniov;
    }
    else {
        dyniov = NULL;
        iov = fixiov;
    }


    for (q = p, i = 0; i < clen; q = q->next, ++i) {
        LWIP_ASSERT1(q != NULL);

        IOVEC_SET_BASE(iov[i], q->payload);
        IOVEC_SET_LEN(iov[i], q->len);
    }

#ifndef RT_OS_WINDOWS
    memset(&mh, 0, sizeof(mh));
    mh.msg_name = name;
    mh.msg_namelen = namelen;
    mh.msg_iov = iov;
    mh.msg_iovlen = clen;

    nsent = sendmsg(sock, &mh, 0);
    if (nsent < 0) {
        DPRINTF(("%s: fd %d: sendmsg errno %d\n",
                 __func__, sock, errno));
    }
#else
    rc = WSASendTo(sock, iov, (DWORD)clen, (DWORD *)&nsent, 0, name, (int)namelen, NULL, NULL);
    if (rc == SOCKET_ERROR) {
         DPRINTF(("%s: fd %d: sendmsg errno %d\n",
                  __func__, sock, WSAGetLastError()));
    }
#endif

  out:
    if (dyniov != NULL) {
        free(dyniov);
    }
    pbuf_free(p);
}


static const char *lwiperr[] = {
    "ERR_OK",
    "ERR_MEM",
    "ERR_BUF",
    "ERR_TIMEOUT",
    "ERR_RTE",
    "ERR_INPROGRESS",
    "ERR_VAL",
    "ERR_WOULDBLOCK",
    "ERR_USE",
    "ERR_ISCONN",
    "ERR_ABRT",
    "ERR_RST",
    "ERR_CLSD",
    "ERR_CONN",
    "ERR_ARG",
    "ERR_IF"
};


const char *
proxy_lwip_strerr(err_t error)
{
    static char buf[32];
    int e = -error;

    if (0 < e || e < (int)__arraycount(lwiperr)) {
        return lwiperr[e];
    }
    else {
        RTStrPrintf(buf, sizeof(buf), "unknown error %d", error);
        return buf;
    }
}
