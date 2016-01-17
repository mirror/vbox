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
#include <iprt/nt/nt-and-windows.h>
#include <rpcproxy.h>
#include <Shlwapi.h>
#include <stdio.h>

#include "VirtualBox.h"
#include <iprt/alloca.h>
#include <iprt/assert.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/uuid.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** @def WITH_MANUAL_CLEANUP
 * Manually clean up the registry. */
#if 1 /*defined(DEBUG) && !defined(VBOX_IN_32_ON_64_MAIN_API) - needed on testboxes (and obviously dev boxes)! */
# define WITH_MANUAL_CLEANUP
#endif

#ifdef DEBUG_bird
# define VBSP_LOG_ENABLED
#endif

#ifdef VBSP_LOG_ENABLED
# define VBSP_LOG_VALUE_CHANGE(a)   RTAssertMsg2 a
#else
# define VBSP_LOG_VALUE_CHANGE(a)   do { } while (0)
#endif

#ifdef VBSP_LOG_ENABLED
# define VBSP_LOG_SET_VALUE(a)      RTAssertMsg2 a
#else
# define VBSP_LOG_SET_VALUE(a)      do { } while (0)
#endif

#ifdef VBSP_LOG_ENABLED
# define VBSP_LOG_NEW_KEY(a)        RTAssertMsg2 a
#else
# define VBSP_LOG_NEW_KEY(a)        do { } while (0)
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
            /* Save the DLL handle so we can get the path to this DLL during
               registration and updating. */
            g_hDllSelf = hInstance;

            /* We don't need callbacks for thread creation and destruction. */
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
}


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


#ifdef VBSP_LOG_ENABLED
# include <iprt/asm.h>

/** For logging full key names.   */
static PCRTUTF16 vbpsDebugKeyToWSZ(HKEY hKey)
{
    static union
    {
        KEY_NAME_INFORMATION NameInfo;
        WCHAR awchPadding[260];
    }                           s_aBufs[4];
    static uint32_t volatile    iNext = 0;
    uint32_t                    i = ASMAtomicIncU32(&iNext) % RT_ELEMENTS(s_aBufs);
    ULONG                       cbRet = 0;
    NTSTATUS                    rcNt;

    memset(&s_aBufs[i], 0, sizeof(s_aBufs[i]));
    rcNt = NtQueryKey(hKey, KeyNameInformation, &s_aBufs[i], sizeof(s_aBufs[i]) - sizeof(WCHAR), &cbRet);
    if (!NT_SUCCESS(rcNt))
        s_aBufs[i].NameInfo.NameLength = 0;
    s_aBufs[i].NameInfo.Name[s_aBufs[i].NameInfo.NameLength] = '\0';
    return s_aBufs[i].NameInfo.Name;
}
#endif

/**
 * Registry modifier state.
 */
