/*
 * Copyright (c) 1995 Danny Gasparovski.
 * Portions copyright (c) 2000 Kelly Price.
 *
 * Please read the file COPYRIGHT for the
 * terms and conditions of the copyright.
 */

#include <slirp.h>
#include <iprt/string.h>
#include <iprt/stream.h>

#ifdef DEBUG
void dump_packet(void *, int);
#endif

#define IP4_ADDR_PRINTF_DECOMP(ip) ((ip) >> 24), ((ip) >> 16) & 0xff, ((ip) >> 8) & 0xff, (ip) & 0xff
#define IP4_ADDR_PRINTF_FORMAT "%u.%u.%u.%u"
/*
 * Dump a packet in the same format as tcpdump -x
 */
#ifdef DEBUG
void
dump_packet(void *dat, int n)
{
    Log(("nat: PACKET DUMPED:\n%.*Vhxd\n", n, dat));
}
#endif

#ifdef LOG_ENABLED
static void
lprint(const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    RTLogPrintfV(pszFormat, args);
    va_end(args);
}

void
ipstats(PNATState pData)
{
    lprint(" \n");

    lprint("IP stats:\n");
    lprint("  %6d total packets received (%d were unaligned)\n",
           ipstat.ips_total, ipstat.ips_unaligned);
    lprint("  %6d with incorrect version\n", ipstat.ips_badvers);
    lprint("  %6d with bad header checksum\n", ipstat.ips_badsum);
    lprint("  %6d with length too short (len < sizeof(iphdr))\n", ipstat.ips_tooshort);
    lprint("  %6d with length too small (len < ip->len)\n", ipstat.ips_toosmall);
    lprint("  %6d with bad header length\n", ipstat.ips_badhlen);
    lprint("  %6d with bad packet length\n", ipstat.ips_badlen);
    lprint("  %6d fragments received\n", ipstat.ips_fragments);
    lprint("  %6d fragments dropped\n", ipstat.ips_fragdropped);
    lprint("  %6d fragments timed out\n", ipstat.ips_fragtimeout);
    lprint("  %6d packets reassembled ok\n", ipstat.ips_reassembled);
    lprint("  %6d outgoing packets fragmented\n", ipstat.ips_fragmented);
    lprint("  %6d total outgoing fragments\n", ipstat.ips_ofragments);
    lprint("  %6d with bad protocol field\n", ipstat.ips_noproto);
    lprint("  %6d total packets delivered\n", ipstat.ips_delivered);
}

void
tcpstats(PNATState pData)
{
    lprint(" \n");

    lprint("TCP stats:\n");

    lprint("  %6d packets sent\n", tcpstat.tcps_sndtotal);
    lprint("          %6d data packets (%d bytes)\n",
            tcpstat.tcps_sndpack, tcpstat.tcps_sndbyte);
    lprint("          %6d data packets retransmitted (%d bytes)\n",
            tcpstat.tcps_sndrexmitpack, tcpstat.tcps_sndrexmitbyte);
    lprint("          %6d ack-only packets (%d delayed)\n",
            tcpstat.tcps_sndacks, tcpstat.tcps_delack);
    lprint("          %6d URG only packets\n", tcpstat.tcps_sndurg);
    lprint("          %6d window probe packets\n", tcpstat.tcps_sndprobe);
    lprint("          %6d window update packets\n", tcpstat.tcps_sndwinup);
    lprint("          %6d control (SYN/FIN/RST) packets\n", tcpstat.tcps_sndctrl);
    lprint("          %6d times tcp_output did nothing\n", tcpstat.tcps_didnuttin);

    lprint("  %6d packets received\n", tcpstat.tcps_rcvtotal);
    lprint("          %6d acks (for %d bytes)\n",
            tcpstat.tcps_rcvackpack, tcpstat.tcps_rcvackbyte);
    lprint("          %6d duplicate acks\n", tcpstat.tcps_rcvdupack);
    lprint("          %6d acks for unsent data\n", tcpstat.tcps_rcvacktoomuch);
    lprint("          %6d packets received in sequence (%d bytes)\n",
            tcpstat.tcps_rcvpack, tcpstat.tcps_rcvbyte);
    lprint("          %6d completely duplicate packets (%d bytes)\n",
            tcpstat.tcps_rcvduppack, tcpstat.tcps_rcvdupbyte);

    lprint("          %6d packets with some duplicate data (%d bytes duped)\n",
            tcpstat.tcps_rcvpartduppack, tcpstat.tcps_rcvpartdupbyte);
    lprint("          %6d out-of-order packets (%d bytes)\n",
            tcpstat.tcps_rcvoopack, tcpstat.tcps_rcvoobyte);
    lprint("          %6d packets of data after window (%d bytes)\n",
            tcpstat.tcps_rcvpackafterwin, tcpstat.tcps_rcvbyteafterwin);
    lprint("          %6d window probes\n", tcpstat.tcps_rcvwinprobe);
    lprint("          %6d window update packets\n", tcpstat.tcps_rcvwinupd);
    lprint("          %6d packets received after close\n", tcpstat.tcps_rcvafterclose);
    lprint("          %6d discarded for bad checksums\n", tcpstat.tcps_rcvbadsum);
    lprint("          %6d discarded for bad header offset fields\n",
            tcpstat.tcps_rcvbadoff);

    lprint("  %6d connection requests\n", tcpstat.tcps_connattempt);
    lprint("  %6d connection accepts\n", tcpstat.tcps_accepts);
    lprint("  %6d connections established (including accepts)\n", tcpstat.tcps_connects);
    lprint("  %6d connections closed (including %d drop)\n",
            tcpstat.tcps_closed, tcpstat.tcps_drops);
    lprint("  %6d embryonic connections dropped\n", tcpstat.tcps_conndrops);
    lprint("  %6d segments we tried to get rtt (%d succeeded)\n",
            tcpstat.tcps_segstimed, tcpstat.tcps_rttupdated);
    lprint("  %6d retransmit timeouts\n", tcpstat.tcps_rexmttimeo);
    lprint("          %6d connections dropped by rxmt timeout\n",
            tcpstat.tcps_timeoutdrop);
    lprint("  %6d persist timeouts\n", tcpstat.tcps_persisttimeo);
    lprint("  %6d keepalive timeouts\n", tcpstat.tcps_keeptimeo);
    lprint("          %6d keepalive probes sent\n", tcpstat.tcps_keepprobe);
    lprint("          %6d connections dropped by keepalive\n", tcpstat.tcps_keepdrops);
    lprint("  %6d correct ACK header predictions\n", tcpstat.tcps_predack);
    lprint("  %6d correct data packet header predictions\n", tcpstat.tcps_preddat);
    lprint("  %6d TCP cache misses\n", tcpstat.tcps_socachemiss);

/*  lprint("    Packets received too short:         %d\n", tcpstat.tcps_rcvshort); */
/*  lprint("    Segments dropped due to PAWS:       %d\n", tcpstat.tcps_pawsdrop); */

}

