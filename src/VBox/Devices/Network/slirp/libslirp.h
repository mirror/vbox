#ifndef _LIBSLIRP_H
#define _LIBSLIRP_H

#ifdef _WIN32
#include <winsock2.h>
#if defined(VBOX) && defined(__cplusplus)
extern "C" {
#endif
int inet_aton(const char *cp, struct in_addr *ia);
#if defined(VBOX) && defined(__cplusplus)
}
#endif
#else
#if defined(VBOX) && defined(__OS2__) /* temporary workaround, see ticket #127 */
# include <sys/time.h>
#endif
#include <sys/select.h>
#include <arpa/inet.h>
#endif

#ifdef VBOX
#include <VBox/types.h>

typedef struct NATState *PNATState;
#endif /* VBOX */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef VBOX
void slirp_init(void);
#else /* VBOX */
int slirp_init(PNATState *, const char *, void *);
void slirp_term(PNATState);
void slirp_link_up(PNATState);
void slirp_link_down(PNATState);
# if ARCH_BITS == 64
error fix the last remaining global variables.....
extern uint32_t g_cpvHashUsed;
extern uint32_t g_cpvHashCollisions;
extern uint64_t g_cpvHashInserts;
extern uint64_t g_cpvHashDone;
# endif
#endif /* VBOX */

#ifdef VBOX
void slirp_select_fill(PNATState pData, int *pnfds,
                       fd_set *readfds, fd_set *writefds, fd_set *xfds);

void slirp_select_poll(PNATState pData, fd_set *readfds, fd_set *writefds, fd_set *xfds);

void slirp_input(PNATState pData, const uint8_t *pkt, int pkt_len);
#else /* !VBOX */
void slirp_select_fill(int *pnfds,
                       fd_set *readfds, fd_set *writefds, fd_set *xfds);

void slirp_select_poll(fd_set *readfds, fd_set *writefds, fd_set *xfds);

void slirp_input(const uint8_t *pkt, int pkt_len);
#endif /* !VBOX */

#ifdef VBOX
/* you must provide the following functions: */
int slirp_can_output(void * pvUser);
void slirp_output(void * pvUser, const uint8_t *pkt, int pkt_len);

int slirp_redir(PNATState pData, int is_udp, int host_port,
                struct in_addr guest_addr, int guest_port);
int slirp_add_exec(PNATState pData, int do_pty, const char *args, int addr_low_byte,
                   int guest_port);
#else /* !VBOX */
/* you must provide the following functions: */
int slirp_can_output(void);
void slirp_output(const uint8_t *pkt, int pkt_len);

int slirp_redir(int is_udp, int host_port,
                struct in_addr guest_addr, int guest_port);
int slirp_add_exec(int do_pty, const char *args, int addr_low_byte,
                   int guest_port);
#endif /* !VBOX */

#ifndef VBOX
extern const char *tftp_prefix;
extern char slirp_hostname[33];
#endif /* !VBOX */

#ifdef __cplusplus
}
#endif

#endif
