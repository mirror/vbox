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
/*
 * Based in part on Microsoft DDK sample code for Sample Notify Object
 *+---------------------------------------------------------------------------
 *
 *  Microsoft Windows
 *  Copyright (C) Microsoft Corporation, 1992-2001.
 *
 *  Author:     Alok Sinha
 *
 *----------------------------------------------------------------------------
 */
#include "VBoxNetFltNotify.h"
#include <Ntddndis.h>
#include <assert.h>
#include <stdio.h>

#include <VBoxNetFltNotifyn_i.c>

CComModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
    OBJECT_ENTRY(CLSID_VBoxNetFltNotify, VBoxNetFltNotify)
END_OBJECT_MAP()

#define ReleaseObj( x )  if ( x ) \
                            ((IUnknown*)(x))->Release();

//# define VBOXNETFLTNOTIFY_DEBUG_BIND true

#ifdef DEBUG
# define Assert(a) assert(a)
# define AssertBreakpoint() assert(0)

# define TraceMsg DbgTraceMsg
#else
# define Assert(a) do{}while(0)
# define AssertBreakpoint() do{}while(0)

# define TraceMsg
#endif


static void DbgTraceMsg (LPWSTR szFormat, ...)
{
    static WCHAR szTempBuf[4096];

    va_list arglist;

    va_start(arglist, szFormat);

    vswprintf( szTempBuf, szFormat, arglist );

    OutputDebugStringW( szTempBuf );

    va_end(arglist);
}

VBoxNetFltNotify::VBoxNetFltNotify (VOID) : m_pncc (NULL),
                                m_pnc(NULL)
                                /*,
                                m_eApplyAction(eActUnknown),
                                m_pUnkContext(NULL)*/
{
    TraceMsg(L"VBoxNetFltNotify\n");
}

VBoxNetFltNotify::~VBoxNetFltNotify (VOID)
{
    TraceMsg(L"-->~VBoxNetFltNotify (destructor)\n");
    ReleaseObj( m_pncc );
    ReleaseObj( m_pnc );
    TraceMsg(L"<--~VBoxNetFltNotify (destructor)\n");
}

/*
 * NOTIFY OBJECT FUNCTIONS
 */

/*
 * INetCfgComponentControl
 *
 * The following functions provide the INetCfgComponentControl interface.
 */

/**
 * Initialize the notify object
 *
 * @param pnccItem   Pointer to INetCfgComponent object
 * @param pnc        Pointer to INetCfg object
 * @param fInstalling  TRUE if we are being installed
 * @return S_OK on success, otherwise an error code
 */
STDMETHODIMP VBoxNetFltNotify::Initialize (INetCfgComponent* pncc,
                                     INetCfg* pnc,
                                     BOOL fInstalling)
{
    HRESULT hr = S_OK;

    TraceMsg(L"-->Initialize\n");

    m_pncc = pncc;
    m_pnc = pnc;

    if (m_pncc)
    {
        m_pncc->AddRef();
    }

    if (m_pnc)
    {
        m_pnc->AddRef();
    }

    TraceMsg(L"<--Initialize\n");

    return hr;
}

/**
 * Cancel any changes made to internal data
 * @return S_OK on success, otherwise an error code
 */
STDMETHODIMP VBoxNetFltNotify::CancelChanges (VOID)
{
    TraceMsg(L"CancelChanges\n");
    return S_OK;
}

/*
 * Apply changes. We can make changes to registry etc. here.
 * @return S_OK on success, otherwise an error code
 */
STDMETHODIMP VBoxNetFltNotify::ApplyRegistryChanges(VOID)
{
    TraceMsg(L"ApplyRegistryChanges\n");
    return S_OK;
}

/**
 * Apply changes.
 * @param pfCallback PnPConfigCallback interface.
 * @return S_OK on success, otherwise an error code
 */
STDMETHODIMP VBoxNetFltNotify::ApplyPnpChanges (
                                       INetCfgPnpReconfigCallback* pfCallback)
{
    TraceMsg(L"ApplyPnpChanges\n");
    return S_OK;
}

