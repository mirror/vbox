#ifndef _LIBSLIRP_H
#define _LIBSLIRP_H

#ifdef _WIN32
#include <winsock2.h>
#ifdef __cplusplus
extern "C" {
#endif
int inet_aton(const char *cp, struct in_addr *ia);
#ifdef __cplusplus
}
#endif
#else
#ifdef RT_OS_OS2 /* temporary workaround, see ticket #127 */
# include <sys/time.h>
#endif
#include <sys/select.h>
#include <arpa/inet.h>
#endif

#include <VBox/types.h>

typedef struct NATState *PNATState;

#ifdef __cplusplus
extern "C" {
#endif

int slirp_init(PNATState *, const char *, uint32_t, bool, const char *, const char *, void *);
void slirp_term(PNATState);
void slirp_link_up(PNATState);
void slirp_link_down(PNATState);

void slirp_select_fill(PNATState pData, int *pnfds,
                       fd_set *readfds, fd_set *writefds, fd_set *xfds);

void slirp_select_poll(PNATState pData, fd_set *readfds, fd_set *writefds, fd_set *xfds);

void slirp_input(PNATState pData, const uint8_t *pkt, int pkt_len);
void slirp_set_ethaddr(PNATState pData, const uint8_t *ethaddr);

/* you must provide the following functions: */
int slirp_can_output(void * pvUser);
void slirp_output(void * pvUser, const uint8_t *pkt, int pkt_len);

int slirp_redir(PNATState pData, int is_udp, int host_port,
                struct in_addr guest_addr, int guest_port);
int slirp_add_exec(PNATState pData, int do_pty, const char *args, int addr_low_byte,
                   int guest_port);


#ifdef __cplusplus
}
#endif

#endif
