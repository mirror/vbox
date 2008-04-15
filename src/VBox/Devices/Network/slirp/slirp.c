#include "slirp.h"
#ifdef RT_OS_OS2
# include <paths.h>
#endif

#include <VBox/err.h>
#include <iprt/assert.h>

static const uint8_t special_ethaddr[6] = {
    0x52, 0x54, 0x00, 0x12, 0x35, 0x00
};

#ifdef _WIN32

static int get_dns_addr_domain(PNATState pData, bool fVerbose,
                               struct in_addr *pdns_addr,
                               const char **ppszDomain)
{
    int rc = 0;
    FIXED_INFO *FixedInfo=NULL;
    ULONG    BufLen;
    DWORD    ret;
    IP_ADDR_STRING *pIPAddr;
    struct in_addr tmp_addr;

    FixedInfo = (FIXED_INFO *)GlobalAlloc(GPTR, sizeof(FIXED_INFO));
    BufLen = sizeof(FIXED_INFO);

    /** @todo: this API returns all DNS servers, no matter whether the
     * corresponding network adapter is disabled or not. Maybe replace
     * this by GetAdapterAddresses(), which is XP/Vista only though. */
    if (ERROR_BUFFER_OVERFLOW == GetNetworkParams(FixedInfo, &BufLen)) {
        if (FixedInfo) {
            GlobalFree(FixedInfo);
            FixedInfo = NULL;
        }
        FixedInfo = GlobalAlloc(GPTR, BufLen);
    }

    if ((ret = GetNetworkParams(FixedInfo, &BufLen)) != ERROR_SUCCESS) {
        Log(("GetNetworkParams failed. ret = %08x\n", (u_int)ret ));
        if (FixedInfo) {
            GlobalFree(FixedInfo);
            FixedInfo = NULL;
        }
        rc = -1;
        goto get_dns_prefix;
    }

    pIPAddr = &(FixedInfo->DnsServerList);
    inet_aton(pIPAddr->IpAddress.String, &tmp_addr);
    Log(("nat: DNS Servers:\n"));
    if (fVerbose || pdns_addr->s_addr != tmp_addr.s_addr)
        LogRel(("NAT: DNS address: %s\n", pIPAddr->IpAddress.String));
    *pdns_addr = tmp_addr;

    pIPAddr = FixedInfo -> DnsServerList.Next;
    while ( pIPAddr )
    {
        if (fVerbose)
            LogRel(("NAT: ignored DNS address: %s\n", pIPAddr ->IpAddress.String));
        pIPAddr = pIPAddr ->Next;
    }
    if (FixedInfo) {
        GlobalFree(FixedInfo);
        FixedInfo = NULL;
    }

get_dns_prefix:
    if (ppszDomain)
    {
        OSVERSIONINFO ver;
        char szDnsDomain[256];
        DWORD dwSize = sizeof(szDnsDomain);

        *ppszDomain = NULL;
        GetVersionEx(&ver);
        if (ver.dwMajorVersion >= 5)
        {
            /* GetComputerNameEx exists in Windows versions starting with 2000. */
            if (GetComputerNameEx(ComputerNameDnsDomain, szDnsDomain, &dwSize))
            {
                if (szDnsDomain[0])
                {
                    /* Just non-empty strings are valid. */
                    *ppszDomain = RTStrDup(szDnsDomain);
                    if (pData->fPassDomain)
                    {
                        if (fVerbose)
                            LogRel(("NAT: passing domain name %s\n", szDnsDomain));
                    }
                    else
                        Log(("nat: ignoring domain %s\n", szDnsDomain));
                }
            }
            else
                Log(("nat: GetComputerNameEx failed (%d)\n", GetLastError()));
        }
    }
    return rc;
}

#else