static HRESULT vboxNetFltWinQueryInstanceKey(IN INetCfgComponent *pComponent, OUT PHKEY phKey)
{
    LPWSTR pPnpId;
    HRESULT hr = pComponent->GetPnpDevNodeId(&pPnpId);
    if(hr == S_OK)
    {
        WCHAR KeyName[MAX_PATH];
        wcscpy(KeyName, L"SYSTEM\\CurrentControlSet\\Enum\\");
        wcscat(KeyName,pPnpId);

        LONG winEr = RegOpenKeyExW(HKEY_LOCAL_MACHINE, KeyName,
          0, /*__reserved  DWORD ulOptions*/
          KEY_READ, /*__in        REGSAM samDesired*/
          phKey);

        if(winEr != ERROR_SUCCESS)
        {
            hr = HRESULT_FROM_WIN32(winEr);
            TraceMsg(L"vboxNetFltWinQueryInstanceKey: RegOpenKeyExW error, hr (0x%x)\n", hr);
            AssertBreakpoint();
        }

        CoTaskMemFree(pPnpId);
    }
    else
    {
        TraceMsg(L"vboxNetFltWinQueryInstanceKey: GetPnpDevNodeId error, hr (0x%x)\n", hr);
        AssertBreakpoint();
    }

    return hr;
}

static HRESULT vboxNetFltWinQueryDriverKey(IN HKEY InstanceKey, OUT PHKEY phKey)
{
    DWORD Type = REG_SZ;
    WCHAR Value[MAX_PATH];
    DWORD cbValue = sizeof(Value);
    HRESULT hr = S_OK;
    LONG winEr = RegQueryValueExW(InstanceKey,
            L"Driver", /*__in_opt     LPCTSTR lpValueName*/
            0, /*__reserved   LPDWORD lpReserved*/
            &Type, /*__out_opt    LPDWORD lpType*/
            (LPBYTE)Value, /*__out_opt    LPBYTE lpData*/
            &cbValue/*__inout_opt  LPDWORD lpcbData*/
            );

    if(winEr == ERROR_SUCCESS)
    {
        WCHAR KeyName[MAX_PATH];
        wcscpy(KeyName, L"SYSTEM\\CurrentControlSet\\Control\\Class\\");
        wcscat(KeyName,Value);

        winEr = RegOpenKeyExW(HKEY_LOCAL_MACHINE, KeyName,
          0, /*__reserved  DWORD ulOptions*/
          KEY_READ, /*__in        REGSAM samDesired*/
          phKey);

        if(winEr != ERROR_SUCCESS)
        {
            hr = HRESULT_FROM_WIN32(winEr);
            TraceMsg(L"vboxNetFltWinQueryDriverKey from instance key: RegOpenKeyExW error, hr (0x%x)\n", hr);
            AssertBreakpoint();
        }
    }
    else
    {
        hr = HRESULT_FROM_WIN32(winEr);
        TraceMsg(L"vboxNetFltWinQueryDriverKey from instance key: RegQueryValueExW error, hr (0x%x)\n", hr);
        AssertBreakpoint();
    }

    return hr;
}

static HRESULT vboxNetFltWinQueryDriverKey(IN INetCfgComponent *pComponent, OUT PHKEY phKey)
{
    HKEY InstanceKey;
    HRESULT hr = vboxNetFltWinQueryInstanceKey(pComponent, &InstanceKey);
    if(hr == S_OK)
    {
        hr = vboxNetFltWinQueryDriverKey(InstanceKey, phKey);
        if(hr != S_OK)
        {
            TraceMsg(L"vboxNetFltWinQueryDriverKey from Component: vboxNetFltWinQueryDriverKey error, hr (0x%x)\n", hr);
            AssertBreakpoint();
        }
        RegCloseKey(InstanceKey);
    }
    else
    {
        TraceMsg(L"vboxNetFltWinQueryDriverKey from Component: vboxNetFltWinQueryInstanceKey error, hr (0x%x)\n", hr);
        AssertBreakpoint();
    }

    return hr;
}

