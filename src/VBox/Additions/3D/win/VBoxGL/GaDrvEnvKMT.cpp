/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - Gallium driver interface to the WDDM miniport driver using Kernel Mode Thunks.
 */

/*
 * Copyright (C) 2016-2018 Oracle Corporation
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

#include <UmHlpInternal.h>

#include "svga3d_reg.h"

#include <common/wddm/VBoxMPIf.h>

#include <iprt/assertcompile.h>
#include <iprt/param.h> /* For PAGE_SIZE */

AssertCompile(sizeof(HANDLE) >= sizeof(D3DKMT_HANDLE));


/*
 * AVL configuration.
 */
#define KAVL_FN(a)                  RTAvlU32##a
#define KAVL_MAX_STACK              27  /* Up to 2^24 nodes. */
#define KAVL_CHECK_FOR_EQUAL_INSERT 1   /* No duplicate keys! */
#define KAVLNODECORE                AVLU32NODECORE
#define PKAVLNODECORE               PAVLU32NODECORE
#define PPKAVLNODECORE              PPAVLU32NODECORE
#define KAVLKEY                     AVLU32KEY
#define PKAVLKEY                    PAVLU32KEY
#define KAVLENUMDATA                AVLU32ENUMDATA
#define PKAVLENUMDATA               PAVLU32ENUMDATA
#define PKAVLCALLBACK               PAVLU32CALLBACK


/*
 * AVL Compare macros
 */
#define KAVL_G(key1, key2)          ( (key1) >  (key2) )
#define KAVL_E(key1, key2)          ( (key1) == (key2) )
#define KAVL_NE(key1, key2)         ( (key1) != (key2) )


#include <iprt/avl.h>

/*
 * Include the code.
 */
#define SSToDS(ptr) ptr
#define KMAX RT_MAX
#define kASSERT(_e) do { } while (0)
#include "avl_Base.cpp.h"
#include "avl_Get.cpp.h"
//#include "avl_GetBestFit.cpp.h"
//#include "avl_RemoveBestFit.cpp.h"
//#include "avl_DoWithAll.cpp.h"
#include "avl_Destroy.cpp.h"


typedef struct GaKmtCallbacks
{
    D3DKMT_HANDLE hAdapter;
    D3DKMT_HANDLE hDevice;
    D3DKMTFUNCTIONS const *d3dkmt;
    LUID AdapterLuid;
} GaKmtCallbacks;

class GaDrvEnvKmt
{
    public:
        GaDrvEnvKmt();
        ~GaDrvEnvKmt();

        HRESULT Init(void);

        const WDDMGalliumDriverEnv *Env();

        /*
         * KMT specific helpers.
         */
        int drvEnvKmtRenderCompose(uint32_t u32Cid,
                                   void *pvCommands,
                                   uint32_t cbCommands,
                                   ULONGLONG PresentHistoryToken);
        D3DKMT_HANDLE drvEnvKmtContextHandle(uint32_t u32Cid);
        D3DKMT_HANDLE drvEnvKmtSurfaceHandle(uint32_t u32Sid);

        GaKmtCallbacks mKmtCallbacks;

    private:

        VBOXGAHWINFO mHWInfo;

        /* Map to convert context id (cid) to WDDM context information (GAWDDMCONTEXTINFO).
         * Key is the 32 bit context id.
         */
        AVLU32TREE mContextTree;

        /* Map to convert surface id (sid) to WDDM surface information (GAWDDMSURFACEINFO).
         * Key is the 32 bit surface id.
         */
        AVLU32TREE mSurfaceTree;

        WDDMGalliumDriverEnv mEnv;

        static DECLCALLBACK(uint32_t) gaEnvContextCreate(void *pvEnv,
                                                         boolean extended,
                                                         boolean vgpu10);
        static DECLCALLBACK(void) gaEnvContextDestroy(void *pvEnv,
                                                      uint32_t u32Cid);
        static DECLCALLBACK(int) gaEnvSurfaceDefine(void *pvEnv,
                                                    GASURFCREATE *pCreateParms,
                                                    GASURFSIZE *paSizes,
                                                    uint32_t cSizes,
                                                    uint32_t *pu32Sid);
        static DECLCALLBACK(int) gaEnvSurfaceDestroy(void *pvEnv,
                                                     uint32_t u32Sid);
        static DECLCALLBACK(int) gaEnvRender(void *pvEnv,
                                             uint32_t u32Cid,
                                             void *pvCommands,
                                             uint32_t cbCommands,
                                             GAFENCEQUERY *pFenceQuery);
        static DECLCALLBACK(void) gaEnvFenceUnref(void *pvEnv,
                                                  uint32_t u32FenceHandle);
        static DECLCALLBACK(int) gaEnvFenceQuery(void *pvEnv,
                                                 uint32_t u32FenceHandle,
                                                 GAFENCEQUERY *pFenceQuery);
        static DECLCALLBACK(int) gaEnvFenceWait(void *pvEnv,
                                                uint32_t u32FenceHandle,
                                                uint32_t u32TimeoutUS);
        static DECLCALLBACK(int) gaEnvRegionCreate(void *pvEnv,
                                                   uint32_t u32RegionSize,
                                                   uint32_t *pu32GmrId,
                                                   void **ppvMap);
        static DECLCALLBACK(void) gaEnvRegionDestroy(void *pvEnv,
                                                     uint32_t u32GmrId,
                                                     void *pvMap);

        /*
         * Internal.
         */
        int doRender(uint32_t u32Cid, void *pvCommands, uint32_t cbCommands,
                     GAFENCEQUERY *pFenceQuery, ULONGLONG PresentHistoryToken, bool fPresentRedirected);
};

typedef struct GAWDDMCONTEXTINFO
{
    AVLU32NODECORE            Core;
    D3DKMT_HANDLE             hContext;
    VOID                     *pCommandBuffer;
    UINT                      CommandBufferSize;
    D3DDDI_ALLOCATIONLIST    *pAllocationList;
    UINT                      AllocationListSize;
    D3DDDI_PATCHLOCATIONLIST *pPatchLocationList;
    UINT                      PatchLocationListSize;
} GAWDDMCONTEXTINFO;

typedef struct GAWDDMSURFACEINFO
{
    AVLU32NODECORE            Core;
    D3DKMT_HANDLE             hAllocation;
} GAWDDMSURFACEINFO;