static int get_dns_addr_domain(PNATState pData, bool fVerbose,
                               struct in_addr *pdns_addr,
                               const char **ppszDomain)
{
    char buff[512];
    char buff2[256];
    FILE *f;
    int found = 0;
    struct in_addr tmp_addr;

#ifdef RT_OS_OS2
    /* Try various locations. */
    char *etc = getenv("ETC");
    f = NULL;
    if (etc)
    {
        snprintf(buff, sizeof(buff), "%s/RESOLV2", etc);
        f = fopen(buff, "rt");
    }
    if (!f) {
        snprintf(buff, sizeof(buff), "%s/RESOLV2", _PATH_ETC);
        f = fopen(buff, "rt");
    }
    if (!f) {
        snprintf(buff, sizeof(buff), "%s/resolv.conf", _PATH_ETC);
        f = fopen(buff, "rt");
    }
#else
    f = fopen("/etc/resolv.conf", "r");
#endif
    if (!f)
        return -1;

    if (ppszDomain)
        *ppszDomain = NULL;
    Log(("nat: DNS Servers:\n"));
    while (fgets(buff, 512, f) != NULL) {
        if (sscanf(buff, "nameserver%*[ \t]%256s", buff2) == 1) {
            if (!inet_aton(buff2, &tmp_addr))
                continue;
            if (tmp_addr.s_addr == loopback_addr.s_addr)
                tmp_addr = our_addr;
            /* If it's the first one, set it to dns_addr */
            if (!found)
            {
                if (fVerbose || pdns_addr->s_addr != tmp_addr.s_addr)
                    LogRel(("NAT: DNS address: %s\n", buff2));
                *pdns_addr = tmp_addr;
            }
            else
            {
                if (fVerbose)
                    LogRel(("NAT: ignored DNS address: %s\n", buff2));
            }
            found++;
        }
        if (   ppszDomain
            && (!strncmp(buff, "domain", 6) || !strncmp(buff, "search", 6)))
        {
            /* Domain name/search list present. Pick first entry */
            if (*ppszDomain == NULL)
            {
                char *tok;
                char *saveptr;
                tok = strtok_r(&buff[6], " \t\n", &saveptr);
                if (tok)
                {
                    *ppszDomain = RTStrDup(tok);
                    if (pData->fPassDomain)
                    {
                        if (fVerbose)
                            LogRel(("NAT: passing domain name %s\n", tok));
                    }
                    else
                        Log(("nat: ignoring domain %s\n", tok));
                }
            }
        }
    }
    fclose(f);
    if (!found)
        return -1;
    return 0;
}

#endif

int get_dns_addr(PNATState pData, struct in_addr *pdns_addr)
{
    return get_dns_addr_domain(pData, false, pdns_addr, NULL);
}

int slirp_init(PNATState *ppData, const char *pszNetAddr, uint32_t u32Netmask,
               bool fPassDomain, const char *pszTFTPPrefix,
               const char *pszBootFile, void *pvUser)
{
    int fNATfailed = 0;
    PNATState pData = malloc(sizeof(NATState));
    *ppData = pData;
    if (!pData)
        return VERR_NO_MEMORY;
    if (u32Netmask & 0x1f)
        /* CTL is x.x.x.15, bootp passes up to 16 IPs (15..31) */
        return VERR_INVALID_PARAMETER;
    memset(pData, '\0', sizeof(NATState));
    pData->fPassDomain = fPassDomain;
    pData->pvUser = pvUser;
#if ARCH_BITS == 64
    pData->cpvHashUsed = 1;
#endif
    tftp_prefix = pszTFTPPrefix;
    bootp_filename = pszBootFile;
    pData->netmask = u32Netmask;

#ifdef _WIN32
    {
        WSADATA Data;
        WSAStartup(MAKEWORD(2,0), &Data);
    }
#endif

    Assert(sizeof(struct ip) == 20);
    link_up = 1;

    if_init(pData);
    ip_init(pData);

    /* Initialise mbufs *after* setting the MTU */
    m_init(pData);

    /* set default addresses */
    inet_aton("127.0.0.1", &loopback_addr);
    inet_aton("127.0.0.1", &dns_addr);

    if (get_dns_addr_domain(pData, true, &dns_addr, &pData->pszDomain) < 0)
        fNATfailed = 1;

    inet_aton(pszNetAddr, &special_addr);
    alias_addr.s_addr = special_addr.s_addr | htonl(CTL_ALIAS);
    getouraddr(pData);
    return fNATfailed ? VINF_NAT_DNS : VINF_SUCCESS;
}

/**
 * Marks the link as up, making it possible to establish new connections.
 */
void slirp_link_up(PNATState pData)
{
    link_up = 1;
}

/**
 * Marks the link as down and cleans up the current connections.
 */
void slirp_link_down(PNATState pData)
{
    struct socket *so;

    while ((so = tcb.so_next) != &tcb)
    {
        if (so->so_state & SS_NOFDREF || so->s == -1)
            sofree(pData, so);
        else
            tcp_drop(pData, sototcpcb(so), 0);
    }

    while ((so = udb.so_next) != &udb)
        udp_detach(pData, so);

    link_up = 0;
}