static HRESULT vboxNetFltWinNotifyCheckNetAdp(IN INetCfgComponent *pComponent, OUT bool * pbShouldBind)
{
    HRESULT hr;
    LPWSTR pDevId;
    hr = pComponent->GetId(&pDevId);
    if(hr == S_OK)
    {
        if(!_wcsnicmp(pDevId, L"sun_VBoxNetAdp", sizeof(L"sun_VBoxNetAdp")/2))
        {
            *pbShouldBind = false;
        }
        else
        {
            hr = S_FALSE;
        }
        CoTaskMemFree(pDevId);
    }
    else
    {
        TraceMsg(L"vboxNetFltWinNotifyCheckNetAdp: GetId failed, hr (0x%x)\n", hr);
        AssertBreakpoint();
    }

    return hr;
}

static HRESULT vboxNetFltWinNotifyCheckMsLoop(IN INetCfgComponent *pComponent, OUT bool * pbShouldBind)
{
    HRESULT hr;
    LPWSTR pDevId;
    hr = pComponent->GetId(&pDevId);
    if(hr == S_OK)
    {
        if(!_wcsnicmp(pDevId, L"*msloop", sizeof(L"*msloop")/2))
        {
            /* we need to detect the medium the adapter is presenting
             * to do that we could examine in the registry the *msloop params */
            HKEY DriverKey;
            hr = vboxNetFltWinQueryDriverKey(pComponent, &DriverKey);
            if(hr == S_OK)
            {
                DWORD Type = REG_SZ;
                WCHAR Value[64]; /* 2 should be enough actually, paranoid check for extra spaces */
                DWORD cbValue = sizeof(Value);
                LONG winEr = RegQueryValueExW(DriverKey,
                            L"Medium", /*__in_opt     LPCTSTR lpValueName*/
                            0, /*__reserved   LPDWORD lpReserved*/
                            &Type, /*__out_opt    LPDWORD lpType*/
                            (LPBYTE)Value, /*__out_opt    LPBYTE lpData*/
                            &cbValue/*__inout_opt  LPDWORD lpcbData*/
                            );
                if(winEr == ERROR_SUCCESS)
                {
                    PWCHAR endPrt;
                    ULONG enmMedium = wcstoul(Value,
                       &endPrt,
                       0 /* base*/);

                    winEr = errno;
                    if(winEr == ERROR_SUCCESS)
                    {
                        if(enmMedium == 0) /* 0 is Ethernet */
                        {
                            TraceMsg(L"vboxNetFltWinNotifyCheckMsLoop: loopback is configured as ethernet, binding\n", winEr);
                            *pbShouldBind = true;
                        }
                        else
                        {
                            TraceMsg(L"vboxNetFltWinNotifyCheckMsLoop: loopback is configured as NOT ethernet, NOT binding\n", winEr);
                            *pbShouldBind = false;
                        }
                    }
                    else
                    {
                        TraceMsg(L"vboxNetFltWinNotifyCheckMsLoop: wcstoul error, winEr (%d), ignoring and binding\n", winEr);
                        AssertBreakpoint();
                        *pbShouldBind = true;
                    }
                }
                else
                {
                    TraceMsg(L"vboxNetFltWinNotifyCheckMsLoop: RegQueryValueExW failed, winEr (%d), ignoring, binding\n", hr);
                    /* TODO: we should check the default medium in HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Class\{4D36E972-E325-11CE-BFC1-08002bE10318}\<driver_id>\Ndi\Params\Medium, REG_SZ "Default" value */
                    AssertBreakpoint();
                    *pbShouldBind = true;
                }

                RegCloseKey(DriverKey);
            }
            else
            {
                TraceMsg(L"vboxNetFltWinNotifyCheckMsLoop: vboxNetFltWinQueryDriverKey for msloop failed, hr (0x%x)\n", hr);
                AssertBreakpoint();
            }
        }
        else
        {
            hr = S_FALSE;
        }
        CoTaskMemFree(pDevId);
    }
    else
    {
        TraceMsg(L"vboxNetFltWinNotifyCheckMsLoop: GetId failed, hr (0x%x)\n", hr);
        AssertBreakpoint();
    }

    return hr;
}

