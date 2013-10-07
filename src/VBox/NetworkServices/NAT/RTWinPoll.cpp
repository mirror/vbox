/* -*- indent-tabs-mode: nil; -*- */
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/cdefs.h>
#include <iprt/err.h>
#include <iprt/string.h>

#include <VBox/err.h>

#include <Winsock2.h>
#include <Windows.h>
#include "winpoll.h"

static HANDLE g_hNetworkEvent;

int
RTWinPoll(struct pollfd *pFds, unsigned int nfds, int timeout, int *pNready)
{
    AssertPtrReturn(pFds, VERR_INVALID_PARAMETER);

    if (g_hNetworkEvent == WSA_INVALID_EVENT)
    {
        g_hNetworkEvent = WSACreateEvent();
        AssertReturn(g_hNetworkEvent != WSA_INVALID_EVENT, VERR_INTERNAL_ERROR);
    }

    for (unsigned int i = 0; i < nfds; ++i)
    {
        long eventMask = 0;
        short pollEvents = pFds[i].events;

        /* clean revents */
        pFds[i].revents = 0;

        /* ignore invalid sockets */
        if (pFds[i].fd == INVALID_SOCKET)
          continue;

        /**
         * POLLIN         Data other than high priority data may be read without blocking.
         * This is equivalent to ( POLLRDNORM | POLLRDBAND ).
         * POLLRDBAND     Priority data may be read without blocking.
         * POLLRDNORM     Normal data may be read without blocking.
         */
        if (pollEvents & POLLIN)
            eventMask |= FD_READ | FD_ACCEPT;

        /**
         * POLLOUT        Normal data may be written without blocking.  This is equivalent
         *  to POLLWRNORM.
         * POLLWRNORM     Normal data may be written without blocking.
         */
        if (pollEvents & POLLOUT)
            eventMask |= FD_WRITE | FD_CONNECT;

        /**
         * This is "moral" equivalent to POLLHUP.
         */
        eventMask |= FD_CLOSE;
        WSAEventSelect(pFds[i].fd, g_hNetworkEvent, eventMask);
    }

    DWORD index = WSAWaitForMultipleEvents(1,
                                           &g_hNetworkEvent,
                                           FALSE,
                                           timeout == RT_INDEFINITE_WAIT ? WSA_INFINITE : timeout,
                                           FALSE);
    if (index != WSA_WAIT_EVENT_0)
    {
        if (index == WSA_WAIT_TIMEOUT)
            return VERR_TIMEOUT;
    }

    int nready = 0;
    for (unsigned int i = 0; i < nfds; ++i)
    {
        short revents = 0;
        WSANETWORKEVENTS NetworkEvents;
        int err;

        if (pFds[i].fd == INVALID_SOCKET)
            continue;

        RT_ZERO(NetworkEvents);

        err = WSAEnumNetworkEvents(pFds[i].fd,
                                   g_hNetworkEvent,
                                   &NetworkEvents);

        if (err == SOCKET_ERROR)
        {
            if (WSAGetLastError() == WSAENOTSOCK)
            {
                pFds[i].revents = POLLNVAL;
                ++nready;
            }
            continue;
        }

        /* deassociate socket with event */
        WSAEventSelect(pFds[i].fd, g_hNetworkEvent, 0);

        if (NetworkEvents.lNetworkEvents & (FD_READ|FD_ACCEPT))
        {
            if (   NetworkEvents.iErrorCode[FD_READ_BIT] != 0
                || NetworkEvents.iErrorCode[FD_ACCEPT_BIT] != 0)
                revents |= POLLERR;

            revents |= POLLIN;
        }

        if (NetworkEvents.lNetworkEvents & (FD_WRITE|FD_CONNECT))
        {
            if (   NetworkEvents.iErrorCode[FD_WRITE_BIT] != 0
                || NetworkEvents.iErrorCode[FD_CONNECT_BIT] != 0)
                revents |= POLLERR;

            revents |= POLLOUT;
        }

        if (NetworkEvents.lNetworkEvents & FD_CLOSE)
        {
            if (NetworkEvents.iErrorCode[FD_CLOSE_BIT] != 0)
                revents |= POLLERR;

            revents |= POLLHUP;
        }

        /* paranoid */
        revents &= (pFds[i].events | POLLHUP | POLLERR);
        if (revents != 0)
        {
            pFds[i].revents = revents;
            ++nready;
        }
    }
    WSAResetEvent(g_hNetworkEvent);

    if (pNready)
      *pNready = nready;

    return VINF_SUCCESS;
}
