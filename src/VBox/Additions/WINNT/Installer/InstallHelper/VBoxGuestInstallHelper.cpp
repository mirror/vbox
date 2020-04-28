/* $Id$ */
/** @file
 * VBoxGuestInstallHelper - Various helper routines for Windows guest installer.
 *                          Works with NSIS 3.x.
 */

/*
 * Copyright (C) 2011-2020 Oracle Corporation
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
#include <iprt/win/windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <strsafe.h>
#pragma warning(push)
#pragma warning(disable: 4995) /* warning C4995: 'lstrcpyA': name was marked as #pragma deprecated */
#include "exdll.h"
#pragma warning(pop)

#include <iprt/err.h>
#include <iprt/initterm.h>
#include <iprt/ldr.h>
#include <iprt/localipc.h>
#include <iprt/mem.h>
#include <iprt/process.h>
#include <iprt/string.h>
#ifdef UNICODE
# include <iprt/utf16.h>
#endif

/* Required structures/defines of VBoxTray. */
#include "../../VBoxTray/VBoxTrayMsg.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define VBOXINSTALLHELPER_EXPORT extern "C" void __declspec(dllexport)


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef DWORD (WINAPI *PFNSFCFILEEXCEPTION)(DWORD param1, PWCHAR param2, DWORD param3);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
HINSTANCE               g_hInstance;
HWND                    g_hwndParent;

/**
 * @todo Clean up this DLL, use more IPRT in here!
 */

/**
 * Pops (gets) a value from the internal NSIS stack.
 * Since the supplied popstring() method easily can cause buffer
 * overflows, use vboxPopString() instead!
 *
 * @return  VBox status code.
 * @param   pszDest     Pointer to pre-allocated string to store result.
 * @param   cchDest     Size (in characters) of pre-allocated string.
 */
static int vboxPopString(TCHAR *pszDest, size_t cchDest)
{
    int rc = VINF_SUCCESS;

    if (!g_stacktop || !*g_stacktop)
    {
        rc = VERR_NO_DATA;
    }
    else
    {
        stack_t *pStack = (*g_stacktop);
        AssertPtr(pStack);

        HRESULT hr = StringCchCopy(pszDest, cchDest, pStack->text);
        if (SUCCEEDED(hr))
        {
            *g_stacktop = pStack->next;
            GlobalFree((HGLOBAL)pStack);
        }
        else
            rc = VERR_INVALID_PARAMETER;
    }
    return rc;
}

static int vboxPopULong(PULONG pulValue)
{
    int rc = VINF_SUCCESS;

    if (!g_stacktop || !*g_stacktop)
    {
        rc = VERR_NO_DATA;
    }
    else
    {
        stack_t *pStack = (*g_stacktop);
        AssertPtr(pStack);

        *pulValue = _tcstoul(pStack->text, NULL, 10 /* Base */);

        *g_stacktop = pStack->next;
        GlobalFree((HGLOBAL)pStack);
    }

    return rc;
}

static void vboxPushHResultAsString(HRESULT hr)
{
    TCHAR szErr[NSIS_MAX_STRLEN];
    if (FAILED(hr))
    {
        if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, hr, 0, szErr, MAX_PATH, NULL))
            szErr[MAX_PATH] = '\0';
        else
            StringCchPrintf(szErr, sizeof(szErr),
                            _T("FormatMessage failed! Error = %ld"), GetLastError());
    }
    else
        StringCchPrintf(szErr, sizeof(szErr), _T("0"));
    pushstring(szErr);
}

static void vboxPushRcAsString(int rc)
{
    TCHAR szErr[NSIS_MAX_STRLEN];
    if (RT_FAILURE(rc))
    {
        static const char s_szPrefix[] = "Error: ";
        AssertCompile(NSIS_MAX_STRLEN > sizeof(s_szPrefix) + 32);
#ifdef UNICODE
        char szTmp[80];
        memcpy(szTmp, s_szPrefix, sizeof(s_szPrefix));
        RTErrQueryDefine(rc, &szTmp[sizeof(s_szPrefix) - 1], sizeof(szTmp) - sizeof(s_szPrefix) - 1, false);

        RT_ZERO(szErr);
        PRTUTF16 pwszDst = szErr;
        RTStrToUtf16Ex(szTmp, RTSTR_MAX, &pwszDst, RT_ELEMENTS(szErr), NULL);
#else
        memcpy(szErr, s_szPrefix, sizeof(s_szPrefix));
        RTErrQueryDefine(rc, &szErr[sizeof(s_szPrefix) - 1], sizeof(szErr) - sizeof(s_szPrefix) - 1, false);
#endif
    }
    else
    {
        szErr[0] = '0';
        szErr[1] = '\0';
    }

    pushstring(szErr);
}

