/*
 * Copyright (c) 1995 Danny Gasparovski.
 *
 * Please read the file COPYRIGHT for the
 * terms and conditions of the copyright.
 */

#define WANT_SYS_IOCTL_H
#include <slirp.h>

#ifndef HAVE_INET_ATON
int
inet_aton(const char *cp, struct in_addr *ia)
{
    u_int32_t addr = inet_addr(cp);
    if (addr == 0xffffffff)
        return 0;
    ia->s_addr = addr;
    return 1;
}
#endif

/*
 * Get our IP address and put it in our_addr
 */
void
getouraddr(PNATState pData)
{
    our_addr.s_addr = loopback_addr.s_addr;
}

struct quehead
{
    struct quehead *qh_link;
    struct quehead *qh_rlink;
};

void
insque(PNATState pData, void *a, void *b)
{
    register struct quehead *element = (struct quehead *) a;
    register struct quehead *head = (struct quehead *) b;
    element->qh_link = head->qh_link;
    head->qh_link = (struct quehead *)element;
    element->qh_rlink = (struct quehead *)head;
    ((struct quehead *)(element->qh_link))->qh_rlink = (struct quehead *)element;
}

void
remque(PNATState pData, void *a)
{
    register struct quehead *element = (struct quehead *) a;
    ((struct quehead *)(element->qh_link))->qh_rlink = element->qh_rlink;
    ((struct quehead *)(element->qh_rlink))->qh_link = element->qh_link;
    element->qh_rlink = NULL;
    /*  element->qh_link = NULL;  TCP FIN1 crashes if you do this.  Why ? */
}

int
add_exec(struct ex_list **ex_ptr, int do_pty, char *exec, int addr, int port)
{
    struct ex_list *tmp_ptr;

    /* First, check if the port is "bound" */
    for (tmp_ptr = *ex_ptr; tmp_ptr; tmp_ptr = tmp_ptr->ex_next)
    {
        if (port == tmp_ptr->ex_fport && addr == tmp_ptr->ex_addr)
            return -1;
    }

    tmp_ptr = *ex_ptr;
    *ex_ptr = (struct ex_list *)RTMemAlloc(sizeof(struct ex_list));
    (*ex_ptr)->ex_fport = port;
    (*ex_ptr)->ex_addr = addr;
    (*ex_ptr)->ex_pty = do_pty;
    (*ex_ptr)->ex_exec = RTStrDup(exec);
    (*ex_ptr)->ex_next = tmp_ptr;
    return 0;
}


/*
 * Set fd blocking and non-blocking
 */
void
fd_nonblock(int fd)
{
#ifdef FIONBIO
    int opt = 1;

    ioctlsocket(fd, FIONBIO, &opt);
#else
    int opt;

    opt = fcntl(fd, F_GETFL, 0);
    opt |= O_NONBLOCK;
    fcntl(fd, F_SETFL, opt);
#endif
}

void
fd_block(int fd)
{
#ifdef FIONBIO
    int opt = 0;

    ioctlsocket(fd, FIONBIO, &opt);
#else
    int opt;

    opt = fcntl(fd, F_GETFL, 0);
    opt &= ~O_NONBLOCK;
    fcntl(fd, F_SETFL, opt);
#endif
}

#ifdef VBOX_WITH_SLIRP_BSD_MBUF
#define ITEM_MAGIC 0xdead0001
struct item
{
    uint32_t magic;
    uma_zone_t zone;
    uint32_t ref_count;
    LIST_ENTRY(item) list;
};

#define ZONE_MAGIC 0xdead0002
struct uma_zone
{
    uint32_t magic;
    PNATState pData; /* to minimize changes in the rest of UMA emulation code */
    RTCRITSECT csZone;
    const char *name;
    size_t size; /* item size */
    ctor_t pfCtor;
    dtor_t pfDtor;
    zinit_t pfInit;
    zfini_t pfFini;
    uma_alloc_t pfAlloc;
    uma_free_t pfFree;
    int max_items;
    int cur_items;
    LIST_HEAD(RT_NOTHING, item) used_items;
    LIST_HEAD(RT_NOTHING, item) free_items;
    uma_zone_t master_zone;
};


