/* $Id$ */
/** @file
 * Main - NetIfList, Windows implementation.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */



/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_MAIN

#include <iprt/asm.h>
#include <iprt/err.h>
#include <list>

#define _WIN32_DCOM
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#ifdef VBOX_WITH_NETFLT
#include "VBox/WinNetConfig.h"
#include "devguid.h"
#endif

#include <iphlpapi.h>

#include "Logging.h"
#include "HostNetworkInterfaceImpl.h"
#include "netif.h"

#ifdef VBOX_WITH_NETFLT
#include <Wbemidl.h>
#include <comdef.h>

static HRESULT netIfWinCreateIWbemServices(IWbemServices ** ppSvc)
{
    HRESULT hres;

    // Step 3: ---------------------------------------------------
    // Obtain the initial locator to WMI -------------------------

    IWbemLocator *pLoc = NULL;

    hres = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID *) &pLoc);
    if(SUCCEEDED(hres))
    {
        // Step 4: -----------------------------------------------------
        // Connect to WMI through the IWbemLocator::ConnectServer method

        IWbemServices *pSvc = NULL;

        // Connect to the root\cimv2 namespace with
        // the current user and obtain pointer pSvc
        // to make IWbemServices calls.
        hres = pLoc->ConnectServer(
             _bstr_t(L"ROOT\\CIMV2"), // Object path of WMI namespace
             NULL,                    // User name. NULL = current user
             NULL,                    // User password. NULL = current
             0,                       // Locale. NULL indicates current
             NULL,                    // Security flags.
             0,                       // Authority (e.g. Kerberos)
             0,                       // Context object
             &pSvc                    // pointer to IWbemServices proxy
             );
        if(SUCCEEDED(hres))
        {
            LogRel(("Connected to ROOT\\CIMV2 WMI namespace\n"));

            // Step 5: --------------------------------------------------
            // Set security levels on the proxy -------------------------

            hres = CoSetProxyBlanket(
               pSvc,                        // Indicates the proxy to set
               RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
               RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
               NULL,                        // Server principal name
               RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx
               RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
               NULL,                        // client identity
               EOAC_NONE                    // proxy capabilities
            );
            if(SUCCEEDED(hres))
            {
                *ppSvc = pSvc;
                /* do not need it any more */
                pLoc->Release();
                return hres;
            }
            else
            {
                LogRel(("Could not set proxy blanket. Error code = 0x%x\n", hres));
            }

            pSvc->Release();
        }
        else
        {
            LogRel(("Could not connect. Error code = 0x%x\n", hres));
        }

        pLoc->Release();
    }
    else
    {
        LogRel(("Failed to create IWbemLocator object. Err code = 0x%x\n", hres));
//        CoUninitialize();
    }

    return hres;
}

static HRESULT netIfWinFindAdapterClassById(IWbemServices * pSvc, GUID * pGuid, IWbemClassObject **pAdapterConfig)
{
    HRESULT hres;
    WCHAR aQueryString[256];
    char uuidStr[RTUUID_STR_LENGTH];
    int rc = RTUuidToStr((PCRTUUID)pGuid, uuidStr, sizeof(uuidStr));
    if(RT_SUCCESS(rc))
    {
        swprintf(aQueryString, L"SELECT * FROM Win32_NetworkAdapterConfiguration WHERE SettingID = \"{%S}\"", uuidStr);
        // Step 6: --------------------------------------------------
        // Use the IWbemServices pointer to make requests of WMI ----

        IEnumWbemClassObject* pEnumerator = NULL;
        hres = pSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t(aQueryString),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            NULL,
            &pEnumerator);
        if(SUCCEEDED(hres))
        {
            // Step 7: -------------------------------------------------
            // Get the data from the query in step 6 -------------------

            IWbemClassObject *pclsObj;
            ULONG uReturn = 0;

            while (pEnumerator)
            {
                HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1,
                    &pclsObj, &uReturn);

                if(SUCCEEDED(hres))
                {
                    if(uReturn)
                    {
                        pEnumerator->Release();
                        *pAdapterConfig = pclsObj;
                        hres = S_OK;
                        return hres;
                    }
                    else
                    {
                        hres = S_FALSE;
                    }
                }

            }
            pEnumerator->Release();
        }
        else
        {
            Log(("Query for operating system name failed. Error code = 0x%x\n", hres));
        }
    }
    else
    {
        hres = -1;
    }

    return hres;
}

