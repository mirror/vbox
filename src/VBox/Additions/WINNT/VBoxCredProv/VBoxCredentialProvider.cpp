/* $Id$ */
/** @file
 * VBoxCredentialProvider - Main file of the VirtualBox Credential Provider.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <windows.h>
#include <initguid.h>

#ifdef VBOX_WITH_SENS
# include <eventsys.h>
# include <sens.h>
# include <Sensevts.h>
#endif

#include <iprt/buildconfig.h>
#include <iprt/initterm.h>
#ifdef VBOX_WITH_SENS
# include <iprt/string.h>
#endif
#include <VBox/VBoxGuestLib.h>

#include "VBoxCredentialProvider.h"
#include "VBoxCredProvFactory.h"

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static LONG g_cDllRefs  = 0;            /**< Global DLL reference count. */
static HINSTANCE g_hDllInst = NULL;     /**< Global DLL hInstance. */

#ifdef VBOX_WITH_SENS
static IEventSystem *g_pIEventSystem;   /**< Pointer to IEventSystem interface. */

/**
 * Subscribed SENS events.
 */
static struct VBOXCREDPROVSENSEVENTS
{
    /** The actual method name the subscription is for. */
    char *pszMethod;
    /** A friendly name for the subscription. */
    char *pszSubscriptionName;
    /** The actual subscription UUID.
     *  Should not be changed. */
    char *pszSubscriptionUUID;
} g_aSENSEvents[] = {
    { "Logon",             "VBoxCredProv SENS Logon",             "{561D0791-47C0-4BC3-87C0-CDC2621EA653}" },
    { "Logoff",            "VBoxCredProv SENS Logoff",            "{12B618B1-F2E0-4390-BADA-7EB1DC31A70A}" },
    { "StartShell",        "VBoxCredProv SENS StartShell",        "{5941931D-015A-4F91-98DA-81AAE262D090}" },
    { "DisplayLock",       "VBoxCredProv SENS DisplayLock",       "{B7E2C510-501A-4961-938F-A458970930D7}" },
    { "DisplayUnlock",     "VBoxCredProv SENS DisplayUnlock",     "{11305987-8FFC-41AD-A264-991BD5B7488A}" },
    { "StartScreenSaver",  "VBoxCredProv SENS StartScreenSaver",  "{6E2D26DF-0095-4EC4-AE00-2395F09AF7F2}" },
    { "StopScreenSaver",   "VBoxCredProv SENS StopScreenSaver",   "{F53426BC-412F-41E8-9A5F-E5FA8A164BD6}" }
};

/**
 * Implementation of the ISensLogon interface for getting
 * SENS (System Event Notification Service) events. SENS must be up
 * and running on this OS!
 */
interface VBoxCredProvSensLogon : public ISensLogon
{
public:

    VBoxCredProvSensLogon(void) :
        m_cRefs(1)
    {
    }

