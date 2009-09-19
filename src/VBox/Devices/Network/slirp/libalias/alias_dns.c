#ifndef RT_OS_WINDOWS
# include <netdb.h>
#endif
# include <iprt/ctype.h>
# include <iprt/assert.h>
# include <slirp.h>
# include "alias.h"
# include "alias_local.h"
# include "alias_mod.h"

#define DNS_CONTROL_PORT_NUMBER 53
/* see RFC 1035(4.1.1) */
union dnsmsg_header
{
    struct {
        uint16_t id;
        uint16_t rd:1;
        uint16_t tc:1;
        uint16_t aa:1;
        uint16_t opcode:4;
        uint16_t qr:1;
        uint16_t rcode:4;
        uint16_t Z:3;
        uint16_t ra:1;
        uint16_t qdcount;
        uint16_t ancount;
        uint16_t nscount;
        uint16_t arcount;
    } X;
    uint16_t raw[5];
};
struct dnsmsg_answer
{
    uint16_t name;
    uint16_t type;
    uint16_t class;
    uint16_t ttl[2];
    uint16_t rdata_len;
    uint8_t rdata[1];  /*depends on value at rdata_len */
};

/* see RFC 1035(4.1) */
static int dns_alias_handler(PNATState pData, int type);
static void cstr2qstr(char *cstr, char *qstr);
static void qstr2cstr(char *qstr, char *cstr);

static int 
fingerprint(struct libalias *la, struct ip *pip, struct alias_data *ah)
{

    if (ah->dport == NULL || ah->sport == NULL || ah->lnk == NULL)
        return (-1);
    fprintf(stderr, "NAT:%s: ah(dport: %hd, sport: %hd) oaddr:%R[IP4] aaddr:%R[IP4]\n", 
        __FUNCTION__, ntohs(*ah->dport), ntohs(*ah->sport),
        &ah->oaddr, &ah->aaddr);
    if (   (ntohs(*ah->dport) == DNS_CONTROL_PORT_NUMBER
        || ntohs(*ah->sport) == DNS_CONTROL_PORT_NUMBER)
        && (ah->oaddr->s_addr == htonl(ntohl(la->special_addr.s_addr)|CTL_DNS)))
        return (0);
    return (-1);
}

