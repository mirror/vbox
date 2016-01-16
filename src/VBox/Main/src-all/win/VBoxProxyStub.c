/* $Id$ */
/** @file
 * VBoxProxyStub - Proxy Stub and Typelib, COM DLL exports and DLL init/term.
 *
 * @remarks This is a C file and not C++ because rpcproxy.h isn't C++ clean,
 *          at least not in SDK v7.1.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define PROXY_DELEGATION                                                                             /* see generated dlldata.c */
#include <rpcproxy.h>
#include <Windows.h>
#include <Shlwapi.h>
#include <stdio.h>

#include "VirtualBox.h"
#include <iprt/assert.h>
#include <iprt/path.h>
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** @def WITH_MANUAL_CLEANUP
 * Manually clean up the registry. */
#if 1 /*defined(DEBUG) && !defined(VBOX_IN_32_ON_64_MAIN_API) - needed on testboxes (and obviously dev boxes)! */
# define WITH_MANUAL_CLEANUP
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** For NdrXxx. */
CStdPSFactoryBuffer         g_ProxyStubFactory =                                                     /* see generated dlldata.c */
{
    NULL,
    0,
    NULL,
    0
};
/** Reference to VirtualBox_p.c structure. */
EXTERN_PROXY_FILE(VirtualBox)                                                                        /* see generated dlldata.c */
/** For NdrXxx and for returning. */
static const ProxyFileInfo *g_apProxyFiles[] =
{
    REFERENCE_PROXY_FILE(VirtualBox),
    NULL /* terminator */
};
/** The class ID for this proxy stub factory (see Makefile). */
static const CLSID          g_ProxyClsId = PROXY_CLSID_IS;
/** The instance handle of this DLL.  For use in registration routines. */
static HINSTANCE            g_hDllSelf;


#ifdef WITH_MANUAL_CLEANUP
/** Type library GUIDs to clean up manually. */
static const char * const   g_apszTypelibGuids[] =
{
    "{46137EEC-703B-4FE5-AFD4-7C9BBBBA0259}",
    "{d7569351-1750-46f0-936e-bd127d5bc264}",
};

/** Same as above but with a "Typelib\\" prefix. */
static const char * const   g_apszTypelibGuidKeys[] =
{
    "TypeLib\\{46137EEC-703B-4FE5-AFD4-7C9BBBBA0259}",
    "TypeLib\\{d7569351-1750-46f0-936e-bd127d5bc264}",
};

/** Type library version to clean up manually. */
static const char * const   g_apszTypelibVersions[] =
{
    "1.0",
    "1.3",
};
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
#ifdef WITH_MANUAL_CLEANUP
static void removeOldMess(void);
#endif




/**
 * DLL main function.
 *
 * @returns TRUE (/ FALSE).
 * @param   hInstance           The DLL handle.
 * @param   dwReason            The rason for the call (DLL_XXX).
 * @param   lpReserved          Reserved.
 */
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
            g_hDllSelf = hInstance;
            DisableThreadLibraryCalls(hInstance);
            /* We don't use IPRT, so no need to init it! */
            break;

        case DLL_PROCESS_DETACH:
            break;
    }

    NOREF(lpReserved);
    return TRUE;
}


/**
 * RPC entry point returning info about the proxy.
 */
void RPC_ENTRY GetProxyDllInfo(const ProxyFileInfo ***ppapInfo, const CLSID **ppClsid)
{
    *ppapInfo = &g_apProxyFiles[0];
    *ppClsid  = &g_ProxyClsId;
};


/**
 * Instantiate the proxy stub class object.
 *
 * @returns COM status code
 * @param   rclsid      Reference to the ID of the call to instantiate (our
 *                      g_ProxyClsId).
 * @param   riid        The interface ID to return (IID_IPSFactoryBuffer).
 * @param   ppv         Where to return the interface pointer on success.
 */
HRESULT STDAPICALLTYPE DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv)
{
    HRESULT hrc;
    Assert(memcmp(rclsid, &g_ProxyClsId, sizeof(g_ProxyClsId)) == 0);

    hrc = NdrDllGetClassObject(rclsid, riid, ppv,                                 /* see DLLGETCLASSOBJECTROUTINE in RpcProxy.h */
                               g_apProxyFiles, &g_ProxyClsId, &g_ProxyStubFactory);

    /*
     * This may fail if the IDL compiler generates code that is incompatible
     * with older windows releases.  Like for instance 64-bit W2K8 SP1 not
     * liking the output of MIDL 7.00.0555 (from the v7.1 SDK), despite
     * /target being set to NT51.
     */
    AssertMsg(hrc == S_OK, ("%Rhrc\n",  hrc));
    return hrc;
}


/**
 * Checks whether the DLL can be unloaded or not.
 *
 * @returns S_OK if it can be unloaded, S_FALSE if not.
 */
HRESULT STDAPICALLTYPE DllCanUnloadNow(void)
{
    return NdrDllCanUnloadNow(&g_ProxyStubFactory);                                        /* see DLLCANUNLOADNOW in RpcProxy.h */
}



/**
 * Release call that could be referenced by VirtualBox_p.c via
 * CStdStubBuffer_METHODS.
 *
 * @returns New reference count.
 * @param   pThis               Buffer to release.
 */
ULONG STDMETHODCALLTYPE CStdStubBuffer_Release(IRpcStubBuffer *pThis)                /* see CSTDSTUBBUFFERRELEASE in RpcProxy.h */
{
    return NdrCStdStubBuffer_Release(pThis, (IPSFactoryBuffer *)&g_ProxyStubFactory);
}


/**
 * Release call referenced by VirtualBox_p.c via
 * CStdStubBuffer_DELEGATING_METHODS.
 *
 * @returns New reference count.
 * @param   pThis               Buffer to release.
 */
ULONG WINAPI CStdStubBuffer2_Release(IRpcStubBuffer *pThis)                         /* see CSTDSTUBBUFFER2RELEASE in RpcProxy.h */
{
    return NdrCStdStubBuffer2_Release(pThis, (IPSFactoryBuffer *)&g_ProxyStubFactory);
}


/**
 * Pure virtual method implementation referenced by VirtualBox_p.c
 *
 * @returns New reference count.
 * @param   pThis               Buffer to release.
 */
void __cdecl _purecall(void)                                                              /* see DLLDUMMYPURECALL in RpcProxy.h */
{
    AssertFailed();
}