static HRESULT netIfAdapterConfigPath(IWbemClassObject *pObj, BSTR * pStr)
{
    VARIANT index;

    // Get the value of the key property
    HRESULT hr = pObj->Get(L"Index", 0, &index, 0, 0);
    if(SUCCEEDED(hr))
    {
        WCHAR strIndex[8];
        swprintf(strIndex, L"%u", index.uintVal);
        *pStr = (bstr_t(L"Win32_NetworkAdapterConfiguration.Index='") + strIndex + "'").copy();
    }
    else
    {
        DWORD dwError = GetLastError();
        Assert(0);
        hr = HRESULT_FROM_WIN32( dwError );
    }
    return hr;
}

static HRESULT netIfExecMethod(IWbemServices * pSvc, IWbemClassObject *pClass, BSTR ObjPath,
        BSTR MethodName, LPWSTR *pArgNames, LPVARIANT *pArgs, UINT cArgs,
        IWbemClassObject** ppOutParams
        )
{
    HRESULT hres;
    // Step 6: --------------------------------------------------
    // Use the IWbemServices pointer to make requests of WMI ----

    IWbemClassObject* pInParamsDefinition = NULL;
    hres = pClass->GetMethod(MethodName, 0,
        &pInParamsDefinition, NULL);
    if(SUCCEEDED(hres))
    {
        IWbemClassObject* pClassInstance = NULL;
        hres = pInParamsDefinition->SpawnInstance(0, &pClassInstance);

        if(SUCCEEDED(hres))
        {
            for(UINT i = 0; i < cArgs; i++)
            {
                // Store the value for the in parameters
                hres = pClassInstance->Put(pArgNames[i], 0,
                    pArgs[i], 0);
                if(FAILED(hres))
                {
                    break;
                }
            }

            if(SUCCEEDED(hres))
            {
                IWbemClassObject* pOutParams = NULL;
                hres = pSvc->ExecMethod(ObjPath, MethodName, 0,
                        NULL, pClassInstance, &pOutParams, NULL);
                if(SUCCEEDED(hres))
                {
                    *ppOutParams = pOutParams;
                }
            }

            pClassInstance->Release();
        }

        pInParamsDefinition->Release();
    }

    return hres;
}

static HRESULT createIpArray(SAFEARRAY **ppArray, in_addr* aIp, UINT cIp)
{
    HRESULT hr;
    SAFEARRAY * pIpArray = SafeArrayCreateVector(VT_BSTR, 0, cIp);
    if(pIpArray)
    {
        for(UINT i = 0; i < cIp; i++)
        {
            char* addr = inet_ntoa(aIp[i]);
            BSTR val = bstr_t(addr).copy();
            long aIndex[1];
            aIndex[0] = i;
            hr = SafeArrayPutElement(pIpArray, aIndex, val);
            if(FAILED(hr))
            {
                SysFreeString(val);
                SafeArrayDestroy(pIpArray);
                break;
            }
        }

        if(SUCCEEDED(hr))
        {
            *ppArray = pIpArray;
        }
    }
    else
    {
        DWORD dwError = GetLastError();
        Assert(0);
        hr = HRESULT_FROM_WIN32( dwError );
    }

    return hr;
}