/**
 * Terminates the slirp component.
 */
void slirp_term(PNATState pData)
{
    if (pData->pszDomain)
        RTStrFree((char *)(void *)pData->pszDomain);

#if ARCH_BITS == 64
    LogRel(("NAT: cpvHashUsed=%RU32 cpvHashCollisions=%RU32 cpvHashInserts=%RU64 cpvHashDone=%RU64\n",
            pData->cpvHashUsed, pData->cpvHashCollisions, pData->cpvHashInserts, pData->cpvHashDone));
#endif

    slirp_link_down(pData);
#ifdef WIN32
    WSACleanup();
#endif
#ifdef LOG_ENABLED
    Log(("\n"
         "NAT statistics\n"
         "--------------\n"
         "\n"));
    ipstats(pData);
    tcpstats(pData);
    udpstats(pData);
    icmpstats(pData);
    mbufstats(pData);
    sockstats(pData);
    Log(("\n"
         "\n"
         "\n"));
#endif
    free(pData);
}


#define CONN_CANFSEND(so) (((so)->so_state & (SS_FCANTSENDMORE|SS_ISFCONNECTED)) == SS_ISFCONNECTED)
#define CONN_CANFRCV(so) (((so)->so_state & (SS_FCANTRCVMORE|SS_ISFCONNECTED)) == SS_ISFCONNECTED)
#define UPD_NFDS(x) if (nfds < (x)) nfds = (x)

/*
 * curtime kept to an accuracy of 1ms
 */
#ifdef _WIN32
static void updtime(PNATState pData)
{
    struct _timeb tb;

    _ftime(&tb);
    curtime = (u_int)tb.time * (u_int)1000;
    curtime += (u_int)tb.millitm;
}
#else
static void updtime(PNATState pData)
{
	gettimeofday(&tt, 0);

	curtime = (u_int)tt.tv_sec * (u_int)1000;
	curtime += (u_int)tt.tv_usec / (u_int)1000;

	if ((tt.tv_usec % 1000) >= 500)
	   curtime++;
}
#endif

void slirp_select_fill(PNATState pData, int *pnfds,
                       fd_set *readfds, fd_set *writefds, fd_set *xfds)
{
    struct socket *so, *so_next;
    struct timeval timeout;
    int nfds;
    int tmp_time;

    nfds = *pnfds;
	/*
	 * First, TCP sockets
	 */
	do_slowtimo = 0;
	if (link_up) {
		/*
		 * *_slowtimo needs calling if there are IP fragments
		 * in the fragment queue, or there are TCP connections active
		 */
		do_slowtimo = ((tcb.so_next != &tcb) ||
			       ((struct ipasfrag *)&ipq != u32_to_ptr(pData, ipq.next, struct ipasfrag *)));

		for (so = tcb.so_next; so != &tcb; so = so_next) {
			so_next = so->so_next;

			/*
			 * See if we need a tcp_fasttimo
			 */
			if (time_fasttimo == 0 && so->so_tcpcb->t_flags & TF_DELACK)
			   time_fasttimo = curtime; /* Flag when we want a fasttimo */

			/*
			 * NOFDREF can include still connecting to local-host,
			 * newly socreated() sockets etc. Don't want to select these.
	 		 */
			if (so->so_state & SS_NOFDREF || so->s == -1)
			   continue;

			/*
			 * Set for reading sockets which are accepting
			 */
			if (so->so_state & SS_FACCEPTCONN) {
                                FD_SET(so->s, readfds);
				UPD_NFDS(so->s);
				continue;
			}

			/*
			 * Set for writing sockets which are connecting
			 */
			if (so->so_state & SS_ISFCONNECTING) {
				FD_SET(so->s, writefds);
				UPD_NFDS(so->s);
				continue;
			}

			/*
			 * Set for writing if we are connected, can send more, and
			 * we have something to send
			 */
			if (CONN_CANFSEND(so) && so->so_rcv.sb_cc) {
				FD_SET(so->s, writefds);
				UPD_NFDS(so->s);
			}

			/*
			 * Set for reading (and urgent data) if we are connected, can
			 * receive more, and we have room for it XXX /2 ?
			 */
			if (CONN_CANFRCV(so) && (so->so_snd.sb_cc < (so->so_snd.sb_datalen/2))) {
				FD_SET(so->s, readfds);
				FD_SET(so->s, xfds);
				UPD_NFDS(so->s);
			}
		}

		/*
		 * UDP sockets
		 */
		for (so = udb.so_next; so != &udb; so = so_next) {
			so_next = so->so_next;

			/*
			 * See if it's timed out
			 */
			if (so->so_expire) {
				if (so->so_expire <= curtime) {
					udp_detach(pData, so);
					continue;
				} else
					do_slowtimo = 1; /* Let socket expire */
			}

			/*
			 * When UDP packets are received from over the
			 * link, they're sendto()'d straight away, so
			 * no need for setting for writing
			 * Limit the number of packets queued by this session
			 * to 4.  Note that even though we try and limit this
			 * to 4 packets, the session could have more queued
			 * if the packets needed to be fragmented
			 * (XXX <= 4 ?)
			 */
			if ((so->so_state & SS_ISFCONNECTED) && so->so_queued <= 4) {
				FD_SET(so->s, readfds);
				UPD_NFDS(so->s);
			}
		}
	}

	/*
	 * Setup timeout to use minimum CPU usage, especially when idle
	 */

	/*
	 * First, see the timeout needed by *timo
	 */
	timeout.tv_sec = 0;
	timeout.tv_usec = -1;
	/*
	 * If a slowtimo is needed, set timeout to 500ms from the last
	 * slow timeout. If a fast timeout is needed, set timeout within
	 * 200ms of when it was requested.
	 */
	if (do_slowtimo) {
		/* XXX + 10000 because some select()'s aren't that accurate */
		timeout.tv_usec = ((500 - (curtime - last_slowtimo)) * 1000) + 10000;
		if (timeout.tv_usec < 0)
		   timeout.tv_usec = 0;
		else if (timeout.tv_usec > 510000)
		   timeout.tv_usec = 510000;

		/* Can only fasttimo if we also slowtimo */
		if (time_fasttimo) {
			tmp_time = (200 - (curtime - time_fasttimo)) * 1000;
			if (tmp_time < 0)
			   tmp_time = 0;

			/* Choose the smallest of the 2 */
			if (tmp_time < timeout.tv_usec)
			   timeout.tv_usec = (u_int)tmp_time;
		}
	}
        *pnfds = nfds;
}

