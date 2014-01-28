/* -*- indent-tabs-mode: nil; -*- */
#include <VBox/com/string.h>
#include <VBox/com/ptr.h>


#include <iprt/assert.h>
#include <iprt/err.h>

#include <Windows.h>

#include <string>
#include <vector>
#include "../HostDnsService.h"

struct HostDnsServiceWin::Data
{
    HostDnsServiceWin::Data(){}
    HKEY hKeyTcpipParameters;
#define DATA_DNS_UPDATE_EVENT 0
#define DATA_SHUTDOWN_EVENT   1
#define DATA_MAX_EVENT        2
    HANDLE haDataEvent[DATA_MAX_EVENT];
};

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

HostDnsServiceWin::HostDnsServiceWin():HostDnsMonitor(true), m(NULL)
{
    m = new Data();

    m->haDataEvent[DATA_DNS_UPDATE_EVENT] = CreateEvent(NULL,
      TRUE, FALSE, NULL);
    AssertReleaseMsg(m->haDataEvent[DATA_DNS_UPDATE_EVENT],
      ("Failed to create event for DNS event (%d)\n", GetLastError()));

    m->haDataEvent[DATA_SHUTDOWN_EVENT] = CreateEvent(NULL,
      TRUE, FALSE, NULL);
    AssertReleaseMsg(m->haDataEvent[DATA_SHUTDOWN_EVENT],
      ("Failed to create event for Shutdown signal (%d)\n", GetLastError()));

    LONG lrc = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
      TEXT("SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters"),
      0, KEY_READ|KEY_NOTIFY, &m->hKeyTcpipParameters);
    AssertReleaseMsg(lrc == ERROR_SUCCESS,
      ("Failed to open Registry Key for read and update notifications (%d)\n",
      GetLastError()));
}


HostDnsServiceWin::~HostDnsServiceWin()
{
    if (m && !m->hKeyTcpipParameters)
    {
        RegCloseKey(m->hKeyTcpipParameters);
        m->hKeyTcpipParameters = 0;

        CloseHandle(m->haDataEvent[DATA_DNS_UPDATE_EVENT]);
        CloseHandle(m->haDataEvent[DATA_SHUTDOWN_EVENT]);

        delete m;

        m = NULL;
    }
}


HRESULT HostDnsServiceWin::init()
{
    HRESULT hrc = HostDnsMonitor::init();
    AssertComRCReturn(hrc, hrc);

    return updateInfo();
}


void HostDnsServiceWin::monitorThreadShutdown()
{
    SetEvent(m->haDataEvent[DATA_SHUTDOWN_EVENT]);
}


int HostDnsServiceWin::monitorWorker()
{
    registerNotification(m->hKeyTcpipParameters,
                         m->haDataEvent[DATA_DNS_UPDATE_EVENT]);

    monitorThreadInitializationDone();

    DWORD dwRc;
    while (true)
    {
        dwRc = WaitForMultipleObjects(DATA_MAX_EVENT,
                                      m->haDataEvent,
                                      FALSE,
                                      INFINITE);
        AssertMsgReturn(dwRc != WAIT_FAILED,
                        ("WaitForMultipleObjects failed (%d) to wait! Please debug",
                         GetLastError()), VERR_INTERNAL_ERROR);

        if ((dwRc - WAIT_OBJECT_0) == DATA_DNS_UPDATE_EVENT)
        {
            updateInfo();
            notifyAll();
            ResetEvent(m->haDataEvent[DATA_DNS_UPDATE_EVENT]);
            registerNotification(m->hKeyTcpipParameters,
                                 m->haDataEvent[DATA_DNS_UPDATE_EVENT]);

        }
        else if ((dwRc - WAIT_OBJECT_0) == DATA_SHUTDOWN_EVENT)
        {
            break;
        }
        else
        {
            AssertMsgFailedReturn(
              ("WaitForMultipleObjects returns out of bound index %d. Please debug!",
                                   dwRc),
              VERR_INTERNAL_ERROR);
        }
    }
    return VINF_SUCCESS;
}


HRESULT HostDnsServiceWin::updateInfo()
{
    HRESULT hrc;
    DWORD regIndex;
    BYTE abDomain[256];
    BYTE abNameServers[256];
    BYTE abSearchList[256];

    RT_ZERO(abDomain);
    RT_ZERO(abNameServers);
    RT_ZERO(abSearchList);

    regIndex = 0;
    do {
        CHAR keyName[256];
        DWORD cbKeyName = sizeof(keyName);
        DWORD keyType = 0;
        BYTE keyData[1024];
        DWORD cbKeyData = sizeof(keyData);

        hrc = RegEnumValueA(m->hKeyTcpipParameters, regIndex, keyName, &cbKeyName, 0,
                            &keyType, keyData, &cbKeyData);
        if (   hrc == ERROR_SUCCESS
            || hrc == ERROR_MORE_DATA)
        {
            if (   RTStrICmp("Domain", keyName) == 0
                && cbKeyData > 1
                && cbKeyData < sizeof(abDomain))
                memcpy(abDomain, keyData, cbKeyData);

            else if (   RTStrICmp("DhcpDomain", keyName) == 0
                     && cbKeyData > 1
                     && abDomain[0] == 0
                     && cbKeyData < sizeof(abDomain))
                memcpy(abDomain, keyData, cbKeyData);

            else if (   RTStrICmp("NameServer", keyName) == 0
                     && cbKeyData > 1
                     && cbKeyData < sizeof(abNameServers))
                memcpy(abNameServers, keyData, cbKeyData);

            else if (   RTStrICmp("DhcpNameServer", keyName) == 0
                     && cbKeyData > 1
                     && abNameServers[0] == 0
                     && cbKeyData < sizeof(abNameServers))
                memcpy(abNameServers, keyData, cbKeyData);

            else if (   RTStrICmp("SearchList", keyName) == 0
                     && cbKeyData > 1
                     && cbKeyData < sizeof(abSearchList))
              memcpy(abSearchList, keyData, cbKeyData);
        }
        regIndex++;
    } while (hrc != ERROR_NO_MORE_ITEMS);

    /* OK, now parse and update DNS structures. */
    /* domain name */
    HostDnsInformation info;
    info.domain = (char*)abDomain;

    /* server list */
    strList2List(info.servers, (char *)abNameServers);
    /* search list */
    strList2List(info.searchList, (char *)abSearchList);

    HostDnsMonitor::setInfo(info);

    return S_OK;
}


void HostDnsServiceWin::strList2List(std::vector<std::string>& lst, char *strLst)
{
    char *next, *current;
    char address[512];

    AssertPtrReturnVoid(strLst);

    if (strlen(strLst) == 0)
      return;

    current = strLst;
    do {
        RT_ZERO(address);
        next = RTStrStr(current, " ");

        if (next)
          strncpy(address, current, RT_MIN(sizeof(address)-1, next - current));
        else
          strcpy(address, current);

        lst.push_back(std::string(address));

        current = next + 1;
    } while(next);

}