    STDMETHODIMP QueryInterface(REFIID interfaceID, void **ppvInterface)
    {
        if (   IsEqualIID(interfaceID, IID_IUnknown)
            || IsEqualIID(interfaceID, IID_IDispatch)
            || IsEqualIID(interfaceID, IID_ISensLogon))
        {
            *ppvInterface = this;
            AddRef();
            return S_OK;
        }

        *ppvInterface = NULL;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef(void)
    {
        return InterlockedIncrement(&m_cRefs);
    }

    ULONG STDMETHODCALLTYPE Release(void)
    {
        ULONG ulTemp = InterlockedDecrement(&m_cRefs);
        return ulTemp;
    }

    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(unsigned int FAR* pctinfo)
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetTypeInfo(unsigned int iTInfo, LCID lcid, ITypeInfo FAR* FAR* ppTInfo)
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID riid,
                                            OLECHAR FAR* FAR* rgszNames, unsigned int cNames,
                                            LCID lcid, DISPID FAR* rgDispId)
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
                                     DISPPARAMS FAR* pDispParams, VARIANT FAR* parResult, EXCEPINFO FAR* pExcepInfo,
                                     unsigned int FAR* puArgErr)
    {
        return E_NOTIMPL;
    }

    /* ISensLogon methods */
    STDMETHODIMP Logon(BSTR bstrUserName)
    {
        VBoxCredProvVerbose(0, "VBoxCredProvSensLogon: Logon\n");
        return S_OK;
    }

    STDMETHODIMP Logoff(BSTR bstrUserName)
    {
        VBoxCredProvVerbose(0, "VBoxCredProvSensLogon: Logoff\n");
        return S_OK;
    }

    STDMETHODIMP StartShell(BSTR bstrUserName)
    {
        VBoxCredProvVerbose(0, "VBoxCredProvSensLogon: Logon\n");
        return S_OK;
    }

    STDMETHODIMP DisplayLock(BSTR bstrUserName)
    {
        VBoxCredProvVerbose(0, "VBoxCredProvSensLogon: DisplayLock\n");
        return S_OK;
    }

    STDMETHODIMP DisplayUnlock(BSTR bstrUserName)
    {
        VBoxCredProvVerbose(0, "VBoxCredProvSensLogon: DisplayUnlock\n");
        return S_OK;
    }

    STDMETHODIMP StartScreenSaver(BSTR bstrUserName)
    {
        VBoxCredProvVerbose(0, "VBoxCredProvSensLogon: StartScreenSaver\n");
        return S_OK;
    }

    STDMETHODIMP StopScreenSaver(BSTR bstrUserName)
    {
        VBoxCredProvVerbose(0, "VBoxCredProvSensLogon: StopScreenSaver\n");
        return S_OK;
    }

protected:

    LONG m_cRefs;
};
static VBoxCredProvSensLogon *g_pISensLogon;


/**
 * Register events to be called by SENS.
 *
 * @return  HRESULT
 */
static HRESULT VBoxCredentialProviderRegisterSENS(void)
{
    VBoxCredProvVerbose(0, "VBoxCredentialProviderRegisterSENS\n");

    HRESULT hr = CoCreateInstance(CLSID_CEventSystem, 0, CLSCTX_SERVER, IID_IEventSystem, (void**)&g_pIEventSystem);
    if (FAILED(hr))
    {
        VBoxCredProvVerbose(0, "VBoxCredentialProviderRegisterSENS: Could not connect to CEventSystem, hr=%Rhrc\n",
                            hr);
        return hr;
    }

    g_pISensLogon = new VBoxCredProvSensLogon();
    if (!g_pISensLogon)
    {
        VBoxCredProvVerbose(0, "VBoxCredentialProviderRegisterSENS: Could not create interface instance; out of memory\n");
        return ERROR_OUTOFMEMORY;
    }

    IEventSubscription *pIEventSubscription;
    int i;
    for (i = 0; i < RT_ELEMENTS(g_aSENSEvents); i++)
    {
        VBoxCredProvVerbose(0, "VBoxCredProv: Registering \"%s\" (%s) ...\n",
                            g_aSENSEvents[i].pszMethod, g_aSENSEvents[i].pszSubscriptionName);

        hr = CoCreateInstance(CLSID_CEventSubscription, 0, CLSCTX_SERVER, IID_IEventSubscription, (LPVOID*)&pIEventSubscription);
        if (FAILED(hr))
            continue;

        hr = pIEventSubscription->put_EventClassID(L"{d5978630-5b9f-11d1-8dd2-00aa004abd5e}" /* SENSGUID_EVENTCLASS_LOGON */);
        if (FAILED(hr))
            break;

        hr = pIEventSubscription->put_SubscriberInterface((IUnknown*)g_pISensLogon);
        if (FAILED(hr))
            break;

        PRTUTF16 pwszTemp;
        int rc = RTStrToUtf16(g_aSENSEvents[i].pszMethod, &pwszTemp);
        if (RT_SUCCESS(rc))
        {
            hr = pIEventSubscription->put_MethodName(pwszTemp);
            RTUtf16Free(pwszTemp);
        }
        else
            hr = ERROR_OUTOFMEMORY;
        if (FAILED(hr))
            break;

        rc = RTStrToUtf16(g_aSENSEvents[i].pszSubscriptionName, &pwszTemp);
        if (RT_SUCCESS(rc))
        {
            hr = pIEventSubscription->put_SubscriptionName(pwszTemp);
            RTUtf16Free(pwszTemp);
        }
        else
            hr = ERROR_OUTOFMEMORY;
        if (FAILED(hr))
            break;

        rc = RTStrToUtf16(g_aSENSEvents[i].pszSubscriptionUUID, &pwszTemp);
        if (RT_SUCCESS(rc))
        {
            hr = pIEventSubscription->put_SubscriptionID(pwszTemp);
            RTUtf16Free(pwszTemp);
        }
        else
            hr = ERROR_OUTOFMEMORY;
        if (FAILED(hr))
            break;

        hr = pIEventSubscription->put_PerUser(TRUE);
        if (FAILED(hr))
            break;

        hr = g_pIEventSystem->Store(PROGID_EventSubscription, (IUnknown*)pIEventSubscription);
        if (FAILED(hr))
            break;

        pIEventSubscription->Release();
        pIEventSubscription = NULL;
    }

    if (FAILED(hr))
        VBoxCredProvVerbose(0, "VBoxCredentialProviderRegisterSENS: Could not register \"%s\" (%s), hr=%Rhrc\n",
                            g_aSENSEvents[i].pszMethod, g_aSENSEvents[i].pszSubscriptionName, hr);

    if (pIEventSubscription != NULL)
		pIEventSubscription->Release();

    return hr;
}

