/*
 * Copyright (c) 1995 Danny Gasparovski
 *
 * Please read the file COPYRIGHT for the
 * terms and conditions of the copyright.
 */

/*
 * mbuf's in SLiRP are much simpler than the real mbufs in
 * FreeBSD.  They are fixed size, determined by the MTU,
 * so that one whole packet can fit.  Mbuf's cannot be
 * chained together.  If there's more data than the mbuf
 * could hold, an external malloced buffer is pointed to
 * by m_ext (and the data pointers) and M_EXT is set in
 * the flags
 */
#include <slirp.h>

#define MBUF_ZONE_SIZE 100
static int mbuf_zone_init(PNATState pData)
{
    int limit;
    struct mbuf_zone *mzone;
    int i;
    struct mbuf *m;
    uint8_t *zone = RTMemAlloc(msize * MBUF_ZONE_SIZE);  
    if (zone == NULL)
    {
        LogRel(("NAT: can't allocate new zone\n"));
        return -1;
    }
    mzone = RTMemAllocZ(sizeof (struct mbuf_zone));
    if (mzone == NULL)
    {
        RTMemFree(zone);
        LogRel(("NAT: can't allocate zone descriptor\n"));
        return -1;
    }

    for(i = 0; i < MBUF_ZONE_SIZE; ++i)
    {
        m = (struct mbuf *)((char *)zone + i*msize);
#ifdef M_BUF_DEBUG
        m->m_hdr.mh_id = pData->mbuf_zone_count * MBUF_ZONE_SIZE + i;
#endif
        insque(pData, m, &m_freelist);
    }
    mzone->mbuf_zone_base_addr = zone;
    LIST_INSERT_HEAD(&pData->mbuf_zone_head, mzone, list);
    pData->mbuf_zone_count++;
    pData->mbuf_water_line_limit = pData->mbuf_zone_count * MBUF_ZONE_SIZE;
    return 0;
} 

void m_fini(PNATState pData)
{
    struct mbuf_zone *mz;
    struct mbuf *m;
    int i;
    void *zone;
    while(!LIST_EMPTY(&pData->mbuf_zone_head))
    {
        mz = LIST_FIRST(&pData->mbuf_zone_head);
        zone = mz->mbuf_zone_base_addr;
        for(i = 0; i < MBUF_ZONE_SIZE; ++i)
        {
            m = (struct mbuf *)((char *)zone + i*msize);
            if (   (m->m_flags & M_EXT) 
                && m->m_ext != NULL)
                RTMemFree(m->m_ext);
        }
        RTMemFree(zone);
        LIST_REMOVE(mz, list);
        RTMemFree(mz);
    }
}

void
m_init(PNATState pData)
{
    int i;
    struct mbuf *m;
    int rc = 0;
    m_freelist.m_next = m_freelist.m_prev = &m_freelist;
    m_usedlist.m_next = m_usedlist.m_prev = &m_usedlist;
    mbuf_alloced = 0;
    msize_init(pData);
#if 1
    rc = RTCritSectInit(&pData->cs_mbuf_zone);
    AssertReleaseRC(rc);
    rc = mbuf_zone_init(pData);
    Assert((rc == 0));
#endif
}

void
msize_init(PNATState pData)
{
    /*
     * Find a nice value for msize
     */
    msize = (if_mtu>if_mru ? if_mtu : if_mru)
          + sizeof(struct m_hdr) + sizeof(void *)   /*pointer to the backstore*/
          + if_maxlinkhdr ;
}
#ifdef m_get
# undef m_get
#endif

#ifdef m_free
# undef m_free
#endif
/*
 * Get an mbuf from the free list, if there are none
 * malloc one
 *
 * Because fragmentation can occur if we alloc new mbufs and
 * free old mbufs, we mark all mbufs above mbuf_thresh as M_DOFREE,
 * which tells m_free to actually free() it
 */
struct mbuf *
m_get(PNATState pData)
{
    register struct mbuf *m;
    int flags = 0;
    int rc = 0;

    DEBUG_CALL("m_get");
    
    rc = RTCritSectEnter(&pData->cs_mbuf_zone);
    AssertReleaseRC(rc);

recheck_zone:
    if (m_freelist.m_next == &m_freelist)
    {
#if 1
        int rc = mbuf_zone_init(pData);
        if (rc == 0)
            goto recheck_zone;
        AssertMsgFailed(("No mbufs on free list\n"));
        return NULL;
#else
        m = (struct mbuf *)RTMemAlloc(msize);
        if (m == NULL)
            goto end_error;
        mbuf_alloced++;
        if (mbuf_alloced > mbuf_thresh)
            flags = M_DOFREE;
        if (mbuf_alloced > mbuf_max)
            mbuf_max = mbuf_alloced;
#endif
    }
    else
    {
        m = m_freelist.m_next;
        remque(pData, m);
    }

    STAM_COUNTER_INC(&pData->StatMBufAllocation);
    /* Insert it in the used list */
    mbuf_alloced++;
    if (mbuf_alloced >= MBUF_ZONE_SIZE/2)
    {
        pData->fmbuf_water_line = 1;
    }
    insque(pData, m, &m_usedlist);
    m->m_flags = (flags | M_USEDLIST);

    /* Initialise it */
    m->m_size = msize - sizeof(struct m_hdr);
    m->m_data = m->m_dat;
    m->m_len = 0;
    m->m_nextpkt = 0;
    m->m_prevpkt = 0;
    m->m_la = NULL;
    memset(m->m_data, 0, if_maxlinkhdr); /*initialization of ether area */

end_error:
    DEBUG_ARG("m = %lx", (long )m);
    rc = RTCritSectLeave(&pData->cs_mbuf_zone);
    AssertReleaseRC(rc);
    return m;
}

