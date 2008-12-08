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
    struct socket *so;

    lprint(" \n");

    lprint(
           "Proto[state]     Sock     Local Address, Port  Remote Address, Port RecvQ SendQ\n");

    for (so = tcb.so_next; so != &tcb; so = so->so_next)
    {
        n = sprintf(buff, "tcp[%s]", so->so_tcpcb?tcpstates[so->so_tcpcb->t_state]:"NONE");
        while (n < 17)
            buff[n++] = ' ';
        buff[17] = 0;
        lprint("%s %3d   %15s %5d ",
               buff, so->s, inet_ntoa(so->so_laddr), ntohs(so->so_lport));
        lprint("%15s %5d %5d %5d\n",
                inet_ntoa(so->so_faddr), ntohs(so->so_fport),
                so->so_rcv.sb_cc, so->so_snd.sb_cc);
    }

    for (so = udb.so_next; so != &udb; so = so->so_next)
    {
        n = sprintf(buff, "udp[%d sec]", (so->so_expire - curtime) / 1000);
        while (n < 17)
            buff[n++] = ' ';
        buff[17] = 0;
        lprint("%s %3d  %15s %5d  ",
                buff, so->s, inet_ntoa(so->so_laddr), ntohs(so->so_lport));
        lprint("%15s %5d %5d %5d\n",
                inet_ntoa(so->so_faddr), ntohs(so->so_fport),
                so->so_rcv.sb_cc, so->so_snd.sb_cc);
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
    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%u.%u.%u.%u",
           (ip >> 24), (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
}

int 
debug_init() 
{
    int rc = VINF_SUCCESS;

    static int g_fFormatRegistered;

    if (!g_fFormatRegistered)
    {
        /*
         * XXX Move this to IPRT using RTNETADDRIPV4. Use the specifier %RNAipv4.
         */
        rc = RTStrFormatTypeRegister("IP4", print_ipv4_address, NULL);
        AssertRC(rc);
    }

    return rc;
}
