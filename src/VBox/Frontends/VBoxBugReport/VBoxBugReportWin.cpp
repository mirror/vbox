/* $Id$ */
/** @file
 * VBoxBugReportWin - VirtualBox command-line diagnostics tool,
 * Windows-specific part.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <VBox/com/com.h>
#include <VBox/com/string.h>

#include <iprt/cpp/exception.h>

#include "VBoxBugReport.h"

#include <netcfgx.h>
#include <devguid.h>


#define ReleaseAndReset(obj) \
    if (obj) \
        obj->Release(); \
    obj = NULL;


class BugReportNetworkAdaptersWin : public BugReportStream
{
public:
    BugReportNetworkAdaptersWin() : BugReportStream("NetworkAdapters") {};
    virtual ~BugReportNetworkAdaptersWin() {};
    virtual PRTSTREAM getStream(void) { collect(); return BugReportStream::getStream(); };
private:
    struct CharacteristicsName
    {
        DWORD dwChar;
        const char *szName;
    };
    void printCharteristics(DWORD dwChars);
    void collect();
    void collectNetCfgComponentInfo(int ident, bool fEnabled, INetCfgComponent *pComponent);
};



void BugReportNetworkAdaptersWin::printCharteristics(DWORD dwChars)
{
    static CharacteristicsName cMap[] =
    {
        { NCF_VIRTUAL, "virtual" },
	{ NCF_SOFTWARE_ENUMERATED, "software_enumerated" },
	{ NCF_PHYSICAL, "physical" },
        { NCF_HIDDEN, "hidden" },
	{ NCF_NO_SERVICE, "no_service" },
	{ NCF_NOT_USER_REMOVABLE, "not_user_removable" },
	{ NCF_MULTIPORT_INSTANCED_ADAPTER, "multiport_instanced_adapter" },
	{ NCF_HAS_UI, "has_ui" },
	{ NCF_SINGLE_INSTANCE, "single_instance" },
	{ NCF_FILTER, "filter" },
	{ NCF_DONTEXPOSELOWER, "dontexposelower" },
	{ NCF_HIDE_BINDING, "hide_binding" },
	{ NCF_NDIS_PROTOCOL, "ndis_protocol" },
	{ NCF_FIXED_BINDING, "fixed_binding" },
	{ NCF_LW_FILTER, "lw_filter" }
    };
    bool fPrintDelim = false;

    for (int i = 0; i < RT_ELEMENTS(cMap); ++i)
    {
        if (dwChars & cMap[i].dwChar)
        {
            if (fPrintDelim)
            {
                putStr(", ");
                fPrintDelim = false;
            }
            putStr(cMap[i].szName);
            fPrintDelim = true;
        }
    }
}

void BugReportNetworkAdaptersWin::collectNetCfgComponentInfo(int ident, bool fEnabled, INetCfgComponent *pComponent)
{
    LPWSTR pwszName = NULL;
    HRESULT hr = pComponent->GetDisplayName(&pwszName);
    if (FAILED(hr))
        throw RTCError(com::Utf8StrFmt("Failed to get component display name, hr=0x%x.\n", hr));
    printf("%s%c %ls [", RTCString(ident, ' ').c_str(), fEnabled ? '+' : '-', pwszName);
    if (pwszName)
        CoTaskMemFree(pwszName);

    DWORD dwChars = 0;
    hr = pComponent->GetCharacteristics(&dwChars);
    if (FAILED(hr))
        throw RTCError(com::Utf8StrFmt("Failed to get component characteristics, hr=0x%x.\n", hr));
    printCharteristics(dwChars);
    putStr("]\n");
}

void BugReportNetworkAdaptersWin::collect(void)
{
    INetCfg                     *pNetCfg          = NULL;
    IEnumNetCfgComponent        *pEnumAdapters    = NULL;
    INetCfgComponent            *pNetCfgAdapter   = NULL;
    INetCfgComponentBindings    *pAdapterBindings = NULL;
    IEnumNetCfgBindingPath      *pEnumBp          = NULL;
    INetCfgBindingPath          *pBp              = NULL;
    IEnumNetCfgBindingInterface *pEnumBi          = NULL;
    INetCfgBindingInterface     *pBi              = NULL;
    INetCfgComponent            *pUpperComponent  = NULL;

    try
    {
        HRESULT hr = CoCreateInstance(CLSID_CNetCfg, NULL, CLSCTX_INPROC_SERVER, IID_INetCfg, (PVOID*)&pNetCfg);
        if (FAILED(hr))
            throw RTCError(com::Utf8StrFmt("Failed to create instance of INetCfg, hr=0x%x.\n", hr));
        hr = pNetCfg->Initialize(NULL);
        if (FAILED(hr))
            throw RTCError(com::Utf8StrFmt("Failed to initialize instance of INetCfg, hr=0x%x.\n", hr));

        hr = pNetCfg->EnumComponents(&GUID_DEVCLASS_NET, &pEnumAdapters);
        if (FAILED(hr))
            throw RTCError(com::Utf8StrFmt("Failed enumerate network adapters, hr=0x%x.\n", hr));

        hr = pEnumAdapters->Reset();
        Assert(SUCCEEDED(hr));
        do
        {
            hr = pEnumAdapters->Next(1, &pNetCfgAdapter, NULL);
            if (hr == S_FALSE)
                break;
            if (hr != S_OK)
                throw RTCError(com::Utf8StrFmt("Failed to get next network adapter, hr=0x%x.\n", hr));
            hr = pNetCfgAdapter->QueryInterface(IID_INetCfgComponentBindings, (PVOID*)&pAdapterBindings);
            if (FAILED(hr))
                throw RTCError(com::Utf8StrFmt("Failed to query INetCfgComponentBindings, hr=0x%x.\n", hr));
            hr = pAdapterBindings->EnumBindingPaths(EBP_ABOVE, &pEnumBp);
            if (FAILED(hr))
                throw RTCError(com::Utf8StrFmt("Failed to enumerate binding paths, hr=0x%x.\n", hr));
            hr = pEnumBp->Reset();
            if (FAILED(hr))
                throw RTCError(com::Utf8StrFmt("Failed to reset enumeration of binding paths (0x%x)\n", hr));
            do
            {
                hr = pEnumBp->Next(1, &pBp, NULL);
                if (hr == S_FALSE)
                    break;
                if (hr != S_OK)
                    throw RTCError(com::Utf8StrFmt("Failed to get next network adapter, hr=0x%x.\n", hr));
                bool fBpEnabled;
                hr =  pBp->IsEnabled();
                if (hr == S_FALSE)
                    fBpEnabled = false;
                else if (hr != S_OK)
                    throw RTCError(com::Utf8StrFmt("Failed to check if bind path is enabled, hr=0x%x.\n", hr));
                else
                    fBpEnabled = true;
                hr = pBp->EnumBindingInterfaces(&pEnumBi);
                if (FAILED(hr))
                    throw RTCError(com::Utf8StrFmt("Failed to enumerate binding interfaces (0x%x)\n", hr));
                hr = pEnumBi->Reset();
                if (FAILED(hr))
                    throw RTCError(com::Utf8StrFmt("Failed to reset enumeration of binding interfaces (0x%x)\n", hr));
                int ident;
                for (ident = 0;; ++ident)
                {
                    hr = pEnumBi->Next(1, &pBi, NULL);
                    if (hr == S_FALSE)
                        break;
                    if (hr != S_OK)
                        throw RTCError(com::Utf8StrFmt("Failed to get next binding interface, hr=0x%x.\n", hr));
                    hr = pBi->GetUpperComponent(&pUpperComponent);
                    if (FAILED(hr))
                        throw RTCError(com::Utf8StrFmt("Failed to get upper component, hr=0x%x.\n", hr));
                    collectNetCfgComponentInfo(ident, fBpEnabled, pUpperComponent);
                    ReleaseAndReset(pUpperComponent);
                    ReleaseAndReset(pBi);
                }
                collectNetCfgComponentInfo(ident, fBpEnabled, pNetCfgAdapter);
                ReleaseAndReset(pEnumBi);
                ReleaseAndReset(pBp);
            } while (true);

            ReleaseAndReset(pEnumBp);
            ReleaseAndReset(pAdapterBindings);
            ReleaseAndReset(pNetCfgAdapter);
        } while (true);
        ReleaseAndReset(pEnumAdapters);
        ReleaseAndReset(pNetCfg);
    }

    catch (RTCError &e)
    {
        ReleaseAndReset(pUpperComponent);
        ReleaseAndReset(pBi);
        ReleaseAndReset(pEnumBi);
        ReleaseAndReset(pBp);
        ReleaseAndReset(pEnumBp);
        ReleaseAndReset(pAdapterBindings);
        ReleaseAndReset(pNetCfgAdapter);
        ReleaseAndReset(pEnumAdapters);
        ReleaseAndReset(pNetCfg);
        RTPrintf("ERROR in osCollect: %s\n", e.what());
        throw;
    }
            
}

void createBugReportOsSpecific(BugReport* report, const char *pszHome)
{
    WCHAR szWinDir[MAX_PATH];

    int cbNeeded = GetWindowsDirectory(szWinDir, RT_ELEMENTS(szWinDir));
    if (cbNeeded == 0)
        throw RTCError(RTCStringFmt("Failed to get Windows directory (err=%d)\n", GetLastError()));
    if (cbNeeded > MAX_PATH)
        throw RTCError(RTCStringFmt("Failed to get Windows directory (needed %d-byte buffer)\n", cbNeeded));
    RTCStringFmt WinInfDir("%ls/inf", szWinDir);
    report->addItem(new BugReportFile(PathJoin(WinInfDir.c_str(), "setupapi.app.log"), "setupapi.app.log"));
    report->addItem(new BugReportFile(PathJoin(WinInfDir.c_str(), "setupapi.dev.log"), "setupapi.dev.log"));
    report->addItem(new BugReportNetworkAdaptersWin);
    RTCStringFmt WinSysDir("%ls/System32", szWinDir);
    report->addItem(new BugReportCommand("SystemEvents", PathJoin(WinSysDir.c_str(), "wevtutil.exe"),
                                         "qe", "System", "/f:text",
                                         "/q:*[System[Provider[@Name='VBoxUSBMon']]]", NULL));
    report->addItem(new BugReportCommand("UpdateHistory", PathJoin(WinSysDir.c_str(), "wbem/wmic.exe"),
                                         "qfe", "list", "brief", NULL));
    report->addItem(new BugReportCommand("DriverServices", PathJoin(WinSysDir.c_str(), "sc.exe"),
                                         "query", "type=", "driver", "state=", "all", NULL));
}
