/* $Id$ */
/** @file
 * VBoxC - COM DLL exports and DLL init/term.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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
#include "VBox/com/defs.h"

#include <SessionImpl.h>
#include <VirtualBoxClientImpl.h>

#include <atlbase.h>
#include <atlcom.h>

#include <iprt/initterm.h>
#include <iprt/assert.h>
#include <iprt/string.h>

CComModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
    OBJECT_ENTRY(CLSID_Session, Session)
    OBJECT_ENTRY(CLSID_VirtualBoxClient, VirtualBoxClient)
END_OBJECT_MAP()


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void removeOldMess(void);


/////////////////////////////////////////////////////////////////////////////
// DLL Entry Point

extern "C"
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        _Module.Init(ObjectMap, hInstance, &LIBID_VirtualBox);
        DisableThreadLibraryCalls(hInstance);

        // idempotent, so doesn't harm, and needed for COM embedding scenario
        RTR3InitDll(RTR3INIT_FLAGS_UNOBTRUSIVE);
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
    return (_Module.GetLockCount()==0) ? S_OK : S_FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// Returns a class factory to create an object of the requested type

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    return _Module.GetClassObject(rclsid, riid, ppv);
}

/////////////////////////////////////////////////////////////////////////////
// DllRegisterServer - Adds entries to the system registry

STDAPI DllRegisterServer(void)
{
    // registers object, typelib and all interfaces in typelib
    removeOldMess();
    return _Module.RegisterServer(TRUE);
}

/////////////////////////////////////////////////////////////////////////////
// DllUnregisterServer - Removes entries from the system registry

STDAPI DllUnregisterServer(void)
{
    return _Module.UnregisterServer(TRUE);
}



/**
 * Hack to clean out the interfaces belonging to the old typelib on development
 * boxes and such likes.
 */
static void removeOldInterfaces(HKEY hkeyClassesRoot)
{
    HKEY hkeyInterface;
    LONG rc = RegOpenKeyExA(hkeyClassesRoot, "Interface", NULL, DELETE | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE,
                            &hkeyInterface);
    if (rc == ERROR_SUCCESS)
    {
        for (DWORD idxKey = 0;; idxKey++)
        {
            char szCurNm[128 + 128];
            DWORD cbCurNm = 128;
            rc = RegEnumKeyExA(hkeyInterface, idxKey, szCurNm, &cbCurNm, NULL, NULL, NULL, NULL);
            if (rc == ERROR_NO_MORE_ITEMS)
                break;

            /*
             * Get the typelib GUID and version associated with the interface.
             */
            AssertBreak(rc == ERROR_SUCCESS);
            strcpy(&szCurNm[cbCurNm], "\\TypeLib");
            HKEY hkeyIfTypelib;
            rc = RegOpenKeyExA(hkeyInterface, szCurNm, NULL, KEY_QUERY_VALUE, &hkeyIfTypelib);
            if (rc != ERROR_SUCCESS)
                continue;

            char szTypelibGuid[128];
            DWORD cbValue = sizeof(szTypelibGuid) - 1;
            rc = RegQueryValueExA(hkeyIfTypelib, NULL, NULL, NULL, (PBYTE)&szTypelibGuid[0], &cbValue);
            if (rc != ERROR_SUCCESS)
                cbValue = 0;
            szTypelibGuid[cbValue] = '\0';
            if (strcmp(szTypelibGuid, "{46137EEC-703B-4FE5-AFD4-7C9BBBBA0259}"))
            {
                RegCloseKey(hkeyIfTypelib);
                continue;
            }

            char szTypelibVer[64];
            cbValue = sizeof(szTypelibVer) - 1;
            rc = RegQueryValueExA(hkeyIfTypelib, "Version", NULL, NULL, (PBYTE)&szTypelibVer[0], &cbValue);
            if (rc != ERROR_SUCCESS)
                cbValue = 0;
            szTypelibVer[cbValue] = '\0';

            RegCloseKey(hkeyIfTypelib);

            if (strcmp(szTypelibVer, "1.3"))
                continue;


            /*
             * Ok, it's an orphaned VirtualBox interface. Delete it.
             */
            szCurNm[cbCurNm] = '\0';
            //RTAssertMsg2("Should delete HCR/Interface/%s\n", szCurNm);
            rc = SHDeleteKeyA(hkeyInterface, szCurNm);
            Assert(rc == ERROR_SUCCESS);
        }

        RegCloseKey(hkeyInterface);
    }
}


/**
 * Hack to clean out the old typelib on development boxes and such.
 */
static void removeOldTypelib(HKEY hkeyClassesRoot)
{
    /*
     * Open it and verify the identity.
     */
    HKEY hkeyOldTyplib;
    LONG rc = RegOpenKeyExA(hkeyClassesRoot, "TypeLib\\{46137EEC-703B-4FE5-AFD4-7C9BBBBA0259}", NULL,
                            DELETE | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hkeyOldTyplib);
    if (rc == ERROR_SUCCESS)
    {
        HKEY hkeyVer1Dot3;
        rc = RegOpenKeyExA(hkeyOldTyplib, "1.3", NULL, KEY_READ, &hkeyVer1Dot3);
        if (rc == ERROR_SUCCESS)
        {
            char szValue[128];
            DWORD cbValue = sizeof(szValue) - 1;
            rc = RegQueryValueExA(hkeyVer1Dot3, NULL, NULL, NULL, (PBYTE)&szValue[0], &cbValue);
            if (rc == ERROR_SUCCESS)
            {
                szValue[cbValue] = '\0';
                if (!strcmp(szValue, "VirtualBox Type Library"))
                {
                    RegCloseKey(hkeyVer1Dot3);
                    hkeyVer1Dot3 = NULL;

                    /*
                     * Delete the type library.
                     */
                    //RTAssertMsg2("Should delete HCR/TypeLib/{46137EEC-703B-4FE5-AFD4-7C9BBBBA0259}/1.3\n");
                    rc = SHDeleteKeyA(hkeyOldTyplib, "1.3");
                    Assert(rc == ERROR_SUCCESS);
                }
            }

            if (hkeyVer1Dot3 != NULL)
                RegCloseKey(hkeyVer1Dot3);
        }
        RegCloseKey(hkeyOldTyplib);

        /*
         * The typelib key should be empty now, so we can try remove it (non-recursively).
         */
        rc = RegDeleteKeyA(hkeyClassesRoot, "TypeLib\\{46137EEC-703B-4FE5-AFD4-7C9BBBBA0259}");
        Assert(rc == ERROR_SUCCESS);
    }
}


/**
 * Hack to clean out the old typelib on development boxes and such.
 */
static void removeOldMess(void)
{
    /*
     * The standard location.
     */
    removeOldTypelib(HKEY_CLASSES_ROOT);
    removeOldInterfaces(HKEY_CLASSES_ROOT);

    /*
     * Wow64 if present.
     */
    HKEY hkeyWow64;
    LONG rc = RegOpenKeyExA(HKEY_CLASSES_ROOT, "Wow6432Node", NULL,
                            DELETE | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hkeyWow64);
    if (rc == ERROR_SUCCESS)
    {
        removeOldTypelib(hkeyWow64);
        removeOldInterfaces(hkeyWow64);

        RegCloseKey(hkeyWow64);
    }
}