void slirp_select_poll(PNATState pData, fd_set *readfds, fd_set *writefds, fd_set *xfds)
{
    struct socket *so, *so_next;
    int ret;

	/* Update time */
	updtime(pData);

	/*
	 * See if anything has timed out
	 */
	if (link_up) {
		if (time_fasttimo && ((curtime - time_fasttimo) >= 2)) {
			tcp_fasttimo(pData);
			time_fasttimo = 0;
		}
		if (do_slowtimo && ((curtime - last_slowtimo) >= 499)) {
			ip_slowtimo(pData);
			tcp_slowtimo(pData);
			last_slowtimo = curtime;
		}
	}

	/*
	 * Check sockets
	 */
	if (link_up) {
		/*
		 * Check TCP sockets
		 */
		for (so = tcb.so_next; so != &tcb; so = so_next) {
			so_next = so->so_next;

			/*
			 * FD_ISSET is meaningless on these sockets
			 * (and they can crash the program)
			 */
			if (so->so_state & SS_NOFDREF || so->s == -1)
			   continue;

			/*
			 * Check for URG data
			 * This will soread as well, so no need to
			 * test for readfds below if this succeeds
			 */
			if (FD_ISSET(so->s, xfds))
			   sorecvoob(pData, so);
			/*
			 * Check sockets for reading
			 */
			else if (FD_ISSET(so->s, readfds)) {
				/*
				 * Check for incoming connections
				 */
				if (so->so_state & SS_FACCEPTCONN) {
					tcp_connect(pData, so);
					continue;
				} /* else */
				ret = soread(pData, so);

				/* Output it if we read something */
				if (ret > 0)
				   tcp_output(pData, sototcpcb(so));
			}

			/*
			 * Check sockets for writing
			 */
			if (FD_ISSET(so->s, writefds)) {
			  /*
			   * Check for non-blocking, still-connecting sockets
			   */
			  if (so->so_state & SS_ISFCONNECTING) {
			    /* Connected */
			    so->so_state &= ~SS_ISFCONNECTING;

                /*
                 * This should be probably guarded by PROBE_CONN too. Anyway,
                 * we disable it on OS/2 because the below send call returns
                 * EFAULT which causes the opened TCP socket to close right
                 * after it has been opened and connected.
                 */
#ifndef RT_OS_OS2
			    ret = send(so->s, (const char *)&ret, 0, 0);
			    if (ret < 0) {
			      /* XXXXX Must fix, zero bytes is a NOP */
			      if (errno == EAGAIN || errno == EWOULDBLOCK ||
				  errno == EINPROGRESS || errno == ENOTCONN)
				continue;

			      /* else failed */
			      so->so_state = SS_NOFDREF;
			    }
			    /* else so->so_state &= ~SS_ISFCONNECTING; */
#endif

			    /*
			     * Continue tcp_input
			     */
			    tcp_input(pData, (struct mbuf *)NULL, sizeof(struct ip), so);
			    /* continue; */
			  } else
			    ret = sowrite(pData, so);
			  /*
			   * XXXXX If we wrote something (a lot), there
			   * could be a need for a window update.
			   * In the worst case, the remote will send
			   * a window probe to get things going again
			   */
			}

			/*
			 * Probe a still-connecting, non-blocking socket
			 * to check if it's still alive
	 	 	 */
#ifdef PROBE_CONN
			if (so->so_state & SS_ISFCONNECTING) {
			  ret = recv(so->s, (char *)&ret, 0,0);

			  if (ret < 0) {
			    /* XXX */
			    if (errno == EAGAIN || errno == EWOULDBLOCK ||
				errno == EINPROGRESS || errno == ENOTCONN)
			      continue; /* Still connecting, continue */

			    /* else failed */
			    so->so_state = SS_NOFDREF;

			    /* tcp_input will take care of it */
			  } else {
			    ret = send(so->s, &ret, 0,0);
			    if (ret < 0) {
			      /* XXX */
			      if (errno == EAGAIN || errno == EWOULDBLOCK ||
				  errno == EINPROGRESS || errno == ENOTCONN)
				continue;
			      /* else failed */
			      so->so_state = SS_NOFDREF;
			    } else
			      so->so_state &= ~SS_ISFCONNECTING;

			  }
			  tcp_input((struct mbuf *)NULL, sizeof(struct ip),so);
			} /* SS_ISFCONNECTING */
#endif
		}

		/*
		 * Now UDP sockets.
		 * Incoming packets are sent straight away, they're not buffered.
		 * Incoming UDP data isn't buffered either.
		 */
		for (so = udb.so_next; so != &udb; so = so_next) {
			so_next = so->so_next;

			if (so->s != -1 && FD_ISSET(so->s, readfds)) {
                            sorecvfrom(pData, so);
                        }
		}
	}

	/*
	 * See if we can start outputting
	 */
	if (if_queued && link_up)
	   if_start(pData);
}

