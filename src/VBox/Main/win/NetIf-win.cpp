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

static int collectNetIfInfo(Bstr &strName, Guid &guid, PNETIFINFO pInfo)
{
    DWORD dwRc;
    int rc = VINF_SUCCESS;
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

        ADAPTER_SETTINGS Settings;
        HRESULT hr = VBoxNetCfgWinGetAdapterSettings((const GUID *)guid.raw(), &Settings);
        if(hr == S_OK)
        {
            if(Settings.ip)
            {
                pInfo->IPAddress.u = Settings.ip;
                pInfo->IPNetMask.u = Settings.mask;
            }
            pInfo->bDhcpEnabled = Settings.bDhcp;
        }
        else
        {
            pInfo->bDhcpEnabled = false;
        }
    }
    RTMemFree(pAddresses);

    return VINF_SUCCESS;
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
    ComObjPtr <VirtualBox> vBox;
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
                        if(SUCCEEDED(rc))
                        {
                            rc = d->iface->setVirtualBox(d->vBox);
                            if(SUCCEEDED(rc))
                            {
                                rc = d->iface->updateConfig();
                            }
                        }
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
        case SVCHlpMsg::DhcpRediscover: /* see usage in code */
        {
            LogFlowFunc (("DhcpRediscover:\n"));
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


int netIfNetworkInterfaceHelperServer (SVCHlpClient *aClient,
                                        SVCHlpMsg::Code aMsgCode)
{
    LogFlowFuncEnter();
    LogFlowFunc (("aClient={%p}, aMsgCode=%d\n", aClient, aMsgCode));

    AssertReturn (aClient, VERR_INVALID_POINTER);

    int vrc = VINF_SUCCESS;
    HRESULT hrc;

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
            Bstr bstrErr;

            hrc = VBoxNetCfgWinCreateHostOnlyNetworkInterface (guid.asOutParam(), name.asOutParam(), bstrErr.asOutParam());

            if (hrc == S_OK)
            {
                ULONG ip, mask;
                hrc = VBoxNetCfgWinGenHostOnlyNetworkNetworkIp(&ip, &mask);
                if(hrc == S_OK)
                {
                    /* ip returned by VBoxNetCfgWinGenHostOnlyNetworkNetworkIp is a network ip,
                     * i.e. 192.168.xxx.0, assign  192.168.xxx.1 for the hostonly adapter */
                    ip = ip | (1 << 24);
                    hrc = VBoxNetCfgWinEnableStaticIpConfig((const GUID*)guid.raw(), ip, mask);
                }

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
                vrc = VERR_GENERAL_FAILURE;
                errMsg = Utf8Str(bstrErr);
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
            Bstr bstrErr;

            vrc = aClient->read (guid);
            if (RT_FAILURE (vrc)) break;

            Utf8Str errMsg;
            hrc = VBoxNetCfgWinRemoveHostOnlyNetworkInterface ((const GUID*)guid.raw(), bstrErr.asOutParam());

            if (hrc == S_OK)
            {
                /* write parameter-less success */
                vrc = aClient->write (SVCHlpMsg::OK);
                if (RT_FAILURE (vrc)) break;
            }
            else
            {
                vrc = VERR_GENERAL_FAILURE;
                errMsg = Utf8Str(bstrErr);
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
            vrc = VERR_NOT_IMPLEMENTED;

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
            hrc = VBoxNetCfgWinEnableStaticIpConfig ((const GUID *)guid.raw(), ip, mask);

            if (hrc == S_OK)
            {
                /* write success followed by GUID */
                vrc = aClient->write (SVCHlpMsg::OK);
                if (RT_FAILURE (vrc)) break;
            }
            else
            {
                vrc = VERR_GENERAL_FAILURE;
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
            hrc = VBoxNetCfgWinEnableDynamicIpConfig ((const GUID *)guid.raw());

            if (hrc == S_OK)
            {
                /* write success followed by GUID */
                vrc = aClient->write (SVCHlpMsg::OK);
                if (RT_FAILURE (vrc)) break;
            }
            else
            {
                vrc = VERR_GENERAL_FAILURE;
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
        case SVCHlpMsg::DhcpRediscover:
        {
            LogFlowFunc (("DhcpRediscover:\n"));

            Guid guid;
            vrc = aClient->read (guid);
            if (RT_FAILURE (vrc)) break;

            Utf8Str errMsg;
            hrc = VBoxNetCfgWinDhcpRediscover ((const GUID *)guid.raw());

            if (hrc == S_OK)
            {
                /* write success followed by GUID */
                vrc = aClient->write (SVCHlpMsg::OK);
                if (RT_FAILURE (vrc)) break;
            }
            else
            {
                vrc = VERR_GENERAL_FAILURE;
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
            rc = collectNetIfInfo(name, Guid(IfGuid), &Info);
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

#endif /* VBOX_WITH_NETFLT */


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
    if(hr == S_OK)
    {
        GUID                IfGuid;
        hr = pIf->COMGETTER(Id)(&IfGuid);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            memset(pInfo, 0, sizeof(NETIFINFO));
            Guid guid(IfGuid);
            pInfo->Uuid = *(guid.raw());

            return collectNetIfInfo(name, guid, pInfo);
        }
    }
    return VERR_GENERAL_FAILURE;
#endif
}

int NetIfCreateHostOnlyNetworkInterface (VirtualBox *pVBox,
                                  IHostNetworkInterface **aHostNetworkInterface,
                                  IProgress **aProgress)
{
#ifndef VBOX_WITH_NETFLT
    return VERR_NOT_IMPLEMENTED;
#else
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
            d->vBox = pVBox;

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
#endif
}

int NetIfRemoveHostOnlyNetworkInterface (VirtualBox *pVBox, IN_GUID aId,
                                  IHostNetworkInterface **aHostNetworkInterface,
                                  IProgress **aProgress)
{
#ifndef VBOX_WITH_NETFLT
    return VERR_NOT_IMPLEMENTED;
#else
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
#endif
}

int NetIfEnableStaticIpConfig(VirtualBox *vBox, HostNetworkInterface * pIf, ULONG aOldIp, ULONG ip, ULONG mask)
{
#ifndef VBOX_WITH_NETFLT
    return VERR_NOT_IMPLEMENTED;
#else
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
#endif
}

int NetIfEnableStaticIpConfigV6(VirtualBox *vBox, HostNetworkInterface * pIf, IN_BSTR aOldIPV6Address, IN_BSTR aIPV6Address, ULONG aIPV6MaskPrefixLength)
{
#ifndef VBOX_WITH_NETFLT
    return VERR_NOT_IMPLEMENTED;
#else
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
#endif
}

int NetIfEnableDynamicIpConfig(VirtualBox *vBox, HostNetworkInterface * pIf)
{
#ifndef VBOX_WITH_NETFLT
    return VERR_NOT_IMPLEMENTED;
#else
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
#endif
}

int NetIfDhcpRediscover(VirtualBox *vBox, HostNetworkInterface * pIf)
{
#ifndef VBOX_WITH_NETFLT
    return VERR_NOT_IMPLEMENTED;
#else
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

                    d->msgCode = SVCHlpMsg::DhcpRediscover;
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
#endif
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