static HRESULT VBoxNetCfgWinEnableStatic(IWbemServices * pSvc, IWbemClassObject *pObj,  in_addr* aIp,  in_addr * aMask, UINT cIp)
{
    IWbemClassObject * pClass;
    BSTR ClassName = SysAllocString(L"Win32_NetworkAdapterConfiguration");
    HRESULT hr;
    if(ClassName)
    {
        hr = pSvc->GetObject(ClassName, 0, NULL, &pClass, NULL);
        if(SUCCEEDED(hr))
        {
            BSTR ObjPath;
            hr = netIfAdapterConfigPath(pObj, &ObjPath);
            if(SUCCEEDED(hr))
            {
                LPWSTR argNames[] = {L"IPAddress", L"SubnetMask"};
                VARIANT ipAddresses;
                VariantInit(&ipAddresses);
                ipAddresses.vt = VT_ARRAY | VT_BSTR;
                SAFEARRAY *pIpArray;
                hr = createIpArray(&pIpArray, aIp, cIp);
                if(SUCCEEDED(hr))
                {
                    ipAddresses.parray = pIpArray;
                    VARIANT ipMasks;
                    VariantInit(&ipMasks);
                    ipMasks.vt = VT_ARRAY | VT_BSTR;
                    SAFEARRAY *pMaskArray;
                    hr = createIpArray(&pMaskArray, aMask, cIp);
                    if(SUCCEEDED(hr))
                    {
                        ipMasks.parray = pMaskArray;
                        LPVARIANT args[] = {&ipAddresses, &ipMasks};
                        IWbemClassObject * pOutParams;

                        hr = netIfExecMethod(pSvc, pClass, ObjPath,
                                bstr_t(L"EnableStatic"), argNames, args, 2, &pOutParams);
                        if(SUCCEEDED(hr))
                        {
                        }
                        SafeArrayDestroy(pMaskArray);
                    }
                    SafeArrayDestroy(pIpArray);
                }
                SysFreeString(ObjPath);
            }
            pClass->Release();
        }
        SysFreeString(ClassName);
    }
    else
    {
        DWORD dwError = GetLastError();
        Assert(0);
        hr = HRESULT_FROM_WIN32( dwError );
    }

    return hr;
}


static int collectNetIfInfo(Bstr &strName, PNETIFINFO pInfo)
{
    DWORD dwRc;
    /*
     * Most of the hosts probably have less than 10 adapters,
     * so we'll mostly succeed from the first attempt.
     */
    ULONG uBufLen = sizeof(IP_ADAPTER_ADDRESSES) * 10;
    PIP_ADAPTER_ADDRESSES pAddresses = (PIP_ADAPTER_ADDRESSES)RTMemAlloc(uBufLen);
    if (!pAddresses)
        return VERR_NO_MEMORY;
    dwRc = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &uBufLen);
    if (dwRc == ERROR_BUFFER_OVERFLOW)
    {
        /* Impressive! More than 10 adapters! Get more memory and try again. */
        RTMemFree(pAddresses);
        pAddresses = (PIP_ADAPTER_ADDRESSES)RTMemAlloc(uBufLen);
        if (!pAddresses)
            return VERR_NO_MEMORY;
        dwRc = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &uBufLen);
    }
    if (dwRc == NO_ERROR)
    {
        PIP_ADAPTER_ADDRESSES pAdapter;
        for (pAdapter = pAddresses; pAdapter; pAdapter = pAdapter->Next)
        {
            char *pszUuid = RTStrDup(pAdapter->AdapterName);
            size_t len = strlen(pszUuid) - 1;
            if (pszUuid[0] == '{' && pszUuid[len] == '}')
            {
                pszUuid[len] = 0;
                if (!RTUuidCompareStr(&pInfo->Uuid, pszUuid + 1))
                {
                    bool fIPFound, fIPv6Found;
                    PIP_ADAPTER_UNICAST_ADDRESS pAddr;
                    fIPFound = fIPv6Found = false;
                    for (pAddr = pAdapter->FirstUnicastAddress; pAddr; pAddr = pAddr->Next)
                    {
                        switch (pAddr->Address.lpSockaddr->sa_family)
                        {
                            case AF_INET:
                                if (!fIPFound)
                                {
                                    fIPFound = true;
                                    memcpy(&pInfo->IPAddress,
                                        &((struct sockaddr_in *)pAddr->Address.lpSockaddr)->sin_addr.s_addr,
                                        sizeof(pInfo->IPAddress));
                                }
                                break;
                            case AF_INET6:
                                if (!fIPv6Found)
                                {
                                    fIPv6Found = true;
                                    memcpy(&pInfo->IPv6Address,
                                        ((struct sockaddr_in6 *)pAddr->Address.lpSockaddr)->sin6_addr.s6_addr,
                                        sizeof(pInfo->IPv6Address));
                                }
                                break;
                        }
                    }
                    PIP_ADAPTER_PREFIX pPrefix;
                    fIPFound = fIPv6Found = false;
                    for (pPrefix = pAdapter->FirstPrefix; pPrefix; pPrefix = pPrefix->Next)
                    {
                        switch (pPrefix->Address.lpSockaddr->sa_family)
                        {
                            case AF_INET:
                                if (!fIPFound)
                                {
                                    fIPFound = true;
                                    ASMBitSetRange(&pInfo->IPNetMask, 0, pPrefix->PrefixLength);
                                }
                                break;
                            case AF_INET6:
                                if (!fIPv6Found)
                                {
                                    fIPv6Found = true;
                                    ASMBitSetRange(&pInfo->IPv6NetMask, 0, pPrefix->PrefixLength);
                                }
                                break;
                        }
                    }
                    if (sizeof(pInfo->MACAddress) != pAdapter->PhysicalAddressLength)
                        Log(("collectNetIfInfo: Unexpected physical address length: %u\n", pAdapter->PhysicalAddressLength));
                    else
                        memcpy(pInfo->MACAddress.au8, pAdapter->PhysicalAddress, sizeof(pInfo->MACAddress));
                    pInfo->enmType = NETIF_T_ETHERNET;
                    pInfo->enmStatus = pAdapter->OperStatus == IfOperStatusUp ? NETIF_S_UP : NETIF_S_DOWN;
                    RTStrFree(pszUuid);
                    break;
                }
            }
            RTStrFree(pszUuid);
        }
    }
    RTMemFree(pAddresses);

    return VINF_SUCCESS;
}

