/* $Id$ */
/** @file
 * VBoxGuestInstallHelper - Various helper routines for Windows guest installer.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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
#include <atlconv.h>
#include <stdlib.h>
#include <Strsafe.h>
#include <tchar.h>
#include "exdll.h"

/* Required structures/defines of VBoxTray. */
#include "../../VBoxTray/VBoxTrayMsg.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define VBOXINSTALLHELPER_EXPORT extern "C" void __declspec(dllexport)


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef DWORD (WINAPI *PFNSFCFILEEXCEPTION)(DWORD param1, PWCHAR param2, DWORD param3);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
HINSTANCE               g_hInstance;
HWND                    g_hwndParent;
PFNSFCFILEEXCEPTION     g_pfnSfcFileException = NULL;


/**
 * Pops (gets) a value from the internal NSIS stack.
 * Since the supplied popstring() method easily can cause buffer
 * overflows, use vboxPopString() instead!
 *
 * @return  HRESULT
 * @param   pszDest     Pointer to pre-allocated string to store result.
 * @param   cchDest     Size (in characters) of pre-allocated string.
 */
static HRESULT vboxPopString(TCHAR *pszDest, size_t cchDest)
{
    HRESULT hr = S_OK;
    if (!g_stacktop || !*g_stacktop)
        hr = __HRESULT_FROM_WIN32(ERROR_EMPTY);
    else
    {
        stack_t *pStack = (*g_stacktop);
        if (pStack)
        {
            hr = StringCchCopy(pszDest, cchDest, pStack->text);
            if (SUCCEEDED(hr))
            {
                *g_stacktop = pStack->next;
                GlobalFree((HGLOBAL)pStack);
            }
        }
    }
    return hr;
}

static HRESULT vboxPopULong(PULONG pulValue)
{
    HRESULT hr = S_OK;
    if (!g_stacktop || !*g_stacktop)
        hr = __HRESULT_FROM_WIN32(ERROR_EMPTY);
    else
    {
        stack_t *pStack = (*g_stacktop);
        if (pStack)
        {
            *pulValue = strtoul(pStack->text, NULL, 10 /* Base */);

            *g_stacktop = pStack->next;
            GlobalFree((HGLOBAL)pStack);
        }
    }
    return hr;
}

static void vboxPushResultAsString(HRESULT hr)
{
    TCHAR szErr[MAX_PATH + 1];
    if (FAILED(hr))
    {
        if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, hr, 0, szErr, MAX_PATH, NULL))
            szErr[MAX_PATH] = '\0';
        else
            StringCchPrintf(szErr, sizeof(szErr),
                            "FormatMessage failed! Error = %ld", GetLastError());
    }
    else
        StringCchPrintf(szErr, sizeof(szErr), "0");
    pushstring(szErr);
}

static void vboxChar2WCharFree(PWCHAR pwString)
{
    if (pwString)
        HeapFree(GetProcessHeap(), 0, pwString);
}

static HRESULT vboxChar2WCharAlloc(const char *pszString, PWCHAR *ppwString)
{
    HRESULT hr;
    int iLen = strlen(pszString) + 2;
    WCHAR *pwString = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, iLen * sizeof(WCHAR));
    if (!pwString)
        hr = __HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY);
    else
    {
        if (MultiByteToWideChar(CP_ACP, 0, pszString, -1, pwString, iLen) == 0)
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            HeapFree(GetProcessHeap(), 0, pwString);
        }
        else
        {
            hr = S_OK;
            *ppwString = pwString;
        }
    }
    return hr;
}

static HANDLE vboxIPCConnect(void)
{
    HANDLE hPipe = NULL;
    while (1)
    {
        hPipe = CreateFile(VBOXTRAY_PIPE_IPC,   /* Pipe name. */
                           GENERIC_READ |       /* Read and write access. */
                           GENERIC_WRITE,
                           0,                   /* No sharing. */
                           NULL,                /* Default security attributes. */
                           OPEN_EXISTING,       /* Opens existing pipe. */
                           0,                   /* Default attributes. */
                           NULL);               /* No template file. */

        /* Break if the pipe handle is valid. */
        if (hPipe != INVALID_HANDLE_VALUE)
            break;

        /* Exit if an error other than ERROR_PIPE_BUSY occurs. */
        if (GetLastError() != ERROR_PIPE_BUSY)
            return NULL;

        /* All pipe instances are busy, so wait for 20 seconds. */
        if (!WaitNamedPipe(VBOXTRAY_PIPE_IPC, 20000))
            return NULL;
    }

    /* The pipe connected; change to message-read mode. */
    DWORD dwMode = PIPE_READMODE_MESSAGE;
    BOOL fSuccess = SetNamedPipeHandleState(hPipe,    /* Pipe handle. */
                                            &dwMode,  /* New pipe mode. */
                                            NULL,     /* Don't set maximum bytes. */
                                            NULL);    /* Don't set maximum time. */
    if (!fSuccess)
        return NULL;
    return hPipe;
}

