/* $Id$ */
/** @file
 * VBoxVMInfo - Virtual machine (guest) information for the host.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#include "VBoxService.h"
#include "VBoxVMInfo.h"
#include "VBoxVMInfoAdditions.h"
#include "VBoxVMInfoUser.h"
#include "VBoxVMInfoNet.h"
#include "VBoxVMInfoOS.h"

static VBOXINFORMATIONCONTEXT gCtx = {0};

int vboxVMInfoWriteProp(VBOXINFORMATIONCONTEXT* a_pCtx, char *a_pszKey, char *a_pszValue)
{
    int rc = VINF_SUCCESS;
    Assert(a_pCtx);
    Assert(a_pszKey);
    /* Not checking for a valid a_pszValue is intentional. */

    char szKeyTemp [_MAX_PATH] = {0};
    char *pszValue = NULL;

    /* Append base path. */
    RTStrPrintf(szKeyTemp, sizeof(szKeyTemp), "/VirtualBox/%s", a_pszKey); /** @todo r=bird: Why didn't you hardcode this into the strings before calling this function? */

    if (a_pszValue != NULL)
    {
        rc = RTStrCurrentCPToUtf8(&pszValue, a_pszValue);
        if (!RT_SUCCESS(rc)) {
            LogRel(("vboxVMInfoThread: Failed to convert the value name \"%s\" to Utf8! Error: %Rrc\n", a_pszValue, rc));
            goto cleanup;
        }
    }

    rc = VbglR3GuestPropWriteValue(a_pCtx->iInfoSvcClientID, szKeyTemp, (a_pszValue == NULL) ? NULL : pszValue);
    if (!RT_SUCCESS(rc))
    {
        LogRel(("vboxVMInfoThread: Failed to store the property \"%s\"=\"%s\"! ClientID: %d, Error: %Rrc\n", szKeyTemp, pszValue, a_pCtx->iInfoSvcClientID, rc));
        goto cleanup;
    }

    if (pszValue != NULL)
        Log(("vboxVMInfoThread: Property written: %s = %s\n", szKeyTemp, pszValue));
    else
        Log(("vboxVMInfoThread: Property deleted: %s\n", szKeyTemp));

cleanup:

    RTStrFree(pszValue);
    return rc;
}

int vboxVMInfoWritePropInt(VBOXINFORMATIONCONTEXT* a_pCtx, char *a_pszKey, int a_iValue)
{
    Assert(a_pCtx);
    Assert(a_pszKey);

    char szBuffer[_MAX_PATH] = {0}; /** @todo r=bird: this is a bit excessive (the size) */
    itoa(a_iValue, szBuffer, 10);

    return vboxVMInfoWriteProp(a_pCtx, a_pszKey, szBuffer);
}

int vboxVMInfoInit(const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread)
{
    Assert(pEnv);
    Assert(ppInstance);

    Log(("vboxVMInfoThread: Init.\n"));

    gCtx.pEnv = pEnv;
    gCtx.fFirstRun = TRUE;
    gCtx.cUsers = INT32_MAX; /* value which isn't reached in real life. */

    int rc = VbglR3GuestPropConnect(&gCtx.iInfoSvcClientID);
    if (!RT_SUCCESS(rc))
        LogRel(("vboxVMInfoThread: Failed to connect to the guest property service! Error: %Rrc\n", rc));
    else
    {
        Log(("vboxVMInfoThread: GuestProp ClientID = %d\n", gCtx.iInfoSvcClientID));

        /* Loading dynamic APIs. */
        HMODULE hKernel32 = LoadLibrary(_T("kernel32"));
        if (NULL != hKernel32)
        {
            gCtx.pfnWTSGetActiveConsoleSessionId = NULL;
            gCtx.pfnWTSGetActiveConsoleSessionId = (fnWTSGetActiveConsoleSessionId)GetProcAddress(hKernel32, "WTSGetActiveConsoleSessionId");

            FreeLibrary(hKernel32);
        }

        *pfStartThread = true;
        *ppInstance = &gCtx;
    }

    return 0;
}

void vboxVMInfoDestroy(const VBOXSERVICEENV *pEnv, void *pInstance)
{
    Assert(pEnv);

    VBOXINFORMATIONCONTEXT *pCtx = (VBOXINFORMATIONCONTEXT *)pInstance;
    Assert(pCtx);

    /** @todo Temporary solution: Zap all values which are not valid anymore when VM goes down (reboot/shutdown).
     * Needs to be replaced with "temporary properties" later. */

    vboxVMInfoWriteProp(pCtx, "GuestInfo/OS/LoggedInUsersList", NULL);
    vboxVMInfoWritePropInt(pCtx, "GuestInfo/OS/LoggedInUsers", 0);
    if (pCtx->cUsers != 0)
        vboxVMInfoWriteProp(pCtx, "GuestInfo/OS/NoLoggedInUsers", "true");

    const char *apszPat[1] = { "/VirtualBox/GuestInfo/Net/*" };
    VbglR3GuestPropDelSet(pCtx->iInfoSvcClientID, &apszPat[0], RT_ELEMENTS(apszPat));
    vboxVMInfoWritePropInt(pCtx, "GuestInfo/Net/Count", 0);

    /* Disconnect from guest properties API. */
    int rc = VbglR3GuestPropDisconnect(pCtx->iInfoSvcClientID);
    if (!RT_SUCCESS(rc))
        LogRel(("vboxVMInfoThread: Failed to disconnect from guest property service! Error: %Rrc\n", rc));

    Log(("vboxVMInfoThread: Destroyed.\n"));
    return;
}

unsigned __stdcall vboxVMInfoThread(void *pInstance)
{
    Assert(pInstance);

    VBOXINFORMATIONCONTEXT *pCtx = (VBOXINFORMATIONCONTEXT *)pInstance;
    bool fTerminate = false;

    Log(("vboxVMInfoThread: Started.\n"));

    if (NULL == pCtx) {
        Log(("vboxVMInfoThread: Invalid context!\n"));
        return -1;
    }

    WSADATA wsaData;
    DWORD cbReturned = 0;
    DWORD dwCnt = 5;

    /* Required for network information. */
    if (WSAStartup(0x0101, &wsaData)) {
         Log(("vboxVMInfoThread: WSAStartup failed! Error: %Rrc\n", RTErrConvertFromWin32(WSAGetLastError())));
         return -1;
    }

    do
    {
        if (dwCnt++ < 5)
        {
            /* Sleep a bit to not eat too much CPU. */
            if (NULL == pCtx->pEnv->hStopEvent)
                Log(("vboxVMInfoThread: Invalid stop event!\n"));

            if (WaitForSingleObject (pCtx->pEnv->hStopEvent, 1000) == WAIT_OBJECT_0)
            {
                Log(("vboxVMInfoThread: Got stop event, terminating ...\n"));
                fTerminate = true;
                break;
            }

            continue;
        }

        dwCnt = 0;

        vboxVMInfoOS(pCtx);
        vboxVMInfoAdditions(pCtx);
        vboxVMInfoNet(pCtx);
        vboxVMInfoUser(pCtx);

        if (pCtx->fFirstRun)
            pCtx->fFirstRun = FALSE;
    }
    while (!fTerminate);

    WSACleanup();
    return 0;
}