static HRESULT vboxNetFltWinNotifyCheckLowerRange(IN INetCfgComponent *pComponent, OUT bool * pbShouldBind)
{
    HKEY DriverKey;
    HKEY InterfacesKey;
    HRESULT hr = vboxNetFltWinQueryDriverKey(pComponent, &DriverKey);
    if(hr == S_OK)
    {
        LONG winEr = RegOpenKeyExW(DriverKey, L"Ndi\\Interfaces",
                        0, /*__reserved  DWORD ulOptions*/
                        KEY_READ, /*__in        REGSAM samDesired*/
                        &InterfacesKey);
        if(winEr == ERROR_SUCCESS)
        {
            DWORD Type = REG_SZ;
            WCHAR Value[MAX_PATH];
            DWORD cbValue = sizeof(Value);
            winEr = RegQueryValueExW(InterfacesKey,
                    L"LowerRange", /*__in_opt     LPCTSTR lpValueName*/
                    0, /*__reserved   LPDWORD lpReserved*/
                    &Type, /*__out_opt    LPDWORD lpType*/
                    (LPBYTE)Value, /*__out_opt    LPBYTE lpData*/
                    &cbValue/*__inout_opt  LPDWORD lpcbData*/
                    );
            if(winEr == ERROR_SUCCESS)
            {
                if(wcsstr(Value,L"ethernet") || wcsstr(Value, L"wan"))
                {
                    *pbShouldBind = true;
                }
                else
                {
                    *pbShouldBind = false;
                }
            }
            else
            {
                /* do not set err status to it */
                *pbShouldBind = false;
                TraceMsg(L"vboxNetFltWinNotifyCheckLowerRange: RegQueryValueExW for LowerRange error, winEr (%d), not binding\n", winEr);
                AssertBreakpoint();
            }

            RegCloseKey(InterfacesKey);
        }
        else
        {
            hr = HRESULT_FROM_WIN32(winEr);
            TraceMsg(L"vboxNetFltWinNotifyCheckLowerRange: RegOpenKeyExW error, hr (0x%x)\n", hr);
            AssertBreakpoint();
        }

        RegCloseKey(DriverKey);
    }
    else
    {
        TraceMsg(L"vboxNetFltWinNotifyShouldBind for INetCfgComponen: vboxNetFltWinQueryDriverKey failed, hr (0x%x)\n", hr);
        AssertBreakpoint();
    }

    return hr;
}

static HRESULT vboxNetFltWinNotifyShouldBind(IN INetCfgComponent *pComponent, OUT bool *pbShouldBind)
{
    TraceMsg(L"-->vboxNetFltWinNotifyShouldBind for INetCfgComponent\n");
    DWORD  fCharacteristics;
    HRESULT hr;

    do
    {
        /* filter out only physical adapters */
        hr = pComponent->GetCharacteristics(&fCharacteristics);
        if(hr != S_OK)
        {
            TraceMsg(L"vboxNetFltWinNotifyShouldBind for INetCfgComponen: GetCharacteristics failed, hr (0x%x)\n", hr);
            AssertBreakpoint();
            break;
        }


        if(fCharacteristics & NCF_HIDDEN)
        {
            /* we are not binding to hidden adapters */
            *pbShouldBind = false;
            break;
        }

        hr = vboxNetFltWinNotifyCheckMsLoop(pComponent, pbShouldBind);
        if(hr == S_OK)
        {
            /* this is a loopback adapter,
             * the pbShouldBind already contains the result */
            break;
        }
        else if(hr != S_FALSE)
        {
            /* error occurred */
            break;
        }

        hr = vboxNetFltWinNotifyCheckNetAdp(pComponent, pbShouldBind);
        if(hr == S_OK)
        {
            /* this is a VBoxNetAdp adapter,
             * the pbShouldBind already contains the result */
            break;
        }
        else if(hr != S_FALSE)
        {
            /* error occurred */
            break;
        }

        /* hr == S_FALSE means this is not a loopback adpater, set it to S_OK */
        hr = S_OK;

//        if(!(fCharacteristics & NCF_PHYSICAL))
//        {
//            /* we are binding to physical adapters only */
//            *pbShouldBind = false;
//            break;
//        }

        hr = vboxNetFltWinNotifyCheckLowerRange(pComponent, pbShouldBind);
        if(hr == S_OK)
        {
            /* the vboxNetFltWinNotifyCheckLowerRange ccucceeded,
             * the pbShouldBind already contains the result */
            break;
        }
        /* we are here because of the fail, nothing else to do */
    } while(0);

    TraceMsg(L"<--vboxNetFltWinNotifyShouldBind for INetCfgComponent, hr (0x%x)\n", hr);

    return hr;
}


