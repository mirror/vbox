/* $Id$ */
/** @file
 * Base class for Host DNS & Co services.
 */

/*
 * Copyright (C) 2013-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <VBox/com/array.h>
#include <VBox/com/ptr.h>
#include <VBox/com/string.h>

#include <iprt/cpp/utils.h>

#include "Logging.h"
#include "VirtualBoxImpl.h"
#include <iprt/thread.h>
#include <iprt/semaphore.h>
#include <iprt/critsect.h>

#include <algorithm>
#include <string>
#include "HostDnsService.h"


static HostDnsMonitor *g_monitor;

static void dumpHostDnsInformation(const HostDnsInformation&);
static void dumpHostDnsStrVector(const std::string&, const std::vector<std::string>&);

/* Lockee */
Lockee::Lockee()
{
    RTCritSectInit(&mLock);
}

Lockee::~Lockee()
{
    RTCritSectDelete(&mLock);
}

const RTCRITSECT* Lockee::lock() const
{
    return &mLock;
}

/* ALock */
ALock::ALock(const Lockee *aLockee)
  : lockee(aLockee)
{
    RTCritSectEnter(const_cast<PRTCRITSECT>(lockee->lock()));
}

ALock::~ALock()
{
    RTCritSectLeave(const_cast<PRTCRITSECT>(lockee->lock()));
}

/* HostDnsInformation */

bool HostDnsInformation::equals(const HostDnsInformation &info) const
{
    return    (servers == info.servers)
           && (domain == info.domain)
           && (searchList == info.searchList);
}

inline static void detachVectorOfString(const std::vector<std::string>& v,
                                        std::vector<com::Utf8Str> &aArray)
{
    aArray.resize(v.size());
    size_t i = 0;
    for (std::vector<std::string>::const_iterator it = v.begin(); it != v.end(); ++it, ++i)
        aArray[i] = Utf8Str(it->c_str());
}

struct HostDnsMonitor::Data
{
    Data(bool aThreaded) :
        fInfoModified(false),
        fThreaded(aThreaded)
    {}

    std::vector<PCHostDnsMonitorProxy> proxies;
    HostDnsInformation info;
    bool fInfoModified;
    const bool fThreaded;
    RTTHREAD hMonitoringThread;
    RTSEMEVENT hDnsInitEvent;
};

struct HostDnsMonitorProxy::Data
{
    Data(const HostDnsMonitor *aMonitor, const VirtualBox *aParent)
      : info(NULL)
      , virtualbox(aParent)
      , monitor(aMonitor)
      , fModified(true)
    {}

    virtual ~Data()
    {
        if (info)
        {
            delete info;
            info = NULL;
        }
    }

    HostDnsInformation *info;
    const VirtualBox *virtualbox;
    const HostDnsMonitor *monitor;
    bool fModified;
};


HostDnsMonitor::HostDnsMonitor(bool fThreaded)
  : m(NULL)
{
   m = new HostDnsMonitor::Data(fThreaded);
}

HostDnsMonitor::~HostDnsMonitor()
{
    if (m)
    {
        delete m;
        m = NULL;
    }
}

const HostDnsMonitor *HostDnsMonitor::getHostDnsMonitor()
{
    /* XXX: Moved initialization from HostImpl.cpp */
    if (!g_monitor)
    {
# if defined (RT_OS_DARWIN)
        g_monitor = new HostDnsServiceDarwin();
# elif defined(RT_OS_WINDOWS)
        g_monitor = new HostDnsServiceWin();
# elif defined(RT_OS_LINUX)
        g_monitor = new HostDnsServiceLinux();
# elif defined(RT_OS_SOLARIS)
        g_monitor =  new HostDnsServiceSolaris();
# elif defined(RT_OS_FREEBSD)
        g_monitor = new HostDnsServiceFreebsd();
# elif defined(RT_OS_OS2)
        g_monitor = new HostDnsServiceOs2();
# else
        g_monitor = new HostDnsService();
# endif
        g_monitor->init();
    }

    return g_monitor;
}

void HostDnsMonitor::addMonitorProxy(PCHostDnsMonitorProxy proxy) const
{
    ALock l(this);
    m->proxies.push_back(proxy);
    proxy->notify();
}

void HostDnsMonitor::releaseMonitorProxy(PCHostDnsMonitorProxy proxy) const
{
    ALock l(this);
    std::vector<PCHostDnsMonitorProxy>::iterator it;
    it = std::find(m->proxies.begin(), m->proxies.end(), proxy);

    if (it == m->proxies.end())
        return;

    m->proxies.erase(it);
}

void HostDnsMonitor::shutdown()
{
    if (g_monitor)
    {
        delete g_monitor;
        g_monitor = NULL;
    }
}

const HostDnsInformation &HostDnsMonitor::getInfo() const
{
    return m->info;
}

void HostDnsMonitor::notifyAll() const
{
    ALock l(this);
    if (m->fInfoModified)
    {
        m->fInfoModified = false;
        std::vector<PCHostDnsMonitorProxy>::const_iterator it;
        for (it = m->proxies.begin(); it != m->proxies.end(); ++it)
            (*it)->notify();
    }
}