/**
 * Unregisters registered SENS events.
 */
static void VBoxCredentialProviderUnregisterSENS(void)
{
    if (g_pIEventSystem)
        g_pIEventSystem->Release();

    /* We need to reconnecto to the event system because we can be called
     * in a different context COM can't handle. */
    HRESULT hr = CoCreateInstance(CLSID_CEventSystem, 0, CLSCTX_SERVER, IID_IEventSystem, (void**)&g_pIEventSystem);
    if (SUCCEEDED(hr))
    {
        VBoxCredProvVerbose(0, "VBoxCredentialProviderUnregisterSENS\n");

        HRESULT hr;

        for (int i = 0; i < RT_ELEMENTS(g_aSENSEvents); i++)
        {
            int iErrorIdX;

            char *pszSubToRemove;
            if (!RTStrAPrintf(&pszSubToRemove, "SubscriptionID=%s",
                              g_aSENSEvents[i].pszSubscriptionUUID))
            {
                continue; /* Keep going. */
            }

            PRTUTF16 pwszTemp;
            int rc2 = RTStrToUtf16(pszSubToRemove, &pwszTemp);
            if (RT_SUCCESS(rc2))
            {
                hr = g_pIEventSystem->Remove(PROGID_EventSubscription, pwszTemp,
                                             &iErrorIdX);
                RTUtf16Free(pwszTemp);
            }
            else
                hr = ERROR_OUTOFMEMORY;

            if (FAILED(hr))
                VBoxCredProvVerbose(0, "VBoxCredentialProviderUnregisterSENS: Could not unregister \"%s\" (query: %s), hr=%Rhrc (index: %d)\n",
                                    g_aSENSEvents[i].pszMethod, pszSubToRemove, hr, iErrorIdX);
                /* Keep going. */

            RTStrFree(pszSubToRemove);
        }

        g_pIEventSystem->Release();
    }

    if (g_pISensLogon)
        delete g_pISensLogon;
}
#endif /* VBOX_WITH_SENS */


