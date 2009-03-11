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
#include "ProgressImpl.h"
#include "VirtualBoxImpl.h"
#include "netif.h"

#ifdef VBOX_WITH_NETFLT
#include <Wbemidl.h>
#include <comdef.h>

#include "svchlp.h"

#include <shellapi.h>
#define INITGUID
#include <guiddef.h>
#include <devguid.h>
#include <objbase.h>
#include <setupapi.h>
#include <shlobj.h>
#include <cfgmgr32.h>

#define VBOX_APP_NAME L"VirtualBox"

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

static HRESULT netIfWinFindAdapterClassById(IWbemServices * pSvc, const Guid &guid, IWbemClassObject **pAdapterConfig)
{
    HRESULT hres;
    WCHAR aQueryString[256];
//    char uuidStr[RTUUID_STR_LENGTH];
//    int rc = RTUuidToStr(guid.toString().raw(), uuidStr, sizeof(uuidStr));
    {
        swprintf(aQueryString, L"SELECT * FROM Win32_NetworkAdapterConfiguration WHERE SettingID = \"{%S}\"", guid.toString().raw());
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

    return hres;
}

static HRESULT netIfWinAdapterConfigPath(IWbemClassObject *pObj, BSTR * pStr)
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
    HRESULT hres = S_OK;
    // Step 6: --------------------------------------------------
    // Use the IWbemServices pointer to make requests of WMI ----

    ComPtr<IWbemClassObject> pInParamsDefinition;
    ComPtr<IWbemClassObject> pClassInstance;

    if(cArgs)
    {
        hres = pClass->GetMethod(MethodName, 0,
            pInParamsDefinition.asOutParam(), NULL);
        if(SUCCEEDED(hres))
        {
            hres = pInParamsDefinition->SpawnInstance(0, pClassInstance.asOutParam());

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
            }
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

    return hres;
}

static HRESULT netIfWinCreateIpArray(SAFEARRAY **ppArray, in_addr* aIp, UINT cIp)
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

static HRESULT netIfWinCreateIpArrayV4V6(SAFEARRAY **ppArray, BSTR Ip)
{
    HRESULT hr;
    SAFEARRAY * pIpArray = SafeArrayCreateVector(VT_BSTR, 0, 1);
    if(pIpArray)
    {
        BSTR val = bstr_t(Ip, false).copy();
        long aIndex[1];
        aIndex[0] = 0;
        hr = SafeArrayPutElement(pIpArray, aIndex, val);
        if(FAILED(hr))
        {
            SysFreeString(val);
            SafeArrayDestroy(pIpArray);
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


static HRESULT netIfWinCreateIpArrayVariantV4(VARIANT * pIpAddresses, in_addr* aIp, UINT cIp)
{
    HRESULT hr;
    VariantInit(pIpAddresses);
    pIpAddresses->vt = VT_ARRAY | VT_BSTR;
    SAFEARRAY *pIpArray;
    hr = netIfWinCreateIpArray(&pIpArray, aIp, cIp);
    if(SUCCEEDED(hr))
    {
        pIpAddresses->parray = pIpArray;
    }
    return hr;
}

static HRESULT netIfWinCreateIpArrayVariantV4V6(VARIANT * pIpAddresses, BSTR Ip)
{
    HRESULT hr;
    VariantInit(pIpAddresses);
    pIpAddresses->vt = VT_ARRAY | VT_BSTR;
    SAFEARRAY *pIpArray;
    hr = netIfWinCreateIpArrayV4V6(&pIpArray, Ip);
    if(SUCCEEDED(hr))
    {
        pIpAddresses->parray = pIpArray;
    }
    return hr;
}

static HRESULT netIfWinEnableStatic(IWbemServices * pSvc, BSTR ObjPath, VARIANT * pIp, VARIANT * pMask)
{
    ComPtr<IWbemClassObject> pClass;
    BSTR ClassName = SysAllocString(L"Win32_NetworkAdapterConfiguration");
    HRESULT hr;
    if(ClassName)
    {
        hr = pSvc->GetObject(ClassName, 0, NULL, pClass.asOutParam(), NULL);
        if(SUCCEEDED(hr))
        {
            LPWSTR argNames[] = {L"IPAddress", L"SubnetMask"};
            LPVARIANT args[] = {pIp, pMask};
            ComPtr<IWbemClassObject> pOutParams;

            hr = netIfExecMethod(pSvc, pClass, ObjPath,
                                bstr_t(L"EnableStatic"), argNames, args, 2, pOutParams.asOutParam());
            if(SUCCEEDED(hr))
            {
                VARIANT varReturnValue;
                hr = pOutParams->Get(bstr_t(L"ReturnValue"), 0,
                    &varReturnValue, NULL, 0);
                Assert(SUCCEEDED(hr));
                if(SUCCEEDED(hr))
                {
//                    Assert(varReturnValue.vt == VT_UINT);
                    int winEr = varReturnValue.uintVal;
                    switch(winEr)
                    {
                    case 0:
                        hr = S_OK;
                        break;
                    default:
                        hr = HRESULT_FROM_WIN32( winEr );
                        break;
                    }
                }
            }
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


static HRESULT netIfWinEnableStaticV4(IWbemServices * pSvc, BSTR ObjPath, in_addr* aIp, in_addr * aMask, UINT cIp)
{
    VARIANT ipAddresses;
    HRESULT hr = netIfWinCreateIpArrayVariantV4(&ipAddresses, aIp, cIp);
    if(SUCCEEDED(hr))
    {
        VARIANT ipMasks;
        hr = netIfWinCreateIpArrayVariantV4(&ipMasks, aMask, cIp);
        if(SUCCEEDED(hr))
        {
            netIfWinEnableStatic(pSvc, ObjPath, &ipAddresses, &ipMasks);
            VariantClear(&ipMasks);
        }
        VariantClear(&ipAddresses);
    }
    return hr;
}

static HRESULT netIfWinEnableStaticV4V6(IWbemServices * pSvc, BSTR ObjPath, BSTR Ip, BSTR Mask)
{
    VARIANT ipAddresses;
    HRESULT hr = netIfWinCreateIpArrayVariantV4V6(&ipAddresses, Ip);
    if(SUCCEEDED(hr))
    {
        VARIANT ipMasks;
        hr = netIfWinCreateIpArrayVariantV4V6(&ipMasks, Mask);
        if(SUCCEEDED(hr))
        {
            netIfWinEnableStatic(pSvc, ObjPath, &ipAddresses, &ipMasks);
            VariantClear(&ipMasks);
        }
        VariantClear(&ipAddresses);
    }
    return hr;
}

/* win API allows to set gw metrics as well, we are not setting them */
static HRESULT netIfWinSetGateways(IWbemServices * pSvc, BSTR ObjPath, VARIANT * pGw)
{
    ComPtr<IWbemClassObject> pClass;
    BSTR ClassName = SysAllocString(L"Win32_NetworkAdapterConfiguration");
    HRESULT hr;
    if(ClassName)
    {
        hr = pSvc->GetObject(ClassName, 0, NULL, pClass.asOutParam(), NULL);
        if(SUCCEEDED(hr))
        {
            LPWSTR argNames[] = {L"DefaultIPGateway"};
            LPVARIANT args[] = {pGw};
            ComPtr<IWbemClassObject> pOutParams;

            hr = netIfExecMethod(pSvc, pClass, ObjPath,
                                bstr_t(L"SetGateways"), argNames, args, 1, pOutParams.asOutParam());
            if(SUCCEEDED(hr))
            {
            }
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

/* win API allows to set gw metrics as well, we are not setting them */
static HRESULT netIfWinSetGatewaysV4(IWbemServices * pSvc, BSTR ObjPath, in_addr* aGw, UINT cGw)
{
    VARIANT gwais;
    HRESULT hr = netIfWinCreateIpArrayVariantV4(&gwais, aGw, cGw);
    if(SUCCEEDED(hr))
    {
        netIfWinSetGateways(pSvc, ObjPath, &gwais);
        VariantClear(&gwais);
    }
    return hr;
}

/* win API allows to set gw metrics as well, we are not setting them */
static HRESULT netIfWinSetGatewaysV4V6(IWbemServices * pSvc, BSTR ObjPath, BSTR Gw)
{
    VARIANT vGw;
    HRESULT hr = netIfWinCreateIpArrayVariantV4V6(&vGw, Gw);
    if(SUCCEEDED(hr))
    {
        netIfWinSetGateways(pSvc, ObjPath, &vGw);
        VariantClear(&vGw);
    }
    return hr;
}

static HRESULT netIfWinEnableDHCP(IWbemServices * pSvc, BSTR ObjPath)
{
    ComPtr<IWbemClassObject> pClass;
    BSTR ClassName = SysAllocString(L"Win32_NetworkAdapterConfiguration");
    HRESULT hr;
    if(ClassName)
    {
        hr = pSvc->GetObject(ClassName, 0, NULL, pClass.asOutParam(), NULL);
        if(SUCCEEDED(hr))
        {
            ComPtr<IWbemClassObject> pOutParams;

            hr = netIfExecMethod(pSvc, pClass, ObjPath,
                                bstr_t(L"EnableDHCP"), NULL, NULL, 0, pOutParams.asOutParam());
            if(SUCCEEDED(hr))
            {
            }
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
                    pInfo->enmMediumType = NETIF_T_ETHERNET;
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

//static HRESULT netIfWinUpdateConfig(HostNetworkInterface * pIf)
//{
//    NETIFINFO Info;
//    memset(&Info, 0, sizeof(Info));
//    GUID guid;
//    HRESULT hr = pIf->COMGETTER(Id) (&guid);
//    if(SUCCEEDED(hr))
//    {
//        Info.Uuid = (RTUUID)*(RTUUID*)&guid;
//        BSTR name;
//        hr = pIf->COMGETTER(Name) (&name);
//        Assert(hr == S_OK);
//        if(hr == S_OK)
//        {
//            int rc = collectNetIfInfo(Bstr(name), &Info);
//            if (RT_SUCCESS(rc))
//            {
//                hr = pIf->updateConfig(&Info);
//            }
//            else
//            {
//                Log(("netIfWinUpdateConfig: collectNetIfInfo() -> %Vrc\n", rc));
//                hr = E_FAIL;
//            }
//        }
//    }
//
//    return hr;
//}

static int netIfEnableStaticIpConfig(const Guid &guid, ULONG ip, ULONG mask)
{
    HRESULT hr;
        ComPtr <IWbemServices> pSvc;
        hr = netIfWinCreateIWbemServices(pSvc.asOutParam());
        if(SUCCEEDED(hr))
        {
            ComPtr <IWbemClassObject> pAdapterConfig;
            hr = netIfWinFindAdapterClassById(pSvc, guid, pAdapterConfig.asOutParam());
            if(SUCCEEDED(hr))
            {
                in_addr aIp[1];
                in_addr aMask[1];
                aIp[0].S_un.S_addr = ip;
                aMask[0].S_un.S_addr = mask;

                BSTR ObjPath;
                hr = netIfWinAdapterConfigPath(pAdapterConfig, &ObjPath);
                if(SUCCEEDED(hr))
                {
                    hr = netIfWinEnableStaticV4(pSvc, ObjPath, aIp, aMask, 1);
                    if(SUCCEEDED(hr))
                    {
#if 0
                        in_addr aGw[1];
                        aGw[0].S_un.S_addr = gw;
                        hr = netIfWinSetGatewaysV4(pSvc, ObjPath, aGw, 1);
                        if(SUCCEEDED(hr))
#endif
                        {
//                            hr = netIfWinUpdateConfig(pIf);
                        }
                    }
                    SysFreeString(ObjPath);
                }
            }
        }

    return SUCCEEDED(hr) ? VINF_SUCCESS : VERR_GENERAL_FAILURE;
}

static int netIfEnableStaticIpConfigV6(const Guid & guid, IN_BSTR aIPV6Address, IN_BSTR aIPV6Mask, IN_BSTR aIPV6DefaultGateway)
{
    HRESULT hr;
        ComPtr <IWbemServices> pSvc;
        hr = netIfWinCreateIWbemServices(pSvc.asOutParam());
        if(SUCCEEDED(hr))
        {
            ComPtr <IWbemClassObject> pAdapterConfig;
            hr = netIfWinFindAdapterClassById(pSvc, guid, pAdapterConfig.asOutParam());
            if(SUCCEEDED(hr))
            {
                BSTR ObjPath;
                hr = netIfWinAdapterConfigPath(pAdapterConfig, &ObjPath);
                if(SUCCEEDED(hr))
                {
                    hr = netIfWinEnableStaticV4V6(pSvc, ObjPath, aIPV6Address, aIPV6Mask);
                    if(SUCCEEDED(hr))
                    {
                        if(aIPV6DefaultGateway)
                        {
                            hr = netIfWinSetGatewaysV4V6(pSvc, ObjPath, aIPV6DefaultGateway);
                        }
                        if(SUCCEEDED(hr))
                        {
//                            hr = netIfWinUpdateConfig(pIf);
                        }
                    }
                    SysFreeString(ObjPath);
                }
            }
        }

    return SUCCEEDED(hr) ? VINF_SUCCESS : VERR_GENERAL_FAILURE;
}

static int netIfEnableStaticIpConfigV6(const Guid &guid, IN_BSTR aIPV6Address, ULONG aIPV6MaskPrefixLength)
{
    RTNETADDRIPV6 Mask;
    int rc = prefixLength2IPv6Address(aIPV6MaskPrefixLength, &Mask);
    if(RT_SUCCESS(rc))
    {
        Bstr maskStr = composeIPv6Address(&Mask);
        rc = netIfEnableStaticIpConfigV6(guid, aIPV6Address, maskStr, NULL);
    }
    return rc;
}

static HRESULT netIfEnableDynamicIpConfig(const Guid &guid)
{
    HRESULT hr;
        ComPtr <IWbemServices> pSvc;
        hr = netIfWinCreateIWbemServices(pSvc.asOutParam());
        if(SUCCEEDED(hr))
        {
            ComPtr <IWbemClassObject> pAdapterConfig;
            hr = netIfWinFindAdapterClassById(pSvc, guid, pAdapterConfig.asOutParam());
            if(SUCCEEDED(hr))
            {
                BSTR ObjPath;
                hr = netIfWinAdapterConfigPath(pAdapterConfig, &ObjPath);
                if(SUCCEEDED(hr))
                {
                    hr = netIfWinEnableDHCP(pSvc, ObjPath);
                    if(SUCCEEDED(hr))
                    {
//                        hr = netIfWinUpdateConfig(pIf);
                    }
                    SysFreeString(ObjPath);
                }
            }
        }


    return hr;
}

/* svc helper func */

struct StaticIpConfig
{
    ULONG  IPAddress;
    ULONG  IPNetMask;
};

struct StaticIpV6Config
{
    BSTR           IPV6Address;
    ULONG          IPV6NetMaskLength;
};

struct NetworkInterfaceHelperClientData
{
    SVCHlpMsg::Code msgCode;
    /* for SVCHlpMsg::CreateHostOnlyNetworkInterface */
    Bstr name;
    ComObjPtr <HostNetworkInterface> iface;
    /* for SVCHlpMsg::RemoveHostOnlyNetworkInterface */
    Guid guid;

    union
    {
        StaticIpConfig StaticIP;
        StaticIpV6Config StaticIPV6;
    } u;


};

static HRESULT netIfNetworkInterfaceHelperClient (SVCHlpClient *aClient,
                                            Progress *aProgress,
                                            void *aUser, int *aVrc)
{
    LogFlowFuncEnter();
    LogFlowFunc (("aClient={%p}, aProgress={%p}, aUser={%p}\n",
                  aClient, aProgress, aUser));

    AssertReturn ((aClient == NULL && aProgress == NULL && aVrc == NULL) ||
                  (aClient != NULL && aProgress != NULL && aVrc != NULL),
                  E_POINTER);
    AssertReturn (aUser, E_POINTER);

    std::auto_ptr <NetworkInterfaceHelperClientData>
        d (static_cast <NetworkInterfaceHelperClientData *> (aUser));

    if (aClient == NULL)
    {
        /* "cleanup only" mode, just return (it will free aUser) */
        return S_OK;
    }

    HRESULT rc = S_OK;
    int vrc = VINF_SUCCESS;

    switch (d->msgCode)
    {
        case SVCHlpMsg::CreateHostOnlyNetworkInterface:
        {
            LogFlowFunc (("CreateHostOnlyNetworkInterface:\n"));
            LogFlowFunc (("Network connection name = '%ls'\n", d->name.raw()));

            /* write message and parameters */
            vrc = aClient->write (d->msgCode);
            if (RT_FAILURE (vrc)) break;
//            vrc = aClient->write (Utf8Str (d->name));
//            if (RT_FAILURE (vrc)) break;

            /* wait for a reply */
            bool endLoop = false;
            while (!endLoop)
            {
                SVCHlpMsg::Code reply = SVCHlpMsg::Null;

                vrc = aClient->read (reply);
                if (RT_FAILURE (vrc)) break;

                switch (reply)
                {
                    case SVCHlpMsg::CreateHostOnlyNetworkInterface_OK:
                    {
                        /* read the GUID */
                        Guid guid;
                        Utf8Str name;
                        vrc = aClient->read (name);
                        if (RT_FAILURE (vrc)) break;
                        vrc = aClient->read (guid);
                        if (RT_FAILURE (vrc)) break;

                        LogFlowFunc (("Network connection GUID = {%RTuuid}\n", guid.raw()));

                        /* initialize the object returned to the caller by
                         * CreateHostOnlyNetworkInterface() */
                        rc = d->iface->init (Bstr(name), guid, HostNetworkInterfaceType_HostOnly);
                        endLoop = true;
                        break;
                    }
                    case SVCHlpMsg::Error:
                    {
                        /* read the error message */
                        Utf8Str errMsg;
                        vrc = aClient->read (errMsg);
                        if (RT_FAILURE (vrc)) break;

                        rc = E_FAIL;//TODO: setError (E_FAIL, errMsg);
                        endLoop = true;
                        break;
                    }
                    default:
                    {
                        endLoop = true;
                        rc = E_FAIL;//TODO: ComAssertMsgFailedBreak ((
                            //"Invalid message code %d (%08lX)\n",
                            //reply, reply),
                            //rc = E_FAIL);
                    }
                }
            }

            break;
        }
        case SVCHlpMsg::RemoveHostOnlyNetworkInterface:
        {
            LogFlowFunc (("RemoveHostOnlyNetworkInterface:\n"));
            LogFlowFunc (("Network connection GUID = {%RTuuid}\n", d->guid.raw()));

            /* write message and parameters */
            vrc = aClient->write (d->msgCode);
            if (RT_FAILURE (vrc)) break;
            vrc = aClient->write (d->guid);
            if (RT_FAILURE (vrc)) break;

            /* wait for a reply */
            bool endLoop = false;
            while (!endLoop)
            {
                SVCHlpMsg::Code reply = SVCHlpMsg::Null;

                vrc = aClient->read (reply);
                if (RT_FAILURE (vrc)) break;

                switch (reply)
                {
                    case SVCHlpMsg::OK:
                    {
                        /* no parameters */
                        rc = S_OK;
                        endLoop = true;
                        break;
                    }
                    case SVCHlpMsg::Error:
                    {
                        /* read the error message */
                        Utf8Str errMsg;
                        vrc = aClient->read (errMsg);
                        if (RT_FAILURE (vrc)) break;

                        rc = E_FAIL; // TODO: setError (E_FAIL, errMsg);
                        endLoop = true;
                        break;
                    }
                    default:
                    {
                        endLoop = true;
                        rc = E_FAIL; // TODO: ComAssertMsgFailedBreak ((
                            //"Invalid message code %d (%08lX)\n",
                            //reply, reply),
                            //rc = E_FAIL);
                    }
                }
            }

            break;
        }
        case SVCHlpMsg::EnableDynamicIpConfig: /* see usage in code */
        {
            LogFlowFunc (("EnableDynamicIpConfig:\n"));
            LogFlowFunc (("Network connection name = '%ls'\n", d->name.raw()));

            /* write message and parameters */
            vrc = aClient->write (d->msgCode);
            if (RT_FAILURE (vrc)) break;
            vrc = aClient->write (d->guid);
            if (RT_FAILURE (vrc)) break;

            /* wait for a reply */
            bool endLoop = false;
            while (!endLoop)
            {
                SVCHlpMsg::Code reply = SVCHlpMsg::Null;

                vrc = aClient->read (reply);
                if (RT_FAILURE (vrc)) break;

                switch (reply)
                {
                    case SVCHlpMsg::OK:
                    {
                        /* no parameters */
                        rc = d->iface->updateConfig();
                        endLoop = true;
                        break;
                    }
                    case SVCHlpMsg::Error:
                    {
                        /* read the error message */
                        Utf8Str errMsg;
                        vrc = aClient->read (errMsg);
                        if (RT_FAILURE (vrc)) break;

                        rc = E_FAIL; // TODO: setError (E_FAIL, errMsg);
                        endLoop = true;
                        break;
                    }
                    default:
                    {
                        endLoop = true;
                        rc = E_FAIL; // TODO: ComAssertMsgFailedBreak ((
                            //"Invalid message code %d (%08lX)\n",
                            //reply, reply),
                            //rc = E_FAIL);
                    }
                }
            }

            break;
        }
        case SVCHlpMsg::EnableStaticIpConfig: /* see usage in code */
        {
            LogFlowFunc (("EnableStaticIpConfig:\n"));
            LogFlowFunc (("Network connection name = '%ls'\n", d->name.raw()));

            /* write message and parameters */
            vrc = aClient->write (d->msgCode);
            if (RT_FAILURE (vrc)) break;
            vrc = aClient->write (d->guid);
            if (RT_FAILURE (vrc)) break;
            vrc = aClient->write (d->u.StaticIP.IPAddress);
            if (RT_FAILURE (vrc)) break;
            vrc = aClient->write (d->u.StaticIP.IPNetMask);
            if (RT_FAILURE (vrc)) break;

            /* wait for a reply */
            bool endLoop = false;
            while (!endLoop)
            {
                SVCHlpMsg::Code reply = SVCHlpMsg::Null;

                vrc = aClient->read (reply);
                if (RT_FAILURE (vrc)) break;

                switch (reply)
                {
                    case SVCHlpMsg::OK:
                    {
                        /* no parameters */
                        rc = d->iface->updateConfig();
                        endLoop = true;
                        break;
                    }
                    case SVCHlpMsg::Error:
                    {
                        /* read the error message */
                        Utf8Str errMsg;
                        vrc = aClient->read (errMsg);
                        if (RT_FAILURE (vrc)) break;

                        rc = E_FAIL; // TODO: setError (E_FAIL, errMsg);
                        endLoop = true;
                        break;
                    }
                    default:
                    {
                        endLoop = true;
                        rc = E_FAIL; // TODO: ComAssertMsgFailedBreak ((
                            //"Invalid message code %d (%08lX)\n",
                            //reply, reply),
                            //rc = E_FAIL);
                    }
                }
            }

            break;
        }
        case SVCHlpMsg::EnableStaticIpConfigV6: /* see usage in code */
        {
            LogFlowFunc (("EnableStaticIpConfigV6:\n"));
            LogFlowFunc (("Network connection name = '%ls'\n", d->name.raw()));

            /* write message and parameters */
            vrc = aClient->write (d->msgCode);
            if (RT_FAILURE (vrc)) break;
            vrc = aClient->write (d->guid);
            if (RT_FAILURE (vrc)) break;
            vrc = aClient->write (Utf8Str(d->u.StaticIPV6.IPV6Address));
            if (RT_FAILURE (vrc)) break;
            vrc = aClient->write (d->u.StaticIPV6.IPV6NetMaskLength);
            if (RT_FAILURE (vrc)) break;

            /* wait for a reply */
            bool endLoop = false;
            while (!endLoop)
            {
                SVCHlpMsg::Code reply = SVCHlpMsg::Null;

                vrc = aClient->read (reply);
                if (RT_FAILURE (vrc)) break;

                switch (reply)
                {
                    case SVCHlpMsg::OK:
                    {
                        /* no parameters */
                        rc = d->iface->updateConfig();
                        endLoop = true;
                        break;
                    }
                    case SVCHlpMsg::Error:
                    {
                        /* read the error message */
                        Utf8Str errMsg;
                        vrc = aClient->read (errMsg);
                        if (RT_FAILURE (vrc)) break;

                        rc = E_FAIL; // TODO: setError (E_FAIL, errMsg);
                        endLoop = true;
                        break;
                    }
                    default:
                    {
                        endLoop = true;
                        rc = E_FAIL; // TODO: ComAssertMsgFailedBreak ((
                            //"Invalid message code %d (%08lX)\n",
                            //reply, reply),
                            //rc = E_FAIL);
                    }
                }
            }

            break;
        }
        default:
            rc = E_FAIL; // TODO: ComAssertMsgFailedBreak ((
//                "Invalid message code %d (%08lX)\n",
//                d->msgCode, d->msgCode),
//                rc = E_FAIL);
    }

    if (aVrc)
        *aVrc = vrc;

    LogFlowFunc (("rc=0x%08X, vrc=%Rrc\n", rc, vrc));
    LogFlowFuncLeave();
    return rc;
}


/* The original source of the VBoxTAP adapter creation/destruction code has the following copyright */
/*
   Copyright 2004 by the Massachusetts Institute of Technology

   All rights reserved.

   Permission to use, copy, modify, and distribute this software and its
   documentation for any purpose and without fee is hereby granted,
   provided that the above copyright notice appear in all copies and that
   both that copyright notice and this permission notice appear in
   supporting documentation, and that the name of the Massachusetts
   Institute of Technology (M.I.T.) not be used in advertising or publicity
   pertaining to distribution of the software without specific, written
   prior permission.

   M.I.T. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
   ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
   M.I.T. BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
   ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
   ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
   SOFTWARE.
*/


#define NETSHELL_LIBRARY _T("netshell.dll")

/**
 *  Use the IShellFolder API to rename the connection.
 */
static HRESULT rename_shellfolder (PCWSTR wGuid, PCWSTR wNewName)
{
    /* This is the GUID for the network connections folder. It is constant.
     * {7007ACC7-3202-11D1-AAD2-00805FC1270E} */
    const GUID CLSID_NetworkConnections = {
        0x7007ACC7, 0x3202, 0x11D1, {
            0xAA, 0xD2, 0x00, 0x80, 0x5F, 0xC1, 0x27, 0x0E
        }
    };

    LPITEMIDLIST pidl = NULL;
    IShellFolder *pShellFolder = NULL;
    HRESULT hr;

    /* Build the display name in the form "::{GUID}". */
    if (wcslen (wGuid) >= MAX_PATH)
        return E_INVALIDARG;
    WCHAR szAdapterGuid[MAX_PATH + 2] = {0};
    swprintf (szAdapterGuid, L"::%ls", wGuid);

    /* Create an instance of the network connections folder. */
    hr = CoCreateInstance (CLSID_NetworkConnections, NULL,
                           CLSCTX_INPROC_SERVER, IID_IShellFolder,
                           reinterpret_cast <LPVOID *> (&pShellFolder));
    /* Parse the display name. */
    if (SUCCEEDED (hr))
    {
        hr = pShellFolder->ParseDisplayName (NULL, NULL, szAdapterGuid, NULL,
                                             &pidl, NULL);
    }
    if (SUCCEEDED (hr))
    {
        hr = pShellFolder->SetNameOf (NULL, pidl, wNewName, SHGDN_NORMAL,
                                      &pidl);
    }

    CoTaskMemFree (pidl);

    if (pShellFolder)
        pShellFolder->Release();

    return hr;
}

extern "C" HRESULT RenameConnection (PCWSTR GuidString, PCWSTR NewName)
{
    typedef HRESULT (WINAPI *lpHrRenameConnection) (const GUID *, PCWSTR);
    lpHrRenameConnection RenameConnectionFunc = NULL;
    HRESULT status;

    /* First try the IShellFolder interface, which was unimplemented
     * for the network connections folder before XP. */
    status = rename_shellfolder (GuidString, NewName);
    if (status == E_NOTIMPL)
    {
/** @todo that code doesn't seem to work! */
        /* The IShellFolder interface is not implemented on this platform.
         * Try the (undocumented) HrRenameConnection API in the netshell
         * library. */
        CLSID clsid;
        HINSTANCE hNetShell;
        status = CLSIDFromString ((LPOLESTR) GuidString, &clsid);
        if (FAILED(status))
            return E_FAIL;
        hNetShell = LoadLibrary (NETSHELL_LIBRARY);
        if (hNetShell == NULL)
            return E_FAIL;
        RenameConnectionFunc =
          (lpHrRenameConnection) GetProcAddress (hNetShell,
                                                 "HrRenameConnection");
        if (RenameConnectionFunc == NULL)
        {
            FreeLibrary (hNetShell);
            return E_FAIL;
        }
        status = RenameConnectionFunc (&clsid, NewName);
        FreeLibrary (hNetShell);
    }
    if (FAILED (status))
        return status;

    return S_OK;
}

#define DRIVERHWID _T("sun_VBoxNetAdp")

#define SetErrBreak(strAndArgs) \
    if (1) { \
        aErrMsg = Utf8StrFmt strAndArgs; vrc = VERR_GENERAL_FAILURE; break; \
    } else do {} while (0)

/* static */
static int createNetworkInterface (SVCHlpClient *aClient,
                                  BSTR * pName,
                                  Guid &aGUID, Utf8Str &aErrMsg)
{
    LogFlowFuncEnter();
//    LogFlowFunc (("Network connection name = '%s'\n", aName.raw()));

    AssertReturn (aClient, VERR_INVALID_POINTER);
//    AssertReturn (!aName.isNull(), VERR_INVALID_PARAMETER);

    int vrc = VINF_SUCCESS;

    HDEVINFO hDeviceInfo = INVALID_HANDLE_VALUE;
    SP_DEVINFO_DATA DeviceInfoData;
    DWORD ret = 0;
    BOOL found = FALSE;
    BOOL registered = FALSE;
    BOOL destroyList = FALSE;
    TCHAR pCfgGuidString [50];

    do
    {
        BOOL ok;
        GUID netGuid;
        SP_DRVINFO_DATA DriverInfoData;
        SP_DEVINSTALL_PARAMS  DeviceInstallParams;
        TCHAR className [MAX_PATH];
        DWORD index = 0;
        PSP_DRVINFO_DETAIL_DATA pDriverInfoDetail;
        /* for our purposes, 2k buffer is more
         * than enough to obtain the hardware ID
         * of the VBoxTAP driver. */
        DWORD detailBuf [2048];

        HKEY hkey = NULL;
        DWORD cbSize;
        DWORD dwValueType;

        /* initialize the structure size */
        DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        DriverInfoData.cbSize = sizeof(SP_DRVINFO_DATA);

        /* copy the net class GUID */
        memcpy(&netGuid, &GUID_DEVCLASS_NET, sizeof(GUID_DEVCLASS_NET));

        /* create an empty device info set associated with the net class GUID */
        hDeviceInfo = SetupDiCreateDeviceInfoList (&netGuid, NULL);
        if (hDeviceInfo == INVALID_HANDLE_VALUE)
            SetErrBreak (("SetupDiCreateDeviceInfoList failed (0x%08X)",
                          GetLastError()));

        /* get the class name from GUID */
        ok = SetupDiClassNameFromGuid (&netGuid, className, MAX_PATH, NULL);
        if (!ok)
            SetErrBreak (("SetupDiClassNameFromGuid failed (0x%08X)",
                          GetLastError()));

        /* create a device info element and add the new device instance
         * key to registry */
        ok = SetupDiCreateDeviceInfo (hDeviceInfo, className, &netGuid, NULL, NULL,
                                     DICD_GENERATE_ID, &DeviceInfoData);
        if (!ok)
            SetErrBreak (("SetupDiCreateDeviceInfo failed (0x%08X)",
                          GetLastError()));

        /* select the newly created device info to be the currently
           selected member */
        ok = SetupDiSetSelectedDevice (hDeviceInfo, &DeviceInfoData);
        if (!ok)
            SetErrBreak (("SetupDiSetSelectedDevice failed (0x%08X)",
                          GetLastError()));

        /* build a list of class drivers */
        ok = SetupDiBuildDriverInfoList (hDeviceInfo, &DeviceInfoData,
                                        SPDIT_CLASSDRIVER);
        if (!ok)
            SetErrBreak (("SetupDiBuildDriverInfoList failed (0x%08X)",
                          GetLastError()));

        destroyList = TRUE;

        /* enumerate the driver info list */
        while (TRUE)
        {
            BOOL ret;

            ret = SetupDiEnumDriverInfo (hDeviceInfo, &DeviceInfoData,
                                         SPDIT_CLASSDRIVER, index, &DriverInfoData);

            /* if the function failed and GetLastError() returned
             * ERROR_NO_MORE_ITEMS, then we have reached the end of the
             * list.  Othewise there was something wrong with this
             * particular driver. */
            if (!ret)
            {
                if(GetLastError() == ERROR_NO_MORE_ITEMS)
                    break;
                else
                {
                    index++;
                    continue;
                }
            }

            pDriverInfoDetail = (PSP_DRVINFO_DETAIL_DATA) detailBuf;
            pDriverInfoDetail->cbSize = sizeof(SP_DRVINFO_DETAIL_DATA);

            /* if we successfully find the hardware ID and it turns out to
             * be the one for the loopback driver, then we are done. */
            if (SetupDiGetDriverInfoDetail (hDeviceInfo,
                                            &DeviceInfoData,
                                            &DriverInfoData,
                                            pDriverInfoDetail,
                                            sizeof (detailBuf),
                                            NULL))
            {
                TCHAR * t;

                /* pDriverInfoDetail->HardwareID is a MULTISZ string.  Go through the
                 * whole list and see if there is a match somewhere. */
                t = pDriverInfoDetail->HardwareID;
                while (t && *t && t < (TCHAR *) &detailBuf [sizeof(detailBuf) / sizeof (detailBuf[0])])
                {
                    if (!_tcsicmp(t, DRIVERHWID))
                        break;

                    t += _tcslen(t) + 1;
                }

                if (t && *t && t < (TCHAR *) &detailBuf [sizeof(detailBuf) / sizeof (detailBuf[0])])
                {
                    found = TRUE;
                    break;
                }
            }

            index ++;
        }

        if (!found)
            SetErrBreak (("Could not find Host Interface Networking driver! "
                              "Please reinstall"));

        /* set the loopback driver to be the currently selected */
        ok = SetupDiSetSelectedDriver (hDeviceInfo, &DeviceInfoData,
                                       &DriverInfoData);
        if (!ok)
            SetErrBreak (("SetupDiSetSelectedDriver failed (0x%08X)",
                          GetLastError()));

        /* register the phantom device to prepare for install */
        ok = SetupDiCallClassInstaller (DIF_REGISTERDEVICE, hDeviceInfo,
                                        &DeviceInfoData);
        if (!ok)
            SetErrBreak (("SetupDiCallClassInstaller failed (0x%08X)",
                          GetLastError()));

        /* registered, but remove if errors occur in the following code */
        registered = TRUE;

        /* ask the installer if we can install the device */
        ok = SetupDiCallClassInstaller (DIF_ALLOW_INSTALL, hDeviceInfo,
                                        &DeviceInfoData);
        if (!ok)
        {
            if (GetLastError() != ERROR_DI_DO_DEFAULT)
                SetErrBreak (("SetupDiCallClassInstaller (DIF_ALLOW_INSTALL) failed (0x%08X)",
                              GetLastError()));
            /* that's fine */
        }

        /* install the files first */
        ok = SetupDiCallClassInstaller (DIF_INSTALLDEVICEFILES, hDeviceInfo,
                                        &DeviceInfoData);
        if (!ok)
            SetErrBreak (("SetupDiCallClassInstaller (DIF_INSTALLDEVICEFILES) failed (0x%08X)",
                          GetLastError()));

        /* get the device install parameters and disable filecopy */
        DeviceInstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS);
        ok = SetupDiGetDeviceInstallParams (hDeviceInfo, &DeviceInfoData,
                                            &DeviceInstallParams);
        if (ok)
        {
            DeviceInstallParams.Flags |= DI_NOFILECOPY;
            ok = SetupDiSetDeviceInstallParams (hDeviceInfo, &DeviceInfoData,
                                                &DeviceInstallParams);
            if (!ok)
                SetErrBreak (("SetupDiSetDeviceInstallParams failed (0x%08X)",
                              GetLastError()));
        }

        /*
         * Register any device-specific co-installers for this device,
         */

        ok = SetupDiCallClassInstaller (DIF_REGISTER_COINSTALLERS,
                                        hDeviceInfo,
                                        &DeviceInfoData);
        if (!ok)
            SetErrBreak (("SetupDiCallClassInstaller (DIF_REGISTER_COINSTALLERS) failed (0x%08X)",
                          GetLastError()));

        /*
         * install any  installer-specified interfaces.
         * and then do the real install
         */
        ok = SetupDiCallClassInstaller (DIF_INSTALLINTERFACES,
                                        hDeviceInfo,
                                        &DeviceInfoData);
        if (!ok)
            SetErrBreak (("SetupDiCallClassInstaller (DIF_INSTALLINTERFACES) failed (0x%08X)",
                          GetLastError()));

        ok = SetupDiCallClassInstaller (DIF_INSTALLDEVICE,
                                        hDeviceInfo,
                                        &DeviceInfoData);
        if (!ok)
            SetErrBreak (("SetupDiCallClassInstaller (DIF_INSTALLDEVICE) failed (0x%08X)",
                          GetLastError()));

        /* Figure out NetCfgInstanceId */
        hkey = SetupDiOpenDevRegKey (hDeviceInfo,
                                     &DeviceInfoData,
                                     DICS_FLAG_GLOBAL,
                                     0,
                                     DIREG_DRV,
                                     KEY_READ);
        if (hkey == INVALID_HANDLE_VALUE)
            SetErrBreak (("SetupDiOpenDevRegKey failed (0x%08X)",
                          GetLastError()));

        cbSize = sizeof (pCfgGuidString);
        DWORD ret;
        ret = RegQueryValueEx (hkey, _T("NetCfgInstanceId"), NULL,
                               &dwValueType, (LPBYTE) pCfgGuidString, &cbSize);
        RegCloseKey (hkey);
    }
    while (0);

    /*
     * cleanup
     */

    if (hDeviceInfo != INVALID_HANDLE_VALUE)
    {
        /* an error has occured, but the device is registered, we must remove it */
        if (ret != 0 && registered)
            SetupDiCallClassInstaller (DIF_REMOVE, hDeviceInfo, &DeviceInfoData);

        found = SetupDiDeleteDeviceInfo (hDeviceInfo, &DeviceInfoData);

        /* destroy the driver info list */
        if (destroyList)
            SetupDiDestroyDriverInfoList (hDeviceInfo, &DeviceInfoData,
                                          SPDIT_CLASSDRIVER);
        /* clean up the device info set */
        SetupDiDestroyDeviceInfoList (hDeviceInfo);
    }

    /* return the network connection GUID on success */
    if (RT_SUCCESS (vrc))
    {
        /* remove the curly bracket at the end */
        pCfgGuidString [_tcslen (pCfgGuidString) - 1] = '\0';
        LogFlowFunc (("Network connection GUID string = {%ls}\n", pCfgGuidString + 1));

        aGUID = Guid (Utf8Str (pCfgGuidString + 1));
        LogFlowFunc (("Network connection GUID = {%RTuuid}\n", aGUID.raw()));
        Assert (!aGUID.isEmpty());

        INetCfg              *pNc;
        INetCfgComponent     *pMpNcc;
        LPWSTR               lpszApp;

        HRESULT hr = VBoxNetCfgWinQueryINetCfg( FALSE,
                           VBOX_APP_NAME,
                           &pNc,
                           &lpszApp );
        if(hr == S_OK)
        {
            hr = VBoxNetCfgWinGetComponentByGuid(pNc,
                                            &GUID_DEVCLASS_NET,
                                            aGUID.asOutParam(),
                                            &pMpNcc);
            if(hr == S_OK)
            {
                LPWSTR name;
                hr = pMpNcc->GetDisplayName(&name);
                if(hr == S_OK)
                {
                    Bstr str(name);
                    str.detachTo(pName);

                    CoTaskMemFree (name);
                }

                VBoxNetCfgWinReleaseRef(pMpNcc);
            }
            VBoxNetCfgWinReleaseINetCfg(pNc, FALSE);
        }

        if(hr != S_OK)
        {
            vrc = VERR_GENERAL_FAILURE;
        }

    }

    LogFlowFunc (("vrc=%Rrc\n", vrc));
    LogFlowFuncLeave();
    return vrc;
}

/* static */
static int removeNetworkInterface (SVCHlpClient *aClient,
                                  const Guid &aGUID,
                                  Utf8Str &aErrMsg)
{
    LogFlowFuncEnter();
    LogFlowFunc (("Network connection GUID = {%RTuuid}\n", aGUID.raw()));

    AssertReturn (aClient, VERR_INVALID_POINTER);
    AssertReturn (!aGUID.isEmpty(), VERR_INVALID_PARAMETER);

    int vrc = VINF_SUCCESS;

    do
    {
        TCHAR lszPnPInstanceId [512] = {0};

        /* We have to find the device instance ID through a registry search */

        HKEY hkeyNetwork = 0;
        HKEY hkeyConnection = 0;

        do
        {
            char strRegLocation [256];
            sprintf (strRegLocation,
                     "SYSTEM\\CurrentControlSet\\Control\\Network\\"
                     "{4D36E972-E325-11CE-BFC1-08002BE10318}\\{%s}",
                     aGUID.toString().raw());
            LONG status;
            status = RegOpenKeyExA (HKEY_LOCAL_MACHINE, strRegLocation, 0,
                                    KEY_READ, &hkeyNetwork);
            if ((status != ERROR_SUCCESS) || !hkeyNetwork)
                SetErrBreak ((
                        "Host interface network is not found in registry (%s) [1]",
                    strRegLocation));

            status = RegOpenKeyExA (hkeyNetwork, "Connection", 0,
                                    KEY_READ, &hkeyConnection);
            if ((status != ERROR_SUCCESS) || !hkeyConnection)
                SetErrBreak ((
                        "Host interface network is not found in registry (%s) [2]",
                    strRegLocation));

            DWORD len = sizeof (lszPnPInstanceId);
            DWORD dwKeyType;
            status = RegQueryValueExW (hkeyConnection, L"PnPInstanceID", NULL,
                                       &dwKeyType, (LPBYTE) lszPnPInstanceId, &len);
            if ((status != ERROR_SUCCESS) || (dwKeyType != REG_SZ))
                SetErrBreak ((
                        "Host interface network is not found in registry (%s) [3]",
                    strRegLocation));
        }
        while (0);

        if (hkeyConnection)
            RegCloseKey (hkeyConnection);
        if (hkeyNetwork)
            RegCloseKey (hkeyNetwork);

        if (RT_FAILURE (vrc))
            break;

        /*
         * Now we are going to enumerate all network devices and
         * wait until we encounter the right device instance ID
         */

        HDEVINFO hDeviceInfo = INVALID_HANDLE_VALUE;

        do
        {
            BOOL ok;
            DWORD ret = 0;
            GUID netGuid;
            SP_DEVINFO_DATA DeviceInfoData;
            DWORD index = 0;
            BOOL found = FALSE;
            DWORD size = 0;

            /* initialize the structure size */
            DeviceInfoData.cbSize = sizeof (SP_DEVINFO_DATA);

            /* copy the net class GUID */
            memcpy (&netGuid, &GUID_DEVCLASS_NET, sizeof (GUID_DEVCLASS_NET));

            /* return a device info set contains all installed devices of the Net class */
            hDeviceInfo = SetupDiGetClassDevs (&netGuid, NULL, NULL, DIGCF_PRESENT);

            if (hDeviceInfo == INVALID_HANDLE_VALUE)
                SetErrBreak (("SetupDiGetClassDevs failed (0x%08X)", GetLastError()));

            /* enumerate the driver info list */
            while (TRUE)
            {
                TCHAR *deviceHwid;

                ok = SetupDiEnumDeviceInfo (hDeviceInfo, index, &DeviceInfoData);

                if (!ok)
                {
                    if (GetLastError() == ERROR_NO_MORE_ITEMS)
                        break;
                    else
                    {
                        index++;
                        continue;
                    }
                }

                /* try to get the hardware ID registry property */
                ok = SetupDiGetDeviceRegistryProperty (hDeviceInfo,
                                                       &DeviceInfoData,
                                                       SPDRP_HARDWAREID,
                                                       NULL,
                                                       NULL,
                                                       0,
                                                       &size);
                if (!ok)
                {
                    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
                    {
                        index++;
                        continue;
                    }

                    deviceHwid = (TCHAR *) malloc (size);
                    ok = SetupDiGetDeviceRegistryProperty (hDeviceInfo,
                                                           &DeviceInfoData,
                                                           SPDRP_HARDWAREID,
                                                           NULL,
                                                           (PBYTE)deviceHwid,
                                                           size,
                                                           NULL);
                    if (!ok)
                    {
                        free (deviceHwid);
                        deviceHwid = NULL;
                        index++;
                        continue;
                    }
                }
                else
                {
                    /* something is wrong.  This shouldn't have worked with a NULL buffer */
                    index++;
                    continue;
                }

                for (TCHAR *t = deviceHwid;
                     t && *t && t < &deviceHwid[size / sizeof(TCHAR)];
                     t += _tcslen (t) + 1)
                {
                    if (!_tcsicmp (DRIVERHWID, t))
                    {
                          /* get the device instance ID */
                          TCHAR devID [MAX_DEVICE_ID_LEN];
                          if (CM_Get_Device_ID(DeviceInfoData.DevInst,
                                               devID, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS)
                          {
                              /* compare to what we determined before */
                              if (wcscmp(devID, lszPnPInstanceId) == 0)
                              {
                                  found = TRUE;
                                  break;
                              }
                          }
                    }
                }

                if (deviceHwid)
                {
                    free (deviceHwid);
                    deviceHwid = NULL;
                }

                if (found)
                    break;

                index++;
            }

            if (found == FALSE)
                SetErrBreak (("Host Interface Network driver not found (0x%08X)",
                              GetLastError()));

            ok = SetupDiSetSelectedDevice (hDeviceInfo, &DeviceInfoData);
            if (!ok)
                SetErrBreak (("SetupDiSetSelectedDevice failed (0x%08X)",
                              GetLastError()));

            ok = SetupDiCallClassInstaller (DIF_REMOVE, hDeviceInfo, &DeviceInfoData);
            if (!ok)
                SetErrBreak (("SetupDiCallClassInstaller (DIF_REMOVE) failed (0x%08X)",
                              GetLastError()));
        }
        while (0);

        /* clean up the device info set */
        if (hDeviceInfo != INVALID_HANDLE_VALUE)
            SetupDiDestroyDeviceInfoList (hDeviceInfo);

        if (RT_FAILURE (vrc))
            break;
    }
    while (0);

    LogFlowFunc (("vrc=%Rrc\n", vrc));
    LogFlowFuncLeave();
    return vrc;
}

#undef SetErrBreak

int netIfNetworkInterfaceHelperServer (SVCHlpClient *aClient,
                                        SVCHlpMsg::Code aMsgCode)
{
    LogFlowFuncEnter();
    LogFlowFunc (("aClient={%p}, aMsgCode=%d\n", aClient, aMsgCode));

    AssertReturn (aClient, VERR_INVALID_POINTER);

    int vrc = VINF_SUCCESS;

    switch (aMsgCode)
    {
        case SVCHlpMsg::CreateHostOnlyNetworkInterface:
        {
            LogFlowFunc (("CreateHostOnlyNetworkInterface:\n"));

//            Utf8Str name;
//            vrc = aClient->read (name);
//            if (RT_FAILURE (vrc)) break;

            Guid guid;
            Utf8Str errMsg;
            Bstr name;
            vrc = createNetworkInterface (aClient, name.asOutParam(), guid, errMsg);

            if (RT_SUCCESS (vrc))
            {
                /* write success followed by GUID */
                vrc = aClient->write (SVCHlpMsg::CreateHostOnlyNetworkInterface_OK);
                if (RT_FAILURE (vrc)) break;
                vrc = aClient->write (Utf8Str (name));
                if (RT_FAILURE (vrc)) break;
                vrc = aClient->write (guid);
                if (RT_FAILURE (vrc)) break;
            }
            else
            {
                /* write failure followed by error message */
                if (errMsg.isEmpty())
                    errMsg = Utf8StrFmt ("Unspecified error (%Rrc)", vrc);
                vrc = aClient->write (SVCHlpMsg::Error);
                if (RT_FAILURE (vrc)) break;
                vrc = aClient->write (errMsg);
                if (RT_FAILURE (vrc)) break;
            }

            break;
        }
        case SVCHlpMsg::RemoveHostOnlyNetworkInterface:
        {
            LogFlowFunc (("RemoveHostOnlyNetworkInterface:\n"));

            Guid guid;
            vrc = aClient->read (guid);
            if (RT_FAILURE (vrc)) break;

            Utf8Str errMsg;
            vrc = removeNetworkInterface (aClient, guid, errMsg);

            if (RT_SUCCESS (vrc))
            {
                /* write parameter-less success */
                vrc = aClient->write (SVCHlpMsg::OK);
                if (RT_FAILURE (vrc)) break;
            }
            else
            {
                /* write failure followed by error message */
                if (errMsg.isEmpty())
                    errMsg = Utf8StrFmt ("Unspecified error (%Rrc)", vrc);
                vrc = aClient->write (SVCHlpMsg::Error);
                if (RT_FAILURE (vrc)) break;
                vrc = aClient->write (errMsg);
                if (RT_FAILURE (vrc)) break;
            }

            break;
        }
        case SVCHlpMsg::EnableStaticIpConfigV6:
        {
            LogFlowFunc (("EnableStaticIpConfigV6:\n"));

            Guid guid;
            Utf8Str ipV6;
            ULONG maskLengthV6;
            vrc = aClient->read (guid);
            if (RT_FAILURE (vrc)) break;
            vrc = aClient->read (ipV6);
            if (RT_FAILURE (vrc)) break;
            vrc = aClient->read (maskLengthV6);
            if (RT_FAILURE (vrc)) break;

            Utf8Str errMsg;
            vrc = netIfEnableStaticIpConfigV6 (guid, Bstr(ipV6), maskLengthV6);

            if (RT_SUCCESS (vrc))
            {
                /* write success followed by GUID */
                vrc = aClient->write (SVCHlpMsg::OK);
                if (RT_FAILURE (vrc)) break;
            }
            else
            {
                /* write failure followed by error message */
                if (errMsg.isEmpty())
                    errMsg = Utf8StrFmt ("Unspecified error (%Rrc)", vrc);
                vrc = aClient->write (SVCHlpMsg::Error);
                if (RT_FAILURE (vrc)) break;
                vrc = aClient->write (errMsg);
                if (RT_FAILURE (vrc)) break;
            }

            break;
        }
        case SVCHlpMsg::EnableStaticIpConfig:
        {
            LogFlowFunc (("EnableStaticIpConfig:\n"));

            Guid guid;
            ULONG ip, mask;
            vrc = aClient->read (guid);
            if (RT_FAILURE (vrc)) break;
            vrc = aClient->read (ip);
            if (RT_FAILURE (vrc)) break;
            vrc = aClient->read (mask);
            if (RT_FAILURE (vrc)) break;

            Utf8Str errMsg;
            vrc = netIfEnableStaticIpConfig (guid, ip, mask);

            if (RT_SUCCESS (vrc))
            {
                /* write success followed by GUID */
                vrc = aClient->write (SVCHlpMsg::OK);
                if (RT_FAILURE (vrc)) break;
            }
            else
            {
                /* write failure followed by error message */
                if (errMsg.isEmpty())
                    errMsg = Utf8StrFmt ("Unspecified error (%Rrc)", vrc);
                vrc = aClient->write (SVCHlpMsg::Error);
                if (RT_FAILURE (vrc)) break;
                vrc = aClient->write (errMsg);
                if (RT_FAILURE (vrc)) break;
            }

            break;
        }
        case SVCHlpMsg::EnableDynamicIpConfig:
        {
            LogFlowFunc (("EnableDynamicIpConfig:\n"));

            Guid guid;
            vrc = aClient->read (guid);
            if (RT_FAILURE (vrc)) break;

            Utf8Str errMsg;
            vrc = netIfEnableDynamicIpConfig (guid);

            if (RT_SUCCESS (vrc))
            {
                /* write success followed by GUID */
                vrc = aClient->write (SVCHlpMsg::OK);
                if (RT_FAILURE (vrc)) break;
            }
            else
            {
                /* write failure followed by error message */
                if (errMsg.isEmpty())
                    errMsg = Utf8StrFmt ("Unspecified error (%Rrc)", vrc);
                vrc = aClient->write (SVCHlpMsg::Error);
                if (RT_FAILURE (vrc)) break;
                vrc = aClient->write (errMsg);
                if (RT_FAILURE (vrc)) break;
            }

            break;
        }
        default:
            AssertMsgFailedBreakStmt (
                ("Invalid message code %d (%08lX)\n", aMsgCode, aMsgCode),
                VERR_GENERAL_FAILURE);
    }

    LogFlowFunc (("vrc=%Rrc\n", vrc));
    LogFlowFuncLeave();
    return vrc;
}

/** @todo REMOVE. OBSOLETE NOW. */
/**
 * Returns TRUE if the Windows version is 6.0 or greater (i.e. it's Vista and
 * later OSes) and it has the UAC (User Account Control) feature enabled.
 */
static BOOL IsUACEnabled()
{
    LONG rc = 0;

    OSVERSIONINFOEX info;
    ZeroMemory (&info, sizeof (OSVERSIONINFOEX));
    info.dwOSVersionInfoSize = sizeof (OSVERSIONINFOEX);
    rc = GetVersionEx ((OSVERSIONINFO *) &info);
    AssertReturn (rc != 0, FALSE);

    LogFlowFunc (("dwMajorVersion=%d, dwMinorVersion=%d\n",
                  info.dwMajorVersion, info.dwMinorVersion));

    /* we are interested only in Vista (and newer versions...). In all
     * earlier versions UAC is not present. */
    if (info.dwMajorVersion < 6)
        return FALSE;

    /* the default EnableLUA value is 1 (Enabled) */
    DWORD dwEnableLUA = 1;

    HKEY hKey;
    rc = RegOpenKeyExA (HKEY_LOCAL_MACHINE,
                        "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                        0, KEY_QUERY_VALUE, &hKey);

    Assert (rc == ERROR_SUCCESS || rc == ERROR_PATH_NOT_FOUND);
    if (rc == ERROR_SUCCESS)
    {

        DWORD cbEnableLUA = sizeof (dwEnableLUA);
        rc = RegQueryValueExA (hKey, "EnableLUA", NULL, NULL,
                               (LPBYTE) &dwEnableLUA, &cbEnableLUA);

        RegCloseKey (hKey);

        Assert (rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND);
    }

    LogFlowFunc (("rc=%d, dwEnableLUA=%d\n", rc, dwEnableLUA));

    return dwEnableLUA == 1;
}

/* end */

static int vboxNetWinAddComponent(std::list <ComObjPtr <HostNetworkInterface> > * pPist, INetCfgComponent * pncc, HostNetworkInterfaceType enmType)
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
            if (SUCCEEDED (iface->init (name, enmType, &Info)))
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

#endif /* #ifndef VBOX_WITH_NETFLT */


static int netIfListHostAdapters(std::list <ComObjPtr <HostNetworkInterface> > &list)
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
//#ifndef DEBUG_bird
//                Assert(hr == S_OK);
//#endif
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
                                vboxNetWinAddComponent(&list, pMpNcc, HostNetworkInterfaceType_HostOnly);
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

int NetIfGetConfig(HostNetworkInterface * pIf, NETIFINFO *pInfo)
{
#ifndef VBOX_WITH_NETFLT
    return VERR_NOT_IMPLEMENTED;
#else
    Bstr name;
    HRESULT hr = pIf->COMGETTER(Name)(name.asOutParam());
    if(SUCCEEDED(hr))
    {
        return collectNetIfInfo(name, pInfo);
    }
    return VERR_GENERAL_FAILURE;
#endif
}

int NetIfCreateHostOnlyNetworkInterface (VirtualBox *pVBox,
                                  IHostNetworkInterface **aHostNetworkInterface,
                                  IProgress **aProgress)
{
    /* create a progress object */
    ComObjPtr <Progress> progress;
    progress.createObject();

    ComPtr<IHost> host;
    HRESULT rc = pVBox->COMGETTER(Host)(host.asOutParam());
    if(SUCCEEDED(rc))
    {
        rc = progress->init (pVBox, host,
                             Bstr (_T ("Creating host only network interface")),
                             FALSE /* aCancelable */);
        if(SUCCEEDED(rc))
        {
            CheckComRCReturnRC (rc);
            progress.queryInterfaceTo (aProgress);

            /* create a new uninitialized host interface object */
            ComObjPtr <HostNetworkInterface> iface;
            iface.createObject();
            iface.queryInterfaceTo (aHostNetworkInterface);

            /* create the networkInterfaceHelperClient() argument */
            std::auto_ptr <NetworkInterfaceHelperClientData>
                d (new NetworkInterfaceHelperClientData());
            AssertReturn (d.get(), E_OUTOFMEMORY);

            d->msgCode = SVCHlpMsg::CreateHostOnlyNetworkInterface;
//            d->name = aName;
            d->iface = iface;

            rc = pVBox->startSVCHelperClient (
                IsUACEnabled() == TRUE /* aPrivileged */,
                netIfNetworkInterfaceHelperClient,
                static_cast <void *> (d.get()),
                progress);

            if (SUCCEEDED (rc))
            {
                /* d is now owned by netIfNetworkInterfaceHelperClient(), so release it */
                d.release();
            }
        }
    }

    return SUCCEEDED(rc) ? VINF_SUCCESS : VERR_GENERAL_FAILURE;
}

int NetIfRemoveHostOnlyNetworkInterface (VirtualBox *pVBox, IN_GUID aId,
                                  IHostNetworkInterface **aHostNetworkInterface,
                                  IProgress **aProgress)
{
    /* create a progress object */
    ComObjPtr <Progress> progress;
    progress.createObject();
    ComPtr<IHost> host;
    HRESULT rc = pVBox->COMGETTER(Host)(host.asOutParam());
    if(SUCCEEDED(rc))
    {
        rc = progress->init (pVBox, host,
                            Bstr (_T ("Removing host network interface")),
                            FALSE /* aCancelable */);
        if(SUCCEEDED(rc))
        {
            CheckComRCReturnRC (rc);
            progress.queryInterfaceTo (aProgress);

            /* create the networkInterfaceHelperClient() argument */
            std::auto_ptr <NetworkInterfaceHelperClientData>
                d (new NetworkInterfaceHelperClientData());
            AssertReturn (d.get(), E_OUTOFMEMORY);

            d->msgCode = SVCHlpMsg::RemoveHostOnlyNetworkInterface;
            d->guid = aId;

            rc = pVBox->startSVCHelperClient (
                IsUACEnabled() == TRUE /* aPrivileged */,
                netIfNetworkInterfaceHelperClient,
                static_cast <void *> (d.get()),
                progress);

            if (SUCCEEDED (rc))
            {
                /* d is now owned by netIfNetworkInterfaceHelperClient(), so release it */
                d.release();
            }
        }
    }

    return SUCCEEDED(rc) ? VINF_SUCCESS : VERR_GENERAL_FAILURE;
}

int NetIfEnableStaticIpConfig(VirtualBox *vBox, HostNetworkInterface * pIf, ULONG ip, ULONG mask)
{
    HRESULT rc;
    GUID guid;
    rc = pIf->COMGETTER(Id) (&guid);
    if(SUCCEEDED(rc))
    {
//        ComPtr<VirtualBox> vBox;
//        rc = pIf->getVirtualBox (vBox.asOutParam());
//        if(SUCCEEDED(rc))
        {
            /* create a progress object */
            ComObjPtr <Progress> progress;
            progress.createObject();
//            ComPtr<IHost> host;
//            HRESULT rc = vBox->COMGETTER(Host)(host.asOutParam());
//            if(SUCCEEDED(rc))
            {
                rc = progress->init (vBox, (IHostNetworkInterface*)pIf,
                                    Bstr ("Enabling Dynamic Ip Configuration"),
                                    FALSE /* aCancelable */);
                if(SUCCEEDED(rc))
                {
                    CheckComRCReturnRC (rc);
//                    progress.queryInterfaceTo (aProgress);

                    /* create the networkInterfaceHelperClient() argument */
                    std::auto_ptr <NetworkInterfaceHelperClientData>
                        d (new NetworkInterfaceHelperClientData());
                    AssertReturn (d.get(), E_OUTOFMEMORY);

                    d->msgCode = SVCHlpMsg::EnableStaticIpConfig;
                    d->guid = guid;
                    d->iface = pIf;
                    d->u.StaticIP.IPAddress = ip;
                    d->u.StaticIP.IPNetMask = mask;

                    rc = vBox->startSVCHelperClient (
                        IsUACEnabled() == TRUE /* aPrivileged */,
                        netIfNetworkInterfaceHelperClient,
                        static_cast <void *> (d.get()),
                        progress);

                    if (SUCCEEDED (rc))
                    {
                        /* d is now owned by netIfNetworkInterfaceHelperClient(), so release it */
                        d.release();

                        progress->WaitForCompletion(-1);
                    }
                }
            }
        }
    }

    return SUCCEEDED(rc) ? VINF_SUCCESS : VERR_GENERAL_FAILURE;
}

int NetIfEnableStaticIpConfigV6(VirtualBox *vBox, HostNetworkInterface * pIf, IN_BSTR aIPV6Address, ULONG aIPV6MaskPrefixLength)
{
    HRESULT rc;
    GUID guid;
    rc = pIf->COMGETTER(Id) (&guid);
    if(SUCCEEDED(rc))
    {
//        ComPtr<VirtualBox> vBox;
//        rc = pIf->getVirtualBox (vBox.asOutParam());
//        if(SUCCEEDED(rc))
        {
            /* create a progress object */
            ComObjPtr <Progress> progress;
            progress.createObject();
//            ComPtr<IHost> host;
//            HRESULT rc = vBox->COMGETTER(Host)(host.asOutParam());
//            if(SUCCEEDED(rc))
            {
                rc = progress->init (vBox, (IHostNetworkInterface*)pIf,
                                    Bstr ("Enabling Dynamic Ip Configuration"),
                                    FALSE /* aCancelable */);
                if(SUCCEEDED(rc))
                {
                    CheckComRCReturnRC (rc);
//                    progress.queryInterfaceTo (aProgress);

                    /* create the networkInterfaceHelperClient() argument */
                    std::auto_ptr <NetworkInterfaceHelperClientData>
                        d (new NetworkInterfaceHelperClientData());
                    AssertReturn (d.get(), E_OUTOFMEMORY);

                    d->msgCode = SVCHlpMsg::EnableStaticIpConfigV6;
                    d->guid = guid;
                    d->iface = pIf;
                    d->u.StaticIPV6.IPV6Address = aIPV6Address;
                    d->u.StaticIPV6.IPV6NetMaskLength = aIPV6MaskPrefixLength;

                    rc = vBox->startSVCHelperClient (
                        IsUACEnabled() == TRUE /* aPrivileged */,
                        netIfNetworkInterfaceHelperClient,
                        static_cast <void *> (d.get()),
                        progress);

                    if (SUCCEEDED (rc))
                    {
                        /* d is now owned by netIfNetworkInterfaceHelperClient(), so release it */
                        d.release();

                        progress->WaitForCompletion(-1);
                    }
                }
            }
        }
    }

    return SUCCEEDED(rc) ? VINF_SUCCESS : VERR_GENERAL_FAILURE;
}

int NetIfEnableDynamicIpConfig(VirtualBox *vBox, HostNetworkInterface * pIf)
{
    HRESULT rc;
    GUID guid;
    rc = pIf->COMGETTER(Id) (&guid);
    if(SUCCEEDED(rc))
    {
//        ComPtr<VirtualBox> vBox;
//        rc = pIf->getVirtualBox (vBox.asOutParam());
//        if(SUCCEEDED(rc))
        {
            /* create a progress object */
            ComObjPtr <Progress> progress;
            progress.createObject();
//            ComPtr<IHost> host;
//            HRESULT rc = vBox->COMGETTER(Host)(host.asOutParam());
//            if(SUCCEEDED(rc))
            {
                rc = progress->init (vBox, (IHostNetworkInterface*)pIf,
                                    Bstr ("Enabling Dynamic Ip Configuration"),
                                    FALSE /* aCancelable */);
                if(SUCCEEDED(rc))
                {
                    CheckComRCReturnRC (rc);
//                    progress.queryInterfaceTo (aProgress);

                    /* create the networkInterfaceHelperClient() argument */
                    std::auto_ptr <NetworkInterfaceHelperClientData>
                        d (new NetworkInterfaceHelperClientData());
                    AssertReturn (d.get(), E_OUTOFMEMORY);

                    d->msgCode = SVCHlpMsg::EnableDynamicIpConfig;
                    d->guid = guid;
                    d->iface = pIf;

                    rc = vBox->startSVCHelperClient (
                        IsUACEnabled() == TRUE /* aPrivileged */,
                        netIfNetworkInterfaceHelperClient,
                        static_cast <void *> (d.get()),
                        progress);

                    if (SUCCEEDED (rc))
                    {
                        /* d is now owned by netIfNetworkInterfaceHelperClient(), so release it */
                        d.release();

                        progress->WaitForCompletion(-1);
                    }
                }
            }
        }
    }

    return SUCCEEDED(rc) ? VINF_SUCCESS : VERR_GENERAL_FAILURE;
}

int NetIfList(std::list <ComObjPtr <HostNetworkInterface> > &list)
{
#ifndef VBOX_WITH_NETFLT
    return VERR_NOT_IMPLEMENTED;
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
                                            vboxNetWinAddComponent(&list, pMpNcc, HostNetworkInterfaceType_Bridged);
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

    netIfListHostAdapters(list);

    return VINF_SUCCESS;
#endif /* #  if defined VBOX_WITH_NETFLT */
}
