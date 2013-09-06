/* -*- indent-tabs-mode: nil; -*- */
#include <VBox/com/string.h>
#include <VBox/com/ptr.h>

#include "../HostDnsService.h"

#include <iprt/assert.h>
#include <iprt/err.h>

#include <Windows.h>

static HKEY g_hKeyTcpipParameters;

HostDnsServiceWin::HostDnsServiceWin()
{
    RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
		 TEXT("SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters"), 
		 0, KEY_READ, &g_hKeyTcpipParameters);
}


HostDnsServiceWin::~HostDnsServiceWin()
{
    if (!g_hKeyTcpipParameters)
    {
	RegCloseKey(g_hKeyTcpipParameters);
        g_hKeyTcpipParameters = 0;
    }
}


HRESULT HostDnsServiceWin::init(void)
{
    HRESULT hrc;
    hrc = HostDnsService::init();
    AssertComRCReturn(hrc, hrc);

    hrc = update();
    AssertComRCReturn(hrc, hrc);

    return S_OK;
}


HRESULT HostDnsServiceWin::start(void)
{
    return S_OK;
}


void HostDnsServiceWin::stop(void)
{
}


HRESULT HostDnsServiceWin::update()
{
    HRESULT hrc;
    DWORD regIndex;
    BYTE abDomain[256];
    BYTE abNameServers[256];
    BYTE abSearchList[256];

    m_llNameServers.clear();
    m_llSearchStrings.clear();
    m_DomainName.setNull();

    RT_ZERO(abDomain);
    RT_ZERO(abNameServers);
    RT_ZERO(abSearchList);
    
    regIndex = 0;
    do {
        CHAR keyName[256];
        DWORD cbKeyName = 256;
        DWORD keyType = 0;
        BYTE keyData[1024];
        DWORD cbKeyData = 1024;
    
        hrc = RegEnumValueA(g_hKeyTcpipParameters, regIndex, keyName, &cbKeyName, 0,
                            &keyType, keyData, &cbKeyData);
        if (   hrc == ERROR_SUCCESS 
            || hrc == ERROR_MORE_DATA)
        {
            if (   RTStrICmp("Domain", keyName) == 0 
                && cbKeyData > 1
                && cbKeyData < 256)
                memcpy(abDomain, keyData, cbKeyData);

            else if (   RTStrICmp("DhcpDomain", keyName) == 0
                     && cbKeyData > 1
                     && abDomain[0] == 0
                     && cbKeyData < 256)
                memcpy(abDomain, keyData, cbKeyData);

            else if (   RTStrICmp("NameServer", keyName) == 0 
                     && cbKeyData > 1
                     && cbKeyData < 256)
                memcpy(abNameServers, keyData, cbKeyData);
      
            else if (   RTStrICmp("DhcpNameServer", keyName) == 0 
                     && cbKeyData > 1
                     && abNameServers[0] == 0
                     && cbKeyData < 256)
                memcpy(abNameServers, keyData, cbKeyData);

            else if (   RTStrICmp("SearchList", keyName) == 0 
                     && cbKeyData > 1
                     && cbKeyData < 256)
              memcpy(abSearchList, keyData, cbKeyData);
        }
        regIndex++;
    } while (hrc != ERROR_NO_MORE_ITEMS);

    /* OK, now parse and update DNS structures. */
    /* domain name */
    m_DomainName = com::Utf8Str((char *)abDomain);
    /* server list */
    strList2List(m_llNameServers, (char *)abNameServers);
    /* search list */
    strList2List(m_llSearchStrings, (char *)abNameServers);
    
    return S_OK;
}



void HostDnsServiceWin::strList2List(Utf8StrList& lst, char *strLst)
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
          strncpy(address, current, next - current);
        else
          strcpy(address, current);

        lst.push_back(com::Utf8Str(address));

        current = next + 1;
    } while(next);

}