BOOL WINAPI DllMain(HINSTANCE hInst, DWORD dwReason, LPVOID pReserved)
{
    NOREF(pReserved);

    g_hDllInst = hInst;

    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
        {
            int rc = RTR3InitDll(0 /* Flags */);
            if (RT_SUCCESS(rc))
                rc = VbglR3Init();

            if (RT_SUCCESS(rc))
            {
                VBoxCredProvVerbose(0, "VBoxCredProv: v%s r%s (%s %s) loaded (refs=%ld)\n",
                                    RTBldCfgVersion(), RTBldCfgRevisionStr(),
                                    __DATE__, __TIME__, g_cDllRefs);
            }

            DisableThreadLibraryCalls(hInst);
            break;
        }

        case DLL_PROCESS_DETACH:

            VBoxCredProvVerbose(0, "VBoxCredProv: Unloaded (refs=%ld)\n", g_cDllRefs);
            if (!g_cDllRefs)
                VbglR3Term();
            break;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }

    return TRUE;
}


/**
 * Increments the reference count by one. Must be released
 * with VBoxCredentialProviderRelease() when finished.
 */
void VBoxCredentialProviderAcquire(void)
{
    LONG cRefCount = InterlockedIncrement(&g_cDllRefs);
    VBoxCredProvVerbose(0, "VBoxCredentialProviderAcquire: Increasing global refcount to %ld\n",
                        cRefCount);
}


/**
 * Decrements the reference count by one.
 */
void VBoxCredentialProviderRelease(void)
{
    LONG cRefCount = InterlockedDecrement(&g_cDllRefs);
    VBoxCredProvVerbose(0, "VBoxCredentialProviderRelease: Decreasing global refcount to %ld\n",
                        cRefCount);
}


/**
 * Returns the current DLL reference count.
 *
 * @return  LONG                The current reference count.
 */
LONG VBoxCredentialProviderRefCount(void)
{
    return g_cDllRefs;
}


/**
 * Entry point for determining whether the credential
 * provider DLL can be unloaded or not.
 *
 * @return  HRESULT
 */
HRESULT __stdcall DllCanUnloadNow(void)
{
    VBoxCredProvVerbose(0, "DllCanUnloadNow (refs=%ld)\n",
                        g_cDllRefs);

#ifdef VBOX_WITH_SENS
    if (!g_cDllRefs)
    {
        VBoxCredentialProviderUnregisterSENS();

        CoUninitialize();
    }
#endif
    return (g_cDllRefs > 0) ? S_FALSE : S_OK;
}


/**
 * Create the VirtualBox credential provider by creating
 * its factory which then in turn can create instances of the
 * provider itself.
 *
 * @return  HRESULT
 * @param   classID             The class ID.
 * @param   interfaceID         The interface ID.
 * @param   ppvInterface        Receives the interface pointer on successful
 *                              object creation.
 */
HRESULT VBoxCredentialProviderCreate(REFCLSID classID, REFIID interfaceID,
                                     void **ppvInterface)
{
    HRESULT hr;
    if (classID == CLSID_VBoxCredProvider)
    {
        VBoxCredProvFactory* pFactory = new VBoxCredProvFactory();
        if (pFactory)
        {
            hr = pFactory->QueryInterface(interfaceID,
                                          ppvInterface);
            pFactory->Release();

#ifdef VBOX_WITH_SENS
            if (SUCCEEDED(hr))
            {
                HRESULT hRes = CoInitializeEx(NULL, COINIT_MULTITHREADED);
                VBoxCredentialProviderRegisterSENS();
            }
#endif
        }
        else
            hr = E_OUTOFMEMORY;
    }
    else
        hr = CLASS_E_CLASSNOTAVAILABLE;

    return hr;
}


/**
 * Entry point for getting the actual credential provider
 * class object.
 *
 * @return  HRESULT
 * @param   classID             The class ID.
 * @param   interfaceID         The interface ID.
 * @param   ppvInterface        Receives the interface pointer on successful
 *                              object creation.
 */
HRESULT __stdcall DllGetClassObject(REFCLSID classID, REFIID interfaceID,
                                    void **ppvInterface)
{
    VBoxCredProvVerbose(0, "DllGetClassObject (refs=%ld)\n",
                        g_cDllRefs);

    return VBoxCredentialProviderCreate(classID, interfaceID, ppvInterface);
}