# define VBOX_APP_NAME L"VirtualBox"

static int vboxNetWinAddComponent(std::list <ComObjPtr <HostNetworkInterface> > * pPist, INetCfgComponent * pncc, bool bReal)
{
    LPWSTR              lpszName;
    GUID                IfGuid;
    HRESULT hr;
    int rc = VERR_GENERAL_FAILURE;

    hr = pncc->GetDisplayName( &lpszName );
    Assert(hr == S_OK);
    if(hr == S_OK)
    {
        size_t cUnicodeName = wcslen(lpszName) + 1;
        size_t uniLen = (cUnicodeName * 2 + sizeof (OLECHAR) - 1) / sizeof (OLECHAR);
        Bstr name (uniLen + 1 /* extra zero */);
        wcscpy((wchar_t *) name.mutableRaw(), lpszName);

        hr = pncc->GetInstanceGuid(&IfGuid);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            NETIFINFO Info;
            memset(&Info, 0, sizeof(Info));
            Info.Uuid = *(Guid(IfGuid).raw());
            rc = collectNetIfInfo(name, &Info);
            if (RT_FAILURE(rc))
            {
                Log(("vboxNetWinAddComponent: collectNetIfInfo() -> %Vrc\n", rc));
            }
            /* create a new object and add it to the list */
            ComObjPtr <HostNetworkInterface> iface;
            iface.createObject();
            /* remove the curly bracket at the end */
            if (SUCCEEDED (iface->init (name, bReal, &Info)))
            {
                pPist->push_back (iface);
                rc = VINF_SUCCESS;
            }
            else
            {
                Assert(0);
            }
        }
        CoTaskMemFree(lpszName);
    }

    return rc;
}

#else /* #ifndef VBOX_WITH_NETFLT */
/**
 * Windows helper function for NetIfList().
 *
 * @returns true / false.
 *
 * @param   guid        The GUID.
 */