#define ETH_ALEN 6
#define ETH_HLEN 14

#define ETH_P_IP	0x0800		/* Internet Protocol packet	*/
#define ETH_P_ARP	0x0806		/* Address Resolution packet	*/

#define	ARPOP_REQUEST	1		/* ARP request			*/
#define	ARPOP_REPLY	2		/* ARP reply			*/

struct ethhdr
{
	unsigned char	h_dest[ETH_ALEN];	/* destination eth addr	*/
	unsigned char	h_source[ETH_ALEN];	/* source ether addr	*/
	unsigned short	h_proto;		/* packet type ID field	*/
};

struct arphdr
{
	unsigned short	ar_hrd;		/* format of hardware address	*/
	unsigned short	ar_pro;		/* format of protocol address	*/
	unsigned char	ar_hln;		/* length of hardware address	*/
	unsigned char	ar_pln;		/* length of protocol address	*/
	unsigned short	ar_op;		/* ARP opcode (command)		*/

	 /*
	  *	 Ethernet looks like this : This bit is variable sized however...
	  */
	unsigned char		ar_sha[ETH_ALEN];	/* sender hardware address	*/
	unsigned char		ar_sip[4];		/* sender IP address		*/
	unsigned char		ar_tha[ETH_ALEN];	/* target hardware address	*/
	unsigned char		ar_tip[4];		/* target IP address		*/
};