/**
 * Connects to VBoxTray IPC under the behalf of the user running
 * in the current thread context.
 *
 * @return  IPRT status code.
 * @param   phSession               Where to store the IPC session.
 */
static int vboxConnectToVBoxTray(RTLOCALIPCSESSION *phSession)
{
    char szPipeName[512 + sizeof(VBOXTRAY_IPC_PIPE_PREFIX)];
    memcpy(szPipeName, VBOXTRAY_IPC_PIPE_PREFIX, sizeof(VBOXTRAY_IPC_PIPE_PREFIX));
    int rc = RTProcQueryUsername(NIL_RTPROCESS,
                                 &szPipeName[sizeof(VBOXTRAY_IPC_PIPE_PREFIX) - 1],
                                 sizeof(szPipeName) - sizeof(VBOXTRAY_IPC_PIPE_PREFIX) + 1,
                                 NULL /*pcbUser*/);
    if (RT_SUCCESS(rc))
        rc = RTLocalIpcSessionConnect(phSession, szPipeName, RTLOCALIPC_FLAGS_NATIVE_NAME);
    return rc;
}

#ifndef UNICODE
static void vboxChar2WCharFree(PWCHAR pwString)
{
    if (pwString)
        HeapFree(GetProcessHeap(), 0, pwString);
}

static int vboxChar2WCharAlloc(const TCHAR *pszString, PWCHAR *ppwString)
{
    int rc = VINF_SUCCESS;

    int iLen = (int)_tcslen(pszString) + 2;
    WCHAR *pwString = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, iLen * sizeof(WCHAR));
    if (!pwString)
    {
        rc = VERR_NO_MEMORY;
    }
    else
    {
        if (MultiByteToWideChar(CP_ACP, 0, pszString, -1, pwString, iLen) == 0)
        {
            rc = VERR_INVALID_PARAMETER;
            HeapFree(GetProcessHeap(), 0, pwString);
        }
        else
        {
            *ppwString = pwString;
        }
    }
    return rc;
}
#endif /* !UNICODE */

/**
 * Disables the Windows File Protection for a specified file
 * using an undocumented SFC API call. Don't try this at home!
 *
 * @param   hwndParent          Window handle of parent.
 * @param   string_size         Size of variable string.
 * @param   variables           The actual variable string.
 * @param   stacktop            Pointer to a pointer to the current stack.
 * @param   extra               Extra parameters. Currently unused.
 */
VBOXINSTALLHELPER_EXPORT DisableWFP(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop,
                                    extra_parameters *extra)
{
    RT_NOREF(hwndParent, extra);

    EXDLL_INIT();

    TCHAR szFile[MAX_PATH + 1];
    int rc = vboxPopString(szFile, sizeof(szFile) / sizeof(TCHAR));
    if (RT_SUCCESS(rc))
    {
        RTLDRMOD hSFC;
        rc = RTLdrLoadSystem("sfc_os.dll", false /* fNoUnload */, &hSFC);
        if (RT_SUCCESS(rc))
        {
            PFNSFCFILEEXCEPTION pfnSfcFileException = NULL;
            rc = RTLdrGetSymbol(hSFC, "SfcFileException", (void **)&pfnSfcFileException);
            if (RT_FAILURE(rc))
            {
                /* Native fallback. */
                HMODULE hSFCNative = (HMODULE)RTLdrGetNativeHandle(hSFC);

                /* If we didn't get the proc address with the call above, try it harder with
                 * the (zero based) index of the function list (ordinal). */
                pfnSfcFileException = (PFNSFCFILEEXCEPTION)GetProcAddress(hSFCNative, (LPCSTR)5);
                if (pfnSfcFileException)
                    rc = VINF_SUCCESS;
            }

            if (RT_SUCCESS(rc))
            {
#ifndef UNICODE
                WCHAR *pwszFile;
                rc = vboxChar2WCharAlloc(szFile, &pwszFile);
                if (RT_SUCCESS(rc))
                {
#else
                    TCHAR *pwszFile = szFile;
#endif
                    if (pfnSfcFileException(0, pwszFile, UINT32_MAX) != 0)
                        rc = VERR_ACCESS_DENIED; /** @todo Find a better rc. */
#ifndef UNICODE
                    vboxChar2WCharFree(pwszFile);
                }
#endif
            }

            RTLdrClose(hSFC);
        }
    }

    vboxPushRcAsString(rc);
}