void HostDnsMonitor::setInfo(const HostDnsInformation &info)
{
    ALock l(this);
    // Check for actual modifications, as the Windows specific code seems to
    // often set the same information as before, without any change to the
    // previous state. Here we have the previous state, so make sure we don't
    // ever tell our clients about unchanged info.
    if (info.equals(m->info))
    {
        m->info = info;
        m->fInfoModified = true;
    }
}

HRESULT HostDnsMonitor::init()
{
    if (m->fThreaded)
    {
        int rc = RTSemEventCreate(&m->hDnsInitEvent);
        AssertRCReturn(rc, E_FAIL);

        rc = RTThreadCreate(&m->hMonitoringThread,
                            HostDnsMonitor::threadMonitoringRoutine,
                            this, 128 * _1K, RTTHREADTYPE_IO, 0, "dns-monitor");
        AssertRCReturn(rc, E_FAIL);

        RTSemEventWait(m->hDnsInitEvent, RT_INDEFINITE_WAIT);
    }
    return S_OK;
}


void HostDnsMonitor::monitorThreadInitializationDone()
{
    RTSemEventSignal(m->hDnsInitEvent);
}


int HostDnsMonitor::threadMonitoringRoutine(RTTHREAD, void *pvUser)
{
    HostDnsMonitor *pThis = static_cast<HostDnsMonitor *>(pvUser);
    return pThis->monitorWorker();
}

/* HostDnsMonitorProxy */
HostDnsMonitorProxy::HostDnsMonitorProxy()
  : m(NULL)
{
}

HostDnsMonitorProxy::~HostDnsMonitorProxy()
{
    if (m)
    {
        if (m->monitor)
            m->monitor->releaseMonitorProxy(this);
        delete m;
        m = NULL;
    }
}

void HostDnsMonitorProxy::init(const HostDnsMonitor *mon, const VirtualBox* aParent)
{
    m = new HostDnsMonitorProxy::Data(mon, aParent);
    m->monitor->addMonitorProxy(this);
    updateInfo();
}

void HostDnsMonitorProxy::notify() const
{
    m->fModified = true;
    const_cast<VirtualBox *>(m->virtualbox)->i_onHostNameResolutionConfigurationChange();
}

HRESULT HostDnsMonitorProxy::GetNameServers(std::vector<com::Utf8Str> &aNameServers)
{
    AssertReturn(m && m->info, E_FAIL);
    ALock l(this);

    if (m->fModified)
        updateInfo();

    LogRel(("HostDnsMonitorProxy::GetNameServers:\n"));
    dumpHostDnsStrVector("Name Server", m->info->servers);

    detachVectorOfString(m->info->servers, aNameServers);

    return S_OK;
}

HRESULT HostDnsMonitorProxy::GetDomainName(com::Utf8Str *pDomainName)
{
    AssertReturn(m && m->info, E_FAIL);
    ALock l(this);

    if (m->fModified)
        updateInfo();

    LogRel(("HostDnsMonitorProxy::GetDomainName: %s\n", m->info->domain.c_str()));

    *pDomainName = m->info->domain.c_str();

    return S_OK;
}

HRESULT HostDnsMonitorProxy::GetSearchStrings(std::vector<com::Utf8Str> &aSearchStrings)
{
    AssertReturn(m && m->info, E_FAIL);
    ALock l(this);

    if (m->fModified)
        updateInfo();

    LogRel(("HostDnsMonitorProxy::GetSearchStrings:\n"));
    dumpHostDnsStrVector("Search String", m->info->searchList);

    detachVectorOfString(m->info->searchList, aSearchStrings);

    return S_OK;
}

bool HostDnsMonitorProxy::operator==(PCHostDnsMonitorProxy& rhs)
{
    if (!m || !rhs->m)
        return false;

    /**
     * we've assigned to the same instance of VirtualBox.
     */
    return m->virtualbox == rhs->m->virtualbox;
}

void HostDnsMonitorProxy::updateInfo()
{
    HostDnsInformation *info = new HostDnsInformation(m->monitor->getInfo());
    HostDnsInformation *old = m->info;

    LogRel(("HostDnsMonitorProxy: Host's DNS information updated:\n"));
    dumpHostDnsInformation(*info);

    m->info = info;
    if (old)
    {
        LogRel(("HostDnsMonitorProxy: Old host information:\n"));
        dumpHostDnsInformation(*old);

        delete old;
    }

    m->fModified = false;
}


static void dumpHostDnsInformation(const HostDnsInformation& info)
{
    dumpHostDnsStrVector("DNS server", info.servers);
    dumpHostDnsStrVector("SearchString", info.searchList);

    if (!info.domain.empty())
        LogRel(("DNS domain: %s\n", info.domain.c_str()));
}


static void dumpHostDnsStrVector(const std::string& prefix, const std::vector<std::string>& v)
{
    int i = 1;
    for (std::vector<std::string>::const_iterator it = v.begin();
         it != v.end();
         ++it, ++i)
        LogRel(("%s %d: %s\n", prefix.c_str(), i, it->c_str()));
}