/// @todo vboxDdi helpers must return NTSTATUS
static NTSTATUS
vboxDdiQueryAdapterInfo(GaKmtCallbacks *pKmtCallbacks,
                        D3DKMT_HANDLE hAdapter,
                        VBOXWDDM_QAI *pAdapterInfo,
                        uint32_t cbAdapterInfo)
{
    NTSTATUS                  Status;
    D3DKMT_QUERYADAPTERINFO   kmtQAI;

    memset(&kmtQAI, 0, sizeof(kmtQAI));
    kmtQAI.hAdapter              = hAdapter;
    kmtQAI.Type                  = KMTQAITYPE_UMDRIVERPRIVATE;
    kmtQAI.pPrivateDriverData    = pAdapterInfo;
    kmtQAI.PrivateDriverDataSize = cbAdapterInfo;

    Status = pKmtCallbacks->d3dkmt->pfnD3DKMTQueryAdapterInfo(&kmtQAI);
    return Status;
}

static void
vboxDdiDeviceDestroy(GaKmtCallbacks *pKmtCallbacks,
                     D3DKMT_HANDLE hDevice)
{
    if (hDevice)
    {
        D3DKMT_DESTROYDEVICE kmtDestroyDevice;
        memset(&kmtDestroyDevice, 0, sizeof(kmtDestroyDevice));
        kmtDestroyDevice.hDevice = hDevice;
        pKmtCallbacks->d3dkmt->pfnD3DKMTDestroyDevice(&kmtDestroyDevice);
    }
}

static NTSTATUS
vboxDdiDeviceCreate(GaKmtCallbacks *pKmtCallbacks,
                    D3DKMT_HANDLE *phDevice)
{
    NTSTATUS                  Status;
    D3DKMT_CREATEDEVICE       kmtCreateDevice;

    memset(&kmtCreateDevice, 0, sizeof(kmtCreateDevice));
    kmtCreateDevice.hAdapter = pKmtCallbacks->hAdapter;
    // kmtCreateDevice.Flags = 0;

    Status = pKmtCallbacks->d3dkmt->pfnD3DKMTCreateDevice(&kmtCreateDevice);
    if (Status == STATUS_SUCCESS)
    {
        *phDevice = kmtCreateDevice.hDevice;
    }
    return Status;
}

static NTSTATUS
vboxDdiContextGetId(GaKmtCallbacks *pKmtCallbacks,
                    D3DKMT_HANDLE hContext,
                    uint32_t *pu32Cid)
{
    NTSTATUS                  Status;
    D3DKMT_ESCAPE             kmtEscape;
    VBOXDISPIFESCAPE_GAGETCID data;

    memset(&data, 0, sizeof(data));
    data.EscapeHdr.escapeCode  = VBOXESC_GAGETCID;
    // data.EscapeHdr.cmdSpecific = 0;
    // data.u32Cid                = 0;

    /* If the user-mode display driver sets hContext to a non-NULL value, the driver must
     * have also set hDevice to a non-NULL value...
     */
    memset(&kmtEscape, 0, sizeof(kmtEscape));
    kmtEscape.hAdapter              = pKmtCallbacks->hAdapter;
    kmtEscape.hDevice               = pKmtCallbacks->hDevice;
    kmtEscape.Type                  = D3DKMT_ESCAPE_DRIVERPRIVATE;
    // kmtEscape.Flags.HardwareAccess  = 0;
    kmtEscape.pPrivateDriverData    = &data;
    kmtEscape.PrivateDriverDataSize = sizeof(data);
    kmtEscape.hContext              = hContext;

    Status = pKmtCallbacks->d3dkmt->pfnD3DKMTEscape(&kmtEscape);
    if (Status == STATUS_SUCCESS)
    {
        *pu32Cid = data.u32Cid;
        return S_OK;
    }
    return E_FAIL;
}

static void
vboxDdiContextDestroy(GaKmtCallbacks *pKmtCallbacks,
                      GAWDDMCONTEXTINFO *pContextInfo)
{
    if (pContextInfo->hContext)
    {
        D3DKMT_DESTROYCONTEXT kmtDestroyContext;
        memset(&kmtDestroyContext, 0, sizeof(kmtDestroyContext));
        kmtDestroyContext.hContext = pContextInfo->hContext;
        pKmtCallbacks->d3dkmt->pfnD3DKMTDestroyContext(&kmtDestroyContext);
    }
}

static HRESULT
vboxDdiContextCreate(GaKmtCallbacks *pKmtCallbacks,
                     void *pvPrivateData, uint32_t cbPrivateData,
                     GAWDDMCONTEXTINFO *pContextInfo)
{
    NTSTATUS                  Status;
    D3DKMT_CREATECONTEXT      kmtCreateContext;

    memset(&kmtCreateContext, 0, sizeof(kmtCreateContext));
    kmtCreateContext.hDevice = pKmtCallbacks->hDevice;
    // kmtCreateContext.NodeOrdinal = 0;
    // kmtCreateContext.EngineAffinity = 0;
    // kmtCreateContext.Flags.Value = 0;
    kmtCreateContext.pPrivateDriverData = pvPrivateData;
    kmtCreateContext.PrivateDriverDataSize = cbPrivateData;
    kmtCreateContext.ClientHint = D3DKMT_CLIENTHINT_OPENGL;

    Status = pKmtCallbacks->d3dkmt->pfnD3DKMTCreateContext(&kmtCreateContext);
    if (Status == STATUS_SUCCESS)
    {
        /* Query cid. */
        uint32_t u32Cid = 0;
        Status = vboxDdiContextGetId(pKmtCallbacks, kmtCreateContext.hContext, &u32Cid);
        if (Status == STATUS_SUCCESS)
        {
            pContextInfo->Core.Key              = u32Cid;
            pContextInfo->hContext              = kmtCreateContext.hContext;
            pContextInfo->pCommandBuffer        = kmtCreateContext.pCommandBuffer;
            pContextInfo->CommandBufferSize     = kmtCreateContext.CommandBufferSize;
            pContextInfo->pAllocationList       = kmtCreateContext.pAllocationList;
            pContextInfo->AllocationListSize    = kmtCreateContext.AllocationListSize;
            pContextInfo->pPatchLocationList    = kmtCreateContext.pPatchLocationList;
            pContextInfo->PatchLocationListSize = kmtCreateContext.PatchLocationListSize;
        }
        else
        {
            vboxDdiContextDestroy(pKmtCallbacks, pContextInfo);
        }
    }

    return (Status == STATUS_SUCCESS) ? S_OK : E_FAIL;
}

/* static */ DECLCALLBACK(void)
GaDrvEnvKmt::gaEnvContextDestroy(void *pvEnv,
                                 uint32_t u32Cid)
{
    GaDrvEnvKmt *pThis = (GaDrvEnvKmt *)pvEnv;

    GAWDDMCONTEXTINFO *pContextInfo = (GAWDDMCONTEXTINFO *)RTAvlU32Remove(&pThis->mContextTree, u32Cid);
    if (pContextInfo)
    {
        vboxDdiContextDestroy(&pThis->mKmtCallbacks, pContextInfo);
        memset(pContextInfo, 0, sizeof(*pContextInfo));
        free(pContextInfo);
    }
}