/**
 * Registry modifier state.
 */
typedef struct VBPSREGSTATE
{
    /** Where the classes and stuff are to be registered. */
    HKEY hkeyClassesRootDst;
    /** The handle to the CLSID key under hkeyClassesRootDst. */
    HKEY hkeyClsidRootDst;
    /** For logging purposes.   */
    const char *pszLogRoot;

    /** Alternative location where duplicates must always be unregistered from. */
    HKEY hkeyAltClassesRootsUnreg;
    /** The handle to the CLSID key under hkeyAltClassesRootsUnreg. */
    HKEY hkeyAltClsidRootsUnreg;

    /** The current total result. */
    LSTATUS rc;

    /** KEY_WOW64_32KEY, KEY_WOW64_64KEY or 0 (for default).  Allows doing all
     * almost the work from one process (at least W7+ due to aliases). */
    DWORD   fSamWow;
    /** Desired key Access when only unregistring. */
    DWORD   fSamUnreg;
    /** Desired key Access when both unregistring and registering. */
    DWORD   fSamBoth;
    /** Set if we're only unregistering. */
    bool    fUnregisterOnly;
} VBPSREGSTATE;


/**
 * Initializes a registry modification job state.
 *
 * Always call vbpsRegTerm!
 *
 * @returns Windows error code (ERROR_SUCCESS on success).
 * @param   pState          The state to init.
 * @param   hkeyRoot        The registry root tree constant.
 * @param   pszSubRoot      The path to the where the classes are registered,
 *                          NULL if @a hkeyRoot.
 * @param   pszLogRoot      For error logging/debugging?
 * @param   hkeyAltRoot     The registry root tree constant for the alternative
 *                          registrations (remove only).
 * @param   pszAltSubRoot   The path to where classes could also be registered,
 *                          but shouldn't be in our setup.
 * @param   fUnregisterOnly If true, only unregister. If false, also register.
 * @param   fSamWow         KEY_WOW64_32KEY or 0.
 */
static LSTATUS vbpsRegInit(VBPSREGSTATE *pState, HKEY hkeyRoot, const char *pszSubRoot, const char *pszLogRoot,
                           HKEY hkeyAltRoot, const char *pszAltSubRoot, bool fUnregisterOnly, DWORD fSamWow)
{
    LSTATUS rc;
    REGSAM  fAccess = !fUnregisterOnly
                    ? DELETE | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_CREATE_SUB_KEY
                    : DELETE | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE;

    pState->hkeyClassesRootDst          = NULL;
    pState->hkeyClsidRootDst            = NULL;
    pState->pszLogRoot                  = pszLogRoot;
    pState->hkeyAltClassesRootsUnreg    = NULL;
    pState->hkeyAltClsidRootsUnreg      = NULL;
    pState->fUnregisterOnly             = fUnregisterOnly;
    pState->fSamWow                     = fSamWow;
    pState->fSamUnreg                   = pState->fSamWow | DELETE | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE;
    pState->fSamBoth                    = pState->fSamUnreg | (!fUnregisterOnly ? KEY_SET_VALUE | KEY_CREATE_SUB_KEY : 0);
    pState->rc                          = ERROR_SUCCESS;

    rc = RegOpenKeyExA(hkeyRoot, pszSubRoot, 0 /*fOptions*/, pState->fSamBoth, &pState->hkeyClassesRootDst);
    AssertMsgReturn(rc == ERROR_SUCCESS, ("%u\n", rc), pState->rc = rc);
    rc = RegOpenKeyExA(pState->hkeyClassesRootDst, "CLSID", 0 /*fOptions*/, pState->fSamBoth, &pState->hkeyClsidRootDst);
    AssertMsgReturn(rc == ERROR_SUCCESS, ("%u\n", rc), pState->rc = rc);
    if (hkeyAltRoot)
    {
        rc = RegOpenKeyExA(hkeyAltRoot, pszAltSubRoot, 0 /*fOptions*/, pState->fSamUnreg, &pState->hkeyAltClassesRootsUnreg);
        AssertMsgReturn(rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND, ("%u\n", rc), pState->rc = rc);
        if (pState->hkeyAltClassesRootsUnreg)
        {
            rc = RegOpenKeyExA(pState->hkeyAltClassesRootsUnreg, "CLSID", 0 /*fOptions*/, pState->fSamUnreg,
                               &pState->hkeyAltClsidRootsUnreg);
            AssertMsgReturn(rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND, ("%u\n", rc), pState->rc = rc);
        }
    }
    return ERROR_SUCCESS;
}


/**
 * Terminates the state, closing all open keys.
 *
 * @param   pState          The state to clean up.
 */
static void vbpsRegTerm(VBPSREGSTATE *pState)
{
    LSTATUS rc;
    if (pState->hkeyClassesRootDst)
    {
        rc = RegCloseKey(pState->hkeyClassesRootDst);
        Assert(rc == ERROR_SUCCESS);
        pState->hkeyClassesRootDst = NULL;
    }
    if (pState->hkeyClsidRootDst)
    {
        rc = RegCloseKey(pState->hkeyClsidRootDst);
        Assert(rc == ERROR_SUCCESS);
        pState->hkeyClsidRootDst = NULL;
    }
    if (pState->hkeyAltClassesRootsUnreg)
    {
        rc = RegCloseKey(pState->hkeyAltClassesRootsUnreg);
        Assert(rc == ERROR_SUCCESS);
        pState->hkeyAltClassesRootsUnreg = NULL;
    }
    if (pState->hkeyAltClsidRootsUnreg)
    {
        rc = RegCloseKey(pState->hkeyAltClsidRootsUnreg);
        Assert(rc == ERROR_SUCCESS);
        pState->hkeyAltClsidRootsUnreg = NULL;
    }
}


/** The destination buffer size required by vbpsFormatUuidInCurly. */
#define CURLY_UUID_STR_BUF_SIZE     48

/**
 * Formats a UUID to a string, inside curly braces.
 *
 * @returns ERROR_SUCCESS on success, otherwise windows error code.
 * @param   pszUuid             Output buffer of size CURLY_UUID_STR_BUF_SIZE.
 * @param   pUuid               The UUID to format.
 */