static void *slirp_uma_alloc(uma_zone_t zone,
    int size, uint8_t *pflags, int wait)
{
    struct item *it;
    RTCritSectEnter(&zone->csZone);
    if (!LIST_EMPTY(&zone->free_items))
    {
        /*
         * @todo (r=vvl) here should be some
         * accounting of extra items in case
         * breakthrough barrier
         */
        if (LIST_EMPTY(&zone->free_items))
        {
            RTCritSectLeave(&zone->csZone);
            return NULL;
        }
        it = LIST_FIRST(&zone->free_items);
        LIST_REMOVE(it, list);
        LIST_INSERT_HEAD(&zone->used_items, it, list);
        goto allocated;
    }

    /*@todo 'Z' should be depend on flag */
    it = RTMemAllocZ(sizeof(struct item) + zone->size);
    if (it == NULL)
    {
        Log(("NAT: uma no memory"));
        RTCritSectLeave(&zone->csZone);
        return NULL;
    }
    it->magic = ITEM_MAGIC;
    LIST_INSERT_HEAD(&zone->used_items, it, list);
    zone->cur_items++;
    it->zone = zone;
    if (zone->cur_items >= zone->max_items)
        LogRel(("NAT: zone(%s) has reached it maximum\n", zone->name));

    allocated:
    if (zone->pfInit)
        zone->pfInit(zone->pData, (void *)&it[1], zone->size, M_DONTWAIT);
    RTCritSectLeave(&zone->csZone);
    return (void *)&it[1];
}

static void slirp_uma_free(void *item, int size, uint8_t flags)
{
    struct item *it;
    uma_zone_t zone;
    Assert(item);
    it = &((struct item *)item)[-1];
    Assert(it->magic == ITEM_MAGIC);
    zone = it->zone;
    RTCritSectEnter(&zone->csZone);
    Assert(zone->magic == ZONE_MAGIC);
    LIST_REMOVE(it, list);
    LIST_INSERT_HEAD(&zone->free_items, it, list);
    zone->cur_items--;
    RTCritSectLeave(&zone->csZone);
}

uma_zone_t uma_zcreate(PNATState pData, char *name, size_t size,
    ctor_t ctor, dtor_t dtor, zinit_t init, zfini_t fini, int flags1, int flags2)
{
    uma_zone_t zone = RTMemAllocZ(sizeof(struct uma_zone) + size);
    Assert((pData));
    zone->magic = ZONE_MAGIC;
    zone->pData = pData;
    zone->name = name;
    zone->size = size;
    zone->pfCtor = ctor;
    zone->pfDtor = dtor;
    zone->pfInit = init;
    zone->pfFini = fini;
    zone->pfAlloc = slirp_uma_alloc;
    zone->pfFree = slirp_uma_free;
    RTCritSectInit(&zone->csZone);
    return zone;

}
uma_zone_t uma_zsecond_create(char *name, ctor_t ctor,
    dtor_t dtor, zinit_t init, zfini_t fini, uma_zone_t master)
{
    uma_zone_t zone;
#if 0
    if (master->pfAlloc != NULL)
        zone = (uma_zone_t)master->pfAlloc(master, sizeof(struct uma_zone), NULL, 0);
#endif
    zone = RTMemAllocZ(sizeof(struct uma_zone));
    if (zone == NULL)
    {
        return NULL;
    }
    Assert((master && master->pData));
    zone->magic = ZONE_MAGIC;
    zone->pData = master->pData;
    zone->name = name;
    zone->pfCtor = ctor;
    zone->pfDtor = dtor;
    zone->pfInit = init;
    zone->pfFini = fini;
    zone->pfAlloc = slirp_uma_alloc;
    zone->pfFree = slirp_uma_free;
    zone->size = master->size;
    RTCritSectInit(&zone->csZone);
    return zone;
}
void uma_zone_set_max(uma_zone_t zone, int max)
{
    zone->max_items = max;
}
void uma_zone_set_allocf(uma_zone_t zone, uma_alloc_t pfAlloc)
{
   zone->pfAlloc = pfAlloc;
}
void uma_zone_set_freef(uma_zone_t zone, uma_free_t pfFree)
{
   zone->pfFree = pfFree;
}