D3DKMT_HANDLE GaDrvEnvKmt::drvEnvKmtContextHandle(uint32_t u32Cid)
{
    GAWDDMCONTEXTINFO *pContextInfo = (GAWDDMCONTEXTINFO *)RTAvlU32Get(&mContextTree, u32Cid);
    Assert(pContextInfo);
    return pContextInfo ? pContextInfo->hContext : 0;
}

/* static */ DECLCALLBACK(uint32_t)
GaDrvEnvKmt::gaEnvContextCreate(void *pvEnv,
                                boolean extended,
                                boolean vgpu10)
{
    GaDrvEnvKmt *pThis = (GaDrvEnvKmt *)pvEnv;

    VBOXWDDM_CREATECONTEXT_INFO privateData;
    GAWDDMCONTEXTINFO *pContextInfo;
    HRESULT hr;

    pContextInfo = (GAWDDMCONTEXTINFO *)malloc(sizeof(GAWDDMCONTEXTINFO));
    if (!pContextInfo)
        return (uint32_t)-1;

    memset(&privateData, 0, sizeof(privateData));
    privateData.u32IfVersion   = 9;
    privateData.enmType        = VBOXWDDM_CONTEXT_TYPE_GA_3D;
    privateData.u.vmsvga.u32Flags  = extended? VBOXWDDM_F_GA_CONTEXT_EXTENDED: 0;
    privateData.u.vmsvga.u32Flags |= vgpu10? VBOXWDDM_F_GA_CONTEXT_VGPU10: 0;

    hr = vboxDdiContextCreate(&pThis->mKmtCallbacks,
                              &privateData, sizeof(privateData), pContextInfo);
    if (SUCCEEDED(hr))
    {
        if (RTAvlU32Insert(&pThis->mContextTree, &pContextInfo->Core))
        {
            return pContextInfo->Core.Key;
        }

        vboxDdiContextDestroy(&pThis->mKmtCallbacks,
                              pContextInfo);
    }

    Assert(0);

    free(pContextInfo);
    return (uint32_t)-1;
}

static D3DDDIFORMAT svgaToD3DDDIFormat(SVGA3dSurfaceFormat format)
{
    switch (format)
    {
        case SVGA3D_X8R8G8B8:       return D3DDDIFMT_X8R8G8B8;
        case SVGA3D_A8R8G8B8:       return D3DDDIFMT_A8R8G8B8;
        case SVGA3D_ALPHA8:         return D3DDDIFMT_A8;
        case SVGA3D_R8G8B8A8_UNORM: return D3DDDIFMT_A8B8G8R8;
        case SVGA3D_A4R4G4B4:       return D3DDDIFMT_A4R4G4B4;
        case SVGA3D_LUMINANCE8:     return D3DDDIFMT_L8;
        case SVGA3D_A1R5G5B5:       return D3DDDIFMT_A1R5G5B5;
        case SVGA3D_LUMINANCE8_ALPHA8: return D3DDDIFMT_A8L8;
        default: break;
    }

    Assert(0);
    return D3DDDIFMT_UNKNOWN;
}