static void doanswer(struct libalias *la, union dnsmsg_header *hdr,char *qname, struct ip *pip, struct hostent *h)
{
    int i;
    if (h == NULL)
    {
        hdr->X.qr = 1; /*response*/
        hdr->X.aa = 1;
        hdr->X.rd = 1;
        hdr->X.rcode = 3;
    } 
    else
    {
        /*!!! We need to be sure that */
        struct mbuf *m = NULL;
        char *query;
        char *answers;
        uint16_t off;
        char **cstr;
        char *c;
        uint16_t packet_len = 0;
        uint16_t addr_off = (uint16_t)~0;
        
#ifndef VBOX_WITH_SLIRP_BSD_MBUF
        m = dtom(la->pData, hdr); 
#else
        AssertMsgFailed(("Unimplemented"));
#endif
        Assert((m));
        
#if 0
        /*here is no compressed names+answers + new query*/
        m_inc(m, h->h_length * sizeof(struct dnsmsg_answer) + strlen(qname) + 2 * sizeof(uint16_t));
#endif
        packet_len = (pip->ip_hl << 2) + sizeof(struct udphdr) + sizeof(union dnsmsg_header) 
            + strlen(qname) +  2 * sizeof(uint16_t); /* ip + udp + header + query */
        fprintf(stderr,"got %d addresses for target:%s (m_len: %d)\n", h->h_length, h->h_name, m->m_len);
        query = (char *)&hdr[1];

        strcpy(query, qname);
        query += strlen(qname);
        query ++;
        
        *(uint16_t *)query = htons(1);
        ((uint16_t *)query)[1] = htons(1);
        answers = (char *)&((uint16_t *)query)[2];

        off = (char *)&hdr[1] - (char *)hdr;
        off |= (0x3 << 14);
        /*add aliases */
        cstr = h->h_aliases;
        while(*cstr) 
        {
            uint16_t len;
            struct dnsmsg_answer *ans = (struct dnsmsg_answer *)answers;
            ans->name = htons(off);
            ans->type = htons(5); /*CNAME*/
            ans->class = htons(1);
            *(uint32_t *)ans->ttl = htonl(3600); /* 1h */
            c = (addr_off == (uint16_t)~0?h->h_name : *cstr);
            len = strlen(c) + 2;
            ans->rdata_len = htons(len);
            ans->rdata[len - 1] = 0;
            cstr2qstr(c, (char *)ans->rdata);
            off = (char *)&ans->rdata - (char *)hdr;
            off |= (0x3 << 14);
            if (addr_off == (uint16_t)~0)
                addr_off = off;
            answers = (char *)&ans[1] + len - 2;  /* note: 1 symbol already counted */
            packet_len += sizeof(struct dnsmsg_answer) + len - 2;
            hdr->X.ancount++;
            cstr++;
        }
        /*add addresses */

        for(i = 0; i < h->h_length && h->h_addr_list[i] != NULL; ++i)
        {
            struct dnsmsg_answer *ans = (struct dnsmsg_answer *)answers;
            
            ans->name = htons(off);
            ans->type = htons(1);
            ans->class = htons(1);
            *(uint32_t *)ans->ttl = htonl(3600); /* 1h */
            ans->rdata_len = htons(4); /* IPv4 */
            *(uint32_t *)ans->rdata = *(uint32_t *)h->h_addr_list[i];
            answers = (char *)&ans[1] + 2;
            packet_len += sizeof(struct dnsmsg_answer) + 3;
            hdr->X.ancount++;
        }
        hdr->X.qr = 1; /*response*/
        hdr->X.aa = 1;
        hdr->X.rd = 1;
        hdr->X.ra = 1;
        hdr->X.rcode = 0;
        HTONS(hdr->X.ancount);
        /*don't forget update m_len*/
        m->m_len = packet_len;
        pip->ip_len = htons(m->m_len);
    }
}
static int 
protohandler(struct libalias *la, struct ip *pip, struct alias_data *ah)
{
    int i;
    /*Parse dns request */
    char *qw_qname = NULL;
    uint16_t *qw_qtype = NULL;
    uint16_t *qw_qclass = NULL;
    struct hostent *h = NULL;
    char *cname[255]; /* ??? */

    struct udphdr *udp = NULL;
    union dnsmsg_header *hdr = NULL;
    udp = (struct udphdr *)ip_next(pip);
    hdr = (union dnsmsg_header *)udp_next(udp);

    if (hdr->X.qr == 1)
        return 0; /* this is respose*/

    qw_qname = (char *)&hdr[1];
    Assert((ntohs(hdr->X.qdcount) == 1));

    for (i = 0; i < ntohs(hdr->X.qdcount); ++i)
    {
        qw_qtype = (uint16_t *)(qw_qname + strlen(qw_qname) + 1);
        qw_qclass = &qw_qtype[1];
        fprintf(stderr, "qname:%s qtype:%hd qclass:%hd\n", 
            qw_qname, ntohs(*qw_qtype), ntohs(*qw_qclass));
    }
    qstr2cstr(qw_qname, (char *)cname);
    h = gethostbyname((char *)cname);
    fprintf(stderr, "cname:%s\n", cname);
    doanswer(la, hdr, qw_qname, pip, h);
    /*we've chenged size and conten of udp, to avoid double csum calcualtion 
     *will assign to zero
     */
    udp->uh_sum = 0;
    udp->uh_ulen = ntohs(htons(pip->ip_len) - (pip->ip_hl << 2));
    pip->ip_sum = 0;
    pip->ip_sum = LibAliasInternetChecksum(la, (uint16_t *)pip, pip->ip_hl << 2);
    return (0);
}
/*
 * qstr is z-string with -dot- replaced with \count to next -dot-
 * e.g. ya.ru is \02ya\02ru
 * Note: it's assumed that caller allocates buffer for cstr
 */
static void qstr2cstr(char *qname, char *cname)
{
    char *q = qname;
    char *c = cname;
    while(*q != 0)
    {
        if (isalpha(*q) || isdigit(*q))
        {
           *c = *q; 
            c++;
        }
        else if (c != &cname[0])
        {
            *c = '.';
            c++;
        }
        q++;
    }
    q = 0;
}
/*
 *
 */
static void cstr2qstr(char *cstr, char *qstr)
{
    char *c, *pc, *q;
    c = cstr;
    q = qstr; 
    while(*c != 0)
    {
        /* a the begining or at -dot- position */
        if (*c == '.' || (c == cstr && q == qstr)) 
        {
            if (c != cstr) c++;
            pc = strchr(c, '.');
            *q = pc != NULL ? (pc - c) : strlen(c);
            q++;
            continue;
        }
        (*q) = (*c); /*direct copy*/
        q++;
        c++;
    }
    q = 0;
}


int
dns_alias_load(PNATState pData)
{
    return dns_alias_handler(pData, MOD_LOAD);
}

int
dns_alias_unload(PNATState pData)
{
    return dns_alias_handler(pData, MOD_UNLOAD);
}

#define handlers pData->dns_module
static int
dns_alias_handler(PNATState pData, int type)
{
    int error;
    if (handlers == NULL)
        handlers = RTMemAllocZ(2 * sizeof(struct proto_handler));
    handlers[0].pri = 20;
    handlers[0].dir = IN;
    handlers[0].proto = UDP;
    handlers[0].fingerprint = &fingerprint;
    handlers[0].protohandler = &protohandler;
    handlers[1].pri = EOH;

    switch (type) {   
    case MOD_LOAD:
        error = 0;
        LibAliasAttachHandlers(pData, handlers);
        break;
    case MOD_UNLOAD:
        error = 0;
        LibAliasDetachHandlers(pData, handlers);
        RTMemFree(handlers);
        handlers = NULL;
        break;
    default:
        error = EINVAL;
    }
    return (error);
}