uint32_t *uma_find_refcnt(uma_zone_t zone, void *mem)
{
    /*@todo (vvl) this function supposed to work with special zone storing
    reference counters */
    struct item *it = (struct item *)mem; /* 1st element */
    Assert(mem != NULL);
    Assert(zone->magic == ZONE_MAGIC);
    /* for returning pointer to counter we need get 0 elemnt */
    Assert(it[-1].magic == ITEM_MAGIC);
    return &it[-1].ref_count;
}
void *uma_zalloc_arg(uma_zone_t zone, void *args, int how)
{
    void *mem;
    Assert(zone->magic == ZONE_MAGIC);
    if (zone->pfAlloc == NULL)
        return NULL;
    RTCritSectEnter(&zone->csZone);
    mem = zone->pfAlloc(zone, zone->size, NULL, 0);
    if (zone->pfCtor)
        zone->pfCtor(zone->pData, mem, zone->size, args, M_DONTWAIT);
    RTCritSectLeave(&zone->csZone);
    return mem;
}

void uma_zfree(uma_zone_t zone, void *item)
{
    uma_zfree_arg(zone, item, NULL);
}

void uma_zfree_arg(uma_zone_t zone, void *mem, void *flags)
{
    struct item *it;
    Assert(zone->magic == ZONE_MAGIC);
    if (zone->pfFree == NULL)
        return;
    Assert((mem));
    RTCritSectEnter(&zone->csZone);
    it = &((struct item *)mem)[-1];
    if (it->magic != ITEM_MAGIC)
    {
        Log(("NAT:UMA: %p seems to be allocated on heap ... freeing\n", mem));
        RTMemFree(mem);
        RTCritSectLeave(&zone->csZone);
        return;
    }
    Assert((zone->magic == ZONE_MAGIC && zone == it->zone));

    if (zone->pfDtor)
        zone->pfDtor(zone->pData, mem, zone->size, flags);
    zone->pfFree(mem,  0, 0);
    RTCritSectLeave(&zone->csZone);
}
int uma_zone_exhausted_nolock(uma_zone_t zone)
{
    return 0;
}
void zone_drain(uma_zone_t zone)
{
}

void slirp_null_arg_free(void *mem, void *arg)
{
    /*@todo (r=vvl) make it wiser*/
    Assert(mem);
    RTMemFree(mem);
}

void *uma_zalloc(uma_zone_t zone, int len)
{
    return NULL;
}

struct mbuf *slirp_ext_m_get(PNATState pData, size_t cbMin, void **ppvBuf, size_t *pcbBuf)
{
    struct mbuf *m;
    size_t size = MCLBYTES;
    if (cbMin < MSIZE)
        size = MCLBYTES;
    else if (cbMin < MCLBYTES)
        size = MCLBYTES;
    else if (cbMin < MJUM9BYTES)
        size = MJUM9BYTES;
    else if (cbMin < MJUM16BYTES)
        size = MJUM16BYTES;
    else
        AssertMsgFailed(("Unsupported size"));

    m = m_getjcl(pData, M_NOWAIT, MT_HEADER, M_PKTHDR, size);
    m->m_len = size ;
    *ppvBuf = mtod(m, void *);
    *pcbBuf = size;
    return m;
}

void slirp_ext_m_free(PNATState pData, struct mbuf *m)
{
    m_free(pData, m);
}

static void zone_destroy(uma_zone_t zone)
{
    struct item *it;
    RTCritSectEnter(&zone->csZone);
    LogRel(("NAT: zone(nm:%s, used:%d)\n", zone->name, zone->cur_items));
    /* freeing */
    while (!LIST_EMPTY(&zone->free_items))
    {
        it = LIST_FIRST(&zone->free_items);
        LIST_REMOVE(it, list);
        RTMemFree(it);
    }
    while (zone->master_zone == NULL && !LIST_EMPTY(&zone->used_items))
    {
        it = LIST_FIRST(&zone->used_items);
        LIST_REMOVE(it, list);
        RTMemFree(it);
    }
    RTCritSectLeave(&zone->csZone);
    RTCritSectDelete(&zone->csZone);
    RTMemFree(zone);
}
void m_fini(PNATState pData)
{
    zone_destroy(pData->zone_mbuf);
    zone_destroy(pData->zone_clust);
    zone_destroy(pData->zone_pack);
    zone_destroy(pData->zone_jumbop);
    zone_destroy(pData->zone_jumbo9);
    zone_destroy(pData->zone_jumbo16);
    /*@todo do finalize here.*/
}
#endif /* VBOX_WITH_SLIRP_BSD_MBUF */