static bool IsTAPDevice(const char *guid)
{
    HKEY hNetcard;
    LONG status;
    DWORD len;
    int i = 0;
    bool ret = false;

    status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}", 0, KEY_READ, &hNetcard);
    if (status != ERROR_SUCCESS)
        return false;

    for (;;)
    {
        char szEnumName[256];
        char szNetCfgInstanceId[256];
        DWORD dwKeyType;
        HKEY  hNetCardGUID;

        len = sizeof(szEnumName);
        status = RegEnumKeyExA(hNetcard, i, szEnumName, &len, NULL, NULL, NULL, NULL);
        if (status != ERROR_SUCCESS)
            break;

        status = RegOpenKeyExA(hNetcard, szEnumName, 0, KEY_READ, &hNetCardGUID);
        if (status == ERROR_SUCCESS)
        {
            len = sizeof(szNetCfgInstanceId);
            status = RegQueryValueExA(hNetCardGUID, "NetCfgInstanceId", NULL, &dwKeyType, (LPBYTE)szNetCfgInstanceId, &len);
            if (status == ERROR_SUCCESS && dwKeyType == REG_SZ)
            {
                char szNetProductName[256];
                char szNetProviderName[256];

                szNetProductName[0] = 0;
                len = sizeof(szNetProductName);
                status = RegQueryValueExA(hNetCardGUID, "ProductName", NULL, &dwKeyType, (LPBYTE)szNetProductName, &len);

                szNetProviderName[0] = 0;
                len = sizeof(szNetProviderName);
                status = RegQueryValueExA(hNetCardGUID, "ProviderName", NULL, &dwKeyType, (LPBYTE)szNetProviderName, &len);

                if (   !strcmp(szNetCfgInstanceId, guid)
                    && !strcmp(szNetProductName, "VirtualBox TAP Adapter")
                    && (   (!strcmp(szNetProviderName, "innotek GmbH"))
                        || (!strcmp(szNetProviderName, "Sun Microsystems, Inc."))))
                {
                    ret = true;
                    RegCloseKey(hNetCardGUID);
                    break;
                }
            }
            RegCloseKey(hNetCardGUID);
        }
        ++i;
    }

    RegCloseKey(hNetcard);
    return ret;
}
#endif /* #ifndef VBOX_WITH_NETFLT */


static int NetIfListHostAdapters(std::list <ComObjPtr <HostNetworkInterface> > &list)
{
#ifndef VBOX_WITH_NETFLT
    /* VBoxNetAdp is available only when VBOX_WITH_NETFLT is enabled */
    return VERR_NOT_IMPLEMENTED;
#else /* #  if defined VBOX_WITH_NETFLT */
    INetCfg              *pNc;
    INetCfgComponent     *pMpNcc;
    LPWSTR               lpszApp = NULL;
    HRESULT              hr;
    IEnumNetCfgComponent  *pEnumComponent;

    /* we are using the INetCfg API for getting the list of miniports */
    hr = VBoxNetCfgWinQueryINetCfg( FALSE,
                       VBOX_APP_NAME,
                       &pNc,
                       &lpszApp );
    Assert(hr == S_OK);
    if(hr == S_OK)
    {
        hr = VBoxNetCfgWinGetComponentEnum(pNc, &GUID_DEVCLASS_NET, &pEnumComponent);
        if(hr == S_OK)
        {
            while((hr = VBoxNetCfgWinGetNextComponent(pEnumComponent, &pMpNcc)) == S_OK)
            {
                ULONG uComponentStatus;
                hr = pMpNcc->GetDeviceStatus(&uComponentStatus);
#ifndef DEBUG_bird
                Assert(hr == S_OK);
#endif
                if(hr == S_OK)
                {
                    if(uComponentStatus == 0)
                    {
                        LPWSTR pId;
                        hr = pMpNcc->GetId(&pId);
                        Assert(hr == S_OK);
                        if(hr == S_OK)
                        {
                            if(!_wcsnicmp(pId, L"sun_VBoxNetAdp", sizeof(L"sun_VBoxNetAdp")/2))
                            {
                                vboxNetWinAddComponent(&list, pMpNcc, false);
                            }
                            CoTaskMemFree(pId);
                        }
                    }
                }
                VBoxNetCfgWinReleaseRef(pMpNcc);
            }
            Assert(hr == S_OK || hr == S_FALSE);

            VBoxNetCfgWinReleaseRef(pEnumComponent);
        }
        else
        {
            LogRel(("failed to get the sun_VBoxNetFlt component, error (0x%x)", hr));
        }

        VBoxNetCfgWinReleaseINetCfg(pNc, FALSE);
    }
    else if(lpszApp)
    {
        CoTaskMemFree(lpszApp);
    }
#endif /* #  if defined VBOX_WITH_NETFLT */
    return VINF_SUCCESS;
}