void
udpstats(PNATState pData)
{
    lprint(" \n");

    lprint("UDP stats:\n");
    lprint("  %6d datagrams received\n", udpstat.udps_ipackets);
    lprint("  %6d with packets shorter than header\n", udpstat.udps_hdrops);
    lprint("  %6d with bad checksums\n", udpstat.udps_badsum);
    lprint("  %6d with data length larger than packet\n", udpstat.udps_badlen);
    lprint("  %6d UDP socket cache misses\n", udpstat.udpps_pcbcachemiss);
    lprint("  %6d datagrams sent\n", udpstat.udps_opackets);
}

void
icmpstats(PNATState pData)
{
    lprint(" \n");
    lprint("ICMP stats:\n");
    lprint("  %6d ICMP packets received\n", icmpstat.icps_received);
    lprint("  %6d were too short\n", icmpstat.icps_tooshort);
    lprint("  %6d with bad checksums\n", icmpstat.icps_checksum);
    lprint("  %6d with type not supported\n", icmpstat.icps_notsupp);
    lprint("  %6d with bad type feilds\n", icmpstat.icps_badtype);
    lprint("  %6d ICMP packets sent in reply\n", icmpstat.icps_reflect);
}

void
mbufstats(PNATState pData)
{
    struct mbuf *m;
    int i;

    lprint(" \n");

    lprint("Mbuf stats:\n");

    lprint("  %6d mbufs allocated (%d max)\n", mbuf_alloced, mbuf_max);

    i = 0;
    for (m = m_freelist.m_next; m != &m_freelist; m = m->m_next)
        i++;
    lprint("  %6d mbufs on free list\n",  i);

    i = 0;
    for (m = m_usedlist.m_next; m != &m_usedlist; m = m->m_next)
        i++;
    lprint("  %6d mbufs on used list\n",  i);
    lprint("  %6d mbufs queued as packets\n\n", if_queued);
}

void
sockstats(PNATState pData)
{
    char buff[256];
    int n;
    struct socket *so, *so_next;

    lprint(" \n");

    lprint(
           "Proto[state]     Sock     Local Address, Port  Remote Address, Port RecvQ SendQ\n");

    QSOCKET_FOREACH(so, so_next, tcp)
    /* { */
        n = sprintf(buff, "tcp[%s]", so->so_tcpcb?tcpstates[so->so_tcpcb->t_state]:"NONE");
        while (n < 17)
            buff[n++] = ' ';
        buff[17] = 0;
        lprint("%s %3d   %15s %5d ",
               buff, so->s, inet_ntoa(so->so_laddr), ntohs(so->so_lport));
        lprint("%15s %5d %5d %5d\n",
                inet_ntoa(so->so_faddr), ntohs(so->so_fport),
                so->so_rcv.sb_cc, so->so_snd.sb_cc);
    LOOP_LABEL(tcp, so, so_next);
    }

    QSOCKET_FOREACH(so, so_next, udp)
    /* { */
        n = sprintf(buff, "udp[%d sec]", (so->so_expire - curtime) / 1000);
        while (n < 17)
            buff[n++] = ' ';
        buff[17] = 0;
        lprint("%s %3d  %15s %5d  ",
                buff, so->s, inet_ntoa(so->so_laddr), ntohs(so->so_lport));
        lprint("%15s %5d %5d %5d\n",
                inet_ntoa(so->so_faddr), ntohs(so->so_fport),
                so->so_rcv.sb_cc, so->so_snd.sb_cc);
    LOOP_LABEL(udp, so, so_next);
    }
}
#endif

