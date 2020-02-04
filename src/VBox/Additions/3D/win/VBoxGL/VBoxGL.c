/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - OpenGL driver.
 */

/*
 * Copyright (C) 2018-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "GaDrvEnvKMT.h"

#include "stw_winsys.h"
#include "stw_device.h"
#include "stw_context.h"

#include "pipe/p_state.h"
#include "svga3d_reg.h"

#include <iprt/asm.h>

#include <common/wddm/VBoxMPIf.h>

static const char *g_pszSvgaDll =
#ifdef VBOX_WOW64
    "VBoxSVGA-x86.dll"
#else
    "VBoxSVGA.dll"
#endif
;

static struct GaDrvFunctions
{
    PFNGaDrvScreenCreate  pfnGaDrvScreenCreate;
    PFNGaDrvScreenDestroy pfnGaDrvScreenDestroy;
    PFNGaDrvGetWDDMEnv    pfnGaDrvGetWDDMEnv;
    PFNGaDrvGetContextId  pfnGaDrvGetContextId;
    PFNGaDrvGetSurfaceId  pfnGaDrvGetSurfaceId;
    PFNGaDrvContextFlush  pfnGaDrvContextFlush;
} g_drvfuncs;


static HMODULE gaDrvLoadSVGA(struct GaDrvFunctions *pDrvFuncs)
{
    struct VBOXWDDMDLLPROC aDrvProcs[] =
    {
        { "GaDrvScreenCreate",  (FARPROC *)&pDrvFuncs->pfnGaDrvScreenCreate },
        { "GaDrvScreenDestroy", (FARPROC *)&pDrvFuncs->pfnGaDrvScreenDestroy },
        { "GaDrvGetWDDMEnv",    (FARPROC *)&pDrvFuncs->pfnGaDrvGetWDDMEnv },
        { "GaDrvGetContextId",  (FARPROC *)&pDrvFuncs->pfnGaDrvGetContextId },
        { "GaDrvGetSurfaceId",  (FARPROC *)&pDrvFuncs->pfnGaDrvGetSurfaceId },
        { "GaDrvContextFlush",  (FARPROC *)&pDrvFuncs->pfnGaDrvContextFlush },
        { NULL, NULL }
    };

    HMODULE hmod = VBoxWddmLoadSystemDll(g_pszSvgaDll);
    if (hmod)
    {
        VBoxWddmLoadAdresses(hmod, aDrvProcs);
    }
    return hmod;
}

struct stw_shared_surface
{
    D3DKMT_HANDLE hResource;
    D3DKMT_HANDLE hSurface;
    uint32_t u32Sid;
};

static NTSTATUS vboxKmtPresent(D3DKMT_HANDLE hContext, HWND hwnd, D3DKMT_HANDLE hSource, LONG lWidth, LONG lHeight)
{
    RECT r;
    r.left  = 0;
    r.top    = 0;
    r.right  = lWidth;
    r.bottom = lHeight;

    D3DKMT_PRESENT PresentData;
    memset(&PresentData, 0, sizeof(PresentData));
    PresentData.hContext           = hContext;
    PresentData.hWindow            = hwnd;
    PresentData.hSource            = hSource;
    PresentData.hDestination       = 0;
    PresentData.Flags.Blt          = 1;
    PresentData.Flags.SrcRectValid = 1;
    PresentData.Flags.DstRectValid = 1;
    PresentData.SrcRect            = r;
    PresentData.SubRectCnt         = 1;
    PresentData.pSrcSubRects       = &r;
    PresentData.DstRect            = r;

    D3DKMTFUNCTIONS const *d3dkmt = D3DKMTFunctions();
    NTSTATUS Status = d3dkmt->pfnD3DKMTPresent(&PresentData);
    return Status;
}

NTSTATUS vboxKmtOpenSharedSurface(D3DKMT_HANDLE hDevice, D3DKMT_HANDLE hSharedSurface, struct stw_shared_surface *pSurf)
{
    D3DKMTFUNCTIONS const *d3dkmt = D3DKMTFunctions();

    D3DKMT_QUERYRESOURCEINFO QueryResourceInfoData;
    memset(&QueryResourceInfoData, 0, sizeof(QueryResourceInfoData));
    QueryResourceInfoData.hDevice = hDevice;
    QueryResourceInfoData.hGlobalShare = hSharedSurface;

    NTSTATUS Status = d3dkmt->pfnD3DKMTQueryResourceInfo(&QueryResourceInfoData);
    if (Status == STATUS_SUCCESS)
    {
        D3DDDI_OPENALLOCATIONINFO OpenAllocationInfoData;
        memset(&OpenAllocationInfoData, 0, sizeof(OpenAllocationInfoData));

        D3DKMT_OPENRESOURCE OpenResourceData;
        memset(&OpenResourceData, 0, sizeof(OpenResourceData));
        OpenResourceData.hDevice = hDevice;
        OpenResourceData.hGlobalShare = hSharedSurface;
        OpenResourceData.NumAllocations = 1;
        OpenResourceData.pOpenAllocationInfo = &OpenAllocationInfoData;
        if (QueryResourceInfoData.PrivateRuntimeDataSize)
        {
            OpenResourceData.pPrivateRuntimeData = malloc(QueryResourceInfoData.PrivateRuntimeDataSize);
            if (OpenResourceData.pPrivateRuntimeData == NULL)
            {
                Status = STATUS_NOT_SUPPORTED;
            }
            OpenResourceData.PrivateRuntimeDataSize = QueryResourceInfoData.PrivateRuntimeDataSize;
        }
        if (QueryResourceInfoData.ResourcePrivateDriverDataSize)
        {
            OpenResourceData.pResourcePrivateDriverData = malloc(QueryResourceInfoData.ResourcePrivateDriverDataSize);
            if (OpenResourceData.pResourcePrivateDriverData == NULL)
            {
                Status = STATUS_NOT_SUPPORTED;
            }
            OpenResourceData.ResourcePrivateDriverDataSize = QueryResourceInfoData.ResourcePrivateDriverDataSize;
        }
        if (QueryResourceInfoData.TotalPrivateDriverDataSize)
        {
            OpenResourceData.pTotalPrivateDriverDataBuffer = malloc(QueryResourceInfoData.TotalPrivateDriverDataSize);
            if (OpenResourceData.pTotalPrivateDriverDataBuffer == NULL)
            {
                Status = STATUS_NOT_SUPPORTED;
            }
            OpenResourceData.TotalPrivateDriverDataBufferSize = QueryResourceInfoData.TotalPrivateDriverDataSize;
        }

        if (Status == STATUS_SUCCESS)
        {
            Status = d3dkmt->pfnD3DKMTOpenResource(&OpenResourceData);
            if (Status == STATUS_SUCCESS)
            {
                Assert(OpenAllocationInfoData.PrivateDriverDataSize == sizeof(VBOXWDDM_ALLOCINFO));
                VBOXWDDM_ALLOCINFO *pVBoxAllocInfo = (VBOXWDDM_ALLOCINFO *)OpenAllocationInfoData.pPrivateDriverData;
                pSurf->hResource = OpenResourceData.hResource;
                pSurf->hSurface = OpenAllocationInfoData.hAllocation;
                pSurf->u32Sid = pVBoxAllocInfo->hostID;
            }
        }

        if (OpenResourceData.pPrivateRuntimeData != NULL)
        {
            free(OpenResourceData.pPrivateRuntimeData);
        }
        if (OpenResourceData.pResourcePrivateDriverData != NULL)
        {
            free(OpenResourceData.pResourcePrivateDriverData);
        }
        if (OpenResourceData.pTotalPrivateDriverDataBuffer != NULL)
        {
            free(OpenResourceData.pTotalPrivateDriverDataBuffer);
        }
    }

    return Status;
}

NTSTATUS vboxKmtCloseSharedSurface(D3DKMT_HANDLE hDevice, struct stw_shared_surface *pSurf)
{
    D3DKMTFUNCTIONS const *d3dkmt = D3DKMTFunctions();

    D3DKMT_DESTROYALLOCATION DestroyAllocationData;
    memset(&DestroyAllocationData, 0, sizeof(DestroyAllocationData));
    DestroyAllocationData.hDevice   = hDevice;
    DestroyAllocationData.hResource = pSurf->hResource;
    /* "If the OpenGL ICD sets the handle in the hResource member to a non-NULL value,
     * the ICD must set phAllocationList to NULL." and
     * "the AllocationCount member is ignored by the OpenGL runtime."
     */
    // DestroyAllocationData.phAllocationList = NULL;
    // DestroyAllocationData.AllocationCount  = 0;

    NTSTATUS Status = d3dkmt->pfnD3DKMTDestroyAllocation(&DestroyAllocationData);
    Assert(Status == STATUS_SUCCESS);
    return Status;
}


