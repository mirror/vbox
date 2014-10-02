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
#include <vector>

typedef std::list<com::Utf8Str> Utf8StrList;
typedef Utf8StrList::iterator Utf8StrListIterator;

class HostDnsMonitorProxy;
typedef const HostDnsMonitorProxy *PCHostDnsMonitorProxy;

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
    explicit ALock(const Lockee *l);
    ~ALock();

  private:
    const Lockee *lockee;
};

class HostDnsInformation
{
  public:
    std::vector<std::wstring> servers;
    std::wstring domain;
    std::vector<std::wstring> searchList;
};

/**
 * This class supposed to be a real DNS monitor object it should be singleton,
 * it lifecycle starts and ends together with VBoxSVC.
 */
class HostDnsMonitor : public Lockee
{
  public:
    static const HostDnsMonitor *getHostDnsMonitor();
    static void shutdown();

    void addMonitorProxy(PCHostDnsMonitorProxy) const;
    void releaseMonitorProxy(PCHostDnsMonitorProxy) const;
    const HostDnsInformation &getInfo() const;
    /* @note: method will wait till client call
       HostDnsService::monitorThreadInitializationDone() */
    virtual HRESULT init();

  protected:
    explicit HostDnsMonitor(bool fThreaded = false);
    virtual ~HostDnsMonitor();

    void notifyAll() const;
    void setInfo(const HostDnsInformation &);

    /* this function used only if HostDnsMonitor::HostDnsMonitor(true) */
    void monitorThreadInitializationDone();
    virtual void monitorThreadShutdown() = 0;
    virtual int monitorWorker() = 0;

  private:
    HostDnsMonitor(const HostDnsMonitor &);
    HostDnsMonitor& operator= (const HostDnsMonitor &);
    static int threadMonitoringRoutine(RTTHREAD, void *);

  public:
    struct Data;
    Data *m;
};

/**
 * This class supposed to be a proxy for events on changing Host Name Resolving configurations.
 */
class HostDnsMonitorProxy : public Lockee
{
    public:
    HostDnsMonitorProxy();
    ~HostDnsMonitorProxy();
    void init(const HostDnsMonitor *aMonitor, const VirtualBox *aParent);
    void notify() const;

    HRESULT GetNameServers(std::vector<com::Utf8Str> &aNameServers);
    HRESULT GetDomainName(com::Utf8Str *pDomainName);
    HRESULT GetSearchStrings(std::vector<com::Utf8Str> &aSearchStrings);

    bool operator==(PCHostDnsMonitorProxy&);

    private:
    void updateInfo();

    private:
    struct Data;
    Data *m;
};

# ifdef RT_OS_DARWIN
class HostDnsServiceDarwin : public HostDnsMonitor
{
  public:
    HostDnsServiceDarwin();
    ~HostDnsServiceDarwin();
    HRESULT init();

    protected:
    virtual void monitorThreadShutdown();
    virtual int monitorWorker();

    private:
    HRESULT updateInfo();
    static void hostDnsServiceStoreCallback(void *store, void *arrayRef, void *info);
    struct Data;
    Data *m;
};
# endif
# ifdef RT_OS_WINDOWS
/* Maximum size of Windows registry key (according to MSDN). */
#define VBOX_KEY_NAME_LEN_MAX   (255)
class HostDnsServiceWin: public HostDnsMonitor
{
    public:

        HostDnsServiceWin();
        ~HostDnsServiceWin();

        HRESULT init();

    protected:

        virtual void monitorThreadShutdown();
        virtual int monitorWorker();

    private:

        /* This structure is used in order to link Windows registry key with
         * an event which is generated once the key has been changed (when interface has updated its DNS setting)
         * or one of the sub-keys has been deleted or added (when interface added or deleted). */
        struct Item
        {
            HKEY    hKey;                                /** Key handle. */
            HANDLE  hEvent;                              /** Event handle. */
            TCHAR   wcsInterface[VBOX_KEY_NAME_LEN_MAX]; /** Path to key within Windows registry. */
        };