#if 0
//TODO: this is sample currently, hardcoded balues should be removed and exposed to the API
static int enableStatic()
{
    INetCfg *pnc;
    LPWSTR lpszLockedBy = NULL;
    int r = 1;
    HRESULT hr;

            hr = VBoxNetCfgWinQueryINetCfg(FALSE, VBOX_APP_NAME, &pnc, &lpszLockedBy);
            if(hr == S_OK)
            {
                INetCfgComponent  *pComponent;
                HRESULT hr = pnc->FindComponent(L"*msloop", &pComponent);
                if(hr == S_OK)
                {
                    GUID guid;
                    hr = pComponent->GetInstanceGuid(&guid);
                    if(SUCCEEDED(hr))
                    {
                        IWbemServices * pSvc;
                        hr = netIfWinCreateIWbemServices(&pSvc);
                        if(SUCCEEDED(hr))
                        {
                            IWbemClassObject *pAdapterConfig;
                            hr = netIfWinFindAdapterClassById(pSvc, &guid, &pAdapterConfig);
                            if(SUCCEEDED(hr))
                            {
                                in_addr ip[1];
                                in_addr mask[1];
                                ip[0].S_un.S_addr = inet_addr("192.168.5.1");
                                mask[0].S_un.S_addr = inet_addr("255.255.255.0");

                                hr = VBoxNetCfgWinEnableStatic(pSvc, pAdapterConfig,  ip,  mask, 1);
                                if(SUCCEEDED(hr))
                                {
                                    printf("succees!!!\n");
                                    r = 0;
                                }
                            }
                        }

                    }
                }
            }


    return r;
}
#endif