static void vboxIPCDisconnect(HANDLE hPipe)
{
    CloseHandle(hPipe);
}

static HRESULT vboxIPCWriteMessage(HANDLE hPipe, BYTE *pMessage, DWORD cbMessage)
{
    HRESULT hr = S_OK;
    DWORD cbWritten = 0;
    if (!WriteFile(hPipe, pMessage, cbMessage - cbWritten, &cbWritten, 0))
        hr = HRESULT_FROM_WIN32(GetLastError());
    return hr;
}

/**
 * Disables the Windows File Protection for a specified file
 * using an undocumented SFC API call. Don't try this at home!
 *
 * @param   hwndParent          Window handle of parent.
 * @param   string_size         Size of variable string.
 * @param   variables           The actual variable string.
 * @param   stacktop            Pointer to a pointer to the current stack.
 */
VBOXINSTALLHELPER_EXPORT DisableWFP(HWND hwndParent, int string_size,
                                    TCHAR *variables, stack_t **stacktop)
{
    EXDLL_INIT();

    TCHAR szFile[MAX_PATH + 1];
    HRESULT hr = vboxPopString(szFile, sizeof(szFile) / sizeof(TCHAR));
    if (SUCCEEDED(hr))
    {
        HMODULE hSFC = LoadLibrary("sfc_os.dll");
        if (NULL != hSFC)
        {
            g_pfnSfcFileException = (PFNSFCFILEEXCEPTION)GetProcAddress(hSFC, "SfcFileException");
            if (g_pfnSfcFileException == NULL)
            {
                /* If we didn't get the proc address with the call above, try it harder with
                 * the (zero based) index of the function list. */
                g_pfnSfcFileException = (PFNSFCFILEEXCEPTION)GetProcAddress(hSFC, (LPCSTR)5);
                if (g_pfnSfcFileException == NULL)
                    hr = HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
            }
        }
        else
            hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

        if (SUCCEEDED(hr))
        {
            WCHAR *pwszFile;
            hr = vboxChar2WCharAlloc(szFile, &pwszFile);
            if (SUCCEEDED(hr))
            {
                if (g_pfnSfcFileException(0, pwszFile, -1) != 0)
                    hr = HRESULT_FROM_WIN32(GetLastError());
                vboxChar2WCharFree(pwszFile);
            }
        }

        if (hSFC)
            FreeLibrary(hSFC);
    }

    vboxPushResultAsString(hr);
}

/**
 * Retrieves a file's architecture (x86 or amd64).
 * Outputs "x86", "amd64" or an error message (if not found/invalid) on stack.
 *
 * @param   hwndParent          Window handle of parent.
 * @param   string_size         Size of variable string.
 * @param   variables           The actual variable string.
 * @param   stacktop            Pointer to a pointer to the current stack.
 */
VBOXINSTALLHELPER_EXPORT FileGetArchitecture(HWND hwndParent, int string_size,
                                             TCHAR *variables, stack_t **stacktop)
{
    EXDLL_INIT();

    TCHAR szFile[MAX_PATH + 1];
    HRESULT hr = vboxPopString(szFile, sizeof(szFile) / sizeof(TCHAR));
    if (SUCCEEDED(hr))
    {
        /* See: http://www.microsoft.com/whdc/system/platform/firmware/PECOFF.mspx */
        FILE *pFh = fopen(szFile, "rb");
        if (pFh)
        {
            /* Assume the file is invalid. */
            hr = __HRESULT_FROM_WIN32(ERROR_FILE_INVALID);

            BYTE byOffsetPE; /* Absolute offset of PE signature. */

            /* Do some basic validation. */
            /* Check for "MZ" header (DOS stub). */
            BYTE byBuf[255];
            if (   fread(&byBuf, sizeof(BYTE), 2, pFh) == 2
                && !memcmp(&byBuf, "MZ", 2))
            {
                /* Seek to 0x3C to get the PE offset. */
                if (!fseek(pFh, 60L /*0x3C*/, SEEK_SET))
                {
                    /* Read actual offset of PE signature. */
                    if (fread(&byOffsetPE, sizeof(BYTE), 1, pFh) == 1)
                    {
                        /* ... and seek to it. */
                        if (!fseek(pFh, byOffsetPE, SEEK_SET))
                        {
                            /* Validate PE signature. */
                            if (fread(byBuf, sizeof(BYTE), 4, pFh) == 4)
                            {
                                if (!memcmp(byBuf, "PE\0\0", 4))
                                    hr = S_OK;
                            }
                        }
                    }
                }
            }

            /* Validation successful? */
            if (SUCCEEDED(hr))
            {
                BYTE byOffsetCOFF = byOffsetPE + 0x4; /* Skip PE signature. */

                /** @todo When we need to do more stuff here, we probably should
                 *        mmap the file w/ a struct so that we easily could access
                 *        all the fixed size stuff. Later. */

                /* Jump to machine type (first entry, 2 bytes):
                 * Use absolute PE offset retrieved above. */
                if (!fseek(pFh, byOffsetCOFF, SEEK_SET))
                {
                    WORD wMachineType;
                    if (fread(&wMachineType, 1,
                              sizeof(wMachineType), pFh) == 2)
                    {
                        switch (wMachineType)
                        {
                            case 0x14C: /* Intel 86 */
                                pushstring("x86");
                                break;

                            case 0x8664: /* AMD64 / x64 */
                                pushstring("amd64");
                                break;

                            default:
                                hr = __HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
                                break;
                        }
                    }
                    else
                        hr = __HRESULT_FROM_WIN32(ERROR_FILE_INVALID);
                }
                else
                    hr = __HRESULT_FROM_WIN32(ERROR_FILE_INVALID);
            }

            fclose(pFh);
        }
        else
            hr = __HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    }

    if (FAILED(hr))
        vboxPushResultAsString(hr);
}