/* static */ DECLCALLBACK(int)
GaDrvEnvKmt::gaEnvSurfaceDefine(void *pvEnv,
                                GASURFCREATE *pCreateParms,
                                GASURFSIZE *paSizes,
                                uint32_t cSizes,
                                uint32_t *pu32Sid)
{
    GaDrvEnvKmt *pThis = (GaDrvEnvKmt *)pvEnv;

    NTSTATUS                          Status;
    D3DKMT_ESCAPE                     EscapeData;
    VBOXDISPIFESCAPE_GASURFACEDEFINE *pData;
    uint32_t                          cbAlloc;
    uint8_t                          *pu8Req;
    uint32_t                          cbReq;

    /* Size of the SVGA request data */
    cbReq = sizeof(GASURFCREATE) + cSizes * sizeof(GASURFSIZE);
    /* How much to allocate for WDDM escape data. */
    cbAlloc =   sizeof(VBOXDISPIFESCAPE_GASURFACEDEFINE)
              + cbReq;

    pData = (VBOXDISPIFESCAPE_GASURFACEDEFINE *)malloc(cbAlloc);
    if (!pData)
        return -1;

    pData->EscapeHdr.escapeCode  = VBOXESC_GASURFACEDEFINE;
    // pData->EscapeHdr.cmdSpecific = 0;
    // pData->u32Sid                = 0;
    pData->cbReq                 = cbReq;
    pData->cSizes                = cSizes;

    pu8Req = (uint8_t *)&pData[1];
    memcpy(pu8Req, pCreateParms, sizeof(GASURFCREATE));
    memcpy(&pu8Req[sizeof(GASURFCREATE)], paSizes, cSizes * sizeof(GASURFSIZE));

    memset(&EscapeData, 0, sizeof(EscapeData));
    EscapeData.hAdapter              = pThis->mKmtCallbacks.hAdapter;
    EscapeData.hDevice               = pThis->mKmtCallbacks.hDevice;
    EscapeData.Type                  = D3DKMT_ESCAPE_DRIVERPRIVATE;
    EscapeData.Flags.HardwareAccess  = 1;
    EscapeData.pPrivateDriverData    = pData;
    EscapeData.PrivateDriverDataSize = cbAlloc;
    // EscapeData.hContext              = 0;

    Status = pThis->mKmtCallbacks.d3dkmt->pfnD3DKMTEscape(&EscapeData);
    if (Status == STATUS_SUCCESS)
    {
        /* Create a kernel mode allocation for render targets,
         * because we will need kernel mode handles for Present.
         */
        if (pCreateParms->flags & SVGA3D_SURFACE_HINT_RENDERTARGET)
        {
            GAWDDMSURFACEINFO *pSurfaceInfo = (GAWDDMSURFACEINFO *)malloc(sizeof(GAWDDMSURFACEINFO));
            if (pSurfaceInfo)
            {
                memset(pSurfaceInfo, 0, sizeof(GAWDDMSURFACEINFO));

                VBOXWDDM_ALLOCINFO wddmAllocInfo;
                memset(&wddmAllocInfo, 0, sizeof(wddmAllocInfo));

                wddmAllocInfo.enmType             = VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC;
                wddmAllocInfo.fFlags.RenderTarget = 1;
                wddmAllocInfo.hSharedHandle       = 0;
                wddmAllocInfo.hostID              = pData->u32Sid;
                wddmAllocInfo.SurfDesc.slicePitch = 0;
                wddmAllocInfo.SurfDesc.depth      = paSizes[0].cDepth;
                wddmAllocInfo.SurfDesc.width      = paSizes[0].cWidth;
                wddmAllocInfo.SurfDesc.height     = paSizes[0].cHeight;
                wddmAllocInfo.SurfDesc.format     = svgaToD3DDDIFormat((SVGA3dSurfaceFormat)pCreateParms->format);
                wddmAllocInfo.SurfDesc.VidPnSourceId = 0;
                wddmAllocInfo.SurfDesc.bpp        = vboxWddmCalcBitsPerPixel(wddmAllocInfo.SurfDesc.format);
                wddmAllocInfo.SurfDesc.pitch      = vboxWddmCalcPitch(wddmAllocInfo.SurfDesc.width,
                                                                      wddmAllocInfo.SurfDesc.format);
                wddmAllocInfo.SurfDesc.cbSize     = vboxWddmCalcSize(wddmAllocInfo.SurfDesc.pitch,
                                                                     wddmAllocInfo.SurfDesc.height,
                                                                     wddmAllocInfo.SurfDesc.format);
                wddmAllocInfo.SurfDesc.d3dWidth   = vboxWddmCalcWidthForPitch(wddmAllocInfo.SurfDesc.pitch,
                                                                              wddmAllocInfo.SurfDesc.format);

                D3DDDI_ALLOCATIONINFO AllocationInfo;
                memset(&AllocationInfo, 0, sizeof(AllocationInfo));
                // AllocationInfo.hAllocation           = NULL;
                // AllocationInfo.pSystemMem            = NULL;
                AllocationInfo.pPrivateDriverData    = &wddmAllocInfo;
                AllocationInfo.PrivateDriverDataSize = sizeof(wddmAllocInfo);

                D3DKMT_CREATEALLOCATION CreateAllocation;
                memset(&CreateAllocation, 0, sizeof(CreateAllocation));
                CreateAllocation.hDevice         = pThis->mKmtCallbacks.hDevice;
                CreateAllocation.NumAllocations  = 1;
                CreateAllocation.pAllocationInfo = &AllocationInfo;

                Status = pThis->mKmtCallbacks.d3dkmt->pfnD3DKMTCreateAllocation(&CreateAllocation);
                if (Status == STATUS_SUCCESS)
                {
                    pSurfaceInfo->Core.Key    = pData->u32Sid;
                    pSurfaceInfo->hAllocation = AllocationInfo.hAllocation;
                    if (!RTAvlU32Insert(&pThis->mSurfaceTree, &pSurfaceInfo->Core))
                    {
                        Status = STATUS_NOT_SUPPORTED;
                    }
                }

                if (Status != STATUS_SUCCESS)
                {
                    free(pSurfaceInfo);
                }
            }
            else
            {
                Status = STATUS_NOT_SUPPORTED;
            }
        }

        if (Status != STATUS_SUCCESS)
        {
            gaEnvSurfaceDestroy(pvEnv, pData->u32Sid);
        }
    }

    if (Status == STATUS_SUCCESS)
    {
        *pu32Sid = pData->u32Sid;
        free(pData);
        return 0;
    }

    Assert(0);
    free(pData);
    return -1;
}

/* static */ DECLCALLBACK(int)
GaDrvEnvKmt::gaEnvSurfaceDestroy(void *pvEnv,
                                 uint32_t u32Sid)
{
    GaDrvEnvKmt *pThis = (GaDrvEnvKmt *)pvEnv;

    NTSTATUS                          Status;
    D3DKMT_ESCAPE                     kmtEscape;
    VBOXDISPIFESCAPE_GASURFACEDESTROY data;

    memset(&data, 0, sizeof(data));
    data.EscapeHdr.escapeCode  = VBOXESC_GASURFACEDESTROY;
    // data.EscapeHdr.cmdSpecific = 0;
    data.u32Sid                = u32Sid;

    memset(&kmtEscape, 0, sizeof(kmtEscape));
    kmtEscape.hAdapter              = pThis->mKmtCallbacks.hAdapter;
    kmtEscape.hDevice               = pThis->mKmtCallbacks.hDevice;
    kmtEscape.Type                  = D3DKMT_ESCAPE_DRIVERPRIVATE;
    kmtEscape.Flags.HardwareAccess  = 1;
    kmtEscape.pPrivateDriverData    = &data;
    kmtEscape.PrivateDriverDataSize = sizeof(data);
    // kmtEscape.hContext              = 0;

    Status = pThis->mKmtCallbacks.d3dkmt->pfnD3DKMTEscape(&kmtEscape);
    Assert(Status == STATUS_SUCCESS);

    /* Try to remove from sid -> hAllocation map. */
    GAWDDMSURFACEINFO *pSurfaceInfo = (GAWDDMSURFACEINFO *)RTAvlU32Remove(&pThis->mSurfaceTree, u32Sid);
    if (pSurfaceInfo)
    {
        D3DKMT_DESTROYALLOCATION DestroyAllocation;
        memset(&DestroyAllocation, 0, sizeof(DestroyAllocation));
        DestroyAllocation.hDevice          = pThis->mKmtCallbacks.hDevice;
        // DestroyAllocation.hResource     = 0;
        DestroyAllocation.phAllocationList = &pSurfaceInfo->hAllocation;
        DestroyAllocation.AllocationCount  = 1;

        Status = pThis->mKmtCallbacks.d3dkmt->pfnD3DKMTDestroyAllocation(&DestroyAllocation);
        Assert(Status == STATUS_SUCCESS);

        free(pSurfaceInfo);
    }

    return (Status == STATUS_SUCCESS)? 0: -1;
}

D3DKMT_HANDLE GaDrvEnvKmt::drvEnvKmtSurfaceHandle(uint32_t u32Sid)
{
    GAWDDMSURFACEINFO *pSurfaceInfo = (GAWDDMSURFACEINFO *)RTAvlU32Get(&mSurfaceTree, u32Sid);
    return pSurfaceInfo ? pSurfaceInfo->hAllocation : 0;
}

