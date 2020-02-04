/* $Id$ */
/** @file
 * tstWinAdditionsInstallHelper - Testcases for the Windows Guest Additions Installer Helper DLL.
 */

/*
 * Copyright (C) 2019-2020 Oracle Corporation
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

#include <tchar.h>

#pragma warning(push)
#pragma warning(disable: 4995) /* warning C4995: 'lstrcpyA': name was marked as #pragma deprecated */
#include "../exdll.h"
#pragma warning(pop)

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/ldr.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/test.h>
#include <iprt/utf16.h>


RT_C_DECLS_BEGIN

/** A generic NSIS plugin function. */
typedef void __cdecl NSIS_PLUGIN_FUNC(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop, extra_parameters *extra);
/** Pointer to a generic NSIS plugin function. */
typedef NSIS_PLUGIN_FUNC *PNSIS_PLUGIN_FUNC;

RT_C_DECLS_END

/** Symbol names to test. */
#define TST_FILEGETARCHITECTURE_NAME   "FileGetArchitecture"
#define TST_FILEGETVENDOR_NAME         "FileGetVendor"
#define TST_VBOXTRAYSHOWBALLONMSG_NAME "VBoxTrayShowBallonMsg"


/**
 * Destroys a stack.
 *
 * @param   pStackTop           Stack to destroy.
 */
static void tstStackDestroy(stack_t *pStackTop)
{
    while (pStackTop)
    {
        stack_t *pStackNext = pStackTop->next;
        GlobalFree(pStackTop);
        pStackTop = pStackNext;
    }
}

/**
 * Pushes a string to a stack
 *
 * @returns VBox status code.
 * @param   ppStackTop          Stack to push string to.
 * @param   pcszString          String to push to the stack.
 */