static LSTATUS vbpsFormatUuidInCurly(char *pszUuid, const CLSID *pUuid)
{
    unsigned char *pszTmpStr = NULL;
    size_t cchTmpStr;
    RPC_STATUS rc = UuidToStringA((UUID *)pUuid, &pszTmpStr);
    AssertReturnStmt(rc == RPC_S_OK, *pszUuid = '\0', ERROR_OUTOFMEMORY);

    cchTmpStr = strlen((const char *)pszTmpStr);
    AssertReturnStmt(cchTmpStr == 36 && cchTmpStr < CURLY_UUID_STR_BUF_SIZE - 3, RpcStringFreeA(&pszTmpStr), ERROR_INVALID_DATA);

    pszUuid[0] = '{';
    memcpy(pszUuid + 1, pszTmpStr, cchTmpStr);
    pszUuid[1 + cchTmpStr] = '}';
    pszUuid[1 + cchTmpStr + 1] = '\0';

    RpcStringFreeA(&pszTmpStr);
    return ERROR_SUCCESS;
}


/**
 * Sets a registry string value.
 *
 * @returns See RegSetValueExA (errors are remembered in the state).
 * @param   pState              The registry modifier state.
 * @param   hKey                The key to add the value to.
 * @param   pszValueNm          The value name. NULL for setting the default.
 * @param   pszValue            The value string.
 * @param   uLine               The line we're called from.
 */
static LSTATUS vbpsSetRegValueAA(VBPSREGSTATE *pState, HKEY hKey, const char *pszValueNm, const char *pszValue, unsigned uLine)
{
    LSTATUS rc = RegSetValueExA(hKey, pszValueNm, 0 /*Reserved*/, REG_SZ, pszValue, (DWORD)strlen(pszValue) + 1);
    if (rc == ERROR_SUCCESS)
        return ERROR_SUCCESS;
    AssertMsgFailed(("%d: '%s'='%s' -> %u\n", uLine, pszValueNm, pszValue, rc));
    return pState->rc = rc;
}

/**
 * Creates a registry key.
 *
 * @returns See RegCreateKeyA and RegSetValueExA (errors are remembered in the
 *          state).
 * @param   pState              The registry modifier state.
 * @param   hKeyParent          The parent key.
 * @param   pszKey              The new key under @a hKeyParent.
 * @param   phKey               Where to return the handle to the new key.
 * @param   uLine               The line we're called from.
 */
static LSTATUS vbpsCreateRegKeyA(VBPSREGSTATE *pState, HKEY hKeyParent, const char *pszKey, PHKEY phKey, unsigned uLine)
{
    HKEY hNewKey;
    LSTATUS rc = RegCreateKeyExA(hKeyParent, pszKey, 0 /*Reserved*/, NULL /*pszClass*/, 0 /*fOptions*/,
                                 pState->fSamBoth, NULL /*pSecAttr*/, &hNewKey, NULL /*pdwDisposition*/);
    if (rc == ERROR_SUCCESS)
        *phKey = hNewKey;
    else
    {
        AssertMsgFailed(("%d: create key '%s' -> %u\n", uLine, pszKey,  rc));
        pState->rc = rc;
        *phKey = NULL;
    }
    return rc;
}


/**
 * Creates a registry key with a default string value.
 *
 * @returns See RegCreateKeyA and RegSetValueExA (errors are remembered in the
 *          state).
 * @param   pState              The registry modifier state.
 * @param   hKeyParent          The parent key.
 * @param   pszKey              The new key under @a hKeyParent.
 * @param   pszValue            The value string.
 * @param   uLine               The line we're called from.
 */
static LSTATUS vbpsCreateRegKeyWithDefaultValueAA(VBPSREGSTATE *pState, HKEY hKeyParent, const char *pszKey,
                                                  const char *pszValue, unsigned uLine)
{
    HKEY hNewKey;
    LSTATUS rc = vbpsCreateRegKeyA(pState, hKeyParent, pszKey, &hNewKey, uLine);
    if (rc == ERROR_SUCCESS)
    {
        rc = vbpsSetRegValueAA(pState, hNewKey, NULL /*pszValueNm*/, pszValue, uLine);
        RegCloseKey(hNewKey);
    }
    else
    {
        AssertMsgFailed(("%d: create key '%s'(/Default='%s') -> %u\n", uLine, pszKey, pszValue, rc));
        pState->rc = rc;
    }
    return rc;
}


/**
 * Creates a registry key with a default string value, return the key.
 *
 * @returns See RegCreateKeyA and RegSetValueExA (errors are remembered in the
 *          state).
 * @param   pState              The registry modifier state.
 * @param   hKeyParent          The parent key.
 * @param   pszKey              The new key under @a hKeyParent.
 * @param   pszValue            The value string.
 * @param   phKey               Where to return the handle to the new key.
 * @param   uLine               The line we're called from.
 */
static LSTATUS vbpsCreateRegKeyWithDefaultValueAAEx(VBPSREGSTATE *pState, HKEY hKeyParent, const char *pszKey,
                                                    const char *pszValue, PHKEY phKey, unsigned uLine)
{
    HKEY hNewKey;
    LSTATUS rc = vbpsCreateRegKeyA(pState, hKeyParent, pszKey, &hNewKey, uLine);
    if (rc == ERROR_SUCCESS)
    {
        rc = vbpsSetRegValueAA(pState, hNewKey, NULL /*pszValueNm*/, pszValue, uLine);
        *phKey = hNewKey;
    }
    else
    {
        AssertMsgFailed(("%d: create key '%s'(/Default='%s') -> %u\n", uLine, pszKey, pszValue, rc));
        pState->rc = rc;
        *phKey = NULL;
    }
    return rc;
}


/**
 * Register an application id.
 *
 * @returns Windows error code (errors are rememberd in the state).
 * @param   pState              The registry modifier state.
 * @param   pszAppId            The application UUID string.
 * @param   pszDescription      The description string.
 */
