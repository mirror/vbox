#include "slirp.h"
#ifdef RT_OS_OS2
# include <paths.h>
#endif

/* disable these counters for the final release */
/* #define VBOX_WITHOUT_RELEASE_STATISTICS */

#include <VBox/err.h>
#include <VBox/pdmdrv.h>
#include <iprt/assert.h>

#if defined(VBOX_WITH_SIMPLIFIED_SLIRP_SYNC) 
# if !defined(RT_OS_WINDOWS)
#  define DO_ENGAGE_EVENT1(so, fdset, label)            \
        do {                                            \
                FD_SET((so)->s, (fdset));              \
                UPD_NFDS((so)->s);                      \
        while(0)


#  define DO_ENGAGE_EVENT2(so, fdset0, fdset1, label)   \
        do {                                            \
                FD_SET((so)->s, (fdset0));              \
                FD_SET((so)->s, (fdset1));              \
                UPD_NFDS((so)->s);                      \
        while(0)
# else /* !RT_OS_WINDOWS */
#  define DO_ENGAGE_EVENT1(so, fdset0, label)           \
                goto label;

#  define DO_ENGAGE_EVENT2(so, fdset0, fdset1, label) DO_ENGAGE_EVENT1((so), (fdset0), label)
# endif /* RT_OS_WINDOWS */

# define TCP_ENGAGE_EVENT1(so, fdset0)                   \
        DO_ENGAGE_EVENT1((so), (fdset0), tcp_engage_event)

# define TCP_ENGAGE_EVENT2(so, fdset0, fdset1)               \
        DO_ENGAGE_EVENT2((so), (fdset0), (fdset1), tcp_engage_event)
#endif /* VBOX_WITH_SIMPLIFIED_SLIRP_SYNC */

static const uint8_t special_ethaddr[6] = {
    0x52, 0x54, 0x00, 0x12, 0x35, 0x00
};

#ifdef RT_OS_WINDOWS

static int get_dns_addr_domain(PNATState pData, bool fVerbose,
                               struct in_addr *pdns_addr,
                               const char **ppszDomain)
{
    int  rc = 0;
    FIXED_INFO *FixedInfo = NULL;
    ULONG BufLen;
    DWORD ret;
    IP_ADDR_STRING *pIPAddr;
    struct in_addr tmp_addr;

    FixedInfo = (FIXED_INFO *)GlobalAlloc(GPTR, sizeof(FIXED_INFO));
    BufLen = sizeof(FIXED_INFO);

    /** @todo: this API returns all DNS servers, no matter whether the
     * corresponding network adapter is disabled or not. Maybe replace
     * this by GetAdapterAddresses(), which is XP/Vista only though. */
    if (ERROR_BUFFER_OVERFLOW == GetNetworkParams(FixedInfo, &BufLen))
    {
        if (FixedInfo)
        {
            GlobalFree(FixedInfo);
            FixedInfo = NULL;
        }
        FixedInfo = GlobalAlloc(GPTR, BufLen);
    }

    if ((ret = GetNetworkParams(FixedInfo, &BufLen)) != ERROR_SUCCESS)
    {
        Log(("GetNetworkParams failed. ret = %08x\n", (u_int)ret ));
        if (FixedInfo)
        {
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
    while (pIPAddr)
    {
        if (fVerbose)
            LogRel(("NAT: ignored DNS address: %s\n", pIPAddr ->IpAddress.String));
        pIPAddr = pIPAddr ->Next;
    }
    if (FixedInfo)
    {
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
    if (!f)
    {
        snprintf(buff, sizeof(buff), "%s/RESOLV2", _PATH_ETC);
        f = fopen(buff, "rt");
    }
    if (!f)
    {
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
    while (fgets(buff, 512, f) != NULL)
    {
        if (sscanf(buff, "nameserver%*[ \t]%256s", buff2) == 1)
        {
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
#if ARCH_BITS == 64 && !defined(VBOX_WITH_BSD_REASS)
    pData->cpvHashUsed = 1;
#endif
    tftp_prefix = pszTFTPPrefix;
    bootp_filename = pszBootFile;
    pData->netmask = u32Netmask;

#ifdef RT_OS_WINDOWS
    {
        WSADATA Data;
        WSAStartup(MAKEWORD(2,0), &Data);
    }
# if defined(VBOX_WITH_SIMPLIFIED_SLIRP_SYNC)
    pData->phEvents[VBOX_SOCKET_EVENT_INDEX] = CreateEvent(NULL, FALSE, FALSE, NULL);
# endif
#endif

    Assert(sizeof(struct ip) == 20);
    link_up = 1;

    debug_init();
    if_init(pData);
    ip_init(pData);
#ifdef VBOX_WITH_SLIRP_ICMP
    icmp_init(pData);
#endif /* VBOX_WITH_SLIRP_ICMP */

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
 * Statistics counters.
 */
void slirp_register_timers(PNATState pData, PPDMDRVINS pDrvIns)
{
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatFill, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS,
                           STAMUNIT_TICKS_PER_CALL, "Profiling slirp fills", "/Drivers/NAT%d/Fill", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatPoll, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS,
                           STAMUNIT_TICKS_PER_CALL, "Profiling slirp polls", "/Drivers/NAT%d/Poll", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatFastTimer, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS,
                           STAMUNIT_TICKS_PER_CALL, "Profiling slirp fast timer", "/Drivers/NAT%d/TimerFast", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatSlowTimer, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS,
                           STAMUNIT_TICKS_PER_CALL, "Profiling slirp slow timer", "/Drivers/NAT%d/TimerSlow", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatTCP, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS,
                           STAMUNIT_COUNT, "TCP sockets", "/Drivers/NAT%d/SockTCP", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatTCPHot, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS,
                           STAMUNIT_COUNT, "TCP sockets active", "/Drivers/NAT%d/SockTCPHot", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatUDP, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS,
                           STAMUNIT_COUNT, "UDP sockets", "/Drivers/NAT%d/SockUDP", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatUDPHot, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS,
                           STAMUNIT_COUNT, "UDP sockets active", "/Drivers/NAT%d/SockUDPHot", pDrvIns->iInstance);
#endif /* VBOX_WITHOUT_RELEASE_STATISTICS */
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

#if ARCH_BITS == 64 && !defined(VBOX_WITH_BSD_REASS)
    LogRel(("NAT: cpvHashUsed=%RU32 cpvHashCollisions=%RU32 cpvHashInserts=%RU64 cpvHashDone=%RU64\n",
            pData->cpvHashUsed, pData->cpvHashCollisions, pData->cpvHashInserts, pData->cpvHashDone));
#endif

    slirp_link_down(pData);
#ifdef RT_OS_WINDOWS
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
#define CONN_CANFRCV(so)  (((so)->so_state & (SS_FCANTRCVMORE|SS_ISFCONNECTED)) == SS_ISFCONNECTED)
#define UPD_NFDS(x)       if (nfds < (x)) nfds = (x)

/*
 * curtime kept to an accuracy of 1ms
 */
#ifdef RT_OS_WINDOWS
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
    int nfds;
#if defined(VBOX_WITH_SIMPLIFIED_SLIRP_SYNC) && defined(RT_OS_WINDOWS)
    int rc;
    int error;
#endif
#ifdef VBOX_WITH_BSD_REASS
    int i;
#endif /* VBOX_WITH_BSD_REASS */

    STAM_REL_PROFILE_START(&pData->StatFill, a);

    nfds = *pnfds;

    /*
     * First, TCP sockets
     */
    do_slowtimo = 0;
    if (link_up)
    {
        /*
         * *_slowtimo needs calling if there are IP fragments
         * in the fragment queue, or there are TCP connections active
         */
#ifndef VBOX_WITH_BSD_REASS
        do_slowtimo =    ((tcb.so_next != &tcb)
                      || ((struct ipasfrag *)&ipq != u32_to_ptr(pData, ipq.next, struct ipasfrag *)));
#else /* !VBOX_WITH_BSD_REASS */
        /* XXX:
         * triggering of fragment expiration should be the same but use new macroses
         */
        do_slowtimo = (tcb.so_next != &tcb);
        if (!do_slowtimo)
        {
            for (i = 0; i < IPREASS_NHASH; i++)
            {
                if (!TAILQ_EMPTY(&ipq[i]))
                {
                    do_slowtimo = 1;
                    break;
                }
            }
        }
#endif /* VBOX_WITH_BSD_REASS */

        STAM_REL_COUNTER_RESET(&pData->StatTCP);
        STAM_REL_COUNTER_RESET(&pData->StatTCPHot);

        for (so = tcb.so_next; so != &tcb; so = so_next)
        {
            so_next = so->so_next;

            STAM_REL_COUNTER_INC(&pData->StatTCP);

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
            if (so->so_state & SS_FACCEPTCONN)
            {
                STAM_REL_COUNTER_INC(&pData->StatTCPHot);
                TCP_ENGAGE_EVENT1(so, readfs);
#if defined(VBOX_WITH_SIMPLIFIED_SLIRP_SYNC) && defined(RT_OS_WINDOWS)
tcp_engage_event:
                rc = WSAEventSelect(so->s, VBOX_SOCKET_EVENT, FD_ALL_EVENTS);
                if (rc == SOCKET_ERROR)
                {
                    /* This should not happen */
                    error = WSAGetLastError();
                    LogRel(("WSAEventSelector (TCP) error %d (so=%x, socket=%s, event=%x)\n",
                             error, so, so->s, VBOX_SOCKET_EVENT));
                }
#endif
                continue;
            }

            /*
             * Set for writing sockets which are connecting
             */
            if (so->so_state & SS_ISFCONNECTING)
            {
                STAM_REL_COUNTER_INC(&pData->StatTCPHot);
                TCP_ENGAGE_EVENT1(so, writefds);
            }

            /*
             * Set for writing if we are connected, can send more, and
             * we have something to send
             */
            if (CONN_CANFSEND(so) && so->so_rcv.sb_cc)
            {
                STAM_REL_COUNTER_INC(&pData->StatTCPHot);
                TCP_ENGAGE_EVENT1(so, writefds);
            }

            /*
             * Set for reading (and urgent data) if we are connected, can
             * receive more, and we have room for it XXX /2 ?
             */
            if (CONN_CANFRCV(so) && (so->so_snd.sb_cc < (so->so_snd.sb_datalen/2)))
            {
                STAM_REL_COUNTER_INC(&pData->StatTCPHot);
                TCP_ENGAGE_EVENT2(so, readfds, xfds);
            }
        }

        /*
         * UDP sockets
         */
        STAM_REL_COUNTER_RESET(&pData->StatUDP);
        STAM_REL_COUNTER_RESET(&pData->StatUDPHot);

        for (so = udb.so_next; so != &udb; so = so_next)
        {
            so_next = so->so_next;

            STAM_REL_COUNTER_INC(&pData->StatUDP);

            /*
             * See if it's timed out
             */
            if (so->so_expire)
            {
                if (so->so_expire <= curtime)
                {
                    udp_detach(pData, so);
                    continue;
                }
                else
                    do_slowtimo = 1; /* Let socket expire */
            }

            /*
             * When UDP packets are received from over the link, they're
             * sendto()'d straight away, so no need for setting for writing
             * Limit the number of packets queued by this session to 4.
             * Note that even though we try and limit this to 4 packets,
             * the session could have more queued if the packets needed
             * to be fragmented.
             *
             * (XXX <= 4 ?)
             */
            if ((so->so_state & SS_ISFCONNECTED) && so->so_queued <= 4)
            {
                STAM_REL_COUNTER_INC(&pData->StatUDPHot);
#if !defined(VBOX_WITH_SIMPLIFIED_SLIRP_SYNC) || !defined(RT_OS_WINDOWS)
                FD_SET(so->s, readfds);
                UPD_NFDS(so->s);
#else
                rc = WSAEventSelect(so->s, VBOX_SOCKET_EVENT, FD_ALL_EVENTS);
                if (rc == SOCKET_ERROR)
                {
                    /* This should not happen */
                    error = WSAGetLastError();
                    LogRel(("WSAEventSelector (UDP) error %d (so=%x, socket=%s, event=%x)\n",
                            error, so, so->s, VBOX_SOCKET_EVENT));
                }
#endif
            }
        }
    }

#if !defined(VBOX_WITH_SIMPLIFIED_SLIRP_SYNC) || !defined(RT_OS_WINDOWS)
    *pnfds = nfds;
#else
    *pnfds = VBOX_EVENT_COUNT;
#endif

    STAM_REL_PROFILE_STOP(&pData->StatFill, a);
}

void slirp_select_poll(PNATState pData, fd_set *readfds, fd_set *writefds, fd_set *xfds)
{
    struct socket *so, *so_next;
    int ret;
#if defined(VBOX_WITH_SIMPLIFIED_SLIRP_SYNC) && defined(RT_OS_WINDOWS)
    WSANETWORKEVENTS NetworkEvents;
    int rc;
    int error;
#endif
    static uint32_t stat_time;

    STAM_REL_PROFILE_START(&pData->StatPoll, a);

    /* Update time */
    updtime(pData);

#ifdef LOG_ENABLED
    if (curtime - stat_time > 10000)
    {
        stat_time = curtime;
        sockstats(pData);
    }
#endif

    /*
     * See if anything has timed out
     */
    if (link_up)
    {
        if (time_fasttimo && ((curtime - time_fasttimo) >= 2))
        {
            STAM_REL_PROFILE_START(&pData->StatFastTimer, a);
            tcp_fasttimo(pData);
            time_fasttimo = 0;
            STAM_REL_PROFILE_STOP(&pData->StatFastTimer, a);
        }
        if (do_slowtimo && ((curtime - last_slowtimo) >= 499))
        {
            STAM_REL_PROFILE_START(&pData->StatSlowTimer, a);
            ip_slowtimo(pData);
            tcp_slowtimo(pData);
            last_slowtimo = curtime;
            STAM_REL_PROFILE_STOP(&pData->StatSlowTimer, a);
        }
    }
#if defined(VBOX_WITH_SIMPLIFIED_SLIRP_SYNC) && defined(RT_OS_WINDOWS)
    if (!readfds && !writefds && !xfds)
        return; /* only timer update */
#endif

    /*
     * Check sockets
     */
    if (link_up)
    {
        /*
         * Check TCP sockets
         */
        for (so = tcb.so_next; so != &tcb; so = so_next)
        {
            so_next = so->so_next;

            /*
             * FD_ISSET is meaningless on these sockets
             * (and they can crash the program)
             */
            if (so->so_state & SS_NOFDREF || so->s == -1)
                continue;

#if defined(VBOX_WITH_SIMPLIFIED_SLIRP_SYNC) && defined(RT_OS_WINDOWS)
            rc = WSAEnumNetworkEvents(so->s, VBOX_SOCKET_EVENT, &NetworkEvents);
            if (rc == SOCKET_ERROR)
            {
                error = WSAGetLastError();
                LogRel(("WSAEnumNetworkEvents TCP error %d\n", error));
                continue;
            }
#endif

            /*
             * Check for URG data
             * This will soread as well, so no need to
             * test for readfds below if this succeeds
             */
#if !defined(VBOX_WITH_SIMPLIFIED_SLIRP_SYNC) || !defined(RT_OS_WINDOWS)
            if (FD_ISSET(so->s, xfds))
#else
            /* out-of-band data */
            if ((NetworkEvents.lNetworkEvents & FD_OOB) && NetworkEvents.iErrorCode[FD_OOB_BIT] == 0)
#endif
            {
                sorecvoob(pData, so);
            }

            /*
             * Check sockets for reading
             */
#if !defined(VBOX_WITH_SIMPLIFIED_SLIRP_SYNC) || !defined(RT_OS_WINDOWS)
            else if (FD_ISSET(so->s, readfds))
#else
            else if ((NetworkEvents.lNetworkEvents & FD_READ) && (NetworkEvents.iErrorCode[FD_READ_BIT] == 0))
#endif
            {
                /*
                 * Check for incoming connections
                 */
                if (so->so_state & SS_FACCEPTCONN)
                {
                    tcp_connect(pData, so);
#if defined(VBOX_WITH_SIMPLIFIED_SLIRP_SYNC) && defined(RT_OS_WINDOWS)
                    if (!NetworkEvents.lNetworkEvents & FD_CLOSE)
#endif
                        continue;
                }

                ret = soread(pData, so);
                /* Output it if we read something */
                if (ret > 0)
                    tcp_output(pData, sototcpcb(so));
            }

#if defined(VBOX_WITH_SIMPLIFIED_SLIRP_SYNC) && defined(RT_OS_WINDOWS)
            /*
             * Check for FD_CLOSE events.
             */
            if (NetworkEvents.lNetworkEvents & FD_CLOSE)
            {
                /*
                 * drain the socket
                 */
                for (;;)
                {
                    ret = soread(pData, so);
                    if (ret > 0)
                        tcp_output(pData, sototcpcb(so));
                    else
                        break;
                }
            }
#endif

            /*
             * Check sockets for writing
             */
#if !defined(VBOX_WITH_SIMPLIFIED_SLIRP_SYNC) || !defined(RT_OS_WINDOWS)
            if (FD_ISSET(so->s, writefds))
#else
            if ((NetworkEvents.lNetworkEvents & FD_WRITE) && (NetworkEvents.iErrorCode[FD_WRITE_BIT] == 0))
#endif
            {
                /*
                 * Check for non-blocking, still-connecting sockets
                 */
                if (so->so_state & SS_ISFCONNECTING)
                {
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
                    if (ret < 0)
                    {
                        /* XXXXX Must fix, zero bytes is a NOP */
                        if (   errno == EAGAIN
                            || errno == EWOULDBLOCK
                            || errno == EINPROGRESS
                            || errno == ENOTCONN)
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
                }
                else
                    ret = sowrite(pData, so);
                /*
                 * XXX If we wrote something (a lot), there could be the need
                 * for a window update. In the worst case, the remote will send
                 * a window probe to get things going again.
                 */
            }

            /*
             * Probe a still-connecting, non-blocking socket
             * to check if it's still alive
             */
#ifdef PROBE_CONN
            if (so->so_state & SS_ISFCONNECTING)
            {
                ret = recv(so->s, (char *)&ret, 0, 0);

                if (ret < 0)
                {
                    /* XXX */
                    if (   errno == EAGAIN
                        || errno == EWOULDBLOCK
                        || errno == EINPROGRESS
                        || errno == ENOTCONN)
                    {
                        continue; /* Still connecting, continue */
                    }

                    /* else failed */
                    so->so_state = SS_NOFDREF;

                    /* tcp_input will take care of it */
                }
                else
                {
                    ret = send(so->s, &ret, 0, 0);
                    if (ret < 0)
                    {
                        /* XXX */
                        if (   errno == EAGAIN
                            || errno == EWOULDBLOCK
                            || errno == EINPROGRESS
                            || errno == ENOTCONN)
                        {
                            continue;
                        }
                        /* else failed */
                        so->so_state = SS_NOFDREF;
                    }
                    else
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
        for (so = udb.so_next; so != &udb; so = so_next)
        {
            so_next = so->so_next;

#if defined(VBOX_WITH_SIMPLIFIED_SLIRP_SYNC) && defined(RT_OS_WINDOWS)
            rc = WSAEnumNetworkEvents(so->s, VBOX_SOCKET_EVENT, &NetworkEvents);
            if (rc == SOCKET_ERROR)
            {
                error = WSAGetLastError();
                LogRel(("WSAEnumNetworkEvents TCP error %d\n", error));
                continue;
            }
#endif
#if !defined(VBOX_WITH_SIMPLIFIED_SLIRP_SYNC) || !defined(RT_OS_WINDOWS)
            if (so->s != -1 && FD_ISSET(so->s, readfds))
#else
            if (((NetworkEvents.lNetworkEvents & FD_READ) && (NetworkEvents.iErrorCode[FD_READ_BIT] == 0))
#ifdef VBOX_WITH_SLIRP_ICMP
		|| (so->s == pData->icmp_socket.s) /* XXX: How check equality of handles */
#endif
	    )
#endif
            {
                sorecvfrom(pData, so);
            }
        }
    }

    /*
     * See if we can start outputting
     */
    if (if_queued && link_up)
        if_start(pData);

    STAM_REL_PROFILE_STOP(&pData->StatPoll, a);
}

#define ETH_ALEN        6
#define ETH_HLEN        14

#define ETH_P_IP        0x0800          /* Internet Protocol packet     */
#define ETH_P_ARP       0x0806          /* Address Resolution packet    */

#define ARPOP_REQUEST   1               /* ARP request                  */
#define ARPOP_REPLY     2               /* ARP reply                    */

struct ethhdr
{
    unsigned char   h_dest[ETH_ALEN];           /* destination eth addr */
    unsigned char   h_source[ETH_ALEN];         /* source ether addr    */
    unsigned short  h_proto;                    /* packet type ID field */
};

struct arphdr
{
    unsigned short  ar_hrd;             /* format of hardware address   */
    unsigned short  ar_pro;             /* format of protocol address   */
    unsigned char   ar_hln;             /* length of hardware address   */
    unsigned char   ar_pln;             /* length of protocol address   */
    unsigned short  ar_op;              /* ARP opcode (command)         */

    /*
     *      Ethernet looks like this : This bit is variable sized however...
     */
    unsigned char   ar_sha[ETH_ALEN];   /* sender hardware address      */
    unsigned char   ar_sip[4];          /* sender IP address            */
    unsigned char   ar_tha[ETH_ALEN];   /* target hardware address      */
    unsigned char   ar_tip[4];          /* target IP address            */
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
    uint32_t htip = ntohl(*(uint32_t*)ah->ar_tip);

    ar_op = ntohs(ah->ar_op);
    switch(ar_op)
    {
        case ARPOP_REQUEST:
            if ((htip & pData->netmask) == ntohl(special_addr.s_addr))
            {
                if (   (htip & ~pData->netmask) == CTL_DNS
                    || (htip & ~pData->netmask) == CTL_ALIAS)
                    goto arp_ok;
                for (ex_ptr = exec_list; ex_ptr; ex_ptr = ex_ptr->ex_next)
                {
                    if ((htip & ~pData->netmask) == ex_ptr->ex_addr)
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
    switch(proto)
    {
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
            if (M_FREEROOM(m) < pkt_len + 2)
            {
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
    if (is_udp)
    {
        if (!udp_listen(pData, htons(host_port), guest_addr.s_addr,
                        htons(guest_port), 0))
            return -1;
    }
    else
    {
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

void slirp_set_ethaddr(PNATState pData, const uint8_t *ethaddr)
{
    memcpy(client_ethaddr, ethaddr, ETH_ALEN);
}

#if defined(VBOX_WITH_SIMPLIFIED_SLIRP_SYNC) && defined(RT_OS_WINDOWS)
HANDLE *slirp_get_events(PNATState pData)
{
        return pData->phEvents;
}
void slirp_register_external_event(PNATState pData, HANDLE hEvent, int index)
{
        pData->phEvents[index] = hEvent;
}
#endif

unsigned int slirp_get_timeout_ms(PNATState pData)
{
    if (link_up)
    {
        if (time_fasttimo)
            return 2;
        if (do_slowtimo)
            return 500; /* see PR_SLOWHZ */
    }
    return 0;
}