static struct pipe_screen *
wddm_screen_create(void)
{
    struct pipe_screen *screen = NULL;

    if (gaDrvLoadSVGA(&g_drvfuncs))
    {
        WDDMGalliumDriverEnv const *pEnv = GaDrvEnvKmtCreate();
        if (pEnv)
        {
            /// @todo pEnv to include destructor callback, to be called from winsys screen destructor?
            screen = g_drvfuncs.pfnGaDrvScreenCreate(pEnv);
        }
    }

    return screen;
}

static void
wddm_present(struct pipe_screen *screen,
             struct pipe_resource *res,
             HDC hDC)
{
    struct stw_context *ctx = stw_current_context();
    struct pipe_context *pipe = ctx->st->pipe;

    const WDDMGalliumDriverEnv *pEnv = g_drvfuncs.pfnGaDrvGetWDDMEnv(screen);
    if (pEnv)
    {
        /* Get context and kernel-mode handle of the resource. */
        uint32_t u32Cid = g_drvfuncs.pfnGaDrvGetContextId(pipe);
        D3DKMT_HANDLE hContext = GaDrvEnvKmtContextHandle(pEnv, u32Cid);

        uint32_t u32SourceSid = g_drvfuncs.pfnGaDrvGetSurfaceId(screen, res);
        D3DKMT_HANDLE hSource = GaDrvEnvKmtSurfaceHandle(pEnv, u32SourceSid);

        HWND hwnd = WindowFromDC(hDC);

        vboxKmtPresent(hContext, hwnd, hSource, res->width0, res->height0);
    }
}

