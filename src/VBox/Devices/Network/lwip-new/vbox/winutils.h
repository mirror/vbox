#ifndef __WINUTILS_H_
# define __WINUTILS_H_

# ifdef RT_OS_WINDOWS
#  include <iprt/cdefs.h>
#  include <WinSock2.h>
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
      AssertMsgFailed((__VA_ARGS__));		\
    }while(0)
#else
#   define err(code,...) do { \
      DPRINTF0((__VA_ARGS__));	  \
      ExitProcess(code); \
    }while(0)
#endif
#  define errx err
#  define __func__ __FUNCTION__
#  define __attribute__(x) /* IGNORE */
#  define inet_ntop(dom, pvaddr, pstrbuf, cbstrbuf) InetNtop((dom),(pvaddr),(pstrbuf),(cbstrbuf))

DECLINLINE(int) inet_pton(int af_family, const char *str, void *dst)
{
    AssertPtrReturn(str, -1);
    AssertPtrReturn(dst, -1);
    
    switch (af_family) {
    case AF_INET: {
      RTNETADDRIPV4 a4;
      int rc = RTNetStrToIPv4Addr(str, &a4);
      AssertRCReturn(rc, -1);
      memcpy(dst, &a4, sizeof(RTNETADDRIPV4));
      break;
    }
    case AF_INET6: {
#if 0
      RTNETADDRIPV6 a6;
      int rc = RTNetStrToIPv6Addr(str, &a6);
      AssertRCReturn(rc, -1);
      memcpy(dst, &a6, sizeof(RTNETADDRIPV6));
#else
      /* RTNetStrToIPv6Addr isn't implemented yet. */
      return -1;
#endif
      break;
    }
    default:
      AssertMsgFailedReturn(("Unsupported internet family"), -1);
    }
    return 0;
}

/**
 * tftpd emulation we're using POSIX operations which needs "DOS errno". see proxy_tftpd.c
 */
#  ifndef _USE_WINSTD_ERRNO
/**
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms737828(v=vs.85).aspx
 * "Error Codes - errno, h_errno and WSAGetLastError" says "Error codes set by Windows Sockets are
 *  not made available through the errno variable."
 */
#   include <errno.h>
#   ifdef errno
#    undef errno
#   endif
#   define errno (WSAGetLastError())
#  endif
/* Missing errno codes */

/**
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms740668(v=vs.85).aspx#WSAEHOSTDOWN
 * Corresponds to WSAEHOSTDOWN (10064) WSAGetLastError()
 */
#  define EHOSTDOWN WSAEHOSTDOWN
/** 
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms740668(v=vs.85).aspx#WSAEHOSTUNREACH 
 */
#  define EHOSTUNREAH WSAEHOSTUNREACH

/**
 * parameters to shutdown (2) with Winsock2
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms740481(v=vs.85).aspx
 */
#  define SHUT_RD SD_RECEIVE
#  define SHUT_WR SD_SEND
#  define SHUT_RDWR SD_BOTH

typedef int socklen_t;

typedef ULONG nfds_t;

typedef WSABUF IOVEC;

#  define IOVEC_GET_BASE(iov) ((iov).buf)
#  define IOVEC_SET_BASE(iov, b) ((iov).buf = (b))

#  define IOVEC_GET_LEN(iov) ((iov).len)
#  define IOVEC_SET_LEN(iov, l) ((iov).len = (ULONG)(l)) 

RT_C_DECLS_BEGIN
int RTWinSocketPair(int domain, int type, int protocol, SOCKET socket_vector[2]);
RT_C_DECLS_END

# else /* !RT_OS_WINDOWS */
#  define ioctlsocket ioctl
#  define closesocket close
#  define SOCKET int
#  define INVALID_SOCKET (-1)
#  define SOCKET_ERROR (-1)

typedef struct iovec IOVEC;

#  define IOVEC_GET_BASE(iov) ((iov).iov_base)
#  define IOVEC_SET_BASE(iov, b) ((iov).iov_base = (b))

#  define IOVEC_GET_LEN(iov) ((iov).iov_len)
#  define IOVEC_SET_LEN(iov, l) ((iov).iov_len = (l)) 
# endif
#endif
