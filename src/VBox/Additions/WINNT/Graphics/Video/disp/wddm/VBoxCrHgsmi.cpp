/* $Id$ */

/** @file
 * VBoxVideo Display D3D User mode dll
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

#include <VBox/VBoxCrHgsmi.h>
#include <iprt/err.h>

#include "VBoxUhgsmiKmt.h"

static VBOXDISPKMT_CALLBACKS g_VBoxCrHgsmiKmtCallbacks;
static int g_bVBoxKmtCallbacksInited = 0;
static uint32_t g_VBoxCrVersionMajor;
static uint32_t g_VBoxCrVersionMinor;

#ifdef VBOX_CRHGSMI_WITH_D3DDEV
static VBOXCRHGSMI_CALLBACKS g_VBoxCrHgsmiCallbacks;
static HMODULE g_hVBoxCrHgsmiProvider = NULL;
static uint32_t g_cVBoxCrHgsmiProvider = 0;


typedef VBOXWDDMDISP_DECL(int) FNVBOXDISPCRHGSMI_INIT(PVBOXCRHGSMI_CALLBACKS pCallbacks);
typedef FNVBOXDISPCRHGSMI_INIT *PFNVBOXDISPCRHGSMI_INIT;

typedef VBOXWDDMDISP_DECL(int) FNVBOXDISPCRHGSMI_TERM();
typedef FNVBOXDISPCRHGSMI_TERM *PFNVBOXDISPCRHGSMI_TERM;

typedef VBOXWDDMDISP_DECL(HVBOXCRHGSMI_CLIENT) FNVBOXDISPCRHGSMI_QUERY_CLIENT();
typedef FNVBOXDISPCRHGSMI_QUERY_CLIENT *PFNVBOXDISPCRHGSMI_QUERY_CLIENT;

static PFNVBOXDISPCRHGSMI_INIT g_pfnVBoxDispCrHgsmiInit = NULL;
static PFNVBOXDISPCRHGSMI_TERM g_pfnVBoxDispCrHgsmiTerm = NULL;
static PFNVBOXDISPCRHGSMI_QUERY_CLIENT g_pfnVBoxDispCrHgsmiQueryClient = NULL;

VBOXCRHGSMI_DECL(int) VBoxCrHgsmiInit(PVBOXCRHGSMI_CALLBACKS pCallbacks)
{
    if (!g_bVBoxKmtCallbacksInited)
    {
        HRESULT hr = vboxDispKmtCallbacksInit(&g_VBoxCrHgsmiKmtCallbacks);
        Assert(hr == S_OK);
        if (hr == S_OK)
            g_bVBoxKmtCallbacksInited = 1;
        else
            g_bVBoxKmtCallbacksInited = -1;
    }

    Assert(g_bVBoxKmtCallbacksInited);
    if (g_bVBoxKmtCallbacksInited < 0)
    {
        Assert(0);
        return VERR_NOT_SUPPORTED;
    }

    g_VBoxCrHgsmiCallbacks = *pCallbacks;
    if (!g_hVBoxCrHgsmiProvider)
    {
        g_hVBoxCrHgsmiProvider = GetModuleHandle(L"VBoxDispD3D");
        if (g_hVBoxCrHgsmiProvider)
        {
            g_hVBoxCrHgsmiProvider = LoadLibrary(L"VBoxDispD3D");
        }

        if (g_hVBoxCrHgsmiProvider)
        {
            g_pfnVBoxDispCrHgsmiInit = (PFNVBOXDISPCRHGSMI_INIT)GetProcAddress(g_hVBoxCrHgsmiProvider, "VBoxDispCrHgsmiInit");
            Assert(g_pfnVBoxDispCrHgsmiInit);
            if (g_pfnVBoxDispCrHgsmiInit)
            {
                g_pfnVBoxDispCrHgsmiInit(pCallbacks);
            }

            g_pfnVBoxDispCrHgsmiTerm = (PFNVBOXDISPCRHGSMI_TERM)GetProcAddress(g_hVBoxCrHgsmiProvider, "VBoxDispCrHgsmiTerm");
            Assert(g_pfnVBoxDispCrHgsmiTerm);

            g_pfnVBoxDispCrHgsmiQueryClient = (PFNVBOXDISPCRHGSMI_QUERY_CLIENT)GetProcAddress(g_hVBoxCrHgsmiProvider, "VBoxDispCrHgsmiQueryClient");
            Assert(g_pfnVBoxDispCrHgsmiQueryClient);
        }
#ifdef DEBUG_misha
        else
        {
            DWORD winEr = GetLastError();
            Assert(0);
        }
#endif
    }

    if (g_hVBoxCrHgsmiProvider)
    {
        if (g_pfnVBoxDispCrHgsmiInit)
        {
            g_pfnVBoxDispCrHgsmiInit(pCallbacks);
        }
        ++g_cVBoxCrHgsmiProvider;
        return VINF_SUCCESS;
    }

    /* we're called from ogl ICD driver*/
    Assert(0);

    return VINF_SUCCESS;
}

