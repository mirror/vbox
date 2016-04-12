/* $Id$ */
/** @file
 * Host DNS listener for Windows.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#include "../HostDnsService.h"

#include <VBox/com/string.h>
#include <VBox/com/ptr.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <VBox/log.h>

#include <Windows.h>
#include <windns.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

struct HostDnsServiceWin::Data
{
    HKEY hKeyTcpipParameters;
    bool fTimerArmed;

#define DATA_SHUTDOWN_EVENT   0
#define DATA_DNS_UPDATE_EVENT 1
#define DATA_TIMER            2
#define DATA_MAX_EVENT        3
    HANDLE haDataEvent[DATA_MAX_EVENT];

    Data()
    {
        hKeyTcpipParameters = NULL;
        fTimerArmed = false;

        for (size_t i = 0; i < DATA_MAX_EVENT; ++i)
            haDataEvent[i] = NULL;
    }

    ~Data()
    {
        if (hKeyTcpipParameters != NULL)
            RegCloseKey(hKeyTcpipParameters);

        for (size_t i = 0; i < DATA_MAX_EVENT; ++i)
            if (haDataEvent[i] != NULL)
                CloseHandle(haDataEvent[i]);
    }
};


HostDnsServiceWin::HostDnsServiceWin()
 : HostDnsMonitor(true)
{
    m = new Data();
}

HostDnsServiceWin::~HostDnsServiceWin()
{
    if (m != NULL)
        delete m;
}


HRESULT HostDnsServiceWin::init(VirtualBox *virtualbox)
{
    if (m == NULL)
        return E_FAIL;

    {
        bool res = true;
        LONG lrc = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                            L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
                            0,
                            KEY_READ|KEY_NOTIFY,
                            &m->hKeyTcpipParameters);
        if (lrc != ERROR_SUCCESS)
        {
            LogRel(("HostDnsServiceWin: failed to open key Tcpip\\Parameters (error %d)\n", lrc));
            res = false;
        }
        else
        {
            for (size_t i = 0; i < DATA_MAX_EVENT; ++i)
            {
                HANDLE h;

                if (i ==  DATA_TIMER)
                    h = CreateWaitableTimer(NULL, FALSE, NULL);
                else
                    h = CreateEvent(NULL, TRUE, FALSE, NULL);

                if (h == NULL)
                {
                    LogRel(("HostDnsServiceWin: failed to create event (error %d)\n", GetLastError()));
                    res = false;
                    break;
                }

                m->haDataEvent[i] = h;
            }
        }
        if(!res)
            return E_FAIL;
    }

    HRESULT hrc = HostDnsMonitor::init(virtualbox);
    if (FAILED(hrc))
        return hrc;

    return updateInfo();
}


void HostDnsServiceWin::monitorThreadShutdown()
{
    Assert(m != NULL);
    SetEvent(m->haDataEvent[DATA_SHUTDOWN_EVENT]);
}


static inline int registerNotification(const HKEY& hKey, HANDLE& hEvent)
{
    LONG lrc = RegNotifyChangeKeyValue(hKey,
                                       TRUE,
                                       REG_NOTIFY_CHANGE_LAST_SET,
                                       hEvent,
                                       TRUE);
    AssertMsgReturn(lrc == ERROR_SUCCESS,
                    ("Failed to register event on the key. Please debug me!"),
                    VERR_INTERNAL_ERROR);

    return VINF_SUCCESS;
}


int HostDnsServiceWin::monitorWorker()
{
    Assert(m != NULL);

    registerNotification(m->hKeyTcpipParameters,
                         m->haDataEvent[DATA_DNS_UPDATE_EVENT]);

    monitorThreadInitializationDone();

    for (;;)
    {
        DWORD dwReady;

        dwReady = WaitForMultipleObjects(DATA_MAX_EVENT, m->haDataEvent,
                                         FALSE, INFINITE);

        if (dwReady == WAIT_OBJECT_0 + DATA_SHUTDOWN_EVENT)
            break;

        if (dwReady == WAIT_OBJECT_0 + DATA_DNS_UPDATE_EVENT)
        {
            /*
             * Registry updates for multiple values are not atomic, so
             * wait a bit to avoid racing and reading partial update.
             */
            if (!m->fTimerArmed)
            {
                LARGE_INTEGER delay; /* in 100ns units */
                delay.QuadPart = -2 * 1000 * 1000 * 10LL; /* relative: 2s */

                BOOL ok = SetWaitableTimer(m->haDataEvent[DATA_TIMER], &delay,
                                           0, NULL, NULL, TRUE);
                if (ok)
                {
                    m->fTimerArmed = true;
                }
                else
                {
                    LogRel(("HostDnsServiceWin: failed to arm timer (error %d)\n", GetLastError()));
                    updateInfo();
                }
            }

            ResetEvent(m->haDataEvent[DATA_DNS_UPDATE_EVENT]);
            registerNotification(m->hKeyTcpipParameters,
                                 m->haDataEvent[DATA_DNS_UPDATE_EVENT]);
        }
        else if (dwReady == WAIT_OBJECT_0 + DATA_TIMER)
        {
            m->fTimerArmed = false;
            updateInfo();
        }
        else if (dwReady == WAIT_FAILED)
        {
            LogRel(("HostDnsServiceWin: WaitForMultipleObjects failed: error %d\n", GetLastError()));
            return VERR_INTERNAL_ERROR;
        }
        else
        {
            LogRel(("HostDnsServiceWin: WaitForMultipleObjects unexpected return value %d\n", dwReady));
            return VERR_INTERNAL_ERROR;
        }
    }

    return VINF_SUCCESS;
}


