#ifndef __WINUTILS_H_
# define __WINUTILS_H_

# ifdef RT_OS_WINDOWS
#  include <iprt/cdefs.h>
#  include <WinSock2.h>
#  include <ws2tcpip.h>
#  include <mswsock.h>
#  include <Windows.h>
#  include <iprt/err.h>
#  include <iprt/net.h>
#  include <iprt/log.h>
/**
 * Inclusion of lwip/def.h was added here to avoid conflict of definitions
 * of hton-family functions in LWIP and windock's headers.
 */
#  include <lwip/def.h>

#  ifndef PF_LOCAL
#   define PF_LOCAL AF_INET
#  endif

#  define warn(...) DPRINTF2((__VA_ARGS__))
#  define warnx warn
#  ifdef DEBUG
#   define err(code,...) do { \
      AssertMsgFailed((__VA_ARGS__));           \
    }while(0)
#else
#   define err(code,...) do { \
      DPRINTF0((__VA_ARGS__));    \
      ExitProcess(code); \
    }while(0)
#endif
#  define errx err
#  define __func__ __FUNCTION__
#  define __attribute__(x) /* IGNORE */

#  define SOCKERRNO() (WSAGetLastError())

/**
 * "Windows Sockets Error Codes" obtained with WSAGetLastError().
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms740668(v=vs.85).aspx
 */
#  undef  EMSGSIZE
#  define EMSGSIZE              WSAEMSGSIZE
#  undef  ENETDOWN
#  define ENETDOWN              WSAENETDOWN
#  undef  ENETUNREACH
#  define ENETUNREACH           WSAENETUNREACH
#  undef  EHOSTDOWN
#  define EHOSTDOWN             WSAEHOSTDOWN
#  undef  EHOSTUNREACH
#  define EHOSTUNREACH          WSAEHOSTUNREACH

/**
 * parameters to shutdown (2) with Winsock2
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms740481(v=vs.85).aspx
 */
#  define SHUT_RD SD_RECEIVE
#  define SHUT_WR SD_SEND
#  define SHUT_RDWR SD_BOTH

typedef ULONG nfds_t;

typedef WSABUF IOVEC;

#  define IOVEC_GET_BASE(iov) ((iov).buf)
#  define IOVEC_SET_BASE(iov, b) ((iov).buf = (b))

#  define IOVEC_GET_LEN(iov) ((iov).len)
#  define IOVEC_SET_LEN(iov, l) ((iov).len = (ULONG)(l))

#if _WIN32_WINNT < 0x0600
/* otherwise defined the other way around in ws2def.h */
#define cmsghdr _WSACMSGHDR

#undef CMSG_DATA       /* wincrypt.h can byte my shiny metal #undef */
#define CMSG_DATA WSA_CMSG_DATA
#define CMSG_LEN WSA_CMSG_LEN
#define CMSG_SPACE WSA_CMSG_SPACE

#define CMSG_FIRSTHDR WSA_CMSG_FIRSTHDR
#define CMSG_NXTHDR WSA_CMSG_NXTHDR
#endif  /* _WIN32_WINNT < 0x0600 - provide unglified CMSG names */

RT_C_DECLS_BEGIN
int RTWinSocketPair(int domain, int type, int protocol, SOCKET socket_vector[2]);
RT_C_DECLS_END

# else /* !RT_OS_WINDOWS */

#  define SOCKET int
#  define INVALID_SOCKET (-1)
#  define SOCKET_ERROR (-1)

#  define SOCKERRNO() (errno)

#  define closesocket(s) close(s)
#  define ioctlsocket(s, req, arg) ioctl((s), (req), (arg))

typedef struct iovec IOVEC;

#  define IOVEC_GET_BASE(iov) ((iov).iov_base)
#  define IOVEC_SET_BASE(iov, b) ((iov).iov_base = (b))

#  define IOVEC_GET_LEN(iov) ((iov).iov_len)
#  define IOVEC_SET_LEN(iov, l) ((iov).iov_len = (l))
# endif
#endif
