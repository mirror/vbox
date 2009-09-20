/*
 * Copyright (c) 1995 Danny Gasparovski.
 *
 * Please read the file COPYRIGHT for the
 * terms and conditions of the copyright.
 */

#include <slirp.h>


#ifndef VBOX_WITH_SLIRP_BSD_MBUF
# define ifs_init(ifm) ((ifm)->ifs_next = (ifm)->ifs_prev = (ifm))

static void ifs_insque(struct mbuf *ifm, struct mbuf *ifmhead)
{
    ifm->ifs_next = ifmhead->ifs_next;
    ifmhead->ifs_next = ifm;
    ifm->ifs_prev = ifmhead;
    ifm->ifs_next->ifs_prev = ifm;
}

static void ifs_remque(struct mbuf *ifm)
{
    ifm->ifs_prev->ifs_next = ifm->ifs_next;
    ifm->ifs_next->ifs_prev = ifm->ifs_prev;
}
#else
#endif

void
if_init(PNATState pData)
{
    /* 14 for ethernet */
    if_maxlinkhdr = 14;
    if_queued = 0;
    if_thresh = 10;
    if_comp = IF_AUTOCOMP;
    if_mtu = 1500;
    if_mru = 1500;
#ifndef VBOX_WITH_SLIRP_BSD_MBUF
    if_fastq.ifq_next = if_fastq.ifq_prev = &if_fastq;
    if_batchq.ifq_next = if_batchq.ifq_prev = &if_batchq;
/*  sl_compress_init(&comp_s); */
    next_m = &if_batchq;
#else
    TAILQ_INIT(&if_fastq);
    TAILQ_INIT(&if_batchq);
    next_m = TAILQ_FIRST(&if_fastq);
#endif
}

/*
 * if_output: Queue packet into an output queue.
 * There are 2 output queue's, if_fastq and if_batchq.
 * Each output queue is a doubly linked list of double linked lists
 * of mbufs, each list belonging to one "session" (socket).  This
 * way, we can output packets fairly by sending one packet from each
 * session, instead of all the packets from one session, then all packets
 * from the next session, etc.  Packets on the if_fastq get absolute
 * priority, but if one session hogs the link, it gets "downgraded"
 * to the batchq until it runs out of packets, then it'll return
 * to the fastq (eg. if the user does an ls -alR in a telnet session,
 * it'll temporarily get downgraded to the batchq)
 */
void
if_output(PNATState pData, struct socket *so, struct mbuf *ifm)
{
    struct mbuf *ifq;
    int on_fastq = 1;

    DEBUG_CALL("if_output");
    DEBUG_ARG("so = %lx", (long)so);
    DEBUG_ARG("ifm = %lx", (long)ifm);

#ifndef VBOX_WITH_SLIRP_BSD_MBUF
    /*
     * First remove the mbuf from m_usedlist,
     * since we're gonna use m_next and m_prev ourselves
     * XXX Shouldn't need this, gotta change dtom() etc.
     */
    if (ifm->m_flags & M_USEDLIST)
    {
        remque(pData, ifm);
        ifm->m_flags &= ~M_USEDLIST;
    }
#endif

    /*
     * See if there's already a batchq list for this session.
     * This can include an interactive session, which should go on fastq,
     * but gets too greedy... hence it'll be downgraded from fastq to batchq.
     * We mustn't put this packet back on the fastq (or we'll send it out of order)
     * XXX add cache here?
     */
#ifndef VBOX_WITH_SLIRP_BSD_MBUF
    for (ifq = if_batchq.ifq_prev; ifq != &if_batchq; ifq = ifq->ifq_prev)
#else
    TAILQ_FOREACH_REVERSE(ifq, &if_batchq, if_queue, m_ifq)
#endif
    {
        if (so == ifq->ifq_so)
        {
            /* A match! */
            ifm->ifq_so = so;
#ifndef VBOX_WITH_SLIRP_BSD_MBUF
            ifs_insque(ifm, ifq->ifs_prev);
#endif
            goto diddit;
        }
    }

    /* No match, check which queue to put it on */
    if (so && (so->so_iptos & IPTOS_LOWDELAY))
    {
#ifndef VBOX_WITH_SLIRP_BSD_MBUF
        ifq = if_fastq.ifq_prev;
#else
        ifq = TAILQ_LAST(&if_fastq, if_queue);
#endif
        on_fastq = 1;
        /*
         * Check if this packet is a part of the last
         * packet's session
         */
        if (ifq->ifq_so == so)
        {
            ifm->ifq_so = so;
#ifndef VBOX_WITH_SLIRP_BSD_MBUF
            ifs_insque(ifm, ifq->ifs_prev);
#endif
            goto diddit;
        }
    }
#ifndef VBOX_WITH_SLIRP_BSD_MBUF
    else
        ifq = if_batchq.ifq_prev;

    /* Create a new doubly linked list for this session */
    ifm->ifq_so = so;
    ifs_init(ifm);
    insque(pData, ifm, ifq);
#else
    else
    {
        TAILQ_INSERT_TAIL(&if_batchq, ifm, m_ifq);
        ifq = TAILQ_LAST(&if_batchq, if_queue);
    }
    
    /* queue already created */
#endif

diddit:
    ++if_queued;

    if (so)
    {
        /* Update *_queued */
        so->so_queued++;
        so->so_nqueued++;
        /*
         * Check if the interactive session should be downgraded to
         * the batchq.  A session is downgraded if it has queued 6
         * packets without pausing, and at least 3 of those packets
         * have been sent over the link
         * (XXX These are arbitrary numbers, probably not optimal..)
         */
        if (on_fastq
            && so->so_nqueued >= 6
            && (so->so_nqueued - so->so_queued) >= 3)
        {
#ifndef VBOX_WITH_SLIRP_BSD_MBUF
            /* Remove from current queue... */
            remque(pData, ifm->ifs_next);

            /* ...And insert in the new.  That'll teach ya! */
            insque(pData, ifm->ifs_next, &if_batchq);
#endif
        }
    }

#ifndef FULL_BOLT
    /*
     * This prevents us from malloc()ing too many mbufs
     */
    if (link_up)
    {
        /* if_start will check towrite */
        if_start(pData);
    }
#endif
}

