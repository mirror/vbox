//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) 2006 Microsoft Corporation. All rights reserved.
//
// Standard dll required functions and class factory implementation.
//
// Modifications (c) 2009 Sun Microsystems, Inc.
//

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <windows.h>

#include <VBox/VBoxGuestLib.h>

#include "dll.h"
#include "guid.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static LONG g_cRef = 0;     /* Global dll reference count */
HINSTANCE g_hinst = NULL;   /* Global dll hinstance */


HRESULT CClassFactory_CreateInstance(REFCLSID rclsid, REFIID riid, void** ppv)
{
    HRESULT hr;
    if (CLSID_VBoxCredProvider == rclsid)
    {
        CClassFactory* pcf = new CClassFactory;
        if (pcf)
        {
            hr = pcf->QueryInterface(riid, ppv);
            pcf->Release();
        }
        else
        {
            hr = E_OUTOFMEMORY;
        }
    }
    else
    {
        hr = CLASS_E_CLASSNOTAVAILABLE;
    }
    return hr;
}


BOOL WINAPI DllMain(HINSTANCE hinstDll,
                    DWORD dwReason,
                    LPVOID pReserved)
{
    UNREFERENCED_PARAMETER(pReserved);

    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:

            DisableThreadLibraryCalls(hinstDll);
            break;

        case DLL_PROCESS_DETACH:
            break;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }

    g_hinst = hinstDll;
    return TRUE;
}


LONG DllAddRef()
{
    return InterlockedIncrement(&g_cRef);
}


LONG DllRelease()
{
    return InterlockedDecrement(&g_cRef);
}


/* DLL entry point */
STDAPI DllCanUnloadNow()
{
    HRESULT hr;

    if (g_cRef > 0)
    {
        hr = S_FALSE;   /* Cocreated objects still exist, don't unload */
    }
    else
    {
        hr = S_OK;      /* Refcount is zero, ok to unload */
    }

    /* Never terminate the runtime! */
    return hr;
}


STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    return CClassFactory_CreateInstance(rclsid, riid, ppv);
}