static HRESULT
vboxDdiFenceCreate(GaKmtCallbacks *pKmtCallbacks,
                   GAWDDMCONTEXTINFO *pContextInfo,
                   uint32_t *pu32FenceHandle)
{
    NTSTATUS                       Status;
    D3DKMT_ESCAPE                  kmtEscape;
    VBOXDISPIFESCAPE_GAFENCECREATE fenceCreate;

    memset(&fenceCreate, 0, sizeof(fenceCreate));
    fenceCreate.EscapeHdr.escapeCode  = VBOXESC_GAFENCECREATE;
    // fenceCreate.EscapeHdr.cmdSpecific = 0;

    /* If the user-mode display driver sets hContext to a non-NULL value, the driver must
     * have also set hDevice to a non-NULL value...
     */
    memset(&kmtEscape, 0, sizeof(kmtEscape));
    kmtEscape.hAdapter              = pKmtCallbacks->hAdapter;
    kmtEscape.hDevice               = pKmtCallbacks->hDevice;
    kmtEscape.Type                  = D3DKMT_ESCAPE_DRIVERPRIVATE;
    // kmtEscape.Flags.HardwareAccess  = 0;
    kmtEscape.pPrivateDriverData    = &fenceCreate;
    kmtEscape.PrivateDriverDataSize = sizeof(fenceCreate);
    kmtEscape.hContext              = pContextInfo->hContext;

    Status = pKmtCallbacks->d3dkmt->pfnD3DKMTEscape(&kmtEscape);
    if (Status == STATUS_SUCCESS)
    {
        *pu32FenceHandle = fenceCreate.u32FenceHandle;
    }

    Assert(Status == STATUS_SUCCESS);
    return (Status == STATUS_SUCCESS) ? S_OK : E_FAIL;
}

static HRESULT
vboxDdiFenceQuery(GaKmtCallbacks *pKmtCallbacks,
                  uint32_t u32FenceHandle,
                  GAFENCEQUERY *pFenceQuery)
{
    NTSTATUS                      Status;
    D3DKMT_ESCAPE                 kmtEscape;
    VBOXDISPIFESCAPE_GAFENCEQUERY fenceQuery;

    memset(&fenceQuery, 0, sizeof(fenceQuery));
    fenceQuery.EscapeHdr.escapeCode  = VBOXESC_GAFENCEQUERY;
    // fenceQuery.EscapeHdr.cmdSpecific = 0;
    fenceQuery.u32FenceHandle = u32FenceHandle;

    memset(&kmtEscape, 0, sizeof(kmtEscape));
    kmtEscape.hAdapter              = pKmtCallbacks->hAdapter;
    kmtEscape.hDevice               = pKmtCallbacks->hDevice;
    kmtEscape.Type                  = D3DKMT_ESCAPE_DRIVERPRIVATE;
    // kmtEscape.Flags.HardwareAccess  = 0;
    kmtEscape.pPrivateDriverData    = &fenceQuery;
    kmtEscape.PrivateDriverDataSize = sizeof(fenceQuery);
    kmtEscape.hContext              = 0;

    Status = pKmtCallbacks->d3dkmt->pfnD3DKMTEscape(&kmtEscape);
    if (Status == STATUS_SUCCESS)
    {
        pFenceQuery->u32FenceHandle    = fenceQuery.u32FenceHandle;
        pFenceQuery->u32SubmittedSeqNo = fenceQuery.u32SubmittedSeqNo;
        pFenceQuery->u32ProcessedSeqNo = fenceQuery.u32ProcessedSeqNo;
        pFenceQuery->u32FenceStatus    = fenceQuery.u32FenceStatus;
    }

    Assert(Status == STATUS_SUCCESS);
    return (Status == STATUS_SUCCESS) ? S_OK : E_FAIL;
}

/* static */ DECLCALLBACK(int)
GaDrvEnvKmt::gaEnvFenceQuery(void *pvEnv,
                             uint32_t u32FenceHandle,
                             GAFENCEQUERY *pFenceQuery)
{
    GaDrvEnvKmt *pThis = (GaDrvEnvKmt *)pvEnv;

    if (!pThis->mKmtCallbacks.hDevice)
    {
        pFenceQuery->u32FenceStatus = GA_FENCE_STATUS_NULL;
        return 0;
    }

    HRESULT hr = vboxDdiFenceQuery(&pThis->mKmtCallbacks, u32FenceHandle, pFenceQuery);
    if (FAILED(hr))
        return -1;

    return 0;
}

static HRESULT
vboxDdiFenceWait(GaKmtCallbacks *pKmtCallbacks,
                 uint32_t u32FenceHandle,
                 uint32_t u32TimeoutUS)
{
    NTSTATUS                     Status;
    D3DKMT_ESCAPE                kmtEscape;
    VBOXDISPIFESCAPE_GAFENCEWAIT fenceWait;

    memset(&fenceWait, 0, sizeof(fenceWait));
    fenceWait.EscapeHdr.escapeCode  = VBOXESC_GAFENCEWAIT;
    // pFenceWait->EscapeHdr.cmdSpecific = 0;
    fenceWait.u32FenceHandle = u32FenceHandle;
    fenceWait.u32TimeoutUS = u32TimeoutUS;

    memset(&kmtEscape, 0, sizeof(kmtEscape));
    kmtEscape.hAdapter              = pKmtCallbacks->hAdapter;
    kmtEscape.hDevice               = pKmtCallbacks->hDevice;
    kmtEscape.Type                  = D3DKMT_ESCAPE_DRIVERPRIVATE;
    // kmtEscape.Flags.HardwareAccess  = 0;
    kmtEscape.pPrivateDriverData    = &fenceWait;
    kmtEscape.PrivateDriverDataSize = sizeof(fenceWait);
    kmtEscape.hContext              = 0;

    Status = pKmtCallbacks->d3dkmt->pfnD3DKMTEscape(&kmtEscape);
    Assert(Status == STATUS_SUCCESS);
    return (Status == STATUS_SUCCESS) ? S_OK : E_FAIL;
}

/* static */ DECLCALLBACK(int)
GaDrvEnvKmt::gaEnvFenceWait(void *pvEnv,
                                 uint32_t u32FenceHandle,
                                 uint32_t u32TimeoutUS)
{
    GaDrvEnvKmt *pThis = (GaDrvEnvKmt *)pvEnv;

    if (!pThis->mKmtCallbacks.hDevice)
        return 0;

    HRESULT hr = vboxDdiFenceWait(&pThis->mKmtCallbacks, u32FenceHandle, u32TimeoutUS);
    return SUCCEEDED(hr) ? 0 : -1;
}

