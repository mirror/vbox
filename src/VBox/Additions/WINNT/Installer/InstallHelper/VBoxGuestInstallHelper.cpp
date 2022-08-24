/* $Id$ */
/** @file
 * VBoxGuestInstallHelper - Various helper routines for Windows guest installer.
 *                          Works with NSIS 3.x.
 */

/*
 * Copyright (C) 2011-2022 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#ifndef UNICODE
# define UNICODE
#endif
#include <iprt/win/windows.h>
#include "exdll.h"

#include <iprt/err.h>
#include <iprt/initterm.h>
#include <iprt/ldr.h>
#include <iprt/localipc.h>
#include <iprt/mem.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/utf16.h>

/* Required structures/defines of VBoxTray. */
#include "../../VBoxTray/VBoxTrayMsg.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define VBOXINSTALLHELPER_EXPORT extern "C" DECLEXPORT(void)


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef DWORD (WINAPI *PFNSFCFILEEXCEPTION)(DWORD param1, PWCHAR param2, DWORD param3);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static HINSTANCE    g_hInstance;
static HWND         g_hwndParent;


/**
 * Frees a popped stack entry after use.
 */
DECLINLINE(void) vboxFreeStackEntry(stack_t *pEntry)
{
    if (pEntry)
        GlobalFree((HGLOBAL)pEntry);
}


/**
 * Allocates a new stack entry for containing a string of the given length
 * (excluding terminator)
 */
DECLINLINE(stack_t *) vboxAllocStackEntry(size_t cwcString)
{
    return (stack_t *)GlobalAlloc(GPTR, RT_UOFFSETOF_DYN(stack_t, text[cwcString + 1]));
}


/**
 * Pops an entry off the stack, return NULL if empty.
 *
 * Call vboxFreeStackEntry when done.
 *
 */
DECLINLINE(stack_t *) vboxPopStack(stack_t **ppTopOfStack)
{
    stack_t *pEntry = ppTopOfStack ? *ppTopOfStack : NULL;
    if (pEntry)
        *ppTopOfStack = pEntry->next;
    return pEntry;
}


/**
 * Pushes an entry onto the stack.
 */
DECLINLINE(void) vboxPushStack(stack_t **ppTopOfStack, stack_t *pEntry)
{
    pEntry->next = *ppTopOfStack;
    *ppTopOfStack = pEntry;
}


static void vboxPushUtf16N(stack_t **ppTopOfStack, wchar_t const *pwszString, size_t cwcString)
{
    stack_t *pEntry = vboxAllocStackEntry(cwcString);

    memcpy(pEntry->text, pwszString, cwcString * sizeof(pEntry->text[0]));
    pEntry->text[cwcString] = '\0';

    vboxPushStack(ppTopOfStack, pEntry);
}


static void vboxPushUtf16(stack_t **ppTopOfStack, wchar_t const *pwszString)
{
    return vboxPushUtf16N(ppTopOfStack, pwszString, RTUtf16Len(pwszString));
}


#define VBOX_PUSH_STRING_LITERAL(a_ppTopOfStack, a_szLiteral) \
    vboxPushUtf16N(a_ppTopOfStack, RT_CONCAT(L, a_szLiteral), sizeof(RT_CONCAT(L, a_szLiteral)) / sizeof(wchar_t) - 1)


static void vboxPushUtf8(stack_t **ppTopOfStack, char const *pszString)
{
    size_t cwcUtf16 = RTStrCalcUtf16Len(pszString);
    stack_t *pEntry = vboxAllocStackEntry(cwcUtf16);

    PRTUTF16 pwszUtf16 = pEntry->text;
    int rc = RTStrToUtf16Ex(pszString, RTSTR_MAX, &pwszUtf16, cwcUtf16 + 1, NULL);
    AssertRC(rc);

    vboxPushStack(ppTopOfStack, pEntry);
}

/**
 * Pushes a string containing an error message and a VBox status code.
 */