LSTATUS VbpsRegisterAppId(VBPSREGSTATE *pState, const char *pszAppId, const char *pszDescription)
{
    LSTATUS rc;
    HKEY hkeyAppIds;
    Assert(*pszAppId == '{');

    /* Always unregister. */
    if (pState->hkeyAltClassesRootsUnreg)
    {
        rc = RegOpenKeyExW(pState->hkeyAltClassesRootsUnreg, L"AppID", 0 /*fOptions*/, pState->fSamUnreg, &hkeyAppIds);
        AssertMsgStmt(rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND, ("%u\n", rc), pState->rc = rc);
        if (rc == ERROR_SUCCESS)
        {
            rc = SHDeleteKeyA(hkeyAppIds, pszAppId);
            AssertMsgStmt(rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND, ("%u\n", rc), pState->rc = rc);
            RegCloseKey(hkeyAppIds);
        }
    }

    rc = RegOpenKeyExW(pState->hkeyClassesRootDst, L"AppID", 0 /*fOptions*/, pState->fSamBoth, &hkeyAppIds);
    AssertMsgReturn(rc == ERROR_SUCCESS, ("%u\n", rc), pState->rc = rc);
    if (rc == ERROR_SUCCESS)
    {
        rc = SHDeleteKeyA(hkeyAppIds, pszAppId);
        AssertMsgStmt(rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND, ("%u\n", rc), pState->rc = rc);
    }

    /* Register */
    if (!pState->fUnregisterOnly)
        vbpsCreateRegKeyWithDefaultValueAA(pState, hkeyAppIds, pszAppId, pszDescription, __LINE__);

    RegCloseKey(hkeyAppIds);

    return pState->rc;
}


/**
 * Register an class name.
 *
 * @returns Windows error code (errors are rememberd in the state).
 * @param   pState              The registry modifier state.
 * @param   pszClassName        The name of the class.
 * @param   pszDescription      The description string
 * @param   pClsId              The UUID for the class.
 * @param   pszCurVerSuffIfRootName     This is the current version suffix to
 *                                      append to @a pszClassName when
 *                                      registering the version idependent name.
 */
LSTATUS VbpsRegisterClassName(VBPSREGSTATE *pState, const char *pszClassName, const char *pszDescription,
                              const CLSID *pClsId, const char *pszCurVerSuffIfRootName)
{
    LSTATUS rc;

    /*
     * Always unregister.
     */
    if (pState->hkeyAltClassesRootsUnreg)
    {
        rc = SHDeleteKeyA(pState->hkeyAltClassesRootsUnreg, pszClassName);
        AssertMsgStmt(rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND, ("%u\n", rc), pState->rc = rc);
    }

    rc = SHDeleteKeyA(pState->hkeyClassesRootDst, pszClassName);
    AssertMsgStmt(rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND, ("%u\n", rc), pState->rc = rc);

    if (!pState->fUnregisterOnly)
    {
        /*
         * Register
         */
        /* pszClassName/Default = description. */
        HKEY hkeyClass;
        rc = vbpsCreateRegKeyWithDefaultValueAAEx(pState, pState->hkeyClassesRootDst, pszClassName, pszDescription,
                                                  &hkeyClass, __LINE__);
        if (rc == ERROR_SUCCESS)
        {
            char szClsId[CURLY_UUID_STR_BUF_SIZE];

            /* CLSID/Default = pClsId. */
            rc = vbpsFormatUuidInCurly(szClsId, pClsId);
            AssertMsgStmt(rc == ERROR_SUCCESS, ("%u\n", rc), pState->rc = rc);
            if (rc == ERROR_SUCCESS)
                vbpsCreateRegKeyWithDefaultValueAA(pState, hkeyClass, "CLSID", szClsId, __LINE__);

            /* CurVer/Default = pszClassName+Suffix. */
            if (pszCurVerSuffIfRootName != NULL)
            {
                char szCurClassNameVer[128];
                rc = RTStrCopy(szCurClassNameVer, sizeof(szCurClassNameVer), pszClassName);
                if (RT_SUCCESS(rc))
                    rc = RTStrCat(szCurClassNameVer, sizeof(szCurClassNameVer), pszCurVerSuffIfRootName);
                AssertStmt(RT_SUCCESS(rc), pState->rc = rc = ERROR_INVALID_DATA);
                if (rc == ERROR_SUCCESS)
                    vbpsCreateRegKeyWithDefaultValueAA(pState, hkeyClass, "CurVer", szCurClassNameVer, __LINE__);
            }

            RegCloseKey(hkeyClass);
        }
    }

    return pState->rc;
}


/**
 * Registers a class ID.
 *
 * @returns Windows error code (errors are rememberd in the state).
 * @param   pState                      The registry modifier state.
 * @param   pClsId                      The UUID for the class.
 * @param   pszDescription              The description string.
 * @param   pszAppId                    The application ID.
 * @param   pszClassName                The version idependent class name.
 * @param   pszCurClassNameVerSuffix    The suffix to add to @a pszClassName for
 *                                      the current version.
 * @param   pTypeLibId                  The UUID for the typelib this class
 *                                      belongs to.
 * @param   pszServerType               The server type (InprocServer32 or
 *                                      LocalServer32).
 * @param   pwszVBoxDir                 The VirtualBox install directory
 *                                      (unicode), trailing slash.
 * @param   pszServerSubPath            What to append to @a pwszVBoxDir to
 *                                      construct the server module name.
 * @param   pszThreadingModel           The threading model for inproc servers,
 *                                      NULL for local servers.
 */