/**
 * Retrieves a file's vendor.
 * Outputs the vendor's name or an error message (if not found/invalid) on stack.
 *
 * @param   hwndParent          Window handle of parent.
 * @param   string_size         Size of variable string.
 * @param   variables           The actual variable string.
 * @param   stacktop            Pointer to a pointer to the current stack.
 */
VBOXINSTALLHELPER_EXPORT FileGetVendor(HWND hwndParent, int string_size,
                                       TCHAR *variables, stack_t **stacktop)
{
    EXDLL_INIT();

    TCHAR szFile[MAX_PATH + 1];
    HRESULT hr = vboxPopString(szFile, sizeof(szFile) / sizeof(TCHAR));
    if (SUCCEEDED(hr))
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
                        _sntprintf(szQuery, sizeof(szQuery), _T("StringFileInfo\\%04X%04X\\CompanyName"),
                                   wCodePage,wLanguageID);

                        LPCTSTR pcData;
                        if (VerQueryValue(pFileInfo, szQuery,(void**)&pcData, &puInfoLen))
                        {
                            pushstring(pcData);
                        }
                        else
                            hr = __HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
                    }
                    else
                        hr = __HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
                }
                GlobalFree(pFileInfo);
            }
            else
                hr = __HRESULT_FROM_WIN32(ERROR_OUTOFMEMORY);
        }
        else
            hr = __HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    }

    if (FAILED(hr))
        vboxPushResultAsString(hr);
}

/**
 * Shows a balloon message using VBoxTray's notification area in the
 * Windows task bar.
 *
 * @param   hwndParent          Window handle of parent.
 * @param   string_size         Size of variable string.
 * @param   variables           The actual variable string.
 * @param   stacktop            Pointer to a pointer to the current stack.
 */
VBOXINSTALLHELPER_EXPORT VBoxTrayShowBallonMsg(HWND hwndParent, int string_size,
                                               TCHAR *variables, stack_t **stacktop)
{
    EXDLL_INIT();

    VBOXTRAYIPCHEADER hdr;
    hdr.ulMsg = VBOXTRAYIPCMSGTYPE_SHOWBALLOONMSG;
    hdr.cbBody = sizeof(VBOXTRAYIPCMSG_SHOWBALLOONMSG);

    VBOXTRAYIPCMSG_SHOWBALLOONMSG msg;
    HRESULT hr = vboxPopString(msg.szContent, sizeof(msg.szContent) / sizeof(TCHAR));
    if (SUCCEEDED(hr))
        hr = vboxPopString(msg.szTitle, sizeof(msg.szTitle) / sizeof(TCHAR));
    if (SUCCEEDED(hr))
        hr = vboxPopULong(&msg.ulType);
    if (SUCCEEDED(hr))
        hr = vboxPopULong(&msg.ulShowMS);

    if (SUCCEEDED(hr))
    {
        msg.ulFlags = 0;

        HANDLE hPipe = vboxIPCConnect();
        if (hPipe)
        {
            hr = vboxIPCWriteMessage(hPipe, (BYTE*)&hdr, sizeof(VBOXTRAYIPCHEADER));
            if (SUCCEEDED(hr))
                hr = vboxIPCWriteMessage(hPipe, (BYTE*)&msg, sizeof(VBOXTRAYIPCMSG_SHOWBALLOONMSG));
            vboxIPCDisconnect(hPipe);
        }
    }

    /* Push simple return value on stack. */
    SUCCEEDED(hr) ? pushstring("0") : pushstring("1");
}

BOOL WINAPI DllMain(HANDLE hInst, ULONG uReason, LPVOID lpReserved)
{
    g_hInstance = (HINSTANCE)hInst;
    return TRUE;
}

