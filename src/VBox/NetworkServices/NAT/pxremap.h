/* -*- indent-tabs-mode: nil; -*- */
#ifndef _pxremap_h_
#define _pxremap_h_

#include "lwip/err.h"
#include "lwip/ip_addr.h"

struct netif;


#define PXREMAP_FAILED (-1)
#define PXREMAP_ASIS   0
#define PXREMAP_MAPPED 1

/* IPv4 */
#if ARP_PROXY
int pxremap_proxy_arp(struct netif *netif, ip_addr_t *dst);
#endif
int pxremap_ip4_divert(struct netif *netif, ip_addr_t *dst);
int pxremap_outbound_ip4(ip_addr_t *dst, ip_addr_t *src);
int pxremap_inbound_ip4(ip_addr_t *dst, ip_addr_t *src);

/* IPv6 */
int pxremap_proxy_na(struct netif *netif, ip6_addr_t *dst);
int pxremap_ip6_divert(struct netif *netif, ip6_addr_t *dst);
int pxremap_outbound_ip6(ip6_addr_t *dst, ip6_addr_t *src);
int pxremap_inbound_ip6(ip6_addr_t *dst, ip6_addr_t *src);

#define pxremap_outbound_ipX(is_ipv6, dst, src)                         \
    ((is_ipv6) ? pxremap_outbound_ip6(&(dst)->ip6, &(src)->ip6)         \
               : pxremap_outbound_ip4(&(dst)->ip4, &(src)->ip4))

#endif /* _pxremap_h_ */