LSTATUS VbpsRegisterClassId(VBPSREGSTATE *pState, const CLSID *pClsId, const char *pszDescription, const char *pszAppId,
                            const char *pszClassName, const char *pszCurClassNameVerSuffix, const CLSID *pTypeLibId,
                            const char *pszServerType, PCRTUTF16 pwszVBoxDir, const char *pszServerSubPath,
                            const char *pszThreadingModel)
{
    LSTATUS rc;
    char szClsId[CURLY_UUID_STR_BUF_SIZE];
    char szTypeLibId[CURLY_UUID_STR_BUF_SIZE];

    Assert(!pszAppId || *pszAppId == '{');
    Assert((pwszVBoxDir == NULL && pState->fUnregisterOnly) || pwszVBoxDir[RTUtf16Len(pwszVBoxDir) - 1] == '\\');

    /*
     * Format the UUID first to get it over with.  We always need CLSID.
     */
    rc = vbpsFormatUuidInCurly(szClsId, pClsId);
    AssertMsgReturn(rc == ERROR_SUCCESS, ("%u\n", rc), pState->rc = rc);
    if (pTypeLibId)
    {
        rc = vbpsFormatUuidInCurly(szTypeLibId, pTypeLibId);
        AssertMsgReturn(rc == ERROR_SUCCESS, ("%u\n", rc), pState->rc = rc);
    }
    else
        szTypeLibId[0] = '\0';

    /*
     * Always unregister.
     */
    if (pState->hkeyAltClsidRootsUnreg)
    {
        rc = SHDeleteKeyA(pState->hkeyAltClsidRootsUnreg, szClsId);
        AssertMsgStmt(rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND, ("%u\n", rc), pState->rc = rc);
    }

    rc = SHDeleteKeyA(pState->hkeyClsidRootDst, szClsId);
    AssertMsgStmt(rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND, ("%u\n", rc), pState->rc = rc);

    if (!pState->fUnregisterOnly)
    {
        /*
         * Register
         */
        HKEY hkeyClass;
        rc = vbpsCreateRegKeyWithDefaultValueAAEx(pState, pState->hkeyClsidRootDst, szClsId, pszDescription,
                                                  &hkeyClass, __LINE__);
        if (rc == ERROR_SUCCESS)
        {
            HKEY hkeyServerType;
            char szCurClassNameVer[128];

            /* pszServerType/Default = module. */
            rc = vbpsCreateRegKeyA(pState, hkeyClass, pszServerType, &hkeyServerType, __LINE__);
            if (rc == ERROR_SUCCESS)
            {
                RTUTF16 wszModule[MAX_PATH * 2];
                PRTUTF16 pwszCur = wszModule;
                bool fQuoteIt = strcmp(pszServerType, "LocalServer32") == 0;
                if (fQuoteIt)
                    *pwszCur++ = '"';

                rc = RTUtf16Copy(pwszCur, MAX_PATH, pwszVBoxDir); AssertRC(rc);
                pwszCur += RTUtf16Len(pwszCur);
                rc = RTUtf16CopyAscii(pwszCur, MAX_PATH - 3, pszServerSubPath); AssertRC(rc);
                pwszCur += RTUtf16Len(pwszCur);

                if (fQuoteIt)
                    *pwszCur++ = '"';
                *pwszCur++ = '\0';      /* included, so ++. */

                rc = RegSetValueExW(hkeyServerType, NULL /*pszValueNm*/, 0 /*Reserved*/,
                                    REG_SZ, (const BYTE *)&wszModule[0], (DWORD)((uintptr_t)pwszCur - (uintptr_t)&wszModule[0]));
                AssertMsgStmt(rc == ERROR_SUCCESS, ("%u\n", rc), pState->rc = rc);

                /* pszServerType/ThreadingModel = pszThreading Model. */
                if (pszThreadingModel)
                    vbpsSetRegValueAA(pState, hkeyServerType, "ThreadingModel", pszThreadingModel, __LINE__);

                RegCloseKey(hkeyServerType);
            }

            /* ProgId/Default = pszClassName + pszCurClassNameVerSuffix. */
            if (pszClassName)
            {
                rc = RTStrCopy(szCurClassNameVer, sizeof(szCurClassNameVer), pszClassName);
                if (RT_SUCCESS(rc))
                    rc = RTStrCat(szCurClassNameVer, sizeof(szCurClassNameVer), pszCurClassNameVerSuffix);
                AssertStmt(RT_SUCCESS(rc), pState->rc = rc = ERROR_INVALID_DATA);
                if (rc == ERROR_SUCCESS)
                    vbpsCreateRegKeyWithDefaultValueAA(pState, hkeyClass, "ProgId", szCurClassNameVer, __LINE__);

                /* VersionIndependentProgID/Default = pszClassName. */
                vbpsCreateRegKeyWithDefaultValueAA(pState, hkeyClass, "VersionIndependentProgID", pszClassName, __LINE__);
            }

            /* TypeLib/Default = pTypeLibId. */
            if (pTypeLibId)
                vbpsCreateRegKeyWithDefaultValueAA(pState, hkeyClass, "TypeLib", szTypeLibId, __LINE__);

            RegCloseKey(hkeyClass);
        }
    }

    return pState->rc;
}


/**
 * Register modules and classes from the VirtualBox.xidl file.
 *
 * @returns COM status code.
 * @param   pwszVBoxDir         The VirtualBox application directory.
 * @param   fIs32On64           Set if this is the 32-bit on 64-bit component.
 *
 * @todo convert to XSLT.
 */
void RegisterXidlModulesAndClassesGenerated(VBPSREGSTATE *pState, PCRTUTF16 pwszVBoxDir, bool fIs32On64)
{
    const char *pszAppId = "{819B4D85-9CEE-493C-B6FC-64FFE759B3C9}";
    const char *pszInprocDll = !fIs32On64 ? "VBoxC.dll" : "x86\\VBoxClient-x86.dll";

    VbpsRegisterAppId(pState, pszAppId, "VirtualBox Application");

    /* VBoxSVC */
    VbpsRegisterClassName(pState, "VirtualBox.VirtualBox.1", "VirtualBox Class", &CLSID_VirtualBox, NULL);
    VbpsRegisterClassName(pState, "VirtualBox.VirtualBox",   "VirtualBox Class", &CLSID_VirtualBox, ".1");
    VbpsRegisterClassId(pState, &CLSID_VirtualBox, "VirtualBox Class", pszAppId, "VirtualBox.VirtualBox", ".1",
                        &LIBID_VirtualBox, "LocalServer32", pwszVBoxDir, "VBoxSVC.exe", NULL /*N/A*/);
    /* VBoxC */
    VbpsRegisterClassName(pState, "VirtualBox.Session.1", "Session Class", &CLSID_Session, NULL);
    VbpsRegisterClassName(pState, "VirtualBox.Session", "Session Class", &CLSID_Session, "VirtualBox.Session.1");
    VbpsRegisterClassId(pState, &CLSID_Session, "Session Class", pszAppId, "VirtualBox.Session", ".1",
                        &LIBID_VirtualBox, "InprocServer32", pwszVBoxDir, pszInprocDll, "Free");

    VbpsRegisterClassName(pState, "VirtualBox.VirtualBoxClient.1", "VirtualBoxClient Class", &CLSID_VirtualBoxClient, NULL);
    VbpsRegisterClassName(pState, "VirtualBox.VirtualBoxClient", "VirtualBoxClient Class", &CLSID_VirtualBoxClient, ".1");
    VbpsRegisterClassId(pState, &CLSID_VirtualBoxClient, "VirtualBoxClient Class", pszAppId,
                        "VirtualBox.VirtualBoxClient", ".1",
                        &LIBID_VirtualBox, "InprocServer32", pwszVBoxDir, pszInprocDll, "Free");

}

