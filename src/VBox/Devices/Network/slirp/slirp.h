#ifndef __COMMON_H__
#define __COMMON_H__

#include <VBox/stam.h>

#ifdef RT_OS_WINDOWS
# include <winsock2.h>
# include <ws2tcpip.h>
typedef int socklen_t;
#endif
#ifdef RT_OS_OS2 /* temporary workaround, see ticket #127 */
# define mbstat mbstat_os2
# include <sys/socket.h>
# undef mbstat
typedef int socklen_t;
#endif

#define CONFIG_QEMU

#ifdef DEBUG
# undef  DEBUG
# define DEBUG 1
#endif

#ifndef CONFIG_QEMU
# include "version.h"
#endif
#define LOG_GROUP LOG_GROUP_DRV_NAT
#include <VBox/log.h>
#include <iprt/mem.h>
#ifdef RT_OS_WINDOWS
# include <windows.h>
# include <io.h>
#endif
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/dir.h>
#include <iprt/rand.h>
#include <VBox/types.h>

#undef malloc
#define malloc          dont_use_malloc
#undef free
#define free            dont_use_free
#undef realloc
#define realloc         dont_use_realloc
#undef strdup
#define strdup          dont_use_strdup

#include "slirp_config.h"

#ifdef RT_OS_WINDOWS

# ifndef _MSC_VER
#  include <inttypes.h>
# endif

typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;
typedef char *caddr_t;

# include <sys/timeb.h>
# include <iphlpapi.h>

# define EWOULDBLOCK WSAEWOULDBLOCK
# define EINPROGRESS WSAEINPROGRESS
# define ENOTCONN WSAENOTCONN
# define EHOSTUNREACH WSAEHOSTUNREACH
# define ENETUNREACH WSAENETUNREACH
# define ECONNREFUSED WSAECONNREFUSED

#else /* !RT_OS_WINDOWS */

# define ioctlsocket ioctl
# define closesocket(s) close(s)
# define O_BINARY 0

#endif /* !RT_OS_WINDOWS */

#include <sys/types.h>
#ifdef HAVE_SYS_BITYPES_H
# include <sys/bitypes.h>
#endif

#ifdef _MSC_VER
# include <time.h>
#else /* !_MSC_VER */
# include <sys/time.h>
#endif /* !_MSC_VER */

#ifdef NEED_TYPEDEFS
typedef char int8_t;
typedef unsigned char u_int8_t;

# if SIZEOF_SHORT == 2
    typedef short int16_t;
    typedef unsigned short u_int16_t;
# else
#  if SIZEOF_INT == 2
    typedef int int16_t;
    typedef unsigned int u_int16_t;
#  else
    #error Cannot find a type with sizeof() == 2
#  endif
# endif

# if SIZEOF_SHORT == 4
   typedef short int32_t;
   typedef unsigned short u_int32_t;
# else
#  if SIZEOF_INT == 4
    typedef int int32_t;
    typedef unsigned int u_int32_t;
#  else
    #error Cannot find a type with sizeof() == 4
#  endif
# endif
#endif /* NEED_TYPEDEFS */

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include <stdio.h>
#include <errno.h>

#ifndef HAVE_MEMMOVE
# define memmove(x, y, z) bcopy(y, x, z)
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif

#ifndef RT_OS_WINDOWS
# include <sys/uio.h>
#endif

#ifndef RT_OS_WINDOWS
# include <netinet/in.h>
# include <arpa/inet.h>
#endif

#ifdef GETTIMEOFDAY_ONE_ARG
# define gettimeofday(x, y) gettimeofday(x)
#endif

#ifndef HAVE_INET_ATON
int inet_aton (const char *cp, struct in_addr *ia);
#endif

#include <fcntl.h>
#ifndef NO_UNIX_SOCKETS
# include <sys/un.h>
#endif
#include <signal.h>
#ifdef HAVE_SYS_SIGNAL_H
# include <sys/signal.h>
#endif
#ifndef RT_OS_WINDOWS
# include <sys/socket.h>
#endif

#if defined(HAVE_SYS_IOCTL_H)
# include <sys/ioctl.h>
#endif

#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif

#if defined(__STDC__) || defined(_MSC_VER)
# include <stdarg.h>
#else
# include <varargs.h>
#endif

#include <sys/stat.h>

/* Avoid conflicting with the libc insque() and remque(), which
 * have different prototypes. */
#define insque slirp_insque
#define remque slirp_remque

#ifdef HAVE_SYS_STROPTS_H
# include <sys/stropts.h>
#endif

#include "libslirp.h"

#include "debug.h"

#include "ip.h"
#include "tcp.h"
#include "tcp_timer.h"
#include "tcp_var.h"
#include "tcpip.h"
#include "udp.h"
#include "icmp_var.h"
#include "mbuf.h"
#include "sbuf.h"
#include "socket.h"
#include "if.h"
#include "main.h"
#include "misc.h"
#include "ctl.h"
#include "bootp.h"
#include "tftp.h"

#include "slirp_state.h"

#undef PVM /* XXX Mac OS X hack */

#ifndef NULL
# define NULL (void *)0
#endif

void if_start (PNATState);

#ifndef HAVE_INDEX
 char *index (const char *, int);
#endif

#ifndef HAVE_GETHOSTID
 long gethostid (void);
#endif

#ifndef RT_OS_WINDOWS
#include <netdb.h>
#endif

#include "dnsproxy/dnsproxy.h"

#define DEFAULT_BAUD 115200

int get_dns_addr(PNATState pData, struct in_addr *pdns_addr);