static HRESULT
vboxDdiFenceUnref(GaKmtCallbacks *pKmtCallbacks,
                  uint32_t u32FenceHandle)
{
    NTSTATUS                     Status;
    D3DKMT_ESCAPE                kmtEscape;
    VBOXDISPIFESCAPE_GAFENCEUNREF fenceUnref;

    memset(&fenceUnref, 0, sizeof(fenceUnref));
    fenceUnref.EscapeHdr.escapeCode  = VBOXESC_GAFENCEUNREF;
    // pFenceUnref->EscapeHdr.cmdSpecific = 0;
    fenceUnref.u32FenceHandle = u32FenceHandle;

    memset(&kmtEscape, 0, sizeof(kmtEscape));
    kmtEscape.hAdapter              = pKmtCallbacks->hAdapter;
    kmtEscape.hDevice               = pKmtCallbacks->hDevice;
    kmtEscape.Type                  = D3DKMT_ESCAPE_DRIVERPRIVATE;
    // kmtEscape.Flags.HardwareAccess  = 0;
    kmtEscape.pPrivateDriverData    = &fenceUnref;
    kmtEscape.PrivateDriverDataSize = sizeof(fenceUnref);
    kmtEscape.hContext              = 0;

    Status = pKmtCallbacks->d3dkmt->pfnD3DKMTEscape(&kmtEscape);
    Assert(Status == STATUS_SUCCESS);
    return (Status == STATUS_SUCCESS) ? S_OK : E_FAIL;
}

/* static */ DECLCALLBACK(void)
GaDrvEnvKmt::gaEnvFenceUnref(void *pvEnv,
                                  uint32_t u32FenceHandle)
{
    GaDrvEnvKmt *pThis = (GaDrvEnvKmt *)pvEnv;

    if (!pThis->mKmtCallbacks.hDevice)
        return;

    vboxDdiFenceUnref(&pThis->mKmtCallbacks, u32FenceHandle);
}

/** Calculate how many commands will fit in the buffer.
 *
 * @param pu8Commands Command buffer.
 * @param cbCommands  Size of command buffer.
 * @param cbAvail     Available buffer size..
 * @param pu32Length  Size of commands which will fit in cbAvail bytes.
 */
static HRESULT
vboxCalcCommandLength(const uint8_t *pu8Commands, uint32_t cbCommands, uint32_t cbAvail, uint32_t *pu32Length)
{
    HRESULT hr = S_OK;

    uint32_t u32Length = 0;
    const uint8_t *pu8Src = pu8Commands;
    const uint8_t *pu8SrcEnd = pu8Commands + cbCommands;

    while (pu8SrcEnd > pu8Src)
    {
        const uint32_t cbSrcLeft = pu8SrcEnd - pu8Src;
        if (cbSrcLeft < sizeof(uint32_t))
        {
            hr = E_INVALIDARG;
            break;
        }

        /* Get the command id and command length. */
        const uint32_t u32CmdId = *(uint32_t *)pu8Src;
        uint32_t cbCmd = 0;

        if (SVGA_3D_CMD_BASE <= u32CmdId && u32CmdId < SVGA_3D_CMD_MAX)
        {
            if (cbSrcLeft < sizeof(SVGA3dCmdHeader))
            {
                hr = E_INVALIDARG;
                break;
            }

            const SVGA3dCmdHeader *pHeader = (SVGA3dCmdHeader *)pu8Src;
            cbCmd = sizeof(SVGA3dCmdHeader) + pHeader->size;
            if (cbCmd % sizeof(uint32_t) != 0)
            {
                hr = E_INVALIDARG;
                break;
            }
            if (cbSrcLeft < cbCmd)
            {
                hr = E_INVALIDARG;
                break;
            }
        }
        else
        {
            /* It is not expected that any of common SVGA commands will be in the command buffer
             * because the SVGA gallium driver does not use them.
             */
            hr = E_INVALIDARG;
            break;
        }

        if (u32Length + cbCmd > cbAvail)
        {
            if (u32Length == 0)
            {
               /* No commands fit into the buffer. */
               hr = E_FAIL;
            }
            break;
        }

        pu8Src += cbCmd;
        u32Length += cbCmd;
    }

    *pu32Length = u32Length;
    return hr;
}

static HRESULT
vboxDdiRender(GaKmtCallbacks *pKmtCallbacks,
              GAWDDMCONTEXTINFO *pContextInfo, uint32_t u32FenceHandle, void *pvCommands, uint32_t cbCommands,
              ULONGLONG PresentHistoryToken, bool fPresentRedirected)
{
    HRESULT hr = S_OK;
    D3DKMT_RENDER kmtRender;
    uint32_t cbLeft;
    const uint8_t *pu8Src;

    // LogRel(("vboxDdiRender: cbCommands = %d, u32FenceHandle = %d\n", cbCommands, u32FenceHandle));

    cbLeft = cbCommands;
    pu8Src = (uint8_t *)pvCommands;
    /* Even when cbCommands is 0, submit the fence. The following code deals with this. */
    do
    {
        /* Actually available space. */
        const uint32_t cbAvail = pContextInfo->CommandBufferSize;
        if (cbAvail <= sizeof(u32FenceHandle))
        {
            hr = E_FAIL;
            break;
        }

        /* How many bytes of command data still to copy. */
        uint32_t cbCommandChunk = cbLeft;

        /* How many bytes still to copy. */
        uint32_t cbToCopy = sizeof(u32FenceHandle) + cbCommandChunk;

        /* Copy the buffer identifier. */
        if (cbToCopy <= cbAvail)
        {
            /* Command buffer is big enough. */
            *(uint32_t *)pContextInfo->pCommandBuffer = u32FenceHandle;
        }
        else
        {
            /* Split. Write zero as buffer identifier. */
            *(uint32_t *)pContextInfo->pCommandBuffer = 0;

            /* Get how much commands data will fit in the buffer. */
            hr = vboxCalcCommandLength(pu8Src, cbCommandChunk, cbAvail - sizeof(u32FenceHandle), &cbCommandChunk);
            if (hr != S_OK)
            {
                break;
            }

            cbToCopy = sizeof(u32FenceHandle) + cbCommandChunk;
        }

        if (cbCommandChunk)
        {
            /* Copy the command data. */
            memcpy((uint8_t *)pContextInfo->pCommandBuffer + sizeof(u32FenceHandle), pu8Src, cbCommandChunk);
        }

        /* Advance the command position. */
        pu8Src += cbCommandChunk;
        cbLeft -= cbCommandChunk;

        memset(&kmtRender, 0, sizeof(kmtRender));
        kmtRender.hContext = pContextInfo->hContext;
        // kmtRender.CommandOffset = 0;
        kmtRender.CommandLength = cbToCopy;
        // kmtRender.AllocationCount = 0;
        // kmtRender.PatchLocationCount = 0;
        kmtRender.PresentHistoryToken = PresentHistoryToken;
        kmtRender.Flags.PresentRedirected = fPresentRedirected;

        NTSTATUS Status = pKmtCallbacks->d3dkmt->pfnD3DKMTRender(&kmtRender);
        Assert(Status == STATUS_SUCCESS);
        if (Status != STATUS_SUCCESS)
        {
            hr = E_FAIL;
            break;
        }

        pContextInfo->pCommandBuffer = kmtRender.pNewCommandBuffer;
        pContextInfo->CommandBufferSize = kmtRender.NewCommandBufferSize;
        pContextInfo->pAllocationList       = kmtRender.pNewAllocationList;
        pContextInfo->AllocationListSize    = kmtRender.NewAllocationListSize;
        pContextInfo->pPatchLocationList    = kmtRender.pNewPatchLocationList;
        pContextInfo->PatchLocationListSize = kmtRender.NewPatchLocationListSize;
    } while (cbLeft);

    return hr;
}