#ifndef VBOX_PROXY_STUB_32_ON_64
void RegisterOtherProxyStubAndTypelibDll(VBPSREGSTATE *pState, PCRTUTF16 pwszVBoxDir, bool fIs32On64)
{
    if (!pState->fUnregisterOnly)
    {
        const char *pszWinXx = !fIs32On64 ? "win64"             : "win32";
        const char *pszPsDll = !fIs32On64 ? "VBoxProxyStub.dll" : "VBoxProxyStub-x86.dll";
        char szTypeLibId[CURLY_UUID_STR_BUF_SIZE];
        char szMajMin[64];
        HKEY hKeyTypeLibs;
        HKEY hKeyTypeLibId;
        HKEY hKeyMajMin;
        HKEY hKey0;
        HKEY hKeyWinXx;
        LSTATUS rc;

        /* Proxy stub factory class ID. */
        VbpsRegisterClassId(pState, &g_ProxyClsId, "PSFactoryBuffer", NULL /*pszAppId*/,
                            NULL /*pszClassName*/, NULL /*pszCurClassNameVerSuffix*/, NULL /*pTypeLibId*/,
                            "InprocServer32", pwszVBoxDir, pszPsDll, "Both");

        /*
         * Typelib DLL.
         */
        rc = vbpsFormatUuidInCurly(szTypeLibId, &LIBID_VirtualBox);
        AssertMsgReturnVoidStmt(rc == ERROR_SUCCESS, ("%u\n", rc), pState->rc = rc);

        rc = RegOpenKeyExA(pState->hkeyClassesRootDst, "TypeLib", 0 /*fOptions*/, pState->fSamBoth, &hKeyTypeLibs);
        AssertMsgReturnVoidStmt(rc == ERROR_SUCCESS, ("%u\n", rc), pState->rc = rc);
        rc = RegOpenKeyExA(hKeyTypeLibs, szTypeLibId, 0 /*fOptions*/, pState->fSamBoth, &hKeyTypeLibId);
        if (rc == ERROR_FILE_NOT_FOUND)
            rc = vbpsCreateRegKeyA(pState, hKeyTypeLibs, szTypeLibId, &hKeyTypeLibId, __LINE__);
        RegCloseKey(hKeyTypeLibs);

        AssertMsgReturnVoidStmt(rc == ERROR_SUCCESS, ("%u\n", rc), pState->rc = rc);

        /* Major.Minor/Default = Name. */
        sprintf(szMajMin, "%u.%u", kTypeLibraryMajorVersion, kTypeLibraryMinorVersion);
        rc = RegOpenKeyExA(hKeyTypeLibId, szMajMin, 0 /*fOptions*/, pState->fSamBoth, &hKeyMajMin);
        if (rc == ERROR_FILE_NOT_FOUND)
            rc = vbpsCreateRegKeyWithDefaultValueAAEx(pState, hKeyTypeLibId, szMajMin, "VirtualBox Type Library",
                                                      &hKeyMajMin, __LINE__);
        RegCloseKey(hKeyTypeLibId);
        AssertMsgReturnVoidStmt(rc == ERROR_SUCCESS, ("%u\n", rc), pState->rc = rc);

        /* 0/. */
        rc = RegOpenKeyExA(hKeyMajMin, "0", 0 /*fOptions*/, pState->fSamBoth, &hKey0);
        if (rc == ERROR_FILE_NOT_FOUND)
            rc = vbpsCreateRegKeyA(pState, hKeyMajMin, "0", &hKey0, __LINE__);
        RegCloseKey(hKeyMajMin);
        AssertMsgReturnVoidStmt(rc == ERROR_SUCCESS, ("%u\n", rc), pState->rc = rc);

        /* winXx/Default = VBoxProxyStub. */
        rc = RegOpenKeyExA(hKey0, pszWinXx, 0 /*fOptions*/, pState->fSamBoth, &hKeyWinXx);
        if (rc == ERROR_FILE_NOT_FOUND)
        {
            RTUTF16 wszDllPath[MAX_PATH * 2];

            rc = RTUtf16Copy(wszDllPath, MAX_PATH, pwszVBoxDir); AssertRC(rc);
            rc = RTUtf16CatAscii(wszDllPath, MAX_PATH * 2, pszPsDll); AssertRC(rc);

            rc = vbpsCreateRegKeyA(pState, hKey0, pszWinXx, &hKeyWinXx, __LINE__);
            if (rc == ERROR_SUCCESS)
            {
                rc = RegSetValueExW(hKeyWinXx, NULL /*pszValueNm*/, 0 /*Reserved*/,
                                    REG_SZ, (const BYTE *)&wszDllPath[0], (DWORD)((RTUtf16Len(wszDllPath) + 1 )* 2));
                AssertMsgStmt(rc == ERROR_SUCCESS, ("%u\n", rc), pState->rc = rc);
            }
        }
        RegCloseKey(hKey0);
        AssertMsgReturnVoidStmt(rc == ERROR_SUCCESS, ("%u\n", rc), pState->rc = rc);
        RegCloseKey(hKeyWinXx);
    }

}
#endif


