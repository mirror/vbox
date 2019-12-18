/* $Id$ */
/** @file
 * tstWinAdditionsInstallHelper - Testcases for the Windows Guest Additions Installer Helper DLL.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
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


RT_C_DECLS_BEGIN

/** A generic NSIS plugin function. */
typedef void __cdecl NSIS_PLUGIN_FUNC(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop, extra_parameters *extra);
/** Pointer to a generic NSIS plugin function. */
typedef NSIS_PLUGIN_FUNC *PNSIS_PLUGIN_FUNC;

RT_C_DECLS_END

/** Symbol names to test. */
#define TST_FILEGETARCHITECTURE_NAME "FileGetArchitecture"
#define TST_FILEGETVENDOR_NAME       "FileGetVendor"


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
static int tstStackPushString(stack_t **ppStackTop, const char *pcszString)
{
    const size_t cchString = strlen(pcszString);
    AssertReturn(cchString, NULL);

    int rc = VINF_SUCCESS;

    stack_t *pStack = (stack_t *)GlobalAlloc(GPTR, sizeof(stack_t) + cchString /* Termination space is part of stack_t */);
    if (pStack)
    {
        lstrcpynA(pStack->text, pcszString, (int)cchString + 1);
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
static char *tstStackPopString(stack_t **ppStackTop)
{
    AssertPtrReturn(ppStackTop, NULL);
    AssertPtrReturn(*ppStackTop, NULL);

    stack_t *pStack = *ppStackTop;

    char *pszString = RTStrDup(pStack->text);
    if (pszString)
    {
        *ppStackTop = pStack->next;
        GlobalFree((HGLOBAL)pStack);
    }

    AssertPtr(pszString);
    return pszString;
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

    /** @todo This ASSUMES that this testcase always is located in the separate "bin/additions" sub directory
     *        and that we currently always repack the Guest Additions stuff in a separate directory.
     *        Might need some more tweaking ... */
    int rc = RTPathAppend(szGuestInstallHelperDll, sizeof(szGuestInstallHelperDll),
                          "../../../repack/resources/VBoxGuestInstallHelper/VBoxGuestInstallHelper.dll");
    if (RT_SUCCESS(rc))
    {
        RTTestIPrintf(RTTESTLVL_ALWAYS, "Using DLL: %s\n", szGuestInstallHelperDll);

        RTLDRMOD hLdrMod;
        rc = RTLdrLoad(szGuestInstallHelperDll, &hLdrMod);
        if (RT_SUCCESS(rc))
        {
            TCHAR szVars[NSIS_MAX_STRLEN] = { 0 };

            PNSIS_PLUGIN_FUNC pfnFileGetArchitecture = NULL;
            rc = RTLdrGetSymbol(hLdrMod, TST_FILEGETARCHITECTURE_NAME, (void**)&pfnFileGetArchitecture);
            if (RT_SUCCESS(rc))
            {
                stack_t *pStack = NULL;
                tstStackPushString(&pStack, "c:\\windows\\system32\\kernel32.dll");

                pfnFileGetArchitecture(NULL /* hWnd */, NSIS_MAX_STRLEN, szVars, &pStack, NULL /* extra */);

                char *pszStack = tstStackPopString(&pStack);
                if (pszStack)
                    RTTestIPrintf(RTTESTLVL_ALWAYS, "Arch: %s\n", pszStack);
                else
                    RTTestIFailed("Getting file arch failed (NSIS API changed?)\n");
                RTStrFree(pszStack);
                tstStackDestroy(pStack);
            }
            else
                RTTestIFailed("Loading FileGetArchitecture failed: %Rrc", rc);

            PNSIS_PLUGIN_FUNC pfnFileGetVendor;
            rc = RTLdrGetSymbol(hLdrMod, TST_FILEGETVENDOR_NAME, (void**)&pfnFileGetVendor);
            if (RT_SUCCESS(rc))
            {
                stack_t *pStack = NULL;
                tstStackPushString(&pStack, "c:\\windows\\system32\\kernel32.dll");
                pfnFileGetVendor(NULL /* hWnd */, NSIS_MAX_STRLEN, szVars, &pStack, NULL /* extra */);
                char *pszStack = tstStackPopString(&pStack);
                if (pszStack)
                    RTTestIPrintf(RTTESTLVL_ALWAYS, "Vendor: %s\n", pszStack);
                else
                    RTTestIFailed("Getting file vendor failed (NSIS API changed?)\n");
                RTStrFree(pszStack);
                tstStackDestroy(pStack);
            }
            else
                RTTestIFailed("Loading FileGetVendor failed: %Rrc", rc);

            RTLdrClose(hLdrMod);
        }
        else
            RTTestIFailed("Loading DLL failed: %Rrc", rc);
    }
    else
        RTTestIFailed("Getting absolute path of DLL failed: %Rrc", rc);

    return RTTestSummaryAndDestroy(hTest);
}

