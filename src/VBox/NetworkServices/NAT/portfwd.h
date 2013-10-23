/* -*- indent-tabs-mode: nil; -*- */
#ifndef _portfwd_h_
#define _portfwd_h_

#ifndef RT_OS_WINDOWS
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include "lwip/ip_addr.h"


struct fwspec {
    int sdom;                   /* PF_INET, PF_INET6 */
    int stype;                  /* SOCK_STREAM, SOCK_DGRAM */

    /* listen on */
    union {
        struct sockaddr sa;
        struct sockaddr_in sin;   /* sdom == PF_INET  */
        struct sockaddr_in6 sin6; /* sdom == PF_INET6 */
    } src;

    /* forward to */
    union {
        struct sockaddr sa;
        struct sockaddr_in sin;   /* sdom == PF_INET  */
        struct sockaddr_in6 sin6; /* sdom == PF_INET6 */
    } dst;
};


void portfwd_init(void);
int portfwd_rule_add(struct fwspec *);
int portfwd_rule_del(struct fwspec *);


int fwspec_set(struct fwspec *, int, int,
               const char *, uint16_t,
               const char *, uint16_t);

int fwspec_equal(struct fwspec *, struct fwspec *);

void fwtcp_init(void);
void fwudp_init(void);

void fwtcp_add(struct fwspec *);
void fwtcp_del(struct fwspec *);
void fwudp_add(struct fwspec *);
void fwudp_del(struct fwspec *);

int fwany_ipX_addr_set_src(ipX_addr_t *, const struct sockaddr *);

#endif /* _portfwd_h_ */