static boolean
wddm_get_adapter_luid(struct pipe_screen *screen,
                      LUID *pAdapterLuid)
{
    const WDDMGalliumDriverEnv *pEnv = g_drvfuncs.pfnGaDrvGetWDDMEnv(screen);
    if (pEnv)
    {
        GaDrvEnvKmtAdapterLUID(pEnv, pAdapterLuid);
        return true;
    }

    return false;
}

static struct stw_shared_surface *
wddm_shared_surface_open(struct pipe_screen *screen,
                         HANDLE hSharedSurface)
{
    struct stw_shared_surface *surface = NULL;

    const WDDMGalliumDriverEnv *pEnv = g_drvfuncs.pfnGaDrvGetWDDMEnv(screen);
    if (pEnv)
    {
        surface = (struct stw_shared_surface *)malloc(sizeof(struct stw_shared_surface));
        if (surface)
        {
            D3DKMT_HANDLE hDevice = GaDrvEnvKmtDeviceHandle(pEnv);
            NTSTATUS Status = vboxKmtOpenSharedSurface(hDevice, (D3DKMT_HANDLE)hSharedSurface, surface);
            if (Status != STATUS_SUCCESS)
            {
                free(surface);
                surface = NULL;
            }
        }
    }
    return surface;
}

static void
wddm_shared_surface_close(struct pipe_screen *screen,
                         struct stw_shared_surface *surface)
{
    const WDDMGalliumDriverEnv *pEnv = g_drvfuncs.pfnGaDrvGetWDDMEnv(screen);
    if (pEnv)
    {
        D3DKMT_HANDLE hDevice = GaDrvEnvKmtDeviceHandle(pEnv);
        vboxKmtCloseSharedSurface(hDevice, surface);
    }
    free(surface);
}