void
m_free(PNATState pData, struct mbuf *m)
{
    int rc;
    DEBUG_CALL("m_free");
    DEBUG_ARG("m = %lx", (long )m);

    rc = RTCritSectEnter(&pData->cs_mbuf_zone);
    AssertReleaseRC(rc);
    mbuf_alloced--;
    if(m)
    {
        /* Remove from m_usedlist */
        if (m->m_flags & M_USEDLIST)
            remque(pData, m);

        /* If it's M_EXT, free() it */
        if (m->m_flags & M_EXT)
            RTMemFree(m->m_ext);

        /*
         * Either free() it or put it on the free list
         */
        if (m->m_flags & M_DOFREE)
        {
#if 1
            if ((m->m_flags & M_EXT) == 0)
                memset(m->m_dat, 0, if_mtu);
            insque(pData, m, &m_freelist);
            m->m_flags = M_FREELIST; /* Clobber other flags */
#else
            RTMemFree(m);
#endif
            mbuf_alloced--;
        }
        else if ((m->m_flags & M_FREELIST) == 0)
        {
            insque(pData, m,&m_freelist);
            m->m_flags = M_FREELIST; /* Clobber other flags */
        }
        STAM_COUNTER_INC(&pData->StatMBufAllocation);
    } /* if(m) */
    rc = RTCritSectLeave(&pData->cs_mbuf_zone);
    AssertReleaseRC(rc);
}

/* update macros for m_get/m_free*/
#undef m_get
#undef m_free
#include "mbuf.h"

/*
 * Copy data from one mbuf to the end of
 * the other.. if result is too big for one mbuf, malloc()
 * an M_EXT data segment
 */
void
m_cat(PNATState pData, register struct mbuf *m, register struct mbuf *n)
{
    /*
     * If there's no room, realloc
     */
    if (M_FREEROOM(m) < n->m_len)
        m_inc(m,m->m_size+MINCSIZE);

    memcpy(m->m_data+m->m_len, n->m_data, n->m_len);
    m->m_len += n->m_len;

    m_free(pData, n);
}


/* make m size bytes large */
void
m_inc(struct mbuf *m, int size)
{
    int datasize;

    /* some compiles throw up on gotos.  This one we can fake. */
    if (m->m_size > size)
        return;

    if (m->m_flags & M_EXT)
    {
        datasize = m->m_data - m->m_ext;
        m->m_ext = (char *)RTMemRealloc(m->m_ext, size);
#if 0
        if (m->m_ext == NULL)
            return (struct mbuf *)NULL;
#endif
        m->m_data = m->m_ext + datasize;
    }
    else
    {
        char *dat;
        datasize = m->m_data - m->m_dat;
        dat = (char *)RTMemAlloc(size);
#if 0
        if (dat == NULL)
            return (struct mbuf *)NULL;
#endif
        memcpy(dat, m->m_dat, m->m_size);

        m->m_ext = dat;
        m->m_data = m->m_ext + datasize;
        m->m_flags |= M_EXT;
    }

    m->m_size = size;
}


void
m_adj(struct mbuf *m, int len)
{
    if (m == NULL)
        return;
    if (len >= 0)
    {
        /* Trim from head */
        m->m_data += len;
        m->m_len -= len;
    }
    else
    {
        /* Trim from tail */
        len = -len;
        m->m_len -= len;
    }
    Assert(m->m_len >= 0);
}


/*
 * Copy len bytes from m, starting off bytes into n
 */
int
m_copy(struct mbuf *n, struct mbuf *m, int off, int len)
{
    if (len > M_FREEROOM(n))
        return -1;

    memcpy((n->m_data + n->m_len), (m->m_data + off), len);
    n->m_len += len;
    return 0;
}


/*
 * Given a pointer into an mbuf, return the mbuf
 * XXX This is a kludge, I should eliminate the need for it
 * Fortunately, it's not used often
 */
struct mbuf *
dtom(PNATState pData, void *dat)
{
    struct mbuf *m;

    DEBUG_CALL("dtom");
    DEBUG_ARG("dat = %lx", (long )dat);

    /* bug corrected for M_EXT buffers */
    for (m = m_usedlist.m_next; m != &m_usedlist; m = m->m_next)
    {
        if (m->m_flags & M_EXT)
        {
            if (   (char *)dat >=  m->m_ext
                && (char *)dat <  (m->m_ext + m->m_size))
                return m;
        }
        else
        {
            if (   (char *)dat >=  m->m_dat
                && (char *)dat <  (m->m_dat + m->m_size))
                return m;
        }
    }

    DEBUG_ERROR((dfd, "dtom failed"));

    return (struct mbuf *)0;
}
#ifndef VBOX_WITH_SLIRP_BSD_MBUF
void *slirp_ext_m_get(PNATState pData)
{
    return (void *)m_get(pData);
}

void slirp_ext_m_free(PNATState pData, void *arg)
{
    struct mbuf *m = (struct mbuf *)arg;
    m_free(pData, m);
}
void slirp_ext_m_append(PNATState pData, void *arg, uint8_t *pu8Buf, size_t cbBuf)
{
    char *c;
    struct mbuf *m = (struct mbuf *)arg;
    if (cbBuf > M_FREEROOM(m))
    {
        m_inc(m, cbBuf);
    } 
    c = mtod(m, char *);
    memcpy(c, pu8Buf, cbBuf);
    m->m_len = cbBuf;
}
#endif
