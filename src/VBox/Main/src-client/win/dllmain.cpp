/* $Id$ */
/** @file
 * VBoxC - COM DLL exports and DLL init/term.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "VBox/com/defs.h"

#include <SessionImpl.h>
#include <VirtualBoxClientImpl.h>

#include <iprt/initterm.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static ATL::CComModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
    OBJECT_ENTRY(CLSID_Session, Session)
    OBJECT_ENTRY(CLSID_VirtualBoxClient, VirtualBoxClient)
END_OBJECT_MAP()


/////////////////////////////////////////////////////////////////////////////
// DLL Entry Point

extern "C"
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        // idempotent, so doesn't harm, and needed for COM embedding scenario
        RTR3InitDll(RTR3INIT_FLAGS_UNOBTRUSIVE);

        _Module.Init(ObjectMap, hInstance, &LIBID_VirtualBox);
        DisableThreadLibraryCalls(hInstance);
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        _Module.Term();
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// Used to determine whether the DLL can be unloaded by OLE

STDAPI DllCanUnloadNow(void)
{
    return (_Module.GetLockCount() == 0) ? S_OK : S_FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// Returns a class factory to create an object of the requested type

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
    return _Module.GetClassObject(rclsid, riid, ppv);
}

/////////////////////////////////////////////////////////////////////////////
// DllRegisterServer - Adds entries to the system registry

STDAPI DllRegisterServer(void)
{
#ifndef VBOX_WITH_MIDL_PROXY_STUB
    // registers object, typelib and all interfaces in typelib
    return _Module.RegisterServer(TRUE);
#else
    return S_OK; /* VBoxProxyStub does all the work, no need to duplicate it here. */
#endif
}

/////////////////////////////////////////////////////////////////////////////
// DllUnregisterServer - Removes entries from the system registry

STDAPI DllUnregisterServer(void)
{
#ifndef VBOX_WITH_MIDL_PROXY_STUB
    HRESULT hrc = _Module.UnregisterServer(TRUE);
    return hrc;
#else
    return S_OK; /* VBoxProxyStub does all the work, no need to duplicate it here. */
#endif
}