/**
 * Retrieves a file's architecture (x86 or amd64).
 * Outputs "x86", "amd64" or an error message (if not found/invalid) on stack.
 *
 * @param   hwndParent          Window handle of parent.
 * @param   string_size         Size of variable string.
 * @param   variables           The actual variable string.
 * @param   stacktop            Pointer to a pointer to the current stack.
 * @param   extra               Extra parameters. Currently unused.
 */
VBOXINSTALLHELPER_EXPORT FileGetArchitecture(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop,
                                             extra_parameters *extra)
{
    RT_NOREF(hwndParent, extra);

    EXDLL_INIT();

    TCHAR szFile[MAX_PATH + 1];
    int rc = vboxPopString(szFile, sizeof(szFile) / sizeof(TCHAR));
    if (RT_SUCCESS(rc))
    {
#ifdef UNICODE
        char *pszFileUtf8;
        rc = RTUtf16ToUtf8(szFile, &pszFileUtf8);
        if (RT_SUCCESS(rc))
        {
#else
            char *pszFileUtf8 = szFile;
#endif
            RTLDRMOD hLdrMod;
            rc = RTLdrOpen(pszFileUtf8, RTLDR_O_FOR_VALIDATION, RTLDRARCH_WHATEVER, &hLdrMod);
            if (RT_SUCCESS(rc))
            {
                if (RTLdrGetFormat(hLdrMod) == RTLDRFMT_PE)
                {
                    RTLDRARCH enmLdrArch = RTLdrGetArch(hLdrMod);
                    switch (enmLdrArch)
                    {
                        case RTLDRARCH_X86_32:
                            pushstring(_T("x86"));
                            break;

                        case RTLDRARCH_AMD64:
                            pushstring(_T("amd64"));
                            break;

                        default:
                            pushstring(_T("Error: Unknown / invalid architecture"));
                            break;
                    }
                }
                else
                    pushstring(_T("Error: Unknown / invalid PE signature"));

                RTLdrClose(hLdrMod);
            }
            else
                pushstring(_T("Error: Could not open file"));
#ifdef UNICODE
            RTStrFree(pszFileUtf8);
        }
#endif
    }
    else
        pushstring(_T("Error: Could not retrieve file name"));
}

/**
 * Retrieves a file's vendor.
 * Outputs the vendor's name or an error message (if not found/invalid) on stack.
 *
 * @param   hwndParent          Window handle of parent.
 * @param   string_size         Size of variable string.
 * @param   variables           The actual variable string.
 * @param   stacktop            Pointer to a pointer to the current stack.
 * @param   extra               Extra parameters. Currently unused.
 */
VBOXINSTALLHELPER_EXPORT FileGetVendor(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop,
                                       extra_parameters *extra)
{
    RT_NOREF(hwndParent, extra);

    EXDLL_INIT();

    TCHAR szFile[MAX_PATH + 1];
    int rc = vboxPopString(szFile, sizeof(szFile) / sizeof(TCHAR));
    if (RT_SUCCESS(rc))
    {
        DWORD dwInfoSize = GetFileVersionInfoSize(szFile, NULL /* lpdwHandle */);
        if (dwInfoSize)
        {
            void *pFileInfo = GlobalAlloc(GMEM_FIXED, dwInfoSize);
            if (pFileInfo)
            {
                if (GetFileVersionInfo(szFile, 0, dwInfoSize, pFileInfo))
                {
                    LPVOID pvInfo;
                    UINT puInfoLen;
                    if (VerQueryValue(pFileInfo, _T("\\VarFileInfo\\Translation"),
                                      &pvInfo, &puInfoLen))
                    {
                        WORD wCodePage = LOWORD(*(DWORD*)pvInfo);
                        WORD wLanguageID = HIWORD(*(DWORD*)pvInfo);

                        TCHAR szQuery[MAX_PATH];
                        StringCchPrintf(szQuery, sizeof(szQuery), _T("StringFileInfo\\%04X%04X\\CompanyName"),
                                        wCodePage, wLanguageID);

                        LPCTSTR pcData;
                        if (VerQueryValue(pFileInfo, szQuery, (void**)&pcData, &puInfoLen))
                        {
                            pushstring(pcData);
                        }
                        else
                            rc = VERR_NOT_FOUND;
                    }
                    else
                        rc = VERR_NOT_FOUND;
                }
                GlobalFree(pFileInfo);
            }
            else
                rc = VERR_NO_MEMORY;
        }
        else
            rc = VERR_NOT_FOUND;
    }

    if (RT_FAILURE(rc))
        vboxPushRcAsString(rc);
}