void vappend(std::vector<std::string> &v, const std::string &s, char sep = ' ')
{
    if (s.empty())
        return;

    std::istringstream stream(s);
    std::string substr;

    while (std::getline(stream, substr, sep))
    {
        if (substr.empty())
            continue;

        if (std::find(v.cbegin(), v.cend(), substr) != v.cend())
            continue;

        v.push_back(substr);
    }
}


HRESULT HostDnsServiceWin::updateInfo()
{
    LONG lrc;

    std::string strDomain;
    std::string strDhcpDomain;
    std::string strSearchList;  /* NB: comma separated, no spaces */

    for (DWORD regIndex = 0; /**/; ++regIndex) {
        char keyName[256];
        DWORD cbKeyName = sizeof(keyName);
        DWORD keyType = 0;
        char keyData[1024];
        DWORD cbKeyData = sizeof(keyData);

        lrc = RegEnumValueA(m->hKeyTcpipParameters, regIndex,
                            keyName, &cbKeyName, 0,
                            &keyType, (LPBYTE)keyData, &cbKeyData);

        if (lrc == ERROR_NO_MORE_ITEMS)
            break;

        if (lrc == ERROR_MORE_DATA) /* buffer too small; handle? */
            continue;

        if (lrc != ERROR_SUCCESS)
        {
            LogRel(("HostDnsServiceWin: RegEnumValue error %d\n", (int)lrc));
            return E_FAIL;
        }

        if (keyType != REG_SZ)
            continue;

        if (cbKeyData > 0 && keyData[cbKeyData - 1] == '\0')
            --cbKeyData;     /* don't count trailing NUL if present */

        if (RTStrICmp("Domain", keyName) == 0)
        {
            strDomain.assign(keyData, cbKeyData);
            Log2(("... Domain=\"%s\"\n", strDomain.c_str()));
        }
        else if (RTStrICmp("DhcpDomain", keyName) == 0)
        {
            strDhcpDomain.assign(keyData, cbKeyData);
            Log2(("... DhcpDomain=\"%s\"\n", strDhcpDomain.c_str()));
        }
        else if (RTStrICmp("SearchList", keyName) == 0)
        {
            strSearchList.assign(keyData, cbKeyData);
            Log2(("... SearchList=\"%s\"\n", strSearchList.c_str()));
        }
    }

    HostDnsInformation info;

    /*
     * When name servers are configured statically it seems that the
     * value of Tcpip\Parameters\NameServer is NOT set, inly interface
     * specific NameServer value is (which triggers notification for
     * us to pick up the change).  Fortunately, DnsApi seems to do the
     * right thing there.
     */
    DNS_STATUS status;
    PIP4_ARRAY pIp4Array = NULL;

    // NB: must be set on input it seems, despite docs' claim to the contrary.
    DWORD cbBuffer = sizeof(&pIp4Array);

    status = DnsQueryConfig(DnsConfigDnsServerList,
                            DNS_CONFIG_FLAG_ALLOC, NULL, NULL,
                            &pIp4Array, &cbBuffer);

    if (status == NO_ERROR && pIp4Array != NULL)
    {
        for (DWORD i = 0; i < pIp4Array->AddrCount; ++i)
        {
            char szAddrStr[16] = "";
            RTStrPrintf(szAddrStr, sizeof(szAddrStr), "%RTnaipv4", pIp4Array->AddrArray[i]);

            Log2(("  server %d: %s\n", i+1,  szAddrStr));
            info.servers.push_back(szAddrStr);
        }

        LocalFree(pIp4Array);
    }

    if (!strDomain.empty())
    {
        info.domain = strDomain;

        info.searchList.push_back(strDomain);
        if (!strDhcpDomain.empty() && strDhcpDomain != strDomain)
            info.searchList.push_back(strDhcpDomain);
    }
    else if (!strDhcpDomain.empty())
    {
        info.domain = strDhcpDomain;
        info.searchList.push_back(strDomain);
    }

    vappend(info.searchList, strSearchList, ',');
    if (info.searchList.size() == 1)
        info.searchList.clear();

    HostDnsMonitor::setInfo(info);

    return S_OK;
}