static HRESULT vboxNetFltWinNotifyShouldBind(IN INetCfgBindingInterface *pIf, OUT bool *pbShouldBind)
{
    TraceMsg(L"-->vboxNetFltWinNotifyShouldBind for INetCfgBindingInterface\n");

    INetCfgComponent * pAdapterComponent;
    HRESULT hr = pIf->GetLowerComponent(&pAdapterComponent);
    if(hr == S_OK)
    {
        hr = vboxNetFltWinNotifyShouldBind(pAdapterComponent, pbShouldBind);

        pAdapterComponent->Release();
    }
    else
    {
        TraceMsg(L"vboxNetFltWinNotifyShouldBind: GetLowerComponent failed, hr (0x%x)\n", hr);
        AssertBreakpoint();
    }

    TraceMsg(L"<--vboxNetFltWinNotifyShouldBind for INetCfgBindingInterface, hr (0x%x)\n", hr);
    return hr;
}

static HRESULT vboxNetFltWinNotifyShouldBind(IN INetCfgBindingPath *pPath, OUT bool * pbDoBind)
{
    TraceMsg(L"-->vboxNetFltWinNotifyShouldBind for INetCfgBindingPath\n");
    IEnumNetCfgBindingInterface  *pEnumBindingIf;
    HRESULT hr = pPath->EnumBindingInterfaces(&pEnumBindingIf);

    if(hr == S_OK)
    {
        hr = pEnumBindingIf->Reset();
        if(hr == S_OK)
        {
            ULONG    ulCount;
            INetCfgBindingInterface  *pBindingIf;

            do
            {
                hr = pEnumBindingIf->Next( 1,
                                 &pBindingIf,
                                 &ulCount );
                if(hr == S_OK)
                {
                    hr = vboxNetFltWinNotifyShouldBind(pBindingIf, pbDoBind);

                    pBindingIf->Release();

                    if(hr == S_OK)
                    {
                        if(!(*pbDoBind))
                        {
                            break;
                        }
                    }
                    else
                    {
                        /* break on failure */
                        break;
                    }
                }
                else if(hr == S_FALSE)
                {
                    /* no more elements */
                    hr = S_OK;
                    break;
                }
                else
                {
                    TraceMsg(L"vboxNetFltWinNotifyShouldBind: Next failed, hr (0x%x)\n", hr);
                    AssertBreakpoint();
                    /* break on falure */
                    break;
                }
            } while(true);
        }
        else
        {
            TraceMsg(L"vboxNetFltWinNotifyShouldBind: Reset failed, hr (0x%x)\n", hr);
            AssertBreakpoint();
        }

        pEnumBindingIf->Release();
    }
    else
    {
        TraceMsg(L"vboxNetFltWinNotifyShouldBind: EnumBindingInterfaces failed, hr (0x%x)\n", hr);
        AssertBreakpoint();
    }

    TraceMsg(L"<--vboxNetFltWinNotifyShouldBind for INetCfgBindingPath, hr (0x%x)\n", hr);
    return hr;
}

static bool vboxNetFltWinNotifyShouldBind(IN INetCfgBindingPath *pPath)
{
#ifdef VBOXNETFLTNOTIFY_DEBUG_BIND
    return VBOXNETFLTNOTIFY_DEBUG_BIND;
#else
    HRESULT hr;
    bool bShouldBind;

    TraceMsg( L"-->vboxNetFltWinNotifyShouldBind\n");

    hr = vboxNetFltWinNotifyShouldBind(pPath, &bShouldBind) ;
    if(hr != S_OK)
    {
        TraceMsg( L"vboxNetFltWinNotifyShouldBind: vboxNetFltWinNotifyShouldBind failed, hr (0x%x)\n", hr );
        bShouldBind = VBOXNETFLTNOTIFY_ONFAIL_BINDDEFAULT;
    }


    TraceMsg( L"<--vboxNetFltWinNotifyShouldBind, bShouldBind (%d)\n", bShouldBind);
    return bShouldBind;
#endif
}

/*
 * INetCfgComponentNotifyBinding
 * The following functions provide the INetCfgComponentNotifyBinding interface.
 */