typedef struct VBPSREGSTATE
{
    /** Where the classes and stuff are to be registered. */
    HKEY hkeyClassesRootDst;
    /** The handle to the CLSID key under hkeyClassesRootDst. */
    HKEY hkeyClsidRootDst;
    /** The handle to the Interface key under hkeyClassesRootDst. */
    HKEY hkeyInterfaceRootDst;

    /** Alternative locations where data needs to be deleted, but never updated.  */
    struct
    {
        /** The classes root key handle. */
        HKEY hkeyClasses;
        /** The classes/CLSID key handle. */
        HKEY hkeyClsid;
        /** The classes/Interface key handle. */
        HKEY hkeyInterface;
    } aAltDeletes[3];
    /** Alternative delete locations. */
    uint32_t cAltDeletes;

    /** The current total result. */
    LSTATUS rc;

    /** KEY_WOW64_32KEY, KEY_WOW64_64KEY or 0 (for default).  Allows doing all
     * almost the work from one process (at least W7+ due to aliases). */
    DWORD   fSamWow;
    /** Desired key access when only deleting. */
    DWORD   fSamDelete;
    /** Desired key access when only doing updates. */
    DWORD   fSamUpdate;
    /** Desired key access when both deleting and updating. */
    DWORD   fSamBoth;
    /** Whether to delete registrations first. */
    bool    fDelete;
    /** Whether to update registry value and keys. */
    bool    fUpdate;

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
 * @param   hkeyAltRoot     The registry root tree constant for the alternative
 *                          registrations (remove only).
 * @param   pszAltSubRoot   The path to where classes could also be registered,
 *                          but shouldn't be in our setup.
 * @param   fDelete         Whether to delete registrations first.
 * @param   fUpdate         Whether to update registrations.
 * @param   fSamWow         KEY_WOW64_32KEY or 0.
 */
static LSTATUS vbpsRegInit(VBPSREGSTATE *pState, HKEY hkeyRoot, const char *pszSubRoot, bool fDelete, bool fUpdate, DWORD fSamWow)
{
    LSTATUS rc;
    unsigned i = 0;

    /*
     * Initialize the whole structure first so we can safely call vbpsRegTerm on failure.
     */
    pState->hkeyClassesRootDst          = NULL;
    pState->hkeyClsidRootDst            = NULL;
    pState->hkeyInterfaceRootDst        = NULL;
    for (i = 0; i < RT_ELEMENTS(pState->aAltDeletes); i++)
    {
        pState->aAltDeletes[i].hkeyClasses   = NULL;
        pState->aAltDeletes[i].hkeyClsid     = NULL;
        pState->aAltDeletes[i].hkeyInterface = NULL;
    }
    pState->cAltDeletes                 = 0;
    pState->rc                          = ERROR_SUCCESS;
    pState->fDelete                     = fDelete;
    pState->fUpdate                     = fUpdate;
    pState->fSamWow                     = fSamWow;
    pState->fSamDelete                  = 0;
    if (fDelete)
        pState->fSamDelete = pState->fSamWow | DELETE | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE
                           | STANDARD_RIGHTS_READ | STANDARD_RIGHTS_WRITE;
    pState->fSamUpdate                  = 0;
    if (fUpdate)
        pState->fSamUpdate = pState->fSamWow | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_CREATE_SUB_KEY
                           | STANDARD_RIGHTS_READ | STANDARD_RIGHTS_WRITE;
    pState->fSamBoth                    = pState->fSamDelete | pState->fSamUpdate;

    /*
     * Open the root keys.
     */
    rc = RegOpenKeyExA(hkeyRoot, pszSubRoot, 0 /*fOptions*/, pState->fSamBoth, &pState->hkeyClassesRootDst);
    AssertMsgReturn(rc == ERROR_SUCCESS, ("%u\n", rc), pState->rc = rc);
    rc = RegOpenKeyExA(pState->hkeyClassesRootDst, "CLSID", 0 /*fOptions*/, pState->fSamBoth, &pState->hkeyClsidRootDst);
    AssertMsgReturn(rc == ERROR_SUCCESS, ("%u\n", rc), pState->rc = rc);

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
    if (pState->hkeyInterfaceRootDst)
    {
        rc = RegCloseKey(pState->hkeyInterfaceRootDst);
        Assert(rc == ERROR_SUCCESS);
        pState->hkeyInterfaceRootDst = NULL;
    }

    while (pState->cAltDeletes > 0 && pState->cAltDeletes <= RT_ELEMENTS(pState->aAltDeletes))
    {
        unsigned i = --pState->cAltDeletes;
        if (pState->aAltDeletes[i].hkeyClasses)
        {
            rc = RegCloseKey(pState->aAltDeletes[i].hkeyClasses);
            Assert(rc == ERROR_SUCCESS);
            pState->aAltDeletes[i].hkeyClasses = NULL;
        }
        if (pState->aAltDeletes[i].hkeyClsid)
        {
            rc = RegCloseKey(pState->aAltDeletes[i].hkeyClsid);
            Assert(rc == ERROR_SUCCESS);
            pState->aAltDeletes[i].hkeyClsid = NULL;
        }
        if (pState->aAltDeletes[i].hkeyInterface)
        {
            rc = RegCloseKey(pState->aAltDeletes[i].hkeyInterface);
            Assert(rc == ERROR_SUCCESS);
            pState->aAltDeletes[i].hkeyInterface = NULL;
        }
    }
}


/**
 * Add an alternative registry classes tree from which to remove keys.
 *
 * @returns ERROR_SUCCESS if we successfully opened the destination root, other
 *          wise windows error code (remebered).
 * @param   pState              The registry modifier state.
 * @param   hkeyAltRoot         The root of the alternate registry classes
 *                              location.
 * @param   pszAltSubRoot       The path to the 'classes' sub-key, or NULL if
 *                              hkeyAltRoot is it.
 */
static LSTATUS vbpsRegAddAltDelete(VBPSREGSTATE *pState, HKEY hkeyAltRoot, const char *pszAltSubRoot)
{
    unsigned i;
    LSTATUS rc;

    /* Ignore call if not in delete mode. */
    if (!pState->fDelete)
        return ERROR_SUCCESS;

    /* Check that there is space in the state. */
    i = pState->cAltDeletes;
    AssertReturn(i < RT_ELEMENTS(pState->aAltDeletes), pState->rc = ERROR_TOO_MANY_NAMES);


    /* Open the root. */
    rc = RegOpenKeyExA(hkeyAltRoot, pszAltSubRoot, 0 /*fOptions*/, pState->fSamDelete,
                       &pState->aAltDeletes[i].hkeyClasses);
    if (rc == ERROR_SUCCESS)
    {
        /* Try open the CLSID subkey, it's fine if it doesn't exists. */
        rc = RegOpenKeyExA(pState->aAltDeletes[i].hkeyClasses, "CLSID", 0 /*fOptions*/, pState->fSamDelete,
                           &pState->aAltDeletes[i].hkeyClsid);
        if (rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND)
        {
            if (rc == ERROR_FILE_NOT_FOUND)
                pState->aAltDeletes[i].hkeyClsid = NULL;
            pState->cAltDeletes = i + 1;
            return ERROR_SUCCESS;
        }
        AssertMsgFailed(("%u\n", rc));
        RegCloseKey(pState->aAltDeletes[i].hkeyClasses);
    }
    /* No need to add non-existing alternative roots, nothing to delete in the void. */
    else if (rc == ERROR_FILE_NOT_FOUND)
        rc = ERROR_SUCCESS;
    else
    {
        AssertMsgFailed(("%u\n", rc));
        pState->rc = rc;
    }

    pState->aAltDeletes[i].hkeyClasses = NULL;
    pState->aAltDeletes[i].hkeyClsid   = NULL;
    return rc;
}


/**
 * Open the 'Interface' keys under the current classes roots.
 *
 * We don't do this during vbpsRegInit as it's only needed for updating.
 *
 * @returns ERROR_SUCCESS if we successfully opened the destination root, other
 *          wise windows error code (remebered).
 * @param   pState              The registry modifier state.
 */
static LSTATUS vbpsRegOpenInterfaceKeys(VBPSREGSTATE *pState)
{
    unsigned i;
    LSTATUS rc;

    /*
     * Under the root destination.
     */
    if (pState->hkeyInterfaceRootDst == NULL)
    {
        rc = RegOpenKeyExA(pState->hkeyClassesRootDst, "Interface", 0 /*fOptions*/, pState->fSamBoth, &pState->hkeyInterfaceRootDst);
        AssertMsgReturnStmt(rc == ERROR_SUCCESS, ("%u\n", rc), pState->hkeyInterfaceRootDst = NULL,  pState->rc = rc);
    }

    /*
     * Under the alternative delete locations.
     */
    i = pState->cAltDeletes;
    while (i-- > 0)
        if (pState->aAltDeletes[i].hkeyInterface == NULL)
        {
            rc = RegOpenKeyExA(pState->aAltDeletes[i].hkeyClasses, "Interface", 0 /*fOptions*/, pState->fSamDelete,
                               &pState->aAltDeletes[i].hkeyInterface);
            if (rc != ERROR_SUCCESS)
            {
                AssertMsgStmt(rc == ERROR_FILE_NOT_FOUND, ("%u\n", rc), pState->rc = rc);
                pState->aAltDeletes[i].hkeyInterface = NULL;
            }
        }

    return ERROR_SUCCESS;
}


/** The destination buffer size required by vbpsFormatUuidInCurly. */
#define CURLY_UUID_STR_BUF_SIZE     40

/**
 * Formats a UUID to a string, inside curly braces.
 *
 * @returns @a pszString
 * @param   pszString           Output buffer of size CURLY_UUID_STR_BUF_SIZE.
 * @param   pUuidIn             The UUID to format.
 */
static const char *vbpsFormatUuidInCurly(char pszString[CURLY_UUID_STR_BUF_SIZE], const CLSID *pUuidIn)
{
    static const char s_achDigits[17] = "0123456789abcdef";
    PCRTUUID pUuid = (PCRTUUID)pUuidIn;
    uint32_t u32TimeLow;
    unsigned u;

    pszString[ 0] = '{';
    u32TimeLow = RT_H2LE_U32(pUuid->Gen.u32TimeLow);
    pszString[ 1] = s_achDigits[(u32TimeLow >> 28)/*& 0xf*/];
    pszString[ 2] = s_achDigits[(u32TimeLow >> 24) & 0xf];
    pszString[ 3] = s_achDigits[(u32TimeLow >> 20) & 0xf];
    pszString[ 4] = s_achDigits[(u32TimeLow >> 16) & 0xf];
    pszString[ 5] = s_achDigits[(u32TimeLow >> 12) & 0xf];
    pszString[ 6] = s_achDigits[(u32TimeLow >>  8) & 0xf];
    pszString[ 7] = s_achDigits[(u32TimeLow >>  4) & 0xf];
    pszString[ 8] = s_achDigits[(u32TimeLow/*>>0*/)& 0xf];
    pszString[ 9] = '-';
    u = RT_H2LE_U16(pUuid->Gen.u16TimeMid);
    pszString[10] = s_achDigits[(u >> 12)/*& 0xf*/];
    pszString[11] = s_achDigits[(u >>  8) & 0xf];
    pszString[12] = s_achDigits[(u >>  4) & 0xf];
    pszString[13] = s_achDigits[(u/*>>0*/)& 0xf];
    pszString[14] = '-';
    u = RT_H2LE_U16(pUuid->Gen.u16TimeHiAndVersion);
    pszString[15] = s_achDigits[(u >> 12)/*& 0xf*/];
    pszString[16] = s_achDigits[(u >>  8) & 0xf];
    pszString[17] = s_achDigits[(u >>  4) & 0xf];
    pszString[18] = s_achDigits[(u/*>>0*/)& 0xf];
    pszString[19] = '-';
    pszString[20] = s_achDigits[pUuid->Gen.u8ClockSeqHiAndReserved >> 4];
    pszString[21] = s_achDigits[pUuid->Gen.u8ClockSeqHiAndReserved & 0xf];
    pszString[22] = s_achDigits[pUuid->Gen.u8ClockSeqLow >> 4];
    pszString[23] = s_achDigits[pUuid->Gen.u8ClockSeqLow & 0xf];
    pszString[24] = '-';
    pszString[25] = s_achDigits[pUuid->Gen.au8Node[0] >> 4];
    pszString[26] = s_achDigits[pUuid->Gen.au8Node[0] & 0xf];
    pszString[27] = s_achDigits[pUuid->Gen.au8Node[1] >> 4];
    pszString[28] = s_achDigits[pUuid->Gen.au8Node[1] & 0xf];
    pszString[29] = s_achDigits[pUuid->Gen.au8Node[2] >> 4];
    pszString[30] = s_achDigits[pUuid->Gen.au8Node[2] & 0xf];
    pszString[31] = s_achDigits[pUuid->Gen.au8Node[3] >> 4];
    pszString[32] = s_achDigits[pUuid->Gen.au8Node[3] & 0xf];
    pszString[33] = s_achDigits[pUuid->Gen.au8Node[4] >> 4];
    pszString[34] = s_achDigits[pUuid->Gen.au8Node[4] & 0xf];
    pszString[35] = s_achDigits[pUuid->Gen.au8Node[5] >> 4];
    pszString[36] = s_achDigits[pUuid->Gen.au8Node[5] & 0xf];
    pszString[37] = '}';
    pszString[38] = '\0';

    return pszString;

}


/**
 * Sets a registry string value, wide char variant.
 *
 * @returns See RegSetValueExA (errors are remembered in the state).
 * @param   pState              The registry modifier state.
 * @param   hkey                The key to add the value to.
 * @param   pwszValueNm         The value name. NULL for setting the default.
 * @param   pwszValue           The value string.
 * @param   uLine               The line we're called from.
 */
static LSTATUS vbpsSetRegValueWW(VBPSREGSTATE *pState, HKEY hkey, PCRTUTF16 pwszValueNm, PCRTUTF16 pwszValue, unsigned uLine)
{
    DWORD const cbValue = (DWORD)((RTUtf16Len(pwszValue) + 1) * sizeof(RTUTF16));
    LSTATUS rc;
    Assert(pState->fUpdate);

    /*
     * If we're not deleting the key prior to updating, we're in gentle update
     * mode where we will query if the existing value matches the incoming one.
     */
    if (!pState->fDelete)
    {
        DWORD       cbExistingData   = cbValue + 128;
        PRTUTF16    pwszExistingData = (PRTUTF16)alloca(cbExistingData);
        DWORD       dwExistingType;
        rc = RegQueryValueExW(hkey, pwszValueNm, 0 /*Reserved*/, &dwExistingType, (BYTE *)pwszExistingData, &cbExistingData);
        if (rc == ERROR_SUCCESS)
        {
            if (   dwExistingType == REG_SZ
                && cbExistingData == cbValue)
            {
                if (memcmp(pwszValue, pwszExistingData, cbValue) == 0)
                    return ERROR_SUCCESS;
            }
            VBSP_LOG_VALUE_CHANGE(("vbpsSetRegValueWW: Value difference: dwExistingType=%d cbExistingData=%#x cbValue=%#x\n"
                                   " hkey=%#x %ls; value name=%ls\n"
                                   "existing: %.*Rhxs (%.*ls)\n"
                                   "     new: %.*Rhxs (%ls)\n",
                                   dwExistingType, cbExistingData, cbValue,
                                   hkey, vbpsDebugKeyToWSZ(hkey), pwszValueNm ? pwszValueNm : L"(default)",
                                   cbExistingData, pwszExistingData, cbExistingData / sizeof(RTUTF16), pwszExistingData,
                                   cbValue, pwszValue, pwszValue));
        }
        else
            Assert(rc == ERROR_FILE_NOT_FOUND || rc == ERROR_MORE_DATA);
    }

    /*
     * Set the value.
     */
    rc = RegSetValueExW(hkey, pwszValueNm, 0 /*Reserved*/, REG_SZ, (const BYTE *)pwszValue, cbValue);
    if (rc == ERROR_SUCCESS)
    {
        VBSP_LOG_SET_VALUE(("vbpsSetRegValueWW: %ls/%ls=%ls (at %d)\n",
                            vbpsDebugKeyToWSZ(hkey), pwszValueNm ? pwszValueNm : L"(Default)", pwszValue, uLine));
        return ERROR_SUCCESS;
    }

    AssertMsgFailed(("%d: '%ls'='%ls' -> %u\n", uLine, pwszValueNm, pwszValue, rc));
    pState->rc = rc;
    return rc;
}


/**
 * Sets a registry string value.
 *
 * @returns See RegSetValueExA (errors are remembered in the state).
 * @param   pState              The registry modifier state.
 * @param   hkey                The key to add the value to.
 * @param   pszValueNm          The value name. NULL for setting the default.
 * @param   pszValue            The value string.
 * @param   uLine               The line we're called from.
 */
static LSTATUS vbpsSetRegValueAA(VBPSREGSTATE *pState, HKEY hkey, const char *pszValueNm, const char *pszValue, unsigned uLine)
{
    DWORD const cbValue = (DWORD)strlen(pszValue) + 1;
    LSTATUS rc;
    Assert(pState->fUpdate);

    /*
     * If we're not deleting the key prior to updating, we're in gentle update
     * mode where we will query if the existing value matches the incoming one.
     */
    if (!pState->fDelete)
    {
        DWORD cbExistingData = cbValue + 128;
        char *pszExistingData = alloca(cbExistingData);
        DWORD dwExistingType;
        rc = RegQueryValueExA(hkey, pszValueNm, 0 /*Reserved*/, &dwExistingType, pszExistingData, &cbExistingData);
        if (rc == ERROR_SUCCESS)
        {
            if (   dwExistingType == REG_SZ
                && cbExistingData == cbValue)
            {
                if (memcmp(pszValue, pszExistingData, cbValue) == 0)
                    return ERROR_SUCCESS;
                if (memicmp(pszValue, pszExistingData, cbValue) == 0)
                    return ERROR_SUCCESS;
            }
            VBSP_LOG_VALUE_CHANGE(("vbpsSetRegValueAA: Value difference: dwExistingType=%d cbExistingData=%#x cbValue=%#x\n"
                                   " hkey=%#x %ls; value name=%s\n"
                                   "existing: %.*Rhxs (%.*s)\n"
                                   "     new: %.*Rhxs (%s)\n",
                                   dwExistingType, cbExistingData, cbValue,
                                   hkey, vbpsDebugKeyToWSZ(hkey), pszValueNm ? pszValueNm : "(default)",
                                   cbExistingData, pszExistingData, cbExistingData, pszExistingData,
                                   cbValue, pszValue, pszValue));
        }
        else
            Assert(rc == ERROR_FILE_NOT_FOUND || rc == ERROR_MORE_DATA);
    }

    /*
     * Set the value.
     */
    rc = RegSetValueExA(hkey, pszValueNm, 0 /*Reserved*/, REG_SZ, pszValue, cbValue);
    if (rc == ERROR_SUCCESS)
    {
        VBSP_LOG_SET_VALUE(("vbpsSetRegValueAA: %ls/%s=%s (at %d)\n",
                            vbpsDebugKeyToWSZ(hkey), pszValueNm ? pszValueNm : "(Default)", pszValue, uLine));
        return ERROR_SUCCESS;
    }

    AssertMsgFailed(("%d: '%s'='%s' -> %u\n", uLine, pszValueNm, pszValue, rc));
    pState->rc = rc;
    return rc;
}


/**
 * Closes a registry key.
 *
 * @returns See RegCloseKey (errors are remembered in the state).
 * @param   pState              The registry modifier state.
 * @param   hkey                The key to close.
 * @param   uLine               The line we're called from.
 */
static LSTATUS vbpsCloseKey(VBPSREGSTATE *pState, HKEY hkey, unsigned uLine)
{
    LSTATUS rc = RegCloseKey(hkey);
    if (rc == ERROR_SUCCESS)
        return ERROR_SUCCESS;

    AssertMsgFailed(("%d: close key -> %u\n", uLine, rc));
    pState->rc = rc;
    return rc;
}


/**
 * Creates a registry key.
 *
 * @returns See RegCreateKeyA and RegSetValueExA (errors are remembered in the
 *          state).
 * @param   pState              The registry modifier state.
 * @param   hkeyParent          The parent key.
 * @param   pszKey              The new key under @a hkeyParent.
 * @param   phkey               Where to return the handle to the new key.
 * @param   uLine               The line we're called from.
 */
static LSTATUS vbpsCreateRegKeyA(VBPSREGSTATE *pState, HKEY hkeyParent, const char *pszKey, PHKEY phkey, unsigned uLine)
{
    /*
     * This will open if it exists and create if new, which is exactly what we want.
     */
    HKEY hNewKey;
    DWORD dwDisposition = 0;
    LSTATUS rc = RegCreateKeyExA(hkeyParent, pszKey, 0 /*Reserved*/, NULL /*pszClass*/, 0 /*fOptions*/,
                                 pState->fSamBoth, NULL /*pSecAttr*/, &hNewKey, &dwDisposition);
    if (rc == ERROR_SUCCESS)
    {
        *phkey = hNewKey;
        if (dwDisposition == REG_CREATED_NEW_KEY)
            VBSP_LOG_NEW_KEY(("vbpsCreateRegKeyA: %ls/%s (at %d)\n", vbpsDebugKeyToWSZ(hkeyParent), pszKey, uLine));
    }
    else
    {
        AssertMsgFailed(("%d: create key '%s' -> %u\n", uLine, pszKey,  rc));
        pState->rc = rc;
        *phkey = NULL;
    }
    return rc;
}


/**
 * Creates a registry key with a default string value.
 *
 * @returns See RegCreateKeyA and RegSetValueExA (errors are remembered in the
 *          state).
 * @param   pState              The registry modifier state.
 * @param   hkeyParent          The parent key.
 * @param   pszKey              The new key under @a hkeyParent.
 * @param   pszValue            The value string.
 * @param   uLine               The line we're called from.
 */
static LSTATUS vbpsCreateRegKeyWithDefaultValueAA(VBPSREGSTATE *pState, HKEY hkeyParent, const char *pszKey,
                                                  const char *pszValue, unsigned uLine)
{
    HKEY hNewKey;
    LSTATUS rc = vbpsCreateRegKeyA(pState, hkeyParent, pszKey, &hNewKey, uLine);
    if (rc == ERROR_SUCCESS)
    {
        rc = vbpsSetRegValueAA(pState, hNewKey, NULL /*pszValueNm*/, pszValue, uLine);
        vbpsCloseKey(pState, hNewKey, uLine);
    }
    else
    {
        AssertMsgFailed(("%d: create key '%s'(/Default='%s') -> %u\n", uLine, pszKey, pszValue, rc));
        pState->rc = rc;
    }
    return rc;
}


/**
 * Creates a registry key with a default wide string value.
 *
 * @returns See RegCreateKeyA and RegSetValueExA (errors are remembered in the
 *          state).
 * @param   pState              The registry modifier state.
 * @param   hkeyParent          The parent key.
 * @param   pszKey              The new key under @a hkeyParent.
 * @param   pwszValue           The value string.
 * @param   uLine               The line we're called from.
 */
static LSTATUS vbpsCreateRegKeyWithDefaultValueAW(VBPSREGSTATE *pState, HKEY hkeyParent, const char *pszKey,
                                                  PCRTUTF16 pwszValue, unsigned uLine)
{
    HKEY hNewKey;
    LSTATUS rc = vbpsCreateRegKeyA(pState, hkeyParent, pszKey, &hNewKey, uLine);
    if (rc == ERROR_SUCCESS)
    {
        rc = vbpsSetRegValueWW(pState, hNewKey, NULL /*pwszValueNm*/, pwszValue, uLine);
        vbpsCloseKey(pState, hNewKey, uLine);
    }
    else
    {
        AssertMsgFailed(("%d: create key '%s'(/Default='%ls') -> %u\n", uLine, pszKey, pwszValue, rc));
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
 * @param   hkeyParent          The parent key.
 * @param   pszKey              The new key under @a hkeyParent.
 * @param   pszValue            The value string.
 * @param   phkey               Where to return the handle to the new key.
 * @param   uLine               The line we're called from.
 */
static LSTATUS vbpsCreateRegKeyWithDefaultValueAAEx(VBPSREGSTATE *pState, HKEY hkeyParent, const char *pszKey,
                                                    const char *pszValue, PHKEY phkey, unsigned uLine)
{
    HKEY hNewKey;
    LSTATUS rc = vbpsCreateRegKeyA(pState, hkeyParent, pszKey, &hNewKey, uLine);
    if (rc == ERROR_SUCCESS)
    {
        rc = vbpsSetRegValueAA(pState, hNewKey, NULL /*pszValueNm*/, pszValue, uLine);
        *phkey = hNewKey;
    }
    else
    {
        AssertMsgFailed(("%d: create key '%s'(/Default='%s') -> %u\n", uLine, pszKey, pszValue, rc));
        pState->rc = rc;
        *phkey = NULL;
    }
    return rc;
}


/**
 * Recursively deletes a registry key.
 *
 * @returns See SHDeleteKeyA (errors are remembered in the state).
 * @param   pState              The registry modifier state.
 * @param   hkeyParent          The parent key.
 * @param   pszKey              The key under @a hkeyParent that should be
 *                              deleted.
 * @param   uLine               The line we're called from.
 */
static LSTATUS vbpsDeleteKeyRecursiveA(VBPSREGSTATE *pState, HKEY hkeyParent, const char *pszKey, unsigned uLine)
{
    LSTATUS rc;

    Assert(pState->fDelete);
    Assert(pszKey);
    AssertReturn(*pszKey != '\0', pState->rc = ERROR_INVALID_PARAMETER);

    rc = SHDeleteKeyA(hkeyParent, pszKey);
    if (rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND)
        return ERROR_SUCCESS;

    AssertMsgFailed(("%d: delete key '%s' -> %u\n", uLine, pszKey, rc));
    pState->rc = rc;
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

    /*
     * Delete.
     */
    if (pState->fDelete)
    {
        unsigned i = pState->cAltDeletes;
        while (i-- > 0)
        {
            rc = RegOpenKeyExW(pState->aAltDeletes[i].hkeyClasses, L"AppID", 0 /*fOptions*/, pState->fSamDelete, &hkeyAppIds);
            AssertMsgStmt(rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND, ("%u\n", rc), pState->rc = rc);
            if (rc == ERROR_SUCCESS)
            {
                vbpsDeleteKeyRecursiveA(pState, hkeyAppIds, pszAppId, __LINE__);
                vbpsCloseKey(pState, hkeyAppIds, __LINE__);
            }
        }
    }

    rc = RegOpenKeyExW(pState->hkeyClassesRootDst, L"AppID", 0 /*fOptions*/, pState->fSamBoth, &hkeyAppIds);
    AssertMsgReturn(rc == ERROR_SUCCESS, ("%u\n", rc), pState->rc = rc);

    if (pState->fDelete)
        vbpsDeleteKeyRecursiveA(pState, hkeyAppIds, pszAppId, __LINE__);

    /*
     * Update.
     */
    if (pState->fUpdate)
        vbpsCreateRegKeyWithDefaultValueAA(pState, hkeyAppIds, pszAppId, pszDescription, __LINE__);

    vbpsCloseKey(pState, hkeyAppIds, __LINE__);

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
     * Delete.
     */
    if (pState->fDelete)
    {
        unsigned i = pState->cAltDeletes;
        while (i-- > 0)
            vbpsDeleteKeyRecursiveA(pState, pState->aAltDeletes[i].hkeyClasses, pszClassName, __LINE__);
        vbpsDeleteKeyRecursiveA(pState, pState->hkeyClassesRootDst, pszClassName, __LINE__);
    }

    /*
     * Update.
     */
    if (pState->fUpdate)
    {
        /* pszClassName/Default = description. */
        HKEY hkeyClass;
        rc = vbpsCreateRegKeyWithDefaultValueAAEx(pState, pState->hkeyClassesRootDst, pszClassName, pszDescription,
                                                  &hkeyClass, __LINE__);
        if (rc == ERROR_SUCCESS)
        {
            char szClsId[CURLY_UUID_STR_BUF_SIZE];

            /* CLSID/Default = pClsId. */
            vbpsCreateRegKeyWithDefaultValueAA(pState, hkeyClass, "CLSID", vbpsFormatUuidInCurly(szClsId, pClsId), __LINE__);

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

            vbpsCloseKey(pState, hkeyClass, __LINE__);
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

    Assert(!pszAppId || *pszAppId == '{');
    Assert((pwszVBoxDir == NULL && !pState->fUpdate) || pwszVBoxDir[RTUtf16Len(pwszVBoxDir) - 1] == '\\');

    /*
     * We need this, whatever we end up having to do.
     */
    vbpsFormatUuidInCurly(szClsId, pClsId);

    /*
     * Delete.
     */
    if (pState->fDelete)
    {
        unsigned i = pState->cAltDeletes;
        while (i-- > 0)
            vbpsDeleteKeyRecursiveA(pState, pState->aAltDeletes[i].hkeyClsid, szClsId, __LINE__);
        vbpsDeleteKeyRecursiveA(pState, pState->hkeyClsidRootDst, szClsId, __LINE__);
    }

    /*
     * Update.
     */
    if (pState->fUpdate)
    {
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

                vbpsSetRegValueWW(pState, hkeyServerType, NULL /*pszValueNm*/, wszModule, __LINE__);

                /* pszServerType/ThreadingModel = pszThreading Model. */
                if (pszThreadingModel)
                    vbpsSetRegValueAA(pState, hkeyServerType, "ThreadingModel", pszThreadingModel, __LINE__);

                vbpsCloseKey(pState, hkeyServerType, __LINE__);
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
            {
                char szTypeLibId[CURLY_UUID_STR_BUF_SIZE];
                vbpsCreateRegKeyWithDefaultValueAA(pState, hkeyClass, "TypeLib",
                                                   vbpsFormatUuidInCurly(szTypeLibId, pTypeLibId), __LINE__);
            }

            vbpsCloseKey(pState, hkeyClass, __LINE__);
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


/**
 * Updates the VBox type lib registration.
 *
 * This is only used when updating COM registrations during com::Initialize.
 * For normal registration and unregistrations we use the RegisterTypeLib and
 * UnRegisterTypeLib APIs.
 *
 * @param   pState              The registry modifier state.
 * @param   pwszVBoxDir         The VirtualBox install directory (unicode),
 *                              trailing slash.
 * @param   fIs32On64           Set if we're registering the 32-bit proxy stub
 *                              on a 64-bit system.
 */
static void vbpsUpdateTypeLibRegistration(VBPSREGSTATE *pState, PCRTUTF16 pwszVBoxDir, bool fIs32On64)
{
    const char * const pszTypeLibDll = !fIs32On64 ? "VBoxProxyStub.dll" : "x86\\VBoxProxyStub-x86.dll";
    const char * const pszWinXx      = !fIs32On64 ? "win64"             : "win32";
    const char * const pszDescription = "VirtualBox Type Library";

    char szTypeLibId[CURLY_UUID_STR_BUF_SIZE];
    HKEY hkeyTypeLibs;
    HKEY hkeyTypeLibId;
    LSTATUS rc;

    Assert(pState->fUpdate && !pState->fDelete);

    /*
     * Type library registration (w/o interfaces).
     */

    /* Open Classes/TypeLib/. */
    rc = vbpsCreateRegKeyA(pState, pState->hkeyClassesRootDst, "TypeLib", &hkeyTypeLibs, __LINE__);
    AssertReturnVoid(rc == ERROR_SUCCESS);

    /* Create TypeLib/{UUID}. */
    rc = vbpsCreateRegKeyA(pState, hkeyTypeLibs, vbpsFormatUuidInCurly(szTypeLibId, &LIBID_VirtualBox), &hkeyTypeLibId, __LINE__);
    if (rc == ERROR_SUCCESS)
    {
        /* {UUID}/Major.Minor/Default = pszDescription. */
        HKEY hkeyMajMin;
        char szMajMin[64];
        sprintf(szMajMin, "%u.%u", kTypeLibraryMajorVersion, kTypeLibraryMinorVersion);
        rc = vbpsCreateRegKeyWithDefaultValueAAEx(pState, hkeyTypeLibId, szMajMin, pszDescription, &hkeyMajMin, __LINE__);
        if (rc == ERROR_SUCCESS)
        {
            RTUTF16 wszBuf[MAX_PATH * 2];
            size_t  off;

            /* {UUID}/Major.Minor/0. */
            HKEY hkey0;
            rc = vbpsCreateRegKeyA(pState, hkeyMajMin, "0", &hkey0, __LINE__);
            if (rc == ERROR_SUCCESS)
            {
                /* {UUID}/Major.Minor/0/winXX/Default = VBoxProxyStub. */
                rc = RTUtf16Copy(wszBuf, MAX_PATH, pwszVBoxDir); AssertRC(rc);
                rc = RTUtf16CatAscii(wszBuf, MAX_PATH * 2, pszTypeLibDll); AssertRC(rc);

                vbpsCreateRegKeyWithDefaultValueAW(pState, hkey0, pszWinXx, wszBuf, __LINE__);
                vbpsCloseKey(pState, hkey0, __LINE__);
            }

            /* {UUID}/Major.Minor/FLAGS */
            vbpsCreateRegKeyWithDefaultValueAA(pState, hkeyMajMin, "FLAGS", "0", __LINE__);

            /* {UUID}/Major.Minor/HELPDIR */
            rc = RTUtf16Copy(wszBuf, MAX_PATH, pwszVBoxDir); AssertRC(rc);
            off = RTUtf16Len(wszBuf);
            while (off > 2 && wszBuf[off - 2] != ':' && RTPATH_IS_SLASH(wszBuf[off - 1]))
                off--;
            wszBuf[off] = '\0';
            vbpsCreateRegKeyWithDefaultValueAW(pState, hkeyMajMin, "HELPDIR", wszBuf, __LINE__);

            vbpsCloseKey(pState, hkeyMajMin, __LINE__);
        }
        vbpsCloseKey(pState, hkeyTypeLibId, __LINE__);
    }
    vbpsCloseKey(pState, hkeyTypeLibs, __LINE__);
}


/**
 * Update the VBox proxy stub registration.
 *
 * This is only used when updating COM registrations during com::Initialize.
 * For normal registration and unregistrations we use the NdrDllRegisterProxy
 * and NdrDllUnregisterProxy.
 *
 * @param   pState              The registry modifier state.
 * @param   pwszVBoxDir         The VirtualBox install directory (unicode),
 *                              trailing slash.
 * @param   fIs32On64           Set if we're registering the 32-bit proxy stub
 *                              on a 64-bit system.
 */
static void vbpsUpdateProxyStubRegistration(VBPSREGSTATE *pState, PCRTUTF16 pwszVBoxDir, bool fIs32On64)
{
    /*
     * Register the proxy stub factory class ID.
     * It's simple compared to the VBox classes, thus all the NULL parameters.
     */
    const char *pszPsDll = !fIs32On64 ? "VBoxProxyStub.dll" : "x86\\VBoxProxyStub-x86.dll";
    Assert(pState->fUpdate && !pState->fDelete);
    VbpsRegisterClassId(pState, &g_ProxyClsId, "PSFactoryBuffer", NULL /*pszAppId*/,
                        NULL /*pszClassName*/, NULL /*pszCurClassNameVerSuffix*/, NULL /*pTypeLibId*/,
                        "InprocServer32", pwszVBoxDir, pszPsDll, "Both");
}


/**
 * Updates the VBox interface registrations.
 *
 * This is only used when updating COM registrations during com::Initialize.
 * For normal registration and unregistrations we use the NdrDllRegisterProxy
 * and NdrDllUnregisterProxy.
 *
 * @param   pState              The registry modifier state.
 * @param   pwszVBoxDir         The VirtualBox install directory (unicode),
 *                              trailing slash.
 * @param   fIs32On64           Set if we're registering the 32-bit proxy stub
 *                              on a 64-bit system.
 */
static void vbpsUpdateInterfaceRegistrations(VBPSREGSTATE *pState)
{
    const ProxyFileInfo **ppProxyFile = &g_apProxyFiles[0];
    const ProxyFileInfo  *pProxyFile;
    LSTATUS               rc;
    char                  szProxyClsId[CURLY_UUID_STR_BUF_SIZE];

    vbpsFormatUuidInCurly(szProxyClsId, &g_ProxyClsId);

    Assert(pState->fUpdate && !pState->fDelete);
    rc = vbpsRegOpenInterfaceKeys(pState);
    AssertReturnVoid(rc == ERROR_SUCCESS);

    /*
     * We walk the proxy file list (even if we only have one).
     */
    while ((pProxyFile = *ppProxyFile++) != NULL)
    {
        const PCInterfaceStubVtblList * const   papStubVtbls  = pProxyFile->pStubVtblList;
        const char * const                     *papszNames    = pProxyFile->pNamesArray;
        unsigned                                iIf           = pProxyFile->TableSize;
        AssertStmt(iIf < 1024, iIf = 0);
        Assert(pProxyFile->TableVersion == 2);

        /*
         * Walk the interfaces in that file, picking data from the various tables.
         */
        while (iIf-- > 0)
        {
            char                szIfId[CURLY_UUID_STR_BUF_SIZE];
            const char * const  pszIfNm  = papszNames[iIf];
            size_t const        cchIfNm  = RT_VALID_PTR(pszIfNm) ? strlen(pszIfNm) : 0;
            char                szMethods[32];
            uint32_t const      cMethods = papStubVtbls[iIf]->header.DispatchTableCount;
            HKEY                hkeyIfId;

            AssertReturnVoidStmt(cchIfNm >= 3 && cchIfNm <= 72, pState->rc = ERROR_INVALID_DATA);

            AssertReturnVoidStmt(cMethods >= 3 && cMethods < 1024, pState->rc = ERROR_INVALID_DATA);
            sprintf(szMethods, "%u", cMethods);

            AssertReturnVoid(rc == ERROR_SUCCESS);

            rc = vbpsCreateRegKeyWithDefaultValueAAEx(pState, pState->hkeyInterfaceRootDst,
                                                      vbpsFormatUuidInCurly(szIfId, papStubVtbls[iIf]->header.piid),
                                                      pszIfNm, &hkeyIfId, __LINE__);
            if (rc == ERROR_SUCCESS)
            {
                vbpsCreateRegKeyWithDefaultValueAA(pState, hkeyIfId, "ProxyStubClsid32", szProxyClsId, __LINE__);
                vbpsCreateRegKeyWithDefaultValueAA(pState, hkeyIfId, "NumMethods", szMethods, __LINE__);
                /** @todo Not having the typelib here means we'll have to fix the orphan cleanup
                 *        code below. */

                vbpsCloseKey(pState, hkeyIfId, __LINE__);
            }
        }
    }
}


static bool vbpsIsUpToDate(VBPSREGSTATE *pState)
{
    /** @todo read some registry key and */
    NOREF(pState);
    return false;
}

static bool vbpsMarkUpToDate(VBPSREGSTATE *pState)
{
    /** @todo write the key vbpsIsUpToDate uses, if pState indicates success. */
    NOREF(pState);
    return false;
}



/**
 * Strips the stub dll name and any x86 subdir off the full DLL path to get a
 * path to the VirtualBox application directory.
 *
 * @param   pwszDllPath     The path to strip, returns will end with a slash.
 */
static void vbpsDllPathToVBoxDir(PRTUTF16 pwszDllPath)
{
    RTUTF16 wc;
    size_t off = RTUtf16Len(pwszDllPath);
    while (   off > 0
           && (   (wc = pwszDllPath[off - 1]) >= 127U
               || !RTPATH_IS_SEP((unsigned char)wc)))
        off--;

#ifdef VBOX_IN_32_ON_64_MAIN_API
    /*
     * The -x86 variant is in a x86 subdirectory, drop it.
     */
    while (   off > 0
           && (   (wc = pwszDllPath[off - 1]) < 127U
               && RTPATH_IS_SEP((unsigned char)wc)))
        off--;
    while (   off > 0
           && (   (wc = pwszDllPath[off - 1]) >= 127U
               || !RTPATH_IS_SEP((unsigned char)wc)))
        off--;
#endif
    pwszDllPath[off] = '\0';
}


/**
 * Wrapper around RegisterXidlModulesAndClassesGenerated for the convenience of
 * the standard registration entry points.
 *
 * @returns COM status code.
 * @param   pwszVBoxDir         The VirtualBox install directory (unicode),
 *                              trailing slash.
 * @param   fDelete             Whether to delete registration keys and values.
 * @param   fUpdate             Whether to update registration keys and values.
 */
HRESULT RegisterXidlModulesAndClasses(PRTUTF16 pwszVBoxDir, bool fDelete, bool fUpdate)
{
#ifdef VBOX_IN_32_ON_64_MAIN_API
    bool const      fIs32On64 = true;
#else
    bool const      fIs32On64 = false;
#endif
    VBPSREGSTATE    State;
    LSTATUS         rc;

    /*
     * Do registration for the current execution mode of the DLL.
     */
    rc = vbpsRegInit(&State, HKEY_CLASSES_ROOT, NULL /* Alt: HKEY_LOCAL_MACHINE, "Software\\Classes", */, fDelete, fUpdate, 0);
    if (rc == ERROR_SUCCESS)
    {
        if (!fUpdate)
        {
            /* When only unregistering, really purge everything twice or trice. :-) */
            vbpsRegAddAltDelete(&State, HKEY_LOCAL_MACHINE, "Software\\Classes");
            vbpsRegAddAltDelete(&State, HKEY_CURRENT_USER,  "Software\\Classes");
            vbpsRegAddAltDelete(&State, HKEY_CLASSES_ROOT,  NULL);
        }

        RegisterXidlModulesAndClassesGenerated(&State, pwszVBoxDir, fIs32On64);
        rc = State.rc;
    }
    vbpsRegTerm(&State);

    /*
     * Translate error code? Return.
     */
    if (rc == ERROR_SUCCESS)
        return S_OK;
    return E_FAIL;
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


/**
 * Register the interfaces proxied by this DLL, and to avoid duplication and
 * minimize work the VBox type library, classes and servers are also registered.
 *
 * This is normally only used by developers via comregister.cmd and the heat.exe
 * tool during MSI creation.  The only situation where users may end up here is
 * if they're playing around or we recommend it as a solution to COM problems.
 * So, no problem if this approach is less gentle, though we leave the cleaning
 * up of orphaned interfaces to DllUnregisterServer.
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
    vbpsDllPathToVBoxDir(wszDllName);
    hrc = RegisterXidlModulesAndClasses(wszDllName, true /*fDelete*/, true /*fUpdate*/);
    AssertMsgReturn(SUCCEEDED(hrc), ("%Rhrc\n", hrc), hrc);

    return S_OK;
}


/**
 * Reverse of DllRegisterServer.
 *
 * This is normally only used by developers via comregister.cmd.  Users may be
 * asked to perform it in order to fix some COM issue.  So, it's OK if we spend
 * some extra time and clean up orphaned interfaces, because developer boxes
 * will end up with a bunch of those as interface UUIDs changes.
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
    hrc2 = RegisterXidlModulesAndClasses(NULL, true /*fDelete*/, false /*fUpdate*/);
    AssertMsgStmt(SUCCEEDED(hrc2), ("%Rhrc\n", hrc2), if (SUCCEEDED(hrc)) hrc = hrc2);

#ifdef WITH_MANUAL_CLEANUP
    /*
     * Purge old mess.
     */
    removeOldMess();
#endif

    return hrc;
}


/**
 * Gently update the COM registrations for VirtualBox.
 *
 * API that com::Initialize (VBoxCOM/initterm.cpp) calls the first time COM is
 * initialized in a process.  ASSUMES that the caller has initialized IPRT.
 *
 * @returns Windows error code.
 */
DECLEXPORT(uint32_t) VbpsUpdateRegistrations(void)
{
    LSTATUS         rc;
    VBPSREGSTATE    State;
#ifdef VBOX_IN_32_ON_64_MAIN_API
    bool const      fIs32On64 = true;
#else
    bool const      fIs32On64 = false;
#endif

    /*
     * Find the VirtualBox application directory first.
     */
    WCHAR wszVBoxDir[MAX_PATH];
    DWORD cwcRet = GetModuleFileNameW(g_hDllSelf, wszVBoxDir, RT_ELEMENTS(wszVBoxDir));
    AssertReturn(cwcRet > 0 && cwcRet < RT_ELEMENTS(wszVBoxDir), ERROR_BUFFER_OVERFLOW);
    vbpsDllPathToVBoxDir(wszVBoxDir);

    /*
     * Update registry entries for the current CPU bitness.
     */
    rc = vbpsRegInit(&State, HKEY_CLASSES_ROOT, NULL, false /*fDelete*/, true /*fUpdate*/, 0);
    if (rc == ERROR_SUCCESS && !vbpsIsUpToDate(&State))
    {
        vbpsUpdateTypeLibRegistration(&State, wszVBoxDir, fIs32On64);
        vbpsUpdateProxyStubRegistration(&State, wszVBoxDir, fIs32On64);
        vbpsUpdateInterfaceRegistrations(&State);
        RegisterXidlModulesAndClassesGenerated(&State, wszVBoxDir, fIs32On64);
        vbpsMarkUpToDate(&State);
        rc = State.rc;
    }
    vbpsRegTerm(&State);


//#if defined(VBOX_IN_32_ON_64_MAIN_API) || (ARCH_BITS == 64 && defined(VBOX_WITH_32_ON_64_MAIN_API))
#ifndef VBOX_IN_32_ON_64_MAIN_API
    /*
     * Update registry entries for the other CPU bitness.
     */
    if (rc == ERROR_SUCCESS)
    {
        //rc = vbpsRegInit(&State, HKEY_CLASSES_ROOT, "Wow6432Node", fDelete, fUpdate, KEY_WOW64_32KEY);
        rc = vbpsRegInit(&State, HKEY_CLASSES_ROOT, NULL, false /*fDelete*/, true /*fUpdate*/,
                         !fIs32On64 ? KEY_WOW64_32KEY : KEY_WOW64_64KEY);
        if (rc == ERROR_SUCCESS && !vbpsIsUpToDate(&State))
        {
            vbpsUpdateTypeLibRegistration(&State, wszVBoxDir, !fIs32On64);
            vbpsUpdateProxyStubRegistration(&State, wszVBoxDir, !fIs32On64);
            vbpsUpdateInterfaceRegistrations(&State);
            RegisterXidlModulesAndClassesGenerated(&State, wszVBoxDir, !fIs32On64);
            vbpsMarkUpToDate(&State);
            rc = State.rc;
        }
        vbpsRegTerm(&State);
    }
#endif

    return VINF_SUCCESS;
}