static void vboxPushVBoxError(stack_t **ppTopOfStack, char const *pszString, int vrc)
{
    RTUTF16 wszTmp[128];
    RTUtf16Printf(wszTmp, RT_ELEMENTS(wszTmp), "Error: %s! %Rrc", pszString, vrc);
    vboxPushUtf16(ppTopOfStack, wszTmp);
}


static void vboxPushLastError(stack_t **ppTopOfStack, char const *pszString)
{
    DWORD const dwErr = GetLastError();
    RTUTF16 wszTmp[128];
    RTUtf16Printf(wszTmp, RT_ELEMENTS(wszTmp), "Error: %s! lasterr=%u (%#x)", pszString, dwErr, dwErr);
    vboxPushUtf16(ppTopOfStack, wszTmp);
}


static void vboxPushLastErrorF(stack_t **ppTopOfStack, const char *pszFormat, ...)
{
    DWORD const dwErr = GetLastError();
    RTUTF16 wszTmp[128];
    va_list va;
    va_start(va, pszFormat);
    RTUtf16Printf(wszTmp, RT_ELEMENTS(wszTmp), "Error: %N! lasterr=%u (%#x)", pszFormat, &va, dwErr, dwErr);
    va_end(va);
    vboxPushUtf16(ppTopOfStack, wszTmp);
}


/**
 * Convers a parameter to uint32_t.
 *
 * @returns IPRT status code.
 * @param   pwsz                Will be trimmed.
 * @param   puValue             Where to return the value.
 */
static int vboxUtf16ToUInt32(PRTUTF16 pwsz, uint32_t *puValue)
{
    *puValue = 0;

    /* Trim the input: */
    RTUTF16 wc;
    while ((wc = *pwsz) == ' ' || wc == '\t')
        pwsz++;
    size_t cwc = RTUtf16Len(pwsz);
    while (cwc > 0 && ((wc = pwsz[cwc - 1]) == ' ' || wc == '\t'))
        pwsz[--cwc] = '\0';

    /* Convert the remains into an UTF-8 string. */
    char szValue[128];
    char *pszValue = &szValue[0];
    int rc = RTUtf16ToUtf8Ex(pwsz, cwc, &pszValue, sizeof(szValue), NULL);
    if (RT_SUCCESS(rc))
        rc = RTStrToUInt32Full(pszValue, 0, puValue);
    return rc;
}


/**
 * Connects to VBoxTray IPC under the behalf of the user running in the current
 * thread context.
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


/**
 * Retrieves a file's architecture (x86 or amd64).
 *
 * Outputs "x86", "amd64" or an error message (if not found/invalid) on stack.
 *
 * @param   hwndParent          Window handle of parent.
 * @param   string_size         Size of variable string.
 * @param   variables           The actual variable string.
 * @param   stacktop            Pointer to a pointer to the current stack.
 * @param   extra               Extra parameters.
 */
VBOXINSTALLHELPER_EXPORT FileGetArchitecture(HWND hwndParent, int string_size, WCHAR *variables, stack_t **stacktop,
                                             extra_parameters *extra)
{
    RT_NOREF(hwndParent, string_size, variables, extra);

    stack_t *pEntry = vboxPopStack(stacktop);
    if (pEntry)
    {
        char *pszFileUtf8;
        int rc = RTUtf16ToUtf8(pEntry->text, &pszFileUtf8);
        if (RT_SUCCESS(rc))
        {
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
                            VBOX_PUSH_STRING_LITERAL(stacktop, "x86");
                            break;

                        case RTLDRARCH_AMD64:
                            VBOX_PUSH_STRING_LITERAL(stacktop, "amd64");
                            break;

                        default:
                            VBOX_PUSH_STRING_LITERAL(stacktop, "Error: Unknown / invalid architecture");
                            break;
                    }
                }
                else
                    VBOX_PUSH_STRING_LITERAL(stacktop, "Error: Unknown / invalid PE signature");

                RTLdrClose(hLdrMod);
            }
            else
                vboxPushVBoxError(stacktop, "RTLdrOpen failed", rc);
            RTStrFree(pszFileUtf8);
        }
        else
            vboxPushVBoxError(stacktop, "RTUtf16ToUtf8 failed", rc);
    }
    else
        VBOX_PUSH_STRING_LITERAL(stacktop, "Error: Could not retrieve file name");
    vboxFreeStackEntry(pEntry);
}