static DECLCALLBACK(size_t)
print_ipv4_address(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                   const char *pszType, void const *pvValue,
                   int cchWidth, int cchPrecision, unsigned fFlags,
                   void *pvUser)
{
    uint32_t ip;

    AssertReturn(strcmp(pszType, "IP4") == 0, 0);

    ip = ntohl(*(uint32_t*)pvValue);
    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, IP4_ADDR_PRINTF_FORMAT,
           IP4_ADDR_PRINTF_DECOMP(ip));
}

static DECLCALLBACK(size_t)
print_socket(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
             const char *pszType, void const *pvValue,
             int cchWidth, int cchPrecision, unsigned fFlags,
             void *pvUser)
{
    struct socket *so = (struct socket*)pvValue;
    uint32_t ip;
    struct sockaddr addr;
    struct sockaddr_in *in_addr;
    socklen_t socklen = sizeof(struct sockaddr);
    int status = 0;

    AssertReturn(strcmp(pszType, "natsock") == 0, 0);
    if (so == NULL) 
        return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, 
                "socket is null");
    if (so->so_state == SS_NOFDREF || so->s == -1) 
        return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, 
                "socket(%d) SS_NODREF",so->s);
    status = getsockname(so->s, &addr, &socklen);

    if(status != 0 || addr.sa_family != AF_INET)
    {
        return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, 
                "socket(%d) is invalid(probably closed)",so->s);
    }

    in_addr = (struct sockaddr_in *)&addr;
    ip = ntohl(so->so_faddr.s_addr);
    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "socket %4d:(proto:%u) "
            "state=%04x ip=" IP4_ADDR_PRINTF_FORMAT ":%d "
            "name=" IP4_ADDR_PRINTF_FORMAT ":%d",
            so->s, so->so_type, so->so_state, IP4_ADDR_PRINTF_DECOMP(ip), 
            ntohs(so->so_fport), 
            IP4_ADDR_PRINTF_DECOMP(ntohl(in_addr->sin_addr.s_addr)),
            ntohs(in_addr->sin_port));
}

static DECLCALLBACK(size_t)
print_networkevents(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                    const char *pszType, void const *pvValue,
                    int cchWidth, int cchPrecision, unsigned fFlags,
                    void *pvUser)
{
    size_t cb = 0;
#ifdef RT_OS_WINDOWS
    WSANETWORKEVENTS *pNetworkEvents = (WSANETWORKEVENTS*)pvValue;
    bool fDelim = false;

    AssertReturn(strcmp(pszType, "natwinnetevents") == 0, 0);

    cb += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "events=%02x (", 
            pNetworkEvents->lNetworkEvents);
# define DO_BIT(bit) \
    if (pNetworkEvents->lNetworkEvents & FD_ ## bit)                        \
    {                                                                       \
        cb += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,                  \
              "%s" #bit "(%d)", fDelim ? "," : "",                          \
              pNetworkEvents->iErrorCode[FD_ ## bit ## _BIT]);              \
        fDelim = true;                                                      \
    }
    DO_BIT(READ);
    DO_BIT(WRITE);
    DO_BIT(OOB);
    DO_BIT(ACCEPT);
    DO_BIT(CONNECT);
    DO_BIT(CLOSE);
    DO_BIT(QOS);
# undef DO_BIT
    cb += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, ")");
#endif
    return cb;
}

#if 0
/*
 * Debugging
 */
int errno_func(const char *file, int line)
{
    int err = WSAGetLastError();
    LogRel(("errno=%d (%s:%d)\n", err, file, line));
    return err;
}
#endif

int
debug_init()
{
    int rc = VINF_SUCCESS;

    static int g_fFormatRegistered;

    if (!g_fFormatRegistered)
    {
        /*
         * XXX(r - frank): Move this to IPRT using RTNETADDRIPV4. 
         * Use the specifier %RNAipv4.
         */
        rc = RTStrFormatTypeRegister("IP4", print_ipv4_address, NULL);
        AssertRC(rc);
        rc = RTStrFormatTypeRegister("natsock", print_socket, NULL);
        AssertRC(rc);
        rc = RTStrFormatTypeRegister("natwinnetevents", 
            print_networkevents, NULL);
        AssertRC(rc);
        g_fFormatRegistered = 1;
    }

    return rc;
}
