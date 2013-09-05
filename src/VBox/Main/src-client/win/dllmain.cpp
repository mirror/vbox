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


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
CComModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
    OBJECT_ENTRY(CLSID_Session, Session)
    OBJECT_ENTRY(CLSID_VirtualBoxClient, VirtualBoxClient)
END_OBJECT_MAP()


/** @def WITH_MANUAL_CLEANUP
 * Manually clean up the registry. */
#if defined(DEBUG) && !defined(VBOX_IN_32_ON_64_MAIN_API)
//# define WITH_MANUAL_CLEANUP
#endif


#ifndef WITH_MANUAL_CLEANUP
/** Type library GUIDs to clean up manually. */
static const char * const g_apszTypelibGuids[] =
{
    "{46137EEC-703B-4FE5-AFD4-7C9BBBBA0259}",
    "{d7569351-1750-46f0-936e-bd127d5bc264}",
};

/** Same as above but with a "Typelib\\" prefix. */
static const char * const g_apszTypelibGuidKeys[] =
{
    "TypeLib\\{46137EEC-703B-4FE5-AFD4-7C9BBBBA0259}",
    "TypeLib\\{d7569351-1750-46f0-936e-bd127d5bc264}",
};

/** Type library version to clean up manually. */
static const char * const g_apszTypelibVersions[] =
{
    "1.0",
    "1.3",
};
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
#ifndef WITH_MANUAL_CLEANUP
static void removeOldMess(void);
#endif


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
    return _Module.RegisterServer(TRUE);
}

/////////////////////////////////////////////////////////////////////////////
// DllUnregisterServer - Removes entries from the system registry

STDAPI DllUnregisterServer(void)
{
    HRESULT hrc = _Module.UnregisterServer(TRUE);
#ifndef WITH_MANUAL_CLEANUP
    removeOldMess();
#endif
    return hrc;
}

#ifndef WITH_MANUAL_CLEANUP

/**
 * Checks if the typelib GUID is one of the ones we wish to clean up.
 *
 * @returns true if it should be cleaned up, false if not.
 * @param   pszTypelibGuid  The typelib GUID as bracketed string.
 */
static bool isTypelibGuidToRemove(const char *pszTypelibGuid)
{
    unsigned i = RT_ELEMENTS(g_apszTypelibGuids);
    while (i-- > 0)
        if (!stricmp(g_apszTypelibGuids[i], pszTypelibGuid))
            return true;
    return false;
}


/**
 * Checks if the typelib version is one of the ones we wish to clean up.
 *
 * @returns true if it should be cleaned up, false if not.
 * @param   pszTypelibVer   The typelib version as string.
 */
static bool isTypelibVersionToRemove(const char *pszTypelibVer)
{
    unsigned i = RT_ELEMENTS(g_apszTypelibVersions);
    while (i-- > 0)
        if (!strcmp(g_apszTypelibVersions[i], pszTypelibVer))
            return true;
    return false;
}


/**
 * Hack to clean out the class IDs belonging to obsolete typelibs on development
 * boxes and such likes.
 */