/**
 * Retrieves a file's vendor.
 *
 * Outputs the vendor's name or an error message (if not found/invalid) on stack.
 *
 * @param   hwndParent          Window handle of parent.
 * @param   string_size         Size of variable string.
 * @param   variables           The actual variable string.
 * @param   stacktop            Pointer to a pointer to the current stack.
 * @param   extra               Extra parameters.
 */
VBOXINSTALLHELPER_EXPORT FileGetVendor(HWND hwndParent, int string_size, WCHAR *variables, stack_t **stacktop,
                                       extra_parameters *extra)
{
    RT_NOREF(hwndParent, string_size, variables, extra);

    stack_t *pEntry = vboxPopStack(stacktop);
    if (pEntry)
    {
        DWORD dwInfoSize = GetFileVersionInfoSizeW(pEntry->text, NULL /* lpdwHandle */);
        if (dwInfoSize)
        {
            void *pvFileInfo = GlobalAlloc(GMEM_FIXED, dwInfoSize);
            if (pvFileInfo)
            {
                if (GetFileVersionInfoW(pEntry->text, 0, dwInfoSize, pvFileInfo))
                {
                    LPVOID pvInfo;
                    UINT   cbInfo;
                    if (VerQueryValueW(pvFileInfo, L"\\VarFileInfo\\Translation", &pvInfo, &cbInfo))
                    {
                        WORD wCodePage   = LOWORD(*(DWORD const *)pvInfo);
                        WORD wLanguageID = HIWORD(*(DWORD const *)pvInfo);

                        WCHAR wszQuery[80];
                        RTUtf16Printf(wszQuery, RT_ELEMENTS(wszQuery), "StringFileInfo\\%04X%04X\\CompanyName",
                                      wCodePage, wLanguageID);

                        LPCWSTR pwszData;
                        if (VerQueryValueW(pvFileInfo, wszQuery, (void **)&pwszData, &cbInfo))
                            vboxPushUtf16(stacktop, pwszData);
                        else
                            vboxPushLastErrorF(stacktop, "VerQueryValueW '%ls' failed", wszQuery);
                    }
                    else
                        vboxPushLastError(stacktop, "VerQueryValueW '\\VarFileInfo\\Translation' failed");
                }
                else
                    vboxPushLastError(stacktop, "GetFileVersionInfo failed");
                GlobalFree(pvFileInfo);
            }
            else
                VBOX_PUSH_STRING_LITERAL(stacktop, "Error: GlobalAlloc failed");
        }
        else
            vboxPushLastError(stacktop, "GetFileVersionInfoSizeW failed");
    }
    else
        VBOX_PUSH_STRING_LITERAL(stacktop, "Error: Could not retrieve file name");
    vboxFreeStackEntry(pEntry);
}

/**
 * Shows a balloon message using VBoxTray's notification area in the
 * Windows task bar.
 *
 * @param   hwndParent          Window handle of parent.
 * @param   string_size         Size of variable string.
 * @param   variables           The actual variable string.
 * @param   stacktop            Pointer to a pointer to the current stack.
 * @param   extra               Extra parameters.
 */
