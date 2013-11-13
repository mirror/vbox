/* $Id$ */
/** @file
 * Linux specific DNS information fetching.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/initterm.h>
#include <iprt/file.h>
#include <iprt/log.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>

#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <string>
#include <vector>
#include "../HostDnsService.h"


static RTTHREAD g_DnsMonitoringThread;
static RTSEMEVENT g_DnsInitEvent;
static int g_DnsMonitorStop[2];

class FileDescriptor
{
    public:
    FileDescriptor(int d = -1):fd(d){}

    virtual ~FileDescriptor() {
        if (fd != -1)
            close(fd);
    }

    int fileDescriptor() const {return fd;}

    protected:
    int fd;
};


class AutoNotify:public FileDescriptor
{
    public:
    AutoNotify()
    {
        FileDescriptor::fd = inotify_init();
        AssertReturnVoid(FileDescriptor::fd != -1);
    }

};


class AutoWatcher:public FileDescriptor
{
    public:
    AutoWatcher(const AutoNotify& notifier, const std::string& filename, uint32_t mask = IN_CLOSE_WRITE)
      :name(filename)
    {
        nfd = notifier.fileDescriptor();
        fd = inotify_add_watch(nfd, name.c_str(), mask);
        AssertMsgReturnVoid(fd != -1, ("failed to add watcher %s\n", name.c_str()));

        int opt = fcntl(fd, F_GETFL);
        opt |= O_NONBLOCK;
        fcntl(fd, F_SETFL, opt);
    }

    ~AutoWatcher()
    {
        int rc = inotify_rm_watch(nfd, fd);
        AssertMsgReturnVoid(rc != -1, ("Can't detach watcher %d from %d (%d: %s)\n", nfd, fd,
                                       errno, strerror(errno)));
    }

    private:
    std::string name;
    int nfd;
};


HostDnsServiceLinux::~HostDnsServiceLinux()
{
    send(g_DnsMonitorStop[0], "", 1, 0);
}


int HostDnsServiceLinux::hostMonitoringRoutine(RTTHREAD ThreadSelf, void *pvUser)
{
    NOREF(ThreadSelf);
    AutoNotify a;
    HostDnsServiceLinux *dns = static_cast<HostDnsServiceLinux *>(pvUser);
    AutoWatcher w(a, std::string(dns->resolvConf().c_str()));

    int rc = socketpair(AF_LOCAL, SOCK_DGRAM, 0, g_DnsMonitorStop);
    AssertMsgReturn(rc == 0, ("socketpair: failed (%d: %s)\n", errno, strerror(errno)), E_FAIL);

    FileDescriptor stopper0(g_DnsMonitorStop[0]);
    FileDescriptor stopper1(g_DnsMonitorStop[1]);

    pollfd polls[2];
    RT_ZERO(polls);

    polls[0].fd = a.fileDescriptor();
    polls[0].events = POLLIN;

    polls[1].fd = g_DnsMonitorStop[1];
    polls[1].events = POLLIN;

    RTSemEventSignal(g_DnsInitEvent);

    while(true)
    {
        rc = poll(polls, 2, -1);
        if (rc == -1)
            continue;

        AssertMsgReturn(   ((polls[0].revents & (POLLERR|POLLNVAL)) == 0)
                        && ((polls[1].revents & (POLLERR|POLLNVAL)) == 0),
                           ("Debug Me"), VERR_INTERNAL_ERROR);

        if (polls[1].revents & POLLIN)
            return VINF_SUCCESS; /* time to shutdown */

        if (polls[0].revents & POLLIN)
        {
            dns->readResolvConf();
            /* notifyAll() takes required locks */
            dns->notifyAll();

            polls[0].revents = 0;

            inotify_event ev;
            rc = read(a.fileDescriptor(), static_cast<void *>(&ev), sizeof(ev));
            AssertMsg(rc == sizeof(ev) && ev.wd == w.fileDescriptor(), ("Hmm, debug me"));
        }
    }
}


HRESULT HostDnsServiceLinux::init(const char *aResolvConfFileName)
{
    HRESULT hrc = HostDnsServiceResolvConf::init(aResolvConfFileName);
    AssertComRCReturnRC(hrc);

    rc = RTSemEventCreate(&g_DnsInitEvent);
    AssertRCReturn(rc, E_FAIL);

    int rc = RTThreadCreate(&g_DnsMonitoringThread, HostDnsServiceLinux::hostMonitoringRoutine,
                            this, 128 * _1K, RTTHREADTYPE_IO, 0, "dns-monitor");
    AssertRCReturn(rc, E_FAIL);

    RTSemEventWait(g_DnsInitEvent, RT_INDEFINITE_WAIT);

    return S_OK;
}