static __declspec(thread) PVBOXUHGSMI_PRIVATE_KMT gt_pHgsmiGL = NULL;

VBOXCRHGSMI_DECL(HVBOXCRHGSMI_CLIENT) VBoxCrHgsmiQueryClient()
{

    HVBOXCRHGSMI_CLIENT hClient;
    if (g_pfnVBoxDispCrHgsmiQueryClient)
    {
        hClient = g_pfnVBoxDispCrHgsmiQueryClient();
//#ifdef DEBUG_misha
//        Assert(hClient);
//#endif
        if (hClient)
            return hClient;
    }
    PVBOXUHGSMI_PRIVATE_KMT pHgsmiGL = gt_pHgsmiGL;
    if (pHgsmiGL)
    {
        Assert(pHgsmiGL->BasePrivate.hClient);
        return pHgsmiGL->BasePrivate.hClient;
    }
    pHgsmiGL = (PVBOXUHGSMI_PRIVATE_KMT)RTMemAllocZ(sizeof (*pHgsmiGL));
    if (pHgsmiGL)
    {
#if 0
        HRESULT hr = vboxUhgsmiKmtCreate(pHgsmiGL, TRUE /* bD3D tmp for injection thread*/);
#else
        HRESULT hr = vboxUhgsmiKmtEscCreate(pHgsmiGL, TRUE /* bD3D tmp for injection thread*/);
#endif
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            hClient = g_VBoxCrHgsmiCallbacks.pfnClientCreate(&pHgsmiGL->BasePrivate.Base);
            Assert(hClient);
            if (hClient)
            {
                pHgsmiGL->BasePrivate.hClient = hClient;
                gt_pHgsmiGL = pHgsmiGL;
                return hClient;
            }
            vboxUhgsmiKmtDestroy(pHgsmiGL);
        }
        RTMemFree(pHgsmiGL);
    }

    return NULL;
}
#else
static int vboxCrHgsmiInitPerform(VBOXDISPKMT_CALLBACKS *pCallbacks)
{
    HRESULT hr = vboxDispKmtCallbacksInit(pCallbacks);
    /*Assert(hr == S_OK);*/
    if (hr == S_OK)
    {
        /* check if we can create the hgsmi */
        PVBOXUHGSMI pHgsmi = VBoxCrHgsmiCreate();
        if (pHgsmi)
        {
            /* yes, we can, so this is wddm mode */
            VBoxCrHgsmiDestroy(pHgsmi);
            Log(("CrHgsmi: WDDM mode supported\n"));
            return 1;
        }
        vboxDispKmtCallbacksTerm(pCallbacks);
    }
    Log(("CrHgsmi: unsupported\n"));
    return -1;
}

VBOXCRHGSMI_DECL(int) VBoxCrHgsmiInit(uint32_t crVersionMajor, uint32_t crVersionMinor)
{
    if (!g_bVBoxKmtCallbacksInited)
    {
        g_bVBoxKmtCallbacksInited = vboxCrHgsmiInitPerform(&g_VBoxCrHgsmiKmtCallbacks);
        Assert(g_bVBoxKmtCallbacksInited);
        if (g_bVBoxKmtCallbacksInited)
        {
            g_VBoxCrVersionMajor = crVersionMajor;
            g_VBoxCrVersionMinor = crVersionMinor;
        }
    }

    return g_bVBoxKmtCallbacksInited > 0 ? VINF_SUCCESS : VERR_NOT_SUPPORTED;
}