static int tstStackPushString(stack_t **ppStackTop, const TCHAR *pcszString)
{
    int rc = VINF_SUCCESS;

    /* Termination space is part of stack_t. */
    stack_t *pStack = (stack_t *)GlobalAlloc(GPTR, sizeof(stack_t) + g_stringsize);
    if (pStack)
    {
        lstrcpyn(pStack->text, pcszString, (int)g_stringsize);
        pStack->next = ppStackTop ? *ppStackTop : NULL;

        *ppStackTop = pStack;
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

/**
 * Pops a string off a stack.
 *
 * @returns Allocated string popped off the stack, or NULL on error / empty stack.
 * @param   ppStackTop          Stack to pop off string from.
 */
static int tstStackPopString(stack_t **ppStackTop, TCHAR *pszStr, size_t cchStr)
{
    stack_t *pStack = *ppStackTop;
    lstrcpyn(pszStr, pStack->text, cchStr);
    *ppStackTop = pStack->next;
    GlobalFree((HGLOBAL)pStack);

    return VINF_SUCCESS;
}

int main()
{
    RTTEST hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate("tstWinAdditionsInstallHelper", &hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(hTest);

    char szGuestInstallHelperDll[RTPATH_MAX];
    RTProcGetExecutablePath(szGuestInstallHelperDll, sizeof(szGuestInstallHelperDll));

    /* Set the default string size. */
    g_stringsize = NSIS_MAX_STRLEN;

    /** @todo This ASSUMES that this testcase always is located in the separate "bin/additions" sub directory
     *        and that we currently always repack the Guest Additions stuff in a separate directory.
     *        Might need some more tweaking ... */
    int rc = RTPathAppend(szGuestInstallHelperDll, sizeof(szGuestInstallHelperDll),
                          "../../../repack/resources/VBoxGuestInstallHelper/VBoxGuestInstallHelper.dll");
    if (RT_SUCCESS(rc))
    {
        RTTestIPrintf(RTTESTLVL_ALWAYS, "Using DLL: %s\n", szGuestInstallHelperDll);

#ifdef UNICODE
        const bool fUnicode = true;
#else
        const bool fUnicode = false;
#endif
        RTTestIPrintf(RTTESTLVL_ALWAYS, "Running %s build\n", fUnicode ? "UNICODE" : "ANSI");

        RTLDRMOD hLdrMod;
        rc = RTLdrLoad(szGuestInstallHelperDll, &hLdrMod);
        if (RT_SUCCESS(rc))
        {
            TCHAR szVars[NSIS_MAX_STRLEN * sizeof(TCHAR)] = { 0 };

            /*
             * Tests FileGetArchitecture
             */
            PNSIS_PLUGIN_FUNC pfnFileGetArchitecture = NULL;
            rc = RTLdrGetSymbol(hLdrMod, TST_FILEGETARCHITECTURE_NAME, (void**)&pfnFileGetArchitecture);
            if (RT_SUCCESS(rc))
            {
                stack_t *pStack = NULL;
                tstStackPushString(&pStack, _T("c:\\windows\\system32\\kernel32.dll"));

                pfnFileGetArchitecture(NULL /* hWnd */, NSIS_MAX_STRLEN, szVars, &pStack, NULL /* extra */);

                TCHAR szStack[NSIS_MAX_STRLEN];
                rc = tstStackPopString(&pStack, szStack, sizeof(szStack));
                if (   RT_SUCCESS(rc)
                    && (   !_tcscmp(szStack, _T("x86"))
                        || !_tcscmp(szStack, _T("amd64"))))
                {
                    if (fUnicode)
                        RTTestIPrintf(RTTESTLVL_ALWAYS, "Arch: %ls\n", szStack);
                    else
                        RTTestIPrintf(RTTESTLVL_ALWAYS, "Arch: %s\n", szStack);
                }
                else
                    RTTestIFailed("Getting file arch failed (NSIS API changed?)\n");
                tstStackDestroy(pStack);
            }
            else
                RTTestIFailed("Loading pfnFileGetArchitecture failed: %Rrc", rc);

            /*
             * Tests FileGetVendor
             */
            PNSIS_PLUGIN_FUNC pfnFileGetVendor;
            rc = RTLdrGetSymbol(hLdrMod, TST_FILEGETVENDOR_NAME, (void**)&pfnFileGetVendor);
            if (RT_SUCCESS(rc))
            {
                stack_t *pStack = NULL;
                tstStackPushString(&pStack, _T("c:\\windows\\system32\\kernel32.dll"));

                pfnFileGetVendor(NULL /* hWnd */, NSIS_MAX_STRLEN, szVars, &pStack, NULL /* extra */);

                TCHAR szStack[NSIS_MAX_STRLEN];
                rc = tstStackPopString(&pStack, szStack, RT_ELEMENTS(szStack));
                if (   RT_SUCCESS(rc)
                    && !_tcscmp(szStack, _T("Microsoft Corporation")))
                {
                    if (fUnicode)
                        RTTestIPrintf(RTTESTLVL_ALWAYS, "Vendor: %ls\n", szStack);
                    else
                        RTTestIPrintf(RTTESTLVL_ALWAYS, "Vendor: %s\n", szStack);
                }
                else
                    RTTestIFailed("Getting file vendor failed (NSIS API changed?)\n");
                tstStackDestroy(pStack);
            }
            else
                RTTestIFailed("Loading pfnFileGetVendor failed: %Rrc", rc);

            /*
             * Tests VBoxTrayShowBallonMsg
             */
            PNSIS_PLUGIN_FUNC pfnVBoxTrayShowBallonMsg;
            rc = RTLdrGetSymbol(hLdrMod, TST_VBOXTRAYSHOWBALLONMSG_NAME, (void **)&pfnVBoxTrayShowBallonMsg);
            if (RT_SUCCESS(rc))
            {
                stack_t *pStack = NULL;
                /* Push stuff in reverse order to the stack. */
                tstStackPushString(&pStack, _T("5000") /* Time to show in ms */);
                tstStackPushString(&pStack, _T("1") /* Type */);
                tstStackPushString(&pStack, _T("This is a message from tstWinAdditionsInstallHelper!"));
                tstStackPushString(&pStack, _T("This is a title from tstWinAdditionsInstallHelper!"));

                pfnVBoxTrayShowBallonMsg(NULL /* hWnd */, NSIS_MAX_STRLEN, szVars, &pStack, NULL /* extra */);

                TCHAR szStack[NSIS_MAX_STRLEN];
                rc = tstStackPopString(&pStack, szStack, RT_ELEMENTS(szStack));
                if (RT_SUCCESS(rc))
                {
                    if (fUnicode)
                        RTTestIPrintf(RTTESTLVL_ALWAYS, "Reply: %ls\n", szStack);
                    else
                        RTTestIPrintf(RTTESTLVL_ALWAYS, "Reply: %s\n", szStack);
                }
                else
                    RTTestIFailed("Sending message to VBoxTray failed (NSIS API changed?)\n");
                tstStackDestroy(pStack);
            }
            else
                RTTestIFailed("Loading pfnVBoxTrayShowBallonMsg failed: %Rrc", rc);

            RTLdrClose(hLdrMod);
        }
        else
            RTTestIFailed("Loading DLL failed: %Rrc", rc);
    }
    else
        RTTestIFailed("Getting absolute path of DLL failed: %Rrc", rc);

    return RTTestSummaryAndDestroy(hTest);
}