/* cksum.c */
int cksum(struct mbuf *m, int len);

/* if.c */
void if_init (PNATState);
void if_output (PNATState, struct socket *, struct mbuf *);

/* ip_input.c */
void ip_init (PNATState);
void ip_input (PNATState, struct mbuf *);
struct mbuf * ip_reass (PNATState, register struct mbuf *);
void ip_freef (PNATState, struct ipqhead *, struct ipq_t *);
void ip_slowtimo (PNATState);
void ip_stripoptions (register struct mbuf *, struct mbuf *);

/* ip_output.c */
int ip_output (PNATState, struct socket *, struct mbuf *);

/* tcp_input.c */
int tcp_reass (PNATState, struct tcpcb *, struct tcphdr *, int *, struct mbuf *);
void tcp_input (PNATState, register struct mbuf *, int, struct socket *);
void tcp_dooptions (PNATState, struct tcpcb *, u_char *, int, struct tcpiphdr *);
void tcp_xmit_timer (PNATState, register struct tcpcb *, int);
int tcp_mss (PNATState, register struct tcpcb *, u_int);

/* tcp_output.c */
int tcp_output (PNATState, register struct tcpcb *);
void tcp_setpersist (register struct tcpcb *);

/* tcp_subr.c */
void tcp_init (PNATState);
void tcp_template (struct tcpcb *);
void tcp_respond (PNATState, struct tcpcb *, register struct tcpiphdr *, register struct mbuf *, tcp_seq, tcp_seq, int);
struct tcpcb * tcp_newtcpcb (PNATState, struct socket *);
struct tcpcb * tcp_close (PNATState, register struct tcpcb *);
void tcp_drain (void);
void tcp_sockclosed (PNATState, struct tcpcb *);
int tcp_fconnect (PNATState, struct socket *);
void tcp_connect (PNATState, struct socket *);
int tcp_attach (PNATState, struct socket *);
u_int8_t tcp_tos (struct socket *);
int tcp_emu (PNATState, struct socket *, struct mbuf *);
int tcp_ctl (PNATState, struct socket *);
struct tcpcb *tcp_drop(PNATState, struct tcpcb *tp, int err);

/*slirp.c*/
void slirp_arp_who_has(PNATState pData, uint32_t dst);
int slirp_arp_cache_update(PNATState pData, uint32_t dst, const uint8_t *mac);
void slirp_arp_cache_add(PNATState pData, uint32_t ip, const uint8_t *ether);
#define MIN_MRU 128
#define MAX_MRU 16384

#ifndef RT_OS_WINDOWS
# define min(x, y) ((x) < (y) ? (x) : (y))
# define max(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifdef RT_OS_WINDOWS
# undef errno
# if 0 /* debugging */
int errno_func(const char *file, int line);
#  define errno (errno_func(__FILE__, __LINE__))
# else
#  define errno (WSAGetLastError())
# endif
#endif

# define ETH_ALEN        6
# define ETH_HLEN        14

# define ARPOP_REQUEST   1               /* ARP request                  */
# define ARPOP_REPLY     2               /* ARP reply                    */

struct ethhdr
{
    unsigned char   h_dest[ETH_ALEN];           /* destination eth addr */
    unsigned char   h_source[ETH_ALEN];         /* source ether addr    */
    unsigned short  h_proto;                    /* packet type ID field */
};
AssertCompileSize(struct ethhdr, 14);
#if defined(VBOX_SLIRP_ALIAS)

# define ip_next(ip) (void *)((uint8_t *)(ip) + ((ip)->ip_hl << 2))
# define udp_next(udp) (void *)((uint8_t *)&((struct udphdr *)(udp))[1] )
# define bcopy(src, dst, len) memcpy((dst), (src), (len))
# define bcmp(a1, a2, len) memcmp((a1), (a2), (len))
# define NO_FW_PUNCH

# ifdef alias_addr
#  error  alias_addr has already defined!!!
# endif

# define arc4random() RTRandU32()
# undef malloc
# undef calloc
# undef free
# define	malloc(x) RTMemAlloc((x))
# define	calloc(x, n) RTMemAllocZ((x)*(n))
# define	free(x)	RTMemFree((x))
# ifndef __unused
#  define __unused
# endif

# define strncasecmp RTStrNICmp

# ifdef DEBUG
# define LIBALIAS_DEBUG
# ifdef fprintf
#  undef fprintf
# endif
# ifdef fflush
#  undef fflush
# endif
# ifdef printf
#  undef printf
# endif
#define fflush(x) do{}while(0)
# define fprintf vbox_slirp_fprintf
# define printf vbox_slirp_printf
static void vbox_slirp_printV(char *format, va_list args)
{
    char buffer[1024];
    memset(buffer, 0, 1024);
    RTStrPrintfV(buffer, 1024, format, args);

#if defined(DEBUG_vvl) && 0
    LogRel(("NAT:ALIAS: %s\n", buffer));
#else
    Log2(("NAT:ALIAS: %s\n", buffer));
#endif
}
static void vbox_slirp_printf(char *format, ...)
{
    va_list args;
    va_start(args, format);
    vbox_slirp_printV(format, args);
    va_end(args);
}
static void vbox_slirp_fprintf(void *ignored, char *format, ...)
{
    va_list args;
    va_start(args, format);
    vbox_slirp_printV(format, args);
    va_end(args);
}
# endif /* DEBUG */
#endif /*VBOX_SLIRP_ALIAS*/

int ftp_alias_load(PNATState);
int ftp_alias_unload(PNATState);
int nbt_alias_load(PNATState);
int nbt_alias_unload(PNATState);
#endif