HRESULT RegisterXidlModulesAndClasses(PRTUTF16 pwszDllName, bool fUnregisterOnly)
{
    VBPSREGSTATE State;
    LSTATUS rc;

    /*
     * Drop the filename and get the directory containing the DLL.
     */
    if (!fUnregisterOnly)
    {
        RTUTF16 wc;
        size_t off = RTUtf16Len(pwszDllName);
        while (   off > 0
               && (   (wc = pwszDllName[off - 1]) >= 127U
                   || !RTPATH_IS_SEP((unsigned char)wc)))
            off--;
#ifdef VBOX_PROXY_STUB_32_ON_64
        /* The -x86 variant is in a x86 subdirectory, drop it. */
        while (   off > 0
               && (   (wc = pwszDllName[off - 1]) < 127U
                   && RTPATH_IS_SEP((unsigned char)wc)))
            off--;
        while (   off > 0
               && (   (wc = pwszDllName[off - 1]) >= 127U
                   || !RTPATH_IS_SEP((unsigned char)wc)))
            off--;
#endif
        pwszDllName[off] = '\0';
    }

    /*
     * Do registration for the current execution mode of the DLL.
     */
    rc = vbpsRegInit(&State,
                     HKEY_CLASSES_ROOT, NULL, "HKCR", /* HKEY_LOCAL_MACHINE, "Software\\Classes", "HKLM\\Software\\Classes", */
                     HKEY_CURRENT_USER,  "Software\\Classes",
                     fUnregisterOnly, 0);
    if (rc == ERROR_SUCCESS)
    {
#ifdef VBOX_PROXY_STUB_32_ON_64
        RegisterXidlModulesAndClassesGenerated(&State, pwszDllName, true);
#else
        RegisterXidlModulesAndClassesGenerated(&State, pwszDllName, false);
#endif
        rc = State.rc;
    }

    vbpsRegTerm(&State);

#ifndef VBOX_PROXY_STUB_32_ON_64
    /*
     * Do the WOW6432Node registrations too.
     */
    if (rc == ERROR_SUCCESS)
    {
        rc = vbpsRegInit(&State,
                         HKEY_CLASSES_ROOT, "Wow6432Node", "HKCR\\Wow6432Node",
                         HKEY_CURRENT_USER, "Software\\Classes",
                         fUnregisterOnly, KEY_WOW64_32KEY);
        if (rc == ERROR_SUCCESS)
        {
            RegisterXidlModulesAndClassesGenerated(&State, pwszDllName, true);
            RegisterOtherProxyStubAndTypelibDll(&State, pwszDllName, true);
            rc = State.rc;
        }
        vbpsRegTerm(&State);
    }
#endif

    /*
     * Translate error code? Return.
     */
    if (rc == ERROR_SUCCESS)
        return S_OK;
    return E_FAIL;
}


/**
 * Register the interfaces proxied by this DLL, and to avoid duplication and
 * minimize work the VBox type library, classes and servers are also registered.
 *
 * @returns COM status code.
 */
HRESULT STDAPICALLTYPE DllRegisterServer(void)
{
    HRESULT hrc;

    /*
     * Register the type library first.
     */
    ITypeLib *pITypeLib;
    WCHAR wszDllName[MAX_PATH];
    DWORD cwcRet = GetModuleFileNameW(g_hDllSelf, wszDllName, RT_ELEMENTS(wszDllName));
    AssertReturn(cwcRet > 0 && cwcRet < RT_ELEMENTS(wszDllName), CO_E_PATHTOOLONG);

    hrc = LoadTypeLib(wszDllName, &pITypeLib);
    AssertMsgReturn(SUCCEEDED(hrc), ("%Rhrc\n", hrc), hrc);
    hrc = RegisterTypeLib(pITypeLib, wszDllName, NULL /*pszHelpDir*/);
    pITypeLib->lpVtbl->Release(pITypeLib);
    AssertMsgReturn(SUCCEEDED(hrc), ("%Rhrc\n", hrc), hrc);

    /*
     * Register proxy stub.
     */
    hrc = NdrDllRegisterProxy(g_hDllSelf, &g_apProxyFiles[0], &g_ProxyClsId);         /* see DLLREGISTRY_ROUTINES in RpcProxy.h */
    AssertMsgReturn(SUCCEEDED(hrc), ("%Rhrc\n", hrc), hrc);

    /*
     * Register the VBox modules and classes.
     */
    hrc = RegisterXidlModulesAndClasses(wszDllName, false /*fUnregisterOnly*/);
    AssertMsgReturn(SUCCEEDED(hrc), ("%Rhrc\n", hrc), hrc);

    return S_OK;
}


/**
 * Reverse of DllRegisterServer.
 *
 * @returns COM status code.
 */
HRESULT STDAPICALLTYPE DllUnregisterServer(void)
{
    HRESULT hrc = S_OK;
    HRESULT hrc2;

    /*
     * Unregister the type library.
     *
     * We ignore TYPE_E_REGISTRYACCESS as that is what is returned if the
     * type lib hasn't been registered (W10).
     */
    hrc2 = UnRegisterTypeLib(&LIBID_VirtualBox, kTypeLibraryMajorVersion, kTypeLibraryMinorVersion,
                             0 /*LCid*/, RT_CONCAT(SYS_WIN, ARCH_BITS));
    AssertMsgStmt(SUCCEEDED(hrc2) || hrc2 == TYPE_E_REGISTRYACCESS, ("%Rhrc\n", hrc2), if (SUCCEEDED(hrc)) hrc = hrc2);

    /*
     * Unregister the proxy stub.
     *
     * We ignore ERROR_FILE_NOT_FOUND as that is returned if not registered (W10).
     */
    hrc2 = NdrDllUnregisterProxy(g_hDllSelf, &g_apProxyFiles[0], &g_ProxyClsId);      /* see DLLREGISTRY_ROUTINES in RpcProxy.h */
    AssertMsgStmt(   SUCCEEDED(hrc2)
                  || hrc2 == MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ERROR_FILE_NOT_FOUND),
                  ("%Rhrc\n", hrc2), if (SUCCEEDED(hrc)) hrc = hrc2);

    /*
     * Register the VBox modules and classes.
     */
    hrc2 = RegisterXidlModulesAndClasses(NULL, true /*fUnregisterOnly*/);
    AssertMsgStmt(SUCCEEDED(hrc2), ("%Rhrc\n", hrc2), if (SUCCEEDED(hrc)) hrc = hrc2);

#ifdef WITH_MANUAL_CLEANUP
    /*
     * Purge old mess.
     */
    removeOldMess();
#endif

    return hrc;
}

