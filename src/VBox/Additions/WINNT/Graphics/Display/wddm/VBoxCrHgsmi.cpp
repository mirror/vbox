#include <VBox/VBoxCrHgsmi.h>
#include <iprt/err.h>

#include "VBoxUhgsmiKmt.h"

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
    g_VBoxCrHgsmiCallbacks = *pCallbacks;
    if (!g_hVBoxCrHgsmiProvider)
    {
        BOOL bRc = GetModuleHandleEx(0, L"VBoxDispD3D", &g_hVBoxCrHgsmiProvider);
//        g_hVBoxCrHgsmiProvider = GetModuleHandle(L"VBoxDispD3D");
        if (bRc)
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
        if (hClient)
            return hClient;
    }
    PVBOXUHGSMI_PRIVATE_KMT pHgsmiGL = gt_pHgsmiGL;
    if (pHgsmiGL)
        return pHgsmiGL->BasePrivate.hClient;
    pHgsmiGL = (PVBOXUHGSMI_PRIVATE_KMT)RTMemAllocZ(sizeof (*pHgsmiGL));
    if (pHgsmiGL)
    {
        HRESULT hr = vboxUhgsmiKmtCreate(pHgsmiGL, TRUE /* bD3D tmp for injection thread*/);
        Assert(hr == S_OK);
        hClient = g_VBoxCrHgsmiCallbacks.pfnClientCreate(&pHgsmiGL->BasePrivate.Base);
        Assert(hClient);
        pHgsmiGL->BasePrivate.hClient = hClient;
        gt_pHgsmiGL = pHgsmiGL;
    }
    else
        hClient = NULL;
    return hClient;
}

VBOXCRHGSMI_DECL(int) VBoxCrHgsmiTerm()
{
    PVBOXUHGSMI_PRIVATE_KMT pHgsmiGL = gt_pHgsmiGL;
    if (pHgsmiGL)
    {
        g_VBoxCrHgsmiCallbacks.pfnClientDestroy(pHgsmiGL->BasePrivate.hClient);
        vboxUhgsmiKmtDestroy(pHgsmiGL);
        gt_pHgsmiGL = NULL;
    }

    if (g_pfnVBoxDispCrHgsmiTerm)
        g_pfnVBoxDispCrHgsmiTerm();
    return VINF_SUCCESS;
}


///* to be used by injection thread and by ogl ICD driver for hgsmi initialization*/
//VBOXCRHGSMI_DECL(int) VBoxCrHgsmiCustomCreate(PVBOXUHGSMI *ppHgsmi)
//{
//    PVBOXUHGSMI_PRIVATE_KMT pHgsmi = RTMemAllocZ(sizeof (*pHgsmi));
//    if (pHgsmi)
//    {
//        HRESULT hr = vboxUhgsmiKmtCreate(pHgsmi, FALSE);
//        Assert(hr == S_OK);
//        if (hr == S_OK)
//        {
//            *ppHgsmi = &pHgsmi->BasePrivate.Base;
//            return VINF_SUCCESS;
//        }
//        RTMemFree(pHgsmi);
//        return VERR_GENERAL_FAILURE;
//    }
//    return VERR_NO_MEMORY;
//}
//
//VBOXCRHGSMI_DECL(int) VBoxCrHgsmiCustomDestroy(PVBOXUHGSMI pHgsmi)
//{
//    PVBOXUHGSMI_PRIVATE_KMT pHgsmiKmt = VBOXUHGSMIKMT_GET(pHgsmi);
//    HRESULT hr = vboxUhgsmiKmtDestroy(pHgsmiKmt, FALSE);
//    Assert(hr == S_OK);
//    if (hr == S_OK)
//    {
//        RTMemFree(pHgsmiKmt);
//        return VINF_SUCCESS;
//    }
//    return VERR_GENERAL_FAILURE;
//}

