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

class HostDnsService
{
public:
    HostDnsService();
    virtual ~HostDnsService();
    virtual HRESULT init(const VirtualBox *aParent);
    virtual HRESULT start(void);
    virtual void stop(void);
    STDMETHOD(COMGETTER(NameServers))(ComSafeArrayOut(BSTR, aNameServers));
    STDMETHOD(COMGETTER(DomainName))(BSTR *aDomainName);
    STDMETHOD(COMGETTER(SearchStrings))(ComSafeArrayOut(BSTR, aSearchStrings));
protected:
    virtual HRESULT update(void);

    /* XXX: hide it with struct Data together with <list> */
    Utf8StrList     m_llNameServers;
    Utf8StrList     m_llSearchStrings;
    com::Utf8Str    m_DomainName;
    RTCRITSECT      m_hCritSect;

private:
    const VirtualBox *mParent;
    HostDnsService(const HostDnsService&);
    HostDnsService& operator =(const HostDnsService&);
};

# ifdef RT_OS_DARWIN
class HostDnsServiceDarwin: public HostDnsService
{
public:
    HostDnsServiceDarwin();
    virtual ~HostDnsServiceDarwin();

    virtual HRESULT init(const VirtualBox *aParent);
    virtual HRESULT start(void);
    virtual void stop(void);
    virtual HRESULT update();
private:
    static void hostDnsServiceStoreCallback(void *store, void *arrayRef, void *info);

};
# endif

# ifdef RT_OS_WINDOWS
class HostDnsServiceWin: public HostDnsService
{
public:
    HostDnsServiceWin();
    virtual ~HostDnsServiceWin();

    virtual HRESULT init(const VirtualBox *aParent);
    virtual HRESULT start(void);
    virtual void stop(void);
    virtual HRESULT update();
private:
    void strList2List(Utf8StrList& lst, char *strLst);
};
# endif

#if defined(RT_OS_SOLARIS) || defined(RT_OS_LINUX) || defined(RT_OS_OS2)

class HostDnsServiceResolvConf: public HostDnsService
{
public:
    HostDnsServiceResolvConf(const char *aResolvConfFileName = "/etc/resolv.conf");
    virtual ~HostDnsServiceResolvConf();
    virtual HRESULT init(const VirtualBox *aParent);
    virtual HRESULT update();
    const com::Utf8Str resolvConf() {return m_ResolvConfFilename; }
protected:
    com::Utf8Str m_ResolvConfFilename;
    RTFILE m_ResolvConfFile;
};

#  if defined(RT_OS_SOLARIS)
/**
 * XXX: https://blogs.oracle.com/praks/entry/file_events_notification
 */
class HostDnsServiceSolaris: public HostDnsServiceResolvConf
{
public:
    HostDnsServiceSolaris(){}
    virtual ~HostDnsServiceSolaris(){}
};
#  elif defined(RT_OS_LINUX)
class HostDnsServiceLinux: public HostDnsServiceResolvConf
{
public:
    HostDnsServiceLinux(){}
    virtual ~HostDnsServiceLinux(){}
    virtual HRESULT init(const VirtualBox *aParent);
    virtual void stop(void);

    static int hostMonitoringRoutine(RTTHREAD ThreadSelf, void *pvUser);
};

#  elif defined(RT_OS_OS2)
class HostDnsServiceOs2: public HostDnsServiceResolvConf
{
public:
    HostDnsServiceOs2()
    {
        /* XXX: \\MPTN\\ETC should be taken from environment variable ETC  */
        ::HostDnsServiceResolvConf("\\MPTN\\ETC\\RESOLV2");
    }
    virtual ~HostDnsServiceOs2(){}
};
#  endif
#endif

#endif /* !___H_DNSHOSTSERVICE */
