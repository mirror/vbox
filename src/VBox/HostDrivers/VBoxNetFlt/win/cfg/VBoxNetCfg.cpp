/* $Id$ */
/** @file
 * VBoxNetCfg.cpp - Network Configuration API.
 */
/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#include "VBox/VBoxNetCfg-win.h"
#include "VBox/VBoxDrvCfg-win.h"

#define _WIN32_DCOM

#include <iphlpapi.h>

#include <devguid.h>
#include <stdio.h>
#include <regstr.h>
#include <shlobj.h>
#include <cfgmgr32.h>
#include <tchar.h>
#include <objbase.h>

#include <crtdbg.h>
#include <stdlib.h>
#include <string.h>

#include <Wbemidl.h>
#include <comdef.h>


#ifndef Assert   /** @todo r=bird: where would this be defined? */
//# ifdef DEBUG
//#  define Assert(_expr) assert(_expr)
//# else
//#  define Assert(_expr) do{ }while (0)
//# endif
# define Assert _ASSERT
# define AssertMsg(expr, msg) do{}while (0)
#endif
static LOG_ROUTINE g_Logger = NULL;

static VOID DoLogging(LPCSTR szString, ...);
#define NonStandardLog DoLogging
#define NonStandardLogFlow(x) DoLogging x

#define DbgLog                              /** @todo r=bird: What does this do? */

#define VBOX_NETCFG_LOCK_TIME_OUT     5000  /** @todo r=bird: What does this do? */


static HRESULT vboxNetCfgWinINetCfgLock(IN INetCfg *pNetCfg,
                                        IN LPCWSTR pszwClientDescription,
                                        IN DWORD cmsTimeout,
                                        OUT LPWSTR *ppszwClientDescription)
{
    INetCfgLock *pLock;
    HRESULT hr = pNetCfg->QueryInterface(IID_INetCfgLock, (PVOID*)&pLock);
    if (FAILED(hr))
    {
        NonStandardLogFlow(("QueryInterface failed, hr (0x%x)\n", hr));
        return hr;
    }

    hr = pLock->AcquireWriteLock(cmsTimeout, pszwClientDescription, ppszwClientDescription);
    if (hr == S_FALSE)
    {
        NonStandardLogFlow(("Write lock busy\n"));
    }
    else if (FAILED(hr))
    {
        NonStandardLogFlow(("AcquireWriteLock failed, hr (0x%x)\n", hr));
    }

    pLock->Release();
    return hr;
}