/**
 * Shows a balloon message using VBoxTray's notification area in the
 * Windows task bar.
 *
 * @param   hwndParent          Window handle of parent.
 * @param   string_size         Size of variable string.
 * @param   variables           The actual variable string.
 * @param   stacktop            Pointer to a pointer to the current stack.
 * @param   extra               Extra parameters. Currently unused.
 */
VBOXINSTALLHELPER_EXPORT VBoxTrayShowBallonMsg(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop,
                                               extra_parameters *extra)
{
    RT_NOREF(hwndParent, extra);

    EXDLL_INIT();

    TCHAR szMsg[256];
    TCHAR szTitle[128];
    int rc = vboxPopString(szMsg, sizeof(szMsg) / sizeof(TCHAR));
    if (RT_SUCCESS(rc))
        rc = vboxPopString(szTitle, sizeof(szTitle) / sizeof(TCHAR));

    /** @todo Do we need to restore the stack on failure? */

    if (RT_SUCCESS(rc))
    {
        RTR3InitDll(0);

#ifdef UNICODE
        char *pszMsgUtf8   = NULL;
        char *pszTitleUtf8 = NULL;
        rc = RTUtf16ToUtf8(szMsg, &pszMsgUtf8);
        if (RT_SUCCESS(rc))
            rc = RTUtf16ToUtf8(szTitle, &pszTitleUtf8);
#else
        char *pszMsgUtf8   = szMsg;
        char *pszTitleUtf8 = szTitle;
#endif
        if (RT_SUCCESS(rc))
        {
            /* We use UTF-8 for the IPC data. */
            uint32_t cbMsg = sizeof(VBOXTRAYIPCMSG_SHOWBALLOONMSG)
                           + (uint32_t)strlen(pszMsgUtf8)   + 1  /* Include terminating zero */
                           + (uint32_t)strlen(pszTitleUtf8) + 1; /* Ditto. */
            Assert(cbMsg);
            PVBOXTRAYIPCMSG_SHOWBALLOONMSG pIpcMsg = (PVBOXTRAYIPCMSG_SHOWBALLOONMSG)RTMemAlloc(cbMsg);
            if (pIpcMsg)
            {
                /* Stuff in the strings. */
                memcpy(pIpcMsg->szMsgContent, pszMsgUtf8,   strlen(pszMsgUtf8)   + 1);
                memcpy(pIpcMsg->szMsgTitle,   pszTitleUtf8, strlen(pszTitleUtf8) + 1);

                /* Pop off the values in reverse order from the stack. */
                rc = vboxPopULong((ULONG *)&pIpcMsg->uType);
                if (RT_SUCCESS(rc))
                    rc = vboxPopULong((ULONG *)&pIpcMsg->uShowMS);

                if (RT_SUCCESS(rc))
                {
                    RTLOCALIPCSESSION hSession = 0;
                    rc = vboxConnectToVBoxTray(&hSession);
                    if (RT_SUCCESS(rc))
                    {
                        VBOXTRAYIPCHEADER ipcHdr = { VBOXTRAY_IPC_HDR_MAGIC, 0 /* Header version */,
                                                     VBOXTRAYIPCMSGTYPE_SHOWBALLOONMSG, cbMsg };

                        rc = RTLocalIpcSessionWrite(hSession, &ipcHdr, sizeof(ipcHdr));
                        if (RT_SUCCESS(rc))
                            rc = RTLocalIpcSessionWrite(hSession, pIpcMsg, cbMsg);

                        int rc2 = RTLocalIpcSessionClose(hSession);
                        if (RT_SUCCESS(rc))
                            rc = rc2;
                    }
                }

                RTMemFree(pIpcMsg);
            }
            else
                rc = VERR_NO_MEMORY;
#ifdef UNICODE
            RTStrFree(pszMsgUtf8);
            RTStrFree(pszTitleUtf8);
#endif
        }
    }

    vboxPushRcAsString(rc);
}

BOOL WINAPI DllMain(HANDLE hInst, ULONG uReason, LPVOID pReserved)
{
    RT_NOREF(pReserved);

    g_hInstance = (HINSTANCE)hInst;

    switch (uReason)
    {
        case DLL_PROCESS_ATTACH:
            RTR3InitDll(RTR3INIT_FLAGS_UNOBTRUSIVE);
            break;

        case DLL_PROCESS_DETACH:
            break;

        case DLL_THREAD_ATTACH:
            break;

        case DLL_THREAD_DETACH:
            break;

        default:
            break;
    }

    return TRUE;
}

