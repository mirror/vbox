#ifndef _nat_proxy_h_
#define _nat_proxy_h_

#if !defined(VBOX)
#include "vbox-compat.h"
#endif

#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "winutils.h"

/* forward */
struct netif;
struct tcpip_msg;
struct pbuf;
struct sockaddr;
struct sockaddr_in;
struct sockaddr_in6;

struct ip4_lomap
{
    ip_addr_t loaddr;
    uint32_t off;
};

struct ip4_lomap_desc
{
    const struct ip4_lomap *lomap;
    unsigned int num_lomap;
};

struct proxy_options {
    int ipv6_enabled;
    int ipv6_defroute;
    const char *tftp_root;
    const struct sockaddr_in *src4;
    const struct sockaddr_in6 *src6;
    const struct ip4_lomap_desc *lomap_desc;
    const char **nameservers;
};

extern volatile struct proxy_options *g_proxy_options;
extern struct netif *g_proxy_netif;

void proxy_init(struct netif *, struct proxy_options *);
SOCKET proxy_connected_socket(int, int, ipX_addr_t *, u16_t);
SOCKET proxy_bound_socket(int, int, struct sockaddr *);
void proxy_reset_socket(SOCKET);
void proxy_sendto(SOCKET, struct pbuf *, void *, size_t);
void proxy_lwip_post(struct tcpip_msg *);
const char *proxy_lwip_strerr(err_t);

/* proxy_rtadvd.c */
void proxy_rtadvd_start(struct netif *);
void proxy_rtadvd_do_quick(void *);

/* rtmon_*.c */
int rtmon_get_defaults(void);

/* proxy_dhcp6ds.c */
err_t dhcp6ds_init(struct netif *);

/* proxy_tftpd.c */
err_t tftpd_init(struct netif *, const char *);

/* pxtcp.c */
void pxtcp_init(void);

/* pxudp.c */
void pxudp_init(void);

/* pxdns.c */
err_t pxdns_init(struct netif *);
void pxdns_set_nameservers(void *);


#if defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS) || defined(RT_OS_WINDOWS)
# define HAVE_SA_LEN 0
#else
# define HAVE_SA_LEN 1
#endif

#define LWIP_ASSERT1(condition) LWIP_ASSERT(#condition, condition)
/* TODO: review debug levels and types */
#if !LWIP_PROXY_DEBUG
# define DPRINTF_LEVEL(y, x)      do {} while (0)
#else
# define DPRINTF_LEVEL(level, x) do { LWIP_DEBUGF(LWIP_PROXY_DEBUG | (level), x); } while (0)
#endif

#define DPRINTF(x) DPRINTF_LEVEL(0, x)
#define DPRINTF0(x) DPRINTF_LEVEL(LWIP_DBG_LEVEL_WARNING, x)
#define DPRINTF1(x) DPRINTF_LEVEL(LWIP_DBG_LEVEL_SERIOUS, x)
#define DPRINTF2(x) DPRINTF_LEVEL(LWIP_DBG_LEVEL_SEVERE, x)

#endif /* _nat_proxy_h_ */
