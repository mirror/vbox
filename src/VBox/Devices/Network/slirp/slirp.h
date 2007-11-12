#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef RT_OS_WINDOWS
# include <winsock2.h>
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
#include "version.h"
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
#include <VBox/types.h>

# define malloc(a)       RTMemAllocZ(a)
# define free(a)         RTMemFree(a)
# define realloc(a,b)    RTMemRealloc(a, b)

#include "slirp_config.h"

#ifdef _WIN32

#ifndef _MSC_VER
# include <inttypes.h>
#endif

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
#else
# define ioctlsocket ioctl
# define closesocket(s) close(s)
# define O_BINARY 0
#endif

#include <sys/types.h>
#ifdef HAVE_SYS_BITYPES_H
# include <sys/bitypes.h>
#endif

#ifdef _MSC_VER
#include <time.h>
#else /* !_MSC_VER */
#include <sys/time.h>
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
#define memmove(x, y, z) bcopy(y, x, z)
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

#ifndef _WIN32
#include <sys/uio.h>
#endif

#ifndef _P
#ifndef NO_PROTOTYPES
#  define   _P(x)   x
#else
#  define   _P(x)   ()
#endif
#endif

#ifndef _WIN32
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#ifdef GETTIMEOFDAY_ONE_ARG
#define gettimeofday(x, y) gettimeofday(x)
#endif

/* Systems lacking strdup() definition in <string.h>. */
#if defined(ultrix)
char *strdup _P((const char *));
#endif

/* Systems lacking malloc() definition in <stdlib.h>. */
#if defined(ultrix) || defined(hcx)
void *malloc _P((size_t arg));
void free _P((void *ptr));
#endif

#ifndef HAVE_INET_ATON
int inet_aton _P((const char *cp, struct in_addr *ia));
#endif

#include <fcntl.h>
#ifndef NO_UNIX_SOCKETS
#include <sys/un.h>
#endif
#include <signal.h>
#ifdef HAVE_SYS_SIGNAL_H
# include <sys/signal.h>
#endif
#ifndef _WIN32
#include <sys/socket.h>
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

#ifdef USE_PPP
#include <ppp/slirppp.h>
#endif

#if defined(__STDC__) || defined(_MSC_VER)
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include <sys/stat.h>

#if 1 /* ndef _MSC_VER */
/* Avoid conflicting with the libc insque() and remque(), which
   have different prototypes. */
#define insque slirp_insque
#define remque slirp_remque
#endif /* !_MSC_VER */

#ifdef HAVE_SYS_STROPTS_H
#include <sys/stropts.h>
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
#ifdef USE_PPP
#include "ppp/pppd.h"
#include "ppp/ppp.h"
#endif

#include "bootp.h"
#include "tftp.h"

#include "slirp_state.h"

#ifndef NULL
#define NULL (void *)0
#endif

void if_start _P((PNATState));

#ifdef BAD_SPRINTF
# define vsprintf vsprintf_len
# define sprintf sprintf_len
 extern int vsprintf_len _P((char *, const char *, va_list));
 extern int sprintf_len _P((char *, const char *, ...));
#endif

#ifdef DECLARE_SPRINTF
# ifndef BAD_SPRINTF
 extern int vsprintf _P((char *, const char *, va_list));
# endif
 extern int vfprintf _P((FILE *, const char *, va_list));
#endif

#ifndef HAVE_STRERROR
 extern char *strerror _P((int error));
#endif

#ifndef HAVE_INDEX
 char *index _P((const char *, int));
#endif

#ifndef HAVE_GETHOSTID
 long gethostid _P((void));
#endif

DECLINLINE(void) lprint (const char *pszFormat, ...)
{
#ifdef LOG_ENABLED
    va_list args;
    va_start(args, pszFormat);
#if 1 /* nearly only for stats which we always want. */
    RTLogPrintfV(pszFormat, args);
#else
    Log(("%N", pszFormat, &args));
#endif
    va_end(args);
#endif
}

extern int do_echo;

#if SIZEOF_CHAR_P == 4
# define insque_32 insque
# define remque_32 remque
#else
extern void insque_32 _P((PNATState, void *, void *));
extern void remque_32 _P((PNATState, void *));
#endif

#ifndef _WIN32
#include <netdb.h>
#endif

#define DEFAULT_BAUD 115200

int get_dns_addr(PNATState pData, struct in_addr *pdns_addr);

/* cksum.c */
int cksum(struct mbuf *m, int len);

/* if.c */
void if_init _P((PNATState));
void if_output _P((PNATState, struct socket *, struct mbuf *));

/* ip_input.c */
void ip_init _P((PNATState));
void ip_input _P((PNATState, struct mbuf *));
struct ip * ip_reass _P((PNATState, register struct ipasfrag *, register struct ipq_t *));
void ip_freef _P((PNATState, struct ipq_t *));
void ip_enq _P((PNATState, register struct ipasfrag *, register struct ipasfrag *));
void ip_deq _P((PNATState, register struct ipasfrag *));
void ip_slowtimo _P((PNATState));
void ip_stripoptions _P((register struct mbuf *, struct mbuf *));

/* ip_output.c */
int ip_output _P((PNATState, struct socket *, struct mbuf *));

/* tcp_input.c */
int tcp_reass _P((PNATState, register struct tcpcb *, register struct tcpiphdr *, struct mbuf *));
void tcp_input _P((PNATState, register struct mbuf *, int, struct socket *));
void tcp_dooptions _P((PNATState, struct tcpcb *, u_char *, int, struct tcpiphdr *));
void tcp_xmit_timer _P((PNATState, register struct tcpcb *, int));
int tcp_mss _P((PNATState, register struct tcpcb *, u_int));

/* tcp_output.c */
int tcp_output _P((PNATState, register struct tcpcb *));
void tcp_setpersist _P((register struct tcpcb *));

/* tcp_subr.c */
void tcp_init _P((PNATState));
void tcp_template _P((struct tcpcb *));
void tcp_respond _P((PNATState, struct tcpcb *, register struct tcpiphdr *, register struct mbuf *, tcp_seq, tcp_seq, int));
struct tcpcb * tcp_newtcpcb _P((PNATState, struct socket *));
struct tcpcb * tcp_close _P((PNATState, register struct tcpcb *));
void tcp_drain _P((void));
void tcp_sockclosed _P((PNATState, struct tcpcb *));
int tcp_fconnect _P((PNATState, struct socket *));
void tcp_connect _P((PNATState, struct socket *));
int tcp_attach _P((PNATState, struct socket *));
u_int8_t tcp_tos _P((struct socket *));
int tcp_emu _P((PNATState, struct socket *, struct mbuf *));
int tcp_ctl _P((PNATState, struct socket *));
struct tcpcb *tcp_drop(PNATState, struct tcpcb *tp, int err);

#ifdef USE_PPP
#define MIN_MRU MINMRU
#define MAX_MRU MAXMRU
#else
#define MIN_MRU 128
#define MAX_MRU 16384
#endif

#ifndef _WIN32
#define min(x,y) ((x) < (y) ? (x) : (y))
#define max(x,y) ((x) > (y) ? (x) : (y))
#endif

#ifdef _WIN32
#undef errno
#define errno (WSAGetLastError())
#endif

#endif