VBOXINSTALLHELPER_EXPORT VBoxTrayShowBallonMsg(HWND hwndParent, int string_size, WCHAR *variables, stack_t **stacktop,
                                               extra_parameters *extra)
{
    RT_NOREF(hwndParent, string_size, variables, extra);

    /*
     * Get parameters from the stack.
     */
    stack_t * const pMsgEntry     = vboxPopStack(stacktop);
    stack_t * const pTitleEntry   = vboxPopStack(stacktop);
    stack_t * const pTypeEntry    = vboxPopStack(stacktop);
    stack_t * const pTimeoutEntry = vboxPopStack(stacktop);
    if (pTimeoutEntry)
    {
        /*
         * Allocate an IPC message payload structure of the right size.
         */
        size_t const cchMsg     = RTUtf16CalcUtf8Len(pMsgEntry->text);
        size_t const cchTitle   = RTUtf16CalcUtf8Len(pTitleEntry->text);
        size_t const cbPayload  = RT_UOFFSETOF_DYN(VBOXTRAYIPCMSG_SHOW_BALLOON_MSG_T, szzStrings[cchMsg + 1 + cchTitle + 1]);
        PVBOXTRAYIPCMSG_SHOW_BALLOON_MSG_T pPayload = (PVBOXTRAYIPCMSG_SHOW_BALLOON_MSG_T)RTMemAllocZVar(cbPayload);
        if (pPayload)
        {
            VBOXTRAYIPCHEADER const MsgHdr =
            {
                VBOXTRAY_IPC_HDR_MAGIC,
                VBOXTRAY_IPC_HDR_VERSION,
                VBOXTRAYIPCMSGTYPE_SHOW_BALLOON_MSG,
                cbPayload
            };

            /*
             * Convert the parametes and put them into the payload structure.
             */
            pPayload->cchMsg   = cchMsg;
            pPayload->cchTitle = cchTitle;
            char *psz = &pPayload->szzStrings[0];
            int rc = RTUtf16ToUtf8Ex(pMsgEntry->text, RTSTR_MAX, &psz, cchMsg + 1, NULL);
            if (RT_SUCCESS(rc))
            {
                psz = &pPayload->szzStrings[cchMsg + 1];
                rc = RTUtf16ToUtf8Ex(pTitleEntry->text, RTSTR_MAX, &psz, cchTitle + 1, NULL);
                if (RT_SUCCESS(rc))
                {
                    rc = vboxUtf16ToUInt32(pTypeEntry->text, &pPayload->uType);
                    if (RT_SUCCESS(rc))
                    {
                        rc = vboxUtf16ToUInt32(pTypeEntry->text, &pPayload->cMsTimeout);
                        if (RT_SUCCESS(rc))
                        {
                            /*
                             * Connect to VBoxTray and send the message.
                             */
                            RTLOCALIPCSESSION hSession = 0;
                            rc = vboxConnectToVBoxTray(&hSession);
                            if (RT_SUCCESS(rc))
                            {
                                rc = RTLocalIpcSessionWrite(hSession, &MsgHdr, sizeof(MsgHdr));
                                if (RT_SUCCESS(rc))
                                {
                                    rc = RTLocalIpcSessionWrite(hSession, pPayload, cbPayload);
                                    if (RT_FAILURE(rc))
                                        vboxPushVBoxError(stacktop, "Failed to write message payload", rc);
                                }
                                else
                                    vboxPushVBoxError(stacktop, "Failed to write message header", rc);

                                int rc2 = RTLocalIpcSessionClose(hSession);
                                if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                                {
                                    vboxPushVBoxError(stacktop, "RTLocalIpcSessionClose failed", rc);
                                    rc = rc2;
                                }
                            }
                            else
                                vboxPushVBoxError(stacktop, "vboxConnectToVBoxTray failed", rc);
                        }
                        else
                            vboxPushVBoxError(stacktop, "MyUtf16ToUInt32 failed on the timeout value", rc);
                    }
                    else
                        vboxPushVBoxError(stacktop, "MyUtf16ToUInt32 failed on the type value", rc);
                }
                else
                    vboxPushVBoxError(stacktop, "RTUtf16ToUtf8Ex failed on the title text", rc);
            }
            else
                vboxPushVBoxError(stacktop, "RTUtf16ToUtf8Ex failed on the message text", rc);
        }
        else
            VBOX_PUSH_STRING_LITERAL(stacktop, "Error: Out of memory!");
    }
    else
        VBOX_PUSH_STRING_LITERAL(stacktop, "Error: Too few parameters on the stack!");
    vboxFreeStackEntry(pTimeoutEntry);
    vboxFreeStackEntry(pTypeEntry);
    vboxFreeStackEntry(pTitleEntry);
    vboxFreeStackEntry(pMsgEntry);
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