static void removeOldClassIDs(HKEY hkeyClassesRoot)
{
    HKEY hkeyClsId;
    LONG rc = RegOpenKeyExA(hkeyClassesRoot, "CLSID", NULL, DELETE | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE,
                            &hkeyClsId);
    if (rc == ERROR_SUCCESS)
    {
        for (DWORD idxKey = 0;; idxKey++)
        {
            char szCurNm[128 + 128];
            DWORD cbCurNm = 128;
            rc = RegEnumKeyExA(hkeyClsId, idxKey, szCurNm, &cbCurNm, NULL, NULL, NULL, NULL);
            if (rc == ERROR_NO_MORE_ITEMS)
                break;

            /*
             * Get the typelib GUID and program ID with the class ID.
             */
            AssertBreak(rc == ERROR_SUCCESS);
            strcpy(&szCurNm[cbCurNm], "\\TypeLib");
            HKEY hkeyIfTypelib;
            rc = RegOpenKeyExA(hkeyClsId, szCurNm, NULL, KEY_QUERY_VALUE, &hkeyIfTypelib);
            if (rc != ERROR_SUCCESS)
                continue;

            char szTypelibGuid[128];
            DWORD cbValue = sizeof(szTypelibGuid) - 1;
            rc = RegQueryValueExA(hkeyIfTypelib, NULL, NULL, NULL, (PBYTE)&szTypelibGuid[0], &cbValue);
            if (rc != ERROR_SUCCESS)
                cbValue = 0;
            szTypelibGuid[cbValue] = '\0';
            RegCloseKey(hkeyIfTypelib);
            if (!isTypelibGuidToRemove(szTypelibGuid))
                continue;

            /* ProgId */
            strcpy(&szCurNm[cbCurNm], "\\ProgId");
            HKEY hkeyIfProgId;
            rc = RegOpenKeyExA(hkeyClsId, szCurNm, NULL, KEY_QUERY_VALUE, &hkeyIfProgId);
            if (rc != ERROR_SUCCESS)
                continue;

            char szProgId[64];
            cbValue = sizeof(szProgId) - 1;
            rc = RegQueryValueExA(hkeyClsId, NULL, NULL, NULL, (PBYTE)&szProgId[0], &cbValue);
            if (rc != ERROR_SUCCESS)
                cbValue = 0;
            szProgId[cbValue] = '\0';
            RegCloseKey(hkeyClsId);
            if (strnicmp(szProgId, RT_STR_TUPLE("VirtualBox.")))
                continue;

            /*
             * Ok, it's an orphaned VirtualBox interface. Delete it.
             */
            szCurNm[cbCurNm] = '\0';
            RTAssertMsg2("Should delete HCR/CLSID/%s\n", szCurNm);
            //rc = SHDeleteKeyA(hkeyClsId, szCurNm);
            Assert(rc == ERROR_SUCCESS);
        }

        RegCloseKey(hkeyClsId);
    }
}


/**
 * Hack to clean out the interfaces belonging to obsolete typelibs on
 * development boxes and such likes.
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
            if (!isTypelibGuidToRemove(szTypelibGuid))
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

            if (!isTypelibVersionToRemove(szTypelibVer))
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
 * Hack to clean obsolete typelibs on development boxes and such.
 */
static void removeOldTypelib(HKEY hkeyClassesRoot)
{
    /*
     * Open it and verify the identity.
     */
    unsigned i = RT_ELEMENTS(g_apszTypelibGuidKeys);
    while (i-- > 0)
    {
        HKEY hkeyTyplib;
        LONG rc = RegOpenKeyExA(hkeyClassesRoot, g_apszTypelibGuidKeys[i], NULL,
                                DELETE | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hkeyTyplib);
        if (rc == ERROR_SUCCESS)
        {
            unsigned iVer = RT_ELEMENTS(g_apszTypelibVersions);
            while (iVer-- > 0)
            {
                HKEY hkeyVer;
                rc = RegOpenKeyExA(hkeyTyplib, g_apszTypelibVersions[iVer], NULL, KEY_READ, &hkeyVer);
                if (rc == ERROR_SUCCESS)
                {
                    char szValue[128];
                    DWORD cbValue = sizeof(szValue) - 1;
                    rc = RegQueryValueExA(hkeyVer, NULL, NULL, NULL, (PBYTE)&szValue[0], &cbValue);
                    if (rc == ERROR_SUCCESS)
                    {
                        szValue[cbValue] = '\0';
                        if (!strcmp(szValue, "VirtualBox Type Library"))
                        {
                            RegCloseKey(hkeyVer);
                            hkeyVer = NULL;

                            /*
                             * Delete the type library.
                             */
                            //RTAssertMsg2("Should delete HCR\\%s\\%s\n", g_apszTypelibGuidKeys[i], g_apszTypelibVersions[iVer]);
                            rc = SHDeleteKeyA(hkeyTyplib, g_apszTypelibVersions[iVer]);
                            Assert(rc == ERROR_SUCCESS);
                        }
                    }

                    if (hkeyVer != NULL)
                        RegCloseKey(hkeyVer);
                }
            }
            RegCloseKey(hkeyTyplib);

            /*
             * The typelib key should be empty now, so we can try remove it (non-recursively).
             */
            rc = RegDeleteKeyA(hkeyClassesRoot, g_apszTypelibGuidKeys[i]);
            Assert(rc == ERROR_SUCCESS);
        }
    }
}


/**
 * Hack to clean out obsolete typelibs on development boxes and such.
 */
static void removeOldMess(void)
{
    /*
     * The standard location.
     */
    removeOldTypelib(HKEY_CLASSES_ROOT);
    removeOldInterfaces(HKEY_CLASSES_ROOT);
    removeOldClassIDs(HKEY_CLASSES_ROOT);

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
        removeOldClassIDs(hkeyWow64);

        RegCloseKey(hkeyWow64);
    }
}

#endif /* WITH_MANUAL_CLEANUP */