/** @todo return bool */
int GaDrvEnvKmt::doRender(uint32_t u32Cid, void *pvCommands, uint32_t cbCommands,
                          GAFENCEQUERY *pFenceQuery, ULONGLONG PresentHistoryToken, bool fPresentRedirected)
{
    HRESULT hr = S_OK;
    uint32_t u32FenceHandle;
    GAWDDMCONTEXTINFO *pContextInfo = (GAWDDMCONTEXTINFO *)RTAvlU32Get(&mContextTree, u32Cid);
    if (!pContextInfo)
        return -1;

    u32FenceHandle = 0;
    if (pFenceQuery)
    {
        hr = vboxDdiFenceCreate(&mKmtCallbacks, pContextInfo, &u32FenceHandle);
    }

    if (SUCCEEDED(hr))
    {
        hr = vboxDdiRender(&mKmtCallbacks, pContextInfo, u32FenceHandle,
                           pvCommands, cbCommands, PresentHistoryToken, fPresentRedirected);
        if (SUCCEEDED(hr))
        {
            if (pFenceQuery)
            {
                HRESULT hr2 = vboxDdiFenceQuery(&mKmtCallbacks, u32FenceHandle, pFenceQuery);
                if (hr2 != S_OK)
                {
                    pFenceQuery->u32FenceStatus = GA_FENCE_STATUS_NULL;
                }
            }
        }
    }
    return SUCCEEDED(hr)? 0: 1;
}

/* static */ DECLCALLBACK(int)
GaDrvEnvKmt::gaEnvRender(void *pvEnv,
                         uint32_t u32Cid,
                         void *pvCommands,
                         uint32_t cbCommands,
                         GAFENCEQUERY *pFenceQuery)
{
    GaDrvEnvKmt *pThis = (GaDrvEnvKmt *)pvEnv;
    return pThis->doRender(u32Cid, pvCommands, cbCommands, pFenceQuery, 0, false);
}

int GaDrvEnvKmt::drvEnvKmtRenderCompose(uint32_t u32Cid,
                                        void *pvCommands,
                                        uint32_t cbCommands,
                                        ULONGLONG PresentHistoryToken)
{
    return doRender(u32Cid, pvCommands, cbCommands, NULL, PresentHistoryToken, true);
}


static HRESULT
vboxDdiRegionCreate(GaKmtCallbacks *pKmtCallbacks,
                    uint32_t u32RegionSize,
                    uint32_t *pu32GmrId,
                    void **ppvMap)
{
    NTSTATUS                  Status;
    D3DKMT_ESCAPE             kmtEscape;
    VBOXDISPIFESCAPE_GAREGION data;

    memset(&data, 0, sizeof(data));
    data.EscapeHdr.escapeCode  = VBOXESC_GAREGION;
    // data.EscapeHdr.cmdSpecific = 0;
    data.u32Command               = GA_REGION_CMD_CREATE;
    data.u32NumPages              = (u32RegionSize + PAGE_SIZE - 1) / PAGE_SIZE;
    // data.u32GmrId              = 0;
    // data.u64UserAddress        = 0;

    memset(&kmtEscape, 0, sizeof(kmtEscape));
    kmtEscape.hAdapter              = pKmtCallbacks->hAdapter;
    kmtEscape.hDevice               = pKmtCallbacks->hDevice;
    kmtEscape.Type                  = D3DKMT_ESCAPE_DRIVERPRIVATE;
    // kmtEscape.Flags.HardwareAccess  = 0;
    kmtEscape.pPrivateDriverData    = &data;
    kmtEscape.PrivateDriverDataSize = sizeof(data);
    // kmtEscape.hContext              = 0;

    Status = pKmtCallbacks->d3dkmt->pfnD3DKMTEscape(&kmtEscape);
    if (Status == STATUS_SUCCESS)
    {
        *pu32GmrId = data.u32GmrId;
        *ppvMap = (void *)(uintptr_t)data.u64UserAddress;
    }
    Assert(Status == STATUS_SUCCESS);
    return (Status == STATUS_SUCCESS) ? S_OK : E_FAIL;
}

/* static */ DECLCALLBACK(int)
GaDrvEnvKmt::gaEnvRegionCreate(void *pvEnv,
                               uint32_t u32RegionSize,
                               uint32_t *pu32GmrId,
                               void **ppvMap)
{
    GaDrvEnvKmt *pThis = (GaDrvEnvKmt *)pvEnv;

    int ret;

    if (pThis->mKmtCallbacks.hDevice)
    {
        /* That is a real device */
        HRESULT hr = vboxDdiRegionCreate(&pThis->mKmtCallbacks, u32RegionSize, pu32GmrId, ppvMap);
        ret = SUCCEEDED(hr)? 0: -1;
    }
    else
    {
        /* That is a fake device, created when WDDM adapter is initialized. */
        *ppvMap = malloc(u32RegionSize);
        if (*ppvMap != NULL)
        {
            *pu32GmrId = 0;
            ret = 0;
        }
        else
            ret = -1;
    }

    return ret;
}

static HRESULT
vboxDdiRegionDestroy(GaKmtCallbacks *pKmtCallbacks,
                     uint32_t u32GmrId)
{
    NTSTATUS                  Status;
    D3DKMT_ESCAPE             kmtEscape;
    VBOXDISPIFESCAPE_GAREGION data;

    memset(&data, 0, sizeof(data));
    data.EscapeHdr.escapeCode  = VBOXESC_GAREGION;
    // data.EscapeHdr.cmdSpecific = 0;
    data.u32Command               = GA_REGION_CMD_DESTROY;
    // data.u32NumPages           = 0;
    data.u32GmrId                 = u32GmrId;
    // data.u64UserAddress        = 0;

    memset(&kmtEscape, 0, sizeof(kmtEscape));
    kmtEscape.hAdapter              = pKmtCallbacks->hAdapter;
    kmtEscape.hDevice               = pKmtCallbacks->hDevice;
    kmtEscape.Type                  = D3DKMT_ESCAPE_DRIVERPRIVATE;
    // kmtEscape.Flags.HardwareAccess  = 0;
    kmtEscape.pPrivateDriverData    = &data;
    kmtEscape.PrivateDriverDataSize = sizeof(data);
    // kmtEscape.hContext              = 0;

    Status = pKmtCallbacks->d3dkmt->pfnD3DKMTEscape(&kmtEscape);
    Assert(Status == STATUS_SUCCESS);
    return (Status == STATUS_SUCCESS) ? S_OK : E_FAIL;
}