static HRESULT vboxNetCfgWinINetCfgUnlock(IN INetCfg *pNetCfg)
{
    INetCfgLock *pLock;
    HRESULT hr = pNetCfg->QueryInterface(IID_INetCfgLock, (PVOID*)&pLock);
    if (FAILED(hr))
    {
        NonStandardLogFlow(("QueryInterface failed, hr (0x%x)\n", hr));
        return hr;
    }

    hr = pLock->ReleaseWriteLock();
    if (FAILED(hr))
        NonStandardLogFlow(("ReleaseWriteLock failed, hr (0x%x)\n", hr));

    pLock->Release();
    return hr;
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinQueryINetCfg(OUT INetCfg **ppNetCfg,
                                                      IN BOOL fGetWriteLock,
                                                      IN LPCWSTR pszwClientDescription,
                                                      IN DWORD cmsTimeout,
                                                      OUT LPWSTR *ppszwClientDescription)
{
    INetCfg *pNetCfg;
    HRESULT hr = CoCreateInstance(CLSID_CNetCfg, NULL, CLSCTX_INPROC_SERVER, IID_INetCfg, (PVOID*)&pNetCfg);
    if (FAILED(hr))
    {
        NonStandardLogFlow(("CoCreateInstance failed, hr (0x%x)\n", hr));
        return hr;
    }

    if (fGetWriteLock)
    {
        hr = vboxNetCfgWinINetCfgLock(pNetCfg, pszwClientDescription, cmsTimeout, ppszwClientDescription);
        if (hr == S_FALSE)
        {
            NonStandardLogFlow(("Write lock is busy\n", hr));
            hr = NETCFG_E_NO_WRITE_LOCK;
        }
    }

    if (SUCCEEDED(hr))
    {
        hr = pNetCfg->Initialize(NULL);
        if (SUCCEEDED(hr))
        {
            *ppNetCfg = pNetCfg;
            return S_OK;
        }
        else
            NonStandardLogFlow(("Initialize failed, hr (0x%x)\n", hr));
    }

    pNetCfg->Release();
    return hr;
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinReleaseINetCfg(IN INetCfg *pNetCfg, IN BOOL fHasWriteLock)
{
    HRESULT hr = pNetCfg->Uninitialize();
    if (FAILED(hr))
    {
        NonStandardLogFlow(("Uninitialize failed, hr (0x%x)\n", hr));
        return hr;
    }

    if (fHasWriteLock)
    {
        hr = vboxNetCfgWinINetCfgUnlock(pNetCfg);
        if (FAILED(hr))
            NonStandardLogFlow(("vboxNetCfgWinINetCfgUnlock failed, hr (0x%x)\n", hr));
    }

    pNetCfg->Release();
    return hr;
}

static HRESULT vboxNetCfgWinGetComponentByGuidEnum(IEnumNetCfgComponent *pEnumNcc,
                                                   IN const GUID *pGuid,
                                                   OUT INetCfgComponent **ppNcc)
{
    HRESULT hr = pEnumNcc->Reset();
    if (FAILED(hr))
    {
        NonStandardLogFlow(("Reset failed, hr (0x%x)\n", hr));
        return hr;
    }

    INetCfgComponent *pNcc;
    while ((hr = pEnumNcc->Next(1, &pNcc, NULL)) == S_OK)
    {
        ULONG uComponentStatus;
        hr = pNcc->GetDeviceStatus(&uComponentStatus);
        if (SUCCEEDED(hr))
        {
            if (uComponentStatus == 0)
            {
                GUID NccGuid;
                hr = pNcc->GetInstanceGuid(&NccGuid);

                if (SUCCEEDED(hr))
                {
                    if (NccGuid == *pGuid)
                    {
                        /* found the needed device */
                        *ppNcc = pNcc;
                        break;
                    }
                }
                else
                    NonStandardLogFlow(("GetInstanceGuid failed, hr (0x%x)\n", hr));
            }
        }

        pNcc->Release();
    }
    return hr;
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGetComponentByGuid(IN INetCfg *pNc,
                                                            IN const GUID *pguidClass,
                                                            IN const GUID * pComponentGuid,
                                                            OUT INetCfgComponent **ppncc)
{
    IEnumNetCfgComponent *pEnumNcc;
    HRESULT hr = pNc->EnumComponents(pguidClass, &pEnumNcc);

    if (SUCCEEDED(hr))
    {
        hr = vboxNetCfgWinGetComponentByGuidEnum(pEnumNcc, pComponentGuid, ppncc);
        if (hr == S_FALSE)
        {
            NonStandardLogFlow(("Component not found\n"));
        }
        else if (FAILED(hr))
        {
            NonStandardLogFlow(("vboxNetCfgWinGetComponentByGuidEnum failed, hr (0x%x)\n", hr));
        }
        pEnumNcc->Release();
    }
    else
        NonStandardLogFlow(("EnumComponents failed, hr (0x%x)\n", hr));
    return hr;
}

static HRESULT vboxNetCfgWinQueryInstaller(IN INetCfg *pNetCfg, IN const GUID *pguidClass, INetCfgClassSetup **ppSetup)
{
    HRESULT hr = pNetCfg->QueryNetCfgClass(pguidClass, IID_INetCfgClassSetup, (void**)ppSetup);
    if (FAILED(hr))
        NonStandardLogFlow(("QueryNetCfgClass failed, hr (0x%x)\n", hr));
    return hr;
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinInstallComponent(IN INetCfg *pNetCfg, IN LPCWSTR pszwComponentId, IN const GUID *pguidClass,
                                                          OUT INetCfgComponent **ppComponent)
{
    INetCfgClassSetup *pSetup;
    HRESULT hr = vboxNetCfgWinQueryInstaller(pNetCfg, pguidClass, &pSetup);
    if (FAILED(hr))
    {
        NonStandardLogFlow(("vboxNetCfgWinQueryInstaller failed, hr (0x%x)\n", hr));
        return hr;
    }

    OBO_TOKEN Token;
    ZeroMemory(&Token, sizeof (Token));
    Token.Type = OBO_USER;

    hr = pSetup->Install(pszwComponentId, &Token,
                         0,    /* IN DWORD dwSetupFlags */
                         0,    /* IN DWORD dwUpgradeFromBuildNo */
                         NULL, /* IN LPCWSTR pszwAnswerFile */
                         NULL, /* IN LPCWSTR pszwAnswerSections */
                         ppComponent);
    if (SUCCEEDED(hr))
    {
        /* ignore the apply failure */
        HRESULT tmpHr = pNetCfg->Apply();
        Assert(tmpHr == S_OK);
        if (tmpHr != S_OK)
            NonStandardLogFlow(("Apply failed, hr (0x%x)\n", tmpHr));
    }
    else
        NonStandardLogFlow(("Install failed, hr (0x%x)\n", hr));

    pSetup->Release();
    return hr;
}

/** @todo r=bird: This function is not in the header file, why is it
 *        exported? */
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinInstallInfAndComponent(IN INetCfg *pNetCfg, IN LPCWSTR pszwComponentId, IN const GUID *pguidClass,
                                                                IN LPCWSTR const *apInfPaths, IN UINT cInfPaths,
                                                                OUT INetCfgComponent **ppComponent)
{
    HRESULT hr = S_OK;
    UINT i = 0;

    NonStandardLogFlow(("Installing %u INF files ...\n", cInfPaths));

    for (; i < cInfPaths; i++)
    {
        NonStandardLogFlow(("Installing INF file \"%ws\" ...\n", apInfPaths[i]));
        hr = VBoxDrvCfgInfInstall(apInfPaths[i]);
        if (FAILED(hr))
        {
            NonStandardLogFlow(("VBoxNetCfgWinInfInstall failed, hr (0x%x)\n", hr));
            break;
        }
    }

    if (SUCCEEDED(hr))
    {
        hr = VBoxNetCfgWinInstallComponent(pNetCfg, pszwComponentId, pguidClass, ppComponent);
        if (FAILED(hr))
            NonStandardLogFlow(("VBoxNetCfgWinInstallComponent failed, hr (0x%x)\n", hr));
    }

    if (FAILED(hr))
    {
        for (UINT j = i - 1; j != 0; j--)
            VBoxDrvCfgInfUninstall(apInfPaths[j], 0);
    }

    return hr;
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinUninstallComponent(IN INetCfg *pNetCfg, IN INetCfgComponent *pComponent)
{
    GUID GuidClass;
    HRESULT hr = pComponent->GetClassGuid(&GuidClass);
    if (FAILED(hr))
    {
        NonStandardLogFlow(("GetClassGuid failed, hr (0x%x)\n", hr));
        return hr;
    }

    INetCfgClassSetup *pSetup = NULL;
    hr = vboxNetCfgWinQueryInstaller(pNetCfg, &GuidClass, &pSetup);
    if (FAILED(hr))
    {
        NonStandardLogFlow(("vboxNetCfgWinQueryInstaller failed, hr (0x%x)\n", hr));
        return hr;
    }

    OBO_TOKEN Token;
    ZeroMemory(&Token, sizeof(Token));
    Token.Type = OBO_USER;

    hr = pSetup->DeInstall(pComponent, &Token, NULL /* OUT LPWSTR *pmszwRefs */);
    if (SUCCEEDED(hr))
    {
        hr = pNetCfg->Apply();
        if (FAILED(hr))
            NonStandardLogFlow(("Apply failed, hr (0x%x)\n", hr));
    }
    else
        NonStandardLogFlow(("DeInstall failed, hr (0x%x)\n", hr));

    if (pSetup)
        pSetup->Release();
    return hr;
}

typedef BOOL (*VBOXNETCFGWIN_NETCFGENUM_CALLBACK) (IN INetCfg *pNetCfg, IN INetCfgComponent *pNetCfgComponent, PVOID pContext);

static HRESULT vboxNetCfgWinEnumNetCfgComponents(IN INetCfg *pNetCfg,
                                                 IN const GUID *pguidClass,
                                                 VBOXNETCFGWIN_NETCFGENUM_CALLBACK callback,
                                                 PVOID pContext)
{
    IEnumNetCfgComponent *pEnumComponent;
    HRESULT hr = pNetCfg->EnumComponents(pguidClass, &pEnumComponent);
    if (SUCCEEDED(hr))
    {
        INetCfgComponent *pNetCfgComponent;
        hr = pEnumComponent->Reset();
        do
        {
            hr = pEnumComponent->Next(1, &pNetCfgComponent, NULL);
            if (hr == S_OK)
            {
//                ULONG uComponentStatus;
//                hr = pNcc->GetDeviceStatus(&uComponentStatus);
//                if (SUCCEEDED(hr))
                BOOL fResult = FALSE;
                if (pNetCfgComponent)
                {
                    if (pContext)
                        fResult = callback(pNetCfg, pNetCfgComponent, pContext);
                    pNetCfgComponent->Release();
                }

                if (!fResult)
                    break;
            }
            else
            {
                if (hr == S_FALSE)
                {
                    hr = S_OK;
                }
                else
                    NonStandardLogFlow(("Next failed, hr (0x%x)\n", hr));
                break;
            }
        } while (true);
        pEnumComponent->Release();
    }
    return hr;
}

static BOOL vboxNetCfgWinRemoveAllNetDevicesOfIdCallback(HDEVINFO hDevInfo, PSP_DEVINFO_DATA pDev, PVOID pContext)
{
    HRESULT hr = S_OK;
    SP_REMOVEDEVICE_PARAMS rmdParams;

    rmdParams.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
    rmdParams.ClassInstallHeader.InstallFunction = DIF_REMOVE;
    rmdParams.Scope = DI_REMOVEDEVICE_GLOBAL;
    rmdParams.HwProfile = 0;

    if (SetupDiSetClassInstallParams(hDevInfo,pDev,&rmdParams.ClassInstallHeader,sizeof(rmdParams)))
    {
        if (SetupDiSetSelectedDevice (hDevInfo, pDev))
        {
            if (SetupDiCallClassInstaller(DIF_REMOVE,hDevInfo,pDev))
            {
                SP_DEVINSTALL_PARAMS devParams;
                devParams.cbSize = sizeof(devParams);
                if (SetupDiGetDeviceInstallParams(hDevInfo,pDev,&devParams))
                {
                    if (devParams.Flags & (DI_NEEDRESTART|DI_NEEDREBOOT))
                    {
                        hr = S_FALSE;
                        NonStandardLog(("!!!REBOOT REQUIRED!!!\n"));
                    }
                }
            }
            else
            {
                DWORD dwErr = GetLastError();
                NonStandardLogFlow(("SetupDiCallClassInstaller failed with %ld\n", dwErr));
                hr = HRESULT_FROM_WIN32(dwErr);
            }
        }
        else
        {
            DWORD dwErr = GetLastError();
            NonStandardLogFlow(("SetupDiSetSelectedDevice failed with %ld\n", dwErr));
            hr = HRESULT_FROM_WIN32(dwErr);
        }
    }
    else
    {
        DWORD dwErr = GetLastError();
        NonStandardLogFlow(("SetupDiSetClassInstallParams failed with %ld\n", dwErr));
        hr = HRESULT_FROM_WIN32(dwErr);
    }

    return TRUE;
}

typedef struct VBOXNECTFGWINPROPCHANGE
{
    VBOXNECTFGWINPROPCHANGE_TYPE enmPcType;
    HRESULT hr;
} VBOXNECTFGWINPROPCHANGE ,*PVBOXNECTFGWINPROPCHANGE;

static BOOL vboxNetCfgWinPropChangeAllNetDevicesOfIdCallback(HDEVINFO hDevInfo, PSP_DEVINFO_DATA pDev, PVOID pContext)
{
    PVBOXNECTFGWINPROPCHANGE pPc = (PVBOXNECTFGWINPROPCHANGE)pContext;
    HRESULT hr = S_OK;
    SP_PROPCHANGE_PARAMS PcParams;
    memset (&PcParams, 0, sizeof (PcParams));

    PcParams.ClassInstallHeader.cbSize = sizeof (SP_CLASSINSTALL_HEADER);
    PcParams.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
    PcParams.Scope = DICS_FLAG_GLOBAL;
    PcParams.HwProfile = 0;
    switch(pPc->enmPcType)
    {
        case VBOXNECTFGWINPROPCHANGE_TYPE_DISABLE:
            PcParams.StateChange = DICS_DISABLE;
            break;
        case VBOXNECTFGWINPROPCHANGE_TYPE_ENABLE:
            PcParams.StateChange = DICS_ENABLE;
            break;
        default:
            NonStandardLogFlow(("unexpected prop change type: %d\n", pPc->enmPcType));
            pPc->hr = E_INVALIDARG;
            return FALSE;
    }


    if (SetupDiSetClassInstallParams(hDevInfo, pDev, &PcParams.ClassInstallHeader, sizeof (PcParams)))
    {
        if (SetupDiSetSelectedDevice(hDevInfo, pDev))
        {
            if (SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, hDevInfo, pDev))
            {
                SP_DEVINSTALL_PARAMS devParams;
                devParams.cbSize = sizeof(devParams);
                if (SetupDiGetDeviceInstallParams(hDevInfo,pDev,&devParams))
                {
                    if (devParams.Flags & (DI_NEEDRESTART|DI_NEEDREBOOT))
                    {
                        hr = S_FALSE;
                        NonStandardLogFlow(("PropChange: !!!REBOOT REQUIRED!!!\n"));
                    }
                }
            }
            else
            {
                DWORD dwErr = GetLastError();
                NonStandardLogFlow(("SetupDiCallClassInstaller failed with %ld\n", dwErr));
                hr = HRESULT_FROM_WIN32(dwErr);
            }
        }
        else
        {
            DWORD dwErr = GetLastError();
            NonStandardLogFlow(("SetupDiSetSelectedDevice failed with %ld\n", dwErr));
            hr = HRESULT_FROM_WIN32(dwErr);
        }
    }
    else
    {
        DWORD dwErr = GetLastError();
        NonStandardLogFlow(("SetupDiSetClassInstallParams failed with %ld\n", dwErr));
        hr = HRESULT_FROM_WIN32(dwErr);
    }

    return TRUE;
}

typedef BOOL (*VBOXNETCFGWIN_NETENUM_CALLBACK) (HDEVINFO hDevInfo, PSP_DEVINFO_DATA pDev, PVOID pContext);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinEnumNetDevices(LPCWSTR pPnPId, VBOXNETCFGWIN_NETENUM_CALLBACK callback, PVOID pContext)
{
    DWORD winEr;
    HRESULT hr = S_OK;

    HDEVINFO hDevInfo = SetupDiGetClassDevsExW(
            &GUID_DEVCLASS_NET,
            NULL, /* IN PCTSTR Enumerator, OPTIONAL*/
            NULL, /*IN HWND hwndParent, OPTIONAL*/
            DIGCF_PRESENT, /*IN DWORD Flags,*/
            NULL, /*IN HDEVINFO DeviceInfoSet, OPTIONAL*/
            NULL, /*IN PCTSTR MachineName, OPTIONAL*/
            NULL /*IN PVOID Reserved*/
        );
    if (hDevInfo != INVALID_HANDLE_VALUE)
    {
        DWORD iDev = 0;
        SP_DEVINFO_DATA Dev;
        PBYTE pBuffer = NULL;
        DWORD cbBuffer = 0;
        DWORD cbRequired = 0;
        BOOL bEnumCompleted;
        size_t cPnPId = wcslen(pPnPId);

        Dev.cbSize = sizeof(Dev);

        for (; bEnumCompleted = SetupDiEnumDeviceInfo(hDevInfo, iDev, &Dev); iDev++)
        {
            if (!SetupDiGetDeviceRegistryPropertyW(hDevInfo,&Dev,
                      SPDRP_HARDWAREID, /* IN DWORD Property,*/
                      NULL, /*OUT PDWORD PropertyRegDataType, OPTIONAL*/
                      pBuffer, /*OUT PBYTE PropertyBuffer,*/
                      cbBuffer, /* IN DWORD PropertyBufferSize,*/
                      &cbRequired /*OUT PDWORD RequiredSize OPTIONAL*/
                    ))
            {
                winEr = GetLastError();
                if (winEr != ERROR_INSUFFICIENT_BUFFER)
                {
                    NonStandardLogFlow(("SetupDiGetDeviceRegistryPropertyW (1) failed winErr(%d)\n", winEr));
                    hr = HRESULT_FROM_WIN32(winEr);
                    break;
                }

                if (pBuffer)
                    free(pBuffer);

                pBuffer = (PBYTE)malloc(cbRequired);
                cbBuffer = cbRequired;

                if (!SetupDiGetDeviceRegistryPropertyW(hDevInfo,&Dev,
                                                       SPDRP_HARDWAREID, /* IN DWORD Property,*/
                                                       NULL, /*OUT PDWORD PropertyRegDataType, OPTIONAL*/
                                                       pBuffer, /*OUT PBYTE PropertyBuffer,*/
                                                       cbBuffer, /* IN DWORD PropertyBufferSize,*/
                                                       &cbRequired /*OUT PDWORD RequiredSize OPTIONAL*/
                                                       ))
                {
                    winEr = GetLastError();
                    NonStandardLogFlow(("SetupDiGetDeviceRegistryPropertyW (2) failed winErr(%d)\n", winEr));
                    hr = HRESULT_FROM_WIN32(winEr);
                    break;
                }
            }

            PWCHAR pCurId = (PWCHAR)pBuffer;
            size_t cCurId = wcslen(pCurId);
            if (cCurId >= cPnPId)
            {
                pCurId += cCurId - cPnPId;
                if (!wcsnicmp(pCurId, pPnPId, cPnPId))
                {

                    if (!callback(hDevInfo,&Dev,pContext))
                        break;
                }
            }

        }

        if (pBuffer)
            free(pBuffer);

        if (bEnumCompleted)
        {
            winEr = GetLastError();
            hr = winEr == ERROR_NO_MORE_ITEMS ? S_OK : HRESULT_FROM_WIN32(winEr);
        }

        SetupDiDestroyDeviceInfoList(hDevInfo);
    }
    else
    {
        DWORD winEr = GetLastError();
        NonStandardLogFlow(("SetupDiGetClassDevsExW failed winErr(%d)\n", winEr));
        hr = HRESULT_FROM_WIN32(winEr);
    }

    return hr;
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinRemoveAllNetDevicesOfId(IN LPCWSTR lpszPnPId)
{
    return VBoxNetCfgWinEnumNetDevices(lpszPnPId, vboxNetCfgWinRemoveAllNetDevicesOfIdCallback, NULL);
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinPropChangeAllNetDevicesOfId(IN LPCWSTR lpszPnPId, VBOXNECTFGWINPROPCHANGE_TYPE enmPcType)
{
    VBOXNECTFGWINPROPCHANGE Pc;
    Pc.enmPcType = enmPcType;
    Pc.hr = S_OK;
    HRESULT hr = VBoxNetCfgWinEnumNetDevices(lpszPnPId, vboxNetCfgWinPropChangeAllNetDevicesOfIdCallback, &Pc);
    if (!SUCCEEDED(hr))
    {
        NonStandardLogFlow(("VBoxNetCfgWinEnumNetDevices failed 0x%x\n", hr));
        return hr;
    }

    if (!SUCCEEDED(Pc.hr))
    {
        NonStandardLogFlow(("vboxNetCfgWinPropChangeAllNetDevicesOfIdCallback failed 0x%x\n", Pc.hr));
        return Pc.hr;
    }

    return S_OK;
}

/*
 * logging
 */
static VOID DoLogging(LPCSTR szString, ...)
{
    LOG_ROUTINE pfnRoutine = (LOG_ROUTINE)(*((void * volatile *)&g_Logger));
    if (pfnRoutine)
    {
        char szBuffer[4096] = {0};
        va_list va;
        va_start(va, szString);
        _vsnprintf(szBuffer, RT_ELEMENTS(szBuffer), szString, va);
        va_end(va);

        pfnRoutine(szBuffer);
    }
}

VBOXNETCFGWIN_DECL(VOID) VBoxNetCfgWinSetLogging(IN LOG_ROUTINE pfnLog)
{
    *((void * volatile *)&g_Logger) = pfnLog;
}

/*
 * IP configuration API
 */
/* network settings config */
/**
 *  Strong referencing operators. Used as a second argument to ComPtr<>/ComObjPtr<>.
 */
template <class C>
class ComStrongRef
{
protected:

    static void addref (C *p) { p->AddRef(); }
    static void release (C *p) { p->Release(); }
};


/**
 *  Base template for smart COM pointers. Not intended to be used directly.
 */
template <class C, template <class> class RefOps = ComStrongRef>
class ComPtrBase : protected RefOps <C>
{
public:

    /* special template to disable AddRef()/Release() */
    template <class I>
    class NoAddRefRelease : public I
    {
        private:
#if !defined (VBOX_WITH_XPCOM)
            STDMETHOD_(ULONG, AddRef)() = 0;
            STDMETHOD_(ULONG, Release)() = 0;
#else /* !defined (VBOX_WITH_XPCOM) */
            NS_IMETHOD_(nsrefcnt) AddRef(void) = 0;
            NS_IMETHOD_(nsrefcnt) Release(void) = 0;
#endif /* !defined (VBOX_WITH_XPCOM) */
    };

protected:

    ComPtrBase () : p (NULL) {}
    ComPtrBase (const ComPtrBase &that) : p (that.p) { addref(); }
    ComPtrBase (C *that_p) : p (that_p) { addref(); }

    ~ComPtrBase() { release(); }

    ComPtrBase &operator= (const ComPtrBase &that)
    {
        safe_assign (that.p);
        return *this;
    }

    ComPtrBase &operator= (C *that_p)
    {
        safe_assign (that_p);
        return *this;
    }

public:

    void setNull()
    {
        release();
        p = NULL;
    }

    bool isNull() const
    {
        return (p == NULL);
    }

    bool operator! () const { return isNull(); }

    bool operator< (C* that_p) const { return p < that_p; }
    bool operator== (C* that_p) const { return p == that_p; }

    template <class I>
    bool equalsTo (I *aThat) const
    {
        return ComPtrEquals (p, aThat);
    }

    template <class OC>
    bool equalsTo (const ComPtrBase <OC> &oc) const
    {
        return equalsTo ((OC *) oc);
    }

    /** Intended to pass instances as in parameters to interface methods */
    operator C* () const { return p; }

    /**
     *  Dereferences the instance (redirects the -> operator to the managed
     *  pointer).
     */
    NoAddRefRelease <C> *operator-> () const
    {
        AssertMsg (p, ("Managed pointer must not be null\n"));
        return (NoAddRefRelease <C> *) p;
    }

    template <class I>
    HRESULT queryInterfaceTo (I **pp) const
    {
        if (pp)
        {
            if (p)
            {
                return p->QueryInterface (COM_IIDOF (I), (void **) pp);
            }
            else
            {
                *pp = NULL;
                return S_OK;
            }
        }

        return E_INVALIDARG;
    }

    /** Intended to pass instances as out parameters to interface methods */
    C **asOutParam()
    {
        setNull();
        return &p;
    }

private:

    void addref()
    {
        if (p)
            RefOps <C>::addref (p);
    }

    void release()
    {
        if (p)
            RefOps <C>::release (p);
    }

    void safe_assign (C *that_p)
    {
        /* be aware of self-assignment */
        if (that_p)
            RefOps <C>::addref (that_p);
        release();
        p = that_p;
    }

    C *p;
};

/**
 *  Smart COM pointer wrapper that automatically manages refcounting of
 *  interface pointers.
 *
 *  @param I    COM interface class
 */
template <class I, template <class> class RefOps = ComStrongRef>
class ComPtr : public ComPtrBase <I, RefOps>
{
    typedef ComPtrBase <I, RefOps> Base;

public:

    ComPtr () : Base() {}
    ComPtr (const ComPtr &that) : Base(that) {}
    ComPtr &operator= (const ComPtr &that)
    {
        Base::operator= (that);
        return *this;
    }

    template <class OI>
    ComPtr (OI *that_p) : Base () { operator= (that_p); }

    /* specialization for I */
    ComPtr (I *that_p) : Base (that_p) {}

    template <class OC>
    ComPtr (const ComPtr <OC, RefOps> &oc) : Base () { operator= ((OC *) oc); }

    template <class OI>
    ComPtr &operator= (OI *that_p)
    {
        if (that_p)
            that_p->QueryInterface (COM_IIDOF (I), (void **) Base::asOutParam());
        else
            Base::setNull();
        return *this;
    }

    /* specialization for I */
    ComPtr &operator=(I *that_p)
    {
        Base::operator= (that_p);
        return *this;
    }

    template <class OC>
    ComPtr &operator= (const ComPtr <OC, RefOps> &oc)
    {
        return operator= ((OC *) oc);
    }
};

static HRESULT netIfWinFindAdapterClassById(IWbemServices * pSvc, const GUID * pGuid, IWbemClassObject **pAdapterConfig)
{
    HRESULT hr;
    WCHAR wszQuery[256];
    WCHAR wszGuid[50];

    int length = StringFromGUID2(*pGuid, wszGuid, RT_ELEMENTS(wszGuid));
    if (length)
    {
        swprintf(wszQuery, L"SELECT * FROM Win32_NetworkAdapterConfiguration WHERE SettingID = \"%s\"", wszGuid);
        IEnumWbemClassObject* pEnumerator = NULL;
        hr = pSvc->ExecQuery(bstr_t("WQL"), bstr_t(wszQuery), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, 
                             NULL, &pEnumerator);
        if (SUCCEEDED(hr))
        {
            if (pEnumerator)
            {
                IWbemClassObject *pclsObj;
                ULONG uReturn = 0;
                hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
                NonStandardLogFlow(("netIfWinFindAdapterClassById: IEnumWbemClassObject::Next -> hr=0x%x pclsObj=%p uReturn=%u 42=%u\n", 
                                    hr, (void *)pclsObj, uReturn, 42));
                if (SUCCEEDED(hr))
                {
                    if (uReturn && pclsObj != NULL)
                    {
                        *pAdapterConfig = pclsObj;
                        pEnumerator->Release();
                        NonStandardLogFlow(("netIfWinFindAdapterClassById: S_OK and %p\n", *pAdapterConfig));
                        return S_OK;
                    }

                    hr = E_FAIL;
                }

                pEnumerator->Release();
            }
            else
            {
                NonStandardLogFlow(("ExecQuery returned no enumerator\n"));
                hr = E_FAIL;
            }
        }
        else
            NonStandardLogFlow(("ExecQuery failed (0x%x)\n", hr));
    }
    else
    {
        DWORD winEr = GetLastError();
        hr = HRESULT_FROM_WIN32( winEr );
        if (SUCCEEDED(hr))
            hr = E_FAIL;
        NonStandardLogFlow(("StringFromGUID2 failed winEr=%u, hr=0x%x\n", winEr, hr));
    }

    NonStandardLogFlow(("netIfWinFindAdapterClassById: 0x%x and %p\n", hr, *pAdapterConfig));
    return hr;
}

static HRESULT netIfWinIsHostOnly(IWbemClassObject * pAdapterConfig, BOOL * pbIsHostOnly)
{
    VARIANT vtServiceName;
    BOOL bIsHostOnly = FALSE;
    VariantInit(&vtServiceName);

    HRESULT hr = pAdapterConfig->Get(L"ServiceName", 0 /*lFlags*/, &vtServiceName, NULL /*pvtType*/, NULL /*plFlavor*/);
    if (SUCCEEDED(hr))
    {
        *pbIsHostOnly = bstr_t(vtServiceName.bstrVal) == bstr_t("VBoxNetAdp");

        VariantClear(&vtServiceName);
    }

    return hr;
}

static HRESULT netIfWinGetIpSettings(IWbemClassObject * pAdapterConfig, ULONG *pIpv4, ULONG *pMaskv4)
{
    VARIANT vtIp;
    HRESULT hr;
    VariantInit(&vtIp);

    *pIpv4 = 0;
    *pMaskv4 = 0;

    hr = pAdapterConfig->Get(L"IPAddress", 0, &vtIp, 0, 0);
    if (SUCCEEDED(hr))
    {
        if (vtIp.vt == (VT_ARRAY | VT_BSTR))
        {
            VARIANT vtMask;
            VariantInit(&vtMask);
            hr = pAdapterConfig->Get(L"IPSubnet", 0, &vtMask, 0, 0);
            if (SUCCEEDED(hr))
            {
                if (vtMask.vt == (VT_ARRAY | VT_BSTR))
                {
                    SAFEARRAY * pIpArray = vtIp.parray;
                    SAFEARRAY * pMaskArray = vtMask.parray;
                    if (pIpArray && pMaskArray)
                    {
                        BSTR pCurIp;
                        BSTR pCurMask;
                        for (LONG i = 0;
                            SafeArrayGetElement(pIpArray, &i, (PVOID)&pCurIp) == S_OK
                            && SafeArrayGetElement(pMaskArray, &i, (PVOID)&pCurMask) == S_OK;
                            i++)
                        {
                            bstr_t ip(pCurIp);

                            ULONG Ipv4 = inet_addr((char*)(ip));
                            if (Ipv4 != INADDR_NONE)
                            {
                                *pIpv4 = Ipv4;
                                bstr_t mask(pCurMask);
                                *pMaskv4 = inet_addr((char*)(mask));
                                break;
                            }
                        }
                    }
                }
                else
                {
                    *pIpv4 = 0;
                    *pMaskv4 = 0;
                }

                VariantClear(&vtMask);
            }
        }
        else
        {
            *pIpv4 = 0;
            *pMaskv4 = 0;
        }

        VariantClear(&vtIp);
    }

    return hr;
}


static HRESULT netIfWinHasIpSettings(IWbemClassObject * pAdapterConfig, SAFEARRAY * pCheckIp, SAFEARRAY * pCheckMask, bool *pFound)
{
    VARIANT vtIp;
    HRESULT hr;
    VariantInit(&vtIp);

    *pFound = false;

    hr = pAdapterConfig->Get(L"IPAddress", 0, &vtIp, 0, 0);
    if (SUCCEEDED(hr))
    {
        VARIANT vtMask;
        VariantInit(&vtMask);
        hr = pAdapterConfig->Get(L"IPSubnet", 0, &vtMask, 0, 0);
        if (SUCCEEDED(hr))
        {
            SAFEARRAY * pIpArray = vtIp.parray;
            SAFEARRAY * pMaskArray = vtMask.parray;
            if (pIpArray && pMaskArray)
            {
                BSTR pIp, pMask;
                for (LONG k = 0;
                    SafeArrayGetElement(pCheckIp, &k, (PVOID)&pIp) == S_OK
                    && SafeArrayGetElement(pCheckMask, &k, (PVOID)&pMask) == S_OK;
                    k++)
                {
                    BSTR pCurIp;
                    BSTR pCurMask;
                    for (LONG i = 0;
                        SafeArrayGetElement(pIpArray, &i, (PVOID)&pCurIp) == S_OK
                        && SafeArrayGetElement(pMaskArray, &i, (PVOID)&pCurMask) == S_OK;
                        i++)
                    {
                        if (!wcsicmp(pCurIp, pIp))
                        {
                            if (!wcsicmp(pCurMask, pMask))
                                *pFound = true;
                            break;
                        }
                    }
                }
            }


            VariantClear(&vtMask);
        }

        VariantClear(&vtIp);
    }

    return hr;
}

static HRESULT netIfWinWaitIpSettings(IWbemServices *pSvc, const GUID * pGuid, SAFEARRAY * pCheckIp, SAFEARRAY * pCheckMask, ULONG sec2Wait, bool *pFound)
{
    /* on Vista we need to wait for the address to get applied */
    /* wait for the address to appear in the list */
    HRESULT hr = S_OK;
    ULONG i;
    *pFound = false;
    ComPtr <IWbemClassObject> pAdapterConfig;
    for (i = 0;
            (hr = netIfWinFindAdapterClassById(pSvc, pGuid, pAdapterConfig.asOutParam())) == S_OK
         && (hr = netIfWinHasIpSettings(pAdapterConfig, pCheckIp, pCheckMask, pFound)) == S_OK
         && !(*pFound)
         && i < sec2Wait/6;
         i++)
    {
        Sleep(6000);
    }

    return hr;
}

static HRESULT netIfWinCreateIWbemServices(IWbemServices ** ppSvc)
{
    IWbemLocator *pLoc = NULL;
    HRESULT hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *) &pLoc);
    if (SUCCEEDED(hr))
    {
        IWbemServices *pSvc = NULL;
        hr = pLoc->ConnectServer(bstr_t(L"ROOT\\CIMV2"), /* [in] const BSTR strNetworkResource */
                NULL, /* [in] const BSTR strUser */
                NULL, /* [in] const BSTR strPassword */
                0,    /* [in] const BSTR strLocale */
                NULL, /* [in] LONG lSecurityFlags */
                0,    /* [in] const BSTR strAuthority */
                0,    /* [in] IWbemContext* pCtx */
                &pSvc /* [out] IWbemServices** ppNamespace */);
        if (SUCCEEDED(hr))
        {
            hr = CoSetProxyBlanket(pSvc, /* IUnknown * pProxy */
                    RPC_C_AUTHN_WINNT, /* DWORD dwAuthnSvc */
                    RPC_C_AUTHZ_NONE, /* DWORD dwAuthzSvc */
                    NULL, /* WCHAR * pServerPrincName */
                    RPC_C_AUTHN_LEVEL_CALL, /* DWORD dwAuthnLevel */
                    RPC_C_IMP_LEVEL_IMPERSONATE, /* DWORD dwImpLevel */
                    NULL, /* RPC_AUTH_IDENTITY_HANDLE pAuthInfo */
                    EOAC_NONE /* DWORD dwCapabilities */
                    );
            if (SUCCEEDED(hr))
            {
                *ppSvc = pSvc;
                /* do not need it any more */
                pLoc->Release();
                return hr;
            }
            else
                NonStandardLogFlow(("CoSetProxyBlanket failed, hr (0x%x)\n", hr));

            pSvc->Release();
        }
        else
            NonStandardLogFlow(("ConnectServer failed, hr (0x%x)\n", hr));
        pLoc->Release();
    }
    else
        NonStandardLogFlow(("CoCreateInstance failed, hr (0x%x)\n", hr));
    return hr;
}

static HRESULT netIfWinAdapterConfigPath(IWbemClassObject *pObj, BSTR * pStr)
{
    VARIANT index;
    HRESULT hr = pObj->Get(L"Index", 0, &index, 0, 0);
    if (SUCCEEDED(hr))
    {
        WCHAR strIndex[8];
        swprintf(strIndex, L"%u", index.uintVal);
        *pStr = (bstr_t(L"Win32_NetworkAdapterConfiguration.Index='") + strIndex + "'").copy();
    }
    else
        NonStandardLogFlow(("Get failed, hr (0x%x)\n", hr));
    return hr;
}

static HRESULT netIfExecMethod(IWbemServices * pSvc, IWbemClassObject *pClass, BSTR ObjPath,
        BSTR MethodName, LPWSTR *pArgNames, LPVARIANT *pArgs, UINT cArgs,
        IWbemClassObject** ppOutParams
        )
{
    HRESULT hr = S_OK;
    ComPtr<IWbemClassObject> pInParamsDefinition;
    ComPtr<IWbemClassObject> pClassInstance;

    if (cArgs)
    {
        hr = pClass->GetMethod(MethodName, 0, pInParamsDefinition.asOutParam(), NULL);
        if (SUCCEEDED(hr))
        {
            hr = pInParamsDefinition->SpawnInstance(0, pClassInstance.asOutParam());
            if (SUCCEEDED(hr))
            {
                for (UINT i = 0; i < cArgs; i++)
                {
                    hr = pClassInstance->Put(pArgNames[i], 0,
                        pArgs[i], 0);
                    if (FAILED(hr))
                        break;
                }
            }
        }
    }

    if (SUCCEEDED(hr))
    {
        IWbemClassObject* pOutParams = NULL;
        hr = pSvc->ExecMethod(ObjPath, MethodName, 0, NULL, pClassInstance, &pOutParams, NULL);
        if (SUCCEEDED(hr))
        {
            *ppOutParams = pOutParams;
        }
    }

    return hr;
}

static HRESULT netIfWinCreateIpArray(SAFEARRAY **ppArray, in_addr* aIp, UINT cIp)
{
    HRESULT hr;
    SAFEARRAY * pIpArray = SafeArrayCreateVector(VT_BSTR, 0, cIp);
    if (pIpArray)
    {
        for (UINT i = 0; i < cIp; i++)
        {
            char* addr = inet_ntoa(aIp[i]);
            BSTR val = bstr_t(addr).copy();
            long aIndex[1];
            aIndex[0] = i;
            hr = SafeArrayPutElement(pIpArray, aIndex, val);
            if (FAILED(hr))
            {
                SysFreeString(val);
                SafeArrayDestroy(pIpArray);
                break;
            }
        }

        if (SUCCEEDED(hr))
        {
            *ppArray = pIpArray;
        }
    }
    else
        hr = HRESULT_FROM_WIN32(GetLastError());

    return hr;
}

static HRESULT netIfWinCreateIpArrayV4V6(SAFEARRAY **ppArray, BSTR Ip)
{
    HRESULT hr;
    SAFEARRAY *pIpArray = SafeArrayCreateVector(VT_BSTR, 0, 1);
    if (pIpArray)
    {
        BSTR val = bstr_t(Ip, false).copy();
        long aIndex[1];
        aIndex[0] = 0;
        hr = SafeArrayPutElement(pIpArray, aIndex, val);
        if (FAILED(hr))
        {
            SysFreeString(val);
            SafeArrayDestroy(pIpArray);
        }

        if (SUCCEEDED(hr))
        {
            *ppArray = pIpArray;
        }
    }
    else
        hr = HRESULT_FROM_WIN32(GetLastError());

    return hr;
}


static HRESULT netIfWinCreateIpArrayVariantV4(VARIANT * pIpAddresses, in_addr* aIp, UINT cIp)
{
    HRESULT hr;
    VariantInit(pIpAddresses);
    pIpAddresses->vt = VT_ARRAY | VT_BSTR;
    SAFEARRAY *pIpArray;
    hr = netIfWinCreateIpArray(&pIpArray, aIp, cIp);
    if (SUCCEEDED(hr))
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
    if (SUCCEEDED(hr))
    {
        pIpAddresses->parray = pIpArray;
    }
    return hr;
}

static HRESULT netIfWinEnableStatic(IWbemServices * pSvc, const GUID * pGuid, BSTR ObjPath, VARIANT * pIp, VARIANT * pMask)
{
    ComPtr<IWbemClassObject> pClass;
    BSTR ClassName = SysAllocString(L"Win32_NetworkAdapterConfiguration");
    HRESULT hr;
    if (ClassName)
    {
        hr = pSvc->GetObject(ClassName, 0, NULL, pClass.asOutParam(), NULL);
        if (SUCCEEDED(hr))
        {
            LPWSTR argNames[] = {L"IPAddress", L"SubnetMask"};
            LPVARIANT args[] = {pIp, pMask};
            ComPtr<IWbemClassObject> pOutParams;

            hr = netIfExecMethod(pSvc, pClass, ObjPath, bstr_t(L"EnableStatic"), argNames, args, 2, pOutParams.asOutParam());
            if (SUCCEEDED(hr))
            {
                VARIANT varReturnValue;
                hr = pOutParams->Get(bstr_t(L"ReturnValue"), 0,
                    &varReturnValue, NULL, 0);
                Assert(SUCCEEDED(hr));
                if (SUCCEEDED(hr))
                {
//                    Assert(varReturnValue.vt == VT_UINT);
                    int winEr = varReturnValue.uintVal;
                    switch (winEr)
                    {
                    case 0:
                        {
                            hr = S_OK;
//                            bool bFound;
//                            HRESULT tmpHr = netIfWinWaitIpSettings(pSvc, pGuid, pIp->parray, pMask->parray, 180, &bFound);
                        }
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
        hr = HRESULT_FROM_WIN32(GetLastError());

    return hr;
}


static HRESULT netIfWinEnableStaticV4(IWbemServices * pSvc, const GUID * pGuid, BSTR ObjPath, in_addr* aIp, in_addr * aMask, UINT cIp)
{
    VARIANT ipAddresses;
    HRESULT hr = netIfWinCreateIpArrayVariantV4(&ipAddresses, aIp, cIp);
    if (SUCCEEDED(hr))
    {
        VARIANT ipMasks;
        hr = netIfWinCreateIpArrayVariantV4(&ipMasks, aMask, cIp);
        if (SUCCEEDED(hr))
        {
            hr = netIfWinEnableStatic(pSvc, pGuid, ObjPath, &ipAddresses, &ipMasks);
            VariantClear(&ipMasks);
        }
        VariantClear(&ipAddresses);
    }
    return hr;
}

static HRESULT netIfWinEnableStaticV4V6(IWbemServices * pSvc, const GUID * pGuid, BSTR ObjPath, BSTR Ip, BSTR Mask)
{
    VARIANT ipAddresses;
    HRESULT hr = netIfWinCreateIpArrayVariantV4V6(&ipAddresses, Ip);
    if (SUCCEEDED(hr))
    {
        VARIANT ipMasks;
        hr = netIfWinCreateIpArrayVariantV4V6(&ipMasks, Mask);
        if (SUCCEEDED(hr))
        {
            hr = netIfWinEnableStatic(pSvc, pGuid, ObjPath, &ipAddresses, &ipMasks);
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
    if (ClassName)
    {
        hr = pSvc->GetObject(ClassName, 0, NULL, pClass.asOutParam(), NULL);
        if (SUCCEEDED(hr))
        {
            LPWSTR argNames[] = {L"DefaultIPGateway"};
            LPVARIANT args[] = {pGw};
            ComPtr<IWbemClassObject> pOutParams;

            hr = netIfExecMethod(pSvc, pClass, ObjPath, bstr_t(L"SetGateways"), argNames, args, 1, pOutParams.asOutParam());
            if (SUCCEEDED(hr))
            {
                VARIANT varReturnValue;
                hr = pOutParams->Get(bstr_t(L"ReturnValue"), 0, &varReturnValue, NULL, 0);
                Assert(SUCCEEDED(hr));
                if (SUCCEEDED(hr))
                {
//                    Assert(varReturnValue.vt == VT_UINT);
                    int winEr = varReturnValue.uintVal;
                    switch (winEr)
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
        hr = HRESULT_FROM_WIN32(GetLastError());

    return hr;
}

/* win API allows to set gw metrics as well, we are not setting them */
static HRESULT netIfWinSetGatewaysV4(IWbemServices * pSvc, BSTR ObjPath, in_addr* aGw, UINT cGw)
{
    VARIANT gwais;
    HRESULT hr = netIfWinCreateIpArrayVariantV4(&gwais, aGw, cGw);
    if (SUCCEEDED(hr))
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
    if (SUCCEEDED(hr))
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
    if (ClassName)
    {
        hr = pSvc->GetObject(ClassName, 0, NULL, pClass.asOutParam(), NULL);
        if (SUCCEEDED(hr))
        {
            ComPtr<IWbemClassObject> pOutParams;

            hr = netIfExecMethod(pSvc, pClass, ObjPath, bstr_t(L"EnableDHCP"), NULL, NULL, 0, pOutParams.asOutParam());
            if (SUCCEEDED(hr))
            {
                VARIANT varReturnValue;
                hr = pOutParams->Get(bstr_t(L"ReturnValue"), 0,
                    &varReturnValue, NULL, 0);
                Assert(SUCCEEDED(hr));
                if (SUCCEEDED(hr))
                {
//                    Assert(varReturnValue.vt == VT_UINT);
                    int winEr = varReturnValue.uintVal;
                    switch (winEr)
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
        hr = HRESULT_FROM_WIN32(GetLastError());

    return hr;
}

static HRESULT netIfWinDhcpRediscover(IWbemServices * pSvc, BSTR ObjPath)
{
    ComPtr<IWbemClassObject> pClass;
    BSTR ClassName = SysAllocString(L"Win32_NetworkAdapterConfiguration");
    HRESULT hr;
    if (ClassName)
    {
        hr = pSvc->GetObject(ClassName, 0, NULL, pClass.asOutParam(), NULL);
        if (SUCCEEDED(hr))
        {
            ComPtr<IWbemClassObject> pOutParams;

            hr = netIfExecMethod(pSvc, pClass, ObjPath, bstr_t(L"ReleaseDHCPLease"), NULL, NULL, 0, pOutParams.asOutParam());
            if (SUCCEEDED(hr))
            {
                VARIANT varReturnValue;
                hr = pOutParams->Get(bstr_t(L"ReturnValue"), 0, &varReturnValue, NULL, 0);
                Assert(SUCCEEDED(hr));
                if (SUCCEEDED(hr))
                {
//                    Assert(varReturnValue.vt == VT_UINT);
                    int winEr = varReturnValue.uintVal;
                    if (winEr == 0)
                    {
                        hr = netIfExecMethod(pSvc, pClass, ObjPath, bstr_t(L"RenewDHCPLease"), NULL, NULL, 0, pOutParams.asOutParam());
                        if (SUCCEEDED(hr))
                        {
                            VARIANT varReturnValue;
                            hr = pOutParams->Get(bstr_t(L"ReturnValue"), 0, &varReturnValue, NULL, 0);
                            Assert(SUCCEEDED(hr));
                            if (SUCCEEDED(hr))
                            {
            //                    Assert(varReturnValue.vt == VT_UINT);
                                int winEr = varReturnValue.uintVal;
                                if (winEr == 0)
                                    hr = S_OK;
                                else
                                    hr = HRESULT_FROM_WIN32( winEr );
                            }
                        }
                    }
                    else
                        hr = HRESULT_FROM_WIN32( winEr );
                }
            }
        }
        SysFreeString(ClassName);
    }
    else
        hr = HRESULT_FROM_WIN32(GetLastError());

    return hr;
}

static HRESULT vboxNetCfgWinIsDhcpEnabled(IWbemClassObject * pAdapterConfig, BOOL *pEnabled)
{
    VARIANT vtEnabled;
    HRESULT hr = pAdapterConfig->Get(L"DHCPEnabled", 0, &vtEnabled, 0, 0);
    if (SUCCEEDED(hr))
        *pEnabled = vtEnabled.boolVal;
    return hr;
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGetAdapterSettings(IN const GUID * pGuid, OUT PADAPTER_SETTINGS pSettings)
{
    HRESULT hr;
    ComPtr <IWbemServices> pSvc;
    hr = netIfWinCreateIWbemServices(pSvc.asOutParam());
    if (SUCCEEDED(hr))
    {
        ComPtr<IWbemClassObject> pAdapterConfig;
        hr = netIfWinFindAdapterClassById(pSvc, pGuid, pAdapterConfig.asOutParam());
        if (SUCCEEDED(hr))
        {
            hr = vboxNetCfgWinIsDhcpEnabled(pAdapterConfig, &pSettings->bDhcp);
            if (SUCCEEDED(hr))
                hr = netIfWinGetIpSettings(pAdapterConfig, &pSettings->ip, &pSettings->mask);
        }
    }

    return hr;
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinIsDhcpEnabled(const GUID * pGuid, BOOL *pEnabled)
{
    HRESULT hr;
    ComPtr <IWbemServices> pSvc;
    hr = netIfWinCreateIWbemServices(pSvc.asOutParam());
    if (SUCCEEDED(hr))
    {
        ComPtr<IWbemClassObject> pAdapterConfig;
        hr = netIfWinFindAdapterClassById(pSvc, pGuid, pAdapterConfig.asOutParam());
        if (SUCCEEDED(hr))
        {
            VARIANT vtEnabled;
            hr = pAdapterConfig->Get(L"DHCPEnabled", 0, &vtEnabled, 0, 0);
            if (SUCCEEDED(hr))
                *pEnabled = vtEnabled.boolVal;
        }
    }

    return hr;
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinEnableStaticIpConfig(IN const GUID *pGuid, IN ULONG ip, IN ULONG mask)
{
    NonStandardLogFlow(("VBoxNetCfgWinEnableStaticIpConfig: ip=0x%x mask=0x%x", ip, mask));
    ComPtr<IWbemServices> pSvc;
    HRESULT hr = netIfWinCreateIWbemServices(pSvc.asOutParam());
    if (SUCCEEDED(hr))
    {
        ComPtr<IWbemClassObject> pAdapterConfig;
        hr = netIfWinFindAdapterClassById(pSvc, pGuid, pAdapterConfig.asOutParam());
        if (SUCCEEDED(hr))
        {
            BOOL bIsHostOnly;
            hr = netIfWinIsHostOnly(pAdapterConfig, &bIsHostOnly);
            if (SUCCEEDED(hr))
            {
                if (bIsHostOnly)
                {
                    in_addr aIp[1];
                    in_addr aMask[1];
                    aIp[0].S_un.S_addr = ip;
                    aMask[0].S_un.S_addr = mask;

                    BSTR ObjPath;
                    hr = netIfWinAdapterConfigPath(pAdapterConfig, &ObjPath);
                    if (SUCCEEDED(hr))
                    {
                        hr = netIfWinEnableStaticV4(pSvc, pGuid, ObjPath, aIp, aMask, ip != 0 ? 1 : 0);
                        if (SUCCEEDED(hr))
                        {
#if 0
                            in_addr aGw[1];
                            aGw[0].S_un.S_addr = gw;
                            hr = netIfWinSetGatewaysV4(pSvc, ObjPath, aGw, 1);
                            if (SUCCEEDED(hr))
#endif
                            {
                            }
                        }
                        SysFreeString(ObjPath);
                    }
                }
                else
                {
                    hr = E_FAIL;
                }
            }
        }
    }

    NonStandardLogFlow(("VBoxNetCfgWinEnableStaticIpConfig: returns 0x%x", hr));
    return hr;
}

#if 0
static HRESULT netIfEnableStaticIpConfigV6(const GUID *pGuid, IN_BSTR aIPV6Address, IN_BSTR aIPV6Mask, IN_BSTR aIPV6DefaultGateway)
{
    HRESULT hr;
        ComPtr <IWbemServices> pSvc;
        hr = netIfWinCreateIWbemServices(pSvc.asOutParam());
        if (SUCCEEDED(hr))
        {
            ComPtr<IWbemClassObject> pAdapterConfig;
            hr = netIfWinFindAdapterClassById(pSvc, pGuid, pAdapterConfig.asOutParam());
            if (SUCCEEDED(hr))
            {
                BSTR ObjPath;
                hr = netIfWinAdapterConfigPath(pAdapterConfig, &ObjPath);
                if (SUCCEEDED(hr))
                {
                    hr = netIfWinEnableStaticV4V6(pSvc, pAdapterConfig, ObjPath, aIPV6Address, aIPV6Mask);
                    if (SUCCEEDED(hr))
                    {
                        if (aIPV6DefaultGateway)
                        {
                            hr = netIfWinSetGatewaysV4V6(pSvc, ObjPath, aIPV6DefaultGateway);
                        }
                        if (SUCCEEDED(hr))
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

static HRESULT netIfEnableStaticIpConfigV6(const GUID *pGuid, IN_BSTR aIPV6Address, ULONG aIPV6MaskPrefixLength)
{
    RTNETADDRIPV6 Mask;
    int rc = prefixLength2IPv6Address(aIPV6MaskPrefixLength, &Mask);
    if (RT_SUCCESS(rc))
    {
        Bstr maskStr = composeIPv6Address(&Mask);
        rc = netIfEnableStaticIpConfigV6(pGuid, aIPV6Address, maskStr, NULL);
    }
    return rc;
}
#endif

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinEnableDynamicIpConfig(IN const GUID *pGuid)
{
    HRESULT hr;
        ComPtr <IWbemServices> pSvc;
        hr = netIfWinCreateIWbemServices(pSvc.asOutParam());
        if (SUCCEEDED(hr))
        {
            ComPtr<IWbemClassObject> pAdapterConfig;
            hr = netIfWinFindAdapterClassById(pSvc, pGuid, pAdapterConfig.asOutParam());
            if (SUCCEEDED(hr))
            {
                BOOL bIsHostOnly;
                hr = netIfWinIsHostOnly(pAdapterConfig, &bIsHostOnly);
                if (SUCCEEDED(hr))
                {
                    if (bIsHostOnly)
                    {
                        BSTR ObjPath;
                        hr = netIfWinAdapterConfigPath(pAdapterConfig, &ObjPath);
                        if (SUCCEEDED(hr))
                        {
                            hr = netIfWinEnableDHCP(pSvc, ObjPath);
                            if (SUCCEEDED(hr))
                            {
//                              hr = netIfWinUpdateConfig(pIf);
                            }
                            SysFreeString(ObjPath);
                        }
                    }
                    else
                    {
                        hr = E_FAIL;
                    }
                }
            }
        }


    return hr;
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinDhcpRediscover(IN const GUID *pGuid)
{
    HRESULT hr;
    ComPtr <IWbemServices> pSvc;
    hr = netIfWinCreateIWbemServices(pSvc.asOutParam());
    if (SUCCEEDED(hr))
    {
        ComPtr<IWbemClassObject> pAdapterConfig;
        hr = netIfWinFindAdapterClassById(pSvc, pGuid, pAdapterConfig.asOutParam());
        if (SUCCEEDED(hr))
        {
            BOOL bIsHostOnly;
            hr = netIfWinIsHostOnly(pAdapterConfig, &bIsHostOnly);
            if (SUCCEEDED(hr))
            {
                if (bIsHostOnly)
                {
                    BSTR ObjPath;
                    hr = netIfWinAdapterConfigPath(pAdapterConfig, &ObjPath);
                    if (SUCCEEDED(hr))
                    {
                        hr = netIfWinDhcpRediscover(pSvc, ObjPath);
                        if (SUCCEEDED(hr))
                        {
                            //hr = netIfWinUpdateConfig(pIf);
                        }
                        SysFreeString(ObjPath);
                    }
                }
                else
                {
                    hr = E_FAIL;
                }
            }
        }
    }


    return hr;
}

typedef bool (*PFNVBOXNETCFG_IPSETTINGS_CALLBACK) (ULONG ip, ULONG mask, PVOID pContext);

static void vboxNetCfgWinEnumIpConfig(PIP_ADAPTER_ADDRESSES pAddresses, PFNVBOXNETCFG_IPSETTINGS_CALLBACK pfnCallback, PVOID pContext)
{
    PIP_ADAPTER_ADDRESSES pAdapter;
    for (pAdapter = pAddresses; pAdapter; pAdapter = pAdapter->Next)
    {
        PIP_ADAPTER_UNICAST_ADDRESS pAddr = pAdapter->FirstUnicastAddress;
        PIP_ADAPTER_PREFIX pPrefix = pAdapter->FirstPrefix;

        if (pAddr && pPrefix)
        {
            do
            {
                bool fIPFound, fMaskFound;
                fIPFound = fMaskFound = false;
                ULONG ip, mask;
                for (; pAddr && !fIPFound; pAddr = pAddr->Next)
                {
                    switch (pAddr->Address.lpSockaddr->sa_family)
                    {
                        case AF_INET:
                            fIPFound = true;
                            memcpy(&ip,
                                    &((struct sockaddr_in *)pAddr->Address.lpSockaddr)->sin_addr.s_addr,
                                    sizeof(ip));
                            break;
//                            case AF_INET6:
//                                break;
                    }
                }

                for (; pPrefix && !fMaskFound; pPrefix = pPrefix->Next)
                {
                    switch (pPrefix->Address.lpSockaddr->sa_family)
                    {
                        case AF_INET:
                            if (!pPrefix->PrefixLength || pPrefix->PrefixLength > 31) /* in case the ip helper API is queried while NetCfg write lock is held */
                                break;                                               /* the address values can contain illegal values */
                            fMaskFound = true;
                            mask = (~(((ULONG)~0) >> pPrefix->PrefixLength));
                            mask = htonl(mask);
                            break;
//                            case AF_INET6:
//                                break;
                    }
                }

                if (!fIPFound || !fMaskFound)
                    break;

                if (!pfnCallback(ip, mask, pContext))
                    return;
            } while (true);
        }
    }
}

typedef struct _IPPROBE_CONTEXT
{
    ULONG Prefix;
    bool bConflict;
}IPPROBE_CONTEXT, *PIPPROBE_CONTEXT;

#define IPPROBE_INIT(_pContext, _addr) \
    ((_pContext)->bConflict = false, \
     (_pContext)->Prefix = _addr)

#define IPPROBE_INIT_STR(_pContext, _straddr) \
    IPROBE_INIT(_pContext, inet_addr(_straddr))

static bool vboxNetCfgWinIpProbeCallback (ULONG ip, ULONG mask, PVOID pContext)
{
    PIPPROBE_CONTEXT pProbe = (PIPPROBE_CONTEXT)pContext;

    if ((ip & mask) == (pProbe->Prefix & mask))
    {
        pProbe->bConflict = true;
        return false;
    }

    return true;
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGenHostOnlyNetworkNetworkIp(OUT PULONG pNetIp, OUT PULONG pNetMask)
{
    DWORD dwRc;
    HRESULT hr = S_OK;
    /*
     * Most of the hosts probably have less than 10 adapters,
     * so we'll mostly succeed from the first attempt.
     */
    ULONG uBufLen = sizeof(IP_ADAPTER_ADDRESSES) * 10;
    PIP_ADAPTER_ADDRESSES pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(uBufLen);
    if (!pAddresses)
        return HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY);
    dwRc = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &uBufLen);
    if (dwRc == ERROR_BUFFER_OVERFLOW)
    {
        /* Impressive! More than 10 adapters! Get more memory and try again. */
        free(pAddresses);
        pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(uBufLen);
        if (!pAddresses)
            return HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY);
        dwRc = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &uBufLen);
    }
    if (dwRc == NO_ERROR)
    {
        IPPROBE_CONTEXT Context;
        const ULONG ip192168 = inet_addr("192.168.0.0");
        srand(GetTickCount());

        *pNetIp = 0;
        *pNetMask = 0;

        for (int i = 0; i < 255; i++)
        {
            ULONG ipProbe = rand()*255/RAND_MAX;
            ipProbe = ip192168 | (ipProbe << 16);
            IPPROBE_INIT(&Context, ipProbe);
            vboxNetCfgWinEnumIpConfig(pAddresses, vboxNetCfgWinIpProbeCallback, &Context);
            if (!Context.bConflict)
            {
                *pNetIp = ipProbe;
                *pNetMask = inet_addr("255.255.255.0");
                break;
            }
        }
        if (*pNetIp == 0)
            dwRc = ERROR_DHCP_ADDRESS_CONFLICT;
    }
    else
        NonStandardLogFlow(("GetAdaptersAddresses err (%d)\n", dwRc));

    if (pAddresses)
        free(pAddresses);

    if (dwRc != NO_ERROR)
    {
        hr = HRESULT_FROM_WIN32(dwRc);
    }

    return hr;
}

/*
 * convenience functions to perform netflt/adp manipulations
 */
#define VBOXNETCFGWIN_NETFLT_ID    L"sun_VBoxNetFlt"
#define VBOXNETCFGWIN_NETFLT_MP_ID L"sun_VBoxNetFltmp"

static HRESULT vboxNetCfgWinNetFltUninstall(IN INetCfg *pNc, DWORD InfRmFlags)
{
    INetCfgComponent * pNcc = NULL;
    HRESULT hr = pNc->FindComponent(VBOXNETCFGWIN_NETFLT_ID, &pNcc);
    if (hr == S_OK)
    {
        NonStandardLog("NetFlt is installed currently, uninstalling ...\n");

        hr = VBoxNetCfgWinUninstallComponent(pNc, pNcc);

        pNcc->Release();
    }
    else if (hr == S_FALSE)
    {
        NonStandardLog("NetFlt is not installed currently\n");
        hr = S_OK;
    }
    else
    {
        NonStandardLogFlow(("FindComponent failed, hr (0x%x)\n", hr));
        hr = S_OK;
    }

    VBoxDrvCfgInfUninstallAllF(L"NetService", VBOXNETCFGWIN_NETFLT_ID, InfRmFlags);
    VBoxDrvCfgInfUninstallAllF(L"Net", VBOXNETCFGWIN_NETFLT_MP_ID, InfRmFlags);

    return hr;
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinNetFltUninstall(IN INetCfg *pNc)
{
    return vboxNetCfgWinNetFltUninstall(pNc, 0);
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinNetFltInstall(IN INetCfg *pNc,
                                                       IN LPCWSTR const *apInfFullPaths, IN UINT cInfFullPaths)
{
    HRESULT hr = vboxNetCfgWinNetFltUninstall(pNc, SUOI_FORCEDELETE);
    if (SUCCEEDED(hr))
    {
        NonStandardLog("NetFlt will be installed ...\n");
        hr = VBoxNetCfgWinInstallInfAndComponent(pNc, VBOXNETCFGWIN_NETFLT_ID,
                                                 &GUID_DEVCLASS_NETSERVICE,
                                                 apInfFullPaths,
                                                 cInfFullPaths,
                                                 NULL);
    }
    return hr;
}

#define VBOX_CONNECTION_NAME L"VirtualBox Host-Only Network"
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGenHostonlyConnectionName(PCWSTR DevName, WCHAR *pBuf, PULONG pcbBuf)
{
    const WCHAR * pSuffix = wcsrchr( DevName, L'#' );
    ULONG cbSize = sizeof(VBOX_CONNECTION_NAME);
    ULONG cbSufSize = 0;

    if (pSuffix)
    {
        cbSize += (ULONG)wcslen(pSuffix) * 2;
        cbSize += 2; /* for space */
    }

    if (*pcbBuf < cbSize)
    {
        *pcbBuf = cbSize;
        return E_FAIL;
    }

    wcscpy(pBuf, VBOX_CONNECTION_NAME);
    if (pSuffix)
    {
        wcscat(pBuf, L" ");
        wcscat(pBuf, pSuffix);
    }

    return S_OK;
}

static BOOL vboxNetCfgWinAdjustHostOnlyNetworkInterfacePriority(IN INetCfg *pNc, IN INetCfgComponent *pNcc, PVOID pContext)
{
    INetCfgComponentBindings *pNetCfgBindings;
    GUID *pGuid = (GUID*)pContext;

    /* Get component's binding. */
    HRESULT hr = pNcc->QueryInterface(IID_INetCfgComponentBindings, (PVOID*)&pNetCfgBindings);
    if (SUCCEEDED(hr))
    {
        /* Get binding path enumerator reference. */
        IEnumNetCfgBindingPath *pEnumNetCfgBindPath;
        hr = pNetCfgBindings->EnumBindingPaths(EBP_BELOW, &pEnumNetCfgBindPath);
        if (SUCCEEDED(hr))
        {
            bool bFoundIface = false;
            hr = pEnumNetCfgBindPath->Reset();
            do
            {
                INetCfgBindingPath *pNetCfgBindPath;
                hr = pEnumNetCfgBindPath->Next(1, &pNetCfgBindPath, NULL);
                if (hr == S_OK)
                {
                    IEnumNetCfgBindingInterface *pEnumNetCfgBindIface;
                    hr = pNetCfgBindPath->EnumBindingInterfaces(&pEnumNetCfgBindIface);
                    if (hr == S_OK)
                    {
                        pEnumNetCfgBindIface->Reset();
                        do
                        {
                            INetCfgBindingInterface *pNetCfgBindIfce;
                            hr = pEnumNetCfgBindIface->Next(1, &pNetCfgBindIfce, NULL);
                            if (hr == S_OK)
                            {
                                INetCfgComponent *pNetCfgCompo;
                                hr = pNetCfgBindIfce->GetLowerComponent(&pNetCfgCompo);
                                if (hr == S_OK)
                                {
                                    ULONG uComponentStatus;
                                    hr = pNetCfgCompo->GetDeviceStatus(&uComponentStatus);
                                    if (hr == S_OK)
                                    {
                                        GUID guid;
                                        hr = pNetCfgCompo->GetInstanceGuid(&guid);
                                        if (   hr == S_OK
                                            && guid == *pGuid)
                                        {
                                            hr = pNetCfgBindings->MoveAfter(pNetCfgBindPath, NULL);
                                            if (FAILED(hr))
                                                NonStandardLogFlow(("Unable to move interface, hr (0x%x)\n", hr));
                                            bFoundIface = true;
                                        }
                                    }
                                    pNetCfgCompo->Release();
                                }
                                else
                                    NonStandardLogFlow(("GetLowerComponent failed, hr (0x%x)\n", hr));
                                pNetCfgBindIfce->Release();
                            }
                            else
                            {
                                if (hr == S_FALSE) /* No more binding interfaces? */
                                    hr = S_OK;
                                else
                                    NonStandardLogFlow(("Next binding interface failed, hr (0x%x)\n", hr));
                                break;
                            }
                        } while (!bFoundIface);
                        pEnumNetCfgBindIface->Release();
                    }
                    else
                        NonStandardLogFlow(("EnumBindingInterfaces failed, hr (0x%x)\n", hr));
                    pNetCfgBindPath->Release();
                }
                else
                {
                    if (hr = S_FALSE) /* No more binding paths? */
                        hr = S_OK;
                    else
                        NonStandardLogFlow(("Next bind path failed, hr (0x%x)\n", hr));
                    break;
                }
            } while (!bFoundIface);
            pEnumNetCfgBindPath->Release();
        }
        else
            NonStandardLogFlow(("EnumBindingPaths failed, hr (0x%x)\n", hr));
        pNetCfgBindings->Release();
    }
    else
        NonStandardLogFlow(("QueryInterface for IID_INetCfgComponentBindings failed, hr (0x%x)\n", hr));
    return TRUE;
}

static UINT WINAPI vboxNetCfgWinPspFileCallback(
        PVOID Context,
        UINT Notification,
        UINT_PTR Param1,
        UINT_PTR Param2
    )
{
    switch (Notification)
    {
        case SPFILENOTIFY_TARGETNEWER:
        case SPFILENOTIFY_TARGETEXISTS:
            return TRUE;
    }
    return SetupDefaultQueueCallback(Context, Notification, Param1, Param2);
}

/* The original source of the VBoxNetAdp adapter creation/destruction code has the following copyright */
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
    if (wcslen(wGuid) >= MAX_PATH)
        return E_INVALIDARG;
    WCHAR szAdapterGuid[MAX_PATH + 2] = {0};
    swprintf(szAdapterGuid, L"::%ls", wGuid);

    /* Create an instance of the network connections folder. */
    hr = CoCreateInstance(CLSID_NetworkConnections, NULL,
                          CLSCTX_INPROC_SERVER, IID_IShellFolder,
                          reinterpret_cast<LPVOID *>(&pShellFolder));
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

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinRenameConnection (LPWSTR pGuid, PCWSTR NewName)
{
    typedef HRESULT (WINAPI *lpHrRenameConnection) (const GUID *, PCWSTR);
    lpHrRenameConnection RenameConnectionFunc = NULL;
    HRESULT status;

    /* First try the IShellFolder interface, which was unimplemented
     * for the network connections folder before XP. */
    status = rename_shellfolder (pGuid, NewName);
    if (status == E_NOTIMPL)
    {
/** @todo that code doesn't seem to work! */
        /* The IShellFolder interface is not implemented on this platform.
         * Try the (undocumented) HrRenameConnection API in the netshell
         * library. */
        CLSID clsid;
        HINSTANCE hNetShell;
        status = CLSIDFromString ((LPOLESTR) pGuid, &clsid);
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
        hrc = E_FAIL; \
        NonStandardLog strAndArgs; \
        break; \
    } else do {} while (0)

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinRemoveHostOnlyNetworkInterface(IN const GUID *pGUID, OUT BSTR *pErrMsg)
{
    HRESULT hrc = S_OK;

    do
    {
        TCHAR lszPnPInstanceId [512] = {0};

        /* We have to find the device instance ID through a registry search */

        HKEY hkeyNetwork = 0;
        HKEY hkeyConnection = 0;

        do
        {
            WCHAR strRegLocation [256];
            WCHAR wszGuid[50];

            int length = StringFromGUID2(*pGUID, wszGuid, RT_ELEMENTS(wszGuid));
            if (!length)
                SetErrBreak(("Failed to create a Guid string"));

            swprintf (strRegLocation,
                     L"SYSTEM\\CurrentControlSet\\Control\\Network\\"
                     L"{4D36E972-E325-11CE-BFC1-08002BE10318}\\%s",
                     wszGuid);

            LONG status;
            status = RegOpenKeyExW (HKEY_LOCAL_MACHINE, strRegLocation, 0,
                                    KEY_READ, &hkeyNetwork);
            if ((status != ERROR_SUCCESS) || !hkeyNetwork)
                SetErrBreak (("Host interface network is not found in registry (%S) [1]",
                    strRegLocation));

            status = RegOpenKeyExW (hkeyNetwork, L"Connection", 0,
                                    KEY_READ, &hkeyConnection);
            if ((status != ERROR_SUCCESS) || !hkeyConnection)
                SetErrBreak (("Host interface network is not found in registry (%S) [2]",
                    strRegLocation));

            DWORD len = sizeof (lszPnPInstanceId);
            DWORD dwKeyType;
            status = RegQueryValueExW (hkeyConnection, L"PnPInstanceID", NULL,
                                       &dwKeyType, (LPBYTE) lszPnPInstanceId, &len);
            if ((status != ERROR_SUCCESS) || (dwKeyType != REG_SZ))
                SetErrBreak (("Host interface network is not found in registry (%S) [3]",
                    strRegLocation));
        }
        while (0);

        if (hkeyConnection)
            RegCloseKey (hkeyConnection);
        if (hkeyNetwork)
            RegCloseKey (hkeyNetwork);

        if (FAILED (hrc))
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

        if (FAILED (hrc))
            break;
    }
    while (0);

    return hrc;
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinUpdateHostOnlyNetworkInterface(LPCWSTR pcsxwInf, BOOL *pbRebootRequired)
{
    return VBoxDrvCfgDrvUpdate(DRIVERHWID, pcsxwInf, pbRebootRequired);
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinCreateHostOnlyNetworkInterface(IN LPCWSTR pInfPath, IN bool bIsInfPathFile,
                                                                        OUT GUID *pGuid, OUT BSTR *lppszName, OUT BSTR *pErrMsg)
{
    HRESULT hrc = S_OK;

    HDEVINFO hDeviceInfo = INVALID_HANDLE_VALUE;
    SP_DEVINFO_DATA DeviceInfoData;
    PVOID pQueueCallbackContext = NULL;
    DWORD ret = 0;
    BOOL found = FALSE;
    BOOL registered = FALSE;
    BOOL destroyList = FALSE;
    WCHAR pWCfgGuidString [50];
    WCHAR DevName[256];

    do
    {
        GUID netGuid;
        SP_DRVINFO_DATA DriverInfoData;
        SP_DEVINSTALL_PARAMS DeviceInstallParams;
        TCHAR className [MAX_PATH];
        DWORD index = 0;
        PSP_DRVINFO_DETAIL_DATA pDriverInfoDetail;
        /* for our purposes, 2k buffer is more
         * than enough to obtain the hardware ID
         * of the VBoxNetAdp driver. */
        DWORD detailBuf [2048];

        HKEY hkey = NULL;
        DWORD cbSize;
        DWORD dwValueType;

        /* initialize the structure size */
        DeviceInfoData.cbSize = sizeof (SP_DEVINFO_DATA);
        DriverInfoData.cbSize = sizeof (SP_DRVINFO_DATA);

        /* copy the net class GUID */
        memcpy(&netGuid, &GUID_DEVCLASS_NET, sizeof(GUID_DEVCLASS_NET));

        /* create an empty device info set associated with the net class GUID */
        hDeviceInfo = SetupDiCreateDeviceInfoList(&netGuid, NULL);
        if (hDeviceInfo == INVALID_HANDLE_VALUE)
            SetErrBreak (("SetupDiCreateDeviceInfoList failed (0x%08X)",
                          GetLastError()));

        /* get the class name from GUID */
        BOOL fResult = SetupDiClassNameFromGuid (&netGuid, className, MAX_PATH, NULL);
        if (!fResult)
            SetErrBreak (("SetupDiClassNameFromGuid failed (0x%08X)",
                          GetLastError()));

        /* create a device info element and add the new device instance
         * key to registry */
        fResult = SetupDiCreateDeviceInfo (hDeviceInfo, className, &netGuid, NULL, NULL,
                                     DICD_GENERATE_ID, &DeviceInfoData);
        if (!fResult)
            SetErrBreak (("SetupDiCreateDeviceInfo failed (0x%08X)",
                          GetLastError()));

        /* select the newly created device info to be the currently
           selected member */
        fResult = SetupDiSetSelectedDevice (hDeviceInfo, &DeviceInfoData);
        if (!fResult)
            SetErrBreak (("SetupDiSetSelectedDevice failed (0x%08X)",
                          GetLastError()));

        if (pInfPath)
        {
            /* get the device install parameters and disable filecopy */
            DeviceInstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS);
            fResult = SetupDiGetDeviceInstallParams (hDeviceInfo, &DeviceInfoData,
                                                &DeviceInstallParams);
            if (fResult)
            {
                memset(DeviceInstallParams.DriverPath, 0, sizeof(DeviceInstallParams.DriverPath));
                size_t pathLenght = wcslen(pInfPath) + 1/* null terminator */;
                if (pathLenght < sizeof(DeviceInstallParams.DriverPath)/sizeof(DeviceInstallParams.DriverPath[0]))
                {
                    memcpy(DeviceInstallParams.DriverPath, pInfPath, pathLenght*sizeof(DeviceInstallParams.DriverPath[0]));

                    if (bIsInfPathFile)
                    {
                        DeviceInstallParams.Flags |= DI_ENUMSINGLEINF;
                    }

                    fResult = SetupDiSetDeviceInstallParams(hDeviceInfo, &DeviceInfoData,
                                                       &DeviceInstallParams);
                    if (!fResult)
                    {
                        DWORD winEr = GetLastError();
                        NonStandardLogFlow(("SetupDiSetDeviceInstallParams failed, winEr (%d)\n", winEr));
                        break;
                    }
                }
                else
                {
                    NonStandardLogFlow(("SetupDiSetDeviceInstallParams faileed: INF path is too long\n"));
                    break;
                }
            }
            else
            {
                DWORD winEr = GetLastError();
                NonStandardLogFlow(("SetupDiGetDeviceInstallParams failed, winEr (%d)\n", winEr));
            }
        }

        /* build a list of class drivers */
        fResult = SetupDiBuildDriverInfoList (hDeviceInfo, &DeviceInfoData,
                                              SPDIT_CLASSDRIVER);
        if (!fResult)
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
             * list.  Otherwise there was something wrong with this
             * particular driver. */
            if (!ret)
            {
                if (GetLastError() == ERROR_NO_MORE_ITEMS)
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
                while (t && *t && t < (TCHAR *) &detailBuf [RT_ELEMENTS(detailBuf)])
                {
                    if (!_tcsicmp(t, DRIVERHWID))
                        break;

                    t += _tcslen(t) + 1;
                }

                if (t && *t && t < (TCHAR *) &detailBuf [RT_ELEMENTS(detailBuf)])
                {
                    found = TRUE;
                    break;
                }
            }

            index ++;
        }

        if (!found)
            SetErrBreak(("Could not find Host Interface Networking driver! Please reinstall"));

        /* set the loopback driver to be the currently selected */
        fResult = SetupDiSetSelectedDriver (hDeviceInfo, &DeviceInfoData,
                                       &DriverInfoData);
        if (!fResult)
            SetErrBreak(("SetupDiSetSelectedDriver failed (0x%08X)",
                         GetLastError()));

        /* register the phantom device to prepare for install */
        fResult = SetupDiCallClassInstaller (DIF_REGISTERDEVICE, hDeviceInfo,
                                             &DeviceInfoData);
        if (!fResult)
        {
            DWORD err = GetLastError();
            SetErrBreak (("SetupDiCallClassInstaller failed (0x%08X)",
                          err));
        }

        /* registered, but remove if errors occur in the following code */
        registered = TRUE;

        /* ask the installer if we can install the device */
        fResult = SetupDiCallClassInstaller (DIF_ALLOW_INSTALL, hDeviceInfo,
                                             &DeviceInfoData);
        if (!fResult)
        {
            if (GetLastError() != ERROR_DI_DO_DEFAULT)
                SetErrBreak (("SetupDiCallClassInstaller (DIF_ALLOW_INSTALL) failed (0x%08X)",
                              GetLastError()));
            /* that's fine */
        }

        /* get the device install parameters and disable filecopy */
        DeviceInstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS);
        fResult = SetupDiGetDeviceInstallParams (hDeviceInfo, &DeviceInfoData,
                                                 &DeviceInstallParams);
        if (fResult)
        {
            pQueueCallbackContext = SetupInitDefaultQueueCallback(NULL);
            if (pQueueCallbackContext)
            {
                DeviceInstallParams.InstallMsgHandlerContext = pQueueCallbackContext;
                DeviceInstallParams.InstallMsgHandler = (PSP_FILE_CALLBACK)vboxNetCfgWinPspFileCallback;
                fResult = SetupDiSetDeviceInstallParams (hDeviceInfo, &DeviceInfoData,
                                                    &DeviceInstallParams);
                if (!fResult)
                {
                    DWORD winEr = GetLastError();
                    NonStandardLogFlow(("SetupDiSetDeviceInstallParams failed, winEr (%d)\n", winEr));
                }
                Assert(fResult);
            }
            else
            {
                DWORD winEr = GetLastError();
                NonStandardLogFlow(("SetupInitDefaultQueueCallback failed, winEr (%d)\n", winEr));
            }
        }
        else
        {
            DWORD winEr = GetLastError();
            NonStandardLogFlow(("SetupDiGetDeviceInstallParams failed, winEr (%d)\n", winEr));
        }

        /* install the files first */
        fResult = SetupDiCallClassInstaller (DIF_INSTALLDEVICEFILES, hDeviceInfo,
                                        &DeviceInfoData);
        if (!fResult)
            SetErrBreak (("SetupDiCallClassInstaller (DIF_INSTALLDEVICEFILES) failed (0x%08X)",
                          GetLastError()));
        /* get the device install parameters and disable filecopy */
        DeviceInstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS);
        fResult = SetupDiGetDeviceInstallParams (hDeviceInfo, &DeviceInfoData,
                                            &DeviceInstallParams);
        if (fResult)
        {
            DeviceInstallParams.Flags |= DI_NOFILECOPY;
            fResult = SetupDiSetDeviceInstallParams(hDeviceInfo, &DeviceInfoData,
                                                    &DeviceInstallParams);
            if (!fResult)
                SetErrBreak (("SetupDiSetDeviceInstallParams failed (0x%08X)",
                              GetLastError()));
        }

        /*
         * Register any device-specific co-installers for this device,
         */
        fResult = SetupDiCallClassInstaller(DIF_REGISTER_COINSTALLERS,
                                            hDeviceInfo,
                                            &DeviceInfoData);
        if (!fResult)
            SetErrBreak (("SetupDiCallClassInstaller (DIF_REGISTER_COINSTALLERS) failed (0x%08X)",
                          GetLastError()));

        /*
         * install any installer-specified interfaces.
         * and then do the real install
         */
        fResult = SetupDiCallClassInstaller(DIF_INSTALLINTERFACES,
                                            hDeviceInfo,
                                            &DeviceInfoData);
        if (!fResult)
            SetErrBreak (("SetupDiCallClassInstaller (DIF_INSTALLINTERFACES) failed (0x%08X)",
                          GetLastError()));

        fResult = SetupDiCallClassInstaller(DIF_INSTALLDEVICE,
                                            hDeviceInfo,
                                            &DeviceInfoData);
        if (!fResult)
            SetErrBreak (("SetupDiCallClassInstaller (DIF_INSTALLDEVICE) failed (0x%08X)",
                          GetLastError()));

        /* Figure out NetCfgInstanceId */
        hkey = SetupDiOpenDevRegKey(hDeviceInfo,
                                    &DeviceInfoData,
                                    DICS_FLAG_GLOBAL,
                                    0,
                                    DIREG_DRV,
                                    KEY_READ);
        if (hkey == INVALID_HANDLE_VALUE)
            SetErrBreak(("SetupDiOpenDevRegKey failed (0x%08X)", GetLastError()));

        cbSize = sizeof(pWCfgGuidString);
        DWORD ret;
        ret = RegQueryValueExW (hkey, L"NetCfgInstanceId", NULL,
                               &dwValueType, (LPBYTE) pWCfgGuidString, &cbSize);

        RegCloseKey (hkey);

        if (!SetupDiGetDeviceRegistryPropertyW(hDeviceInfo, &DeviceInfoData,
                                               SPDRP_FRIENDLYNAME , /* IN DWORD Property,*/
                                               NULL, /*OUT PDWORD PropertyRegDataType, OPTIONAL*/
                                               (PBYTE)DevName, /*OUT PBYTE PropertyBuffer,*/
                                               sizeof(DevName), /* IN DWORD PropertyBufferSize,*/
                                               NULL /*OUT PDWORD RequiredSize OPTIONAL*/))
        {
            int err = GetLastError();
            if (err != ERROR_INVALID_DATA)
            {
                SetErrBreak (("SetupDiGetDeviceRegistryProperty failed (0x%08X)",
                              err));
            }

            if (!SetupDiGetDeviceRegistryPropertyW(hDeviceInfo, &DeviceInfoData,
                              SPDRP_DEVICEDESC, /* IN DWORD Property,*/
                              NULL, /*OUT PDWORD PropertyRegDataType, OPTIONAL*/
                              (PBYTE)DevName, /*OUT PBYTE PropertyBuffer,*/
                              sizeof(DevName), /* IN DWORD PropertyBufferSize,*/
                              NULL /*OUT PDWORD RequiredSize OPTIONAL*/
                            ))
            {
                err = GetLastError();
                SetErrBreak (("SetupDiGetDeviceRegistryProperty failed (0x%08X)",
                                              err));
            }
        }
    }
    while (0);

    /*
     * cleanup
     */
    if (pQueueCallbackContext)
        SetupTermDefaultQueueCallback(pQueueCallbackContext);

    if (hDeviceInfo != INVALID_HANDLE_VALUE)
    {
        /* an error has occurred, but the device is registered, we must remove it */
        if (ret != 0 && registered)
            SetupDiCallClassInstaller(DIF_REMOVE, hDeviceInfo, &DeviceInfoData);

        found = SetupDiDeleteDeviceInfo(hDeviceInfo, &DeviceInfoData);

        /* destroy the driver info list */
        if (destroyList)
            SetupDiDestroyDriverInfoList(hDeviceInfo, &DeviceInfoData,
                                         SPDIT_CLASSDRIVER);
        /* clean up the device info set */
        SetupDiDestroyDeviceInfoList (hDeviceInfo);
    }

    /* return the network connection GUID on success */
    if (SUCCEEDED(hrc))
    {
        WCHAR ConnectoinName[128];
        ULONG cbName = sizeof(ConnectoinName);

        HRESULT hr = VBoxNetCfgWinGenHostonlyConnectionName(DevName, ConnectoinName, &cbName);
        if (SUCCEEDED(hr))
            hr = VBoxNetCfgWinRenameConnection(pWCfgGuidString, ConnectoinName);

        if (lppszName)
        {
            *lppszName = SysAllocString((const OLECHAR *) DevName);
            if (!*lppszName)
            {
                NonStandardLogFlow(("SysAllocString failed\n"));
                hrc = HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY);
            }
        }

        if (pGuid)
        {
            hrc = CLSIDFromString(pWCfgGuidString, (LPCLSID)pGuid);
            if (FAILED(hrc))
                NonStandardLogFlow(("CLSIDFromString failed, hrc (0x%x)\n", hrc));
        }

        INetCfg *pNetCfg = NULL;
        LPWSTR lpszApp = NULL;
        hr = VBoxNetCfgWinQueryINetCfg(&pNetCfg, TRUE, L"VirtualBox Host-Only Creation",
                                       30 * 1000, /* on Vista we often get 6to4svc.dll holding the lock, wait for 30 sec.  */
                                       /* TODO: special handling for 6to4svc.dll ???, i.e. several retrieves */
                                       &lpszApp);
        if (hr == S_OK)
        {
            hr = vboxNetCfgWinEnumNetCfgComponents(pNetCfg,
                                                   &GUID_DEVCLASS_NETSERVICE,
                                                   vboxNetCfgWinAdjustHostOnlyNetworkInterfacePriority,
                                                   pGuid);
            if (SUCCEEDED(hr))
            {
                hr = vboxNetCfgWinEnumNetCfgComponents(pNetCfg,
                                                       &GUID_DEVCLASS_NETTRANS,
                                                       vboxNetCfgWinAdjustHostOnlyNetworkInterfacePriority,
                                                       pGuid);
                if (SUCCEEDED(hr))
                    hr = vboxNetCfgWinEnumNetCfgComponents(pNetCfg,
                                                           &GUID_DEVCLASS_NETCLIENT,
                                                           vboxNetCfgWinAdjustHostOnlyNetworkInterfacePriority,
                                                           pGuid);
            }

            if (SUCCEEDED(hr))
            {
                hr = pNetCfg->Apply();
            }
            else
                NonStandardLogFlow(("Enumeration failed, hr 0x%x\n", hr));
            VBoxNetCfgWinReleaseINetCfg(pNetCfg, TRUE);
        }
        else if (hr == NETCFG_E_NO_WRITE_LOCK && lpszApp)
        {
            NonStandardLogFlow(("Application %ws is holding the lock, failed\n", lpszApp));
            CoTaskMemFree(lpszApp);
        }
        else
            NonStandardLogFlow(("VBoxNetCfgWinQueryINetCfg failed, hr 0x%x\n", hr));
    }
    return hrc;
}

#undef SetErrBreak