static void
wddm_compose(struct pipe_screen *screen,
             struct pipe_resource *res,
             struct stw_shared_surface *dest,
             LPCRECT pRect,
             ULONGLONG PresentHistoryToken)
{
    struct stw_context *ctx = stw_current_context();
    struct pipe_context *pipe = ctx->st->pipe;

    /* The ICD asked to present something, make sure that any outstanding commends are submitted. */
    g_drvfuncs.pfnGaDrvContextFlush(pipe);

    uint32_t u32SourceSid = g_drvfuncs.pfnGaDrvGetSurfaceId(screen, res);

    /* Generate SVGA_3D_CMD_SURFACE_COPY command for these resources. */
    struct
    {
        SVGA3dCmdHeader header;
        SVGA3dCmdSurfaceCopy surfaceCopy;
        SVGA3dCopyBox box;
    } command;

    command.header.id   = SVGA_3D_CMD_SURFACE_COPY;
    command.header.size = sizeof(command) - sizeof(SVGA3dCmdHeader);

    command.surfaceCopy.src.sid     = u32SourceSid;
    command.surfaceCopy.src.face    = 0;
    command.surfaceCopy.src.mipmap  = 0;
    command.surfaceCopy.dest.sid    = dest->u32Sid;
    command.surfaceCopy.dest.face   = 0;
    command.surfaceCopy.dest.mipmap = 0;

    command.box.x    = pRect->left;
    command.box.y    = pRect->top;
    command.box.z    = 0;
    command.box.w    = pRect->right - pRect->left;
    command.box.h    = pRect->bottom - pRect->top;
    command.box.d    = 1;
    command.box.srcx = 0;
    command.box.srcy = 0;
    command.box.srcz = 0;

    const WDDMGalliumDriverEnv *pEnv = g_drvfuncs.pfnGaDrvGetWDDMEnv(screen);
    if (pEnv)
    {
        uint32_t u32Cid = g_drvfuncs.pfnGaDrvGetContextId(pipe);
        GaDrvEnvKmtRenderCompose(pEnv, u32Cid, &command, sizeof(command), PresentHistoryToken);
    }
}

static const struct stw_winsys stw_winsys = {
   wddm_screen_create,
   wddm_present,
   wddm_get_adapter_luid,
   wddm_shared_surface_open,
   wddm_shared_surface_close,
   wddm_compose
};

BOOL WINAPI DllMain(HINSTANCE hDLLInst,
                    DWORD fdwReason,
                    LPVOID lpvReserved)
{
    RT_NOREF2(hDLLInst, lpvReserved);

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            D3DKMTLoad();
            stw_init(&stw_winsys);
            stw_init_thread();
            break;

        case DLL_PROCESS_DETACH:
            break;

        case DLL_THREAD_ATTACH:
            stw_init_thread();
            break;

        case DLL_THREAD_DETACH:
            stw_cleanup_thread();
            break;

        default:
            if (lpvReserved == NULL)
            {
               // We're being unloaded from the process.
               stw_cleanup_thread();
               stw_cleanup();
            }
            else
            {
               // Process itself is terminating, and all threads and modules are
               // being detached.
               //
               // The order threads (including llvmpipe rasterizer threads) are
               // destroyed can not be relied up, so it's not safe to cleanup.
               //
               // However global destructors (e.g., LLVM's) will still be called, and
               // if Microsoft OPENGL32.DLL's DllMain is called after us, it will
               // still try to invoke DrvDeleteContext to destroys all outstanding,
               // so set stw_dev to NULL to return immediately if that happens.
               stw_dev = NULL;
            }
            break;
    }

    return TRUE;
}