#ifdef WITH_MANUAL_CLEANUP

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
    LONG rc = RegOpenKeyExA(hkeyClassesRoot, "CLSID", 0, DELETE | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE,
                            &hkeyClsId);
    if (rc == ERROR_SUCCESS)
    {
        DWORD idxKey;
        for (idxKey = 0;; idxKey++)
        {
            char szCurNm[128 + 128];
            DWORD cbCurNm = 128;
            rc = RegEnumKeyExA(hkeyClsId, idxKey, szCurNm, &cbCurNm, NULL, NULL, NULL, NULL);
            if (rc == ERROR_SUCCESS)
            {
                /*
                 * Get the typelib GUID and program ID with the class ID.
                 */
                HKEY hkeyIfTypelib;
                strcpy(&szCurNm[cbCurNm], "\\TypeLib");
                rc = RegOpenKeyExA(hkeyClsId, szCurNm, 0, KEY_QUERY_VALUE, &hkeyIfTypelib);
                if (rc == ERROR_SUCCESS)
                {
                    char szTypelibGuid[128];
                    DWORD cbValue = sizeof(szTypelibGuid) - 1;
                    rc = RegQueryValueExA(hkeyIfTypelib, NULL, NULL, NULL, (PBYTE)&szTypelibGuid[0], &cbValue);
                    if (rc != ERROR_SUCCESS)
                        cbValue = 0;
                    szTypelibGuid[cbValue] = '\0';
                    RegCloseKey(hkeyIfTypelib);
                    if (isTypelibGuidToRemove(szTypelibGuid))
                    {
                        /* ProgId */
                        HKEY hkeyIfProgId;
                        strcpy(&szCurNm[cbCurNm], "\\ProgId");
                        rc = RegOpenKeyExA(hkeyClsId, szCurNm, 0, KEY_QUERY_VALUE, &hkeyIfProgId);
                        if (rc == ERROR_SUCCESS)
                        {
                            char szProgId[64];
                            cbValue = sizeof(szProgId) - 1;
                            rc = RegQueryValueExA(hkeyIfProgId, NULL, NULL, NULL, (PBYTE)&szProgId[0], &cbValue);
                            if (rc != ERROR_SUCCESS)
                                cbValue = 0;
                            szProgId[cbValue] = '\0';
                            RegCloseKey(hkeyIfProgId);
                            if (   rc == ERROR_SUCCESS
                                && strnicmp(szProgId, RT_STR_TUPLE("VirtualBox.")) == 0)
                            {
                                /*
                                 * Ok, it's an orphaned VirtualBox interface. Delete it.
                                 */
                                szCurNm[cbCurNm] = '\0';
#ifdef DEBUG_bird
                                RTAssertMsg2("Should delete HCR/CLSID/%s\n", szCurNm);
#endif
                                rc = SHDeleteKeyA(hkeyClsId, szCurNm);
                                Assert(rc == ERROR_SUCCESS);
                            }
                        }
                    }
                }

            }
            else
            {
                Assert(rc == ERROR_NO_MORE_ITEMS);
                break;
            }
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
    LONG rc = RegOpenKeyExA(hkeyClassesRoot, "Interface", 0, DELETE | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE,
                            &hkeyInterface);
    if (rc == ERROR_SUCCESS)
    {
        DWORD idxKey;
        for (idxKey = 0;; idxKey++)
        {
            char szCurNm[128 + 128];
            DWORD cbCurNm = 128;
            rc = RegEnumKeyExA(hkeyInterface, idxKey, szCurNm, &cbCurNm, NULL, NULL, NULL, NULL);
            if (rc == ERROR_SUCCESS)
            {
                /*
                 * Get the typelib GUID and version associated with the interface.
                 */
                HKEY hkeyIfTypelib;
                strcpy(&szCurNm[cbCurNm], "\\TypeLib");
                rc = RegOpenKeyExA(hkeyInterface, szCurNm, 0, KEY_QUERY_VALUE, &hkeyIfTypelib);
                if (rc == ERROR_SUCCESS)
                {
                    char szTypelibGuid[128];
                    DWORD cbValue = sizeof(szTypelibGuid) - 1;
                    rc = RegQueryValueExA(hkeyIfTypelib, 0, NULL, NULL, (PBYTE)&szTypelibGuid[0], &cbValue);
                    if (rc != ERROR_SUCCESS)
                        cbValue = 0;
                    szTypelibGuid[cbValue] = '\0';
                    if (   rc == ERROR_SUCCESS
                        && isTypelibGuidToRemove(szTypelibGuid))
                    {
                        char szTypelibVer[64];
                        cbValue = sizeof(szTypelibVer) - 1;
                        rc = RegQueryValueExA(hkeyIfTypelib, "Version", 0, NULL, (PBYTE)&szTypelibVer[0], &cbValue);
                        if (rc != ERROR_SUCCESS)
                            cbValue = 0;
                        szTypelibVer[cbValue] = '\0';

                        if (   rc == ERROR_SUCCESS
                            && isTypelibVersionToRemove(szTypelibVer))
                        {
                            /*
                             * Ok, it's an orphaned VirtualBox interface. Delete it.
                             */
                            szCurNm[cbCurNm] = '\0';
                            RTAssertMsg2("Should delete HCR/Interface/%s\n", szCurNm);
                            rc = SHDeleteKeyA(hkeyInterface, szCurNm);
                            Assert(rc == ERROR_SUCCESS);
                        }
                    }
                    RegCloseKey(hkeyIfTypelib);
                }
            }
            else
            {
                Assert(rc == ERROR_NO_MORE_ITEMS);
                break;
            }
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
        LONG rc = RegOpenKeyExA(hkeyClassesRoot, g_apszTypelibGuidKeys[i], 0,
                                DELETE | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hkeyTyplib);
        if (rc == ERROR_SUCCESS)
        {
            unsigned iVer = RT_ELEMENTS(g_apszTypelibVersions);
            while (iVer-- > 0)
            {
                HKEY hkeyVer;
                rc = RegOpenKeyExA(hkeyTyplib, g_apszTypelibVersions[iVer], 0, KEY_READ, &hkeyVer);
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
                            RTAssertMsg2("Should delete HCR\\%s\\%s\n", g_apszTypelibGuidKeys[i], g_apszTypelibVersions[iVer]);
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
    HKEY hkeyWow64;
    LONG rc;

    /*
     * The standard location.
     */
    removeOldTypelib(HKEY_CLASSES_ROOT);
    removeOldInterfaces(HKEY_CLASSES_ROOT);
    removeOldClassIDs(HKEY_CLASSES_ROOT);

    /*
     * Wow64 if present.
     */
    rc = RegOpenKeyExA(HKEY_CLASSES_ROOT, "Wow6432Node", 0, DELETE | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hkeyWow64);
    if (rc == ERROR_SUCCESS)
    {
        removeOldTypelib(hkeyWow64);
        removeOldInterfaces(hkeyWow64);
        removeOldClassIDs(hkeyWow64);

        RegCloseKey(hkeyWow64);
    }
}

#endif /* WITH_MANUAL_CLEANUP */

