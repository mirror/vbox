/* $Id$ */
/** @file
 * Host DNS listener.
 */

/*
 * Copyright (C) 2005-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___H_DNSHOSTSERVICE
#define ___H_DNSHOSTSERVICE
#include "VirtualBoxBase.h"

#include <iprt/cdefs.h>
#include <iprt/critsect.h>
#include <iprt/types.h>


#include <list>

typedef std::list<com::Utf8Str> Utf8StrList;
typedef Utf8StrList::iterator Utf8StrListIterator;


class Lockee
{
    public:
    Lockee();
    virtual ~Lockee();
    const RTCRITSECT* lock() const;

    private:
    RTCRITSECT mLock;
};


class ALock
{
    public:
    ALock(const Lockee *l);
    ~ALock();

    private:
    const Lockee *lck;
};


struct HostDnsInformation
{
    std::vector<std::string> servers;
    std::string domain;
    std::vector<std::string> searchList;
};


class HostDnsMonitorProxy;

/**
 * This class supposed to be a real DNS monitor object it should be singleton,
 * it lifecycle starts and ends together with VBoxSVC.
 * 
 */
class HostDnsMonitor:public Lockee
{
    public:
    static const HostDnsMonitor* getHostDnsMonitor();
    static void shutdown();

    void addMonitorProxy(const HostDnsMonitorProxy&) const;
    void releaseMonitorProxy(const HostDnsMonitorProxy&) const;
    const HostDnsInformation& getInfo() const;
    virtual HRESULT init();


    protected:
    void notifyAll() const;
    void setInfo(const HostDnsInformation&);
    HostDnsMonitor();
    virtual ~HostDnsMonitor();

    private:
    HostDnsMonitor(const HostDnsMonitor&);
    HostDnsMonitor& operator= (const HostDnsMonitor&);

    public:
    struct Data;
    Data *m;
};

/**
 * This class supposed to be a proxy for events on changing Host Name Resolving configurations.
 */
class HostDnsMonitorProxy: public Lockee
{
    public:
    HostDnsMonitorProxy();
    ~HostDnsMonitorProxy();
    void init(const HostDnsMonitor* mon, const VirtualBox* aParent);
    void notify() const;
    
    STDMETHOD(COMGETTER(NameServers))(ComSafeArrayOut(BSTR, aNameServers));
    STDMETHOD(COMGETTER(DomainName))(BSTR *aDomainName);
    STDMETHOD(COMGETTER(SearchStrings))(ComSafeArrayOut(BSTR, aSearchStrings));

    bool operator==(const HostDnsMonitorProxy&);

    private:
    void updateInfo();

    private:
    struct Data;
    Data *m;
};

# ifdef RT_OS_DARWIN
class HostDnsServiceDarwin: public HostDnsMonitor
{
    public:
    HostDnsServiceDarwin();
    ~HostDnsServiceDarwin();
    HRESULT init();

    private:
    HRESULT updateInfo();
    static void hostDnsServiceStoreCallback(void *store, void *arrayRef, void *info);
};
# endif
# ifdef RT_OS_WINDOWS
class HostDnsServiceWin: public HostDnsMonitor
{
    public:
    HostDnsServiceWin();
    ~HostDnsServiceWin();
    HRESULT init();

    private:
    void strList2List(std::vector<std::string>& lst, char *strLst);
    HRESULT updateInfo();
};
# endif
# if defined(RT_OS_SOLARIS) || defined(RT_OS_LINUX) || defined(RT_OS_OS2)
class HostDnsServiceResolvConf: public HostDnsMonitor
{    
    public:
    HostDnsServiceResolvConf():m(NULL){}
    virtual ~HostDnsServiceResolvConf();
    virtual HRESULT init(const char *aResolvConfFileName);
    const std::string& resolvConf();

    protected:
    HRESULT readResolvConf();

    protected:
    struct Data;
    Data *m;
};
#  if defined(RT_OS_SOLARIS)
/**
 * XXX: https://blogs.oracle.com/praks/entry/file_events_notification
 */
class HostDnsServiceSolaris: public HostDnsServiceResolvConf
{
    public:
    HostDnsServiceSolaris(){}
    ~HostDnsServiceSolaris(){}
    HRESULT init(){ return init("/etc/resolv.conf");}
};

#  elif defined(RT_OS_LINUX)
class HostDnsServiceLinux: public HostDnsServiceResolvConf
{
    public:
    HostDnsServiceLinux(){}
    ~HostDnsServiceLinux();
    HRESULT init() {return init("/etc/resolv.conf");}
    HRESULT init(const char *aResolvConfFileName);

    static int hostMonitoringRoutine(RTTHREAD ThreadSelf, void *pvUser);
};

#  elif defined(RT_OS_OS2)
class HostDnsServiceOs2: public HostDnsServiceResolvConf
{
    public:
    HostDnsServiceOs2(){}
    ~HostDnsServiceOs2(){}
    /* XXX: \\MPTN\\ETC should be taken from environment variable ETC  */
    HRESULT init(){ return init("\\MPTN\\ETC\\RESOLV2");}
};

#  endif
# endif

#endif /* !___H_DNSHOSTSERVICE */