VBOXCRHGSMI_DECL(PVBOXUHGSMI) VBoxCrHgsmiCreate()
{
    PVBOXUHGSMI_PRIVATE_KMT pHgsmiGL = (PVBOXUHGSMI_PRIVATE_KMT)RTMemAllocZ(sizeof (*pHgsmiGL));
    if (pHgsmiGL)
    {
#if 0
        HRESULT hr = vboxUhgsmiKmtCreate(pHgsmiGL, g_VBoxCrVersionMajor, g_VBoxCrVersionMinor, TRUE /* bD3D tmp for injection thread*/);
#else
        HRESULT hr = vboxUhgsmiKmtEscCreate(pHgsmiGL, g_VBoxCrVersionMajor, g_VBoxCrVersionMinor, TRUE /* bD3D tmp for injection thread*/);
#endif
        Log(("CrHgsmi: faled to create KmtEsc UHGSMI instance, hr (0x%x)\n", hr));
        if (hr == S_OK)
        {
            return &pHgsmiGL->BasePrivate.Base;
        }
        RTMemFree(pHgsmiGL);
    }

    return NULL;
}

VBOXCRHGSMI_DECL(void) VBoxCrHgsmiDestroy(PVBOXUHGSMI pHgsmi)
{
    PVBOXUHGSMI_PRIVATE_KMT pHgsmiGL = VBOXUHGSMIKMT_GET(pHgsmi);
    HRESULT hr = vboxUhgsmiKmtDestroy(pHgsmiGL);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        RTMemFree(pHgsmiGL);
    }
}
#endif

VBOXCRHGSMI_DECL(int) VBoxCrHgsmiTerm()
{
#if 0
    PVBOXUHGSMI_PRIVATE_KMT pHgsmiGL = gt_pHgsmiGL;
    if (pHgsmiGL)
    {
        g_VBoxCrHgsmiCallbacks.pfnClientDestroy(pHgsmiGL->BasePrivate.hClient);
        vboxUhgsmiKmtDestroy(pHgsmiGL);
        gt_pHgsmiGL = NULL;
    }

    if (g_pfnVBoxDispCrHgsmiTerm)
        g_pfnVBoxDispCrHgsmiTerm();
#endif
    if (g_bVBoxKmtCallbacksInited > 0)
    {
        vboxDispKmtCallbacksTerm(&g_VBoxCrHgsmiKmtCallbacks);
    }
    return VINF_SUCCESS;
}

VBOXCRHGSMI_DECL(void) VBoxCrHgsmiLog(char * szString)
{
    vboxVDbgPrint(("%s", szString));
}

VBOXCRHGSMI_DECL(int) VBoxCrHgsmiCtlConGetClientID(PVBOXUHGSMI pHgsmi, uint32_t *pu32ClientID)
{
    PVBOXUHGSMI_PRIVATE_BASE pHgsmiPrivate = (PVBOXUHGSMI_PRIVATE_BASE)pHgsmi;
    int rc = VBoxCrHgsmiPrivateCtlConGetClientID(pHgsmiPrivate, pu32ClientID);
    if (!RT_SUCCESS(rc))
    {
        WARN(("VBoxCrHgsmiPrivateCtlConCall failed with rc (%d)", rc));
    }
    return rc;
}

VBOXCRHGSMI_DECL(int) VBoxCrHgsmiCtlConCall(PVBOXUHGSMI pHgsmi, struct VBoxGuestHGCMCallInfo *pCallInfo, int cbCallInfo)
{
    PVBOXUHGSMI_PRIVATE_BASE pHgsmiPrivate = (PVBOXUHGSMI_PRIVATE_BASE)pHgsmi;
    int rc = VBoxCrHgsmiPrivateCtlConCall(pHgsmiPrivate, pCallInfo, cbCallInfo);
    if (!RT_SUCCESS(rc))
    {
        WARN(("VBoxCrHgsmiPrivateCtlConCall failed with rc (%d)", rc));
    }
    return rc;
}