/*
 * Send a packet
 * We choose a packet based on it's position in the output queues;
 * If there are packets on the fastq, they are sent FIFO, before
 * everything else.  Otherwise we choose the first packet from the
 * batchq and send it.  the next packet chosen will be from the session
 * after this one, then the session after that one, and so on..  So,
 * for example, if there are 3 ftp session's fighting for bandwidth,
 * one packet will be sent from the first session, then one packet
 * from the second session, then one packet from the third, then back
 * to the first, etc. etc.
 */
void
if_start(PNATState pData)
{
    struct mbuf *ifm, *ifqt;
#ifdef VBOX_WITH_SLIRP_BSD_MBUF
    struct if_queue *pqueue = NULL;
    ifm = NULL;
    ifqt = NULL;
#endif

    DEBUG_CALL("if_start");

    if (!if_queued)
        return; /* Nothing to do */

    for (;;)
    {
        /* check if we can really output */
        if (!slirp_can_output(pData->pvUser))
        {
            Log(("if_start: can't send\n"));
            return;
        }

#ifndef VBOX_WITH_SLIRP_BSD_MBUF
        /*
         * See which queue to get next packet from
         * If there's something in the fastq, select it immediately
         */
        if (if_fastq.ifq_next != &if_fastq)
            ifm = if_fastq.ifq_next;
        else
        {
            /* Nothing on fastq, see if next_m is valid */
            if (next_m != &if_batchq)
                ifm = next_m;
            else
                ifm = if_batchq.ifq_next;

            /* Set which packet to send on next iteration */
            next_m = ifm->ifq_next;
        }
        /* Remove it from the queue */
        ifqt = ifm->ifq_prev;
        remque(pData, ifm);
        --if_queued;

        /* If there are more packets for this session, re-queue them */
        if (ifm->ifs_next != /* ifm->ifs_prev != */ ifm)
        {
            insque(pData, ifm->ifs_next, ifqt);
            ifs_remque(ifm);
        }
#else
        if (!TAILQ_EMPTY(&if_fastq))
        {
            pqueue = &if_fastq;
        }
        else if (!TAILQ_EMPTY(&if_batchq))
        {
            pqueue = &if_batchq;
        }
        if (pqueue != NULL) {
            ifm = TAILQ_FIRST(pqueue);
            TAILQ_REMOVE(pqueue, ifm, m_ifq);
            --if_queued;
        }
#endif

        /* Update so_queued */
        if (ifm->ifq_so)
        {
            if (--ifm->ifq_so->so_queued == 0)
                /* If there's no more queued, reset nqueued */
                ifm->ifq_so->so_nqueued = 0;
        }

        if_encap(pData, ETH_P_IP, ifm);

        if (!if_queued) 
        {
            return;
        }
    }
}