/* static */ DECLCALLBACK(void)
GaDrvEnvKmt::gaEnvRegionDestroy(void *pvEnv,
                                     uint32_t u32GmrId,
                                     void *pvMap)
{
    GaDrvEnvKmt *pThis = (GaDrvEnvKmt *)pvEnv;

    if (pThis->mKmtCallbacks.hDevice)
    {
        vboxDdiRegionDestroy(&pThis->mKmtCallbacks, u32GmrId);
    }
    else
    {
        free(pvMap);
    }
}

GaDrvEnvKmt::GaDrvEnvKmt()
    :
    mContextTree(0)
{
    RT_ZERO(mKmtCallbacks);
    RT_ZERO(mHWInfo);
    RT_ZERO(mEnv);
}

GaDrvEnvKmt::~GaDrvEnvKmt()
{
}

HRESULT GaDrvEnvKmt::Init(void)
{
    mKmtCallbacks.d3dkmt = D3DKMTFunctions();

    /* Figure out which adapter to use. */
    NTSTATUS Status = vboxDispKmtOpenAdapter2(&mKmtCallbacks.hAdapter, &mKmtCallbacks.AdapterLuid);
    Assert(Status == STATUS_SUCCESS);

    if (Status == STATUS_SUCCESS)
    {
        VBOXWDDM_QAI adapterInfo;
        Status = vboxDdiQueryAdapterInfo(&mKmtCallbacks, mKmtCallbacks.hAdapter, &adapterInfo, sizeof(adapterInfo));
        Assert(Status == STATUS_SUCCESS);

        if (Status == STATUS_SUCCESS)
        {
            Status = vboxDdiDeviceCreate(&mKmtCallbacks, &mKmtCallbacks.hDevice);
            Assert(Status == STATUS_SUCCESS);

            if (Status == STATUS_SUCCESS)
            {
                mHWInfo = adapterInfo.u.vmsvga.HWInfo;

                /*
                 * Success.
                 */
                return S_OK;
            }

            vboxDdiDeviceDestroy(&mKmtCallbacks, mKmtCallbacks.hDevice);
        }

        vboxDispKmtCloseAdapter(mKmtCallbacks.hAdapter);
    }

    return E_FAIL;
}

const WDDMGalliumDriverEnv *GaDrvEnvKmt::Env()
{
    if (mEnv.cb == 0)
    {
        mEnv.cb                = sizeof(WDDMGalliumDriverEnv);
        mEnv.pHWInfo           = &mHWInfo;
        mEnv.pvEnv             = this;
        mEnv.pfnContextCreate  = gaEnvContextCreate;
        mEnv.pfnContextDestroy = gaEnvContextDestroy;
        mEnv.pfnSurfaceDefine  = gaEnvSurfaceDefine;
        mEnv.pfnSurfaceDestroy = gaEnvSurfaceDestroy;
        mEnv.pfnRender         = gaEnvRender;
        mEnv.pfnFenceUnref     = gaEnvFenceUnref;
        mEnv.pfnFenceQuery     = gaEnvFenceQuery;
        mEnv.pfnFenceWait      = gaEnvFenceWait;
        mEnv.pfnRegionCreate   = gaEnvRegionCreate;
        mEnv.pfnRegionDestroy  = gaEnvRegionDestroy;
    }

    return &mEnv;
}

RT_C_DECLS_BEGIN

const WDDMGalliumDriverEnv *GaDrvEnvKmtCreate(void)
{
    GaDrvEnvKmt *p = new GaDrvEnvKmt();
    if (p)
    {
        HRESULT hr = p->Init();
        if (hr != S_OK)
        {
            delete p;
            p = NULL;
        }
    }
    return p ? p->Env() : NULL;
}

void GaDrvEnvKmtDelete(const WDDMGalliumDriverEnv *pEnv)
{
    if (pEnv)
    {
        GaDrvEnvKmt *p = (GaDrvEnvKmt *)pEnv->pvEnv;
        delete p;
    }
}

D3DKMT_HANDLE GaDrvEnvKmtContextHandle(const WDDMGalliumDriverEnv *pEnv, uint32_t u32Cid)
{
    GaDrvEnvKmt *p = (GaDrvEnvKmt *)pEnv->pvEnv;
    return p->drvEnvKmtContextHandle(u32Cid);
}

D3DKMT_HANDLE GaDrvEnvKmtSurfaceHandle(const WDDMGalliumDriverEnv *pEnv, uint32_t u32Sid)
{
    GaDrvEnvKmt *p = (GaDrvEnvKmt *)pEnv->pvEnv;
    return p->drvEnvKmtSurfaceHandle(u32Sid);
}

void GaDrvEnvKmtAdapterLUID(const WDDMGalliumDriverEnv *pEnv, LUID *pAdapterLuid)
{
    GaDrvEnvKmt *p = (GaDrvEnvKmt *)pEnv->pvEnv;
    *pAdapterLuid = p->mKmtCallbacks.AdapterLuid;
}

D3DKMT_HANDLE GaDrvEnvKmtAdapterHandle(const WDDMGalliumDriverEnv *pEnv)
{
    GaDrvEnvKmt *p = (GaDrvEnvKmt *)pEnv->pvEnv;
    return p->mKmtCallbacks.hAdapter;
}

D3DKMT_HANDLE GaDrvEnvKmtDeviceHandle(const WDDMGalliumDriverEnv *pEnv)
{
    GaDrvEnvKmt *p = (GaDrvEnvKmt *)pEnv->pvEnv;
    return p->mKmtCallbacks.hDevice;
}

void GaDrvEnvKmtRenderCompose(const WDDMGalliumDriverEnv *pEnv,
                              uint32_t u32Cid,
                              void *pvCommands,
                              uint32_t cbCommands,
                              ULONGLONG PresentHistoryToken)
{
    GaDrvEnvKmt *p = (GaDrvEnvKmt *)pEnv->pvEnv;
    p->drvEnvKmtRenderCompose(u32Cid, pvCommands, cbCommands, PresentHistoryToken);
}

RT_C_DECLS_END