static
void arp_input(PNATState pData, const uint8_t *pkt, int pkt_len)
{
    struct ethhdr *eh = (struct ethhdr *)pkt;
    struct arphdr *ah = (struct arphdr *)(pkt + ETH_HLEN);
    uint8_t arp_reply[ETH_HLEN + sizeof(struct arphdr)];
    struct ethhdr *reh = (struct ethhdr *)arp_reply;
    struct arphdr *rah = (struct arphdr *)(arp_reply + ETH_HLEN);
    int ar_op;
    struct ex_list *ex_ptr;

    ar_op = ntohs(ah->ar_op);
    switch(ar_op) {
    case ARPOP_REQUEST:
        if (!memcmp(ah->ar_tip, &special_addr, 3)) {
            if (ah->ar_tip[3] == CTL_DNS || ah->ar_tip[3] == CTL_ALIAS)
                goto arp_ok;
            for (ex_ptr = exec_list; ex_ptr; ex_ptr = ex_ptr->ex_next) {
                if (ex_ptr->ex_addr == ah->ar_tip[3])
                    goto arp_ok;
            }
            return;
        arp_ok:
            /* XXX: make an ARP request to have the client address */
            memcpy(client_ethaddr, eh->h_source, ETH_ALEN);

            /* ARP request for alias/dns mac address */
            memcpy(reh->h_dest, pkt + ETH_ALEN, ETH_ALEN);
            memcpy(reh->h_source, special_ethaddr, ETH_ALEN - 1);
            reh->h_source[5] = ah->ar_tip[3];
            reh->h_proto = htons(ETH_P_ARP);

            rah->ar_hrd = htons(1);
            rah->ar_pro = htons(ETH_P_IP);
            rah->ar_hln = ETH_ALEN;
            rah->ar_pln = 4;
            rah->ar_op = htons(ARPOP_REPLY);
            memcpy(rah->ar_sha, reh->h_source, ETH_ALEN);
            memcpy(rah->ar_sip, ah->ar_tip, 4);
            memcpy(rah->ar_tha, ah->ar_sha, ETH_ALEN);
            memcpy(rah->ar_tip, ah->ar_sip, 4);
            slirp_output(pData->pvUser, arp_reply, sizeof(arp_reply));
        }
        break;
    default:
        break;
    }
}

void slirp_input(PNATState pData, const uint8_t *pkt, int pkt_len)
{
    struct mbuf *m;
    int proto;

    if (pkt_len < ETH_HLEN)
        return;

    proto = ntohs(*(uint16_t *)(pkt + 12));
    switch(proto) {
    case ETH_P_ARP:
        arp_input(pData, pkt, pkt_len);
        break;
    case ETH_P_IP:
	/* Update time. Important if the network is very quiet, as otherwise
         * the first outgoing connection gets an incorrect timestamp. */
	updtime(pData);

        m = m_get(pData);
        if (!m)
            return;
        /* Note: we add to align the IP header */
        if (M_FREEROOM(m) < pkt_len + 2) {
            m_inc(m, pkt_len + 2);
        }
        m->m_len = pkt_len + 2;
        memcpy(m->m_data + 2, pkt, pkt_len);

        m->m_data += 2 + ETH_HLEN;
        m->m_len -= 2 + ETH_HLEN;

        ip_input(pData, m);
        break;
    default:
        break;
    }
}

/* output the IP packet to the ethernet device */
void if_encap(PNATState pData, const uint8_t *ip_data, int ip_data_len)
{
    uint8_t buf[1600];
    struct ethhdr *eh = (struct ethhdr *)buf;

    if (ip_data_len + ETH_HLEN > sizeof(buf))
        return;

    memcpy(eh->h_dest, client_ethaddr, ETH_ALEN);
    memcpy(eh->h_source, special_ethaddr, ETH_ALEN - 1);
    /* XXX: not correct */
    eh->h_source[5] = CTL_ALIAS;
    eh->h_proto = htons(ETH_P_IP);
    memcpy(buf + sizeof(struct ethhdr), ip_data, ip_data_len);
    slirp_output(pData->pvUser, buf, ip_data_len + ETH_HLEN);
}

int slirp_redir(PNATState pData, int is_udp, int host_port,
                struct in_addr guest_addr, int guest_port)
{
    if (is_udp) {
        if (!udp_listen(pData, htons(host_port), guest_addr.s_addr,
                        htons(guest_port), 0))
            return -1;
    } else {
        if (!solisten(pData, htons(host_port), guest_addr.s_addr,
                      htons(guest_port), 0))
            return -1;
    }
    return 0;
}

int slirp_add_exec(PNATState pData, int do_pty, const char *args, int addr_low_byte,
                  int guest_port)
{
    return add_exec(&exec_list, do_pty, (char *)args,
                    addr_low_byte, htons(guest_port));
}