int NetIfList(std::list <ComObjPtr <HostNetworkInterface> > &list)
{
#ifndef VBOX_WITH_NETFLT
    static const char *NetworkKey = "SYSTEM\\CurrentControlSet\\Control\\Network\\"
                                    "{4D36E972-E325-11CE-BFC1-08002BE10318}";
    HKEY hCtrlNet;
    LONG status;
    DWORD len;
    status = RegOpenKeyExA (HKEY_LOCAL_MACHINE, NetworkKey, 0, KEY_READ, &hCtrlNet);
    if (status != ERROR_SUCCESS)
    {
        Log(("NetIfList: Could not open registry key \"%s\"", NetworkKey));
        return E_FAIL;
    }

    for (int i = 0;; ++ i)
    {
        char szNetworkGUID [256];
        HKEY hConnection;
        char szNetworkConnection [256];

        len = sizeof (szNetworkGUID);
        status = RegEnumKeyExA (hCtrlNet, i, szNetworkGUID, &len, NULL, NULL, NULL, NULL);
        if (status != ERROR_SUCCESS)
            break;

        if (!IsTAPDevice(szNetworkGUID))
            continue;

        RTStrPrintf (szNetworkConnection, sizeof (szNetworkConnection),
                     "%s\\Connection", szNetworkGUID);
        status = RegOpenKeyExA (hCtrlNet, szNetworkConnection, 0, KEY_READ,  &hConnection);
        if (status == ERROR_SUCCESS)
        {
            DWORD dwKeyType;
            status = RegQueryValueExW (hConnection, TEXT("Name"), NULL,
                                       &dwKeyType, NULL, &len);
            if (status == ERROR_SUCCESS && dwKeyType == REG_SZ)
            {
                size_t uniLen = (len + sizeof (OLECHAR) - 1) / sizeof (OLECHAR);
                Bstr name (uniLen + 1 /* extra zero */);
                status = RegQueryValueExW (hConnection, TEXT("Name"), NULL,
                                           &dwKeyType, (LPBYTE) name.mutableRaw(), &len);
                if (status == ERROR_SUCCESS)
                {
                    LogFunc(("Connection name %ls\n", name.mutableRaw()));
                    /* put a trailing zero, just in case (see MSDN) */
                    name.mutableRaw() [uniLen] = 0;
                    /* create a new object and add it to the list */
                    ComObjPtr <HostNetworkInterface> iface;
                    iface.createObject();
                    /* remove the curly bracket at the end */
                    szNetworkGUID [strlen(szNetworkGUID) - 1] = '\0';

                    NETIFINFO Info;
                    memset(&Info, 0, sizeof(Info));
                    Info.Uuid = *(Guid(szNetworkGUID + 1).raw());
                    int rc = collectNetIfInfo(name, &Info);
                    if (RT_FAILURE(rc))
                    {
                        Log(("vboxNetWinAddComponent: collectNetIfInfo() -> %Vrc\n", rc));
                    }

                    if (SUCCEEDED (iface->init (name, TRUE, &Info)))
                        list.push_back (iface);
                }
            }
            RegCloseKey (hConnection);
        }
    }
    RegCloseKey (hCtrlNet);
#else /* #  if defined VBOX_WITH_NETFLT */
    INetCfg              *pNc;
    INetCfgComponent     *pMpNcc;
    INetCfgComponent     *pTcpIpNcc;
    LPWSTR               lpszApp;
    HRESULT              hr;
    IEnumNetCfgBindingPath      *pEnumBp;
    INetCfgBindingPath          *pBp;
    IEnumNetCfgBindingInterface *pEnumBi;
    INetCfgBindingInterface *pBi;

    /* we are using the INetCfg API for getting the list of miniports */
    hr = VBoxNetCfgWinQueryINetCfg( FALSE,
                       VBOX_APP_NAME,
                       &pNc,
                       &lpszApp );
    Assert(hr == S_OK);
    if(hr == S_OK)
    {
# ifdef VBOX_NETFLT_ONDEMAND_BIND
        /* for the protocol-based approach for now we just get all miniports the MS_TCPIP protocol binds to */
        hr = pNc->FindComponent(L"MS_TCPIP", &pTcpIpNcc);
# else
        /* for the filter-based approach we get all miniports our filter (sun_VBoxNetFlt)is bound to */
        hr = pNc->FindComponent(L"sun_VBoxNetFlt", &pTcpIpNcc);
#  ifndef VBOX_WITH_HARDENING
        if(hr != S_OK)
        {
            /* TODO: try to install the netflt from here */
        }
#  endif

# endif

        if(hr == S_OK)
        {
            hr = VBoxNetCfgWinGetBindingPathEnum(pTcpIpNcc, EBP_BELOW, &pEnumBp);
            Assert(hr == S_OK);
            if ( hr == S_OK )
            {
                hr = VBoxNetCfgWinGetFirstBindingPath(pEnumBp, &pBp);
                Assert(hr == S_OK || hr == S_FALSE);
                while( hr == S_OK )
                {
                    /* S_OK == enabled, S_FALSE == disabled */
                    if(pBp->IsEnabled() == S_OK)
                    {
                        hr = VBoxNetCfgWinGetBindingInterfaceEnum(pBp, &pEnumBi);
                        Assert(hr == S_OK);
                        if ( hr == S_OK )
                        {
                            hr = VBoxNetCfgWinGetFirstBindingInterface(pEnumBi, &pBi);
                            Assert(hr == S_OK);
                            while(hr == S_OK)
                            {
                                hr = pBi->GetLowerComponent( &pMpNcc );
                                Assert(hr == S_OK);
                                if(hr == S_OK)
                                {
                                    ULONG uComponentStatus;
                                    hr = pMpNcc->GetDeviceStatus(&uComponentStatus);
//#ifndef DEBUG_bird
//                                    Assert(hr == S_OK);
//#endif
                                    if(hr == S_OK)
                                    {
                                        if(uComponentStatus == 0)
                                        {
                                            vboxNetWinAddComponent(&list, pMpNcc, true);
                                        }
                                    }
                                    VBoxNetCfgWinReleaseRef( pMpNcc );
                                }
                                VBoxNetCfgWinReleaseRef(pBi);

                                hr = VBoxNetCfgWinGetNextBindingInterface(pEnumBi, &pBi);
                            }
                            VBoxNetCfgWinReleaseRef(pEnumBi);
                        }
                    }
                    VBoxNetCfgWinReleaseRef(pBp);

                    hr = VBoxNetCfgWinGetNextBindingPath(pEnumBp, &pBp);
                }
                VBoxNetCfgWinReleaseRef(pEnumBp);
            }
            VBoxNetCfgWinReleaseRef(pTcpIpNcc);
        }
        else
        {
            LogRel(("failed to get the sun_VBoxNetFlt component, error (0x%x)", hr));
        }

        VBoxNetCfgWinReleaseINetCfg(pNc, FALSE);
    }

    NetIfListHostAdapters(list);
#endif /* #  if defined VBOX_WITH_NETFLT */
    return VINF_SUCCESS;
}