/**
 * This is specific to the component being installed. This will
 * ask us if we want to bind to the Item being passed into
 * this routine. We can disable the binding by returning
 * NETCFG_S_DISABLE_QUERY
 *
 * @param dwChangeFlag Type of binding change
 * @param pncbpItem   Pointer to INetCfgBindingPath object
 * @return S_OK on success, otherwise an error code.
 */
STDMETHODIMP VBoxNetFltNotify::QueryBindingPath (IN DWORD dwChangeFlag,
                                           IN INetCfgBindingPath *pPath)
{
    HRESULT hr = S_OK;
    TraceMsg( L"-->QueryBindingPath, flags (0x%x)\n", dwChangeFlag );

    if(!vboxNetFltWinNotifyShouldBind(pPath))
    {
        TraceMsg( L"QueryBindingPath: we are NOT supporting the current component\n");
        hr = NETCFG_S_DISABLE_QUERY;
    }
    else
    {
        TraceMsg( L"QueryBindingPath: we are supporting the current component\n");
    }
    TraceMsg( L"<--QueryBindingPath, hr (0x%x)\n", hr);
    return hr;
}

/**
 * bind to the component passed to us.
 * @param dwChangeFlag Type of system change
 * @param pncc Pointer to INetCfgComponent object
 * @return S_OK on success, otherwise an error code
 */
STDMETHODIMP VBoxNetFltNotify::NotifyBindingPath (IN DWORD dwChangeFlag,
                                            IN INetCfgBindingPath *pPath)
{
    HRESULT hr = S_OK;

    TraceMsg( L"-->NotifyBindingPath, flags (0x%x)\n", dwChangeFlag );
    /* NCN_ADD | NCN_ENABLE
     * NCN_REMOVE | NCN_ENABLE
     * NCN_ADD | NCN_DISABLE
     * NCN_REMOVE | NCN_DISABLE
     * */
    if ( (dwChangeFlag & NCN_ENABLE) && !(dwChangeFlag & NCN_REMOVE))
    {
        if(!vboxNetFltWinNotifyShouldBind(pPath))
        {
            TraceMsg( L"NotifyBindingPath: binding enabled for the component we are not supporting\n");
            AssertBreakpoint();
            hr = NETCFG_S_DISABLE_QUERY;
        }
    }

    TraceMsg( L"<--NotifyBindingPath, hr (0x%x)\n", hr);

    return hr;
}

/*
 * DLL Entry Point
 */
extern "C"
BOOL WINAPI DllMain (HINSTANCE hInstance,
                     DWORD dwReason,
                     LPVOID /*lpReserved*/)
{
    TraceMsg( L"-->DllMain.\n");

    if (dwReason == DLL_PROCESS_ATTACH) {

        TraceMsg( L"   Reason: Attach.\n");

        _Module.Init(ObjectMap, hInstance);

        DisableThreadLibraryCalls(hInstance);
    }
    else if (dwReason == DLL_PROCESS_DETACH) {

        TraceMsg( L"   Reason: Detach.\n");

           _Module.Term();
    }

    TraceMsg( L"<--DllMain.\n");

    return TRUE;
}

/*
 * Used to determine whether the DLL can be unloaded by OLE
 */
STDAPI DllCanUnloadNow(void)
{
    HRESULT hr;

    TraceMsg( L"-->DllCanUnloadNow.\n");

    hr = (_Module.GetLockCount() == 0) ? S_OK : S_FALSE;

    TraceMsg( L"<--DllCanUnloadNow, hr (0x%x).\n",
            hr );

    return hr;
}

/*
 * Returns a class factory to create an object of the requested type
 */
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    TraceMsg( L"DllGetClassObject.\n");

    return _Module.GetClassObject(rclsid, riid, ppv);
}


/*
 * DllRegisterServer - Adds entries to the system registry
 */
STDAPI DllRegisterServer(void)
{
    /* Registers object, typelib and all interfaces in typelib */

    TraceMsg( L"DllRegisterServer.\n");

    return _Module.RegisterServer(TRUE);
}

/*
 * DllUnregisterServer - Removes entries from the system registry
 */
STDAPI DllUnregisterServer(void)
{
    TraceMsg( L"DllUnregisterServer.\n");

    _Module.UnregisterServer();

    return S_OK;
}