        /* Bit flags to determine what was exactly was changed when Windows triggered event notification. */
        enum {
            VBOX_EVENT_NO_CHANGES           = 0,
            VBOX_EVENT_SERVERS_CHANGED      = RT_BIT(1),
            VBOX_EVENT_DOMAIN_CHANGED       = RT_BIT(2),
            VBOX_EVENT_SEARCHLIST_CHANGED   = RT_BIT(3),
        };

        /* Keys and events storage.
         * Size of this vector should not be greater than MAXIMUM_WAIT_OBJECTS because
         * this is exactly maximum amount of events which we allowed to wait for. */
        std::vector<struct Item> m_aWarehouse;
        /* Cached host network configuration. */
        HostDnsInformation m_hostInfoCache;

        /* TCHAR[] constants initialized outside of class definition. */
        static const TCHAR m_pwcKeyRoot[];

        /* m_aWarehouse array offsets. */
        enum {
            VBOX_OFFSET_SHUTDOWN_EVENT = 0,
            VBOX_OFFSET_TREE_EVENT     = 1,
            VBOX_OFFSET_SUBTREE_EVENTS = 2,
        };

        /* Do actual unsubscription for given item. */
        bool releaseWarehouseItem(int idxItem);
        /* Release all allocated resources and unsubscribe from everything. */
        void releaseResources();
        /* Remove subscription from DNS change notifications and release corresponding resources. */
        bool dropSubTreeNotifications();

        /* Create & add event into the list of events we monitor to. */
        bool subscribeTo(TCHAR *wcsPath, TCHAR *wcsInterface, DWORD fFilter);
        /* Subscribe to DNS changes. */
        bool enumerateSubTree();

        /* Get plain array of event handles. */
        void getEventHandles(HANDLE *ahEvents);
        void extendVectorWithStrings(std::vector<std::wstring>& pVectorToExtend, std::wstring &wcsParameter);
        HRESULT updateInfo(uint8_t *fWhatsChanged);

        /* This flag indicates whether constructor performed initialization correctly. If not set,
         * monitorWorker() will not perform any action and will be terminated as soon as there will be
         * an attempt to run it. */
        bool m_fInitialized;
};
# endif
# if defined(RT_OS_SOLARIS) || defined(RT_OS_LINUX) || defined(RT_OS_OS2) || defined(RT_OS_FREEBSD)
class HostDnsServiceResolvConf: public HostDnsMonitor
{
  public:
    explicit HostDnsServiceResolvConf(bool fThreaded = false) : HostDnsMonitor(fThreaded), m(NULL) {}
    virtual ~HostDnsServiceResolvConf();
    virtual HRESULT init(const char *aResolvConfFileName);
    const std::string& resolvConf() const;

  protected:
    HRESULT readResolvConf();
    /* While not all hosts supports Hosts DNS change notifiaction
     * default implementation offers return VERR_IGNORE.
     */
    virtual void monitorThreadShutdown() {}
    virtual int monitorWorker() {return VERR_IGNORED;}

  protected:
    struct Data;
    Data *m;
};
#  if defined(RT_OS_SOLARIS)
/**
 * XXX: https://blogs.oracle.com/praks/entry/file_events_notification
 */
class HostDnsServiceSolaris : public HostDnsServiceResolvConf
{
  public:
    HostDnsServiceSolaris(){}
    ~HostDnsServiceSolaris(){}
    HRESULT init(){ return HostDnsServiceResolvConf::init("/etc/resolv.conf");}
};

#  elif defined(RT_OS_LINUX)
class HostDnsServiceLinux : public HostDnsServiceResolvConf
{
  public:
    HostDnsServiceLinux():HostDnsServiceResolvConf(true){}
    virtual ~HostDnsServiceLinux();
    virtual HRESULT init(){ return HostDnsServiceResolvConf::init("/etc/resolv.conf");}

  protected:
    virtual void monitorThreadShutdown();
    virtual int monitorWorker();
};

#  elif defined(RT_OS_FREEBSD)
class HostDnsServiceFreebsd: public HostDnsServiceResolvConf
{
    public:
    HostDnsServiceFreebsd(){}
    ~HostDnsServiceFreebsd(){}
    HRESULT init(){ return HostDnsServiceResolvConf::init("/etc/resolv.conf");}
};

#  elif defined(RT_OS_OS2)
class HostDnsServiceOs2 : public HostDnsServiceResolvConf
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
