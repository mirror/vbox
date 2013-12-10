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

/*
 * XXX: inet_ntop() is only available starting from Vista.
 */
DECLINLINE(PCSTR)
inet_ntop(INT Family, PVOID pAddr, PSTR pStringBuf, size_t StringBufSize)
{
    DWORD size = (DWORD)StringBufSize;
    int status;

    if (Family == AF_INET)
    {
        struct sockaddr_in sin;
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        memcpy(&sin.sin_addr, pAddr, sizeof(sin.sin_addr));
        sin.sin_port = 0;
        status = WSAAddressToStringA((LPSOCKADDR)&sin, sizeof(sin), NULL,
                                     pStringBuf, &size);
    }
    else if (Family == AF_INET6)
    {
        struct sockaddr_in6 sin6;
        memset(&sin6, 0, sizeof(sin6));
        sin6.sin6_family = AF_INET6;
        memcpy(&sin6.sin6_addr, pAddr, sizeof(sin6.sin6_addr));
        sin6.sin6_port = 0;
        status = WSAAddressToStringA((LPSOCKADDR)&sin6, sizeof(sin6), NULL,
                                     pStringBuf, &size);
    }
    else
    {
        WSASetLastError(WSAEAFNOSUPPORT);
        return NULL;
    }

    if (status == SOCKET_ERROR)
    {
        return NULL;
    }

    return pStringBuf;
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
